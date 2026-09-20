---
type: Concept
title: Substrates & dimensions
description: "What a visual representation of Void Core may assume (an OS, compute, *a* visual component — NOT a rectangular display, a mouse, 2D, or euclidean space), the abstraction axes that follow, and the planned targets: mobile, VR/3D, and block languages."
tags: [status:planned, audience:dev, confidence:asserted]
timestamp: 2026-07-13T00:00:00Z
---

Set by the author, 2026-07-13. Void Core is a framework for literally anything
(drones trading agent messages included). Void Maiz is its VISUAL
representation — still extremely abstract. This concept fixes what "visual"
is allowed to assume, so nothing built from here on quietly hardcodes more.

# The assumptions (and the deliberate non-assumptions)

A Void Maiz host device is assumed to have:

1. **A filesystem / an OS** — some way to manage data.
2. **Compute.**
3. **A visual component.**

Explicitly NOT assumed (the author considered and rejected each):

- a **rectangular display** (desktop windows are one substrate among several),
- a pointer/keyboard (touch, controllers, hands are peers),
- **two dimensions**,
- **euclidean space** (a research horizon, but a real one — see references).

# The abstraction axes

Every target is a coordinate on these axes; the library must keep them
independent:

| axis | values today | planned values |
|---|---|---|
| dimension | 2D | 3D |
| space/metric | euclidean, **geographic** (lat/lon, host-projected) | hyperbolic (research) |
| substrate | desktop window, **mobile touch** | HMD (OpenXR) |
| input | mouse/keyboard, **touch gestures** | 6-DoF controllers + haptics, hands |
| wire rendering | drawn (bezier) | hidden/adjacency (**blocks**: snap = link) |
| motion | none (instant re-project) | animations/tweens (device-modular, not yet built) |

# What is already substrate-free (keep it that way)

- **The embed layer, projection engine, gesture compilers, reduce executor** —
  pure C++20, pure data, zero rendering types. A VR grab-and-connect and a
  touch drag compile to the *same* `link` command; total observability makes
  every frontend just another command emitter. This is the portability crown
  jewel.
- **The touch recognizer** (`voidmaiz/touch.hpp`, 2026-09-13) — contacts in,
  gestures out, no rendering types, and its thresholds in millimetres rather
  than pixels. It belongs on this list because it is the proof of the axis:
  a pinch and a mouse wheel compile the same `config set view.camera`, and a
  long press synthesizes a right-click so the canvas never learns a finger
  exists. **The modality is absorbed at the boundary.** See
  [touch](/concepts/touch.md), which is where this page's "touch is an input
  modality, not a geometry" stopped being a slogan and became a state machine.
- **The hints conventions** (ports, shape, color, enter) — semantic
  declarations ("the principal is the apex"), not pixels.
- **View-state-as-content** — `pos` is an opaque array in content; `[x,y,z]`
  is a compatible extension the day a 3D client exists.

**The rule going forward: nothing above the view module acquires dimension-
or device-specific types.** The 2D-bound code (Camera, canvas, hit-testing,
CanvasStyle, ImGui) is deliberately quarantined in `voidmaiz_view`. The
Scene's `x,y/rot` fields are read as the z=0, roll-only slice of the general
case; they grow axes when the first real 3D client starts, not speculatively.

**Geographic views are 2D-in-nature (ruled 2026-07-20).** Void Hormiga's
Territory map projects runes-with-lat/lon onto a base map. That is a new
*metric* (geographic coordinates), NOT a new dimension: interaction stays
2D-euclidean on a rectangular display, so a flat map **belongs to Void Maiz**,
not Void Maiz XR (a 3D globe/terrain map would be XR). The library stays
substrate-free by keeping geography entirely host-side: the `Camera`'s three
floats already serve as a geographic viewport (x=lon, y=lat, zoom), persisted
via `compile_camera(cam, "view.map.camera")` — the substrate-shaped config-
tier pattern Q3 charged us to keep — while the lat/lon→screen projection, base
map, tiles, and markers are the host's. No geographic type enters the core; the
map is a host view against the [views-as-projections](/concepts/views-as-projections.md)
seam, like the table.

# The library family — XR and NE are SEPARATE libraries (decided 2026-07-13)

*(Registry and charters in [horizons](/horizons.md); Void Maiz itself is "2D
in nature": 2D euclidean interaction space, free to render with 3D graphics —
the modern-2D-Mario principle. Void Maiz NE, the non-euclidean sibling, is
deliberately back-burnered.)*

Rather than a `voidmaiz_view3d` module, 3D spatial nodes live in their own
library, **Void Maiz XR**, repeating the speciation that created Void Maiz
out of VLS (own repo, own OKF, message files between agents):

- **Void Maiz hardens its charter**: rectangular display, 2D euclidean
  graphics, pointer OR touch. Every 2D API decision gets simpler; no
  dimension hedging, ever. (Touch is an input modality, not a geometry — the
  Android playground stays HERE; only IC-VR moves to XR.)
- **The seam already exists in the build**: XR consumes `voidmaiz` (embed +
  projection + gesture compilers) and `voidmaiz_reduce`, and replaces
  `voidmaiz_view` wholesale — its own renderer (OpenXR), spatial gestures,
  haptic feedback sink, solids (cone = 3D triangle), quaternion
  auto-orientation.
- **Void Maiz's OKF conventions are normative**: hints, wire kinds, staging
  discipline; XR conforms (the same posture as web adoption, Q#10), with
  conformance cases if drift ever threatens.
- **The repo is created when XR work starts, not before** — the decision
  pays now (Void Maiz's constraints harden today); the scaffolding waits for
  its client.

# The planned targets (each is a demo — the test matrix)

1. **InteractionCombinators** (shipping): 2D · euclidean · desktop ·
   mouse/kb · drawn wires. Tests the mathematics + 2D geometric abstraction.
2. **Node Blocks** (desktop): a Scratch-like built on Void Maiz. 2D-euclidean
   (the author's "3D node block" reads as visual depth/stacking, the model is
   planar) · **hidden wires**: snapping blocks together IS the link gesture;
   sequencing is an i/o port pair; containment (C-blocks) is the subgraph
   pattern; **the connector shape is the port type rendered geometrically**
   (Scratch's notches = node-geometry taken to its conclusion). Everything is
   an interaction-net framework underneath, and the CLI still works.
3. **InteractionCombinators APK** (shipping, 2026-07-14): 2D · euclidean ·
   mobile · touch. Superseded the planned bare "playground" — the author
   asked for IC itself, which forces the axes harder: the FULL app (projects,
   ribbon, animation, physics) is platform-free (`src/app.cpp`), and the two
   shells (GLFW desktop, NativeActivity) are input sources around it. One
   finger = ImGui pointer (the desktop gestures verbatim); two fingers =
   camera (pan/pinch → the same `config set view.camera` command). Built
   Gradle-free: NDK CMake → aapt2 → zipalign → apksigner; Void Core compiles
   from C11 source into the .so. v1 gaps, deliberate: no soft keyboard (Save
   As pre-fills a timestamp name; the command bar is desktop-only until an
   IME shim lands), no long-press context menu.

   **Both gaps moved on 2026-09-13.** The long-press menu is **built and in the
   library** — it synthesizes a right-click, so it is the desktop menu rather
   than a second one — and the soft keyboard is now **Q29**, a ground-rule
   question rather than a to-do: the platform IME means JNI into Android's Java
   runtime, which costs this entry's own zero-Java claim. The APK's hand-rolled
   touch layer is superseded by `voidmaiz/touch.hpp`; its shell shrinks to the
   six-line hookup in `examples/mobile_window.cpp`.
4. **InteractionCombinators VR (Quest 3)**: 3D · euclidean · HMD · 6-DoF +
   haptics · non-traditional connection gestures. The node-geometry concept
   extends to solids: **a cone is the 3D triangle** (apex = principal, base
   circle = aux ports), a sphere is ε; auto-orientation (point the principal
   at its partner) is *more* natural in 3D, not less. Haptic feedback arrives
   as a **feedback sink** — gesture events (port grabbed, verdict ok/reject,
   link made) emitted through a host-registered channel, symmetric with the
   log sink.

# The language-boundary question (the C++20 rule, walked through)

- **Android & Quest 3 are the same answer**: both are Android; the NDK builds
  C++20; OpenXR is a C API; Dear ImGui ships android/GLES backends; GLFW is
  replaced by the platform's native glue. The rule survives with one
  footnote: *platform shims (a Java activity, gradle files) are build
  scaffolding with zero logic in them* — like vendored licenses, they don't
  count. **Proven 2026-07-14**: the IC APK shipped with zero Java and zero
  Gradle — a hasCode=false NativeActivity manifest and a packaging script are
  the entire shim.
- **The web is the real boundary.** Two honest paths, kept as *general*
  options:
  (a) **Emscripten/WASM** — the actual C++ library (voidcore compiles too) in
  the browser; heavyweight, but it is the same code.
  (b) **Protocol-level adoption** — a JS/TS view that implements the OKF's
  conventions (scene projection, gesture→command grammar, hints, staging
  discipline) against voidcore over the `/state`+dispatcher seam — exactly
  VLS's proven editor.html pattern. Void Maiz here is a *spec*, not a binary.
  If a web client ever exists, the conventions get pinned the way the reduce
  contract was: language-neutral conformance cases, so the two can't drift.
  **Neither is Hormiga's path anymore.** The recorded lean ("(b) for
  Hormiga's Blocks/Antfarm now") was reversed by the author on 2026-07-15 at
  **Void Hormiga**'s founding (`../VoidHormiga`, its `MESSAGE_FOR_VOIDMAIZ.md`):
  Hormiga is rebuilt ground-up as a **native C++20 application on Void Core +
  Void Maiz** — no web shell, no protocol adoption. The calculus changed
  because Void Maiz changed: when the lean was written the library didn't
  exist as working code; by the reversal it had a shipped desktop demo, an
  APK, and a proven app/platform split. Void Hormiga arrives as a native
  client in the InteractionCombinators sibling-repo pattern (see
  [scope & clients](/concepts/scope-and-clients.md)).

# References (the author asked not to work blind)

- **3D hyperbolic graph layout**: Munzner's **H3** ([paper](https://graphics.stanford.edu/papers/h3/html.nosplit/),
  [H3Viewer](https://graphics.stanford.edu/~munzner/h3/)) — large directed
  graphs in 3D hyperbolic space; hyperbolic volume grows exponentially, an
  "impedance match" for trees; Focus+Context navigation. Walrus is a later
  implementation. The 2D ancestor is Lamping & Rao's hyperbolic tree browser
  (Xerox PARC, 1995).
- **VR node-link interaction** (recent academia): ray-based selection is the
  default but degrades on dense graphs — a comparative study found the
  *filter-plane* metaphor superior ([SUI '24](https://dl.acm.org/doi/10.1145/3677386.3682102));
  aesthetic-driven navigation for node-link diagrams in VR
  ([SUI '23](https://dl.acm.org/doi/10.1145/3607822.3614537)); multi-focus
  probes for editing 3D node-link diagrams, egocentric view wins for search
  and navigation ([arXiv 2025](https://arxiv.org/pdf/2507.01140)); commercial
  AR/VR graph exploration ([yFiles](https://www.yfiles.com/solutions/use-cases/ar-vr-graph-visualization)).
- **Block languages**: Google **Blockly** (developers.google.com/blockly) is
  the open, documented reference — its *connection checks* are a port type
  system, its custom renderers (Geras/Zelos) show connector-shape-as-type,
  and Scratch itself is built on it. Study its connection model before
  designing Node Blocks' snap gesture.

# What we do NOW (cheap) vs LATER (pulled by a client)

Now: keep the rule (no new dimension/device types above the view), write
positions as arrays, name geometry semantically. **The mobile substrate stopped
being "later" on 2026-09-13** — recognizer, chrome kit and reference host, ahead
of Void Hormiga going mobile ([touch](/concepts/touch.md)). The animation layer landed
2026-07-14 (`CanvasFx`, host-clock-driven — already device-modular: the APK
plays the same choreography untouched), and the APK itself shipped the same
day (see the target list). Later, in order of pull: Node Blocks (desktop,
mostly view work) → IC-VR (a real Void Maiz XR) → non-euclidean camera
(research, H3 as the map).
