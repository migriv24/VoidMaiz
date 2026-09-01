/*
 * voidmaiz/sentinel.hpp — the Sentinel, as Void Maiz spells it, PLUS the one
 * piece that did not travel.
 *
 * The analysis — the effect graph, Tarjan SCC, Kahn strata, the interaction
 * graph — lives in Void Allomone (`../VoidAllomone`, 2026-08-29). The PICTURE
 * of it stays here, because a `Scene` is Void Maiz's projection type and a
 * language must not learn what a canvas is.
 *
 * That split cost exactly one function, which is the measure of how well the
 * boundary was already drawn: the analyzer answers "can these rules feed each
 * other in a loop?", and drawing the answer is a different question owned by a
 * different library.
 */
#pragma once

#include "allomone/sentinel.hpp"
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/scene.hpp"

namespace maiz {

using allomone::EffectNode;
using allomone::EffectEdge;
using allomone::EffectGraph;
using allomone::RuleRef;
using allomone::Interaction;

using allomone::effect_node;
using allomone::build_effect_graph;
using allomone::analyze;
using allomone::effect_verdict;
using allomone::interactions;
using allomone::interaction_line;

/* THE LAYOUT CARRIES THE MEANING, which is the whole argument for drawing this
 * rather than listing it. Columns are STRATA: stratum 0 reads nothing any
 * script writes, stratum 1 reads only stratum 0, so left-to-right IS the
 * evaluation order acyclicity buys. A cyclic graph has no strata by definition,
 * and the fallback layout says so by putting every cycle member in one column —
 * the picture of "these cannot be ordered" is the diagnosis.
 *
 * The tool is `draw_canvas`, NOT `edit_canvas`: this graph is DERIVED, so there
 * is nothing here a person could meaningfully drag, link or delete. Handing it
 * to an editor would offer gestures whose commands could not be honoured, which
 * is worse than offering none. */
struct EffectSceneStyle {
    float col_gap = 260.0f;
    float row_gap = 130.0f;
    float origin_x = 60.0f;
    float origin_y = 60.0f;
    unsigned cycle_rgb = 0xf85149; // members of an SCC — the report, in colour
    unsigned node_rgb = 0x3b4351;
};

/* Lay the graph out as a Scene: one node per script, one wire per dependency,
 * columns by stratum, cycle members coloured. Pure; no Core, no ImGui, no I/O.
 *
 * The wire's `relation` is the shared SYMBOL that created the edge, so the
 * canvas shows *"D writes prop:color, which B reads"* on the wire itself rather
 * than in a message somewhere else. */
Scene effect_scene(const EffectGraph& g, const EffectSceneStyle& style = {});

} // namespace maiz
