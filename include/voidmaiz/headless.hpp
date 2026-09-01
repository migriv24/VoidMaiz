/*
 * voidmaiz/headless.hpp — a Void Maiz application with no front-end
 * (DRAFT 2026-08-18, author's direction; okf/concepts/headless.md).
 *
 * THE ASK. "I want to give an agent a task, and I want that agent to USE stuff
 * from these applications to actually complete the task, likely from a real
 * terminal" — and then: "when I open up the app, it should be updated. If
 * something is slightly wrong, I should be able to modify it." Not for a
 * developer: for a client who installed a binary.
 *
 * WHY THIS IS SMALL. Founding commitment 2 has said "humans, scripts, and AI
 * agents share ONE interaction surface" since day one, and commitment 1 says
 * the model lives in Void Core. So a headless session and a GUI session are two
 * front-ends over ONE state document — there is no sync problem because there
 * is nothing to sync, and there is no import path because nothing was ever
 * exported. Headless is the REMOVAL OF A PROJECTION, not a new capability. If
 * it had needed a second code path, that would have been evidence the
 * architecture was wrong.
 *
 * ONE DECLARATION, TWO FRONT-ENDS. A host fills in `HostApp` once and uses it
 * for both its GUI `main()` and its headless one. That is exactly the property
 * ActionDescriptor already gives a single action ("a volunteer's click and an
 * agent's `map place contact @here` become the SAME transcript entry"), scaled
 * from one action to a whole application — and it is what stops the two
 * front-ends drifting into two applications.
 *
 * NO NEW VERBS. Nothing here adds a command a human at the in-app console could
 * not type. The headless front-end is a way to REACH the dispatcher, never a
 * second language. A capability that only an agent has would break the founding
 * commitment, so there is deliberately no flag for it and no way to act
 * unlogged.
 *
 * UI-free, like action.hpp and census.hpp before it: this is the base
 * `voidmaiz_headless` target and includes no ImGui. It links `voidmaiz` for
 * Core and the registries, and nothing else.
 */
#pragma once

#include "voidmaiz/action.hpp"
#include "voidmaiz/embed.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* ── the holiday seam, declared ──────────────────────────────────────────────
 *
 * One op the host's effect handler supports (`save`, `deploy`, `build`,
 * `preview`, or a custom `effect <op>`; SPEC §9).
 *
 * WHY DECLARE WHAT THE HANDLER ALREADY DOES. `Core::EffectHandler` is one opaque
 * `std::function`: nothing can enumerate what ops it answers to, so an agent
 * arriving cold cannot know whether this application can deploy a website — and
 * cannot know what happens if it tries. This is the same gap `ActionDescriptor`
 * closed for a view's gestures, at the other seam, and it is closed the same
 * way: name it, document it, and let the library enumerate without ever
 * deciding what it means.
 *
 * `consequence` is the field that matters and the one a host will be tempted to
 * leave empty. Everything else a headless agent does lands in the document and
 * is reversible by `revert`. An effect is where the work LEAVES the document —
 * a git push, a sent newsletter, a written backend — and no amount of undo
 * reaches it. A person deciding whether to hand an agent `--allow-effects`
 * needs this sentence, in your words, about your application. */
struct EffectOp {
    std::string name;        // "deploy", "build", "publish-newsletter"
    std::string doc;         // what it does, agent-legible
    /* Does the document's own `revert` undo this? Almost always false, and
     * saying so is the point — an op that claims true had better mean it. */
    bool reversible = false;
    std::string consequence; // "pushes the site live to the public URL"
};

/* ── who is at the keyboard ──────────────────────────────────────────────────
 *
 * **A PERSON OUTRANKS AN AGENT.** The author's rule, 2026-08-18: *"a person
 * should always have more power than an agent, even in the headless mode."*
 *
 * The advisory lock was first-come-first-served, which quietly encoded the
 * opposite: an agent that started at 09:00 could keep someone locked out of
 * their own application all morning. Precedence fixes that without inventing
 * distrust — this is not a defence against rogue agents (the author does not
 * assume any, and neither do we), it is the ordinary observation that when two
 * parties want the same document, the human's want is the one that counts.
 *
 * The rule is exactly one line: **a Human session may take the lock from an
 * Agent session. Nothing else preempts anything.** Two humans still collide
 * normally, and an agent never displaces anyone.
 *
 * The evicted agent is not killed — nothing here can reach across a process —
 * but it does notice. Its next command fails, its save is refused, and its work
 * is left where it was. So "whatever changes are made in the GUI will be done
 * first" holds in the way that matters: the person's document is never
 * overwritten by a session that no longer holds the floor. */
enum class SessionKind {
    Agent, // the headless default: yields to a person
    Human, // a GUI, or a headless session a person is driving (`--human`)
};

/* ── streaming effects ───────────────────────────────────────────────────────
 *
 * SPEC §9: long operations (deploy/build/preview) MUST stream line-by-line
 * rather than returning one lump at the end. `Core::EffectHandler` cannot —
 * it returns a single string when it is finished — so a host with a long
 * effect either says nothing for ninety seconds or lies about when it
 * happened.
 *
 * A streaming handler is the same function with an `emit` callback. Whatever it
 * emits reaches the journal and the terminal AS IT HAPPENS, which is what makes
 * an unattended deploy watchable and, more to the point, diagnosable when it
 * fails at line 40 of 60. */
using EffectEmit = std::function<void(std::string_view line)>;
using StreamingEffectHandler = std::function<std::string(
    std::string_view op, std::string_view args, const EffectEmit& emit)>;

/* ── the application, declared once ──────────────────────────────────────────
 *
 * Everything a front-end needs to BE this application. A GUI main() and a
 * headless main() take the same value; whatever is missing here is missing from
 * both, which is the point.
 *
 * The library never invents any of it — glyphs, actions and effects are the
 * host's, exactly as everywhere else. */
struct HostApp {
    std::string id;      // "hormiga", "vls" — the stable machine name; also
                         // names the default state file
    std::string label;   // "Void Hormiga" — for humans
    std::string version; // the APPLICATION's version, not the library's

    /* Glyph descriptors, as JSON, in registration order. Glyphs are host config
     * and are NOT part of the exported state (embed.hpp), so a headless session
     * must re-register exactly what the GUI registers — a divergence here is
     * the one way the two front-ends can silently disagree about the same
     * document, which is why they come from one declaration. */
    std::vector<std::string> glyphs;

    /* The view's interaction vocabulary, if the app has one. Optional: an app
     * with no custom view leaves it empty and loses nothing but Phase H2.
     *
     * Registering actions is how a view's affordances survive the loss of the
     * view — a hand-rolled gesture emitting raw commands is invisible to an
     * agent by construction (canvas-actions.md, and the ask we put to Hormiga
     * on 2026-07-24). */
    ActionRegistry actions;

    /* The app's OKF bundle directory, so an agent can read the design rather
     * than guess it. Reported in the briefing, never parsed here — retrieval is
     * Void Core's OKF holiday (`python -m okf --bundle <dir> get --head <id>`),
     * and `--head` exists so a page costs ~1.3 KB instead of ~17 KB. */
    std::string okf_root;

    /* Extra core setup the app does at boot and the document does not carry:
     * `config set` calls, transform registration, domain fields. Runs AFTER the
     * state document is loaded and the glyphs are registered. */
    std::function<void(Core&)> configure;

    /* The holiday seam (SPEC §9): save / deploy / build / preview / effect.
     * THIS is where "also update the website" actually happens — the state
     * document is Void Maiz's business, but writing a real backend is the
     * host's, and the core performs no file I/O of its own.
     *
     * Two shapes reach this handler, and the asymmetry is the core's, not ours:
     * `save` is called with the WHOLE STATE DOCUMENT as its args string, while
     * `deploy`/`build`/`preview`/`effect` are called with `{"args":[…]}`. */
    Core::EffectHandler effects;

    /* The streaming form. When set it is used INSTEAD of `effects` — a host
     * supplies one or the other, and the streaming one is strictly more
     * capable, so a host with a slow effect should prefer it. Everything it
     * emits is journalled and shown live; its return value is the result,
     * exactly as for `effects`. */
    StreamingEffectHandler streaming_effects;

    /* What that handler answers to, for the briefing and the gate. Declaring an
     * op does not enable it (see EffectPolicy) and omitting one does not disable
     * it — the handler is still the handler. This is a DESCRIPTION, exactly like
     * a glyph hint: the library enumerates it and never interprets it. */
    std::vector<EffectOp> effect_ops;

    /* Condition words the app's Allomone registers, for the briefing only. A
     * plain string list rather than the registry itself, so `voidmaiz_headless`
     * does not have to link `voidmaiz_allomone`; a host that has one passes
     * `preds.names()`. */
    std::vector<std::string> predicates;
};

/* ── may this session reach the world? ───────────────────────────────────────
 *
 * THE ONE-WAY DOOR. Everything else a headless agent does lands in the state
 * document, which means a person can see it with `diff` and discard it with
 * `revert`. An effect is where that stops being true: a deploy pushes a live
 * site, a save writes a real backend, a newsletter is sent to people. The
 * review story that makes unattended agent work acceptable does not extend past
 * this line, and pretending otherwise is how someone's Tuesday gets emailed to
 * four hundred subscribers.
 *
 * So the default REFUSES, and the refusal is loud. That is not timidity about
 * agents; it is the observation that "make the changes" and "publish the
 * changes" are two different grants, and only one of them is undoable. An agent
 * told to update a database and the website should be able to do all the
 * database work unattended and then be stopped at the door — which is exactly
 * what a person would want to review anyway. */
enum class EffectPolicy {
    Refuse, // effect verbs are declined, with the op's `consequence` in the
            // message so the caller learns what it was asking for. Default.
    DryRun, // report what WOULD run, run nothing. The rehearsal — an agent can
            // confirm it built a valid deploy without performing one.
    Allow,  // permitted, subject to SessionOptions::allowed_effects
};

/* ── how this session should behave ──────────────────────────────────────── */
struct SessionOptions {
    /* Where the state document lives. Empty = `<app.id>.state.json` in the
     * working directory.
     *
     * THIS PATH IS THE WHOLE PERSISTENCE STORY, and it must be the same file
     * the GUI opens. If a headless run and a GUI run disagree about it, the
     * author's "when I open up Hormiga it should be updated" quietly fails —
     * and it fails by showing stale data rather than by erroring, which is the
     * worst shape a bug can have. */
    std::string state_path;

    /* Who is acting. Non-empty sets Void Core's `config.actor` (SPEC §9), so
     * every log record and every undo frame carries a `who`.
     *
     * DEFAULTED, NOT OPTIONAL. An unattributed agent session is exactly the
     * thing that makes a person unable to tell their own work from a robot's
     * when they open the app afterwards, and the review story in
     * okf/concepts/headless.md rests entirely on this. Pass your agent's name;
     * the default at least says it was not a person. */
    std::string actor = "agent";

    /* Write the state document back when the session ends cleanly. Off means
     * "read-only": every command still runs, so `ls`/`tree`/`describe` work and
     * a mutation is still visible within the session, but nothing survives it.
     * The honest way to let an agent LOOK at an application. */
    bool save_on_close = true;

    /* Take the advisory lock beside the state file (see Session::start). Off
     * only for tests and for a host that has its own exclusion. */
    bool lock = true;

    /* Who this session speaks for. **A GUI host must set `Human`** — that is
     * the entire mechanism by which a person outranks an agent, and a GUI that
     * leaves the default is a GUI that can be locked out by a background task.
     * See SessionKind. */
    SessionKind kind = SessionKind::Agent;

    /* Every line a streaming effect emits, as it is emitted. `run_cli` sends
     * these to **stderr**, so a `--json` caller's stdout stays parseable while
     * a person watching still sees the deploy scroll past. */
    std::function<void(std::string_view op, std::string_view line)> on_effect_line;

    /* Append the session's log records to this file. Empty = `<state_path>.log`;
     * set `journal` false to write none.
     *
     * WHY THIS IS NOT OPTIONAL DECORATION. Void Core's undo stack and log are
     * SESSION-SCOPED — the state document carries the model, not the history of
     * how it got there (measured, 2026-08-18: `history` in a second session
     * shows nothing from the first). So attribution is live and correct *inside*
     * a run and evaporates when the process exits.
     *
     * That is fine for a GUI, where the session and the sitting are the same
     * thing. It is NOT fine for headless, where the whole review story is a
     * person opening the app LATER and asking what the agent did. Without a
     * journal, "if something is slightly wrong I should be able to modify it"
     * degrades to reading a diff of a JSON document.
     *
     * The core performs no file I/O by contract (SPEC §9), so persisting the
     * spine is the front-end's job. Records are written in Void Core's own line
     * format — `[ISO-8601] LEVEL op (who): message` — so the file a person reads
     * afterwards is the same shape as the one they see live. */
    std::string journal_path;
    bool journal = true;

    /* May this session reach the world? Refusing is the default; see
     * EffectPolicy for why that is a design position and not caution. */
    EffectPolicy effects = EffectPolicy::Refuse;

    /* Under `Allow`, the ops permitted. **Empty means all of them** — an
     * explicit `Allow` with no list is a deliberate "yes, everything", which is
     * the only reading that does not make the empty default silently
     * permissive under the safe policy. Ignored under Refuse and DryRun. */
    std::vector<std::string> allowed_effects;

    /* Every dispatched command line, before it runs — a transcript hook for a
     * host that wants to echo, tee or record. Not a veto: returning does not
     * stop the dispatch, because a front-end that could silently drop commands
     * would break the "the log is complete" property. */
    std::function<void(std::string_view command)> on_command;
};

/* ── one session: load, act, save ────────────────────────────────────────────
 *
 * A session is a PROCESS, not a service: it opens the document, does the work,
 * writes it back and exits. There is no daemon and no server here
 * (okf/concepts/headless.md, "What headless is NOT"). */
class Session {
public:
    Session(const HostApp& app, SessionOptions opts = {});
    ~Session();

    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /* Open the document and make the core ready: acquire the lock, read the
     * state file (absent = a fresh document, which is not an error — a first
     * run has to start somewhere), register glyphs, set the actor, run
     * `configure`, install the effect handler.
     *
     * Returns false on a real failure — the lock is held elsewhere, or the
     * state file exists but is unreadable. `error()` says which. A malformed
     * document is NOT silently replaced with an empty one: that would destroy a
     * user's work to satisfy a return type. */
    bool start();

    /* Dispatch one command line. Everything a headless front-end does goes
     * through here, so the transcript hook and the log see all of it.
     *
     * ── THE EFFECT GATE, and why it is two mechanisms rather than one ────────
     *
     * A refused effect must (a) not happen and (b) report as a FAILED command.
     * Neither is free, because of two things the core does that a host cannot
     * change from outside (both read in `verbs_lifecycle.c`, 2026-08-18):
     *
     * 1. **`save` snapshots the baseline unconditionally.** `vc_snapshot_baseline`
     *    runs after the adapter call whatever the adapter returned — so refusing
     *    at the HANDLER would still move `_baseline` and erase exactly the diff
     *    that tells a person what an agent did. The gate therefore intercepts at
     *    the VERB, before the command reaches the core at all.
     * 2. **An effect handler cannot fail a dispatch.** The result is built with
     *    `res_make(1)` regardless of what the handler returns; the handler's
     *    value becomes `data`, never `ok`. So a refusal that only spoke through
     *    the handler would be reported to the caller as a success — the worst
     *    possible outcome for a safety mechanism.
     *
     * Hence: the verb gate is the one that works, and it is backed by a second
     * gate inside the handler wrapper for anything that reaches the seam by a
     * route the verb gate cannot read (a `script` body). `batch` payloads ARE
     * read and gated, because an atomic batch is the obvious way to smuggle a
     * `save` past a check on the leading verb.
     *
     * A refused command comes back `ok == false` with the reason in `lines`,
     * which is the honest shape and the one a shell or an agent branches on. */
    Result dispatch(std::string_view command);

    /* Dispatch many as ONE undo frame, atomically — Void Core's `batch` verb
     * (SPEC §7), which rolls back entirely if any line fails.
     *
     * Prefer this for an agent's task. A half-applied multi-step edit is the
     * state a person cannot review, because it is neither the old document nor
     * the intended one; and one frame means one `undo` puts it all back. */
    Result batch(const std::vector<std::string>& commands);

    /* Write the state document to `state_path` now. Called automatically at
     * close when `save_on_close`; exposed for a long task that wants a
     * checkpoint. Returns false and sets error() on an I/O failure — a save
     * that quietly did nothing is the failure mode this whole design is meant
     * to avoid.
     *
     * THIS IS NOT THE `save` VERB, and the difference is load-bearing.
     *
     * This persists the DOCUMENT. Void Core's `save` verb additionally
     * snapshots `_baseline` (SPEC §7) — and `_baseline`, unlike the undo stack,
     * IS model content and therefore survives the process. That is what makes
     * the whole cross-process review story work: after a headless run, a person
     * opening the app sees the agent's work as **unsaved changes against the
     * baseline**, so `status` and `diff` show exactly what changed and `revert`
     * discards all of it (measured 2026-08-18).
     *
     * If this function dispatched `save`, that diff would be erased and the
     * agent's work would be indistinguishable from the user's own. So it
     * deliberately does not — and a host wanting a real backend write must
     * dispatch `save` itself, on purpose, because that is a decision about the
     * user's data rather than about serialization. */
    bool save();

    /* Release the lock, saving first when configured. Idempotent; the
     * destructor calls it. */
    void close();

    Core& core();
    const std::string& state_path() const;
    /* The last failure, or "" — set by start(), save() and close(). */
    const std::string& error() const;

    /* Does this session still hold the floor? Re-reads the lock, so it is a
     * filesystem touch rather than a cached flag — the whole point is to notice
     * something another process did.
     *
     * False means a Human session took it (see SessionKind). Once that has
     * happened every `dispatch` fails, `save` refuses, and `close` writes
     * nothing: the work stays in memory and dies with the process, which is the
     * correct outcome. Losing an agent's uncommitted half-hour is a far smaller
     * harm than silently overwriting the document a person is editing right
     * now, and it is the only one of the two that can be explained afterwards.
     *
     * Always true when `SessionOptions::lock` is off — a session that took no
     * lock cannot lose one. */
    bool holds_lock() const;

    /* True once this session has been preempted. Sticky: it never recovers,
     * even if the person closes their session and the lock frees, because
     * whatever they did in between is now the document and this session's
     * in-memory copy is a fork of it. */
    bool preempted() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

/* ── the briefing: how to drive this application ─────────────────────────────
 *
 * One JSON document answering "what is this, what can I do, and how do I say
 * it" for an agent that arrived cold at an installed binary and has never seen
 * the source.
 *
 * ASSEMBLED, NOT AUTHORED. Every field comes from something that already
 * existed: `help`, `glyphs` and `mantles` from Void Core; the action manifest
 * from `ActionRegistry::manifest()`, which was built on 2026-07-21 so that a
 * volunteer's click and an agent's command would be the same transcript entry —
 * four weeks before anyone asked for headless. The briefing is a READER, and
 * an application that discovers it has nothing to say here has a real gap in
 * its own registries rather than a missing feature.
 *
 * NOT THE SURFACE CENSUS, and the two must not merge (headless.md): the census
 * emits commands that build a doc mantle for humans; this returns an ephemeral
 * JSON string and touches no mantle. They read the same registries on purpose —
 * if they ever disagree about what an app affords, one of them is lying. */
std::string capabilities(const HostApp& app, Core& core,
                         std::string_view state_path = {},
                         const SessionOptions* opts = nullptr);

/* ── the terminal front-end ──────────────────────────────────────────────────
 *
 * The whole CLI, so an application's headless `main()` is one line:
 *
 *     int main(int argc, char** argv) { return maiz::run_cli(build_app(), argc, argv); }
 *
 * Modes, chosen by the arguments:
 *
 *     app <command...>        dispatch one command line, print, save, exit
 *     app --script <file>     run a file of command lines (`-` = stdin)
 *     app --repl              read commands from stdin until EOF or `exit`
 *     app --describe          print the briefing and exit (no state written)
 *
 * Flags:
 *
 *     --state <path>   the state document (default `<id>.state.json`)
 *     --actor <name>   attribution; who the log says did this
 *     --json           machine envelopes rather than human lines
 *     --no-save        run everything, write nothing (read-only)
 *     --atomic         with --script: one `batch`, all-or-nothing, one undo frame
 *     --no-lock        skip the advisory lock (tests, or a host with its own)
 *
 *     --allow-effects[=a,b]  permit effect verbs (all of them, or just these).
 *                            OFF by default: effects are the one-way door, and
 *                            `revert` does not reach past them.
 *     --dry-run-effects      report what an effect WOULD do and run nothing
 *
 * Returns a process exit code: 0 on success, 1 on a failed command, 2 on a
 * usage or startup error. A failed command is exit 1 rather than a crash,
 * because a shell script and an agent both branch on that. */
int run_cli(const HostApp& app, int argc, char** argv);

} // namespace maiz
