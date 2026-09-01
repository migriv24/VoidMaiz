/* headless.cpp — Session, capabilities() and run_cli().
 *
 * The whole file is I/O and argument shuffling around one idea: a Void Maiz
 * application is a dispatcher over a state document, and a front-end is
 * whatever puts command lines into it. There is deliberately no interpretation
 * of commands here, no verb of our own, and no path that mutates the document
 * without going through Core::dispatch (okf/concepts/headless.md). */
#include "voidmaiz/headless.hpp"

#include "cJSON.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace maiz {

namespace {

struct CJson {
    cJSON* p = nullptr;
    ~CJson() { cJSON_Delete(p); }
    explicit operator bool() const { return p != nullptr; }
};

std::string slurp(const fs::path& p, bool& ok) {
    std::ifstream in(p, std::ios::binary);
    if (!in) { ok = false; return {}; }
    std::ostringstream ss;
    ss << in.rdbuf();
    ok = in.good() || in.eof();
    return ss.str();
}

/* Write via a temporary and rename. A state document half-written because the
 * disk filled or the process died is worse than one not written at all: the
 * user's work is gone AND the file looks present. Rename is atomic on both
 * platforms we target. */
bool spew_atomic(const fs::path& p, const std::string& data, std::string& err) {
    fs::path tmp = p;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { err = "cannot open " + tmp.string() + " for writing"; return false; }
        out.write(data.data(), (std::streamsize)data.size());
        out.flush();
        if (!out) { err = "write failed: " + tmp.string(); return false; }
    }
    std::error_code ec;
    fs::rename(tmp, p, ec);
    if (ec) {
        // Windows rename over an existing file can fail; remove and retry once.
        fs::remove(p, ec);
        ec.clear();
        fs::rename(tmp, p, ec);
    }
    if (ec) {
        err = "cannot replace " + p.string() + ": " + ec.message();
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

/* ── the advisory lock ───────────────────────────────────────────────────────
 *
 * ADVISORY, and the header says so. It exists to make "a person had the GUI
 * open and an agent overwrote their afternoon" LOUD rather than silent; it is
 * not exclusion against a hostile writer, and a stale file from a killed
 * process must be removable by hand.
 *
 * The file is a small JSON claim rather than a bare name, because precedence
 * needs three facts about the holder: WHO (for the message), WHAT KIND (a
 * person outranks an agent), and a TOKEN — a value only the holder knows, so a
 * session can ask "is the lock still mine?" and get a real answer rather than
 * "is there a lock file?", which stays true after someone else takes it. */
struct LockClaim {
    std::string actor, kind, token, opened;
};

std::string claim_json(const LockClaim& c) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "actor", c.actor.c_str());
    cJSON_AddStringToObject(o, "kind", c.kind.c_str());
    cJSON_AddStringToObject(o, "token", c.token.c_str());
    cJSON_AddStringToObject(o, "opened", c.opened.c_str());
    char* printed = cJSON_PrintUnformatted(o);
    std::string out = printed ? printed : "{}";
    cJSON_free(printed);
    cJSON_Delete(o);
    return out;
}

/* Read a claim. A lock file we cannot parse is treated as held by an unknown
 * HUMAN — the conservative reading, because the one thing precedence must never
 * do is evict a person on the strength of a corrupt file. */
bool read_claim(const fs::path& p, LockClaim& out) {
    bool ok = true;
    std::string text = slurp(p, ok);
    if (!ok) return false;
    CJson doc{cJSON_ParseWithLength(text.data(), text.size())};
    if (!doc || !cJSON_IsObject(doc.p)) {
        out = {text.empty() ? "another session" : text, "human", "", ""};
        return true;
    }
    auto s = [&](const char* k) {
        const cJSON* v = cJSON_GetObjectItemCaseSensitive(doc.p, k);
        return cJSON_IsString(const_cast<cJSON*>(v)) ? std::string(v->valuestring)
                                                     : std::string();
    };
    out = {s("actor"), s("kind").empty() ? "human" : s("kind"), s("token"),
           s("opened")};
    return true;
}

bool write_claim(const fs::path& p, const LockClaim& c) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << claim_json(c);
    return (bool)out;
}

/* Enough entropy that two sessions cannot collide by accident. Not a secret and
 * not trying to be — an advisory lock defends against surprise, not malice. */
std::string mint_token() {
    std::random_device rd;
    std::ostringstream ss;
    ss << std::hex << rd() << rd();
    return ss.str();
}

/* Parse a dispatch result's `data` (compact JSON) and hand back the node, or
 * null. Used only to fold the core's own introspection verbs into the briefing
 * — we re-serialize what the core said rather than restating it, so the
 * briefing cannot drift from the dispatcher. */
cJSON* parse_or_null(const std::string& json) {
    return cJSON_ParseWithLength(json.data(), json.size());
}

void add_parsed(cJSON* into, const char* key, const std::string& json) {
    cJSON* v = parse_or_null(json);
    cJSON_AddItemToObject(into, key, v ? v : cJSON_CreateNull());
}

} // namespace

// ── Session ─────────────────────────────────────────────────────────────────

namespace {

/* ISO-8601 UTC, matching Void Core's log line format (SPEC §9). A clock is
 * legitimate HERE and nowhere else in the library: Session is the I/O layer, and
 * the pure pieces (merge, project, reduce, census) still take none. */
std::string iso_now() {
    using namespace std::chrono;
    const std::time_t t = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

} // namespace

namespace {

/* The verbs that reach the world (SPEC §9). `effect` carries its op as the next
 * token; the rest are their own op. */
bool is_effect_verb(std::string_view v) {
    return v == "save" || v == "deploy" || v == "build" || v == "preview" ||
           v == "effect";
}

/* NO `leading_verb` HERE ANY MORE. This gate used to read the verb by hand —
 * find_first_not_of(" \t"), take the next run — which meant a command whose verb
 * was quoted (`'save' now`) presented as the token `'save'`, matched no effect
 * verb, and walked straight through a gate that exists to stop exactly that.
 * The gate now splits with maiz::split_argv, i.e. with the tokenizer that will
 * actually run the command, so what it inspects and what executes cannot
 * disagree (Hormiga, 2026-08-25). */

} // namespace

struct Session::Impl {
    HostApp app;
    SessionOptions opts;
    Core core;
    fs::path state;
    fs::path lock;
    fs::path journal;
    std::ofstream journal_out;
    bool started = false;
    bool locked = false;
    std::string token;             // our claim on the lock; "" when not locking
    bool lost = false;             // sticky: a Human session took the floor
    std::string preempted_holder;  // the agent WE evicted, if any
    std::string err;

    /* Is the lock file still carrying OUR token? A filesystem touch on purpose:
     * a cached flag cannot notice what another process did. */
    bool still_ours() const {
        if (!locked || token.empty()) return true; // not locking: nothing to lose
        LockClaim held;
        if (!read_claim(lock, held)) return false; // gone = not ours
        return held.token == token;
    }

    const EffectOp* declared(std::string_view op) const {
        for (const EffectOp& e : app.effect_ops)
            if (e.name == op) return &e;
        return nullptr;
    }

    bool permitted(std::string_view op) const {
        if (opts.effects != EffectPolicy::Allow) return false;
        if (opts.allowed_effects.empty()) return true; // explicit "yes, all"
        for (const std::string& a : opts.allowed_effects)
            if (a == op) return true;
        return false;
    }

    /* The sentence a refusal says. It names the consequence when the host
     * declared one, because "deploy is not permitted" teaches nothing and
     * "deploy is not permitted: pushes the site live to the public URL" tells
     * the caller exactly what grant it is missing and why it exists. */
    std::string refusal(std::string_view verb, std::string_view op) const {
        std::string m = "refused: `";
        m += verb;
        m += "` reaches outside the document and this session was not granted "
             "effects";
        if (const EffectOp* e = declared(op); e && !e->consequence.empty()) {
            m += " (";
            m += op;
            m += ": ";
            m += e->consequence;
            m += ")";
        }
        m += ". Re-run with --allow-effects";
        if (!op.empty() && op != verb) { m += "="; m += op; }
        else if (!op.empty()) { m += "="; m += op; }
        m += " if that is intended, or --dry-run-effects to rehearse it.";
        return m;
    }

    std::string dry_run(std::string_view verb, std::string_view op) const {
        std::string m = "dry run: `";
        m += verb;
        m += "` would run";
        if (const EffectOp* e = declared(op); e) {
            if (!e->doc.empty()) { m += " — "; m += e->doc; }
            if (!e->consequence.empty()) { m += " (" + e->consequence + ")"; }
        }
        m += ". Nothing was performed.";
        return m;
    }

    /* Gate one command. Returns true and fills `why` when it must NOT run. */
    bool blocked(std::string_view command, std::string& why) const {
        const Argv a = split_argv(command);
        /* An unterminated quote is not a gate decision: nothing is permitted or
         * refused, the command is malformed and vc_dispatch will say so with a
         * better sentence than we could. Let it through to fail there. */
        if (!a.ok || a.argv.empty()) return false;

        const std::string& verb = a.argv[0];
        /* `effect` carries its op as the next argument; the rest are their own
         * op (SPEC §9). */
        const std::string op =
            (verb == "effect" && a.argv.size() > 1) ? a.argv[1] : verb;

        /* `batch` applies a JSON array atomically, which makes it the obvious
         * way to smuggle an effect past a check on the leading verb. Read it —
         * from the DECODED argument, so the payload we parse is byte-identical
         * to the one the core will parse. (This used to hunt for the first and
         * last `'` in the line and undo the escaping by hand, which is a second
         * §6.1 implementation with all the usual properties.) */
        if (verb == "batch") {
            if (a.argv.size() < 2) return false;
            const std::string& payload = a.argv[1];
            CJson arr{cJSON_ParseWithLength(payload.data(), payload.size())};
            if (!arr || !cJSON_IsArray(arr.p)) return false;
            const cJSON* it = nullptr;
            cJSON_ArrayForEach(it, arr.p) {
                if (!cJSON_IsString(const_cast<cJSON*>(it))) continue;
                if (blocked(it->valuestring, why)) return true;
            }
            return false;
        }

        if (!is_effect_verb(verb)) return false;
        if (opts.effects == EffectPolicy::DryRun) { why = dry_run(verb, op); return true; }
        if (permitted(op)) return false;
        why = refusal(verb, op);
        return true;
    }
};

Session::Session(const HostApp& app, SessionOptions opts)
    : p_(std::make_unique<Impl>()) {
    p_->app = app;
    p_->opts = std::move(opts);
    if (p_->opts.state_path.empty())
        p_->opts.state_path = (p_->app.id.empty() ? "voidmaiz" : p_->app.id) +
                              ".state.json";
    p_->state = fs::path(p_->opts.state_path);
    p_->lock = p_->state;
    p_->lock += ".lock";
    if (p_->opts.journal_path.empty()) {
        p_->journal = p_->state;
        p_->journal += ".log";
    } else {
        p_->journal = fs::path(p_->opts.journal_path);
    }
}

Session::~Session() {
    if (p_) close();
}

Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

bool Session::start() {
    Impl& s = *p_;
    if (s.started) return true;
    s.err.clear();

    if (s.opts.lock) {
        const bool we_are_human = s.opts.kind == SessionKind::Human;
        LockClaim mine{s.opts.actor.empty() ? "an unnamed session" : s.opts.actor,
                       we_are_human ? "human" : "agent", mint_token(), iso_now()};

        /* Exclusive create: the check and the create are ONE operation, so two
         * sessions starting at the same instant cannot both win. */
        bool got = false;
        if (std::FILE* f = std::fopen(s.lock.string().c_str(), "wbx")) {
            const std::string text = claim_json(mine);
            std::fwrite(text.data(), 1, text.size(), f);
            std::fclose(f);
            got = true;
        }

        if (!got) {
            LockClaim held;
            if (!read_claim(s.lock, held)) held = {"another session", "human", "", ""};

            /* THE PRECEDENCE RULE, entire: a human may take it from an agent.
             * Nothing else preempts anything — two humans collide normally, and
             * an agent never displaces anyone. */
            const bool may_preempt = we_are_human && held.kind == "agent";
            if (!may_preempt) {
                s.err = "the state document is held by " +
                        (held.actor.empty() ? std::string("another session")
                                            : held.actor) +
                        " (" + held.kind + ", since " +
                        (held.opened.empty() ? "unknown" : held.opened) + "). ";
                /* The advice depends on WHO IS HOLDING IT, not on who is
                 * asking — an agent refused by another agent was being told "a
                 * person is using this document", which is both false and
                 * unactionable (caught by driving it, 2026-08-18). */
                if (held.kind == "human" && !we_are_human)
                    s.err += "A person is using this document; an agent does "
                             "not take the floor from them. Try again later.";
                else if (held.kind == "agent" && !we_are_human)
                    s.err += "Another agent session holds it. Try again later, "
                             "or delete " + s.lock.string() + " if it is gone.";
                else
                    s.err += "Close that session, or delete " + s.lock.string() +
                             " if it is gone.";
                return false;
            }
            if (!write_claim(s.lock, mine)) {
                s.err = "cannot take the lock at " + s.lock.string();
                return false;
            }
            s.preempted_holder = held.actor;
        }
        s.token = mine.token;
        s.locked = true;
    }

    /* Load. An ABSENT file is a fresh document, not an error — a first run has
     * to start somewhere, and refusing would make "point your agent at a new
     * folder" impossible. An UNREADABLE or MALFORMED file IS an error: Core's
     * own contract silently yields the empty state for a non-object, and
     * inheriting that here would answer "your document is corrupt" by throwing
     * it away. */
    if (fs::exists(s.state)) {
        bool ok = true;
        std::string doc = slurp(s.state, ok);
        if (!ok) {
            s.err = "cannot read " + s.state.string();
            close();
            return false;
        }
        CJson probe{parse_or_null(doc)};
        if (!probe || !cJSON_IsObject(probe.p)) {
            s.err = "the state document at " + s.state.string() +
                    " is not a JSON object. Refusing to start rather than "
                    "replacing it with an empty one.";
            close();
            return false;
        }
        s.core = Core(doc);
    }

    // Glyphs are host config and are NOT carried by the document (embed.hpp),
    // so they must be re-registered on every fresh Core — which is exactly why
    // they live in HostApp rather than in each front-end's main().
    for (const std::string& g : s.app.glyphs) s.core.register_glyph(g);

    /* The BACKSTOP gate. The verb gate above is the one that works (it stops
     * the baseline snapshot and can fail the dispatch); this one exists for
     * anything that reaches the seam by a route the verb gate cannot read — a
     * `script` body, or a future verb that calls the handler.
     *
     * It cannot fail the command: the core builds the result with `res_make(1)`
     * regardless of what comes back, so refusing here can only decline to DO
     * the thing and say so in `data`. That is a real limitation and it is why
     * this is the second gate rather than the only one. */
    if (s.app.effects || s.app.streaming_effects) {
        Impl* self = &s;
        Core::EffectHandler host = s.app.effects;
        StreamingEffectHandler streaming = s.app.streaming_effects;
        s.core.set_effect_handler(
            [self, host, streaming](std::string_view op,
                                    std::string_view args) -> std::string {
                if (self->opts.effects == EffectPolicy::Allow &&
                    self->permitted(op)) {
                    if (!streaming) return host ? host(op, args) : std::string();
                    /* SPEC §9: a long operation streams line-by-line. Each line
                     * reaches the journal and the caller's sink AS IT HAPPENS,
                     * which is what makes an unattended deploy diagnosable when
                     * it dies at line 40 of 60 rather than merely failed.
                     *
                     * Flushed per line for the same reason the journal is: the
                     * run that gets killed is exactly the run whose last lines
                     * matter, and a buffer takes them with it. */
                    const std::string op_s(op);
                    EffectEmit emit = [self, &op_s](std::string_view line) {
                        if (self->journal_out) {
                            self->journal_out
                                << "[" << iso_now() << "] INFO " << op_s
                                << (self->opts.actor.empty()
                                        ? ""
                                        : " (" + self->opts.actor + ")")
                                << ": " << line << "\n";
                            self->journal_out.flush();
                        }
                        if (self->opts.on_effect_line)
                            self->opts.on_effect_line(op_s, line);
                    };
                    return streaming(op, args, emit);
                }
                const bool dry = self->opts.effects == EffectPolicy::DryRun;
                if (self->journal_out) {
                    self->journal_out
                        << "[" << iso_now() << "] WARN effect-gate"
                        << (self->opts.actor.empty()
                                ? ""
                                : " (" + self->opts.actor + ")")
                        << ": backstop " << (dry ? "dry-run" : "refused") << " for `"
                        << op << "`\n";
                    self->journal_out.flush();
                }
                cJSON* o = cJSON_CreateObject();
                cJSON_AddStringToObject(o, "maiz_effect",
                                        dry ? "dry-run" : "refused");
                cJSON_AddStringToObject(o, "op", std::string(op).c_str());
                cJSON_AddStringToObject(
                    o, "reason",
                    dry ? "dry run: nothing was performed"
                        : "this session was not granted effects");
                char* printed = cJSON_PrintUnformatted(o);
                std::string out = printed ? printed : "{}";
                cJSON_free(printed);
                cJSON_Delete(o);
                return out;
            });
    }

    /* Persist the mutation spine. APPEND, never truncate: a journal that lost
     * the previous run would answer "what did the agent do" with only the most
     * recent answer, which is the question a person asks precisely when several
     * runs have happened. A journal that cannot be opened is a warning, not a
     * failure — refusing to work because a log file is read-only would be worse
     * than working unrecorded and saying so. */
    if (s.opts.journal) {
        s.journal_out.open(s.journal, std::ios::app);
        if (s.journal_out) {
            const std::string who = s.opts.actor.empty() ? "" : " (" + s.opts.actor + ")";
            s.journal_out << "[" << iso_now() << "] INFO session" << who
                          << ": opened " << s.state.string() << "\n";
            s.journal_out.flush();
            Impl* self = &s;
            s.core.set_log_sink([self](std::string_view level, std::string_view op,
                                       std::string_view msg) {
                if (!self->journal_out) return;
                const std::string who =
                    self->opts.actor.empty() ? "" : " (" + self->opts.actor + ")";
                self->journal_out << "[" << iso_now() << "] " << level << " " << op
                                  << who << ": " << msg << "\n";
                /* Flushed per record, deliberately. A long agent task that is
                 * killed halfway is exactly when the journal matters most, and
                 * a buffer would take the last and most interesting lines with
                 * it. Headless throughput is command-rate, not log-rate. */
                self->journal_out.flush();
            });
        } else {
            s.err = "cannot append to journal " + s.journal.string() +
                    " (continuing unrecorded)";
        }
    }

    /* Attribution before anything is dispatched, so the very first command is
     * already attributed (SPEC §9). This is the whole review story: a person
     * opening the app afterwards can tell an agent's changes from their own. */
    if (!s.opts.actor.empty())
        s.core.dispatch("config set actor " + arg(s.opts.actor));

    if (s.app.configure) s.app.configure(s.core);

    s.started = true;
    return true;
}

Result Session::dispatch(std::string_view command) {
    Impl& s = *p_;

    /* Did a person take the floor? Checked BEFORE the command runs, so a long
     * agent task stops at the next step rather than doing another four hundred
     * into a document it can no longer write. One filesystem read per command
     * is nothing at headless command rates, and the alternative — noticing only
     * at save — means discovering it after all the work.
     *
     * Sticky, and deliberately not recoverable: even if they close and the lock
     * frees, whatever they did in between IS the document now, and this
     * session's in-memory copy is a fork of it. */
    if (!s.lost && s.locked && !s.still_ours()) {
        s.lost = true;
        s.err = "a person took the floor on this document; this session no "
                "longer holds it and will not save.";
        if (s.journal_out) {
            s.journal_out << "[" << iso_now() << "] WARN session"
                          << (s.opts.actor.empty() ? "" : " (" + s.opts.actor + ")")
                          << ": preempted by a human session; stopping\n";
            s.journal_out.flush();
        }
    }
    if (s.lost) {
        Result r;
        r.ok = false;
        r.lines.push_back(
            "stopped: a person opened this document and takes precedence. "
            "Nothing further will be written. Re-run when they are done — the "
            "document on disk is theirs, not this session's.");
        r.data = "null";
        return r;
    }

    if (s.opts.on_command) s.opts.on_command(command);

    /* The verb gate — BEFORE the core sees it, because `save` snapshots the
     * baseline whatever its adapter did, and because a handler that returns an
     * error still yields ok==true (see headless.hpp). Refusing here is the only
     * place where "did not happen" and "reported as failed" are both true. */
    std::string why;
    if (s.blocked(command, why)) {
        /* Recorded on the spine, not just returned. A refusal is part of what
         * the agent did, and a person reading the journal afterwards should see
         * where it was stopped — that is half the value of stopping it. */
        if (s.journal_out) {
            s.journal_out << "[" << iso_now() << "] WARN effect-gate"
                          << (s.opts.actor.empty() ? "" : " (" + s.opts.actor + ")")
                          << ": " << why << " | " << command << "\n";
            s.journal_out.flush();
        }
        Result r;
        /* A dry run SUCCEEDED at what it was asked to do — rehearse — so it is
         * ok; a refusal is a failure, because the caller did not get what it
         * asked for and must be able to branch on that. */
        r.ok = s.opts.effects == EffectPolicy::DryRun;
        r.lines.push_back(why);
        r.data = "null";
        return r;
    }
    return s.core.dispatch(command);
}

Result Session::batch(const std::vector<std::string>& commands) {
    if (commands.empty()) return Result{true, {}, "null"};
    /* Void Core's `batch` takes a JSON array of command strings and applies it
     * atomically as ONE undo frame (SPEC §7). Building the array with cJSON
     * rather than by hand because a command line is full of quotes, and this is
     * precisely the escaping that bit two hosts (see maiz::arg). */
    cJSON* arr = cJSON_CreateArray();
    for (const std::string& c : commands)
        cJSON_AddItemToArray(arr, cJSON_CreateString(c.c_str()));
    char* printed = cJSON_PrintUnformatted(arr);
    std::string payload = printed ? printed : "[]";
    cJSON_free(printed);
    cJSON_Delete(arr);
    return dispatch("batch " + arg(payload));
}

bool Session::save() {
    Impl& s = *p_;
    if (!s.started) { s.err = "session not started"; return false; }
    /* THE LINE THAT MAKES PRECEDENCE REAL. Everything else is a courtesy
     * message; this is the refusal that keeps a person's document from being
     * overwritten by a session that no longer holds the floor. Re-checked here
     * even though dispatch() checks too, because save() is reachable directly
     * and this is the only place where the harm actually happens. */
    if (s.lost || (s.locked && !s.still_ours())) {
        s.lost = true;
        s.err = "refusing to save: a person took the floor on this document "
                "while this session was working. Their version is on disk; this "
                "session's changes were not written.";
        return false;
    }
    std::string doc = s.core.export_state();
    if (!spew_atomic(s.state, doc, s.err)) return false;
    return true;
}

void Session::close() {
    Impl& s = *p_;
    if (s.started && s.opts.save_on_close && !s.lost) save();
    if (s.journal_out) {
        s.journal_out << "[" << iso_now() << "] INFO session"
                      << (s.opts.actor.empty() ? "" : " (" + s.opts.actor + ")")
                      << ": closed\n";
        s.journal_out.close();
    }
    s.core.set_log_sink({});
    s.started = false;
    if (s.locked) {
        /* Only remove a lock that is still OURS. Deleting it after being
         * preempted would drop the floor out from under the person who took
         * it — a tidy-up that hands the document to whoever asks next. */
        if (!s.lost && s.still_ours()) {
            std::error_code ec;
            fs::remove(s.lock, ec);
        }
        s.locked = false;
    }
}

bool Session::holds_lock() const {
    const Impl& s = *p_;
    if (s.lost) return false;
    return s.still_ours();
}

bool Session::preempted() const { return p_->lost; }

Core& Session::core() { return p_->core; }
const std::string& Session::state_path() const { return p_->opts.state_path; }
const std::string& Session::error() const { return p_->err; }

// ── the briefing ────────────────────────────────────────────────────────────

/* The briefing's effects section. Two questions, and an agent needs both
 * answered before it plans: what can this application do to the world, and am I
 * allowed to. Reporting only the first would produce an agent that builds a
 * whole deploy and then discovers the door is shut; reporting only the second
 * would produce one that never knows what it was missing. */
static cJSON* effects_section(const HostApp& app, const SessionOptions* opts) {
    cJSON* sec = cJSON_CreateObject();
    const char* policy = "refuse";
    if (opts) {
        switch (opts->effects) {
        case EffectPolicy::Allow: policy = "allow"; break;
        case EffectPolicy::DryRun: policy = "dry-run"; break;
        case EffectPolicy::Refuse: policy = "refuse"; break;
        }
    }
    cJSON_AddStringToObject(sec, "policy", policy);
    cJSON_AddBoolToObject(sec, "handler_registered",
                          (app.effects || app.streaming_effects) ? 1 : 0);
    cJSON_AddBoolToObject(sec, "streams", app.streaming_effects ? 1 : 0);

    cJSON* allowed = cJSON_CreateArray();
    if (opts && opts->effects == EffectPolicy::Allow) {
        if (opts->allowed_effects.empty()) {
            cJSON_AddItemToArray(allowed, cJSON_CreateString("*"));
        } else {
            for (const std::string& a : opts->allowed_effects)
                cJSON_AddItemToArray(allowed, cJSON_CreateString(a.c_str()));
        }
    }
    cJSON_AddItemToObject(sec, "allowed", allowed);

    cJSON* ops = cJSON_CreateArray();
    for (const EffectOp& e : app.effect_ops) {
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", e.name.c_str());
        cJSON_AddStringToObject(o, "doc", e.doc.c_str());
        cJSON_AddBoolToObject(o, "reversible", e.reversible ? 1 : 0);
        cJSON_AddStringToObject(o, "consequence", e.consequence.c_str());
        cJSON_AddItemToArray(ops, o);
    }
    cJSON_AddItemToObject(sec, "ops", ops);
    cJSON_AddStringToObject(
        sec, "note",
        "Effects are the one-way door: everything else you do lands in the "
        "document and a person can `revert` it. These cannot be reverted. "
        "Under `refuse` (the default) the verbs fail rather than run.");
    return sec;
}

std::string capabilities(const HostApp& app, Core& core,
                         std::string_view state_path,
                         const SessionOptions* opts) {
    cJSON* root = cJSON_CreateObject();

    cJSON* id = cJSON_CreateObject();
    cJSON_AddStringToObject(id, "app", app.id.c_str());
    cJSON_AddStringToObject(id, "label", app.label.c_str());
    cJSON_AddStringToObject(id, "version", app.version.c_str());
    cJSON_AddStringToObject(id, "voidmaiz", std::string(kVoidmaizVersion).c_str());
    cJSON_AddStringToObject(id, "voidcore", std::string(Core::core_version()).c_str());
    cJSON_AddItemToObject(root, "identity", id);

    /* The state path is in the briefing because an agent must be able to tell a
     * person WHERE it changed things, and because the GUI reading a different
     * file is the failure this design most needs to make visible. */
    cJSON_AddStringToObject(root, "state_path", std::string(state_path).c_str());

    /* Straight from the dispatcher, re-serialized rather than restated: if the
     * briefing and `help`/`glyphs` ever disagree, the briefing is lying. */
    add_parsed(root, "verbs", core.dispatch("help").data);
    add_parsed(root, "glyphs", core.dispatch("glyphs").data);
    add_parsed(root, "mantles", core.dispatch("mantles").data);

    /* The view's interaction vocabulary, surviving the loss of the view. This
     * is `ActionRegistry::manifest()` verbatim — built 2026-07-21 for Hormiga's
     * Territory map, four weeks before headless was asked for. An app whose
     * gestures emit raw commands anonymously has an empty array here, and that
     * emptiness is an accurate report rather than a missing feature. */
    add_parsed(root, "actions", app.actions.manifest());

    cJSON* preds = cJSON_CreateArray();
    for (const std::string& n : app.predicates)
        cJSON_AddItemToArray(preds, cJSON_CreateString(n.c_str()));
    cJSON_AddItemToObject(root, "predicates", preds);

    cJSON_AddItemToObject(root, "effects", effects_section(app, opts));

    /* Documentation by REFERENCE, never embedded. Void Core's OKF holiday is
     * the retrieval tool and `get --head` exists so a page costs ~1.3 KB rather
     * than ~17 KB — the split that lets an agent read design cheaply. */
    cJSON* okf = cJSON_CreateObject();
    cJSON_AddStringToObject(okf, "bundle", app.okf_root.c_str());
    cJSON_AddStringToObject(okf, "index",
                            app.okf_root.empty()
                                ? ""
                                : (fs::path(app.okf_root) / "index.md").string().c_str());
    cJSON_AddStringToObject(okf, "read",
                            "python -m okf --bundle <bundle> get --head <id>");
    cJSON_AddStringToObject(okf, "list", "python -m okf --bundle <bundle> ls");
    cJSON_AddItemToObject(root, "okf", okf);

    /* Said out loud, because an agent reading this is exactly the reader who
     * needs to know that acting and being observed are the same act here. */
    cJSON* rules = cJSON_CreateArray();
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "Every change goes through the dispatcher and is logged with an actor; "
        "there is no unlogged path."));
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "Group a multi-step task with --atomic (one `batch`): it applies "
        "all-or-nothing, so a failure leaves the document untouched rather than "
        "half-edited."));
    /* The quoting rule, stated to an agent because an agent writes script files
     * BY HAND and there is no arg() at the text level. Measured the hard way
     * 2026-08-18: a task file containing `set n text 'Don't forget'` silently
     * truncated at the apostrophe — the same failure shape that bit two hosts
     * in C++ and Python, arriving a third time through the CLI. Since Void Core
     * 0.2.7 (rule 5) it is a hard error naming its line instead of a silent
     * truncation, so the sentence promises the failure as well as the rule: an
     * agent that trusts a diagnostic will look for one. */
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "Quoting: single quotes wrap one argument and the ONLY escape inside "
        "them is \\' — so write 'Don\\'t forget', never 'Don't forget'. An "
        "unterminated quote is an ERROR naming its line, not a silent "
        "truncation, and nothing after it runs."));
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "A quoted value MAY span several lines: a script is split into "
        "statements on newline and `;` only OUTSIDE quotes, so multi-line text "
        "— a description, two paragraphs someone typed — is written literally "
        "inside the quotes and arrives whole."));
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "From a shell, every argv element is passed through as exactly ONE "
        "argument, so shell-level quoting is enough and a value with spaces or "
        "apostrophes needs nothing else. Put `--` before a value that itself "
        "begins with `--`."));
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "Changes land in the state document the GUI opens; a person will see "
        "and be able to edit whatever you make."));
    /* The review affordance, stated correctly. `undo` is SESSION-scoped and is
     * gone when this process exits; the baseline is model content and is not. */
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "Your work stays as unsaved changes against the saved baseline, so a "
        "person can run `status` / `diff` to see exactly what you did and "
        "`revert` to discard all of it. Do not run `save` unless you were asked "
        "to — it snapshots the baseline and erases that diff."));
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "`undo` only reaches frames from THIS session; it cannot undo an "
        "earlier run. Use `diff` and `revert` for anything across processes."));
    cJSON_AddItemToArray(rules, cJSON_CreateString(
        "`help <verb>` documents a verb; `describe <ref>` documents a rune; "
        "`validate` checks the document."));
    cJSON_AddItemToObject(root, "house_rules", rules);

    char* printed = cJSON_Print(root);
    std::string out = printed ? printed : "{}";
    cJSON_free(printed);
    cJSON_Delete(root);
    return out;
}

// ── the terminal front-end ──────────────────────────────────────────────────

namespace {

struct Cli {
    std::string state, actor, script;
    bool json = false, repl = false, describe = false, atomic = false;
    bool no_save = false, no_lock = false;
    bool allow_effects = false, dry_run_effects = false;
    bool human = false;
    std::vector<std::string> allowed; // --allow-effects=a,b ; empty = all
    std::vector<std::string> words;   // the one-shot command
};

/* "a,b,c" -> {"a","b","c"}. Empty input yields an empty list, which under
 * --allow-effects means "all of them" (see SessionOptions::allowed_effects). */
std::vector<std::string> split_csv(std::string_view in) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < in.size()) {
        size_t j = in.find(',', i);
        std::string_view piece =
            in.substr(i, j == std::string_view::npos ? j : j - i);
        if (!piece.empty()) out.emplace_back(piece);
        if (j == std::string_view::npos) break;
        i = j + 1;
    }
    return out;
}

void usage(const HostApp& app, std::ostream& os) {
    const std::string n = app.id.empty() ? "app" : app.id;
    os << (app.label.empty() ? n : app.label)
       << (app.version.empty() ? "" : " " + app.version) << " — headless\n\n"
       << "  " << n << " <command...>        run one command\n"
       << "  " << n << " --script <file>     run a file of commands ('-' = stdin)\n"
       << "  " << n << " --repl              read commands from stdin\n"
       << "  " << n << " --describe          print the capability briefing\n\n"
       << "  --state <path>   state document (default " << n << ".state.json)\n"
       << "  --actor <name>   attribution recorded in the log\n"
       << "  --json           machine-readable envelopes\n"
       << "  --no-save        run everything, write nothing\n"
       << "  --atomic         with --script: one batch, all-or-nothing\n"
       << "  --no-lock        skip the advisory lock\n"
       << "  --               end of options: every later argv element is a\n"
       << "                   command word, even one starting with --\n\n"
       << "Commands are Void Core dispatcher verbs; `" << n
       << " help` lists them.\n\n"
       << "Each argv element is passed through as exactly ONE argument, so\n"
       << "  " << n << " set maria role \"Outreach Coordinator\"\n"
       << "stores the whole phrase, apostrophes and all.\n";
}

/* One result, printed. `--json` emits the envelope so a caller can branch on
 * `ok` without parsing prose; the human form is the lines the CLI would print
 * anyway. Both go to stdout; only startup/usage failures go to stderr. */
void report(const Result& r, bool json) {
    if (json) {
        cJSON* o = cJSON_CreateObject();
        cJSON_AddBoolToObject(o, "ok", r.ok);
        cJSON* lines = cJSON_CreateArray();
        for (const std::string& l : r.lines)
            cJSON_AddItemToArray(lines, cJSON_CreateString(l.c_str()));
        cJSON_AddItemToObject(o, "lines", lines);
        cJSON* data = parse_or_null(r.data);
        cJSON_AddItemToObject(o, "data", data ? data : cJSON_CreateNull());
        char* printed = cJSON_PrintUnformatted(o);
        std::cout << (printed ? printed : "{}") << "\n";
        cJSON_free(printed);
        cJSON_Delete(o);
        return;
    }
    const std::string text = r.text();
    if (!text.empty()) std::cout << text << "\n";
}

/* Read a whole transcript and split it into the statements it will run.
 *
 * THE WHOLE FILE AT ONCE, DELIBERATELY. This used to be a per-line function:
 * std::getline, trim, drop `#` comments, dispatch. That cuts on newlines BEFORE
 * any quote is considered, so a value containing a newline — a bio, a
 * description, the two paragraphs a volunteer actually typed — was split across
 * two "commands" and its second half executed. Against Void Core 0.2.6 that was
 * a command injection; 0.2.7's rule 5 turned it into a hard error instead, which
 * is a good failure and still means a multi-line value CANNOT BE SCRIPTED
 * (Hormiga, 2026-08-25).
 *
 * maiz::split_transcript is the dispatcher's own reader: newline and `;` are
 * boundaries only OUTSIDE a quoted run, `#` comments are dropped, and an
 * unterminated quote is reported with the line it started on rather than
 * guessed past. Agreeing with the dispatcher by construction rather than by
 * coincidence is the entire reason to call it. */
bool read_transcript(const std::string& src, std::vector<std::string>& out) {
    const Transcript t = split_transcript(src);
    if (!t.ok) {
        std::cerr << "error: line " << t.error_line << ": " << t.error << "\n"
                  << "hint: quote a value with single quotes; the only escape "
                     "inside them is \\' — write 'Don\\'t' for a value "
                     "containing an apostrophe (SPEC §6.1).\n";
        return false;
    }
    /* A transcript with a block or a SPEC §8 control word is Voidscript, not a
     * list of commands — the statements are handed over one at a time here, so
     * its control flow would be lost silently. Say so and keep going: the
     * caller may know exactly what they are doing. */
    if (!t.flat)
        std::cerr << "note: this transcript uses blocks or control words; they "
                     "are dispatched as plain statements here. Use Void Core's "
                     "`script` verb for Voidscript.\n";
    for (const Statement& st : t.commands) out.push_back(st.text);
    return true;
}

std::string slurp_stream(std::istream& in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int run_cli(const HostApp& app, int argc, char** argv) {
    Cli cli;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "error: " << what << " needs a value\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--state") cli.state = next("--state");
        else if (a == "--actor") cli.actor = next("--actor");
        else if (a == "--script") cli.script = next("--script");
        else if (a == "--json") cli.json = true;
        else if (a == "--repl") cli.repl = true;
        else if (a == "--describe" || a == "describe-app") cli.describe = true;
        else if (a == "--atomic") cli.atomic = true;
        else if (a == "--no-save") cli.no_save = true;
        else if (a == "--no-lock") cli.no_lock = true;
        else if (a == "--allow-effects") cli.allow_effects = true;
        else if (a.rfind("--allow-effects=", 0) == 0) {
            cli.allow_effects = true;
            cli.allowed = split_csv(std::string_view(a).substr(16));
        }
        else if (a == "--dry-run-effects") cli.dry_run_effects = true;
        else if (a == "--human") cli.human = true;
        else if (a == "--help" || a == "-h") { usage(app, std::cout); return 0; }
        /* END OF OPTIONS. Without it there is no way to pass a VALUE that looks
         * like a flag: `set n notes --json` had its value eaten by the option
         * parser and a mode turned on from nowhere (Hormiga's open question,
         * 2026-08-25 — and yes, it was real). After `--`, every remaining argv
         * element is a command word, `--`-prefixed or not. */
        else if (a == "--") {
            while (++i < argc) cli.words.emplace_back(argv[i]);
        }
        else cli.words.push_back(std::move(a));
    }

    if (cli.words.empty() && cli.script.empty() && !cli.repl && !cli.describe) {
        usage(app, std::cout);
        return 0;
    }

    SessionOptions opts;
    opts.state_path = cli.state;
    if (!cli.actor.empty()) opts.actor = cli.actor;
    /* --describe never writes: asking an application what it can do must not be
     * a mutation, and an agent orienting itself should not need write access. */
    opts.save_on_close = !cli.no_save && !cli.describe;
    opts.lock = !cli.no_lock && !cli.describe;
    /* Refuse stays the default. --dry-run-effects wins over --allow-effects when
     * both are given: the more conservative reading of a contradictory
     * instruction is the only safe one at a one-way door. */
    if (cli.human) opts.kind = SessionKind::Human;
    /* Effect lines go to STDERR so a `--json` caller's stdout stays parseable
     * while a person watching still sees the deploy scroll past. */
    opts.on_effect_line = [](std::string_view op, std::string_view line) {
        std::cerr << "[" << op << "] " << line << "\n";
    };
    if (cli.dry_run_effects) opts.effects = EffectPolicy::DryRun;
    else if (cli.allow_effects) {
        opts.effects = EffectPolicy::Allow;
        opts.allowed_effects = cli.allowed;
    }

    Session session(app, opts);
    if (!session.start()) {
        std::cerr << "error: " << session.error() << "\n";
        return 2;
    }

    if (cli.describe) {
        std::cout << capabilities(app, session.core(), session.state_path()) << "\n";
        return 0;
    }

    int rc = 0;

    if (!cli.script.empty()) {
        std::string src;
        if (cli.script == "-") {
            src = slurp_stream(std::cin);
        } else {
            std::ifstream in(cli.script);
            if (!in) {
                std::cerr << "error: cannot read " << cli.script << "\n";
                return 2;
            }
            src = slurp_stream(in);
        }
        std::vector<std::string> lines;
        if (!read_transcript(src, lines)) return 2;
        if (cli.atomic) {
            Result r = session.batch(lines);
            report(r, cli.json);
            if (!r.ok) rc = 1;
        } else {
            for (const std::string& c : lines) {
                Result r = session.dispatch(c);
                report(r, cli.json);
                /* Stop at the first failure. Continuing would build the state
                 * a person cannot review: neither the old document nor the
                 * intended one, with no marker for where it went wrong.
                 * `--atomic` is the stronger form of the same instinct. */
                if (!r.ok) { rc = 1; break; }
            }
        }
    } else if (cli.repl) {
        /* A REPL reads lines, but a STATEMENT is not a line: a quoted value can
         * span several, and that is the ordinary case for the multi-line fields
         * hosts actually have. So lines accumulate until they form a complete
         * transcript, and only then run. A BLANK LINE abandons a pending
         * statement — otherwise one stray quote swallows the rest of the
         * session with no way out but a kill. */
        std::string pending, raw;
        bool stop = false;
        while (!stop && std::getline(std::cin, raw)) {
            const bool blank = raw.find_first_not_of(" \t\r") == std::string::npos;
            if (!pending.empty()) pending += '\n';
            pending += raw;

            const Transcript t = split_transcript(pending);
            if (!t.ok) {
                if (blank) { // the quote was a typo, not a continuation
                    std::cerr << "error: " << t.error << "\n";
                    pending.clear();
                }
                continue; // still inside a quoted value — keep reading
            }
            pending.clear();
            for (const Statement& st : t.commands) {
                if (!st.argv.empty() &&
                    (st.argv[0] == "exit" || st.argv[0] == "quit")) {
                    stop = true;
                    break;
                }
                Result r = session.dispatch(st.text);
                report(r, cli.json);
                if (!r.ok) rc = 1; // remembered, but a REPL keeps going
            }
        }
        if (!pending.empty())
            std::cerr << "error: input ended inside a quoted value; "
                         "the last statement was not run\n";
    } else {
        /* THE ONE-SHOT PATH, AND THE LINE THAT USED TO CORRUPT EVERY VALUE
         * CONTAINING A SPACE. The OS already split argv for us; this used to
         * throw that away, re-join on spaces and hand the result to a tokenizer
         * that split it again — so `set maria role "Outreach Coordinator"`
         * stored `Outreach`, `Ana's Place` stored `Anas Place`, and an empty
         * argument was never written at all, each with exit 0 (measured by
         * Hormiga against a real build, 2026-08-25). An argv element is ALREADY
         * one argument; command_line() says so, in Void Core's own codec. */
        Result r = session.dispatch(command_line(cli.words));
        report(r, cli.json);
        if (!r.ok) rc = 1;
    }

    session.close();
    if (!session.error().empty()) {
        std::cerr << "error: " << session.error() << "\n";
        return 2;
    }
    return rc;
}

} // namespace maiz
