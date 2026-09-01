---
type: Concept
title: Node Blocks
description: "The Scratch-like demo target — snapping blocks together IS the link gesture, hidden-wire adjacency rendering, connector-shape-as-type. Its charter, the Blockly connection-model mapping, the block-as-interaction-net model, and the two new library capabilities it forces (snap-to-connect gesture, adjacency render mode). Structural-first, net-native, turtle domain."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-15T00:00:00Z
---

Set by the author, 2026-07-13, when picking the next development target (Void
Maiz 2D, demos-first; XR/NE later). **Node Blocks** is demo target #2 from
[substrates](/concepts/substrates.md) — a Scratch-like block editor built on
Void Maiz. It is chosen not for the domain but because it forces a genuinely
**new library capability** (hidden-wire rendering + a snap-to-connect gesture)
while staying 2D-euclidean-desktop, no new toolchain — the hardest test yet of
the projection architecture, since here *position is the visible wiring.*

Two author decisions fix its shape:

- **Structural-first, net-native model.** Ship the structural editor FIRST
  (snap = link, hidden wires — the new view work), but model the blocks as a
  **real interaction net** so "run = step the net via `maiz::reduce`" lights up
  later without redesign (Phase A / Phase B below). This honors Q9 ("apps should
  utilize the interaction-net structure wherever possible") without gating the
  view-capability proof on a full rule set.
- **Turtle / motion-graphics domain.** move / turn / pen / repeat / when-run —
  the instantly-legible "Scratch on Void Maiz" story, with visible output and
  control flow (repeat/if) exercising containment.

# Blockly's connection model (the studied prior art)

Per the [substrates](/concepts/substrates.md) directive to study Blockly before
designing the snap gesture. Blockly (which Scratch is built on) has **four
connection types** in two families:

- **Vertical (statements)**: a block's **previous** connection (top notch) and
  **next** connection (bottom tab) stack blocks into a sequence; a
  **statement-input** connection holds a nested stack (the C-block body).
- **Horizontal (expressions)**: a block's **output** connection plugs into a
  **value-input** socket on another block (numbers, booleans, reporters).

**Connection checks are a type system.** Every connection carries a nullable
array of strings; two connections may join iff their arrays **share at least one
string**, OR either array is **null** (the universal wildcard). The strings are
arbitrary labels (`'Number'`, `'Boolean'`, …) with no meaning beyond matching.

# The punchline: our linguine type-check already IS Blockly's

Void Maiz's existing aux-wire rule ([interaction-connections](/concepts/interaction-connections.md),
`check_wire`) — *linguine connects iff output meets input and types match, empty
type = wildcard* — **is Blockly's connection-check model.** Empty/`null` =
wildcard on both sides; otherwise the label must match. Node Blocks needs **no
new type system**; it reuses the linguine port-type check that
InteractionCombinators already exercises. (Blockly's per-connection *array* of
labels is a mild generalization of our single type string; `check_wire` can grow
to array-intersection if a block ever needs polymorphic sockets — filed as an
open sub-question, not a blocker.)

# The block-as-interaction-net model

Every block is a **rune** (agent). Its connectors are ports; snapping is
dispatch. The mapping, so nothing is bespoke:

| Blockly connector | Void Maiz model | rendered as |
|---|---|---|
| previous / next (vertical stack) | a directed **linguine** chain: aux `next` output → aux `prev` input | **hidden** — adjacency (blocks touching) shows the link |
| value output → value input | typed **linguine** into an aux value socket | the plugged connector shape (**connector shape = port type**, node-geometry) |
| statement-input (C-block body) | **subgraph / mantle-enter** containment (`hints.enter`, the VLS Loop pattern) | the block's inner well; a `>substack` marker |
| principal port + fettuccine | **RESERVED for execution** (the redex a program-cursor forms — Phase B) | nothing in Phase A; the hot halo in Phase B |

So a Phase-A program is a **linguine graph** (sequence + values); the principal
ports sit unused, waiting for the execution model. Connector shape as type is
[node geometry](/concepts/node-geometry.md) taken to its conclusion: Scratch's
notches are port types drawn geometrically.

# The two new library capabilities (what Node Blocks forces)

Both are generalizable and belong in the library, not the demo:

1. **Snap-to-connect gesture** (`voidmaiz` gesture layer, UI-free +
   `voidmaiz_view` for the hit-testing). On drag-release, if a compatible free
   connector lies within a snap radius of a dragged block's connector, compile
   **`link` + a `setjson pos` that aligns the block to the connector** — one
   batch, one undo frame (a snap is one lasagna layer). Verdict-tinted preview
   during the drag reuses the existing `check_wire` machinery. Dropping clear of
   any connector is a plain move. This is the wire-drag grammar inverted: the
   *body* proximity, not a port pull, initiates the link.

2. **Hidden-wire / adjacency render mode** (`voidmaiz_view`). A sequence
   linguine is **not drawn as a bezier**; the blocks' adjacency (stacked flush,
   notch into tab) *is* the visible connection. A per-wire render style
   (`drawn` | `adjacency`) on `SceneWire` — set by projection from a hint, not a
   new wire *kind* (it is still a linguine semantically). Value linguine may
   stay drawn-as-plug or hidden-as-socket per the block's face. This is the
   `wire rendering` axis from [substrates](/concepts/substrates.md) becoming
   real for the first time.

Block **body geometry** (the notch/tab silhouette) extends
[node geometry](/concepts/node-geometry.md) with a `"block"` shape kind whose
hints declare connector positions/shapes — the same "shape is notation, declared
not enumerated" posture as triangle/circle.

# Staging: A (structural) then B (run = reduce)

**Phase A — the structural editor (this block of work).** Turtle glyph set;
block-shaped bodies; the snap gesture; the adjacency render mode; C-block
containment via mantle-enter; the inspector/face for block arguments
(`move [10]` → a face drag-number, already built). Exit: build a turtle program
mouse-only, snapping blocks, and the transcript replays to an identical scene —
the same exit-test shape as Phase 3, now for snap gestures. **Nothing executes.**

**Phase B — run = reduction (later, net-native payoff).** A **program-cursor**
agent forms a fettuccine **active pair** with the current statement block;
`step` reduces it — the block's rule performs its effect (emit a turtle command
to the host output channel) and advances the cursor to `next`; `undo` walks
execution backward. This reuses the *entire* InteractionCombinators reduce
machinery (`maiz::reduce`, `active_pairs`, `step`, net-diff → batch). The turtle
effects ride an **effect/feedback sink** (host-registered, like the log sink) —
the library still computes nothing. Rules are demo config, not library code.

# Library vs demo (the boundary holds)

- **Library** (`voidmaiz` / `voidmaiz_view`): the snap-to-connect gesture +
  compiler, the adjacency render mode, the `"block"` shape kind. Generalizable —
  any snapping/adjacency host reuses them.
- **Demo** (`../NodeBlocks`, a sibling repo like InteractionCombinators): the
  turtle glyph set + their `hints`, the turtle **output face** (a small canvas
  widget drawing the turtle's path — host compute, via the face API), and any
  Phase-B rules. The turtle interpreter/effects are the *host's* compute
  (the boundary: Void Maiz renders and dispatches; applications compute).

# Phase A implementation (landed 2026-07-15)

The two library capabilities exist, test-pinned, and the demo repo is founded
(`../NodeBlocks`). The conventions as built:

- **Connectors are identified by port NAME**: the aux input named `prev` is
  the top notch, the aux output named `next` the bottom tab (exactly the
  hidden chain's wording above); other aux inputs are value sockets down the
  right edge, another aux output a reporter's plug on the left. Anchor math is
  ONE world-space function (`maiz::block_anchor`, UI-free) the view scales by
  the camera — gesture and drawing cannot disagree.
- **Adjacency is a port hint**: `"render":"adjacency"` on a port declaration;
  the projection stamps `SceneWire::Style::Adjacency` on any linguine touching
  such a port. Flush endpoints draw nothing; separated-but-linked blocks draw
  a dim honest tether (the link exists even when geometry disagrees).
- **The snap release is ONE batch** (`maiz::compile_block_release`): staged
  moves (whole dragged set aligned rigid), occupancy unlinks (block connector
  ends are single-occupancy on BOTH sides), the link, **splice** (dropping on
  a seam inserts — the displaced neighbor re-attaches to the dragged stack's
  free end), **tear** (adjacency wires pulled past `tear_factor ×
  snap_radius` unlink — dragging out of a stack IS the unlink gesture),
  **heal** (a middle block's old neighbors join), then the re-flow of any
  resting chain the links changed. One undo frame un-does all of it.
- **Grab-the-stack**: dragging a block stages its statement chain below plus
  plugged reporters (`maiz::block_stack_below`) — selection stays honest, only
  the staged set grows.
- **Stack re-flow on demand**: `maiz::compile_stack_layout` — one batch that
  brings every chained block flush (the blocks' "clean").
- **The principal is not a drag target on blocks** (undrawn, unhittable) —
  reserved for Phase B's program cursor, per the table above.
- Geometry rides `maiz::BlockMetrics` (defaults 150×40, notch at x=26, snap
  radius 18, tear 2.5×), shared by `CanvasStyle::block` and the compilers.

# Open sub-questions (leans, resolve as we build)

- **Connector array vs single type** — grow `check_wire` to Blockly's
  array-intersection, or keep the single type string? *Lean: keep single now;
  generalize only when a polymorphic socket appears.* (Void Hormiga's typed
  sockets, 2026-07-15, use the single-type model hard and will report if they
  ever hit a polymorphic socket.)
- ~~**Adjacency as render-flag vs wire kind**~~ — resolved as the lean: a
  render style on the linguine (2026-07-15).
- ~~**Snap alignment ownership**~~ — resolved as the lean, extended: the snap
  emits the aligning `setjson pos`, and tears/heals/splices ride the same
  batch; re-flow is an explicit layout act (`compile_stack_layout`), not a
  projection behavior — snapped blocks are placed, and placed positions stay
  model content (2026-07-15).
- ~~**Demo repo name/location**~~ — `../NodeBlocks`, as leaned (2026-07-15).
- **Splitting a C-block's body view** — the demo enters a repeat's body via
  mantle-enter (the VLS Loop pattern); Scratch draws the body INSIDE the
  C-block. An inline-substack rendering is a future view capability, not a
  model change (the containment is already right).

# What this demands of the library TODAY

Nothing above the view module gains block- or snap-specific types (the substrate
rule): the snap gesture compiles to the same `link` command a wire-drag emits;
the adjacency mode is a `voidmaiz_view` drawing choice; block geometry is a hints
declaration. Total observability holds — snapping a block is a logged, replayable
`link`, and Phase B execution is logged `batch`es, exactly as
InteractionCombinators' rewriting already is.

---

*Sources for the Blockly study:*
[Blockly connection checks](https://docs.blockly.com/guides/create-custom-blocks/inputs/connection-checks/),
[Blockly connections guide](https://docs.blockly.com/guides/create-custom-blocks/connections),
[Connection checks (Google for Developers)](https://developers.google.com/blockly/guides/create-custom-blocks/inputs/connection-checks).
