/* allomone_minimal.cpp — the whole Allomone loop, headless, in one file.
 *
 * WHAT THIS IS FOR. `okf/concepts/allomone/host-protocol.md` tells an
 * application what it must supply, and then points at
 * `allomone_playground.cpp` — which is 1,500 lines of showcase. A first adopter
 * does not need a showcase; they need the shortest thing that works, with every
 * required step visible and nothing else in the way. This is that.
 *
 * IT IS ALSO A TEST (`allomone_minimal` in ctest). An integration example that
 * silently rots is worse than none, because it is the first thing a new client
 * copies. It returns non-zero if any step fails.
 *
 * NO IMGUI. It links `voidmaiz_allomone` and `voidmaiz` only — no window, no
 * view module. That is a real claim worth pinning: a CLI, a build step or an
 * agent can use Allomone with no GUI at all, which is the "use the merge, not
 * the language" and "use the language, hide the editor" rungs of the opt-out
 * ladder being genuinely available rather than merely described.
 *
 * THE SIX STEPS, in the order host-protocol.md lists them:
 *   1. glyphs — including the ones the LIBRARY's own commands require
 *   2. subjects — your domain objects, projected
 *   3. properties + merge laws — the part only you can declare
 *   4. predicates — your domain's condition vocabulary (optional)
 *   5. derive — parse, evaluate, merge
 *   6. render, and settle conflicts through the dispatcher
 */
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <iostream>
#include <string>
#include <vector>

static int failures = 0;
#define REQUIRE(cond)                                                            \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " #cond "\n";                 \
        }                                                                        \
    } while (0)

/* `field_of` and `arg` USED TO LIVE HERE, four lines each. They are now
 * `maiz::field_of` (voidmaiz/project.hpp) and `maiz::arg` (voidmaiz/embed.hpp),
 * shipped 2026-08-18 because every host was writing them and two codebases had
 * already got `arg` silently wrong. If you copied this file before that date,
 * delete your copies — the library's `field_of` also resolves \uXXXX escapes,
 * which the four-line version quietly mangled, and its `null` rule is written
 * down rather than accidental.
 */

int main() {
    using namespace maiz;
    Core core;

    // ── 1. GLYPHS ───────────────────────────────────────────────────────────
    // Your own, plus every glyph the LIBRARY's emitted commands name. Miss one
    // of those and `rune new` fails with "unknown glyph"; declare it with the
    // wrong field list and the projection silently carries nothing.
    REQUIRE(core.register_glyph(
        R"({"glyph":"widget","label":"Widget","fields":["label","value"]})"));
    REQUIRE(core.register_glyph(
        R"({"glyph":"allo-script","label":"Script","fields":["source","enabled"]})"));
    REQUIRE(core.register_glyph(std::string(resolution_glyph()))); // <- the library's

    // ── 2. SUBJECTS ─────────────────────────────────────────────────────────
    // Model content first, then project it. Subjects are built FROM the
    // projection, so what a rule can see is exactly what the model holds.
    REQUIRE(core.dispatch("mantle new app").ok);
    REQUIRE(core.dispatch("rune new widget volume").ok);
    REQUIRE(core.dispatch("tag volume +audio +danger").ok);
    REQUIRE(core.dispatch("rune new widget mute").ok);
    REQUIRE(core.dispatch("tag mute +audio").ok);
    REQUIRE(core.dispatch("rune new widget preset").ok);
    REQUIRE(core.dispatch("tag preset +library").ok);

    Scene app = project_scene(core, {"app"});
    std::vector<Subject> subjects;
    for (const SceneNode& n : app.nodes)
        subjects.push_back({.id = n.name, .kind = n.glyph, .name = n.name,
                            .mantle = "app", .tags = n.tags});
    REQUIRE(subjects.size() == 3);

    // ── 3. PROPERTIES AND THEIR MERGE LAWS ──────────────────────────────────
    // The one thing discovery cannot do for you. Undeclared = Unique, which
    // surfaces disagreement rather than inventing a combination rule.
    MergeOptions opts;
    opts.lattices = {{"weight", Lattice::Sum},   // contributions add
                     {"glow", Lattice::Max}};    // loudest wins
                                                 // `color` stays Unique

    // ── 4. PREDICATES (optional) ────────────────────────────────────────────
    // Your domain's condition words. MUST be pure: no clock, no I/O, no mutable
    // state — an impure predicate makes the merge depend on WHEN it was asked.
    // Closing over this frame's data is fine; that is an argument in all but
    // name.
    PredicateRegistry preds;
    preds.add("named-longer-than",
              [](const Subject& s, std::string_view n, const UserGraph*) {
                  return s.name.size() > (size_t)std::stoi(std::string(n));
              });

    // ── 5. DERIVE ───────────────────────────────────────────────────────────
    // Scripts are model content too, so they are read back from runes exactly
    // like the subjects were.
    const char* theme = "when all               then color \"#888888\"\n"
                        "when has \"audio\"       then weight 1\n"
                        "when has \"danger\"      then color \"#ff0000\", weight 2\n"
                        "define risky(t) = has t and has \"danger\"\n"
                        "when risky(\"audio\")    then glow 2\n";
    const char* rival = "when has \"danger\"      then color \"#0000ff\"\n"
                        "when named-longer-than \"4\" then glow 1\n";

    REQUIRE(core.dispatch("rune new allo-script theme").ok);
    REQUIRE(core.dispatch("set theme source " + arg(theme)).ok);
    REQUIRE(core.dispatch("rune new allo-script rival").ok);
    REQUIRE(core.dispatch("set rival source " + arg(rival)).ok);

    Scene scripts = project_scene(core, {"app"});
    std::vector<ConstraintMap> sources;
    for (const SceneNode& n : scripts.nodes) {
        if (n.glyph != "allo-script") continue;
        Script s = allo_parse(n.name, field_of(n, "source"));
        for (const Diagnostic& d : s.diagnostics)      // never throws; report
            std::cerr << n.name << ":" << (d.line + 1) << " " << d.message << "\n";
        for (const Diagnostic& d : allo_check(s, &preds)) // unknown predicates
            std::cerr << n.name << ":" << (d.line + 1) << " " << d.message << "\n";
        sources.push_back(allo_eval(s, subjects, &preds));
    }
    REQUIRE(sources.size() == 2);

    Merged merged = merge(sources, opts);

    // ── 6. RENDER, AND SETTLE ───────────────────────────────────────────────
    std::cout << "derived:\n";
    for (const Subject& s : subjects) {
        // value() returns "" for a CONFLICTED cell, on purpose — a renderer
        // cannot accidentally display an arbitrary winner. Show your own
        // default, or show the conflict; never a guess.
        std::cout << "  " << s.id
                  << "  color=" << (merged.value(s.id, "color").empty()
                                        ? "<none/conflict>"
                                        : merged.value(s.id, "color"))
                  << "  weight=" << merged.value(s.id, "weight")
                  << "  glow=" << merged.value(s.id, "glow") << "\n";
    }

    // `theme` and `rival` disagree about volume's colour at equal strength, so
    // it is ⊤ rather than a coin flip.
    std::vector<const MergedCell*> conflicts = merged.conflicts();
    REQUIRE(conflicts.size() == 1);
    if (conflicts.size() == 1) {
        const MergedCell& c = *conflicts[0];
        REQUIRE(c.subject == "volume" && c.property == "color");
        std::cout << "conflict: " << c.subject << "." << c.property << "\n";
        // explain_cell classifies every contributor — the library does this so
        // a host does not re-derive the merge's reasoning and get it subtly
        // wrong (combining laws have no winner; agreement is not an override).
        for (const CellVerdict& v : explain_cell(c))
            std::cout << "    " << v.source << " (" << v.origin << ") = "
                      << v.value << "  " << v.verdict << "\n";
    }

    // Settling is a COMMAND, so it is logged, attributed, replayable and
    // undoable — then you re-derive. Never mutate a cached result.
    REQUIRE(core.dispatch("mantle new resolutions").ok);
    for (const std::string& cmd :
         compile_resolution("resolutions", {"volume", "color", "theme", ""}))
        REQUIRE(core.dispatch(cmd).ok);
    REQUIRE(core.dispatch("use app").ok);

    // Read the resolutions back the same way as everything else, and re-merge.
    Scene res = project_scene(core, {"resolutions"});
    for (const SceneNode& n : res.nodes)
        opts.resolutions.push_back({field_of(n, "subject"), field_of(n, "property"),
                                    field_of(n, "winner"), field_of(n, "value")});
    REQUIRE(opts.resolutions.size() == 1);

    Merged settled = merge(sources, opts);
    REQUIRE(settled.conflicts().empty());
    REQUIRE(settled.value("volume", "color") == "#ff0000");
    std::cout << "settled: volume.color=" << settled.value("volume", "color") << "\n";

    // A few properties worth seeing hold, since they are what the design is for.
    REQUIRE(merged.value("volume", "weight") == "3");   // Sum: 1 + 2
    REQUIRE(merged.value("volume", "glow") == "2");     // Max: define gave 2
    REQUIRE(merged.value("preset", "color") == "#888888"); // the broad default

    // Order-independence, the property everything rests on.
    std::vector<ConstraintMap> flipped(sources.rbegin(), sources.rend());
    REQUIRE(merge(flipped, opts).cells.size() == merge(sources, opts).cells.size());

    if (failures == 0) {
        std::cout << "OK - allomone minimal integration passed\n";
        return 0;
    }
    std::cerr << failures << " step(s) failed\n";
    return 1;
}
