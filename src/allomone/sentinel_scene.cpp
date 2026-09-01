/* sentinel_scene.cpp — the effect graph, DRAWN.
 *
 * The only piece of the Sentinel that did not move to Void Allomone
 * (2026-08-29), because a `Scene` is Void Maiz's projection type. The analysis
 * is upstream; this turns its answer into a picture, and it is a pure data ->
 * data function: no Core, no mantle, no ImGui, no I/O. */
#include "voidmaiz/sentinel.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace maiz {

Scene effect_scene(const EffectGraph& g, const EffectSceneStyle& style) {
    Scene scene;
    scene.mantle = "effect-graph";

    // Which scripts sit in a cycle — the SCC members, which are the report.
    std::set<std::string> cyclic;
    for (const auto& comp : g.cycles)
        for (const std::string& id : comp) cyclic.insert(id);

    // Column = stratum. This is the layout doing the explaining: left-to-right
    // IS the evaluation order acyclicity buys.
    std::map<std::string, std::pair<int, int>> at; // id -> (col, row)
    for (size_t s = 0; s < g.strata.size(); ++s)
        for (size_t r = 0; r < g.strata[s].size(); ++r)
            at[g.strata[s][r]] = {(int)s, (int)r};

    // A cyclic graph HAS no strata, so anything unplaced goes in one column —
    // and that picture ("these cannot be ordered") is exactly the diagnosis.
    int spill = (int)g.strata.size();
    int spill_row = 0;
    for (const EffectNode& n : g.nodes)
        if (!at.count(n.id)) at[n.id] = {spill, spill_row++};

    for (const EffectNode& n : g.nodes) {
        SceneNode sn;
        sn.id = n.id;
        sn.name = n.id;
        sn.glyph = "allo-script";
        sn.label = n.id;
        const auto [col, row] = at[n.id];
        sn.x = style.origin_x + (float)col * style.col_gap;
        sn.y = style.origin_y + (float)row * style.row_gap;
        sn.placed = true; // a computed layout is still a decided one
        sn.has_color = true;
        sn.rgb = cyclic.count(n.id) ? style.cycle_rgb : style.node_rgb;
        if (cyclic.count(n.id)) sn.tags.push_back("cycle");
        scene.nodes.push_back(std::move(sn));
    }

    for (const EffectEdge& e : g.edges) {
        SceneWire w;
        w.from = e.from;
        w.to = e.to;
        w.kind = SceneWire::Kind::Loose; // a dependency, not a typed port net
        w.relation = e.via;              // the shared symbol IS the explanation
        w.directed = true;
        // Emphasize the edges that close a loop — `active` is the canvas's own
        // "this one matters" channel, and this is what it is for.
        w.active = cyclic.count(e.from) && cyclic.count(e.to);
        scene.wires.push_back(std::move(w));
    }
    return scene;
}

} // namespace maiz
