/*
 * voidmaiz/project.hpp — the projection engine: state document → Scene.
 *
 * The one-sync rule (okf/concepts/log-first-inheritance.md): a view never
 * mutates its own picture — it dispatches, then re-projects. project_scene is
 * that re-projection: cheap, pure, and total (a rejected edit simply never
 * shows up because it never reached the state document).
 *
 * Position convention (until the core's `place` verb lands): a rune is
 * *placed* if `rune.placement` is `{"x":…,"y":…}` or `[x,y]`, else if
 * `content.pos` is one of those shapes. Everything else gets depth
 * auto-layout: columns by longest directed-wire distance from a source,
 * rows by order within the column.
 */
#pragma once

#include "voidmaiz/annotate.hpp"
#include "voidmaiz/scene.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace maiz {

class Core;

struct ProjectOptions {
    std::string mantle;          // which mantle to project; empty = the active one
    float col_gap = 240.0f;      // auto-layout column pitch
    float row_gap = 130.0f;      // auto-layout row pitch
    float origin_x = 60.0f;      // auto-layout top-left
    float origin_y = 60.0f;
};

/* Pure form: project from an exported state document and the `glyphs` verb's
 * data payload (a JSON array of full descriptors, arbitrary keys preserved —
 * including the "hints" convention). Unknown mantle → empty scene. */
Scene project_scene(std::string_view state_json, std::string_view glyphs_json,
                    const ProjectOptions& opts = {});

/* Convenience form: pulls both documents from a live core. */
Scene project_scene(Core& core, const ProjectOptions& opts = {});

/* Extract one mantle's JSON fragment from an exported state document (empty
 * name = the active mantle). Returns "" when absent. This is the seam between
 * the state document and the reduce contract's mantle adapter. */
std::string extract_mantle(std::string_view state_json, std::string_view name = {});

/* ── reading a projected field ───────────────────────────────────────────────
 *
 * `SceneField::value_json` is JSON, because that is what the model stores and a
 * projection may not quietly reinterpret it. But almost every host wants the
 * PLAIN value — the text a user typed, not `"\"Susie\""` — and so every host
 * was writing the same decode. Hormiga wrote it, the library's own
 * `allomone_minimal` wrote it, and the two had already diverged on `null`
 * before anyone compared them (reported 2026-08-17). Shipping it is how the
 * divergence stops. */

/* The named field, or nullptr. For the cases that need `is_string`, `editor` or
 * `label` rather than just the value. */
const SceneField* find_field(const SceneNode& node, std::string_view key);

/* The named field decoded to plain text. Absent field → "".
 *
 * THE DECODE, stated so two hosts cannot answer it differently:
 *   - a JSON string  → its decoded content (escapes and \uXXXX resolved to
 *                      UTF-8), WITHOUT the quotes
 *   - JSON `null`    → "" — the SAME answer as an absent field, deliberately.
 *                      Hormiga's `field "key"` predicate means "this field has
 *                      a value", and the four literal characters `null` would
 *                      otherwise count as one. `null` is the model's way of
 *                      saying nothing is there; the decode agrees with it.
 *                      A host that must tell "absent" from "explicitly null"
 *                      has `find_field` and the raw `value_json`.
 *   - anything else  → the compact JSON verbatim (`2.5`, `true`, `[1,2]`),
 *                      which is what a number or a structure IS as text.
 *
 * Total and non-throwing: unparseable JSON comes back verbatim rather than
 * empty, because a projection should never delete a value it failed to read. */
std::string field_of(const SceneNode& node, std::string_view key);

/* ── reading a derivation over a Scene ───────────────────────────────────────
 *
 * THE CONSTRAINT MAP STAYS PARALLEL TO THE SCENE. Q17 asked whether the
 * derivation should just BE a `Scene` decoration channel; Hormiga — who has the
 * real domain — answered no (2026-08-17), for three reasons we accept in full:
 * their subjects are not all Scene nodes and the ones that are are not always
 * in the same Scene; they derive with NO Scene at all (the export path, the
 * tests, and `allomone_minimal` link no view module); and `Merged` has a
 * different invalidation rule — a Scene changes when the VIEW changes, a
 * derivation when the DATA does, and folding them makes one object with two
 * reasons to be rebuilt.
 *
 * So this is the cheap half they asked for instead: the ergonomics without the
 * coupling. Two free functions over two independent projections. Nothing here
 * is stored, nothing is coupled, and a host that never builds a Scene never
 * calls them.
 *
 * THE ADDRESS IS `node.name`, and this is the convention, written down so two
 * hosts cannot pick differently: a subject is addressed by the rune NAME, which
 * is what the dispatcher takes, what `layout.edges` references and what tag
 * expressions match — not by `node.id`. A rename therefore re-addresses a
 * subject, which is correct: the derivation is about the rune the user can
 * name, and a resolution stored against the old name is the same stale-pointer
 * problem `okf/concepts/allomone/discovery.md` §1 already tracks. */

/* This node's settled value for one property; "" when absent OR conflicted.
 * Exactly `merged.value(node.name, prop)` — named so the convention has one
 * spelling, and safe on a temporary because `Merged::value` returns by value. */
std::string decoration(const Merged& merged, const SceneNode& node,
                       std::string_view property);

/* The full cell, for hosts that need `conflicted`, `settled_by` or the
 * contributor list rather than just a value. Null when the node has no opinion.
 *
 * `merged` is taken by const& and the pointer aims into it, so it lives exactly
 * as long as the Merged does — the lvalue-only discipline `Merged::find`
 * enforces applies here too: name your Merged, do not decorate a temporary. */
const MergedCell* decoration_cell(const Merged& merged, const SceneNode& node,
                                  std::string_view property);

/* Walk the two projections together: `fn(node, value)` for every node in scene
 * order that has a settled, non-empty value for `property`.
 *
 * Scene order rather than cell order on purpose — a renderer draws in scene
 * order, and a walk that hands nodes back in `Merged`'s sorted-by-subject order
 * would make the caller re-look-up every node to draw it. Conflicted cells are
 * skipped, because their value is by construction not trustworthy; a host that
 * wants to DRAW the disagreement wants `merged.conflicts()`, which is the
 * first-class output for exactly that. */
void for_each_decoration(
    const Scene& scene, const Merged& merged, std::string_view property,
    const std::function<void(const SceneNode&, const std::string&)>& fn);

/* ── reading an attribute assertion ──────────────────────────────────────────
 *
 * An edge whose `to` is a MEASURE rune asserts a value rather than a strength
 * (SPEC §3.7.1, Void Core 0.2.14), and `SceneWire::is_value` says which reading
 * applies. The value is the weight; the UNIT is on the measure rune at the far
 * end, so formatting one takes a scene lookup — which is exactly the shape that
 * two hosts diverge on, the way `field_of` diverged on `null` before it shipped.
 * So it ships once, here, before there are two of them.
 *
 * "5 m/s" for a wire with a unit, "5" for a dimensionless one, "" for a wire
 * that is not an assertion at all (so a renderer can call it unconditionally
 * and draw a label only when it comes back non-empty). Trailing zeros are
 * trimmed: the model stores 5 and a reader should see `5`, not `5.000000`. */
std::string value_label(const Scene& scene, const SceneWire& wire);

/* ── the filter convention ───────────────────────────────────────────────────
 * ONE tag grammar filters every surface (widget-registry.md's tag-awareness
 * clause): the bag a node offers to Core::tag_match is its tags + its name
 * (name-as-tag) + "glyph:<g>". The canvas filter box, the table view, and
 * host widgets all match against this same bag. */
std::vector<std::string> filter_bag(const SceneNode& node);

/* Does the node pass the filter expression? Empty expression matches all;
 * a malformed one (mid-keystroke typo) matches all too — filtering should
 * degrade to "show everything", never to a blank screen. */
bool node_matches(std::string_view filter, const SceneNode& node);

} // namespace maiz
