/*
 * voidmaiz/embed.hpp — the C++20 embedding of Void Core (Phase 1).
 *
 * A thin RAII layer over the pure C ABI in voidcore.h: manager lifetime,
 * string ownership, the {ok,lines,data} dispatch envelope, glyph registration,
 * and log-sink / effect-handler binding as std::function. Usable by ANY C++
 * Void Core application, UI or not — Void Maiz's canvas is one client of it.
 *
 * Ownership: maiz::Core owns one VC_Manager. Every char* the C ABI returns is
 * released with vc_free_str inside this layer; nothing library-allocated
 * escapes to the caller.
 *
 * Threading (inherited contract, voidcore.h / SPEC §6): a Core is NOT
 * thread-safe — serialize calls on the same instance or confine it to one
 * thread. Distinct Core instances are fully independent. Callbacks (log sink,
 * effect handler) are invoked synchronously on the dispatching thread, inside
 * the dispatch — do not re-enter the same Core from them. tag_match() and
 * core_version() are stateless and safe from any thread.
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct VC_Manager; // opaque; defined by voidcore

namespace maiz {

inline constexpr std::string_view kVoidmaizVersion = "0.1.0";

/* One dispatch's parsed envelope (SPEC §6): {"ok":bool,"lines":[...],"data":any}. */
struct Result {
    bool ok = false;
    std::vector<std::string> lines; // human-readable output, one entry per line
    std::string data = "null";      // the machine value, re-serialized as compact JSON
    explicit operator bool() const { return ok; }
    /* All lines joined with '\n' — what a CLI or log strip prints. */
    std::string text() const;
};

/* ── the SPEC §6.1 command codec ─────────────────────────────────────────────
 *
 * NOT IMPLEMENTED HERE. Every function below forwards to Void Core 0.2.7's
 * exported codec (`vc_arg_quote`, `vc_argv_split_json`,
 * `vc_transcript_split_json`) and does nothing of its own but move bytes across
 * the ABI. That is the whole point.
 *
 * WHY. §6.1 is a rule every host must implement twice — once to encode a value
 * into a command, once to decode a command it is about to run — and FIVE
 * independent codebases implemented it wrong, the reference core included:
 * Hormiga emitted `\''` for an apostrophe, so an Allomone comment saying
 * `don't` truncated the rest of the script; Reyna wrote Python's
 * `s.replace("\'", "'")`, a no-op that left every apostrophe in a corpus of
 * municipal policy as `\'`; and OUR OWN four-line `arg()` — correct on the day
 * it was written, and reviewed twice — quietly produced an unterminated
 * argument for any value ending in a backslash, because a trailing `\` lands
 * against the closing quote and rule 3 reads the pair as an escaped apostrophe.
 * `C:\` became `C:'` and ate the rest of the line, with ok:true.
 *
 * That last one is the argument. A careful reimplementation is the DANGEROUS
 * kind: it is right on the day it ships and goes quietly wrong when the rule
 * moves, which rule 5 did on 2026-08-25. So Void Maiz implements none of it.
 * The law Core pins, and that command_smoke re-checks on this side of the ABI:
 *
 *     split_argv(arg(v)).argv == {v}     for every NUL-free byte string v
 *
 * All of these are pure, stateless and thread-safe: no Core handle, usable
 * before a Core exists and from a headless build. */

/* Wrap a value as exactly ONE dispatcher argument — newlines, quotes,
 * backslashes and control characters all survive. */
std::string arg(std::string_view value);

/* Join an argv back into one command line, each element quoted as itself.
 *
 * This is what a front-end handed an OS argv must call. An argv element is
 * ALREADY one argument; re-joining on spaces and letting the tokenizer split it
 * again is how `set maria role "Outreach Coordinator"` silently stores
 * `Outreach` (reported by Hormiga against a real build, 2026-08-25). Every
 * element is quoted, including ones that would have survived bare: which values
 * are bare-safe is itself a §6.1 rule, and this file does not know §6.1. */
std::string command_line(const std::vector<std::string>& argv);

/* One tokenized command line — the DECODER half, exactly as vc_dispatch will
 * split it. For a host that must inspect a command BEFORE running it (an effect
 * gate reading argv[0], a submission review) rather than guess from the text. */
struct Argv {
    bool ok = false;
    std::vector<std::string> argv;
    std::string error; // set when !ok — e.g. an unterminated quote (rule 5)
    explicit operator bool() const { return ok; }
};
Argv split_argv(std::string_view line);

/* One statement of a transcript, with the line it started on, its raw text (the
 * form to dispatch) and its decoded argv (the form to inspect). */
struct Statement {
    int line = 0;
    std::string text;
    std::vector<std::string> argv;
};

/* A whole transcript, split into the statements it will run.
 *
 * Boundaries are newline and `;` but ONLY outside a quoted run, so a newline
 * inside a value is data — which is the only way a multi-line value (a bio, a
 * description, two paragraphs a volunteer typed) can be scripted at all.
 * Reading a transcript line-by-line and quoting afterwards cuts such a value in
 * half and turns its tail into commands; before Core 0.2.7 that was a command
 * injection, and it is why nothing here splits on '\n' itself.
 *
 * `flat` is false if any statement opens a block or begins with a SPEC §8
 * control word — a flat transcript is one whose effect can be read off its
 * statements without simulating it, which is what a gate wants to know. */
struct Transcript {
    bool ok = false;
    bool flat = false;
    std::vector<Statement> commands;
    std::string error;   // set when !ok
    int error_line = 0;  // 1-based line the bad statement started on
    explicit operator bool() const { return ok; }
};
Transcript split_transcript(std::string_view src);

class Core {
public:
    /* Receives every log line live. `who` is empty unless config.actor is set
     * upstream (attribution, SPEC §9, rides inside msg for `log` records —
     * this callback forwards the raw level/op/msg triple unchanged). */
    using LogSink = std::function<void(std::string_view level,
                                       std::string_view op,
                                       std::string_view msg)>;
    /* Handles effectful ops (save/deploy/build/preview). Return a JSON string,
     * or an empty string for "no result" (forwarded to the core as NULL). */
    using EffectHandler = std::function<std::string(std::string_view op,
                                                    std::string_view args_json)>;

    /* Empty state document. Throws std::runtime_error on allocation failure
     * (the only way vc_create returns NULL). */
    Core();
    /* Replay a saved state document (export_state round-trips through here).
     * Malformed/non-object input yields the empty state — the core's contract. */
    explicit Core(std::string_view state_json);
    ~Core();

    Core(Core&& other) noexcept;
    Core& operator=(Core&& other) noexcept;
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    /* Dispatch one command line and parse the envelope. */
    Result dispatch(std::string_view command);
    /* Dispatch and hand back the raw envelope JSON, unparsed. */
    std::string dispatch_raw(std::string_view command);

    /* Serialize the full state document (SPEC §2). */
    std::string export_state() const;

    /* Register (or override) a glyph from its JSON descriptor. Glyphs are host
     * config, NOT exported state — re-register after every fresh Core. */
    bool register_glyph(std::string_view glyph_json);

    /* Install/replace the log sink; pass {} to remove. */
    void set_log_sink(LogSink sink);
    /* Install/replace the effect handler; pass {} to remove. */
    void set_effect_handler(EffectHandler handler);

    /* SPEC §5 filter expression against a bag of tags, stateless (no manager).
     * Include the entity's name in `tags` for name-as-tag matching, and
     * "glyph:<g>" for glyph matching. Throws std::invalid_argument on
     * malformed input (the ABI's -1). Empty expression matches. */
    static bool tag_match(std::string_view expr,
                          const std::vector<std::string>& tags);

    /* The linked Void Core's version string (e.g. "0.2.3"). */
    static std::string_view core_version();

    /* The underlying handle, for calls this layer doesn't wrap yet. Never
     * destroy it, and never free strings from it with anything but the escape
     * hatch of voidcore.h itself. */
    VC_Manager* raw() noexcept { return m_; }

private:
    struct Callbacks; // stable-address home for the std::function targets
    VC_Manager* m_ = nullptr;
    std::unique_ptr<Callbacks> cb_;
};

} // namespace maiz
