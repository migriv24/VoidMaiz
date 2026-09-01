---
type: Concept
title: Views as projections
description: The renderer architecture — a view projects the state document into pixels and compiles input back into commands; Dear ImGui is the first backend, node faces and widgets are per-glyph renderers.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-09T00:00:00Z
---

Because [Void Maiz owns no truth](/concepts/log-first-inheritance.md), a *view* is
just a function pair:

    project : state document  → scene (what to draw)
    compile : input gesture   → dispatcher commands (what the user meant)

Everything renderable is downstream of `project`; everything mutable is upstream
of `compile`. A view that honors this contract can look like anything — which is
how the draft's "visual metamorphosis" (§7) survives, minus the mysticism.

# The first view: the 2D canvas (ImGui backend)

Per VLS question #16 (decided C++20 + Dear ImGui): the first backend renders with
ImGui's draw lists into a host-provided window. We build the node canvas
**ourselves** (the author's instinct), studying **imnodes** as the reference the
way LiteGraph was studied for the web editor — small enough to own, not worth
adopting wholesale because the canvas IS the product here. What v1 must match is
the *proven* VLS editor feature set, piece by piece (see [roadmap](/roadmap.md)):
typed ports in signal colors, type-checked wire dragging, multi-inlet spare
slots, marquee/multi-select, searchable add box at the cursor (Shift+A), collapse,
depth auto-layout for placeless nodes, and the log strip.

# Node faces & widgets (founding features, not extensions)

Carried over from VLS by the author's explicit call (2026-07-09: "widgets as
nodes will also be a feature within the new c++ version"):

- **A glyph can supply a face renderer** — the node's body drawn by domain code
  (piano roll, waveform, frequency curve, live camera). Faces receive the rune's
  content + tagged children and emit commands like any view. VLS precedents: the
  midi node's in-canvas roll, VST nodes driving a real native plugin window.
- **Nodes are windows**: collapse (title-bar dot) / maximize (resizable face) —
  the window chrome is Void Maiz's, the face is the glyph's.
- **Face widgets**: curated fields render as sliders/drag-numbers/combos ON the
  node; a field owned by the control graph (wired param) shows disabled. The VLS
  #14b staging discipline carries over: widget drags stage locally and flush as
  ONE command on release — the transcript stays readable, and re-projection never
  yanks a control mid-gesture.

# Motion without state: the CanvasFx layer (2026-07-14)

The canvas can *animate* a model change without owning anything: `CanvasFx`
(canvas.hpp) is a per-frame bundle of transform overrides (world center +
scale, by node name) and **ghosts** (visual copies of nodes the model no
longer has). The host rebuilds it every frame from its own animation clock and
hands it to `edit_canvas`; geometry, wire anchors, and hit-testing all honor
it, so wires follow mid-flight nodes and a not-yet-emerged copy can't be
clicked. Drop the struct and the picture snaps to the model — the projection
discipline's animation corollary: **motion is a replay of a change that
already happened, never a preview of one that hasn't.** First client: the
demo's rewrite choreography (redex pair glides together, compresses, collapses
into the fusion point; what the rule left behind grows out of it).

# Later views (the same mantle, other projections)

The draft's table of "rendering modes" becomes a *test of the architecture*, not
a promise list. Credible early candidates, each driven by a real client need:
a **timeline/arrangement view** (VLS's DJ-dynamic sequences will want one), a
**tree/outline view** (mantle structure, cheap), and a **text view** (the CLI
transcript already is one — making it a first-class panel closes the loop).
3D/spreadsheet/force-directed: deferred until someone needs them; nothing in the
contract forbids them.

# Host-built views: the sanctioned seam (2026-07-16)

Void Hormiga asked (its Territory section wants runes-with-location-facets on
an interactive MAP) what shape a third-party view should take, and the answer
is: **exactly the pair at the top of this page, concretely `project_scene` in
and compiled commands out.** A host view

- consumes a `Scene` (or, for readings the scene doesn't carry, the exported
  state document — but prefer the scene: it is the shared projection every
  other surface agrees with);
- returns command lines the host dispatches, then re-projects (the one-sync
  rule — `edit_canvas`, `draw_inspector`, and every protocol widget already
  have this exact signature shape);
- filters with `node_matches` (the one filter bag: tags + name + `glyph:<g>`);
- keeps its viewport/camera analog as **config-tier view state** (`view.*`
  namespace, flushed on gesture end, undo-exempt — the Q3 charge that the
  camera abstraction stay substrate-shaped applies: a map's lat/lon/zoom fits
  the same pattern as the canvas camera);
- stages in-flight gestures locally and flushes ONE command/batch on release.

`EditorState` is NOT part of the seam — it is the 2D canvas's own gesture
state; a map or table keeps its own equivalent. The library's coming table
view will be built against this same contract deliberately, as its
first-party proof.

**Naming the writes: canvas actions (2026-07-21).** The seam makes a view's
commands first-class in the transcript, but not its *vocabulary* — a view
hand-rolls gesture handling and emits commands anonymously. The optional
[canvas-actions](/concepts/canvas-actions.md) layer (`voidmaiz/action.hpp`)
lets a view declare **named, param-schema'd, introspectable** actions whose one
host `compile(scene, args)` drives both a gesture and (once Core grows verbs) a
CLI verb — so a click and a typed command are the same logged entry, and an
agent can enumerate what a canvas affords. It sits on top of this seam: the
compile still returns command lines the host dispatches; the descriptor only
adds names, a schema, and discovery.

**The geographic camera, realized (2026-07-20).** Void Hormiga's Territory
map is this seam with a geographic coordinate space. The `Camera` was ALREADY
substrate-shaped — three floats, a viewport, not pixels — so a geographic
viewport simply reuses it (x=lon, y=lat, zoom=map zoom); the lat/lon→screen
projection, base-map raster, tiles, and markers are the host's, and no
geographic type enters the substrate-free core (substrates.md's quarantine).
The one gap closed: `compile_camera(cam, config_key)` now takes the `view.*`
key, so a SECOND view persists its OWN viewport (`view.map.camera`) through the
blessed undo-exempt config-tier helper, at `%.7g` precision (lat/lon survive
the round-trip). Ruling: a flat map is **2D-in-nature** — it stays Void Maiz;
a 3D globe/terrain map would be Void Maiz XR ([substrates](/concepts/substrates.md)).

**Hosting many views: the docking workspace (2026-07-20).** With four heavy
workflows (data / blocks / node graph / map), the fixed shell strained. The
author ruled to enable Dear ImGui's **DockSpace** (Q11, retired the old "no
docking framework" lean): panels are movable / floating / re-dockable, using
the vendored ImGui's own feature rather than a hand-rolled pane manager.
DockSpace only — never multi-viewport (the single-surface NDK path). The
library ships the boilerplate (`enable_docking`, `begin_dockspace` /
`end_dockspace` in widgets.hpp); the host still owns WHICH panels and their
default arrangement. Each panel is a view built against the seam above — the
dockspace only changes how they are *hosted*, never how they project.

> **Correction, 2026-08-06: it had never actually worked.** All three docking
> paths in `widgets.cpp` were guarded by `#ifdef ImGuiConfigFlags_DockingEnable`
> — an **enum value, not a macro**, so every guard was false and
> `enable_docking` was a silent no-op from the day of the ruling. The
> checked-in `imgui.ini` hid it perfectly: it carries **zero** `DockNode`
> entries, so every "docked" panel was a floating window with remembered
> geometry, which looks right until someone starts fresh. Fixed to
> `IMGUI_HAS_DOCK`. A second trap came with it — the seed guard every host
> writes, `DockBuilderGetNode(dock) == nullptr`, can never be true, because
> `begin_dockspace()` has already called `DockSpace()` and created the node; so
> a first run built no default layout at all. The library now answers that
> question itself (`maiz::dockspace_needs_seed`), and a user's saved arrangement
> still wins. Found by building the Allomone demo, which had no `.ini` to
> inherit — the general lesson being that a checked-in layout file is a
> first-run test nobody is running.

# Non-goals of the render layer

- It does not execute the graph (compute boundary — the application computes).
- It does not persist anything (view state goes through the dispatcher like
  everything else — [total observability](/concepts/total-observability.md)).
- It does not define semantics: colors, port types, and face meanings come from
  glyph metadata the host registers. Void Maiz ships sensible defaults, not
  opinions.
