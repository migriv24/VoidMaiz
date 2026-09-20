---
type: Backlog
title: Backlog
description: The long feature list — everything wanted, tiered by necessity, knocked out one by one. Includes the Blender/LiteGraph feature matrix (answers open question #7).
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-11T00:00:00Z
---

The author's direction (2026-07-11): *keep LONG lists of features, knock them
out one by one, maybe only the necessary ones.* Tiers:

- **T1 — necessary**: the InteractionCombinators demo can't ship without it
  (open question #8's scope).
- **T2 — wanted**: makes the library genuinely good; scheduled after T1.
- **T3 — someday**: recorded so it isn't re-invented; needs a client to pull it.

Done items move to ~~struck~~ with the log entry date.

# T1 — necessary for the demo

- ~~Embed layer (`maiz::Core`)~~ (2026-07-10)
- ~~Projection engine: state → scene, hints, i:j wires, depth auto-layout~~ (2026-07-10)
- ~~Canvas rendering: nodes/ports/wires, three wire kinds, grid~~ (2026-07-10)
- ~~Camera pan/zoom; click/shift/marquee selection~~ (2026-07-10)
- ~~Node drag → setjson pos (batch = one undo frame)~~ (2026-07-10)
- ~~Wire drag with type checking; rewire/detach; fettuccine 1:1~~ (2026-07-10)
- ~~Add box (Shift+A) → rune new at cursor~~ (2026-07-10)
- ~~Inspector field editing (staged, set/setjson)~~ (2026-07-10)
- ~~Collapse/resize chrome; subgraph enter/exit~~ (2026-07-11)
- ~~Command bar — the CLI inside the UI, result echo, Up/Down history~~ (2026-07-11)
- ~~Log strip widget (level-colored, pinned-to-tail)~~ (2026-07-11)
- ~~Undo/redo surfacing — Ctrl+Z/Ctrl+Shift+Z/Ctrl+Y + visible undo depth~~ (2026-07-11)
- ~~Face renderer API — per-glyph body renderers via `FaceRegistry`~~ (2026-07-11)
- ~~Default face widgets: drag-number + combo, staged, disabled-when-wired~~ (2026-07-11);
  slider variant still open (T2)
- ~~Reduce port (C++20) — `maiz::reduce` (`voidmaiz_reduce` target, the canvas
  never calls it); all 10 pinned cases pass on canonical forms, in ctest~~ (2026-07-11)
- ~~Active-pair affordance — `SceneWire.active` + hot halo; `active_pairs` and
  single-`step` exposed from maiz::reduce; step compiles a net-diff into ONE
  batch (undo un-rewrites)~~ (2026-07-11)
- ~~The demo app itself (`../InteractionCombinators`): γ/δ/ε glyphs, six
  Lafont rules, step/reduce/undo, shortcut-launchable~~ (2026-07-11, founded;
  dataflow-patch-alongside and polish continue there)
- ~~Phase 3 exit test — `replay_smoke` (ctest): a 17-command transcript of
  compiled gestures (add/link/rewire/group-move/set/tag/chrome/fettuccine/
  undo/redo/delete) replays into a fresh core to an identical projected
  scene; replay-by-state also pinned~~ (2026-07-13). **T1 is complete.**

- ~~Node geometry: glyph-declared shapes (triangle/circle/polygon) with
  perimeter port anchors and `rot:"auto"` (principal faces its partner);
  true-shape hit-testing; demo reshaped to real notation~~ (2026-07-13)

# T2 — wanted

## Networking (opened 2026-09-18, Q30; [networking](/concepts/networking.md))

- ~~**Stage A**: surfaces (immediate mode), presence and roster, bounded codec,
  one renderer, `CanvasNet`, and the Allomone `present` / `share_by_annotation`~~
  (2026-09-18)
- ~~**Stage B**: profile presentation, member list, the Networking settings with
  sender and receiver groups~~ (2026-09-18)
- ~~**Stage C**: `voidmaiz_net`, real Cores syncing through Palabra's session:
  the replica loop, the splice, presence over the ephemeral channel, and the
  ShareFilter as the ExportSet~~ (2026-09-19)
- **A real transport**: gated on the trust model (Palabra's open-questions §6.1).
  Whether Hormiga's sealed stand-in may carry session frames meanwhile is the
  author's explicit call.
- ~~**Conflict and anomaly UI.** Plain rows projected by `voidmaiz_net`
  (`conflicts()`, `anomalies()`, `resolve()`), drawn by `draw_conflicts` /
  `draw_anomalies` in the view module, which links no sync library~~ (2026-09-19)
- **Conflict provenance in the UI.** A row says what disagreed, not WHO or WHEN.
  Palabra's utterance history could name both; the panel has nowhere to show it
  yet.
- **A join flow that does not seed the document.** Joiners must receive mantles,
  not create them (see the concept). Worth a helper once a second application
  joins.
- **Avatar textures.** `Profile::avatar` is an asset reference with no texture
  path, so every avatar is an initials disc.
- **Presence on glass**: a declaration on a bottom sheet, and badges at touch
  size. Not yet drawn.
- **The surface census reads `Surfaces`.** The census has waited for a harvest
  source since July, and now has one.
- **The cautious-files placeholder**: the setting exists; the placeholder a view
  draws for a known-but-unfetched file does not.

## Mobile & touch (opened 2026-09-13, ahead of Void Hormiga going mobile)

Built this session — recorded so the tier list shows what the kit already has:

- ~~**Touch recognizer** (`voidmaiz/touch.hpp`): the deferred press (tap vs
  drag vs long press decided BEFORE the pointer is delivered), two-finger
  pan/pinch/rotate, fling, opt-in edge swipe, a millimetre-based `TouchProfile`,
  and the pointer policy the IC shell hand-rolled~~ (2026-09-13)
- ~~**Mobile chrome kit** (`voidmaiz/mobile.hpp`): bottom sheet with detents,
  snackbar with an UNDO action, FAB + speed dial, segmented control, stepper
  with hold-to-repeat, swipe-actionable list row, `apply_touch_canvas`~~
  (2026-09-13)
- ~~**Reference touch host** (`examples/mobile_window.cpp`): a phone-shaped
  desktop binary where the mouse is a finger and Alt adds a mirrored second
  one — so the kit is looked at, not reasoned about. **Built, not yet run**~~
  (2026-09-13)

Next, roughly in order of pull:

- **Run the reference host and fix what a hand finds.** The honest gap: 81
  assertions cannot tell you a gesture feels wrong. First job of the next
  session.
- **Stroke gestures** — draw a stroke across wires to CUT them, a circle to
  lasso-select, a stroke node→node to link. The handheld borrowing (*Phantom
  Hourglass*, *Kirby Canvas Curse*) and the same feature as the existing T2
  "wire cut gesture (Ctrl-drag)" arriving from the touch side. **The one most
  worth building next**; compiles to `unlink`/`link`, no new command shapes.
- **Fling consumption** — the recognizer reports velocity; the canvas does not
  coast. Inertia is an animation, so it belongs in `CanvasFx`, not in the
  camera.
- **`widget_field_stepper`** — the registry-side field editor (ONE
  `set`/`setjson` per tap), so a glyph's `hints.editors` can name `"stepper"`
  and every surface picks it up. The raw control shipped; the registry citizen
  did not.
- **Action sheet / wheel picker / pull-to-refresh** — a bottom sheet at a fixed
  detent, a drum for enums and dates, and "re-read the state document" as a
  real gesture rather than a hack.
- **Safe-area insets** — notches and gesture bars; ImGui's work area covers
  part of it, not all.
- **Haptic feedback sink** — designed for XR (gesture events → a host channel,
  symmetric with the log sink) and pulled forward by mobile, where a verdict
  buzz is the cheapest possible confirmation.
- **Soft keyboard / IME** — **Q29**, a ground-rule question before it is a
  feature: the platform IME costs the zero-Java claim.
- **Responsive pane layout** — **Q28**, leaning NO. The pieces shipped; the
  engine waits for a second host writing the same twenty lines.

- ~~**`field_of(SceneNode, key)` in the library.** Reading one field out of a
  projected node means walking `fields` and un-escaping `value_json`, and there
  were **three private copies** of that (the Allomone demo, `command_smoke`,
  and the census path) plus a fourth in Void Hormiga, which had already diverged
  on `null` before anyone compared them. Shipped in `project.hpp` with
  `find_field` alongside it, the `null == absent` rule written down (Hormiga's
  answer, taken), and cJSON doing the decode so `\uXXXX` resolves — which the
  four-line copies silently mangled~~ (2026-08-18, on Hormiga's A3.1 vote)
- ~~**`arg(value)` in the library.** Wrapping a value as one dispatcher
  argument. Four lines, and Hormiga and Void Reyna got them wrong independently
  in two languages, both silently corrupting content. Shipped in `embed.hpp`~~
  (2026-08-18)

*Author's first hands-on feedback batch (2026-07-13) — the Q8 loop working.
Items marked **[fb]** came from it and outrank the rest of T2:*

- ~~**[fb#6] Principal↔aux wire drags** — `check_wire` now returns Linguine
  for principal↔aux (a passive wire, how real IC programs get built); the
  drag grammar routes direction by the aux end (0:j / i:0), principals are
  strictly single-occupancy (linking one clears its standing wires), and
  grabbing a principal holding a passive wire detaches it for re-routing~~
  (2026-07-13)
- ~~**[fb#5] Per-rune color as content** — `content.color` ("#rrggbb")
  overrides `hints.color` in the projection (undoable, view-state-as-content
  tier); demo's ε declares `color` editable so erasers can be recolored to
  track port remappings through rewrites~~ (2026-07-13)
- ~~**[fb#1] Clean view (overlap resolution)** — `maiz::compile_clean(scene)`:
  iterative pairwise separation of bounding boxes (+margin), ONE batch of
  `setjson pos` = one undo frame; already-clean scenes compile nothing
  (test-pinned). Demo grew a "clean" header button~~ (2026-07-13). *Wire
  crossing minimization is the remaining half of the tidy story.*
- ~~**[fb#2] Laptop-friendly camera controls** — Space+left-drag and
  Alt+left-drag pan (no middle button needed); right/middle-drag unchanged~~
  (2026-07-13). *Two-finger scroll pan / a host-tweakable binding table
  remain open if the chords don't cover it.*
- ~~**[fb#3] Right-click context menu** — `edit_canvas` opens a menu on a
  clean right-CLICK (right-drag still pans; click-slop disambiguates): node →
  collapse/expand + delete(-selection); empty canvas → add submenu from the
  palette; hosts extend via the new `ContextMenuFn` hook (entries render above
  built-ins, push commands into CanvasIO — the menu is just another compiler
  frontend)~~ (2026-07-13). *Remaining: wire targets (needs wire hit-testing)
  and duplicate (needs a `rune dup`-shaped compile).*
- ~~**[fb#9] Log copy options** — `maiz::log_to_text(entries, condensed)` +
  right-click menu on the log strip ("copy condensed"/"copy all" → OS
  clipboard); condensed keeps the structural story (errors/warnings +
  rune/rm/link/unlink/tag/mantle/undo/redo/set + structural batches; drops
  camera config, pos/size/collapsed writes, move-only batches, query chatter);
  demo adds visible copy buttons above the strip~~ (2026-07-13)
- ~~**[fb#10] One interaction at a time** — right-click a hot agent →
  "interact — fire this pair": steps exactly THAT redex (`compile_step` takes
  a chosen agent; `maiz::reduce::step` already accepted a specific redex).
  step/reduce buttons unchanged; confluence makes order immaterial, but the
  choice is now the user's~~ (2026-07-13)
- ~~**[fb#4] Tooltips** — ports/wires tip immediately (name : type (dir);
  from — to (relation)); nodes show a dwell card (~0.55s: name, label, tags,
  fields, enter hint)~~ (2026-07-13). *Facets on the card when someone
  renders them into the scene.*
- ~~**[fb#11] Hover highlights** — a `hover` theme accent outlines whatever
  the cursor would act on: node bodies by TRUE shape (rect/circle/polygon),
  port rings (also during wire drags), collapse-dot/resize-grip accents,
  wires re-traced thicker (bezier-sampled `hit_wire` — the first half of
  wire hit-testing)~~ (2026-07-13)
- ~~**[fb#13] Quick add-and-link** — a fresh wire dropped on empty canvas
  opens the add box at the drop point; the pick mints the node, places it,
  and links it back to the dragged port via its PRINCIPAL (always legal:
  fettuccine from a principal, passive linguine from an aux) — one batch,
  one undo frame~~ (2026-07-13). *A radial "wheel" presentation is possible
  polish; the filterable list ships first.*
- ~~**[fb#14] Pigment inheritance through rewrites** (demo) — the contract's
  copies start tagless (engine untouched); the demo's step compiler re-tags
  minted copies from their same-glyph redex parent, as visible `tag`
  commands inside the step batch~~ (2026-07-13)
- ~~**[fb#12] Tag pigments** — tags ARE colors, mixed by a background
  interaction net: color tags fold through arity-0 pigment agents whose
  redexes fire the new **fuse rule kind** (`a~b → into`, our `maiz::reduce`
  extension beyond the upstream contract; boundary-adopting general form,
  test-pinned). RYB in the demo: primaries → secondaries, else brown~~
  (2026-07-13). *Generalize to a reusable library helper when a second host
  wants it. Upstream FYI happened: Void Core 0.2.4 recorded `fuse` in the
  contract's home concept page as noted-not-adopted — it becomes a candidate
  the way `swap` did when a second consumer appears.*
- ~~**[fb#4b] Shaped-node resize** — the gap behind the resizing gripe: the
  grip only existed on window chrome, and the demo is ALL shapes. Now:
  selected shaped nodes draw a grip at their bounding-box corner (checked
  before body hit-testing — the corner lies outside the true shape),
  shape-aware minimums (24px vs window chrome's), same `setjson size`
  release; plus a manual **size field in the inspector** (staged, "w h",
  one command on commit). The author's resize-as-tag idea was proposed and
  retracted same-day — size stays view-state-as-content, the existing
  tier~~ (2026-07-13)
- **[fb#7] Vicious-circle detection** — detect principal→aux cycles that can
  never reduce (Lafont's deadlock; defined in the demo's
  `okf/interaction-combinators.md`) and surface them (wire tint/badge).
  Detect, never prevent (author's ruling). Scene-analysis helper in the
  library; rendering affordance in the view.
- **Smush squash** — the rewrite animation (2026-07-14) scales bodies
  uniformly; the author's "smush" reads better with non-uniform compression
  along the approach axis, which needs elliptical/squashed shaped-node
  rendering in the canvas. Polish, not correctness.
- **Relax, part two** — `compile_relax` (2026-07-14) handles overlap +
  wire-length; wire-crossing minimization remains open (same second half as
  clean's tidy story).

- ~~Rewrite-copy placement: copies land toward the surviving partner their
  principal attaches to (demo-side)~~ (2026-07-13)
- ~~Wire routing aware of shape tangents: beziers leave along perimeter
  normals; arrowheads follow; facing apexes connect near-straight~~ (2026-07-13)
- **Explicit `rot` as content** (`setjson rot`) for posing free-principal
  shaped nodes; a rotate gesture.

- **Wire hit-testing**: click a wire to select it; Delete unlinks. (Hover
  highlight + `hit_wire` landed 2026-07-13; wires are right-click targets
  now too — host hook + built-in "unlink". Click-to-SELECT is the remaining
  piece.)
- **Keyboard vocabulary**: Ctrl+A select-all, Ctrl+D duplicate (`rune dup`),
  F2 rename (`rune rename`), arrow-key nudge (batched move).
- **Reroute nodes** (Blender): a pass-through node glyph convention for tidy
  routing.
- **Frames/groups** (Blender): a visual container node type; move-as-group.
- **Tag chips in canvas**: click a tag badge to select-by-tag
  (`vc_tag_match` at render time); tag filter box in the header;
  dim/hide non-matching nodes.
- ~~**Tag editing UI**: inspector chips — click a chip's × to remove, the
  "+ tag…" input adds (`compile_tag` → `tag <ref> +x`/`-x`, logged,
  undoable); any tag edits the same way, pigments included~~ (2026-07-13).
  *Completion from the mantle's existing tags (`axes`) still open.*
- **Search/jump**: find node by name (`find`), center camera on it.
- **Minimap** (LiteGraph): corner overview with camera rect.
- **Auto-layout command**: re-layout placeless AND placed nodes on demand
  (batch of setjson pos — undoable as one frame).
- **Wire shapes**: orthogonal/bezier toggle; avoid-node routing (someday).
- **Multi-inlet spare slots** (VLS): inputs that grow a new slot when wired.
  *[vls]-seconded 2026-07-16 (`mix.in`, `sequence.arr`) — wanted by their
  milestones 2–3; will be a DAW patch's most-touched gesture.*
- **Snap-to-grid** (view preference; positions still model content).
- **Duplicate-with-wires**; copy/paste as command sequences (paste = replay).
- **Per-mantle camera** (`config set view.camera.<mantle>`).
- **Selection logging** (pending Q#3 ruling — config tier if wanted).
- **Micro-undo** (lasagna bottom layer, per-application opt-in).
- **Node tooltips**: facets (who/what/why…) on hover — the six facets are
  model content nobody renders yet.
- **Error toasts**: failed commands (res.ok=false) surfaced visibly, not just
  in the log.
- **`place` verb adoption** the day upstream lands it (gesture compiler
  retargets; positions leave the undo slice).
- ~~Light theme: `CanvasTheme::light()/dark()` presets, auto-contrast header
  text, theme-safe widget colors; examples default light (author preference)~~
  (2026-07-11). HiDPI/font scaling still open.
- **A second view: the TABLE** — the projection-architecture test, made real
  by its client (see [vh] below): one mantle's runes as sortable, filterable
  rows compiling selections/edits to the same dispatcher commands as the
  canvas. (Was "tree/outline"; Void Hormiga's data manager wants a table.)

*Void Hormiga's founding message (2026-07-15) — items marked **[vh]** carry a
second client's weight. Face widgets §3.1–3.3 landed same-day; the rest
queue here (existing items it also leans on: tag chips/filter box, tag
completion from `axes`, search/jump, error toasts — all above):*

- ~~**[vh] Multiline text face widget** — `face_text_multiline`: staged
  (ImGui owns the active buffer; re-projection never yanks it), ONE `set` on
  blur/Ctrl+Enter, Escape reverts, disabled-when-wired~~ (2026-07-15)
- ~~**[vh] Image face widget** — `face_image`: host-provided texture drawn
  aspect-fit, placeholder while loading, click returns to the host (it
  compiles the command — an image click is domain semantics). Host owns file
  I/O/decode/texture lifetime; library owns layout/draw/hit-test~~
  (2026-07-15). *The sanctioned-pattern write-up (texture handoff, DPI)
  remains open as documentation.*
- ~~**[vh] Date face widget** — `face_date`: three staged drag-fields, ONE
  `set` of a clamped ISO-8601 string per completed drag (leap-aware)~~
  (2026-07-15). *A calendar-popup variant is possible polish.*
- **[vh] Bulk select → one batch** — select N rows/nodes → one tag/set/rm
  batch = one undo frame (the group-move shape, generalized: a
  `compile_tags`/`compile_sets` family over the selection). Scenario: tag 50
  events `+month:june` at once.
- **[vh] Table view as a library projection** — see "A second view" above;
  lean per the message: library-worthy (it renders mantle state and compiles
  to dispatcher commands — a projection, not app chrome).
- ~~**[vh] The widget protocol (Phase 4's deferred engineering)** —
  `voidmaiz/widget.hpp`: `WidgetContext` (projection in, commands out, filter,
  width), the FIELD EDITOR as the registrable unit (`hints.editors` on the
  glyph → `SceneField.editor` → `WidgetRegistry` kind→renderer), the default
  kit (text/number/multiline/combo/date — ONE implementation, face widgets
  now thin frames over it), `widget_form` (the detail-pane building block),
  and the one filter bag (`filter_bag`/`node_matches`, project.hpp). The
  inspector renders through the registry — a host-registered editor appears
  there with zero inspector changes~~ (2026-07-16, draft status — Hormiga is
  the forcing client; sharp edges welcome). *Q12 (wrapped-toolkit adapter
  sanction) open with the author.*
- ~~**[vh] Widget-kit bruises (Hormiga's third message)** — the four edges the
  Data section hit, all closed 2026-07-18: **per-field labels** (`hints.labels`
  → `SceneField.label`, threaded through the kit, `"label##key"` IDs);
  **`path`/file kind** (`widget_field_path` + `WidgetRegistry::add_path` — the
  library owns the box, the host owns the OS dialog via `PathBrowseFn`);
  **bool/checkbox kind** (`widget_field_bool`, atomic `setjson true|false`,
  registered `"bool"`/`"checkbox"`); **multi-field batch commit helper**
  (`compile_commit` — none/one/many → one undo frame)~~ (2026-07-18). *Number
  formatting/units args still open — Hormiga hasn't needed them.*
- ~~**[vh] Palette categories** — `AddPalette::Entry.category` (convention: a
  `"category"` key in glyph hints, copied by the host); the add box AND the
  right-click add menu group under headers, in first-appearance order,
  uncategorized first; flat lists stay flat~~ (2026-07-16)
- **[vh] Palette PANEL + drag-from-panel** — accepted as a library gesture
  (2026-07-16, Hormiga §3: the Scratch idiom volunteers know): a persistent
  categorized panel widget; a drag that ends on the canvas compiles the same
  `rune new` + place (+ snap, for blocks) batch a canvas add does. Needs the
  cross-panel drag plumbing (ImGui drag-drop payload → an `edit_canvas` drop
  target) and a snap path for not-yet-minted nodes; queued behind the table.
- ~~**[vh] Workspace/pane rung (Q11's second host)**~~ — DECIDED 2026-07-20:
  the author overrode the proposed "no docking framework" ruling and enabled
  ImGui docking (Hormiga's fourth message: four heavy workflows want
  FL-Studio-style windows). Re-vendored `v1.92.1-docking` (DockSpace only, no
  multi-viewport); shipped `enable_docking` + `begin_dockspace`/`end_dockspace`
  (widgets.hpp); `canvas_window` example converted; all siblings rebuild clean.
  The splitter primitive stays for hand-split panes.
- ~~**[vh] Geographic camera (Territory map)**~~ — DONE 2026-07-20 (Hormiga's
  fourth message §1): `compile_camera(cam, config_key)` lets a second view
  persist its own viewport (`view.map.camera`) at `%.7g` precision; a
  geographic viewport IS the substrate-shaped `Camera` (x=lon,y=lat,zoom). The
  map VIEW (lat/lon projection, tiles, markers) stays host-side against the
  views-as-projections seam — no geo type in the core. Ruled 2D-in-nature
  (stays Void Maiz; a 3D map would be XR).
- ~~**[vh] Canvas actions as first-class commands** (draft)~~ — 2026-07-21
  (Hormiga's fifth message §1): `voidmaiz/action.hpp` — `ActionDescriptor`
  `{name, params[], gesture, compile}` + `ActionRegistry` (`run` = the one
  entry point a gesture AND a CLI verb call; `manifest()` = the introspection
  an agent reads). UI-free, base library, [canvas-actions](/concepts/canvas-actions.md).
  *Draft — Hormiga is the forcing client; open: whether the view wants routing
  help, the param `type` vocabulary.* The Core-side (§2/§3: host-registered
  verbs + `ls`/Scry query predicates) is relayed in `MESSAGE_FOR_VOIDCORE.md`.
- **[vh] One definition → two front-ends** — the CLI-verb front-end for a canvas
  action. **Core answered 2026-07-21** (blessed as **verb macros**,
  compile-to-`batch`): semantics available TODAY via a host command bar calling
  `batch` with the compiled lines (the real spine, not a fake) — the delta is
  verb-table *registration* (discoverability/composability/parse-once), which
  lands on Core's side when Hormiga's map forces it. Rule: verb macros are
  pure→`batch`, never the effect seam. Design: the action param-schema doubles
  as the verb arg-spec (one type). Query side: pluggable `where`-predicates,
  `effect query` the interim. See `MESSAGE_FOR_VOIDMAIZ_core-reply-2026-07-21.md`.

*VLS-native's first message (2026-07-16, the visual-identity batch) — items
marked **[vls]**:*

- ~~**[vls] Port style registry** — `CanvasStyle::port_types`: host type →
  `PortStyle{color, shape}` (circle/square/diamond/triangle/ring — shape as
  the second channel; color must never be the only signal). One
  `draw_port_marker` serves every body kind (window rows, shaped perimeter
  anchors, block sockets); the port hover ring takes the declared color too.
  Undeclared types keep the hashed circle~~ (2026-07-16)
- ~~**[vls] Typed wire styling** — drawn linguine take the from-port type's
  declared color (the to-port's when the from end is an untyped principal);
  the pending drag wire reads as its type in open space, verdict tint at a
  candidate port; theme fallback everywhere else~~ (2026-07-16). *Per-type
  width/dash: unasked niceties, still open.*
- ~~**[vls] `p:<field>` params-as-ports in the kit's wired check** — both
  spellings match everywhere the kit disables-when-wired~~ (2026-07-16).
  *The `hints.bind` alternative (glyph names the port owning a field) stays
  unbuilt unless a client needs a third spelling.*
- ~~**[vls] `face_knob` absorbed** — `widget_field_knob` + the
  `"knob:min,max,default,snap"` editor kind + the `face_knob` face frame;
  kit adaptations: commit via commit_value (string-typed fields stay
  strings), colors derived from the ImGui style (SliderGrab accent) so the
  knob wears the host's theme~~ (2026-07-16). *VLS keeps its local
  `vls::face_knob` (calls qualified same-day to resolve the ADL ambiguity)
  until its agent adopts the kit's.*

## Cross-platform and wire routing (added 2026-09-08)

- **T2 — `Orthogonal` wire routing** (Void Mago's §2.3, filed as Q27). Elbow
  wires — leave downward, run horizontally, enter from above — which a build DAG
  reads better than any curve. Blocked on the placement question, not the code:
  routing is a property of a graph's MEANING, so it may belong per-glyph in
  `presentations.canvas` rather than per-canvas on `CanvasStyle`. Needs a client
  with two graph shapes in one canvas.
- **T2 — a consistent perpendicular bow for loose wires** (Mago §2.2). Two
  nodes with several relationships currently draw as overlapping straight runs;
  a signed offset per wire would fan them. Mago has no such pair today, but a
  family graph with `requires` AND a seam between one pair is exactly the case
  they already report as "described twice". Note the ordering: this is the first
  thing that would make a loose wire a CURVE, and the tangents it needs were
  dead-and-wrong until 2026-09-08.
- **T3 — witness a GUI binary on Linux and on macOS.** CI proves the three
  desktops compile and that the headless suite passes; no runner has a display,
  so `void.json`'s `platforms` array stays two long until a person watches the
  canvas draw on a third. This is the only thing standing between Void Hormiga
  and a non-Windows release, and it needs a machine rather than a commit — the
  same second-computer constraint as their phase F exit test.

## What Void Core 0.2.14 opened (added 2026-09-03)

Adoption itself is done (see [rune kinds](/concepts/rune-kinds.md)); these are
the things that only became *possible* because kinds, quantities and travelling
declarations exist. Ordered by how much each one is already half-built here.

- **T2 — the act-rune role affordance.** An `act` rune is a verb reified as a
  node with typed ports for its ROLES, which is an interaction-net agent — the
  thing this canvas already draws. What is missing is that a role is not a
  dataflow port: "agent", "patient", "path" want naming, ordering and a
  drop-verdict of their own, and today they arrive as generic aux ports through
  `presentations.canvas.ports`. That is the wrong home, because a role is
  **schema** (true in every modality) and port hints are presentation. Blocked
  on an upstream answer, asked 2026-09-03 — see Q26.
- **T2 — dimensional checking at wire-drag time.** The canvas already computes
  a verdict tint while a wire is in flight (`wire_ok`/`wire_bad`/`wire_neutral`)
  from port TYPES. A wire landing on a measure rune can now be checked against
  something stronger: the measure's `level` (dropping a value onto an `ordinal`
  dimension is a category error) and its `min`/`max`. Costs nothing new — the
  machinery, the projection and the annotation are all present.
- **T2 — direct manipulation of an assertion.** If a weight is a value, then
  dragging the label on an assertion wire should compile
  `link <of> <measure> --weight <n>` — one command, staged like every other
  gesture, undoable like every other edit. This is the first gesture in the
  library whose subject is an EDGE rather than a node, which is why it is worth
  writing down before it is written.
- **T2 — a measure view (the second first-party projection).** `values
  --measure speed` answers "everything with a speed" *structurally*, which no
  field scan ever did. [Views are projections](/concepts/views-as-projections.md)
  and the table view was always going to be the proof; a measure lane — one
  dimension, every rune asserting it, sorted, in units — is a better one,
  because it is a view that could not have been built before 0.2.14.
- **T2 — unit suffixes on field editors.** `SceneField::quantity.unit` is
  projected and unused. Every numeric editor in the kit should show it.
- **T3 — declared glyphs make a bundle self-describing, which unblocks two
  things at once.** [Surface census](/concepts/surface-census.md) harvests a GUI
  into concept runes, and a census bundle previously carried its runes but not
  their meaning — open it anywhere but the harvesting host and the fields were
  present and unreachable. `glyph declare` fixes that with no census change.
  Bigger: an **authoring tool** (Allmusely, [horizons](/horizons.md)) needs a
  user to be able to invent a node TYPE at runtime, and until today a new type
  meant host code. A declaration is an ordinary command — logged, journaled,
  mergeable and **undoable** — which is precisely the property that authoring
  tool's whole premise rests on.
- **T3 — `presentations` is the multi-substrate seam we said we would need.**
  [Substrates](/concepts/substrates.md) promised Void Maiz XR and NE would
  render the same mantle without the model knowing which. A per-modality
  presentation map is exactly that home: `canvas` is ours, `xr` and `ne` are
  reserved by the same convention, and none of them is the schema. Nothing to
  build now; a great deal not to build wrongly later.

# T3 — someday / client-driven

*(see [substrates & dimensions](/concepts/substrates.md) for the axes and
per-target analysis)*

- **Node Blocks demo** (desktop): Scratch-like on Void Maiz — **ACTIVE** target
  (chosen 2026-07-13), concept written ([node-blocks](/concepts/node-blocks.md)),
  Blockly connection model studied. Structural-first + net-native, turtle
  domain. Staged:
  - ~~*Phase A* — the two new **library** capabilities: snap-to-connect
    gesture+compiler (`find_snap`/`compile_block_release`: proximity drop →
    aligned moves + link + occupancy unlinks + tear/heal/splice, ONE batch)
    and the adjacency/hidden-wire render mode (`Style::Adjacency` on the
    linguine, from the `"render":"adjacency"` port hint); the `"block"` shape
    kind (notch/tab silhouette, name-based prev/next connectors); C-block
    body = subgraph/mantle-enter; grab-the-stack staging; stack re-flow
    (`compile_stack_layout`). Demo founded (`../NodeBlocks`): turtle glyphs,
    inline face args, the Stage turtle-preview face (host compute)~~
    (2026-07-15). *Remaining Phase A polish: the snap-build exit test
    (transcript replay of snap gestures), inline C-block body rendering
    (view capability, see concept).*
  - *Phase B (later)* — run = reduction: a program-cursor forms a fettuccine
    active pair with the current block; `step` runs + advances, `undo` un-runs
    (reuses `maiz::reduce`/`active_pairs`/`step`). Rules + turtle effects are
    demo config on a host effect sink.
  - *Sub-questions*: `check_wire` single-type vs Blockly array-intersection
    stays open (Void Hormiga will report if a polymorphic socket appears);
    the rest resolved 2026-07-15 (see concept).
- ~~**Touch gesture set** (pinch zoom, long-press menus) — the item that sat
  under "2D node playground APK" since the start. Landed 2026-09-13 as a
  LIBRARY layer rather than a demo's: `maiz::TouchRecognizer` (UI-free,
  81 assertions, no window) + `camera_pan`/`camera_pinch`. Long-press
  synthesizes a clean right-CLICK, so every existing context menu works on
  glass with no canvas change~~ (2026-09-13, [touch](/concepts/touch.md)).
- **InteractionCombinators VR** (Quest 3, OpenXR): → **Void Maiz XR's
  charter** (separate library per Q#12; consumes voidmaiz+voidmaiz_reduce,
  replaces voidmaiz_view) — solids (cone = 3D triangle, sphere = ε), surface
  port anchors, quaternion auto-orientation, ray + grab gestures, **haptic
  feedback sink** (gesture events → host channel, symmetric with the log
  sink).
- **Animation/tween layer** — device-modular motion (node births, wire
  connects, rewrite transitions); design when two substrates need it.
- **Web adoption (no client)** — protocol-level Void Maiz (JS view over the
  /state+dispatcher seam, VLS editor.html pattern) or WASM; pin conventions as
  conformance cases if a web implementation ever appears. *Was "Web/Hormiga
  adoption": Hormiga's path reversed 2026-07-15 — **Void Hormiga** arrives as
  a native C++20 client instead (see scope-and-clients.md); this stays only
  as a general option with no client pulling it.*
- **Non-euclidean camera** (research): hyperbolic focus+context for large
  graphs — H3/Munzner is the map.
- **Game of Life stress demo** (Void Core primary; author brainstorm
  2026-07-13): a 100×100 cell grid as runes, generation ticks as app-computed
  `batch`es of `set` (compute boundary — NOT maiz::reduce: a synchronous CA is
  the opposite model from asynchronous net rewriting), **undo = time-reversal
  of an irreversible CA** (the log's party trick), adjacency implicit
  (position IS the wiring — the extreme of the Blocks axis). Void Maiz angle:
  a **raster/LOD view** (node = pixel at far zoom, inspectable graph up
  close) — the projection-architecture test, and the 10⁴-rune client that
  would justify the upstream diff-seam ask with numbers.

- Vulkan backend behind the renderer seam.
- Timeline/arrangement view (VLS-native will want it).
- Input holidays (MIDI controllers emitting dispatcher commands).
- Centrality/analytics-driven visuals (Scry hooks → widget responsiveness).
- Qt widget adapter for the registry — parked behind Q12 (the author's
  wrapped-toolkit sanction; the 2026-07-16 protocol draft is deliberately
  adapter-compatible but ships no bridging).
- Branch/merge of histories (upstream research).
- The "Figma/Qt-Designer on Void Maiz" host application.

# The Blender / LiteGraph matrix (Q#7 answered)

| feature | Blender | LiteGraph | verdict |
|---|---|---|---|
| Add-search at cursor (Shift+A) | ✓ | ✓ (double-click) | **T1, done** — both gestures worth supporting eventually |
| Socket type colors + validation | ✓ | partial | **T1, done** (hints types; check_wire) |
| Collapse to header | ✓ (H) | ✓ (dot) | **T1, done** (dot; H key later) |
| Node groups (subgraph as node) | ✓ | ✓ | **T1, done** as mantle-enter convention; group-*creation* gesture (select → group) is T2 |
| Frames (visual containers) | ✓ | ✗ | T2 |
| Reroute nodes | ✓ | ✓ | T2 |
| Mute/disable node (M) | ✓ | ✓ | T2 — host semantics (a tag?), UI affordance cheap |
| Wire cut gesture (Ctrl-drag) | ✓ | ✗ | T2 (Delete-on-selected-wire first) |
| Minimap | ✗ | ✓ | T2 |
| Live wire preview colors | ✓ | ✓ | **done** (verdict tint) |
| Properties panel (N) | ✓ | ✓ | **T1, done** (inspector) |
| Multi-window/split views | ✓ | ✗ | T3 (host's job — it owns windows) |
| Custom node body drawing | ✓ (limited) | ✓ | **T1 in progress** (face API) |
| Widgets on nodes | ✓ | ✓ | **T1** (face widgets, staging discipline) |
| Keyboard-first operators | ✓✓ | partial | T2 (vocabulary above) |
| Undo that just works | ✓ | flaky | **done, better** — the core's undo is the model's, one frame per gesture |
| What neither has: total observability, agent parity, replayable transcripts, tags as addressing | — | — | **the point of Void Maiz** |

Universally-useful = adopted above; application-specific (Blender's material
previews, LiteGraph's built-in execution) stays out — execution especially
(the compute boundary; a UI library that executes graphs has eaten its host).
