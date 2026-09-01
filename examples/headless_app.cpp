/* headless_app.cpp — a whole Void Maiz application with no front-end.
 *
 * WHAT THIS IS FOR. `okf/concepts/headless.md` argues that a headless
 * application is the same application with the projection removed. This is the
 * argument as a running binary: a tiny notes/contacts app declared ONCE as a
 * `HostApp`, whose entire terminal front-end is one line of `main`.
 *
 * Drive it the way an agent would:
 *
 *     maiz_headless --describe                       # what can you do?
 *     maiz_headless --state notes.json mantle new work
 *     maiz_headless --state notes.json rune new note idea
 *     maiz_headless --state notes.json set idea text 'don''t forget'
 *     maiz_headless --state notes.json --json tree
 *     maiz_headless --state notes.json --script task.vs --atomic
 *
 * Then open the same `notes.json` in a GUI build and the work is there — not
 * because anything synchronizes, but because there is only one document.
 *
 * IT IS ALSO A TEST (`headless_smoke` in ctest, `--selftest`). The claims this
 * file makes are the ones most expensive to be wrong about — that changes
 * persist, that a second session sees them, that the lock refuses a concurrent
 * writer, that `--atomic` rolls back — so it checks them rather than asserting
 * them in prose. Returns non-zero if any fails.
 *
 * NO IMGUI. It links `voidmaiz_headless` and `voidmaiz` only. That is the
 * claim: an installed application can expose its engine to an agent with no
 * window system present at all.
 */
#include "voidmaiz/headless.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int failures = 0;
#define REQUIRE(cond)                                                            \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " #cond "\n";                 \
        }                                                                        \
    } while (0)

/* ── the application, declared once ──────────────────────────────────────────
 *
 * A GUI build would take this SAME value and hand it to a window loop. That is
 * the anti-drift property: the glyph list, the action vocabulary and the effect
 * handler cannot differ between the two front-ends, because there is only one
 * of each. */
static maiz::HostApp build_app() {
    maiz::HostApp app;
    app.id = "maiznotes";
    app.label = "Maiz Notes";
    app.version = "0.1.0";
    app.okf_root = "okf"; // where an agent reads why this app is shaped so

    app.glyphs = {
        R"({"glyph":"note","label":"Note","fields":["text","done"],)"
        R"("hints":{"editors":{"done":"toggle"},"labels":{"text":"Body"}}})",
        R"({"glyph":"person","label":"Person","fields":["email"]})",
    };

    /* The view's interaction vocabulary — registered even though this build has
     * no view, which is exactly the point. `place` is a canvas gesture; here it
     * is a named tool an agent can enumerate and invoke, and both front-ends
     * would call this one `compile`. */
    app.actions.add({
        .name = "note-at",
        .label = "Add a note at a position",
        .doc = "Create a note with body text, placed at world (x, y).",
        .params = {{"name", "text", true, "the rune name"},
                   {"body", "text", true, "the note body"},
                   {"x", "number", false, "world x (default 0)"},
                   {"y", "number", false, "world y (default 0)"}},
        .gesture = "click",
        .compile = [](const maiz::Scene&, const maiz::ActionArgs& a) {
            auto get = [&](const char* k, const char* fallback) {
                auto it = a.find(k);
                return it == a.end() ? std::string(fallback) : it->second;
            };
            const std::string name = get("name", "");
            if (name.empty()) return std::vector<std::string>{}; // decline
            return std::vector<std::string>{
                "rune new note " + name,
                "set " + name + " text " + maiz::arg(get("body", "")),
                "place " + name + " " + get("x", "0") + " " + get("y", "0"),
            };
        },
    });

    /* The domain's Allomone condition words, for the briefing. Names only —
     * this target does not link `voidmaiz_allomone`, which is the seam holding. */
    app.predicates = {"done", "assigned", "overdue"};

    /* The holiday seam — "also update the website" lives here. A real app writes
     * a backend; this one appends a line to a witness file, so the self-test can
     * prove not just that the path works but that a REFUSED effect genuinely
     * did not run. An effect gate you cannot observe is a gate you cannot
     * trust. */
    /* The STREAMING form, because a real deploy takes ninety seconds and SPEC §9
     * says a long operation reports line by line rather than going quiet and
     * then claiming success. Whatever `emit` receives reaches the journal and
     * the terminal as it happens — which is what makes an unattended deploy
     * diagnosable when it dies at line 40 of 60. */
    app.streaming_effects = [](std::string_view op, std::string_view args,
                               const maiz::EffectEmit& emit) {
        const fs::path witness =
            fs::temp_directory_path() / "maiz_headless_selftest" / "effects.log";
        std::error_code ec;
        fs::create_directories(witness.parent_path(), ec);
        std::ofstream out(witness, std::ios::app);
        out << op << "\n";
        (void)args;

        if (op == "deploy") {
            emit("rendering 3 notes");
            emit("uploading to the live site");
            emit("done");
        } else if (op == "build") {
            emit("wrote ./out/index.html");
        }
        return std::string(R"({"ok":true})");
    };

    /* What that handler answers to. Declaring it is what lets an agent find out
     * BEFORE it plans a task that this application can publish, and what the
     * publishing would mean. */
    app.effect_ops = {
        {"save", "Write the notes to the real backend.", false,
         "overwrites the stored copy and moves the review baseline"},
        {"build", "Render the notes to static HTML in ./out.", true,
         "rewrites generated files only; nothing is published"},
        {"deploy", "Publish the rendered notes to the live site.", false,
         "pushes to the public URL — visible to everyone, immediately"},
    };

    return app;
}

// ── the self-test ───────────────────────────────────────────────────────────

static void selftest() {
    using namespace maiz;
    const fs::path dir = fs::temp_directory_path() / "maiz_headless_selftest";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string state = (dir / "notes.state.json").string();

    HostApp app = build_app();

    // ── 1. A first session on a file that does not exist yet ────────────────
    // An absent document is a fresh start, not an error: "point your agent at a
    // new folder" has to work.
    {
        SessionOptions o;
        o.state_path = state;
        o.actor = "test-agent";
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(s.dispatch("mantle new work").ok);
        REQUIRE(s.dispatch("rune new note idea").ok);
        // The apostrophe path that corrupted two hosts' scripts — through the
        // shipped arg(), through a real dispatcher, into a real file.
        REQUIRE(s.dispatch("set idea text " + arg("don't forget")).ok);
        s.close();
        REQUIRE(s.error().empty());
    }
    REQUIRE(fs::exists(state));

    // ── 2. A SECOND session sees the first one's work ────────────────────────
    // This is the author's actual requirement ("when I open up Hormiga it
    // should be updated"), reduced to its testable core: a new process, a new
    // Core, the same document.
    {
        SessionOptions o;
        o.state_path = state;
        o.actor = "second";
        Session s(app, o);
        REQUIRE(s.start());
        Result r = s.dispatch("get idea text");
        REQUIRE(r.ok);
        // The value survived the round-trip through the file, apostrophe intact.
        REQUIRE(r.text().find("don't forget") != std::string::npos);

        /* Attribution is LIVE and correct within a session — `history` carries
         * the actor on each frame (SPEC §9). Note what this check does NOT
         * claim: the first session's frames are not here. Void Core's undo
         * stack is session-scoped and the state document carries the model, not
         * the history of how it got there. */
        REQUIRE(s.dispatch("rune new note here").ok);
        Result h = s.dispatch("history");
        REQUIRE(h.ok);
        REQUIRE(h.text().find("second") != std::string::npos);
        s.close();
    }

    /* ── 2b. …which is exactly why the journal exists ─────────────────────────
     * The author's requirement is that a person opening the app LATER can see
     * what an agent did. Since the undo stack does not survive the process, the
     * front-end persists the log spine beside the document — and it APPENDS, so
     * both sessions above are in it. Without this, "if something is slightly
     * wrong I should be able to modify it" degrades to diffing JSON. */
    {
        const fs::path journal = fs::path(state + ".log");
        REQUIRE(fs::exists(journal));
        std::ifstream in(journal);
        std::string all((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
        REQUIRE(all.find("test-agent") != std::string::npos); // session 1
        REQUIRE(all.find("second") != std::string::npos);     // session 2
        REQUIRE(all.find("rune new note idea") != std::string::npos);
    }

    // ── 3. The advisory lock refuses a concurrent writer ─────────────────────
    // Two writers over one document is the real hazard of making both surfaces
    // first-class. Agent-vs-agent is a plain collision: first come, first served.
    {
        SessionOptions o;
        o.state_path = state;
        Session held(app, o);
        REQUIRE(held.start());

        SessionOptions o2;
        o2.state_path = state;
        o2.actor = "intruder";
        Session second(app, o2);
        REQUIRE(!second.start());
        REQUIRE(second.error().find("held by") != std::string::npos);
        held.close();

        // Released on close, so the next session starts normally.
        Session third(app, o2);
        REQUIRE(third.start());
        third.close();
    }

    /* ── 3b. A PERSON OUTRANKS AN AGENT ───────────────────────────────────────
     *
     * The author's rule (2026-08-18): "a person should always have more power
     * than an agent, even in the headless mode." Not a defence against rogue
     * agents — just the observation that when two parties want the same
     * document, the human's want is the one that counts.
     *
     * The property that has to hold is not "the human gets a lock". It is that
     * **the evicted agent cannot overwrite the person's document afterwards.** */
    {
        SessionOptions agent;
        agent.state_path = state;
        agent.actor = "long-running-agent";
        agent.kind = SessionKind::Agent;
        Session working(app, agent);
        REQUIRE(working.start());
        REQUIRE(working.dispatch("mantle new agentwork").ok);
        REQUIRE(working.holds_lock());

        // A person opens the application. They take the floor.
        SessionOptions person;
        person.state_path = state;
        person.actor = "migri";
        person.kind = SessionKind::Human;
        Session gui(app, person);
        REQUIRE(gui.start());
        REQUIRE(gui.holds_lock());

        // The agent notices on its very next command, and stops.
        REQUIRE(!working.holds_lock());
        Result blocked = working.dispatch("rune new note tooLate");
        REQUIRE(!blocked.ok);
        REQUIRE(blocked.text().find("takes precedence") != std::string::npos);
        REQUIRE(working.preempted());

        // And it will not write, even if asked directly. This is the line that
        // makes precedence real rather than advisory.
        REQUIRE(!working.save());
        REQUIRE(working.error().find("refusing to save") != std::string::npos);

        // The person's work is what lands.
        REQUIRE(gui.dispatch("mantle new personwork").ok);
        gui.close();
        REQUIRE(gui.error().empty());

        // Closing the evicted agent writes nothing and does not steal the lock
        // back by deleting a file that is no longer its own.
        working.close();
    }
    {
        // The person's mantle is there; the agent's post-eviction work is not.
        SessionOptions o;
        o.state_path = state;
        o.save_on_close = false;
        Session s(app, o);
        REQUIRE(s.start());
        Result m = s.dispatch("mantles");
        REQUIRE(m.ok);
        REQUIRE(m.text().find("personwork") != std::string::npos);
        REQUIRE(!s.dispatch("get tooLate text").ok);
        s.close();
    }
    {
        // An agent never displaces a person — precedence runs one way only.
        SessionOptions person;
        person.state_path = state;
        person.actor = "migri";
        person.kind = SessionKind::Human;
        Session gui(app, person);
        REQUIRE(gui.start());

        SessionOptions agent;
        agent.state_path = state;
        agent.actor = "eager-agent";
        agent.kind = SessionKind::Agent;
        Session bot(app, agent);
        REQUIRE(!bot.start());
        REQUIRE(bot.error().find("A person is using this document") !=
                std::string::npos);

        // Nor does one person silently evict another.
        Session other(app, person);
        REQUIRE(!other.start());
        gui.close();
    }

    // ── 4. --no-save really writes nothing ───────────────────────────────────
    // The honest way to let an agent LOOK at an application.
    {
        SessionOptions o;
        o.state_path = state;
        o.save_on_close = false;
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(s.dispatch("rune new note ghost").ok);
        s.close();
    }
    {
        SessionOptions o;
        o.state_path = state;
        o.save_on_close = false;
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(!s.dispatch("get ghost text").ok); // never landed
        s.close();
    }

    // ── 5. batch() is atomic and is ONE undo frame ───────────────────────────
    // A half-applied multi-step edit is the state a person cannot review.
    {
        SessionOptions o;
        o.state_path = state;
        o.actor = "batcher";
        Session s(app, o);
        REQUIRE(s.start());
        Result bad = s.batch({"rune new note first", "rune new nosuchglyph second"});
        REQUIRE(!bad.ok);
        s.close();
    }
    {
        SessionOptions o;
        o.state_path = state;
        Session s(app, o);
        REQUIRE(s.start());
        // Rolled back entirely: the good command did not survive the bad one.
        REQUIRE(!s.dispatch("get first text").ok);
        s.close();
    }
    {
        SessionOptions o;
        o.state_path = state;
        o.actor = "batcher";
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(s.batch({"rune new note a1", "rune new note a2"}).ok);
        REQUIRE(s.dispatch("get a1 text").ok);
        REQUIRE(s.dispatch("get a2 text").ok);
        // One frame: a single undo puts BOTH back.
        REQUIRE(s.dispatch("undo").ok);
        REQUIRE(!s.dispatch("get a1 text").ok);
        REQUIRE(!s.dispatch("get a2 text").ok);
        s.close();
    }

    // ── 6. A registered action runs with no view present ─────────────────────
    // The piece that lets an agent use a VIEW's vocabulary without the view.
    {
        SessionOptions o;
        o.state_path = state;
        o.actor = "action-runner";
        Session s(app, o);
        REQUIRE(s.start());
        Scene empty; // no canvas, no projection needed for this action
        std::vector<std::string> cmds =
            app.actions.run("note-at", empty,
                            {{"name", "placed"}, {"body", "from an action"},
                             {"x", "40"}, {"y", "80"}});
        REQUIRE(cmds.size() == 3);
        REQUIRE(s.batch(cmds).ok);
        Result got = s.dispatch("get placed text");
        REQUIRE(got.ok);
        REQUIRE(got.text().find("from an action") != std::string::npos);
        Result pl = s.dispatch("place placed");
        REQUIRE(pl.ok);
        REQUIRE(pl.data.find("40") != std::string::npos);
        s.close();
    }

    /* ── 6b. The cross-process review story ──────────────────────────────────
     *
     * The claim this pins is the one the concept doc got WRONG on the first
     * draft and that driving the binary corrected: `undo` is session-scoped and
     * cannot reach an earlier run, but `_baseline` is model content and can. So
     * a person arriving after an agent finds its work as unsaved changes, sees
     * it with `status`/`diff`, and can throw all of it away with `revert`.
     *
     * This only holds because Session::save() writes the DOCUMENT and does not
     * dispatch the `save` VERB — which would snapshot the baseline and erase
     * exactly this diff. It is the load-bearing half of an otherwise invisible
     * distinction, so it is checked rather than commented. */
    {
        SessionOptions o;
        o.state_path = state;
        Session s(app, o);
        REQUIRE(s.start());

        // Nothing from any earlier session is undoable here.
        REQUIRE(!s.dispatch("undo").ok);

        // But everything earlier sessions did is visible as a change set.
        Result st = s.dispatch("status");
        REQUIRE(st.ok);
        REQUIRE(st.text().find("placed") != std::string::npos); // from step 6

        REQUIRE(s.dispatch("revert").ok);
        REQUIRE(!s.dispatch("get placed text").ok); // the whole run, discarded
        s.close();
    }

    // ── 7. The briefing describes this app to a stranger ─────────────────────
    {
        SessionOptions o;
        o.state_path = state;
        o.save_on_close = false;
        Session s(app, o);
        REQUIRE(s.start());
        std::string brief = capabilities(app, s.core(), s.state_path());
        REQUIRE(brief.find("maiznotes") != std::string::npos);
        REQUIRE(brief.find("note-at") != std::string::npos);   // the action
        REQUIRE(brief.find("\"note\"") != std::string::npos);  // the glyph
        REQUIRE(brief.find("overdue") != std::string::npos);   // a predicate
        REQUIRE(brief.find("state_path") != std::string::npos);
        REQUIRE(brief.find("house_rules") != std::string::npos);
        s.close();
    }

    /* ── 7b. The effect gate: the one-way door ────────────────────────────────
     *
     * Everything above lands in the document and a person can `revert` it. An
     * effect cannot be reverted, so the default refuses — and these checks are
     * about the two things that make a gate real rather than decorative: the
     * refusal FAILS the command (so a caller branches correctly) and the effect
     * genuinely DID NOT RUN (witnessed, not assumed). */
    const fs::path witness = dir / "effects.log";
    auto witnessed = [&]() -> std::string {
        std::ifstream in(witness);
        if (!in) return {};
        return std::string((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    };

    {   // Default policy: refused, and the refusal is a FAILURE.
        SessionOptions o;
        o.state_path = state;
        o.actor = "agent";
        Session s(app, o);
        REQUIRE(s.start());
        Result r = s.dispatch("deploy");
        REQUIRE(!r.ok);
        REQUIRE(r.text().find("refused") != std::string::npos);
        // The host's own words about what it was asking for.
        REQUIRE(r.text().find("public URL") != std::string::npos);
        s.close();
    }
    REQUIRE(witnessed().find("deploy") == std::string::npos); // it did not run

    {   /* `save` is the one that MUST be gated at the verb: the core snapshots
         * `_baseline` whatever the adapter returned, so a handler-level refusal
         * would still erase the diff a person reviews. Prove the baseline did
         * not move — the document must still show unsaved changes afterwards. */
        SessionOptions o;
        o.state_path = state;
        Session s(app, o);
        REQUIRE(s.start());
        // Step 6b's `revert` took the mantles back to the baseline, so make one.
        REQUIRE(s.dispatch("mantle new review").ok);
        REQUIRE(s.dispatch("rune new note reviewme").ok);
        REQUIRE(!s.dispatch("save").ok);
        Result st = s.dispatch("status");
        REQUIRE(st.ok);
        REQUIRE(st.text().find("reviewme") != std::string::npos);
        s.close();
    }

    {   // A batch cannot smuggle an effect past a check on the leading verb.
        SessionOptions o;
        o.state_path = state;
        Session s(app, o);
        REQUIRE(s.start());
        Result r = s.batch({"rune new note innocent", "deploy"});
        REQUIRE(!r.ok);
        REQUIRE(r.text().find("refused") != std::string::npos);
        s.close();
    }
    REQUIRE(witnessed().find("deploy") == std::string::npos);

    {   /* NEITHER CAN A QUOTED VERB. The gate used to read the leading token by
         * hand — skip whitespace, take the next run — so `'deploy'` presented
         * as the six-character token `'deploy'`, matched no effect verb, and
         * walked through. The tokenizer that RUNS the command strips those
         * quotes, so the gate and the dispatcher disagreed about what the verb
         * was, which is the only thing a gate must never do. It now asks
         * maiz::split_argv, i.e. the same tokenizer. */
        SessionOptions o;
        o.state_path = state;
        Session s(app, o);
        REQUIRE(s.start());
        Result q = s.dispatch("'deploy'");
        REQUIRE(!q.ok);
        REQUIRE(q.text().find("refused") != std::string::npos);
        // The same for a batch whose payload quotes its verb.
        Result b = s.batch({"rune new note innocent2", "\"deploy\""});
        REQUIRE(!b.ok);
        REQUIRE(b.text().find("refused") != std::string::npos);
        s.close();
    }
    REQUIRE(witnessed().find("deploy") == std::string::npos);

    {   /* A VALUE THAT LOOKS LIKE AN EFFECT IS NOT ONE. The other half of the
         * same property: the gate must not refuse `set note text 'deploy'`,
         * because argv[0] is `set`. A gate that reads text instead of argv gets
         * this wrong in whichever direction its heuristic happens to lean. */
        SessionOptions o;
        o.state_path = state;
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(s.dispatch("rune new note deployish").ok);
        REQUIRE(s.dispatch("set deployish text " + arg("deploy the site")).ok);
        Result got = s.dispatch("get deployish text");
        REQUIRE(got.ok);
        REQUIRE(got.text().find("deploy the site") != std::string::npos);
        s.close();
    }

    {   // Dry run: SUCCEEDS at rehearsing, still runs nothing.
        SessionOptions o;
        o.state_path = state;
        o.effects = EffectPolicy::DryRun;
        Session s(app, o);
        REQUIRE(s.start());
        Result r = s.dispatch("deploy");
        REQUIRE(r.ok); // it did what it was asked: rehearse
        REQUIRE(r.text().find("dry run") != std::string::npos);
        REQUIRE(r.text().find("Publish") != std::string::npos);
        s.close();
    }
    REQUIRE(witnessed().find("deploy") == std::string::npos);

    {   // A named grant permits exactly one op and no more.
        SessionOptions o;
        o.state_path = state;
        o.effects = EffectPolicy::Allow;
        o.allowed_effects = {"build"};
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(s.dispatch("build").ok);
        REQUIRE(!s.dispatch("deploy").ok); // still shut
        s.close();
    }
    REQUIRE(witnessed().find("build") != std::string::npos);  // ran
    REQUIRE(witnessed().find("deploy") == std::string::npos); // did not

    {   /* And a full grant opens the door — with the effect STREAMING as it
         * runs (SPEC §9), rather than going quiet and then claiming success.
         * Every emitted line reaches the caller's sink and the journal. */
        std::vector<std::string> streamed;
        SessionOptions o;
        o.state_path = state;
        o.effects = EffectPolicy::Allow; // empty list = all
        o.on_effect_line = [&](std::string_view op, std::string_view line) {
            streamed.push_back(std::string(op) + ": " + std::string(line));
        };
        Session s(app, o);
        REQUIRE(s.start());
        REQUIRE(s.dispatch("deploy").ok);
        REQUIRE(streamed.size() == 3);
        if (streamed.size() == 3) {
            REQUIRE(streamed[0] == "deploy: rendering 3 notes");
            REQUIRE(streamed[2] == "deploy: done");
        }
        s.close();
    }
    REQUIRE(witnessed().find("deploy") != std::string::npos);
    {
        // …and the journal has them too, so a person reading afterwards sees
        // what the deploy actually said, not just that one happened.
        std::ifstream in(fs::path(state + ".log"));
        std::string all((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
        REQUIRE(all.find("uploading to the live site") != std::string::npos);
    }

    {   // The briefing tells an agent the policy BEFORE it plans.
        SessionOptions o;
        o.state_path = state;
        o.save_on_close = false;
        Session s(app, o);
        REQUIRE(s.start());
        std::string brief = capabilities(app, s.core(), s.state_path(), &o);
        REQUIRE(brief.find("\"policy\":\t\"refuse\"") != std::string::npos ||
                brief.find("\"policy\":") != std::string::npos);
        REQUIRE(brief.find("public URL") != std::string::npos);
        REQUIRE(brief.find("one-way door") != std::string::npos);
        s.close();
    }

    // ── 8. A corrupt document is refused, not silently emptied ───────────────
    // Core's own contract yields the empty state for a non-object; inheriting
    // that here would answer "your document is corrupt" by deleting it.
    {
        const std::string bad = (dir / "bad.state.json").string();
        { std::ofstream out(bad); out << "this is not json"; }
        SessionOptions o;
        o.state_path = bad;
        Session s(app, o);
        REQUIRE(!s.start());
        REQUIRE(s.error().find("Refusing") != std::string::npos);
        // Still there, untouched.
        REQUIRE(fs::exists(bad));
        REQUIRE(fs::file_size(bad) > 0);
    }

    fs::remove_all(dir, ec);
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--selftest") {
        selftest();
        if (failures == 0) {
            std::cout << "OK - headless smoke passed\n";
            return 0;
        }
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    /* The entire terminal front-end of an application, in one line. */
    return maiz::run_cli(build_app(), argc, argv);
}
