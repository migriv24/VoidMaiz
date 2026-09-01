/* command_smoke.cpp — every command the library EMITS must actually DISPATCH.
 *
 * WHY THIS EXISTS. `annotate_smoke` and `usergraph_smoke` assert on the exact
 * strings compile_resolution() and UserGraph::compile() produce — and both
 * happily pinned `mantle enter <m>`, which Void Core rejects (the verb is
 * `use <m>`; `mantle` only has new/rm/rename). The tests were green, the
 * library was wrong, and the demo silently did nothing for four whole features
 * because the active mantle was never what the code believed.
 *
 * A string-equality test proves the code produced a string. It cannot prove the
 * string means anything. So this file runs every emitted command through a REAL
 * Core and requires ok — the only check that would have caught it.
 *
 * The rule this encodes: if a library function returns dispatcher commands,
 * SOMETHING must dispatch them in a test. */
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/usergraph.hpp"

#include <iostream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

/* A field arrives from the projection as JSON. The demo carries its own copy of
 * this; so does this test. Two copies is a smell worth naming rather than
 * fixing in passing — `field_of(SceneNode, key)` is a host need general enough
 * to belong in the library, and is on the backlog rather than bolted on here. */
static std::string unquote_json(const std::string& j) {
    if (j.size() < 2 || j.front() != '"') return j;
    std::string out;
    for (size_t i = 1; i + 1 < j.size(); ++i) {
        if (j[i] == '\\' && i + 2 < j.size()) {
            char n = j[++i];
            out += (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
        } else {
            out += j[i];
        }
    }
    return out;
}

/* Dispatch every line; report the first that fails, with the core's own
 * message, because "a command failed" without the text is not actionable. */
static bool run_all(maiz::Core& core, const std::vector<std::string>& cmds,
                    const char* what) {
    bool all_ok = true;
    for (const std::string& c : cmds) {
        maiz::Result r = core.dispatch(c);
        if (!r.ok) {
            all_ok = false;
            ++failures;
            std::cerr << "FAIL [" << what << "] command rejected: " << c << "\n"
                      << "        core said: " << r.text() << "\n";
        }
    }
    return all_ok;
}

int main() {
    using namespace maiz;

    // ── UserGraph::compile() ────────────────────────────────────────────────
    {
        Core core;
        CHECK(core.register_glyph(std::string(UserGraph::affordance_glyph())));
        CHECK(core.dispatch("mantle new usergraph").ok);

        UserGraph g;
        g.open_frame();
        g.touch("volume", "widget", std::string(maiz::channel::touch));
        g.touch("mute", "widget", std::string(maiz::channel::touch));
        g.touch("preset", "control", std::string(maiz::channel::pointer));
        g.close_frame();

        std::vector<std::string> cmds = g.compile("usergraph");
        CHECK(!cmds.empty());
        run_all(core, cmds, "UserGraph::compile");

        // And the graph really landed as model content — not merely accepted.
        Result ls = core.dispatch("ls");
        CHECK(ls.ok);
        CHECK(ls.text().find("volume") != std::string::npos);

        // `describe` prints FACETS in its lines and content in `data`, so the
        // machine half is what to assert on — or ask for the field directly.
        Result d = core.dispatch("get volume device");
        CHECK(d.ok);
        CHECK(d.text().find("touch") != std::string::npos);
        Result t = core.dispatch("get volume touches");
        CHECK(t.ok && t.text().find("1") != std::string::npos);
    }

    // ── compile_resolution() ────────────────────────────────────────────────
    {
        Core core;
        // Register ONLY what the library ships. The previous version of this
        // test hand-wrote the descriptor, which meant it proved the commands
        // work *given* the right glyph — not that a host could obtain one.
        // `resolution_glyph()` did not exist until 2026-08-10; a first adopter
        // had to reverse-engineer the name and field list from our source, and
        // getting a field wrong makes `set` silently do nothing.
        CHECK(core.register_glyph(std::string(resolution_glyph())));
        CHECK(core.dispatch("mantle new resolutions").ok);

        run_all(core, compile_resolution("resolutions", {"mute", "color", "danger", ""}),
                "compile_resolution/winner");
        run_all(core, compile_resolution("resolutions", {"", "weight", "", "3"}),
                "compile_resolution/wildcard-value");

        Result ls = core.dispatch("ls");
        CHECK(ls.ok);
        CHECK(ls.text().find("res-color-mute") != std::string::npos);
        CHECK(ls.text().find("res-weight-all") != std::string::npos);

        // Round-trip: what was written is what a host would read back to
        // rebuild the Resolution.
        Result d = core.dispatch("get res-color-mute winner");
        CHECK(d.ok);
        CHECK(d.text().find("danger") != std::string::npos);
        Result pr = core.dispatch("get res-color-mute property");
        CHECK(pr.ok && pr.text().find("color") != std::string::npos);

        /* EVERY field compile_resolution can set must be DECLARED by the glyph,
         * and the check has to go through the PROJECTION to mean anything.
         *
         * Measured 2026-08-10, because the first version of this assertion was
         * wrong: `set` and `get` accept an **undeclared** field perfectly
         * happily, so a `get`-based check stays green against a short
         * descriptor. What does not survive is `project_scene` — a `SceneNode`
         * carries only the fields the glyph declares. A host reads resolutions
         * back through the projection (`field_of(n, "property")` in the demo),
         * so a descriptor that is right in name and short by one field yields a
         * Resolution with a silently empty member.
         *
         * That is the same shape as the `fields`-as-object bug found earlier
         * the same week: registration succeeds, `set` succeeds, and the
         * projection quietly carries nothing. */
        run_all(core, compile_resolution("resolutions", {"volume", "glow", "", "7"}),
                "compile_resolution/subject+value");

        Scene rs = project_scene(core, {"resolutions"});
        const SceneNode* node = nullptr;
        for (const SceneNode& n : rs.nodes)
            if (n.name == "res-glow-volume") node = &n;
        CHECK(node != nullptr);
        if (node) {
            auto projected = [&](const char* key) {
                for (const auto& f : node->fields)
                    if (f.key == key) return unquote_json(f.value_json);
                return std::string("<UNDECLARED>");
            };
            CHECK(projected("property") == "glow");
            CHECK(projected("subject") == "volume");
            CHECK(projected("value") == "7");
            // `winner` is unset on this resolution but must still be DECLARED,
            // or a winner-style resolution would read back empty.
            CHECK(projected("winner") != "<UNDECLARED>");
        }
    }

    // ── the mantle switch itself, pinned so nobody "fixes" it back ──────────
    {
        Core core;
        CHECK(core.register_glyph(R"({"glyph":"thing","label":"T","fields":["v"]})"));
        CHECK(core.dispatch("mantle new a").ok);
        CHECK(core.dispatch("rune new thing t1").ok);
        CHECK(core.dispatch("mantle new b").ok); // `mantle new` also ENTERS

        // `mantle enter` is NOT a verb — this is the exact mistake that broke
        // the demo, kept as a live assertion rather than a comment.
        CHECK(!core.dispatch("mantle enter a").ok);
        CHECK(!core.dispatch("tag t1 +x").ok); // still in `b`, so t1 is unreachable

        CHECK(core.dispatch("use a").ok);
        CHECK(core.dispatch("tag t1 +x").ok);
    }

    // ── a script's SOURCE must survive the round trip through a command ─────
    //
    // `define` introduced three characters the language had never emitted
    // through the dispatcher — `(`, `)` and `=`. Core's argument tokenizer has
    // already surprised us once (double-quoted text is taken literally, which
    // is why the demo quotes with apostrophes), so "the parser accepts it" is
    // not evidence that "the author can save it". Store it, read it back, and
    // parse what came back.
    {
        Core core;
        // The demo's exact descriptor shape. `fields` is an ARRAY of names —
        // an object here registers without error and then projects nothing,
        // which is the same silent-success failure `affordance_glyph` had.
        CHECK(core.register_glyph(
            R"({"glyph":"allo-script","label":"Allomone script",)"
            R"("fields":["source","enabled"],)"
            R"("hints":{"editors":{"source":"multiline","enabled":"bool"}}})"));
        CHECK(core.dispatch("mantle new scripts").ok);
        CHECK(core.dispatch("rune new allo-script s1").ok);

        const std::string source =
            "define risky(t) = has t and has \"danger\"\n"
            "when risky(\"muted\") then weight 2\n";

        // The demo's quoting discipline, copied exactly: SINGLE quotes, and
        // REAL newlines passed straight through. Inside single quotes Core
        // escapes only `\'` and takes everything else literally — including the
        // double quotes a script is full of. Escaping the newlines here (the
        // first thing I tried) stores two characters and the script comes back
        // as one line, which is the same shape as the original `\n` bug that
        // this quoting discipline exists to avoid.
        std::string arg = "'";
        for (char ch : source) {
            if (ch == '\'') arg += "\\'";
            else arg += ch;
        }
        arg += "'";
        Result set = core.dispatch("set s1 source " + arg);
        CHECK(set.ok);
        if (!set.ok) std::cerr << "        core said: " << set.text() << "\n";

        // Read it back the way the DEMO does — through the projection — rather
        // than through `get`, which returns a JSON-quoted display form. Reading
        // it the wrong way was this test's first failure, and it is worth the
        // sentence: a round-trip test that reads through a different door than
        // the application proves nothing about the application.
        Scene scene = project_scene(core);
        std::string back_text;
        bool found = false;
        for (const SceneNode& n : scene.nodes) {
            if (n.name != "s1") continue;
            found = true;
            for (const auto& f : n.fields)
                if (f.key == "source") back_text = unquote_json(f.value_json);
        }
        CHECK(found);

        // Whatever came back must still be the script we wrote — checked by
        // PARSING it, not by comparing strings, because the question is whether
        // it still MEANS the same thing.
        Script back = allo_parse("s1", back_text);
        CHECK(back.ok());
        CHECK(back.definitions.size() == 1);
        CHECK(back.rules.size() == 1);
        if (back.rules.size() == 1) CHECK(back.rules[0].strength() == 2);
        if (!back.ok() || back.rules.size() != 1 || back.definitions.size() != 1) {
            std::cerr << "        stored:    [" << source << "]\n"
                      << "        read back: [" << back_text << "]\n";
            for (const Diagnostic& d : back.diagnostics)
                std::cerr << "        round-trip diagnostic: " << d.message << "\n";
        }
    }

    // ── the SPEC §6.1 codec, on THIS side of the ABI ───────────────────────
    //
    // Void Core pins `vc_argv_split(vc_arg_quote(v)) == [v]` with its own
    // property test. This re-checks it through maiz::arg / maiz::split_argv,
    // because what can break HERE is not the rule — Maiz no longer implements
    // it — but the MARSHALLING: a string_view that was not NUL-terminated
    // before crossing, a JSON envelope decoded wrong, a library string freed
    // with the host allocator. The corpus is values that actually broke a host.
    {
        const std::vector<std::string> corpus = {
            "",                                 // Hormiga: field was never written
            "Outreach Coordinator",             // Hormiga: stored as "Outreach"
            "Ana's Place",                      // Hormiga: stored as "Anas Place"
            "x --json",                         // Hormiga: value eaten by the flag parser
            "Don't forget",                     // the 2026-08-17 apostrophe bug
            "\\'",                              // Reyna: the no-op replace's residue
            "C:\\",                              // OURS: a trailing backslash
            "C:\\path\\to\\",                    // ...and a run of them
            "two\nlines",                       // the value that could not be scripted
            "quote\" and 'quote'",
            "a\tb\r\nc",
            "{\"json\":[1,2,{\"k\":\"v'w\"}]}",       // a batch payload
            "  leading and trailing  ",
            "#not-a-comment ; not-a-separator",
            "{braces} and $dollar (parens)",
        };
        for (const std::string& v : corpus) {
            Argv a = split_argv(arg(v));
            if (!a.ok || a.argv.size() != 1 || a.argv[0] != v) {
                ++failures;
                std::cerr << "FAIL [codec] arg/split did not round-trip: ["
                          << v << "] -> [" << arg(v) << "] -> "
                          << a.argv.size() << " arg(s)"
                          << (a.argv.empty() ? std::string() : " [" + a.argv[0] + "]")
                          << (a.ok ? "" : " error: " + a.error) << "\n";
            }
        }

        // command_line() is run_cli's join: an OS argv in, one dispatchable
        // line out, every element intact. THIS is the defect Hormiga measured.
        const std::vector<std::string> words = {"set", "maria", "role",
                                                "Outreach Coordinator"};
        Argv rejoined = split_argv(command_line(words));
        CHECK(rejoined.ok);
        CHECK(rejoined.argv == words);

        // ...and it must survive a REAL dispatch, not merely a round-trip.
        {
            Core core;
            CHECK(core.dispatch("mantle new codec").ok);
            CHECK(core.dispatch("rune new text maria").ok);
            CHECK(core.dispatch(command_line(words)).ok);
            Result got = core.dispatch("get maria role");
            CHECK(got.ok);
            CHECK(got.text().find("Outreach Coordinator") != std::string::npos);

            // An empty argument IS an argument. It used to vanish in the join,
            // so the field was never written — silently, with exit 0.
            CHECK(core.dispatch(command_line({"set", "maria", "role", ""})).ok);
            Result cleared = core.dispatch("get maria role");
            CHECK(cleared.ok);
            CHECK(cleared.text().find("Outreach") == std::string::npos);
        }

        // An unterminated quote is an ERROR, not a value running to end of line
        // (SPEC §6.1 rule 5, tightened in Void Core 0.2.7).
        Argv bad = split_argv("set n text 'oops");
        CHECK(!bad.ok);
        CHECK(!bad.error.empty());
    }

    // ── the transcript reader (what a --script run will actually do) ────────
    //
    // The case that matters is the one that used to be a command injection: a
    // newline INSIDE a quoted value is data and must not become a statement.
    {
        Transcript t = split_transcript(
            "# a comment\n"
            "set visitor bio 'I volunteer on weekends.\n"
            "set treasurer email attacker@evil.example'\n"
            "rune new note after ; set after text 'two'\n");
        CHECK(t.ok);
        CHECK(t.flat);
        CHECK(t.commands.size() == 3);
        if (t.commands.size() == 3) {
            CHECK(t.commands[0].argv.size() == 4 &&
                  t.commands[0].argv[3] ==
                      "I volunteer on weekends.\n"
                      "set treasurer email attacker@evil.example");
            CHECK(t.commands[1].argv.size() == 4 &&
                  t.commands[1].argv[0] == "rune"); // `;` split it, not a newline
            CHECK(t.commands[2].argv.size() == 4 &&
                  t.commands[2].argv[3] == "two");
            // Both halves of `a ; b` are on ONE physical line, whatever that
            // line's number is.
            CHECK(t.commands[1].line == t.commands[2].line);
            // 4, and it was 3 until Void Core 0.2.9: their splitter advanced
            // its line counter only at a statement boundary, so newlines
            // swallowed inside a quoted value were never counted and every
            // later number drifted low by exactly that many. We reported it and
            // pinned the DRIFTED value on purpose, so the fix would turn this
            // red rather than pass silently. It did, on 2026-08-29. Trued up.
            CHECK(t.commands[1].line == 4);
        }

        // A quote that never closes names the line it opened on, rather than
        // offering a plausible partial parse.
        Transcript bad = split_transcript("rune new note a\nset a text 'oops\n");
        CHECK(!bad.ok);
        CHECK(bad.error_line == 2);

        // Control flow is REPORTED, so a front-end that dispatches statements
        // one at a time can say so instead of silently dropping the blocks.
        Transcript block = split_transcript("if x {\n  set a b c\n}\n");
        CHECK(block.ok);
        CHECK(!block.flat);
    }

    if (failures == 0) {
        std::cout << "OK — command smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
