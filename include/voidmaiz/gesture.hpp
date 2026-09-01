/*
 * voidmaiz/gesture.hpp — the gesture→command compilers (UI-free half).
 *
 * Every gesture ends as a dispatcher command line; these builders are where
 * that compilation happens, kept free of rendering types so they're testable
 * against a live core without a window. Multi-target gestures compile to ONE
 * `batch` command — one log line, one undo frame (SPEC §6), which is the
 * lasagna model's "a group drag is one action".
 *
 * Quoting: batch payloads ride in single quotes (the core's tokenizer passes
 * everything literal inside them; `\'` is the only escape).
 */
#pragma once

#include "voidmaiz/editor.hpp"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace maiz {

using MoveList = std::vector<std::pair<std::string, std::pair<float, float>>>;

/* Wrap several command lines as ONE atomic `batch '…'` (single undo frame,
 * rolls back if any sub-command fails). The compilers below use it for
 * multi-target gestures; hosts use it for their own compound actions (e.g.
 * a rewrite step). */
std::string compile_batch(const std::vector<std::string>& commands);

/* The multi-field commit helper (Void Hormiga §1.3): collapse a set of
 * compiled commands into the ONE thing to dispatch — "" for none, the lone
 * command for one, a `batch` for several — so a wizard's finish or a form's
 * accumulated field edits land as a single undo frame. This is the
 * single/many/none discipline compile_moves and compile_deletes already use,
 * exposed for hosts building compound flows (e.g. a `+ New` that mints a rune
 * and tags it: compile_commit({"rune new person p1", "tag p1 +draft"})). */
std::string compile_commit(const std::vector<std::string>& commands);

/* One node move: `setjson <name> pos [x,y]` (positions-as-content, the interim
 * convention until the core's `place` verb lands — see okf Q#3). */
std::string compile_move(std::string_view name, float x, float y);

/* A whole selection's move: single command, or one `batch '…'` for several. */
std::string compile_moves(const MoveList& moves);

/* Delete the selection: `rm <name>` or one batch of them. */
std::string compile_deletes(const std::vector<std::string>& names);

/* Camera → the undo-exempt config tier: `config set <key> "x y zoom"`.
 * Logged and persisted with the session, never popped by undo. The key
 * defaults to the node canvas's `view.camera`; a SECOND view (a table's
 * scroll, a map's geographic viewport) persists ITS camera under its own
 * `view.*` key by passing one — the same substrate-shaped, config-tier
 * pattern (Q#3's charge to keep `view.*` substrate-shaped, not pixel-shaped).
 * The Camera's three floats are a viewport, not pixels: the node canvas reads
 * x,y as the top-left WORLD coordinate; a geographic view reads them as the
 * center lon,lat and zoom as its map zoom — the projection is the view's, the
 * persistence pattern is shared. Precision is preserved (%.7g) so lat/lon
 * survive the round-trip; integer world coords still print as integers. */
std::string compile_camera(const Camera& cam, std::string_view config_key = "view.camera");

/* Parse the stored "x y zoom" back (tolerates surrounding JSON quotes). View-
 * agnostic already — it reads only the value, so any view's camera key works. */
bool parse_camera(std::string_view value, Camera& out);

// ── wires (piece 3) ──────────────────────────────────────────────────────────

/* One end of a would-be wire. port 0 = the principal. */
struct PortRef {
    std::string node;
    int port = 0;
    bool is_output = false; // meaningless for the principal
    std::string type;       // empty = untyped (wildcard)
};

enum class WireVerdict {
    Fettuccine, // principal ↔ principal
    Linguine,   // aux output → aux input, types compatible
    Reject,
};

/* The type checker (okf/concepts/interaction-connections.md):
 *  - same node → Reject (no self-wires in the drag grammar, v1)
 *  - principal ↔ principal → Fettuccine (1:1 is enforced by the rewire batch)
 *  - principal ↔ aux → Linguine: a PASSIVE wire (no active pair) — how real
 *    IC programs wire an agent's aux ports into other agents' principals.
 *    The principal is untyped and directionless; direction of the compiled
 *    link follows the aux end (aux input ⇒ principal is `from`, 0:j; aux
 *    output ⇒ the aux is `from`, i:0).
 *  - aux ↔ aux → Linguine iff one end is an output and one an input and the
 *    types match (an empty type is a wildcard); Reject otherwise. */
WireVerdict check_wire(const PortRef& a, const PortRef& b);

/* `link <from> <to> --relation i:j` — the reduce contract's adapter form.
 * Fettuccine (0:0) additionally gets `--undirected` (interaction wires are
 * symmetric). For Linguine, pass the OUTPUT side as `from`. */
std::string compile_link(const PortRef& from, const PortRef& to);

/* `unlink <from> <to> --relation <r>` for an existing scene wire. */
std::string compile_unlink(const SceneWire& wire);

/* A drop that must displace existing wires (an occupied input, a principal's
 * standing fettuccine): ONE command — plain link, or a batch of unlinks + the
 * link (one undo frame: a rewire is one action). */
std::string compile_rewire(const std::vector<SceneWire>& unlinks,
                           const PortRef& from, const PortRef& to);

// ── field editing (piece 5) ──────────────────────────────────────────────────

/* `set <rune> <field> '<text>'` — the value rides single-quoted (the
 * tokenizer's literal mode; embedded ' escapes as \'), so any text is safe. */
std::string compile_set(std::string_view node, std::string_view field, std::string_view text);

/* `setjson <rune> <field> '<json>'` for non-string values. */
std::string compile_setjson(std::string_view node, std::string_view field,
                            std::string_view json);

/* `tag <node> +<tag>` / `tag <node> -<tag>` — the inspector's chip editor.
 * Tags are model state (the save/quit/reload test passes), so adds and
 * removes are ordinary logged, undoable commands. */
std::string compile_tag(std::string_view node, std::string_view tag, bool add);

// ── node chrome & subgraphs (piece 6) ────────────────────────────────────────

/* Collapse state rides content, like pos: `setjson <rune> collapsed true`. */
std::string compile_collapse(std::string_view node, bool collapsed);

/* Face size: `setjson <rune> size [w,h]` (whole units, once per gesture). */
std::string compile_resize(std::string_view node, float w, float h);

/* Switch the active mantle (subgraph enter/exit): `use <mantle>`. */
std::string compile_use(std::string_view mantle);

// ── add box (piece 4) ────────────────────────────────────────────────────────

/* `glyph-N`, unique against the scene's node names. */
std::string unique_name(const Scene& scene, std::string_view glyph);

/* Mint a rune at a canvas position: batch of `rune new` + `setjson pos`. */
std::string compile_add(std::string_view glyph, std::string_view name, float x, float y);

// ── clean view ───────────────────────────────────────────────────────────────

/* The clean gesture [fb#1]: resolve node overlaps by iteratively nudging
 * bounding boxes apart (margin px of breathing room; nodes without a declared
 * size assume default_w/h). Returns "" when nothing overlaps; otherwise the
 * moves as ONE command (a batch for several) — a tidy is one undo frame.
 * Deliberately writes `pos` for every node it moves, auto-laid ones included
 * (a clean is an explicit placement act). */
std::string compile_clean(const Scene& scene, float default_w = 170.0f,
                          float default_h = 110.0f, float margin = 16.0f);

// ── physics relax (force-directed layout) ────────────────────────────────────

/* The force model behind the relax gesture and the live physics toggle:
 * short-range body repulsion (nothing overlaps, distant nodes ignore each
 * other), wires as springs toward a rest length (connected things stay
 * near), position-based integration (no velocities, so it cannot explode).
 * Like every layout act, its OUTPUT is positions — model content — and the
 * discipline is the drag gesture's: iterate view-side as staged ephemera,
 * flush as ONE batch when the system rests. */
struct RelaxParams {
    float repulse_range = 260.0f; // center distance beyond which bodies ignore each other
    float repulse = 26.0f;        // peak separation push, px per iteration at contact
    float spring_len = 180.0f;    // wire rest length, center to center
    float spring_k = 0.02f;       // wire pull per px of stretch, per iteration
    float max_step = 14.0f;       // per-iteration displacement clamp (stability)
    float default_w = 96.0f;      // body extents for nodes without a declared size
    float default_h = 96.0f;
};

/* name → top-left world position (the same shape drags stage). */
using PositionMap = std::map<std::string, std::pair<float, float>>;

/* One relaxation iteration over `pos` (nodes absent from the map are seeded
 * from the scene). Returns the largest displacement in px — at rest when it
 * gets small. Deterministic; UI-free; the live physics toggle calls this
 * once per frame. */
float relax_step(const Scene& scene, PositionMap& pos, const RelaxParams& params = {});

/* The relax gesture: iterate to rest (or max_iterations), then compile every
 * node that moved ≥1px into ONE command — like clean, a relax is one undo
 * frame and an explicit placement act. Returns "" when already at rest. */
std::string compile_relax(const Scene& scene, int max_iterations = 240,
                          const RelaxParams& params = {});

// ── blocks: the snap-to-connect gesture (okf/concepts/node-blocks.md) ────────

/* Block-shape geometry conventions, in WORLD units. The connectors are
 * identified by port NAME — the aux input named "prev" is the top notch, the
 * aux output named "next" is the bottom tab (the hidden linguine chain);
 * other aux inputs are value sockets down the right edge, another aux output
 * is a reporter plug on the left. The view scales the same math by the
 * camera, so gesture and drawing can never disagree. */
struct BlockMetrics {
    float default_w = 150.0f;  // body extents when the node declares none
    float default_h = 40.0f;
    float notch_x = 26.0f;     // prev/next connector inset from the left edge
    float notch_w = 16.0f;     // silhouette notch/tab width…
    float notch_h = 7.0f;      // …and depth (drawing only)
    float snap_radius = 18.0f; // connectors closer than this snap on release
    float tear_factor = 2.5f;  // × snap_radius: flush wires torn past this unlink
};

/* World anchor of a Block node's aux connector, given its top-left and
 * extents (pass scale=zoom with a screen rect to get screen anchors).
 * Returns false for non-Block nodes and for port 0 (the principal is
 * RESERVED for Phase-B execution — not a snap connector). */
bool block_anchor(const SceneNode& n, int port, bool is_output, float x, float y,
                  float w, float h, float scale, const BlockMetrics& m, float& ax,
                  float& ay);

/* The snap candidate a Move drag would connect on release: the nearest
 * compatible free-connector pair (check_wire verdicts) between a dragged
 * Block node and a resting one, within snap_radius. `staged` holds the
 * dragged nodes' in-flight positions (EditorState::staged). */
struct SnapCandidate {
    bool valid = false;
    PortRef from, to;    // the link to compile (from = the source end)
    std::string dragged; // which dragged node snaps
    float dx = 0, dy = 0; // translation that aligns the dragged SET flush
    float ax = 0, ay = 0; // world anchor of the resting connector (preview)
};
SnapCandidate find_snap(const Scene& scene, const StagedMap& staged,
                        const BlockMetrics& m = {});

/* Everything that rides along when a block is grabbed: the block itself, the
 * statement chain hanging below it, and reporters plugged into any member —
 * Scratch's grab-the-stack. The canvas stages all of it on a block drag. */
std::vector<std::string> block_stack_below(const Scene& scene, const std::string& head);

/* Compile a block Move drag's release as ONE command (one undo frame):
 * the staged moves (aligned when a snap lands), the snap's link, occupancy
 * unlinks (both block connector ends are single-occupancy), tear-offs
 * (adjacency wires pulled past tear range unlink — dragging a block out of
 * a stack detaches it), and the heal link when a middle block leaves a
 * stack (its neighbors join). Returns "" when nothing changed. */
std::string compile_block_release(const Scene& scene, const StagedMap& staged,
                                  const BlockMetrics& m = {});

/* Stack re-flow: ONE batch of moves that brings every adjacency-linked block
 * flush against its predecessor (chains walked from their heads). "" when
 * every stack already sits flush. The tidy story for blocks — an explicit
 * placement act, like clean. */
std::string compile_stack_layout(const Scene& scene, const BlockMetrics& m = {});

} // namespace maiz
