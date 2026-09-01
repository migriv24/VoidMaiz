/* effect_scene_smoke.cpp — the effect graph, DRAWN (voidmaiz/sentinel.hpp).
 *
 * The analysis moved to Void Allomone on 2026-08-29; the PICTURE stayed, because
 * a Scene is Void Maiz's projection type. These are the three blocks that came
 * back out of `sentinel_smoke.cpp` with the function, unchanged except for
 * where they live — pure data to data, so no ImGui and no Core.
 *
 * The layout is the point: columns ARE strata, so left-to-right is the
 * evaluation order acyclicity buys. */
#include "voidmaiz/sentinel.hpp"

#include <algorithm>
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

int main() {
    using namespace maiz;

    // ── the effect graph as a Scene ─────────────────────────────────────────
    //
    // Pure data → data, so it is testable with no ImGui and no Core. The layout
    // is the point: columns ARE strata, so left-to-right is the evaluation
    // order acyclicity buys.
    {
        std::vector<EffectNode> nodes = {
            {"a", {}, {"prop:x"}},              // stratum 0: reads nothing written
            {"b", {"prop:x"}, {"prop:y"}},      // stratum 1
            {"c", {"prop:y"}, {}},              // stratum 2
        };
        EffectGraph g = build_effect_graph(nodes);
        CHECK(g.acyclic());
        CHECK(g.strata.size() == 3);

        Scene s = effect_scene(g);
        CHECK(s.nodes.size() == 3);
        CHECK(s.wires.size() == 2);

        auto node = [&](const char* n) { return s.find(n); };
        CHECK(node("a") && node("b") && node("c"));
        if (node("a") && node("b") && node("c")) {
            // Strictly increasing x: the picture states the order.
            CHECK(node("a")->x < node("b")->x);
            CHECK(node("b")->x < node("c")->x);
            // Every node is deliberately placed — a computed layout is still a
            // decided one, and `placed=false` would invite an auto-layout to
            // move it and destroy the meaning.
            for (const SceneNode& n : s.nodes) CHECK(n.placed);
            // Nothing is in a cycle, so nothing is marked.
            for (const SceneNode& n : s.nodes) CHECK(n.tags.empty());
        }
        // The wire carries the SHARED SYMBOL, so the explanation rides on the
        // edge rather than living in a message somewhere else.
        bool via_x = false;
        for (const SceneWire& w : s.wires)
            if (w.from == "a" && w.to == "b" && w.relation == "prop:x") via_x = true;
        CHECK(via_x);
        for (const SceneWire& w : s.wires) CHECK(!w.active); // nothing to emphasize
    }
    {
        // A CYCLE. There are no strata by definition, so the fallback puts the
        // members in one column — the picture of "these cannot be ordered" IS
        // the diagnosis, and the members are coloured and tagged.
        std::vector<EffectNode> nodes = {
            {"p", {"prop:q"}, {"prop:p"}},
            {"q", {"prop:p"}, {"prop:q"}},
        };
        EffectGraph g = build_effect_graph(nodes);
        CHECK(!g.acyclic());
        CHECK(g.strata.empty());

        EffectSceneStyle st;
        Scene s = effect_scene(g, st);
        CHECK(s.nodes.size() == 2);
        for (const SceneNode& n : s.nodes) {
            CHECK(n.has_color && n.rgb == st.cycle_rgb);
            CHECK(std::find(n.tags.begin(), n.tags.end(), "cycle") != n.tags.end());
        }
        // Same column, different rows: unorderable, and drawn that way.
        if (s.nodes.size() == 2) {
            CHECK(s.nodes[0].x == s.nodes[1].x);
            CHECK(s.nodes[0].y != s.nodes[1].y);
        }
        // Both endpoints in the cycle ⇒ the wire is the loop, and emphasized.
        for (const SceneWire& w : s.wires) CHECK(w.active);
    }
    {
        // An empty graph is an empty scene, not a crash — the panel renders
        // before any script exists.
        Scene s = effect_scene(build_effect_graph({}));
        CHECK(s.nodes.empty() && s.wires.empty());
    }

    if (failures == 0) {
        std::cout << "OK - effect scene smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
