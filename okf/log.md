---
type: Log
title: Log
description: The running history of Void Node decisions and work, newest last.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-09T00:00:00Z
---

## 2026-07-09 — Founding: the project exists, OKF-first

Born from Void Loops Studio's native-era call (VLS `okf/log.md`, same date): the
author confirmed **C++20**, then went further — the node UI layer should be a
**separate, generalizable library**: *"we'll call this Void Node."* Founding
requirements, verbatim in spirit: completely compatible with Void Core; the CLI
expanded until *everything* is logged (node movements included); widgets-as-nodes
as a feature; most future UI-bearing Void Core apps build on it (FaultSack-class
and ESP32-class exempt); production order = recreate the existing VLS capability
better, piece by piece; own OKF; concept before code; eventually its own agent.

**The rethink of the pre-Void-Core draft is the founding intellectual work.** An
outside AI drafted a concept document without knowing Void Core; its "radical
log-first architecture" turned out to describe… Void Core. The mapping (what the
core already provides vs what's genuinely Void Node's) is
[log-first inheritance](/concepts/log-first-inheritance.md); the draft is
[archived with critique](/references/voidnode-draft-v1.md). Cut in the rethink:
the execution engine (violates the compute boundary — applications compute),
BaaS/cloud tiers (host holidays), git-like branch/merge (upstream research),
compilation targets (application-side). Kept and sharpened: views as projections,
total observability (now two-tiered), devices as input holidays, agents as
first-class dispatcher users.

**Void Core C++ readiness was checked** (the author's ask) — verdict in
`MESSAGE_FOR_VOIDCORE.md`: the C ABI is C++-ready by design (`extern "C"` header,
self-contained dll, CMake, CI prebuilts; threading contract documented). The one
real gap for the native VLS: **the interaction-net reducer lives at the Python
seam** (`reduce/`), so a C++ host has no Reduce today. Asks filed: a C-ABI reduce
(or a portable spec for hosts), glyph host-callback bridge timing, attribution in
the log spine, and the view-state undo-slice question.

Written on founding day: [index](/index.md), the four load-bearing concepts
([log-first inheritance](/concepts/log-first-inheritance.md),
[total observability](/concepts/total-observability.md),
[views as projections](/concepts/views-as-projections.md),
[differentiation](/concepts/differentiation.md),
[scope & clients](/concepts/scope-and-clients.md)), the [roadmap](/roadmap.md)
(phases 0–5, VLS-parity-gated), the founding
[developer questions](/developer_questions.md) (6 open, with leans), README,
CLAUDE.md (for the future dedicated agent), and the upstream message. **No C++
was written, deliberately** — Phase 0 is concept; Phase 1 waits on the founding
questions and the upstream reduce answer.

## 2026-07-10 — Phase 0 exits; the direction summary lands; Phase 1 built and green

**Two inputs resolved the founding gates in one day.** The author's direction
summary (`promptToStart.md`) answered founding questions 1/2/4/5 (GLFW+OpenGL 3
with a renderer abstraction seam; the modular repo shape as proposed; keep the
gesture vocabulary but favor Blender's UX; static library) and set new
direction: **interaction nets are the mathematical foundation**, with two wire
kinds — *linguine* (auxiliary, directed, fan-out, typed) and *fettuccine*
(principal-to-principal interaction wires, symmetric, one-to-one) — captured in
the new [interaction connections](/concepts/interaction-connections.md) concept
("ravioli" for nodes was evaluated and dropped; wire kinds get names, nodes
don't need one). The **Rule Workshop is explicitly out of scope** (port-mapping
protocol yes, mapping UI no — hosts build their own). The **widget strategy**
became its own concept ([widget registry](/concepts/widget-registry.md)):
node-specific widgets + a registry/protocol whose contract is CLI
representation, tag awareness, undo participation, responsiveness hooks. The
undo layering got its name — **the lasagna model** — folded into
[total observability](/concepts/total-observability.md). The **first client
changed**: an InteractionCombinators demo (γ/δ/ε, all six Lafont rules, a
dataflow patch alongside) precedes VLS-native as the exit-test vehicle for
phases 2–4 ([roadmap](/roadmap.md) updated; scope = open question #8). New open
questions #7–#9 (feature matrix, demo scope, API depth of net concepts) filed.

**Upstream's reply (`MESSAGE_FOR_VOIDNODE.md`) closed the founding asks**: the
reduce gap is now a portable contract (`VoidCore/conformance/reduce/` — README +
10 pinned cases + a ~100-line runner we port in the demo phase); attribution is
implemented (0.2.3, `config set actor`; we standardized the `kind:name`
convention); the `place`-verb direction for view state is endorsed in our
questions doc (interim: positions-as-content, view writes kept distinguishable).
Concepts trued up: [log-first inheritance](/concepts/log-first-inheritance.md)
(attribution + reduce rows), total observability (attribution implemented;
lasagna tiers).

**Phase 1 shipped and is green.** The repo skeleton (CMake + Ninja, mirroring
the core's build; cJSON vendored byte-identical from the core, licenses noted
in `vendor/README.md`) and the embed layer: `vn::Core`
([include/voidnode/embed.hpp](/../include/voidnode/embed.hpp),
`src/embed/embed.cpp`) — RAII manager lifetime, `{ok,lines,data}` parsed into
`vn::Result`, glyph registration, log-sink/effect-handler as `std::function`
(stable-address trampolines, movable), stateless `tag_match`, and a `raw()`
escape hatch. The smoke test (`tests/embed_smoke.cpp`, ctest `embed_smoke`)
builds a mantle through dispatch (glyphs, runes, set/tag/link, undo/redo,
attribution via `log --tail`), then **exports the state, replays it into a
fresh Core, and walks its nodes** — the Phase 1 exit test, self-contained
because no saved VLS state document exists on disk yet (noted; a real VLS-state
replay stays desirable once one exists). Found upstream while verifying:
**`vc_version()` hardcodes "0.2.1"** while `VC_VERSION_STR`/the `version` verb
say 0.2.3 — reported in the new `MESSAGE_FOR_VOIDCORE.md`, along with the
`kind:name` adoption, the reduce-port plan, and the `place` endorsement.

**Readiness audit: development is NOT gated on Void Core.** At the author's
prompt ("do we have everything we need?") we audited every verb phases 2–4
depend on, against the implementation: `glyphs` returns **full descriptors
including arbitrary keys** (`vc_glyph_register` preserves them) — so the
render-hints/port-declaration need is a pure **host convention**, no core
change; the proposed `"hints"` descriptor shape (typed ports, at most one
`principal:true`, face size, color, category) is recorded in the upstream
message as FYI. `setjson` exists; `batch` is atomic with one undo frame and
rollback (the drag-coalescing tool); `links` carries link objects in `data`.
The upstream message was reframed accordingly: a bug report + conventions +
two forward notes (the hints shape; a possible far-future diff seam for
10⁴-rune mantles), explicitly **non-blocking** — Phase 2 (projection engine +
first window; ImGui + GLFW get vendored) starts without waiting for a reply.

**Phase 2 kicked off the same day — the substrate is up.** Since the audit
proved nothing gates on upstream, development started immediately (the author's
preference, stated when finalizing the message): **GLFW 3.4** (Zlib) and
**Dear ImGui 1.92.1** (MIT) vendored into `vendor/` with licenses noted —
GLFW built via `add_subdirectory` with examples/tests/docs/install off, ImGui
as a static `voidnode_imgui` target (core + the glfw/opengl3 backend pair,
demo compiled in for dev convenience). `examples/hello_window.cpp` (`vn_hello`)
is the first window: GLFW + OpenGL 3 + ImGui hosting a **live `vn::Core`** —
buttons that ARE dispatches (`rune new`, `undo`, `redo`, `ls`), a log strip
showing the transcript live (total observability in miniature), and the
`"hints"` descriptor convention exercised on its demo glyph. Verified running
(window loop alive, embed smoke still green). Phase 2 proper — the projection
engine (state document → scene graph) and the flat canvas drawing real
boxes/wires — is next; its exit test ("a saved VLS patch renders
recognizably") still needs a saved VLS state document, which remains the one
outstanding want.

## 2026-07-10 (later) — Phase 2 substance: the projection engine and the canvas

The library's first real substance landed, all verified by test and by eye
(screenshot of `vn_canvas` renders the demo patch recognizably):

- **The scene model** (`include/voidnode/scene.hpp`) — pure data, no rendering
  types: nodes (id/name/glyph/label/tags, position + `placed` flag, face size,
  hint color, principal name, aux inputs/outputs), wires with the three kinds
  (**linguine / fettuccine / loose**). Port indexing follows the reduce
  contract verbatim: **0 = principal, 1..n auxiliary**; an edge relation
  `"i:j"` classifies the wire — `"0:0"` = fettuccine (forced symmetric), other
  `"i:j"` = linguine, non-numeric = loose semantic link.
- **The projection engine** (`src/project/project.cpp`, `vn::project_scene`) —
  state document + `glyphs` descriptors → Scene. Reads the `"hints"` convention
  (ports with at most one `principal:true`, face w/h, `#rrggbb` color);
  synthesizes aux anchors for wire-referenced ports glyphs didn't declare
  (adds, never reorders — port order is app knowledge); position convention:
  `rune.placement` outranks `content.pos` (both `{x,y}` and `[x,y]` accepted),
  neither → **depth auto-layout** (longest directed-wire distance, cycle-safe
  relaxation); dangling edges skipped (that's `validate`'s job). Full rebuild
  per call — sub-millisecond at this scale; incremental is a later
  optimization, not a different design.
- **The canvas** (`src/view/canvas.cpp`, `vn::draw_canvas`, new
  `voidnode_view` target so the base library stays ImGui-free) — grid, node
  boxes with glyph-colored headers, principal port as a **diamond on the top
  edge**, typed aux ports as colored dots (stable type→color hash), tag
  badges; wire visuals: linguine thin bezier + arrowhead, fettuccine a
  **thick ribbon arcing between principals**, loose dim center-to-center.
  Draw-only — Phase 3 owns input.
- **Tests**: `project_smoke` (ctest, green) pins hints→ports, wire
  classification, both position conventions and their precedence, auto-layout
  non-overlap, and the **one-sync property** (link → re-project → wire
  appears; undo → re-project → gone). `embed_smoke` still green.
- **Example**: `vn_canvas` builds the demo patch *through the dispatcher only*
  (osc→filter→out chain, γ/δ fettuccine pair, a placed note with a loose
  `annotates` link), renders it with a log strip, and re-projects after every
  button dispatch.

Phase 2's exit test ("a saved VLS patch renders recognizably") remains open
pending a real saved VLS state document — the demo patch stands in
meanwhile. Next: Phase 3 piece 1 — camera pan/zoom + hit-testing/selection,
the first gesture→command compilation.

## 2026-07-10 (later still) — Phase 3 pieces 1–2: the gesture→command compiler is real

The part no other node library has, working end-to-end and verified by a
scripted OS-level mouse drag against the live window: **drag gamma → release →
`setjson gamma pos [270,270]` appears in the log strip → re-projection moves
the node and its fettuccine follows.** The screenshot shows the compiled
command in the mutation spine — the founding commitment, on screen.

- **`EditorState`** (`include/voidnode/editor.hpp`, UI-free) — camera,
  selection, and the transient drag machinery (staged positions, marquee,
  flush timers). The lasagna tiers in one struct: positions are model content;
  **camera flushes to `config set view.camera` on gesture end** (session meta →
  the undo-exempt-but-logged config tier, its designed home; restored on boot
  via `config get`); **selection stays local** pending Q#3 (noted there);
  staged drags and marquee-in-progress are ephemera.
- **The compilers** (`include/voidnode/gesture.hpp`, `src/gesture/gesture.cpp`,
  UI-free and unit-tested): `compile_move` → `setjson <rune> pos [x,y]`
  (rounded once per gesture, no drift), `compile_moves` → ONE `batch '…'` for
  a multi-selection (single-quoted payload — the tokenizer's literal mode),
  `compile_deletes` → `rm`/batch, `compile_camera`/`parse_camera` for the
  config round-trip.
- **`edit_canvas`** (`voidnode_view`) — wheel zoom to cursor (clamped),
  middle/right-drag pan, click/shift-click/marquee selection (additive with
  shift), node drag staging with click-slop, Delete key → rm. It never
  dispatches: gestures compile into `CanvasIO.commands` and the *caller*
  dispatches + re-projects, so a rejected command simply snaps back on the
  next projection — no local truth.
- **Tests**: `gesture_smoke` (green, 3/3 suites now) pins the command strings
  AND drives them through a live core: a two-node batch move undone by ONE
  undo (group drag = one lasagna layer), batch delete/resurrect, camera
  config round-trip surviving an unrelated undo.
- `vn_canvas` is now interactive; hint line in the header, selection/zoom
  readout, camera restored from config on boot.

Verification note for future sessions: scripted input against the window must
be **DPI-aware** (`SetProcessDPIAware`) — ImGui coordinates are physical
pixels; the first drag attempt missed by the 1.5× virtualization factor.

Remaining in Phase 3: wire drag with type checking (piece 3), the add-search
box (piece 4), field editing/inspector (5), chrome + subgraphs (6), and the
in-app command bar + undo/redo surfacing (7).

## 2026-07-10 (evening) — Phase 3 pieces 3–4: wire dragging with type checking; the add box

Both verified by scripted OS-level input against the live window; the log
strip screenshot shows the compiled commands:
`batch '["unlink lowpass speakers --relation 3:1","link carrier speakers --relation 2:1"]'`
(a drop on an occupied input = a one-frame rewire) and
`batch '["rune new osc osc-1","setjson osc-1 pos [732,375]"]'` (Shift+A).

- **The type checker** (`vn::check_wire`, UI-free, matrix-tested): same node →
  reject; principal↔principal → fettuccine; principal↔aux → reject (legal in
  nets, not draggable in v1); aux↔aux → linguine iff output meets input and
  types match, empty type = wildcard.
- **Compilers**: `compile_link` (the adapter's `i:j` relation; `0:0` gets
  `--undirected`), `compile_unlink`, `compile_rewire` (unlinks + link as ONE
  batch — a rewire is one lasagna layer), `compile_add`
  (`rune new` + `setjson pos` batch), `unique_name`.
- **The drag grammar** in `edit_canvas`: port hit-testing (screen-space
  radius, checked before node bodies); dragging from an output always starts a
  new wire (fan-out); **grabbing an occupied input or a principal with a
  standing fettuccine DETACHES the wire for re-routing** — drop it elsewhere
  to rewire (batch), on nothing to unlink, back home for a no-op. Pending
  wire renders verdict-tinted (green = valid target, red = reject) and snaps
  to the candidate port. 1:1 fettuccine is enforced by the rewire batch
  clearing standing fettuccine on both principals.
- **The add box**: Shift+A at the cursor; filterable palette
  (`vn::AddPalette`, host-supplied — glyphs are host config); Enter picks the
  first match; the new rune lands at the cursor's world position.
- Tests: verdict matrix, all compiler strings, and a live-core rewire batch
  undone by ONE undo (3/3 suites green).
- Nice emergent behavior observed: rewiring re-flows auto-laid nodes (speakers
  moved columns when its feeder changed) — placeless positions follow the
  graph, placed ones stay put. Exactly the projection contract.

Remaining in Phase 3: field editing/inspector (5), collapse/maximize chrome +
subgraph enter/exit (6), the command bar + undo/redo surfacing + log strip as
a library piece (7).

## 2026-07-10 (night) — Phase 3 piece 5: field editing via the inspector

Verified by scripted input against the live window: click gamma → click its
`kind` field → select-all, type `epsilon`, Enter → the log strip shows
**`set gamma kind 'epsilon'`** (exactly one command for the whole edit) and
the field then displays the *re-projected* value — the commit round-trips
through the model, never the widget's buffer.

- **Scene carries fields now**: `SceneField {key, value_json, is_string}` on
  every node — the glyph's declared `fields` (the editability registry, SPEC
  §3.3) valued from content, in declaration order. Undeclared content keys
  (like our `pos` convention) deliberately never surface.
- **Compilers**: `compile_set` (plain text, single-quoted with `\'` escaping —
  any text is safe) and `compile_setjson` (non-string values edit as raw
  JSON; an invalid payload is rejected by the core and visibly logged).
- **`draw_inspector`** (`voidnode_view`): selection header (name, label,
  tags), one InputText per field with the **VLS #14b staging discipline** —
  activation snapshots the projected value into the editor's buffer, the
  active buffer wins over re-projection mid-edit, Enter/defocus commits ONE
  command iff the value changed, Escape abandons silently. Multi-selection
  shows the list; field editing is single-target for now.
- Tests: field projection pinned in `project_smoke` (declared-only, ordered,
  valued); compiler quoting incl. embedded quotes pinned in `gesture_smoke`
  plus a live-core round-trip of a quote-bearing value. 3/3 suites green.
- Scripted-input note: WScript.Shell `SendKeys` is the reliable way to type
  into the window (raw `keybd_event` without scancodes may not produce
  chars); and re-measure widget y-offsets from a screenshot before clicking —
  the first two attempts missed the input box by ~7px.

Remaining in Phase 3: collapse/maximize chrome + subgraph enter/exit (6), the
command bar + undo/redo surfacing + log strip as library widgets (7).

## 2026-07-11 — Phase 3 piece 6: node chrome (collapse, resize) + subgraph enter/exit

All four gestures verified by scripted input with screenshots: the collapse
dot compiles **`setjson gamma collapsed true`** (node folds to header-only
chrome, filled dot, wires gather at edge midpoints); the resize grip compiles
**`setjson speakers size [236,110]`** on release (staged during the drag, one
command, exact to the pixel); double-clicking a node with an enter hint
compiles **`use inside-gamma`** (the canvas re-projects the submantle);
double-clicking empty canvas pops the editor's mantle stack and compiles
`use canvas-demo` back.

- **Conventions**: collapse and size ride content (`collapsed` bool,
  `size [w,h]`) — the same view-state-as-content tier as `pos`, all
  `place`-retargetable later. Subgraph entry is the hints convention
  generalizing VLS's Loop pattern: `hints.enter` names the content field
  holding a mantle name (`"enter":"target"`); the projection surfaces it as
  `SceneNode.enter_mantle` and the canvas shows a `>mantle` marker. The
  mantle stack is view state (the core's mantle list is flat — nesting is
  this editor's reading of it).
- **Chrome rendering**: dot (ring = expanded, filled = collapsed), resize
  grip triangle, collapsed = header-only with aux anchors at edge midpoints;
  content.size overrides hints.face.
- Tests: chrome compilers + projection of collapsed/size/enter pinned; 3/3
  suites green.
- **Upstream observation** (added to MESSAGE_FOR_VOIDCORE.md): `use` is not
  on the mutation spine — navigation produces no log line. Fine for now
  (host-side logging is possible); flagged as a candidate "navigation tier"
  (logged, not undoable) if the spine ever grows one.
- Scripted-input lesson repeated and now written down properly: measure the
  canvas origin from a screenshot before aiming at small targets — it is
  client (8, 24) in the current example layout, and a 10px error selects the
  node instead of hitting its collapse dot.

Remaining in Phase 3: piece 7 — the command bar (the CLI inside the UI),
undo/redo surfacing, and the log strip as a reusable library widget. Then the
Phase 3 exit test: rebuild the starter patch mouse-only and replay the
transcript to an identical state document.

## 2026-07-11 (cont.) — The backlog; Phase 3 complete; Phase 4 opens with faces

**The backlog exists** ([backlog.md](/backlog.md)), per the author's direction:
*long lists, knocked out one by one, maybe only the necessary ones.* Tiered
T1 (demo-necessary) / T2 (wanted) / T3 (someday), done items struck with
dates. It also answers open question #7: the **Blender/LiteGraph feature
matrix** is in the same file — universally-useful features adopted (most
already built), application-specific ones (execution above all) explicitly
kept out.

**Phase 3 piece 7 landed — Phase 3's interaction grammar is complete.**
`vn::draw_command_bar` (the CLI inside the UI: Enter dispatches, focus stays,
Up/Down history) and `vn::draw_log_strip` (level-colored, `>` echo entries,
pinned-to-tail) are library widgets now; Ctrl+Z/Ctrl+Shift+Z/Ctrl+Y ride the
canvas. Verified by scripted input: typed `tag delta +marked` → spine line +
blue echo with result; Ctrl+Z → `undo: undid 1 change(s)`, badge reverted.
(The Phase 3 *exit test* — mouse-only patch rebuild + transcript replay —
stays open in the backlog as an automatable end-to-end test.)

**Phase 4 opened: the face API** (`include/voidnode/face.hpp`,
`voidnode_view`): hosts register `FaceRenderer`s per glyph in a
`vn::FaceRegistry`; `edit_canvas` draws them as real ImGui widgets inside the
node body (below ports, above tags — hosts size via hints.face), with
`SetNextItemAllowOverlap` on the canvas surface so widgets take input
precedence. Faces get a `FaceContext` (projected node, area, zoom, command
sink) — the **widget-registry contract's first concrete surface**: commands
out, tags in, undo by construction, responsiveness via re-projection. Default
widgets shipped: `face_drag_number` (staged in ImGui state storage — the
projection never yanks a drag; ONE `set`/`setjson` on release) and
`face_combo`. Verified by scripted drag on the oscillator's freq widget:
**`set lfo freq '5.6'`** — one line for the whole drag, widget re-projects.

Next from the T1 backlog: visible undo depth, face-widget
disabled-when-wired, then the reduce port + active-pair affordance — at which
point the InteractionCombinators demo repo can be founded.

## 2026-07-11 (cont.) — Light theme (author's preference); three more T1/T2 items down

**The author dislikes dark-mode UIs** — theming became real rather than
hardcoded: `vn::CanvasTheme` gathers every canvas color (grid, node bg/border,
selection, wire kinds, verdict tints, marquee, chrome) with `dark()`/`light()`
presets; header text picks black/white automatically by the host glyph
color's luminance, so host hint colors work on both themes untouched; the
log strip's info lines use the ImGui style's own text color (only
error/warn/echo are explicit, chosen to read on both). Examples now default
to **light** with a toggle in the header (preference saved to agent memory).
Verified by screenshot — the light canvas is clean: white bodies, saturated
host headers, dark wires.

Also knocked out, verified in the same screenshot:
- **Visible undo depth** — `history` line count in the example header
  (`undo: 30`), refreshed after every dispatch.
- **Disabled-when-wired face widgets** (VLS #14b) — `FaceContext` now carries
  the whole Scene; `face_drag_number` renders disabled when an input port
  named like the field has a linguine feeding it. Demo: lfo.out → carrier.freq
  (audio-rate FM) — carrier's freq widget grays out while lfo's stays live;
  bonus, the new wire re-flowed carrier downstream in the depth layout.

Backlog updated. Remaining T1: the reduce port (C++20, 10 pinned cases), the
active-pair affordance, the Phase 3 exit test, and founding the
InteractionCombinators demo repo.

## 2026-07-11 (cont.) — The reduce port: 10/10 conformance on the first run

**`vn::reduce` exists and passes the whole contract** — the C++20 port of the
interaction-net executor (`include/voidnode/reduce.hpp`,
`src/reduce/reduce.cpp`, plus `tests/reduce_conformance.cpp` porting
`run.py`). All ten pinned cases green in ctest (`reduce_conformance`, 4/4
suites total): identity, erasure, annihilation cross-linking, the m×n
commutation grid, **confluence over 8 randomized schedules**, opaque
freezing, the termination guard, tag fidelity, the locality rejection, and
the strict `"i:j"` adapter.

Port decisions worth recording:
- **It lives in a separate `voidnode_reduce` static target** — the compute
  boundary holds: the canvas library never links or calls it; it is
  application-side machinery shipped as a reusable component (the demo and
  VLS-native's compile pipeline are the intended consumers). It shares
  `voidnode`'s cJSON objects rather than compiling its own (no duplicate
  symbols).
- **Agent content is stored as pre-normalized canonical JSON text** (sorted
  keys, compact) — copies, equality, and the canonical fingerprint become
  string operations; the contract's "opaque content" rule is upheld by
  construction since we never look inside.
- **Serializer equivalence, not byte-compatibility**: conformance compares
  canonical text, and both the expected values and our results pass through
  OUR serializer, so determinism + sorted keys suffice. One recorded caveat:
  a case whose content held a float like `1.0` could sort differently than
  Python's `json.dumps` (we print integral doubles as integers); no current
  case does — if one ever appears, that becomes a case request upstream.
- **Randomized schedules use std::mt19937, not Python's RNG** — the contract
  requires schedule-independence (convergence), not a specific random
  sequence; reproducing CPython's Mersenne-Twister `choice` would pin the
  wrong thing.
- No ambiguities hit — the README + reference were sufficient; no case
  requests needed yet (upstream asked to hear this either way).

This was the last *infrastructure* item before the demo: with the executor,
the face API, and the full gesture grammar in place, the
InteractionCombinators repo can be founded next — γ/δ/ε glyphs, the six
Lafont rules as a reduce Spec, and the active-pair affordance wiring `step`
to `vn::reduce`.

## 2026-07-11 (cont.) — InteractionCombinators founded: the whole stack, demonstrated

**The showcase app exists and works** (`../InteractionCombinators`: CMake
consuming Void Node via add_subdirectory, `src/main.cpp`, README, launcher
.bat). Verified by scripted runs with screenshots at every stage:

- **The starter net** (10 agents, built entirely through the dispatcher): a
  γδ commute pair feeding two ε erasers and two γ anchors, plus a γγ
  annihilation pair — `active pairs: 2`, both fettuccine glowing with the
  new hot halo.
- **step** fired the γδ commute: dup/con removed, the 2×2 grid minted
  (delta-1/2, gamma-1/2 fanned near the redex midpoint), and the cascade
  appeared exactly as the math predicts — the δ-copies' principals landed on
  the ε erasers → `active pairs: 3`.
- **reduce** reached normal form (`active pairs: 0`) in four batches, the
  last one reading `"rm g1","rm g2","link left right --relation 1:1"` — the
  γγ annihilation cross-link, in the log, as a command.
- **undo × 2 un-rewrote the net**: g1~g2 and delta-2~era2 came back, halos
  hot again. One rewrite = one lasagna layer, provably.

The step mechanism is the design's payoff: export state → `extract_mantle` →
`to_net` → `vn::reduce::step` (pure) → **diff the nets** → compile the
difference into ONE `batch` (rm redex, mint copies renamed from `_rN` to
model names, link the new wiring, all `--undirected` with explicit `i:j`).
Total observability holds through the rewriting engine itself.

Library additions that made it possible: `vn::reduce::active_pairs` +
`vn::reduce::step` exposed (fresh ids continue past existing `_rN` so
re-stepping never collides); `vn::extract_mantle` (state document → mantle
fragment, the seam to the adapter); `SceneWire.active` (host-set post-
projection) with the canvas's hot-halo rendering; `vn::compile_batch` made
public. All 4 VoidNode suites still green; demo builds against Void Node as
a subdirectory with zero changes.

T1 remaining: only the Phase 3 exit test (transcript-replay automation).
Everything else on the demo's list is polish tracked in its own repo now.

## 2026-07-13 — Node geometry: shape is notation

The author, from actually using the demo: interaction combinators are *drawn*
— triangles for γ/δ, circles for ε — and the geometric transformation IS the
UX ("when gamma and delta point to each other and interact, we get 4
triangles facing away from each other, ports connected criss-cross… I
learned to DRAW the stuff"). Rectangular window nodes erase the notation the
user thinks in. New concept:
[node geometry](/concepts/node-geometry.md) — generalizable, not an enum of
pictures:

- **Glyph-declared bodies** via the hints convention:
  `"shape": {"kind":"triangle"|"circle"|"polygon","sides":n,"rot":"auto"|deg}`.
  Window (the chrome rect) stays the default and keeps faces/fields/chrome;
  shaped nodes are compact notation bodies (name centered, tags beneath, no
  chrome).
- **Ports anchor on the perimeter as a function of orientation**: principal
  AT the apex/angle θ; auxiliaries along the opposite feature (a triangle's
  base edge, else the opposite arc) in index order.
- **`rot:"auto"` is the payoff**: the body rotates so its principal points at
  its principal-wire partner (free → points up). Verified in the demo: the
  redex pairs literally point at each other, and after the γδ commute the
  four copies **face away with the criss-cross grid between their bases** —
  the textbook picture emerging from the wiring alone, screenshot-confirmed.
- True-shape hit-testing (point-in-polygon/circle); box-based move/marquee
  unchanged; faces and chrome skip shaped nodes.
- The "does it have to be a shape?" horizon question is recorded in the
  concept: custom bodies would arrive through the face API, not more enum.

Library: `NodeShape` on SceneNode + hints parsing; `node_theta`,
`shaped_anchor`, `draw_shaped_node`, precise hit-testing in the canvas. Demo
glyphs reshaped (γ/δ triangles 96px, ε circles 60px). 4/4 suites green.
T2 follow-ups filed: smarter rewrite-copy placement, explicit `rot` content
posing, shape-tangent wire routing.

## 2026-07-13 (cont.) — Tangent wires, partner-seeking copies, and T1 CLOSED

Three finishing moves, all screenshot-verified in the demo:

- **Tangent-aware wire routing**: bezier control points now leave each anchor
  along its outward tangent — the perimeter normal on shaped bodies, the old
  up/sideways conventions on window nodes — and linguine arrowheads orient
  along the destination tangent. The payoff: **two facing apexes connect by a
  dead-straight fettuccine bar** (the redex looks like a drawn interaction
  wire, not a UI arc), and grid wires leave triangle bases perpendicular.
- **Rewrite-copy placement** (demo): a minted copy lands toward the surviving
  partner its principal attaches to (55% of the way from the redex midpoint),
  fan fallback otherwise. Post-commute the δ-copies sit beside their erasers,
  no overlap; the picture is the textbook drawing.
- **The Phase 3 exit test is green** (`replay_smoke`, 5/5 suites): a
  17-command transcript of exactly the strings the gesture compilers emit —
  add box, link, rewire-batch, group move, field edits, tags, collapse,
  resize, a fettuccine, undo/redo, delete — replays into a fresh core to an
  **identical projected scene** (id-independent fingerprint), and rebuilding
  from the exported state matches too. "The transcript is the session,"
  automated. **With it, the T1 backlog is complete** — everything the
  InteractionCombinators demo needed from Void Node exists and is tested.

Scripted-input note: a first click immediately after SetForegroundWindow can
be swallowed; wake the window with a throwaway click before aiming at small
buttons.

## 2026-07-13 (cont.) — Substrates & dimensions: the planned horizons, captured

The author laid out the futures before continuing: mobile GUI (Android), VR
GUI (Quest 3 — 3D geometry, haptics), and block-language support (Hormiga's
Blocks; "isn't a block language a node graph?" — yes, and Scratch's connector
notches are port types rendered geometrically, node-geometry's conclusion).
New concept: [substrates & dimensions](/concepts/substrates.md) —

- **Assumptions fixed**: a visual host has an OS, compute, and *a* visual
  component; NOT assumed: rectangular display, mouse, 2D, euclidean space.
- **The axes**: dimension / metric / substrate / input / wire-rendering /
  motion — and the rule that nothing above the view module acquires
  dimension- or device-specific types (the 2D code stays quarantined in
  voidnode_view; siblings join it).
- **Four-demo test matrix**: InteractionCombinators (2D-euclidean-desktop,
  shipping), Node Blocks (Scratch-like, hidden wires, snap=link), 2D
  playground APK (touch), IC-VR (cone = 3D triangle, feedback sink for
  haptics).
- **Language boundary walked through**: Android AND Quest are NDK C++20 (the
  rule survives; shims are zero-logic scaffolding); the web is the one true
  boundary → new Q#10 (lean: protocol-level adoption à la VLS editor.html,
  conventions pinned as conformance cases when implementation #2 exists).
  New Q#11: 3D = extend Scene when the first 3D client starts, never
  speculatively.
- **References found** (the author works with maps, not blind): Munzner's H3
  (3D hyperbolic graph layout, Focus+Context), Lamping/Rao's hyperbolic
  browser, recent SUI/VR node-link interaction studies (filter-plane beats
  ray selection on dense graphs; egocentric navigation wins), Google Blockly
  as the block-connection-model reference. Links in the concept.

No code this session-slice, deliberately: this was scope-fixing so the next
implementations don't hardcode what these futures forbid.

## 2026-07-13 (cont.) — Horizons: the library family, time, and Latin-OS

Brainstorm session with the author, captured in the new
[horizons](/horizons.md) registry (the "what Void Node will eventually be
used for" file, complementing backlog and substrates):

- **The library family is decided** (Q#12 confirmed and extended): **Void
  Node** stays *2D in nature* — 2D euclidean interaction space on
  rectangular displays, but free to use 3D *graphics* (nodes as 3D buttons,
  depth) the way modern 2D Mario is 3D-rendered: rendering dimension ≠
  interaction dimension. **Void Node XR** = visualizing Void Core in three
  interaction dimensions (separate library, repo when work starts). **Void
  Node NE** = non-euclidean spaces, a third sibling, deliberately
  back-burnered. All consume the same substrate-free foundation; the OKF
  conventions are the normative spec.
- **Time as the fourth axis, given its names**: temporal graphs / dynamic
  network visualization / graph rewriting traces. Key observation recorded:
  the dispatcher log already IS the time axis (undo = scrubbing; the demo
  walks reductions backwards today). "Dynamic nodes" decomposes into history
  visualization (view work), live agents dispatching (already possible), and
  the tween layer (presentation). No design until a use case exists — the
  author's own read.
- **Latin-OS** recorded as a far horizon: a Void Core OS with windows/panels
  as nodes — in display-stack terms, a *compositor whose scene graph is a
  mantle* plus Void Node as toolkit, likely hybridizing Void Node + XR
  (SimulaVR/monado cited as spiritual precedent). Security note for that
  future: the dispatcher is a natural choke point — attributed, logged
  inter-app commands make auditability a construction property, and
  capability scoping could ride tags/actors. Total observability as a
  security primitive.
- The **"what horizons demand TODAY"** list (conventions-as-spec, views
  replaceable wholesale, no library singletons, attribution everywhere,
  performance headroom, the log as time axis) is the enforceable takeaway —
  all already in force.

## 2026-07-13 (cont.) — Renamed Void Node → Void Maiz; open questions cleared

The author renamed the library: **Void Node → Void Maiz** (*maíz*, corn — an
abstract Spanish term with little collision, matching Void Core's
generic-root spirit; and Latin-OS keeps its "latinos" double meaning). Done
before the GitHub repo exists, when it's cheapest.

**Mechanical rename executed and verified by a clean rebuild + all tests
green (5/5) + demo rebuilt and launches:**
- Namespace `vn::` → `maiz::`; targets `voidnode*` → `voidmaiz*`; header dir
  `include/voidnode/` → `include/voidmaiz/`; version const `kVoidmaizVersion`.
  Scripted token replace across 28 code files + the sibling demo; sanity
  grep clean.
- **Living OKF docs + README + CLAUDE.md** renamed (Void Node → Void Maiz),
  draft-link `voidnode-draft-v1.md` deliberately preserved.
- **Historical artifacts kept the old name by design** (append-only truth):
  `okf/log.md` prior entries, `okf/references/`, `MESSAGE_FOR_VOIDCORE.md`,
  `MESSAGE_FOR_VOIDNODE.md`, `promptToStart.md`. Front-door note added to
  index.md + CLAUDE.md explaining the split.
- **The physical top-level folder is still `VoidNode/`** — Windows won't
  rename a directory that is a live shell's working directory. This is the
  ONE remaining manual step (rename folder → update the demo's `VOIDMAIZ_ROOT`
  default from `../VoidNode`), best done by the author before creating the
  GitHub repo. Everything inside is already Void Maiz.

**Terminology policy** (author left broad Spanish/corn renaming optional):
declined — the only genuinely generic term was the library name; the pasta
wire names (linguine/fettuccine) are deliberately distinctive and stay; Void
Core's vocabulary (mantle/glyph/rune/spirit) is upstream's, not ours to
rename. Revisit per-term only if one proves confusing in use.

**All open developer questions cleared** (Q3, Q6–Q11 → Decided):
- Q3: view-state split stands, `place` endorsed; new charge to keep the
  `view.*` config namespace **substrate-shaped** so XR/NE/touch cameras reuse
  the same config-tier pattern.
- Q7: fundamentals confirmed covered (matrix is the reference); remaining
  gaps are T2 QoL, not fundamentals.
- Q8: keep all four demos in mind but "just make them work"; focus is a solid
  library; **feedback loop** — author tests, reports, that seeds the backlog.
- Q9: interaction nets first-class, and apps should **use the net structure
  wherever possible**, not treat the graph as inert dataflow.
- Q10: Hormiga a consideration, not the focus. Q11: extend Scene (moot near
  term — 3D interaction lives in Void Maiz XR).

Next phase: Void Maiz T2 quality-of-life (wire selection, keyboard
vocabulary, tag filtering UI, frames/reroutes) driven by the author's
hands-on feedback from the demos.

## 2026-07-13 (cont.) — Folder rename closed out; nothing broke

The author renamed the physical top-level folder `VoidNode/` → `VoidMaiz/`
(the one manual step the rename entry flagged as pending — Windows couldn't do
it while the folder was a live shell's cwd). Swept the tree for fallout:

- **The predicted breakage, fixed**: the sibling demo's `VOIDMAIZ_ROOT`
  default still pointed at `../VoidNode` — retargeted to `../VoidMaiz`
  (`../InteractionCombinators/CMakeLists.txt`). Its README's build instructions
  and remaining "Void Node" branding trued up in the same pass.
- **Stale build caches blown away**: both `VoidMaiz/build/` and
  `InteractionCombinators/build/` had absolute paths baked to the old folder;
  deleted and regenerated. Clean **configure + full build green** in both, incl.
  `interaction_combinators.exe` linking against Void Maiz as a subdirectory
  through the corrected path.
- **Void Maiz tests**: 4/5 green on a clean rebuild. `reduce_conformance`
  fails *only under ctest* (`0xc0000139` / STATUS_ENTRYPOINT_NOT_FOUND) but
  **passes when executed directly** with the exact ctest arguments — `ldd`
  shows the ucrt64-built exe resolving `libstdc++-6`/`libgcc_s_seh-1` from
  `mingw64/bin`, a pre-existing MSYS2 runtime-DLL search mismatch. Unrelated to
  the rename (the target links only the static `voidmaiz_reduce`; no path
  dependence). Left as an environment note, not a code change.
- **Docs trued up**: the "folder is still `VoidNode/` pending rename" notes in
  `index.md` and `CLAUDE.md` now record the rename as done. Historical
  artifacts still keep the old name by design.

Verdict: the rename broke nothing in the code — the only real casualty was the
demo's hardcoded path, now fixed, and stale generated build caches, now
regenerated.

## 2026-07-13 (cont.) — Next target chosen: Node Blocks (concept written)

With T1 complete and the developer-questions queue empty, the author set
direction: **continue Void Maiz 2D, demos-first** (XR/NE later). Of the four
demo targets, the author picked **Node Blocks** — the Scratch-like — because it
forces a genuinely new library capability (hidden-wire rendering + a
snap-to-connect gesture) while staying 2D-desktop, the hardest projection test
yet (*position is the visible wiring*). Two shape decisions:

- **Structural-first, net-native**: ship the structural editor first (snap =
  link, adjacency rendering — the new view work), but model blocks as a **real
  interaction net** so "run = step via `maiz::reduce`" lights up later without
  redesign (Phase A / Phase B). Honors Q9 without gating the view proof on a
  rule set.
- **Turtle / motion-graphics** domain — the legible story, with control flow
  exercising C-block containment.

**Studied Blockly's connection model** (the substrates concept's flagged
prerequisite) and wrote the [node-blocks](/concepts/node-blocks.md) concept. The
finding worth recording: **Blockly's connection-check (array-of-strings
intersection, `null` = wildcard) IS Void Maiz's existing linguine type-check**
(`check_wire`: types match, empty = wildcard) — Node Blocks needs no new type
system. The block-as-net mapping: prev/next = a directed **linguine** chain
(hidden, shown by adjacency); value plugs = typed linguine (connector shape =
port type, node-geometry's conclusion); C-block body = **subgraph/mantle-enter**
containment; the **principal port + fettuccine are reserved for Phase-B
execution** (a program-cursor forms an active pair; `step` runs + advances;
`undo` un-runs — reusing the whole InteractionCombinators reduce machinery). The
two new **library** capabilities (generalizable, not demo code): the
snap-to-connect gesture+compiler (proximity drop → `link` + aligning `setjson
pos`, one batch) and the adjacency render mode (a `drawn`|`adjacency` style on
the linguine, not a new wire kind); block bodies extend node-geometry with a
`"block"` shape kind. Demo (`../NodeBlocks`) owns the turtle glyphs, the turtle
output face, and Phase-B rules — the host computes, the boundary holds.

Backlog updated (Node Blocks promoted from T3 to an active, staged entry);
index map gains the concept. **No code yet** — concept-before-code (ground rule
1). Next: the Phase A build plan, then the snap gesture + adjacency mode.

## 2026-07-13 (cont.) — The first feedback batch: the Q8 loop fires

The author's first hands-on report from driving the demo — eight items,
exactly the "author uses it, reports, that seeds the backlog" loop Q8
designed. Two were design corrections, shipped immediately:

- **Principal↔aux wires are now draggable** (feedback #6 — "that's honestly
  what has made making an interaction combinators program so difficult").
  The old `check_wire` rejected them as a v1 caution; in the mathematics a
  wire joins ANY two ports, and passive principal↔aux wiring is how real IC
  programs are built (the demo's own starter net dispatches `1:0` links!).
  Now: principal↔aux → Linguine (principal untyped/directionless; the aux
  end decides direction — `0:j` feeding an input, `i:0` fed by an output);
  **principals are strictly single-occupancy** (linking one clears its
  standing fettuccine AND passive wires in the rewire batch — a net port
  holds one wire end); grabbing a principal holding a passive wire detaches
  it for re-routing. `to_net` confirmed indifferent to direction (reads only
  `i:j`), so hand-wired nets reduce. Concept trued up
  ([interaction-connections](/concepts/interaction-connections.md), new
  "passive wires" section); verdict matrix + compile strings pinned in
  `gesture_smoke`.
- **Per-rune color as content** (feedback #5, the ε-tracking aid):
  `content.color` ("#rrggbb") now overrides `hints.color` in the projection —
  the same view-state-as-content tier as pos/size/collapsed, so recolors are
  logged and undoable and **survive rewrites** (rewrite copies carry
  content). Demo's ε glyph declares `color` editable → select an eraser,
  type `#00ff88` in the inspector. Invalid values fall back to the hint
  (pinned in `project_smoke` with undo round-trip).

Also from the batch: **the demo has a mini-OKF of the mathematics itself**
(feedback #8 — `../InteractionCombinators/okf/interaction-combinators.md`):
agents/arities, the six rules (γγ cross vs δδ parallel annihilation — the
asymmetry that buys universality), locality, strong confluence (why any
click order works), the termination caveat, **vicious circles** (Lafont's
deadlock — principal→aux cycles where no redex can ever form; author ruling:
detectable, never prevented), and the maths→model mapping table. Linked from
the demo README.

Queued to T2 as **[fb]** items outranking the rest: auto-clean layout (#1),
laptop camera controls (#2), right-click context menu (#3), tooltips (#4),
vicious-circle detection (#7). Resizing (#4b) needs clarification — the grip
exists; asked the author what felt wrong. Builds green both repos; 4/4 real
suites + reduce 10/10 (the reduce_conformance ctest/Git-Bash failure is a
ucrt64-vs-mingw64 DLL environment quirk — PowerShell runs it clean).

## 2026-07-13 (cont.) — Feedback round two: log copy, the context menu, one interaction at a time

Two more author asks, both shipped same-session (plus fb#3 landed as their
vehicle):

- **[fb#9] The log leaves the app** — `maiz::log_to_text(entries, condensed)`
  in the widgets API, and the log strip itself grew a right-click menu:
  **copy condensed** / **copy all** → the OS clipboard. Condensed keeps the
  structural story — errors/warnings, rune/rm/link/unlink/tag/mantle/undo/
  redo/set lines, and batches containing structural commands — and drops the
  view tier (camera config, pos/size/collapsed writes, move-only batches) and
  query chatter. The filter is a documented heuristic over the line text;
  hosts wanting different curation call `log_to_text` themselves (the demo
  adds visible copy buttons above its strip).
- **[fb#3] The right-click context menu is real** — `edit_canvas` opens a
  popup on a clean right-CLICK; right-DRAG still pans (click-slop
  disambiguates, same rule as node clicks). Built-ins: node → collapse/expand
  + delete (deletes the selection when the target is in it); empty canvas →
  an **add** submenu from the palette at the click's world position. Hosts
  extend via the new `ContextMenuFn` hook (drawn above the built-ins,
  compiled commands pushed into CanvasIO) — the menu is just another
  compiler frontend, every entry a logged dispatch.
- **[fb#10] One interaction at a time** — the author's control ask: never
  reduce the whole net from a click on one thing. Right-clicking a **hot
  agent** (member of an active pair) offers **"interact — fire this pair"**,
  which steps exactly THAT redex: `compile_step` gained a `chosen` agent
  parameter (`maiz::reduce::step` already took a specific redex — the demo
  just always passed `pairs.front()`). step/reduce buttons unchanged;
  confluence makes the order immaterial to the result, but the choice now
  belongs to the user — which is the interaction-net pedagogy working.

Header hint updated ("right-click a hot agent: fire JUST that pair").
Verified: both repos rebuild green, 4/4 real suites (link once failed with a
bare `ld exit 5` — the demo exe was RUNNING; noted: stop the app before
rebuilding). Remaining [fb] queue: auto-clean (#1), laptop camera controls
(#2), tooltips (#4), resizing clarification (#4b), vicious-circle detection
(#7); menu wire-targets await wire hit-testing.

## 2026-07-13 (cont.) — Hover highlights [fb#11]; tooltips [fb#4] and laptop pan [fb#2] ride along

The author's ask: **UI elements should highlight under the mouse.** Landed as
a canvas-wide hover pass in `edit_canvas`, with two queued [fb] items riding
the same machinery:

- **The `hover` theme accent** (blue on both presets) outlines whatever the
  cursor would act on, checked in the same priority order as clicks: **port**
  (accent ring, also live during a wire drag alongside the verdict tint) →
  **node body by its TRUE shape** (rounded rect / circle / polygon — the same
  math as hit-testing, so the highlight never lies about the click target),
  with **chrome accents** when the cursor sits on the collapse dot or resize
  grip (the small-target aiming pain, addressed) → **wire** (re-traced
  thicker in accent).
- **`hit_wire` exists** — nearest wire within 6px of the drawn curve
  (bezier sampled 24 segments, control points shared with `draw_wire` via
  the new `wire_endpoints` helper factored out of `render_scene`). This is
  the first half of the T2 "wire hit-testing" item; click-to-select is the
  remaining half.
- **[fb#4] Tooltips**: ports and wires tip immediately (`name : type (dir)`,
  `from — to (relation)`); nodes show a dwell card after ~0.55s (name, label,
  tags, fields, the enter hint) so crossing the canvas doesn't flicker cards.
- **[fb#2] Laptop pan**: **Space+left-drag / Alt+left-drag** pan — no middle
  button needed; middle/right-drag unchanged (the pan state tracks which
  button started it so the right-click menu logic stays clean).

All view-ephemera — nothing here dispatches or logs (lasagna bottom tier).
Both repos green, 4/4 suites, demo relaunched.

## 2026-07-13 (cont.) — The logging razor, stated; tag pigments [fb#12] via a fuse extension

**The author articulated the lasagna model's razor**, prompted by unlogged
hover: *"things that need to be logged are things that change states — when
you save a project, quit, and log back in, what do you expect to be the
same?"* Colors yes, positions yes, last-hovered no — with the deliberate
caveat that this is **a per-application choice, not a Void Maiz law**
(IC chose unlogged hover; an attention-analytics app could stream it).
Folded into [total observability](/concepts/total-observability.md) as "the
author's criterion," next to the ephemera tier it governs.

**Tag pigments shipped** — the author's design: tags ARE colors (`red` tints
red; +`yellow` → orange; `blue`+`yellow` → green), and the mixing is computed
by **interaction nets running invisibly behind the UI** — "a use case of
interaction nets happening in the background of the software itself, not in
the node graph viewer." Implementation:

- **`maiz::reduce` gained a `fuse` rule kind** — our extension beyond the
  upstream contract (which pins only annihilate/commute): `{"glyphs":[a,b],
  "rule":"fuse","into":c}` replaces the pair with ONE agent of glyph c whose
  aux ports adopt a's boundaries 1..m then b's 1..n (arity-0 fusions leave a
  free-floating result). Pinned in the conformance runner as a separate
  extension block (arity-0 fusion, boundary adoption on arity-1, spec
  guards); the 10 contract cases still pass untouched. Candidate upstream
  FYI if the contract ever wants a third kind.
- **The demo's pigment algebra**: seven arity-0 glyphs (red/yellow/blue,
  orange/green/purple, brown), primaries fuse to secondaries, every other
  distinct pair muddies to brown — one rule per unordered pair (the
  confluence guard), 21 rules generated as data. `mix_tags` folds a node's
  color tags: two agents, one wire, one redex, read the survivor, repeat.
- **Post-projection pass**: color tags outrank hints.color/content.color.
  `tag era1 +red +yellow` → orange eraser, logged, undoable, replayable —
  the TAGS are the state; the mixing net is pure background compute (the
  compute boundary holds: the mantle never sees the pigment net).

Documented in the demo's math mini-OKF ("Tag pigments — nets as the
software's own substrate"). All suites green (4/4 + conformance 10/10 +
fuse extension ok); demo relaunched.

## 2026-07-13 (cont.) — Pigments edit as TAGS: the chip editor; birth tags; ε's color field retired

The author, after driving pigments: the `set color` field "is not supposed to
be the thing" — tags are; agents "should already come with color tags (since
they already have color when placed)"; and there must be "a little delete
option" for tags — any tags, color or otherwise. Landed:

- **The inspector's tag chip editor** (library): every tag renders as a chip
  — click its × to remove (`tag <node> -red`); a "+ tag…" input adds
  (`tag <node> +red`, focus stays for the next one). New `compile_tag`
  gesture compiler, string-pinned in `gesture_smoke`. Any tag edits the same
  way — pigments get no special casing; they're just tags the background net
  happens to read. (T2 "tag editing UI" done; tag completion from `axes`
  still open.)
- **Birth pigments** (demo): the starter net tags every agent with its
  identity color at creation — γ +orange, δ +blue, ε +red — so the visible
  color IS editable state from the first frame (the save/quit/reload test:
  colors survive because tags are model content). Rewrite copies start
  tagless (per the contract) and fall back to hint colors.
- **ε's `color` content field retired** — two color mechanisms was one too
  many; the content.color projection override stays as a generic library
  convention (VLS-shaped), but the demo no longer surfaces it.

All suites green (4/4; conformance + fuse via PowerShell); demo relaunched.

## 2026-07-13 (cont.) — Pigment inheritance [fb#14]; quick add-and-link [fb#13]

Two more from the author's hands-on loop:

- **"Colors transfer weird after the interaction"** — diagnosed: the colored
  ε's principal WAS the redex (γε/δε commute consumes it and mints copies),
  and the contract pins **copies start tagless** (conformance case 08) — so
  the copies fell back to hint color. Fix is app-policy, not an engine
  change: the demo's `compile_step` now **re-tags minted copies from their
  same-glyph redex parent** — visible `tag` commands inside the step batch,
  so a colored ε's copies stay colored through commutes, and the γ/δ birth
  pigments propagate too. The contract stays honored; the policy is layered
  above it and documented in the demo's mini-OKF.
- **Quick add-and-link** — the author's wheel ask: drag a wire from a port,
  release on empty canvas → the add box opens at the drop point; picking a
  glyph mints it, places it, and **links it back to the dragged port via the
  new node's principal** — always legal (fettuccine from a principal,
  passive linguine from an aux; principals are untyped so the check always
  passes), and always ONE batch = one undo frame. Fresh drags only —
  detached wires keep their drop-to-unlink semantics. Direction follows the
  aux end (aux input is fed BY the new principal, 0:j). Shipped as the
  filterable list (the existing add-box UI); a radial wheel presentation is
  recorded as possible polish.

All suites green (4/4); demo relaunched.

## 2026-07-13 (cont.) — The interaction IS the wire: wire right-click targets [fb#15]

The author, after a full `reduce` emptied a hand-built net they'd only meant
to step once: *"i should be able to right click on interactions between 2
nodes (the wires between the two nodes), and JUST have the interaction rules
from only that wire take place."* Exactly right — the redex is the WIRE, so
the wire is the natural click target:

- **Wires are context-menu targets now** (`edit_canvas`): a right-click that
  hits no node falls through to `hit_wire`; the `ContextMenuFn` hook grew a
  wire parameter (node, wire, io), and wires get a built-in **unlink** entry
  (most of the T2 wire-hit-testing item; click-to-select remains).
- **The demo fires the EXACT pair from the wire**: right-click the glowing
  fettuccine → "interact — fire this pair" → `compile_step` with both
  endpoint names (an exact-pair match — the node path's either-member match
  stays for hot agents). One redex, one batch, one undo frame.
- **"reduce" renamed "reduce all"** — the scope confusion that triggered the
  report shouldn't recur.

Incidentally the author's log was a full end-to-end validation of the last
batch: quick add-and-link built the net, birth pigments + the chip editor
recolored it, and pigment inheritance carried +purple/+blue/+red/+green
through every commute — visible as `tag` commands inside the step batches.
All suites green (4/4); demo relaunched.

## 2026-07-13 (cont.) — Shaped-node resize [fb#4b]; clean view [fb#1]. The feedback batch is CLOSED (except #7)

**Resize, properly diagnosed at last**: the author asked for resizing (first
proposing size-as-a-TAG, then retracting it same-message — correctly: size
already rides `content.size`, the view-state-as-content tier, and a
tag-based size "makes no sense… UX wise not smart." The retraction is worth
keeping: it is the save/quit/reload razor applied by its own author).
The REAL gap: the resize grip only existed on window chrome — and the demo
is all shaped bodies, so resize effectively didn't exist. Now:

- **Shaped nodes resize**: selecting one draws a grip at its bounding-box
  corner; the gesture checks that corner BEFORE body hit-testing (it lies
  outside the true shape); shape-aware minimums (24px, not window chrome's
  80×46); release compiles the same ONE `setjson size [w,h]`.
- **The inspector grew a size field** — every node, "w h" text, the
  fields' staging discipline, one `setjson size` on commit. Manual entry
  as the author asked.
- **Clean view [fb#1] shipped as `maiz::compile_clean`** (UI-free,
  test-pinned): iterative pairwise separation of overlapping bounding boxes
  (+16px margin), ONE batch of `setjson pos` — a tidy is one undo frame;
  already-clean scenes compile nothing; auto-laid nodes get pinned by a
  clean deliberately (an explicit placement act). The demo's header has a
  **clean** button. Wire-crossing minimization remains the tidy story's
  second half.

With this, the 2026-07-13 feedback queue is closed except **vicious-circle
detection (#7)** — next. All suites green (4/4); demo relaunched.

## 2026-07-14 — Two mathematics bugs from hands-on use: γγ≠δδ, and self-wired redexes

The author's deepest catches yet, both from actually drawing nets:

- **"The γγ interaction is wrong — it swaps the ports; that's δδ."**
  Diagnosed: the engine had ONE annihilation (index-straight, x_i≡y_i) for
  both γγ and δδ — the γ/δ asymmetry that makes the calculus universal
  literally could not be expressed, and index-straight IS δδ's crossing
  picture. Fixed with a **`"swap": true` annihilation flavor** (x_i≡y_{n+1−i}
  — Lafont's γγ, the parallel-arcs drawing between mirrored bodies); the
  demo's γγ rule carries it. Convention trap recorded in the mini-OKF: index
  and drawing INVERT because redex partners face each other mirrored.
- **"Program crashed on a looped connection… this should still be legal."**
  γ~δ principals + γ.aux↔δ.aux — a legal Lafont net whose commute is
  well-defined; the contract's case 09 *deliberately* rejects it ("the
  restricted subset's divergence from full Lafont semantics") and the demo
  didn't catch the throw. Fixed at all three levels: **the engine resolves
  internal redex wires correctly by default** (annihilation: union-find over
  the wire equations — two external ends bridge, one stays free, a closed
  loop vanishes; commutation: the internal wire becomes a principal-principal
  pair of the minted copies — a fresh redex, Lafont's own picture; fuse:
  joins the fused agent's own aux ports); **`strict_locality` flag** keeps
  the contract's restricted subset, passed only by the conformance runner —
  case 09 still pins it, 10/10 stays green; and **the demo can no longer
  crash on reduce errors** — every step path (buttons, context menus) and
  the per-frame net analysis in reproject are guarded, degrading to a
  visible `[error]` log line (a malformed hand-typed relation now logs once
  instead of crash-looping too).

Extension tests pinned: γγ-swap endpoint wiring, the author's exact crash
net (internal-wire commute → copy principals paired), annihilate-with-loop
(loop vanishes, externals bridge), strict mode still throws Locality.
**Upstream message drafted fresh** (`MESSAGE_FOR_VOIDCORE.md`): the
annihilation-permutation gap and the locality-vs-legal-nets finding, each
with a concrete request (a `swap` flavor + full-tier cases; we offered our
three extension tests as case drafts). All suites green; demo relaunched.

## 2026-07-14 (cont.) — Void Core 0.2.4: both findings adopted; the contract is now 14 cases

Void Core's reply (`MESSAGE_FOR_VOIDMAIZ.md`, 2026-07-14) adopted both
requests — our engine's extended behavior IS the contract's default now, and
the `swap` spelling was taken verbatim:

- **`swap` annihilation adopted** (case 11), reversal only, deliberately not
  general permutations (reversal is an involution, hence symmetric in the
  pair's unordered rule form; a general permutation would force an agent
  ordering — a new finding if a real program ever needs one). Authoring
  subtlety worth remembering: the canonical form's wire endpoints are
  `[glyph, port]` — glyph only, no content — so a swap-distinguishing case
  needs *distinct boundary glyphs* (case 11 carries four); distinguishing by
  content alone would not have caught our bug.
- **Internal redex wires resolve by DEFAULT** — promoted to the contract
  default, not a tier (our "a node editor either crashes or must forbid
  drawing legal nets" argument). Semantics as we implemented: two external
  ends bridge, one stays free, closed loop vanishes; commute joins the
  copies' principals — a fresh active pair. **Loop ruling (we asked):
  vanish, normatively** — the net model can't represent an agentless wire;
  loops-as-values is host-side counting, and needing loop counts to survive
  a round-trip would be a contract finding, not a workaround.
- **Cases 12–14 pinned.** Case 13 is deliberately case 09's *exact net*,
  resolved instead of rejected. Case 14 pins the **aux-to-self** commute
  flavor, NOT our cross-agent γ.aux↔δ.aux build — that one is legal but
  *divergent* under γδ commute (each rewrite regenerates the internal wire a
  generation down), so a reduce-to-normal-form case can't pin it; upstream
  recommends exactly what we already have, a single-`step()` pin on our
  side. Kept, with the comment updated to say why.
- **`fuse`: noted, not adopted** — recorded in the contract's home concept
  page as a rule kind in the wild; becomes a candidate the way `swap` did
  when a second consumer appears. Backlog note updated.

Our side of the evolution (the one runner change upstream named, plus
true-ups): **the conformance runner now forwards the `strict_locality` case
key** (case 09 carries it in its JSON) instead of hardcoding strict for
every case — mirroring `run.py`; without it cases 12–14 would reject nets
the contract now resolves. Comments trued up where "our extension beyond the
contract" became the contract (`reduce.hpp`, `reduce.cpp`, the runner's
extension block — which is now "fuse + step-level pins"); OKF trued up
(index, roadmap, developer questions, backlog, interaction-connections):
we build against **Void Core 0.2.4**. `MESSAGE_FOR_VOIDCORE.md` is consumed;
no open upstream asks.

Also from the housekeeping section: the `vc_version()` drift we reported is
fixed (the C string agrees with the `version` verb now, all at 0.2.4) —
`embed_smoke`'s version floor bumped to 0.2.4 and it now asserts the two
sources agree, retiring its "vc_version() lags" caveat.

**14/14 conformance on the first run** — including case 11's swap wiring,
case 13's resolution of the old locality net, and case 14's aux-to-self
flavor we never tested locally: the adoption really was verbatim. All 5
ctest suites green.

## 2026-07-14 (cont.) — UI push: projects, ribbon, panels, Space-to-fire, the smush animation, live physics

Six author asks in one session, library + demo:

- **Log strip scrolls now.** The bug: `SetScrollHereY(1.0)` ran
  unconditionally every frame, so the wheel fought the tail pin and lost.
  Fix (widgets.cpp): follow the tail only when already AT the tail — the
  standard idiom, scrolling up to read history sticks.
- **Panels resize.** New library widget `maiz::splitter` (a draggable
  divider; `released` fires at drag end). The demo's fixed three-window
  layout became one workspace with two splitters (canvas|inspector,
  top/log); the fractions are view state and flush to the config tier on
  release (`config set view.panels "c l"`) — camera semantics, applied to
  layout. They ride the saved project like everything in config.
- **Space fires an interaction** — the hovered active wire's exact pair, or
  a RANDOM active pair ("just let things interact"). `edit_canvas` now
  exposes the hovered wire (`ed.hover_wire`, view ephemera); Space was
  REASSIGNED from the laptop-pan chord by this ask (Alt+drag still pans) —
  author's ruling by implication, recorded here. Ignored while typing.
- **Projects: save/load/new/recent.** `save` was already a Void Core verb
  that hands the exported state to the host's EFFECT handler — the demo
  finally installs one and writes the file (the world-facing half the core
  can't do; the dispatch is logged like everything). Load replays the file
  through the `Core(state_json)` constructor + re-registers glyphs
  (host config, not state); New is a fresh core + empty mantle. Projects
  live in `./projects/*.json`; the recents list is app preference, not
  model state — a host-side dotfile, capped at 8. Camera/panels/animation
  settings ride INSIDE the saved file (config tier) — save/quit/reload
  honors them for free.
- **The menu ribbon** (File/Edit/Selection/View/Net) — the author's ask for
  the standard Qt-style bar. Every model-touching entry compiles to the same
  dispatcher commands as the gestures (undo/redo/clean/relax/delete); the
  menu is just another compiler frontend, like the context menu before it.
  Selection entries stay view-local (Q#3's pending ruling unchanged).
- **The rewrite animation** — the author's choreography: "nodes should kinda
  smush together, fuse a bit, then leave behind whatever they were gonna
  do." Library: `CanvasFx` (transform overrides + ghosts; geometry, wire
  anchors, and hit-tests all honor it — wires follow mid-flight bodies).
  Demo: three phases over ~0.9s (glide+compress → collapse into the fusion
  point → results grow outward), driven per-frame and dropped when done —
  pure ephemera, the model changed atomically when the batch dispatched
  (concept true-up in views-as-projections.md). A **Settings window**
  (View menu) holds on/off + speed (0.25–3×), persisted as `view.anim` in
  the config tier. `reduce all` became a stoppable chain: one step per
  finished replay.
- **Physics.** Library: `relax_step`/`compile_relax` (gesture.cpp, next to
  clean) — short-range body repulsion + wires-as-springs toward a rest
  length, position-based integration (no velocities, cannot explode),
  test-pinned (overlapping wired bodies separate but stay tethered; a
  spread scene is at rest; one batch). Demo: **Relax layout** (one-shot,
  clean's sibling) and a **Live physics** toggle that stages positions
  frame-by-frame like a drag and flushes ONE batch on settle — or right
  before any other command, so the transcript never interleaves. Pauses
  during drags and animations. Concept true-up in total-observability.md
  (the two hard cases, sorted by the save/quit/reload razor).

All 5 suites green (relax pins added to gesture_smoke); demo rebuilt and
relaunched. Polish left open: non-uniform squash on the smush (needs
elliptical shaped-node rendering), wire-crossing minimization in relax.

## 2026-07-14 (cont.) — The APK: Interaction Combinators on Android, substrate axis proven

The author asked for an APK of the demo — substrates.md's planned mobile
target, upgraded from "bare playground" to the full app. Shipped:
**`android/interaction_combinators.apk`** (4.75 MB, arm64-v8a, minSdk 26,
signed), built entirely from what was already on the machine.

- **The app/platform split.** The demo's monolithic main.cpp became
  `src/app.hpp/app.cpp` (**everything** — core, projection, gestures,
  projects, ribbon, animation, physics — platform-free) plus two shells:
  `src/main_desktop.cpp` (GLFW, unchanged behavior) and
  `android/src/main_android.cpp` (NativeActivity glue + EGL/GLES3). The
  platform seams are exactly four: a base dir for projects, a title
  callback, a quit callback, and `touch_mode`.
- **Touch, per the concept**: one finger IS the ImGui pointer — node drags,
  wire pulls, menu taps are the desktop gestures verbatim; two fingers are
  the CAMERA (pan + pinch, consumed by the shell, routed to
  `app.touch_pan/touch_zoom`), marking cam_dirty so the SAME
  `config set view.camera` command logs on gesture end. Touch mode also
  widens hit targets (port radius 20px, fatter splitters) and adds an
  **Add menu** to the ribbon (no Shift+A, no right-click on glass; mints at
  the visible canvas center).
- **Build system, per the language-boundary rule**: zero Java, zero Gradle.
  A `hasCode=false` NativeActivity manifest + `build_apk.ps1` (NDK CMake →
  aapt2 link → aapt add → zipalign 16K → apksigner, debug keystore minted
  once) are the whole shim. Void Maiz's CMake grew an `if(ANDROID)` branch:
  Void Core compiled **from C11 source into a static lib** (desktop keeps
  the prebuilt DLL), imgui's android backend (vendored at our exact 1.92.1
  tag) + GLES3 replaces glfw+GL, examples/tests skipped.
- **Clang caught two real dangling-reference bugs** GCC never flagged, both
  the same shape — `auto key = std::minmax(temporary, temporary)` binds
  references that die at the full expression: one in **the engine's fuse
  path** (reduce.cpp internal-wire dedup), one in the demo's pigment-spec
  builder. Both materialized to value pairs; all suites re-green. Cross-
  compiling paid for itself before the APK even installed.
- **v1 gaps, recorded**: no soft keyboard (Save As pre-fills a
  `net-<timestamp>` name so projects save untyped; the command bar types
  only on desktop) — an IME shim is the known next step; no long-press
  context menu; single-ABI arm64 (the script takes `-Abi`).

Desktop rebuilt on the refactor and relaunched, 5/5 suites green, APK
signed. Untested on hardware — no device was attached; `build_apk.ps1
-Install` sideloads when one is.

## 2026-07-14 (cont.) — APK touch pass: bigger, log-free, double-tap-to-fire

Author feedback on the APK, applied (all gated on `touch_mode`; desktop
untouched):

- **Bigger**: density scale ×1.3 on top of dpi (floor 1.3×, cap 5×), touch
  metrics set before scaling (fatter FramePadding/ItemSpacing/scrollbars,
  `TouchExtraPadding` forgiveness), toolbar uses real Buttons instead of
  SmallButtons, port hit radius 24px, click-slop 10px, and the default
  camera starts at 1.6× zoom so bodies are finger targets. The activity
  locks **landscape** — canvas + side inspector under a ribbon is a
  landscape layout.
- **No log pane on mobile**: the transcript still records (total
  observability is about the record, not the pixels — any app can read it;
  desktop still shows it), but the phone gives the height to the canvas.
  Errors surface as a 6-second red **toast** under the toolbar instead. The
  status readout also went; a small "N pairs" counter stays.
- **Double-tap fires an interaction** — on a glowing wire or a hot agent,
  exactly that pair; anywhere else on the canvas, a random one. The tap
  position doubles as the hover (the pointer rests where the finger last
  landed, so `ed.hover_wire`/`ed.press_node` are fresh); the second tap's
  accidental drag is aborted before firing. Library fix along the way:
  `edit_canvas` now clears `press_node` on an empty-canvas press — it went
  stale otherwise, and the double-tap handler keys off it.

Both targets rebuilt clean; desktop relaunched; APK re-signed (4.76 MB).

## 2026-07-14 (cont.) — APK touch pass 2: portrait, thumb-sized double-tap, the double-scaled-text bug

Second round of author feedback from the phone:

- **Portrait, not landscape** (author's call, reversing our lean). The touch
  layout restacked: canvas on top (full width), inspector BELOW it — a side
  column is unusable at phone widths. `canvas_frac` persists the vertical
  split on touch, same config key.
- **The white-text bug, diagnosed**: canvas node names were drawn at
  `GetFontSize() × zoom` — but in imgui 1.92 `io.FontGlobalScale` folds into
  `GetFontSize()`, so on the phone (UI scale ~3.9×) node text rendered
  **UI-scale × zoom** — giant white contrast-text spilling off the small
  bodies onto the light canvas: white on white. Fix in the library, two
  parts: `canvas_font()` divides the UI scale back out (node text scales
  with the CAMERA, not the chrome — the world is not chrome), and
  `text_with_halo()` puts a dark halo behind light contrast text so any
  overflow stays readable on any canvas. Bonus kill: `CanvasStyle::
  hover_tooltips=false` on touch — the pointer never leaves a tap, so
  tooltips were popping 0.55s after every touch and lingering forever.
- **Double-tap reach**: two fixes. `MouseDoubleClickMaxDist` 6px→48px +
  time 0.3→0.35s (two taps of a thumb don't land within 6px — the gesture
  mostly wasn't registering at all); and the target is no longer hit-tested
  — the app picks the NEAREST active pair within a thumb radius
  (~110px × UI scale, distance to either agent or the wire midpoint),
  falling back to random. Anywhere on the canvas reduces something.
- **Chrome abstraction begun** (the author's "should be abstractable?"):
  `maiz::apply_touch_metrics(scale)` — one library call makes every widget
  finger-sized (metrics set pre-scale, TouchExtraPadding, the double-tap
  thresholds) — and `maiz::tool_button` (SmallButton on desktop, Button on
  touch). The workspace/pane rung is deliberately NOT climbed yet —
  recorded as **Q11** with a lean (one rung per real need; a second host
  should exist before the pane shape freezes into the library).

Both targets rebuilt; 5/5 suites; desktop relaunched; APK re-signed.

## 2026-07-15 — Void Hormiga arrives; Node Blocks Phase A ships; three face widgets

**A new client founded itself.** `MESSAGE_FOR_VOIDMAIZ.md` (repo root, replacing
the consumed Void Core 0.2.4 reply) announced **Void Hormiga**
(`../VoidHormiga`): the ground-up native C++20 rebuild of Hormiga on Void Core
+ Void Maiz — the author's call REVERSING the recorded web-adoption lean (no
protocol shell, no WASM; the calculus changed because Void Maiz now exists as
working code). OKF trued up: `substrates.md` (the Hormiga clause is now a
recorded reversal; web paths stay as general client-less options),
`scope-and-clients.md` (Void Hormiga is client #2 — the first data-heavy one),
backlog (T3 "Web/Hormiga adoption" → "Web adoption (no client)"; a [vh] queue
of its asks), Q10's decided entry annotated. Reply drafted per convention:
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA.md`.

**Node Blocks Phase A shipped — the whole package, same day** (the message's
§2.1 pressure landed on work already chartered):

- **`"block"` shape kind** (scene + projection + canvas): notch/tab silhouette
  bodies; connectors identified by port NAME — aux input `prev` = top notch,
  aux output `next` = bottom tab, other aux inputs = value sockets down the
  right edge, another aux output = a reporter's plug on the left. The anchor
  math is ONE world-space function (`maiz::block_anchor`, UI-free); the canvas
  scales it by the camera, so gesture and drawing cannot disagree. Blocks show
  the glyph LABEL (Scratch's "move", not "move-1"), no chrome, no drawn
  principal (reserved for Phase B; not a drag target either).
- **Adjacency render mode**: `"render":"adjacency"` on a port declaration →
  the projection stamps `SceneWire::Style::Adjacency` on wires touching that
  port (a render style, not a fourth kind — the lean held). Flush endpoints
  draw NOTHING (adjacency IS the link); separated-but-linked blocks draw a dim
  honest tether; hover makes the hidden link visible (ring at the seam).
- **Snap-to-connect**: `find_snap` (nearest compatible free-connector pair
  within `snap_radius`, `check_wire`-gated, connectors wired inside the
  dragged set excluded) + `compile_block_release` — a block drag's release is
  ONE batch: aligned moves (the whole dragged set lands rigid), occupancy
  unlinks (block connector ends are single-occupancy on BOTH sides),
  the link, **splice** (dropping on a seam inserts: the displaced neighbor
  re-attaches to the dragged stack's free end), **tear** (adjacency wires
  pulled past 2.5× snap radius unlink — dragging out of a stack IS the unlink
  gesture), **heal** (a torn-out middle block's old neighbors join), then the
  re-flow of affected resting chains. One undo un-does the entire gesture.
  Verdict-tinted snap preview (target ring + aligned ghost outline) while
  dragging. **Grab-the-stack**: dragging a block stages its chain below plus
  plugged reporters (`block_stack_below`) — selection stays honest.
- **Stack re-flow**: `compile_stack_layout` — one batch bringing every chained
  block flush (the blocks' "clean"); bounded passes so linked cycles
  terminate.
- Geometry/thresholds in `maiz::BlockMetrics`, shared by the UI-free compilers
  and `CanvasStyle::block`. Faces render on block bodies (inline, after the
  label — block arguments ARE face widgets).
- **Tests**: block anchors, snap verdict + pinned release batch strings
  (plain snap / seam-insert with splice+re-flow / tear+heal), stack layout,
  grab-the-stack, and a live-core round-trip (snap batch = ONE undo frame
  restoring link AND position) in `gesture_smoke`; shape + adjacency-style
  projection pinned in `project_smoke`. **5/5 suites green.**

**`../NodeBlocks` founded** (sibling repo, add_subdirectory pattern): turtle
glyph set (when-run hat / move / turn / pen / repeat, Scratch palette colors),
inline face args (drag-numbers + pen combo), `repeat` as a real C-block
(mantle-enter body; palette-minted repeats get a host auto-repair batch that
creates their body mantle), a **Stage** node whose face draws the turtle
path — a host-computed STATIC trace of the program structure (recursing into
body mantles via per-mantle projections), recomputed after every dispatch;
Phase A executes nothing in the model, and the README says so. Screenshot-
verified: the starter stack renders flush with hidden wires, the trace draws
the square. (Scripted drag verification was aborted — the author was actively
using the machine; the snap path is compiler-level test-pinned, and the
hands-on loop will exercise the rest. Phase A's snap-gesture transcript-replay
exit test stays open in the backlog.)

**Three face widgets shipped** (Void Hormiga asks §3.1–3.3, all lean-library,
all the staging discipline): `face_text_multiline` (ImGui's active-buffer
ownership IS the stage; ONE `set` on blur/Ctrl+Enter; Escape reverts and
compiles nothing; disabled-when-wired), `face_image` (host texture, aspect-fit,
placeholder, click returns to the host — it compiles the command),
`face_date` (three staged drag-fields → ONE `set` of a clamped leap-aware
ISO-8601 string). **Q11 got its second host**: proposed ruling drafted in the
reply + `developer_questions.md` — primitives (`splitter`,
`apply_touch_metrics`, `tool_button`) + the blessed IC workspace pattern;
layout stays host-owned; no docking framework. Awaiting the author's confirm.

Queued [vh] (the new client's weight, in `backlog.md`): the TABLE as the
second view (library-worthy, per both sides' lean), bulk select→one batch,
palette categories, plus promoted existing items (tag filter box, completion
from axes, search/jump, error toasts). Marquee sizing fixed to shared
node_width along the way. Node-blocks concept trued up (Phase A section;
three sub-questions resolved as their leans; new open question: inline
C-block body rendering).

## 2026-07-16 — The widget protocol ships (draft): Void Hormiga's second message, consumed

`MESSAGE_FOR_VOIDMAIZ.md` (2026-07-16, Void Hormiga) asked for Phase 4's
deferred engineering — its Data section (tables, forms, detail panes,
wizards) is "traditional desktop UI that is still CLI-complete", and §1
named the widget protocol as the gating ask. Shipped as a draft the same
day, with Hormiga as the declared forcing client:

- **`voidmaiz/widget.hpp` + `src/view/widget.cpp`** — the protocol. The
  minimal surface is the CONTEXT, not an interface: `WidgetContext` =
  projected Scene (read) + commands vector (write) + active tag filter +
  width hint. CLI representation and undo participation fall out of the
  shape (a widget has no other way to change anything); responsiveness is
  re-projection; staging is the widget's duty (VLS #14b). Not to be
  confused with widgets.hpp (the observability widgets) — recorded in both
  headers.
- **The registrable unit is the FIELD EDITOR.** Glyphs bind fields to
  editor kinds via a new `hints.editors` map ("date", "number:0.1,0,10",
  "combo:a,b,c", host kinds like "color"); the projection surfaces it as
  `SceneField.editor` (pinned in project_smoke); `WidgetRegistry` maps
  kind → renderer. ONE registration lights the editor up on every field
  surface: inspector, faces, `widget_form` detail panes, table cells when
  the table lands. Unknown/missing kinds fall back to staged text — a
  declared field is never uneditable.
- **One implementation per discipline**: the kit (text/number/multiline/
  combo/date = `WidgetRegistry::defaults()`) absorbed the face widgets'
  cores; face.cpp is now thin frames over widget.cpp (face_image stays —
  an image click is domain semantics, not a field commit). The INSPECTOR
  became a registry client (`draw_inspector` takes an optional
  `WidgetRegistry*`): a host-registered editor appears there with zero
  inspector changes, and inspector fields now honor disabled-when-wired
  like every field widget (previously faces only).
- **The one filter bag fixed library-side** (`filter_bag`/`node_matches`,
  project.hpp, UI-free, test-pinned): tags + name-as-tag + "glyph:<g>"
  against the core's grammar; empty AND malformed expressions degrade to
  "show everything" (mid-keystroke typos must not blank the view). The
  coming canvas filter box, the table, and host widgets all match
  identically.
- **Palette categories** (Hormiga §3's first rung): `AddPalette::Entry.
  category` (convention: a "category" key in glyph hints, copied by the
  host); the add box and right-click add menu group under SeparatorText
  headers, first-appearance order, uncategorized first; category-less
  palettes render exactly as before. Drag-from-panel ACCEPTED as a library
  gesture, queued [vh] behind the table (needs cross-panel drag plumbing +
  a snap path for not-yet-minted nodes).
- **The host-view seam blessed** (Hormiga §4, views-as-projections.md):
  a third-party view = project_scene in, compiled commands out,
  node_matches for filtering, viewport as config-tier `view.*` state.
  `EditorState` is explicitly NOT part of the seam (it is the 2D canvas's
  own gesture state). The table view will prove the contract first-party.
- **Q12 opened** (the author's own question, relayed via Hormiga §1):
  ImGui-composed kit vs sanctioned wrapped-toolkit (Qt-class) adapter.
  Lean, strongly: ImGui-composed is the one sanctioned path (matches
  Hormiga's lean; Qt fights vendoring, the render loop, and the NDK path);
  the contract stays adapter-compatible but no bridging ships without a
  sanctioned client. T3's "Qt widget adapter" parked behind it.

Concept true-ups: widget-registry.md (the protocol section answers the
three deferred questions), views-as-projections.md (host-view seam),
backlog ([vh] queue), index (message pointer). Reply updated at
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA.md` — §2 (table) is next on the
[vh] queue, unchanged. All 5 suites green (editor-hint + filter pins
added); InteractionCombinators and NodeBlocks rebuilt clean against the
new headers (IC palette entries updated for the new field).

## 2026-07-16 (cont.) — VLS-native's first message: the visual-identity batch

`MESSAGE_FOR_VOIDMAIZ.md` turned over the same day: **Void Loops Studio
(native)** — the client the library was founded for — sent its first
message, from the author's "makeover specific to the needs of VLS" charge.
They built everything the host surface already carried (a full CanvasTheme,
a rotary knob face widget) and asked for the three gaps only canvas.cpp can
close, plus a convention and a seconding. All landed same-day:

- **The port style registry** (§1): `CanvasStyle::port_types` — host type →
  `PortStyle{color, shape}`, shapes from `PortShape` (circle / square /
  diamond / triangle / ring). Shape is the second channel BY DESIGN (their
  argument, adopted verbatim: color must never be the only signal —
  color-vision deficiencies, dim jam rooms). One `draw_port_marker` now
  serves every body kind — window port rows, shaped perimeter anchors,
  block value sockets — so the registry cannot be half-applied; the port
  hover ring takes the declared color too. Undeclared types keep the hashed
  palette + circle; a declared entry with color 0 keeps the hashed color
  but takes the shape.
- **Typed wire styling** (§2): drawn linguine take the from-port type's
  declared color; when the from end is an untyped principal (a passive
  principal→aux wire) the to-port's type reads instead. The PENDING wire
  shows its type color in open space and hands over to the verdict tint
  (ok/bad) at a candidate port — what you're dragging vs whether it's
  legal, each at the moment it matters. Adjacency tethers stay theme-dim
  (deliberate: a tether is an honesty affordance, not a signal path).
  Per-type width/dash: noted as unasked niceties, not built.
- **`p:<field>` blessed** (§3): the kit's disabled-when-wired check matches
  both `<key>` and `p:<key>` (params-as-ports, proven in the Python VLS
  #14a) — one change in widget.cpp's `field_is_wired` covers the inspector,
  faces, forms, everything, since yesterday's consolidation. The
  `hints.bind` alternative stays unbuilt unless someone needs a third
  spelling.
- **`face_knob` absorbed** (§4, their offering): `widget_field_knob` +
  `"knob:min,max,default,snap"` editor kind + `face_knob` face frame —
  270° sweep, vertical-drag staging (one command on release), double-click
  reset, integer snap, wired = disabled + tooltip. Kit adaptations from
  their implementation: commits via commit_value (a string-typed field
  stays a string — theirs always emitted setjson), colors derive from the
  ImGui style (SliderGrab accent) so the knob wears any host's theme;
  unset/null fields read as vdefault (kept from theirs).
  **ADL trap found by building VLS against the result**: `vls::face_knob`
  and the new `maiz::face_knob` have identical signatures and FaceContext
  pulls maiz:: into ADL — every unqualified call went ambiguous. Fixed in
  VLS (sessions are shared per CLAUDE.md): calls qualified `vls::` — their
  knob stays theirs until the VLS agent adopts the kit's (reply flags the
  SliderGrab theming note for that day).
- **Multi-inlet spare slots** (§5): [vls]-seconded in the backlog; still
  queued (their milestones 2–3).

Also: NodeBlocks' palette got Scratch-style categories (motion/pen/control/
events) — dogfooding yesterday's category hint and clearing the
missing-initializer warning the new field introduced there.

Reply drafted at `../VoidLoopsStudio/MESSAGE_FOR_VLS.md`. Verified: Maiz
5/5 suites, VLS-native 6/6, InteractionCombinators + NodeBlocks rebuilt
clean.

## 2026-07-18 — The widget protocol's first bruises: labels, path, bool, batch helper

Void Hormiga's THIRD message (`MESSAGE_FOR_VOIDMAIZ.md`, drafted 2026-07-16,
the "now bruise it" reply we invited) — its Data section is now real (kind
sidebar → rune list → detail pane, all through `hints.editors` +
`draw_inspector(scene, ed, &registry)`; the Builder gained a categorized
palette and an HTML `effect render` preview). The protocol held; four edges
it hit, ordered by how much the Data section wanted them. All four closed
this session, exactly the "these gate our kit backlog, not yours" contract:

- **Per-field labels [§1.1, the biggest volunteer-facing gap]** — a form
  showed raw field keys (`text_en` where a volunteer needs "Text (English)").
  Fixed the way `editors` was: a `hints.labels` map, glyph-declared, surfaced
  as `SceneField.label` (project.cpp `fill_fields`), threaded through the kit.
  Every field widget now shows the label with the ID kept keyed on the field
  key (`"label##key"` — relabeling never resets a live edit); faces pass
  nullptr (a node-face label is inline chrome), the registry threads
  `SceneField.label`. Empty/absent → the raw key, unchanged. ONE glyph
  declaration lights it up on every surface (inspector, forms, faces, table
  cells when they land) — the same one-registration payoff as editors.
- **A `path`/file kind [§1.2]** — `image.path`/`resource.path` rendered as
  staged text. New `widget_field_path`: a staged text box (edit by hand, ONE
  `set` on commit, like text) PLUS a "…" browse button. **The library owns
  the box and the commit; the HOST owns the OS file dialog** — a
  `PathBrowseFn` callback returns the chosen path (empty = cancel) and the
  widget commits ONE `set`. Deliberately NOT in `defaults()` (the library
  ships no portable dialog — the same boundary as `face_image` returning an
  image click to the host); `WidgetRegistry::add_path(browse)` wires it in in
  one line, then file pickers appear on every field surface uniformly.
- **A bool/checkbox kind [§1.3]** — `widget_field_bool`: a checkbox over a
  boolean field, registered under both `"bool"` and `"checkbox"`. A click is
  atomic (no staging — nothing to abandon) and commits ONE `setjson <key>
  true|false`; disabled-when-wired like every field editor. Serves Hormiga's
  `status:`-flag workflows.
- **The multi-field batch commit helper [§1.3]** — `compile_commit(commands)`
  (gesture.cpp): the single/many/none discipline `compile_moves`/
  `compile_deletes` already used, exposed for hosts building compound flows —
  "" for none, the lone command for one, a `batch` for several. Hormiga's
  `+ New` flow (a `rune new` + `tag` that dispatched as two frames) now lands
  as ONE undo frame: `compile_commit({"rune new person p1","tag p1 +draft"})`.

Number formatting/units: not built — Hormiga confirmed it hasn't needed them
(§1.3), so the reply's fourth expected gap stays open until asked.

**§1.4 (not a bruise, a note):** `draw_inspector` with a registry turned out
to BE Hormiga's detail form — `widget_form` went unused because the inspector
already adds the chrome (tag chips). Confirmation the concept works as
written; when the table lands with the same registry plumbing, their whole
Data section is registry-fed with no host widget code beyond chrome.

**§2 working note recorded (no code):** `node_matches`'s "malformed
expression matches all" rule reads right in a filter box but is the wrong
default inside a *query block* (a typo'd query should render "(no events
match …)", not the whole database). Hormiga guards it host-side at their
phase-D renderer and accepts the permissive default for now; flagged for the
day the filter grammar grows a strict-parse entry point (which they'd use
there). Left as a note against `filter_bag`/`node_matches`; no library change.

Tests: `hints.labels` projection pinned in `project_smoke` (label present →
value, absent → empty); `compile_commit` none/one/many pinned in
`gesture_smoke`. Raw-string trap noted in the test: a label containing `)"`
(like "Text (English)") closes a `R"(...)"` literal early — the pin uses a
paren-free label; real glyph JSON is unaffected. All 5 suites green (reduce
14/14 direct — the ctest `0xc0000139` stays the MSYS2 DLL-search quirk);
InteractionCombinators, NodeBlocks, and VoidHormiga all rebuilt clean against
the new headers (additive signatures — optional trailing `label`, new
symbols — so no caller changed). Reply drafted at
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA.md`; concept trued up
(widget-registry.md), backlog [vh] updated, index message pointer moved.

## 2026-07-18 (cont.) — Brainstorm captured: the UI-authoring horizon (Allmusely, Kingsterson)

Alongside the message work, the author opened a design conversation about a
**UI/UX authoring tool built on Void Core + Void Maiz** — the motivation being
that LLM-generated GUIs are sometimes weak and there's no interface to tweak
them, so a tool whose design IS a Void Core state document lets an agent
generate a UI AND a human edit the same artifact through the same
dispatcher seam. Two paths were weighed: **VMF** (a wrapper that imports
Figma/Qt-Designer exports) vs **Allmusely** (our own tool). Recorded in
[horizons](/horizons.md): VMF is set aside (a dead export in another tool's
model can't be log-first or round-trip our tags/interactions); **Allmusely**
is the lean, scoped as a Void Maiz *authoring surface* (place real
widget-registry components, not rectangles), NOT a Figma-class graphics
editor. Its sibling **Kingsterson** is the XR authoring tool (Void Maiz XR's
application builder — spatial layouts and gestures as first-class); latinOS's
default apps would be authored in these. The FaultSack boundary was drawn:
FaultSack is inward/analytic (study existing apps), Allmusely is
forward/generative (author new UI), sharing at most an annotation/tag
substrate. **No code and no new project — captured so the direction isn't
lost** (ground rule 1: concept before code; these become their own OKFs when
they start). The widget registry already names this as its far-horizon "host
application," never library.

## 2026-07-20 — Void Hormiga's fourth message: the geographic camera, and docking enabled

Two forward-looking asks (a fourth workflow, **Territory** — a map — is
coming, and the fixed shell strains under four canvas/editor sections).
Neither blocked Hormiga today; both weighed and answered.

**§1 — a geographic/spatial canvas view.** The custom-view seam (2026-07-18)
already blesses "project_scene in, compiled commands out, viewport as
config-tier `view.*` state." Territory's map is that shape with one twist:
the coordinate space is geographic (lat/lon), not node-layout. The honest,
substrate-respecting answer (substrates.md's quarantine: no dimension-/geo-
specific types above the view module): **the `Camera` is ALREADY substrate-
shaped** — three floats, a viewport, not pixels. A geographic viewport IS a
`Camera` (x=lon, y=lat, zoom=map zoom); the lat/lon→screen projection, the
base-map raster, tiles, and markers are all the host's (as Hormiga guessed —
"we build the map view host-side against Scene, like your table"). The one
real library gap: `compile_camera` **hardcoded** `view.camera`, so a SECOND
view couldn't persist its own viewport through the blessed config-tier helper.
Fixed: `compile_camera(cam, config_key = "view.camera")` — a map flushes to
`view.map.camera`, a table to its own key, same undo-exempt config-tier
pattern (Q#3's charge to keep `view.*` substrate-shaped, realized). Precision
raised to `%.7g` so lat/lon survive the round-trip where the node camera's
integer pixels did (float noise still trimmed). **Ruling on Hormiga's "3D map
→ Void Maiz XR?" question:** a flat map is **2D-in-nature** (2D-euclidean
interaction, geographic domain) → it stays **Void Maiz**; a 3D globe/terrain
map would be Void Maiz XR. Test-pinned in `gesture_smoke` (node camera format
updated to `%.7g`; a keyed geographic camera round-trips). No marker/geo code
in the base library — that would leak geography into the substrate-free core.

**§2 — movable/floating/re-dockable panels (FL-Studio-style).** Four heavy
workflows strain the fixed shell; the author wants "everything a window." The
vendored ImGui was **master** (docking disabled), so Hormiga (rightly) would
not fork the vendor themselves. **Author's ruling (asked and answered this
session): enable ImGui docking** — the FL-Studio experience, using the library
we already vendor, beats hand-rolling a pane manager (which would be *more* of
a framework, reinventing docking worse). This **reverses the standing Q11 "no
docking framework" lean** — made when one host had a trivial layout; Q11's own
gate ("a second host with a real need before the pane shape freezes") is now
met by Hormiga's four workflows. Executed:

- **Re-vendored ImGui `v1.92.1-docking`** (exact version match). The vendored
  master was first confirmed **unpatched** (diff-clean vs stock v1.92.1 modulo
  whitespace/eol — all 18 core+backend files), so the swap was wholesale;
  CRLF→LF normalized to the tree's convention. `IMGUI_HAS_DOCK` now present;
  `imconfig.h` stays stock (docking is a runtime `io.ConfigFlags` bit, not a
  compile define). vendor/README.md row trued up.
- **DockSpace only — multi-viewport deliberately OFF.** It cannot work on a
  single-surface mobile/NDK target, and the node canvas assumes one OS window.
- **Library helpers (widgets.hpp/cpp), the "one rung past splitter" Q11
  gated:** `enable_docking(on=true)` (sets `DockingEnable`, never viewports;
  no-op-safe without docking), and `begin_dockspace(id)` / `end_dockspace()`
  — the fullscreen host window + `DockSpace` boilerplate every host repeats,
  returning the dock id for a host's DockBuilder default layout. The host
  still owns WHICH panels and their arrangement (Q11's "host owns layout"
  holds); the library owns only the repeated boilerplate. Layout persists in
  ImGui's `.ini` (host-side preference, like the recents list).
- **`canvas_window` example converted** from three fixed windows to a
  DockSpace with a one-time DockBuilder default (Canvas center / Inspector
  right / Log bottom, seeded only when no saved `.ini` layout exists). Panels
  are now movable / floating / re-dockable. Verified: the app launches and
  runs the docking path without crashing (a live-author-safe liveness check —
  no scripted input, the on-screen drag-to-dock left for the hands-on loop,
  per the Node Blocks precedent when the author is at the machine).

Verified: Maiz 4/4 smoke suites green (reduce untouched, 14/14 direct as
ever); **InteractionCombinators, NodeBlocks, and Void Hormiga all rebuild
clean against the docking-branch ImGui** — the vendor swap is safe downstream.
Q11 moves to Decided; developer_questions, backlog, views-as-projections and
substrates trued up; reply drafted at
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA.md`; index message pointer moved.

## 2026-07-21 — Void Hormiga's fifth message: canvas actions as first-class commands

A forward-looking DESIGN ask (no blocker; "the SHAPE matters more than the
timing"): make a custom view's **interaction vocabulary** first-class — named,
param-schema'd, introspectable — so a map's place/move/draw-region/select-in-
radius tools are legible to humans, the CLI, and agents, and one definition
drives both a gesture and a CLI verb. Read "map" as one instance of a general
need (timelines, patch editors, a fantasy-map tool).

- **§0 (confirm, don't build): the WRITE side is already first-class.** A map on
  the custom-view seam emits ordinary Core commands (`set <rune> geo`, `rune new`
  + `set geo`, a region = a rune with a shape facet), all logged/undoable/
  replayable, and an agent reproduces them through the same verbs. Positions were
  always model content; geo is another facet. Confirmed in the reply — no new
  write path; §1–§3 are the deltas.

- **§1 shipped as a DRAFT — `voidmaiz/action.hpp` + `src/action/action.cpp`**
  (base `voidmaiz` library, UI-free, in the widget-protocol tradition of
  shipping a draft the forcing client bruises). An **`ActionDescriptor`** =
  `{ name, label, doc, params[], gesture, compile }`: `params` is the schema
  (`{name, type, required, doc}`, `type` a host convention surfaced but not
  interpreted); `compile(scene, args) -> command line(s)` is the host's (the
  library never invents a command — total observability); `gesture` is a
  view-interpreted hint surfaced for discovery (the library never dispatches a
  gesture — that stays the view's, like `EditorState`). **`ActionRegistry`**
  (host-owned like `WidgetRegistry`): `run(name, scene, args)` = the ONE entry
  point a gesture handler and a CLI verb both call (looks up + compiles +
  returns commands; the host dispatches + re-projects), and `manifest()` emits
  a JSON list of actions+schemas — the introspection an agent reads to discover
  the vocabulary. Domain-agnostic (strings + a `Scene→strings` fn); no map/geo
  type enters the library (substrates' quarantine holds). Pinned in the new
  `action_smoke` suite (run, decline-on-missing-arg, the manifest).

- **§2/§3 relayed upstream — fresh `MESSAGE_FOR_VOIDCORE.md`** (the first open
  Core ask since the 0.2.4 reduce adoptions). The Core-side of "one definition,
  two front-ends": (1) **host-registered CLI verbs** so an `ActionDescriptor`
  backs a real dispatcher verb (the second front-end; until then a host command
  bar can fake it losslessly by mapping tokens into `ActionArgs` and calling
  `run`), and (2) **host-registered query predicates for `ls`/Scry** — Hormiga's
  `--near`/`--in-region`, generalized to a pluggable predicate the Scry
  evaluator calls (spatial is the instance; distance math stays host compute,
  Core owns only the seam — the quarantine holds on both sides). Recommended to
  Core exactly Hormiga's framing: "Core grows host-registered verbs/predicates
  and Void Maiz binds gestures to them" — we care about the one-definition
  property, not where the registry lives.

- **§4 boundaries acknowledged**: map view / projection / base-map / tiles /
  markers / rasterization all Hormiga's; reactive visuals via Scry →
  responsiveness hook (no new ask). They are the forcing client for both the
  geographic view and this generalization.

New concept: [canvas-actions](/concepts/canvas-actions.md) (status:draft). All
5 smoke suites green (embed, project, gesture, replay, action; reduce untouched,
14/14 direct). Reply at `../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA.md`; index map +
message pointers updated (both the Hormiga reply and the new upstream Core ask);
backlog [vh] extended.

## 2026-07-21 (cont.) — Core answers the canvas-action asks; the message-titling convention lands

**Void Core replied** (`MESSAGE_FOR_VOIDMAIZ_core-reply-2026-07-21.md`),
blessing BOTH asks as one seam shape (*"Core owns the seam + calling
convention; the host owns the domain computation; the output rides the
observable spine"*):

- **§1 host-verbs → verb macros (compile-to-`batch`).** Register `{verb-name,
  arg-spec, compile(argv) -> [command lines]}`; the core parses, calls the
  host's compile, dispatches as a **`batch`** — atomic, one undo frame,
  attributed, logged, replayable by construction, and the SAME compile the
  gesture calls (one definition, two front-ends). Semantics are available
  **today**: a host command bar calling `batch` with the compiled lines is the
  real spine, not a fake — verb-table registration is the only delta (buys
  `man`/`?` discoverability, voidscript composability, parse-once). **Hard
  rule:** a verb macro emits mutations → pure → `batch`, and must NOT use the
  effect seam (that's for effectful, non-undoable host I/O). Converges with
  planned voidscript `def`; **Core recommended the action param-schema and the
  verb arg-spec be ONE type** — adopted (noted in `action.hpp` + the concept).
- **§2 query predicates → pluggable `where`-predicates** in Scry/`ls`, composing
  with the tag grammar. Interim today: `effect query "<expr>"` (host-side,
  non-composable). Boundary holds: Core owns the seam, host owns distance math.

Trued up: `canvas-actions.md` (the boundary section is now "answered", + the
one-type param-schema/arg-spec consequence), `action.hpp` (the design note),
backlog [vh] ("one definition → two front-ends" now Core-blessed, interim via
`batch` available today), the Hormiga reply (Core's rulings relayed).

**Message-titling convention adopted (author's direction).** Old generic files
(`MESSAGE_FOR_VOIDMAIZ.md`, `MESSAGE_FOR_VOIDCORE.md`) let an agent mistake a
stale message for the live one — and with several agents relaying at once, a
bare inbox name collides. New rule: **every inter-agent message is uniquely
titled** — `MESSAGE_FOR_<RECIPIENT>_<sender>-<topic>-<YYYY-MM-DD>.md` with a
self-identifying H1 — and the OKF index tracks open vs consumed (unique titles
let dated messages coexist as history; retire by the index, not by deletion).
Codified in **CLAUDE.md ground rule 4** and **index.md §"Inter-agent
messages."** Applied: our Hormiga reply re-issued as
`MESSAGE_FOR_VOIDHORMIGA_maiz-canvas-actions-2026-07-21.md` (old generic one
retired); our consumed `MESSAGE_FOR_VOIDCORE.md` ask is answered and retired
(Core said it could be); Core's reply is kept (uniquely titled = safe history).
Hormiga's own outgoing draft copy in their repo stays theirs to retitle — the
convention propagates to sibling agents via their CLAUDE.md.

## 2026-07-24 — The surface census: the GUI documents itself

**Author's direction:** *"I think we need an engine or something in order to
register UI and its shape, its flow, etc, into the OKF… this way we can look at
the conceptual behavior of the application separately from the GUI… there's
already some applications that are completely built with Void Maiz, so I don't
wanna have to rewrite EVERYTHING to make this work."*

**The reframe that made it small: don't write markdown.** Void Core's OKF
engine (`VoidCore/okf/components/okf-engine.md`, `holidays/okf/`) already has
**produce — mantle → conformant bundle**, proven lossless (19 concepts + 92
links round-trip identical), with the mapping we want already in place
(concept ↔ rune `okf-concept`, concept id ↔ `spirit.name` with `/` splitting
into directories, type ↔ a `type:` tag, body ↔ `content.body`, link ↔ layout
edge, bundle ↔ mantle). So the missing piece is not a writer but a **census**:
harvest a running GUI, emit the dispatcher commands that build the concept
runes, hand the state document to the existing producer. Void Maiz ships no
documentation format, and inherits `validate` / tag-filtering / `analyze` /
FaultSack study for free.

Keeps every ground rule: the census **owns no truth** (it emits commands), and
**documenting yourself is a gesture, therefore a command** — the markdown is a
projection of a mantle exactly as the canvas is a projection of the model.

**The no-rewrite answer is a four-tier ladder**, and tier 0 costs a shipping
app zero lines: `glyphs`/`mantles`/`describe`/`links`/`help` off a live core
yield the whole node vocabulary — ports, editable fields, and which widget
edits each (the glyph `hints` were always carrying this) — and the live ImGui
window/dock tree yields the pane shape (reaching into `imgui_internal.h` is
sanctioned precedent: the DockBuilder default layout). Tier 1 is one line per
registry the host ALREADY builds — `ActionRegistry::manifest()` (built
2026-07-21 with no consumer; the census is its first writer), `WidgetRegistry`,
`FaceRegistry`, `AddPalette`. Tier 2 is `SurfaceNote` — intent, from a human,
the only thing reflection cannot recover; **the census never invents prose.**
Tier 3 is **flow observed rather than declared**: attribute each dispatched
command to the surface that emitted it (the log sink hosts already install) and
a session yields a real gesture → command → effect table — total observability
paying a documentation dividend, and `confidence:verified` in the literal
sense because it was watched.

**Standardization stayed at four items, none of which touch the OKF engine:**
no new glyph (reuse `okf-concept`, type rides as a `type:` tag — OKF mandates
tolerating unknown types), names are paths, a free-tag vocabulary
(`surface:*`, `generated`, plus the standard axes; `confidence:` values stay
inside the glossary-enforced set), and links carry the flow so the bundle graph
IS the UI graph.

**Regeneration solved structurally, not by text merging:** because the truth is
a rune and not a file, harvest lands in `content.body` and human prose in
`content.notes`, and `produce` emits body-then-notes — a re-census overwrites
the harvest and cannot touch the commentary. No markers, no merge. This is the
one genuine upstream ask (a `notes` append in `produce`); it also answers an
open Core question — `ui-ux.md` (`status:planned`) asserts every app must
describe its UI/UX and leaves *how* open. Message goes out once the design is
trued up (ground rule 4 — messages, not edits).

New concept: [surface census](/concepts/surface-census.md) (status:draft), with
the contract sketched in `voidmaiz/census.hpp` (draft header, UI-free base
library; the view-module registries arrive as incomplete types, `harvest_panes`
lives in `voidmaiz_view` — the action.hpp split). **Not implemented** — no
CMake entry, no smoke suite yet. New questions: Q13 (trigger: in-process action
vs `--census` headless frame vs continuous log-sink ride — lean: action +
continuous, headless falls out) and Q14 (where the bundle lands — lean: the
app's own OKF under a reserved `okf/surfaces/`, everything tagged `generated`).

## 2026-07-24 (cont.) — The census broadcast: three agents, one of which had never heard of us

The author asked for a message to sibling agents, **written so an agent that has
never heard of Void Maiz can still use it** — because one recipient hasn't.

**FaultSack** (`../FaultSack`) is the author's Void Core application analyzer:
a unified graph of OKF concepts + code, centrality-ranked "what's most
responsible", a generated gamified study site, and the headline — the user
highlights anything they disagree with, attaches a **developer note**, and
exports those notes as markdown back to the agent that built the project. It
**predates Void Maiz and its OKF never mentions us.** It is also the *reason*
the census exists: the author's motivation, verbatim — *"the main reason I
wanted a dedicated OKF for UI and UX concepts… I would love if I could annotate
actual UI elements themselves, but I think I'll have to wait until we fully
develop Allmusely for that."*

**The finding that softens that wait:** FaultSack's notes key off `node_id`
(`config.py:140`, `server.py:221`), and a note's node may be ANY graph node —
including an OKF concept node (`okf:<concept-id>`). Its `find_bundle` already
prefers `<target>/okf`. So a census bundle is **annotatable UI on the day it
exists, with no FaultSack change**, at surface / glyph / action granularity.
Allmusely remains the answer for widget-and-pixel granularity; concept
granularity lands now. That is a genuinely earlier payoff than the author
expected, and it is why the census's concept-id convention
(`surfaces/…`, `vocabulary/glyph/…`, `actions/…`, `flows/…`) matters: those ids
are what a human will be annotating.

**Suggestions sent to FaultSack** (offered, not asked — they own their tool),
from reading their code today:

- **They drop the OKF tags and types they already parse.**
  `ingest/okf_graph.py:99-109` builds a `Node` from a `bundle.Concept` keeping
  only title/description/body/resource; `Concept.type` and `.tags` are parsed
  by the holiday and thrown away, and `Node` (`adapters/base.py`) has no field
  for them. Void Core's whole **honesty convention** lives in those axes, so
  their analyzer currently cannot ask "what is declared but not built?"
  (`status:planned`), "which claims are shaky?" (`confidence:exploratory|stale`),
  or "show me only the UI half" (`type:Surface`). Two fields, large blast radius.
- **They never run the OKF holiday's `validate`** — though they already import
  `bundle`, `manifest`, and `theme` from it. Its warnings (a `status:current`
  concept with no `resource:`; a `status:planned` tag over a body claiming built;
  resource freshness drift; off-vocabulary `confidence:`; index bullets
  disagreeing with their target's status) are **ready-made faults with node ids
  that already exist in their graph**. Pre-seeding them as machine-authored
  developer notes means the human opens the study site to a list of real
  findings instead of a blank sack — pure pipeline, no LLM, exactly their brief.
- Plus: a doc-drift signal only they can compute (concept `timestamp:` older
  than its bound file's mtime), centrality over a graph that now contains UI
  ("which surface is the chokepoint?"), and a minor note that
  `bind._norm_resource` silently drops sibling-repo `resource:` paths — common
  in this family, and a broken cross-repo pointer is itself a finding.

**The non-client boundary was re-stated before the news**, so the message can't
read as a pitch: FaultSack stays on the core directly
([scope](/concepts/scope-and-clients.md)); the only door held open is
`voidmaiz/embed.hpp` (the standalone RAII layer, no rendering). The author's own
inward/analytic vs forward/generative line (FaultSack vs Allmusely) was quoted
as-is.

**Void Hormiga** got the same news with a different ask: tier 0 costs them zero
lines, and their Territory `ActionRegistry` is the richest census input that
exists — `manifest()` (shipped for them 2026-07-21 with no consumer) is already
a documentation-grade description of an interaction vocabulary. So: keep
registering actions instead of hand-rolling gesture handlers, and fill in
`doc`/`label` — **a registered action is a documented action; an anonymous
gesture handler is invisible to the census by construction.** Q13/Q14 leans put
to them as the most demanding client.

**Void Core** got the ask (`produce` → append `content.notes` after
`content.body`; backward compatible, byte-identical when absent, and the general
fix for any produced bundle's machine-half/human-half problem) plus our answer
to their open `ui-ux.md` question: *how* a GUI app declares its UI/UX is a
**harvested sub-bundle beside `app.md`** — not a manifest field, not a
hand-written design page — which satisfies their own "readable statically, from
files, without running the app" requirement even though a running app generated
it. Explicitly not proposed as a spec change.

All three are uniquely titled per the 2026-07-21 convention and tracked as
**open** in the [index](/index.md).

## 2026-07-24 (cont.) — Two replies land: `notes` is shipped, FaultSack rebuilt itself, and two of our claims were wrong

Both messages came back the same day. Neither carried an ask; both carried a
correction, and one carried a version note that opens a decision the OKF has
been holding since founding day.

### Void Core — the `notes` ask is built, and they went one step past it

`MESSAGE_FOR_VOIDMAIZ_core-notes-field-and-ui-ux-2026-07-24.md`. Shipped in
`holidays/okf/` the same day, in the shape we proposed, plus **the mirror we
left to their call**: `consume` now splits the halves back apart, so the
round-trip is field-level lossless rather than asymmetric. Contract: separator
`<!-- okf:notes -->` **recognized only on a line of its own** (their new test
caught an inline mention of the marker splitting `okf-engine.md` in half — a
case census pages will hit, since they document things); no notes means
byte-identical output; no marker on read means the whole file is body; links
inside `notes` join the graph and `validate`'s lint; `notes` declared on the
`okf-concept` glyph so authoring is the ordinary `set <c> notes "…"`, with **no
core or dispatcher change**.

They accepted the generalization rather than the convenience — *"any produced
bundle has a machine half and a human half, so the fix belongs in the engine and
not in every producer. We'd have taken this ask from a stranger."* Trued up in
[surface census](/concepts/surface-census.md): the regeneration section now
documents a built contract instead of an ask, and the boundary section credits
the split to them.

**Our `ui-ux.md` answer is recorded** on their page as the leading candidate for
the open *how*, with the three properties in our order, and **deliberately left
`status:planned` on our own advice** — one instance can't distinguish "host-
neutral shape" from "node-editor shape we haven't noticed is a node-editor
shape." Their page now names a second, non-GUI harvest as the explicit promotion
gate. Their reading of what mattered is worth keeping: we had framed their
question as "manifest field or design page?", and both are things an app
*asserts*; the census is a thing *read off* the app. *"That reframing is the
contribution, more than the census is."*

Also confirmed: `census` is a **verb macro** by their 2026-07-21 ruling (pure,
mutation-only, compiles to `batch`, never the effect seam) — blessed shape in
their `/design/host-extension-seams.md`, and nothing about the census waits on
verb-table registration.

### The version note that opens Q15

Core flagged that **0.2.5 has landed** with `place` + the view slice, and that
positions in `rune.placement` are now *"the sanctioned pattern rather than a
suggestion"* (0.2.6 adds `mantle rm`/`rename`). That is precisely the sign-off
[total observability](/concepts/total-observability.md) has been waiting on
since 2026-07-09 — the page's own words: *"view writes must be distinguishable
commands, so the gesture compiler can retarget them to `place` the day it
lands."* The day landed.

Mechanically it's small: we already read `placement` first
(`src/project/project.cpp:320`) and every position write funnels through one
compiler (`compile_move`, `src/gesture/gesture.cpp:63`, emitting
`setjson <name> pos [x,y]`). **What makes it the author's call is that it takes
node moves OUT of undo** — which is the point (the original complaint was `undo`
popping a *move* when the user expected a *note*) but is a visible behavior
change in three shipping apps. Filed as **Q15** with a lean to retarget and take
the change; the floor bump 0.2.4 → 0.2.5 is gated on the same answer. Not
implemented — no code touched.

### FaultSack — four of five suggestions built, and two corrections for us

`MESSAGE_FOR_VOIDMAIZ_faultsack-suggestions-built-and-census-ingest-notes-2026-07-24.md`.
They built §4.1, §4.2, §4.3 and §4.5 in one session (62 → 86 tests) and
confirmed §4.4 needed nothing. Notable beyond the ask:

- **§4.1 was worse than we said** — `okf_graph.py` read `concept.body` rather
  than `concept.text`, so **Void Core's brand-new `notes` half would have been
  invisible to the analyzer**: a produced bundle's human commentary simply would
  not have existed in the study site. Nodes and cards now carry `okf_type`,
  `okf_tags`, `timestamp`, `notes`, `resource`, `resource_kind`, with declared
  tags kept off the NLP keyword field (a test asserts it) — *"what a project
  claims about itself and what its prose looks like are different kinds of
  evidence, and only the first can be wrong on purpose."* Their filter rail is
  built from whatever tags a bundle declares, not a hardcoded axis list, so a
  census emitting `type:Surface` / `surface:canvas` yields a working "study only
  the UI half" filter with **no FaultSack change**.
- **§4.2 authorship is enforced at the seam, not by convention** — `Note.author`
  is `human|machine`, and the HTTP surface *ignores* a client-supplied `author`,
  verified against a live server. They dropped `Report.info` from seeding on
  purpose (the spec-tolerated broken-link class): *"a study site that opens on
  noise is worse than one that opens empty."*
- They dogfooded it: 9 genuine findings against their own bundle (fixed, re-run
  clean), 9 against Void Core, **12 against ours**.

**Correction 1, and it was our error to publish:** `surfaces/` is **not
reserved**. The engine's `RESERVED` set is exactly `{index.md, log.md}` — two
filenames, nothing directory-shaped. Our developer-question Q14 said "reserved
subtree", which is a `status:current` claim the engine doesn't back — the exact
drift the honesty convention exists to catch. Fixed in Q14 and stated explicitly
in the concept's boundary section. (Footnote from them: `app.md` isn't reserved
either, so a manifest loads as an ordinary concept *and* through
`read_manifest` — it will appear as a study card.)

**Correction 2 — `resource:` and `timestamp:` are now non-negotiable census
output**, promoted to items 5 and 6 of the standardization (was four items):

- Without `resource:`, *"a census is a well-formed subgraph attached to
  nothing"* — it is what binds the harvested UI half to the code half in their
  unified graph, and therefore what makes "which UI surface is the chokepoint"
  mean anything. It is also what keeps generated concepts off the honesty rule,
  which fires on exactly `status:current` with no `resource:` — and **every
  census concept is `status:current` by nature**, so an unattributed census
  would bury a study session under warnings. The library can't know these paths;
  added `CensusOptions::resource_for` (concept id → source path) as the one host
  hook, plus a per-concept override on `SurfaceNote`.
- Without `timestamp:`, a concept is invisible to drift detection forever —
  *"a shame given a census is the one kind of doc that always knows exactly when
  it was true."* Already in `CensusOptions`; now documented as required, and why.

They also asked us to keep prose in `notes` (which Core had shipped hours
earlier — their renderer separates the halves) and to use `confidence:verified`
on observed flows (*"the highest-value card in a study session, and the only
place that value is currently expressible"*).

### Our own honesty debt — one fixed, the rest deliberately not

`references/voidnode-draft-v1` declared `confidence:speculative`, which is
outside Void Core's glossary vocabulary — corrected to `exploratory`.

The other 11 findings are all one rule: `status:current` with no `resource:`.
**Deliberately not bulk-fixed.** Seven have genuine backing code and four are
rationale/plan documents (`differentiation`, `scope-and-clients`, `backlog`,
`developer_questions`) that honestly have none — Core's validator exempts that
class by `type` (`_DOC_TYPES` = dictionary/reference/roadmap/manifest/design)
but our rationale lives in `concepts/` as `type: Concept`, so the exemption
doesn't reach it. And for the seven, adding a `resource:` without re-verifying
each doc against its implementation would just trade "no resource" warnings for
"stale" warnings — or, if the timestamp were bumped to today to avoid that,
would hide real drift behind a fresh date. That is a true-up session with actual
reading in it, not a frontmatter edit; left for one.

## 2026-08-06 — Allomone lands in Void Maiz: composition as a projection, and the user action graph

Void Hormiga's sixth message
(`MESSAGE_FOR_VOIDMAIZ_hormiga-allomone-to-maiz-2026-08-06.md`) proposed moving
**Allomone** — their derive-only scripting layer — up into Void Maiz as a
host-agnostic capability. **The author's call: build it here**, keeping the name.
New concept: [Allomone](/../../VoidAllomone/okf/index.md). Built this session, with the demo
that tests it. Hormiga adopts once it settles; they have not been told yet.

### The message, sorted against our own rules

Three asks, three different answers:

- **The composition engine — taken, as a PROJECTION.** Merging N annotation maps
  is a pure function of `(scripts, mantle) → annotations`, which is the `project`
  half of the view seam. Legal because it is re-derived and never stored.
  Described without the word "script" — *N maps in, one map plus a disagreement
  set out* — it passes the second-client test.
- **The interpreter — taken, quarantined.** The base library must not grow a
  language, so Allomone Script is **`voidmaiz_allomone`**, a sibling target like
  `voidmaiz_reduce`. The "not an execution engine" non-goal survives because
  Allomone executes nothing; it derives, and derivation is projection.
- **The Action Hypergraph — taken, reshaped.** Only Maiz sees gestures, so the
  observation point is genuinely ours — but hover and selection are ruled
  **ephemera, "Never state."** Resolution: the **census pattern verbatim** —
  accumulate in-session, `compile()` to commands. The graph becomes runes in a
  mantle, queryable *by Allomone itself*, instead of a private structure.

**The AST-as-node-overlay ask was declined for this pass.** Hormiga's own
2026-08-04 message proved `edit_canvas` is the wrong paradigm for syntax trees;
that is now three bespoke-view findings in a row (map, blocks, AST). The
generalizable thing is the **tree-shaped view** already named in
[views as projections](/concepts/views-as-projections.md).

### The mathematics: a lattice, not a topology

Their message offered a sheaf/cohomology framing and explicitly asked for it to
be evaluated rather than adopted. **Evaluated: the vocabulary is apt and the
machinery is not available.** Sheaf cohomology is defined for sheaves of abelian
groups; colours, icons and glyph names are not one, so there is no H¹ to compute.
(Michael Robinson's data-fusion program hits exactly this wall and answers it by
replacing cohomology with a *consistency radius* for sheaves of pseudometric
spaces.) What the topology predicts here is one object — the **disagreement set
indexed by (subject, property)** — which was the plan anyway. So the framing
earns nothing extra and its vocabulary is **not adopted**.

What does fit, and is a few hundred lines: the **join-semilattice / constraint
store** (Saraswat's concurrent constraint programming; the CRDT merge law).
A merge that is **commutative, associative and idempotent** IS the "no clock, no
run button, order irrelevant" property — a theorem of the algebra, not a
discipline anyone maintains. **Conflict is the join escaping to ⊤**; the merged
state stays total, and ⊤-valued cells *are* the conflict set. Resolving is
lowering ⊤ back into the lattice. `merge(A,B) == merge(B,A)` and
`merge(A,A) == merge(A)` are pinned by tests.

**The CSS cascade settles an internal Hormiga contradiction.** Their
`conflicts.md` (08-03) says overlap is a feature resolved **silently** by
specificity; the 08-06 message says conflicts must be **surfaced**. Both are
right about different scopes, and CSS is the shipped prior art: **within one
script, specificity wins silently** (a sharper rule beating a broad default is
the point of writing rules); **across scripts, surface it** (CSS's origin/layer
tier). So `strength` settles intra-source ties and ⊤ fires only when distinct
sources disagree at equal strength.

### What shipped

- **`voidmaiz/annotate.hpp`** + `src/annotate/` — `ConstraintMap`, six merge laws
  (`Unique`/`Max`/`Min`/`Sum`/`Any`/`All`), `Merged` with `conflicts()` and a
  contributor list per cell (the devtools computed-styles pane), `Resolution`,
  and `compile_resolution` so settling a conflict is a dispatcher command.
- **`voidmaiz/usergraph.hpp`** + `src/usergraph/` — affordances as nodes,
  **symmetric timeless** edges, frames as **hyperedges** (a triple is not three
  pairs), `Device` as a first-class type (pointer/touch/pen/gamepad/voice/xr —
  the "new type for mobile" the author asked for), and `compile()` on the census
  pattern. Precedent recorded: Eclipse Mylyn's degree-of-interest model, which
  weights by frequency **and recency**; we drop recency deliberately to keep the
  structure order-free, and accept that stale coherence accumulates.
- **`voidmaiz_allomone`** — tokenizer, parser, evaluator. A rule set, not a
  sequence. `with` is the responsive operator: it reads the USER (the coherence
  graph), not the data. Derive-only structurally — there is no mutating form in
  the grammar.
- **`voidmaiz/code.hpp`** + `src/view/code.cpp` — the from-scratch code editor
  ImGui's `InputTextMultiline` cannot be: token spans become **live widgets**
  (click a `#rrggbb`, the wheel opens on it and edits the script in place),
  keyed `kind → renderer` on the widget-registry protocol so the wheel in a
  script is the wheel on a field.
- **`examples/allomone_playground.cpp`** (`maiz_allomone`) — the two-tab test
  vehicle: the script workbench, and a playground of buttons/sliders/toggles/
  lists that are **runes**, right-clickable to add and subtract tags.
  Deliberately meaning-free: if the engine needed to know what a "contact" is,
  it would not be the general engine it claims to be.
- Three smoke tests (`annotate`, `usergraph`, `allomone`), all green.

### Two real bugs found by building on it

- **Docking has never actually worked.** `widgets.cpp` guarded all three docking
  paths with `#ifdef ImGuiConfigFlags_DockingEnable` — which is an **enum value,
  not a macro**, so the guard was always false and `enable_docking` was a no-op
  since the Q11 ruling of 2026-07-20. The checked-in `imgui.ini` hid it: it
  carries **zero** `DockNode` entries, so every "docked" panel has really been a
  floating window with remembered geometry. Fixed to `IMGUI_HAS_DOCK` (imgui.h
  line 36). Docking now genuinely engages.
- **The seed guard every host writes is a trap.** `DockBuilderGetNode(dock) ==
  nullptr` can never be true, because `begin_dockspace()` has already called
  `DockSpace()`, which creates the node. Both example hosts had it. Added
  **`maiz::dockspace_needs_seed(dock)`** (no node, or an empty un-split central
  node) so a saved layout still wins, and switched both hosts to it.

Also found: the core's argument tokenizer takes double-quoted text **literally**
(`src/dispatch/args.c`) — `\n` stays two characters and a `"` closes the token.
Multi-line values with embedded quotes must ride **single** quotes, where `\'`
escapes and everything else passes through. A whole seeded script was parsing as
one comment line until that landed.

### Open, and deliberately not done

Hormiga's four questions are the author's calls, filed as **Q16–Q19** in
[developer questions](/developer_questions.md). Their earlier messages of
**2026-08-03** (surface census) and **2026-08-04** (blocks) were left in
*Hormiga's* repo root rather than ours and are still unconsumed — the titling
convention fixed the naming, not the destination.

## 2026-08-06 (cont.) — Allomone becomes a folder: a base language, documented for its future hosts

The author's reframing, same day: Allomone is *"not just this large system of
emergent behaviors, but also a base programming language for other applications
to build on top of… each application that builds on void maiz is gonna need to
make a custom library or something to run a domain specific version of
allomone."* Plus the explicit charge to document **the mathematics with its
corresponding computational structures**, because *"this is a lot of
non-traditional non-linear thinking."*

So `concepts/allomone.md` was retired and replaced by **`concepts/allomone/`**,
eight documents, following Hormiga's own folder convention (`type: Concept`
throughout, each sub-page opening `"Part of [Allomone](…/index.md)"`).

### The reframing that drove the structure

Every page now declares **which of two audiences** it serves: us building the
kernel, or **an application building on it**. That second reader is new and is
the reason the folder exists — a page that only we read could have stayed a
page.

- **[host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md)** is the load-bearing
  new document: the four things a host MUST supply (subjects, properties + laws,
  what a property means, storage), the four it MAY extend (predicates, value
  kinds, inline widgets, non-script sources), the five it MUST NOT do, the shape
  of a `<yourapp>_allomone` library, and — first-class, not a footnote — **four
  honest ways to opt out**. Many apps will never expose a language to a user;
  that is a supported position, not a degraded one, and it is why the language
  is its own target.
- **[foundations](/../../VoidAllomone/okf/concepts/foundations.md)** pairs each mathematical
  idea with the structure that implements it, in a table, and enforces one rule:
  **no mathematics appears unless it pays for itself.** Join semilattices,
  constraint stores as closure operators, CALM/monotonicity, Datalog's EDB/IDB,
  subsumption, stigmergy, weighted symmetric hypergraphs — plus §7, the standing
  record of why the sheaf/cohomology framing was **evaluated and rejected** (no
  abelian group, therefore no H¹; Robinson's own programme abandons cohomology
  for the same reason), so nobody re-derives it. The one place it would pay —
  metric values, consistency radius, a conflict-severity slider — is named and
  gated.
- **[non-linearity](/../../VoidAllomone/okf/concepts/non-linearity.md)** makes the author's
  "avoid linear thinking" operational: four substitutions (sets for lists,
  lattices for reduction order, symmetry for direction, adjacency for
  chronology), **the costs stated honestly** (no stepping through it; no
  principled winner for incomparable rules; no decay; unfamiliarity), the three
  places linearity legitimately survives (the log is Void Core's time axis;
  framing policy may consult a clock while the record may not; authored
  resolutions), and **a five-point test** a proposal must pass. Two worked
  examples of the test firing are included, because the failures are subtle.

The rest: [composition](/../../VoidAllomone/okf/concepts/composition.md) (normative — every
tiebreak stated exactly, so a second implementation could reproduce it),
[language](/../../VoidAllomone/okf/concepts/language.md) (grammar, and why `or`/`else`/loops
are each absent), [user-graph](/concepts/allomone/user-graph.md),
[editor](/concepts/allomone/editor.md), and
[roadmap](/../../VoidAllomone/okf/concepts/roadmap.md).

### The honesty device

[roadmap](/../../VoidAllomone/okf/concepts/roadmap.md) exists so the other seven pages can
describe the design cleanly without anyone mistaking design for implementation.
Its rule: **if a claim elsewhere is not listed as built there, it is not built.**

It names **one blocking gap**: host-registered predicates. The kernel's predicate
set is a fixed enum, so a domain language cannot add `when overdue "30"` — which
is precisely the seam between "Allomone is our engine" and "Allomone is a base
language other applications extend." The design is settled (Void Core's
host-registered query-predicate ruling, applied verbatim) and unbuilt. The
workaround — encode domain vocabulary as tags, so `when tag "overdue"` works
today — is a real stopgap and not a reason to defer it.

Also captured: the purity requirement that makes the whole thing hold. **A
host-supplied predicate must be pure and a host-supplied merge law must satisfy
the three axioms**, because a violation breaks order-independence for every
downstream consumer *silently* — the failure mode is a result that differs by how
it was assembled, which no test catches unless it is looking.

## 2026-08-06 (cont.) — The vocabulary audit, Void Palabra's seam, and status tags that mean something

Three asks in one: audit the terms the author collected while researching
Allomone (*"we can't just have these as nebulous terms, but in how they actually
apply"*), take **Void Palabra**'s domain into account for the user action graph,
and mark every page **planned / in development / complete**.

### The audit — [vocabulary](/../../VoidAllomone/okf/concepts/vocabulary.md)

Ten terms, each with a verdict and, where it applies, the specific thing it
applies to. Searched 2026-08-06; where a term did not surface as established
vocabulary, that is said plainly.

**Applies, already built:** *stigmergic behaviour* (the coordination model — and
stigmergic programming languages are a real if small research line: Stigcode,
Springer 2006) and *undirected hypergraph of co-occurrence* (literally `Frame` +
`Coincidence`).

**Applies, worth building — the two real finds:**

- **Spectral graph theory / the Laplacian.** Our user graph is *already* the
  exact object the Laplacian is defined for — undirected, non-negatively
  weighted. The Fiedler vector partitions it into **work clusters with no labels
  and no training**, and λ₂ says whether attention is one activity or several.
  Concrete payoff: a `when in-cluster-with "volume"` predicate, strictly stronger
  than pairwise coherence because it catches things related *through a chain*.
  Costs stated (an eigensolver; a choice of `k`; and the discipline that a
  cluster label must never become model content, since clusters are not stable
  under perturbation).
- **Hodge decomposition — but NOT where it was pointed.** The author's own doubt
  (*"might not be the best way to enact symmetry"*) was right, and now there is a
  precise reason: combinatorial Hodge theory decomposes **edge flows** —
  *antisymmetric* data — and our coherence weights are symmetric and
  non-negative. We threw away the asymmetry it operates on, deliberately.
  Applying it would mean inventing a direction we removed on purpose. (And
  "Hodge **dual**" names the ⋆ operator on forms, a different object again;
  symmetry here is enforced by *representation* — the canonical `a < b` — which
  is cheaper and unfalsifiable.) **Where it genuinely applies is the resolution
  set:** resolutions are pairwise judgements, which is exactly HodgeRank's input,
  and the decomposition detects and localizes *circular rulings* (`A > B > C >
  A`) that nothing currently surfaces. Cheap — linear least squares.

**Does not apply:** *Stigmergic Quiescence* — and the author's instinct was
right. Quiescence detection is precise distributed-systems vocabulary
(Dijkstra–Scholten) presupposing **processes that run and might not stop**.
Allomone has no processes, no messages and no run; `merge()` is a total function
that returns. Needing quiescence detection would be *evidence something had gone
wrong*. Recorded rather than deleted, because it becomes real the moment
derivation goes incremental/asynchronous or multi-peer.

*Embodied Stigmergic Programming* — **not an established term**; "stigmergic
programming" is. But a useful reading survives and is kept: the person is the
embodied agent, `Device` is the body, and what they touch is the trace. Do not
cite the phrase; do keep the idea.

*Graph embeddings* — real, applicable, **deliberately not now**: embedding
dimensions are not interpretable, which collides with the commitment that every
derived value be explainable by provenance. Spectral clustering is the better
trade at this stage — also an embedding, but its answer can be explained by
pointing at edges.

*Topological overlay on the text* — half built (projectional editing: inline
widget spans, hover previews), half declined (the AST-node overlay).

### The sheaf rejection got a second, better leg

Void Palabra's `academic-foundations.md` §9 reaches the same verdict from the
other direction, and their argument does not depend on our value types at all:
**in a join-semilattice, associativity means pairwise agreement already implies
global agreement**, so once conflicts are objects the obstruction to gluing
vanishes *by construction*. So the rejection now rests on two independent legs —
we could not compute `H¹`, and **even if we could it would be zero**. What
survives is diagnostic: nonzero `H¹` and "conflict" are one phenomenon under two
names, so it is where damage would show up if a future change ever made the merge
partial again. Their formulation is the keeper: *use it to think; do not cite it
to decide.* [foundations §7](/../../VoidAllomone/okf/concepts/foundations.md) updated; in
fairness the first write-up also understated the field (cellular sheaf theory
*is* a matured computational toolset — the objection is our values, not its
practicality).

### Void Palabra — [multi-user](/../../VoidAllomone/okf/concepts/multi-user.md)

Palabra is the system layer (history, versions, convergence, sync) sitting on
Void Core beside us; neither imports the other. **Their `join` and our `merge`
are the same algebra at two layers** — same three laws — and both arrived at
*conflict-as-object* independently: theirs categorical (Mimram & Di Giusto: the
patch category lacks pushouts, so merge is partial; the cocompletion's new
objects **are** the conflicts), ours lattice-theoretic (`blue ⊔ red = ⊤` keeps
merge total). Two spellings of one fact, and the agreement is worth more than
either alone.

**The ruling that matters: derived annotations must never sync; their inputs
must.** Straight out of the EDB/IDB split — scripts, resolutions, tags and any
materialized user graph are model content and sync; merged annotations are
derived and each peer re-derives. Three reasons, of which the third is the one
people will get wrong: **two peers agreeing on scripts and disagreeing on derived
annotations is correct behaviour**, because their property vocabularies,
predicates and user graphs may legitimately differ.

Conflicts at two layers stay separate: **Palabra resolves what the text is;
Allomone resolves what the text means together.** A ⊤ can arise on a single peer
with no network at all.

**And one genuinely open question, now Q20:** whose attention does the user graph
record — per-peer, or merged? It changes what `with` *means*. The shared reading
is attractive and closer to the real stigmergic picture (pheromone is shared
environment; a colony where each ant reads only its own trail is not a colony),
and it **joins correctly with no new machinery**. But a materialized attention
graph records what a specific person did, on which device, in a log that is
attributed and permanent by design — [horizons](/horizons.md)' "total
observability is a security primitive" cuts both ways. **Lean: per-peer by
default, sharing an explicit opt-in**, because un-sharing something already
synced is not possible. Decide before any host materializes one; nothing does.

### Status tags that carry weight

Every page in the folder now declares one, mapped onto the OKF validator's actual
honesty rule (`status:current` requires a `resource:` backing it, unless the type
is reference/design/roadmap):

- **complete** → `status:current` + `resource:` — composition, language,
  user-graph, editor;
- **in development** → `status:draft` — index, host-protocol, vocabulary;
- **designed only** → `status:planned` — multi-user.

The legend is in the folder's [index](/../../VoidAllomone/okf/index.md) so the
distinction is visible rather than buried in frontmatter.

## 2026-08-06 (cont.) — The write path, planned and gated: Voidscript, emergence, and one correction

The author set the eventual direction — **Allomone should interact with
Voidscript**, creating runes and adding tags — with an explicit condition: *"for
that to be possible I would like a robust debugging system, and with emergent
behavior being something built into allomone itself, we would need systems to
prevent chaos."* Two new pages capture it:
[materialization](/../../VoidAllomone/okf/concepts/materialization.md) (the seam) and
[emergence](/../../VoidAllomone/okf/concepts/emergence.md) (the analysis). Both
`status:planned` — nothing is built, and the deferral is now a plan.

### A correction I owe: "Allomone has no processes"

The author pushed back on that claim, and **the pushback was right in the
direction it points**, so the record is corrected rather than defended
([vocabulary](/../../VoidAllomone/okf/concepts/vocabulary.md), revised verdict).

The claim is a **theorem about the derive-only kernel** — `merge()` is a pure
total function that returns, so there are no processes and quiescence is
inapplicable. That stands for tier 0. But it becomes **false the instant a rule's
output can be another rule's input**: Allomone stops being a function and becomes
a **fixpoint computation**, and "has it settled?" is a real question. So the
author's instinct that quiescence "must be important to this early phase" is
right in the way that matters — it cannot yet be *observed*, but it must be
**designed for now**, because the decision it constrains (may rules feed each
other?) is being made by default every time a feature lands.

One refinement carried forward: once it applies, *quiescence* is not quite the
right borrowed word — distributed-systems quiescence is about **messages in
flight**, and we have none. The precise vocabulary is **termination (strong
normalization)** and **confluence**; together, **convergence**, which gives unique
normal forms. **Getting convergence is the work; detecting quiescence is free
once you have it.**

### The layering, and the four tiers

Voidscript is the **mechanism of action** (every statement is a dispatcher
command); Allomone is the **logic and behaviour** (every statement is a
constraint). So the seam is one-directional and obvious: **Allomone compiles to
Voidscript**, never the reverse — and the write path itself is therefore *not the
hard part*.

What is hard is the tiering, which collapsing everything into "materialization"
hides: **0 derive** (bounded, always terminates) then **1 amend** fields/tags on
existing runes (bounded; terminates iff the effect graph is acyclic) then **2
create** runes (unbounded — output becomes new input; needs a budget) then **3
reflect**, create or modify *scripts* (self-modifying; undecidable in principle).
Noted for scoping: Hormiga's styling is tier 0, their plausible next want
(auto-tagging) is **tier 1**, and it is worth finding out whether anyone needs
tier 2 before designing for it.

### The script-that-writes-scripts has a name, and it is a theorem

The author's worry — *"what would it mean if a script made another script? I feel
like Allomone would respond with a script that also makes another script, and so
then there would be a ripple effect"* — is **Knuth–Bendix completion**: find the
critical pairs where rules overlap and disagree, generate rules to resolve them.
That is exactly "search for the asymmetry and restore the symmetry of logic." And
its central known property is that **it may not terminate** — completion either
halts or produces rules forever, with divergence the generic case.

So the ripple is the *expected* outcome, not the unlucky one. **Ruling:
propose-don't-generate.** Allomone may compute the asymmetry and *offer* the
rule; it may not write it. The cascade is broken by construction, the log stays
honest (an accepted proposal is an authored edit), and static analysis stays
meaningful because the rule set does not change underneath it. This is the same
pattern the conflict UI already uses one level down: **the engine finds what
needs deciding; the person decides it.**

### The decidable question — and the honest limit

The ask was *"an engine that can analyze whether a script will generate a vicious
cycle."* Half of the honest answer is that **this is undecidable in general** —
termination of term rewriting is undecidable, graph rewriting likewise, and
rewriting systems are Turing-complete. Anyone promising otherwise is wrong.

The other half is that **the question we actually need is much smaller and IS
decidable**: not "does this halt" but **"can rules feed each other in a loop?"**
Build the **effect graph** — an edge from S1 to S2 exactly when what S1 writes
intersects what S2 reads, both extracted statically from the AST — and check for
cycles. **Acyclic implies termination**; this is Datalog stratification, and
Tarjan SCC answers it cheaply. Run against the author's own four-script example,
the SCC *is* the report: it names the exact scripts in the loop and the symbol
that closes it.

Stated conservatively on purpose: the check rejects some systems that would in
fact terminate. **Refusing a safe program is an inconvenience; accepting an
unsafe one is a runaway.** And acyclicity bounds *feedback*, not *volume* —
tier 2 growth needs budgets regardless.

Void Core reached the same two-layer conclusion from the interaction-net side,
and the wording is adopted: *"interaction combinators are Turing-complete, so the
guard is the only general termination story"*
(`conformance/reduce/cases/07-termination-guard.json`). **A static check for the
structural hazard, plus a runtime guard for what it cannot see.** The author's
observation that vicious cycles are already familiar from interaction combinators
was the right instinct — the substrate has lived with this, and its answer
transfers.

### The logging question answers itself, and the answer is reassuring

The author caught the tension mid-thought: *"if Allomone creates a new script,
this should be logged… actually, isn't this adding a rune? Isn't this the very
chaos I said we needed to avoid?"* **Yes to all three — they are one fact seen
from two sides.** A generated script is a rune, so it is a dispatcher command, so
it is logged, attributed, replayable and undoable. There is no version where
generated things are invisible.

**And the log is the defence, not merely the exposure.** Attribution names the
generator (`agent:allomone/<script>`), a cascade lands as one batch and therefore
one undo frame, and replay reproduces it exactly. An emergent system without a
transcript is chaos; the same system with one is a system you can watch, bound
and revert.

### The sequencing — the actual recommendation

The author asked, in both directions, whether to engage Voidscript early. The
answer splits: **do not build the write path early** (the original instinct was
right), but **do build the analyzer early — before the capability it protects.**
The effect graph is buildable *now*, over derive-only scripts, where it will
always report "acyclic." That is the correct baseline, not a wasted exercise: it
starts life passing and stays passing until something real changes, and when
writes arrive it is a **precondition** rather than a retrofit.

**The standing rule: no write tier ships before the analyzer that bounds it, and
no analyzer ships without a test that makes it fire.** The demo is where to
construct a cyclic rule set on purpose — the author's own suggestion, and the
right one.

One pleasing turn: **the effect graph genuinely IS a node graph.** We declined
the AST overlay because a syntax tree is a tree wearing a graph's clothes, but
script-depends-on-script is a real directed graph with real cycles — so
`edit_canvas` is finally the right tool, and highlighting an SCC in red beats any
error message.

### Also captured

**Federated learning**, per the author's note, is the right shape if a model ever
reads user graphs — train locally, share updates not data — and it fits the Q20
per-peer lean rather than fighting it. Two caveats recorded: it collides with the
explainability commitment (the same objection that parks graph embeddings), and
**Void Bicho** already exists as the sibling that runs models, so it is neither
Maiz's nor Palabra's to own. The near-term version of "learn from the user" that
stays explainable is spectral clustering.

## 2026-08-06 (cont.) — Research phase closed; the predicate registry ships

Two halves: a final cross-check that closes the research phase
([paradigm](/../../VoidAllomone/okf/concepts/paradigm.md)), and the first development it
unblocked — **host predicates**, the one gap that stood between "Allomone is our
engine" and "Allomone is a base language other applications extend."

### The cross-check — an independent pass, and what it changed

The author ran an agnostic research agent on *"stigmergy programming language
with interaction combinators"*, with no knowledge of this project. It framed the
core tension better than any single source we had read:

> **Interaction nets are about computing a definite result. Stigmergy is about
> coordinating behaviour in a dynamic environment.**

That named the thing Allomone sits inside, and the resolution is now written
down: **Allomone is stigmergic in coordination topology and confluent in
semantics.** From stigmergy we take the *shape of information flow* — no
messages, no addressing, the shared environment IS the channel. From interaction
nets we take the *shape of outcomes* — deterministic, confluent, terminating.
That combination is deliberate and is not the usual one; classical stigmergic
systems get their emergence from being stochastic and time-dependent, and we keep
the architecture while discarding the dynamics.

**The test that falls out:** if a proposal changes *how information reaches a
rule*, it is a stigmergy question. If it changes *what answer comes out*, it is a
confluence question, and confluence wins.

**Five of its suggestions were refused**, each for a reason already load-bearing:
pheromone **decay** (a clock in the record); **stochastic/Markov** semantics
(would break order-independence and explainability at once — a conflict could not
be told from a coin flip); **perception-action agents** (the actor model, which
reintroduces processes and addressing); **pi-calculus** (models channel-based
concurrency; we have no channels — the constraint store is the right formalism);
**temporal logic** (no time to reason over; *spatial* adjacency is the real
question, and spectral clustering answers it). The pattern is worth stating as a
rule: **every one of them adds time or chance.** Allomone borrows stigmergy's
topology and refuses its physics.

**The name turns out to be more precise than we knew.** Classical stigmergy runs
on **pheromones**, which are specifically *intraspecific* — same species,
homogeneous agents. **Allomones are interspecific.** That is the actual
architecture: a script is emitted by one party (a human, an agent, a host's
domain language) and read by something of an entirely different kind, which
**never interprets what it carries**. Recorded honestly: the strict biological
sense has an allomone benefit the *emitter* at the receiver's expense, where
*synomone* would be kinder — but there is a fair reading where the host emits and
gets what it wants while the library is an indifferent substrate. (The
independent pass reached for "pheromone trails" unprompted, not knowing the
project was named for the neighbouring term. The convergence is evidence; the
*difference* between the two words is the part worth keeping.)

**NELA** was offered as precedent and is noted briefly: a real project
(`heikowagner/nela-lang`), interaction nets as formal semantic foundation and
compiler target — the same architectural move as Allomone to Voidscript to Core's
nets. But its goal is different (a language optimized for *LLMs* to read and
write), so: read for comfort, do not import.

**No overhaul.** The author asked whether new trajectories meant rebuilding, and
the honest answer is no. The independent pass re-derived what we had and diverged
only toward time and randomness. Everything genuinely new this session came from
our own passes — the lattice framing, the CSS split, Knuth-Bendix, the effect
graph, Palabra's associativity argument — and all of it **fit the existing
structures** rather than replacing them, which is itself a signal the shape was
right. The research phase leaves a queue, not a rewrite.

### Built: host predicates

`PredicateRegistry` in `allomone.hpp` — `name` to a pure
`fn(Subject, arg, UserGraph*) -> bool`, host-owned, no globals. Void Core's
host-registered query-predicate ruling applied verbatim: **the core owns
registration and the calling convention; the host owns the domain computation.**

Four decisions, each pinned by tests:

- **Parsing is registry-free.** An unknown word parses as a `Host` term; whether
  *this* host has it is a separate check (`allo_check`) returning diagnostics,
  not parse errors. So the AST of a script is the same in every host, and a
  script written for another domain stays readable, colourable and editable
  instead of a wall of errors.
- **An unknown predicate kills its whole rule, including under `not`.** Found by
  a test: the diagnostic promised *"this rule will never match"* and the code
  disagreed, letting `not <unknown>` match everything. Unknown is **not** false —
  reading it that way would make a foreign script's every negated rule fire on
  everything here.
- **The kernel cannot be shadowed.** Registering `tag`/`kind`/`name`/`device`/
  `with` is ignored: those five mean the same thing in every domain forever.
- **Purity is a hard rule**, stated in the header — no clock, no I/O, no mutable
  state, because a predicate that consults the time makes the merge depend on
  *when* it was asked and destroys order-independence silently.

A host predicate counts toward `strength` like any other term: specificity is
about how much had to hold, not who implemented the check.

**The demo registers one**, deliberately chosen to make the argument: `louder
"0.5"` is **parameterized and continuous**, which is exactly what tags encode
badly and therefore the honest case for the seam existing. Its closure reads the
frame's own `Scene` — the expected pattern, and still pure in everything but
name. Tags remain a legitimate alternative rather than a mere fallback: if a
nightly job can tag `overdue`, `when tag "overdue"` is simpler and needs no C++.

Trued up: [language](/../../VoidAllomone/okf/concepts/language.md) (grammar + the three
language-level semantics), [host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md)
(the section is no longer aspirational), [roadmap](/../../VoidAllomone/okf/concepts/roadmap.md)
(gap closed), [index](/../../VoidAllomone/okf/index.md). Tests green except the
pre-existing upstream `reduce_conformance` case 15.

## 2026-08-06 (cont.) — The Sentinel proposal, triaged; influence analysis built

A governance proposal arrived (the "Sentinel" memo): harmony scoring, sensitivity
analysis, sandbox exploration, a bad-script library, an autonomy slider, and a
Kernel/Sentinel/Weaver split. The author's instruction set the standard — *"i
don't care if it takes longer, i care if its wrong or not the correct
direction"* — so it was **triaged, not adopted**:
[governance](/../../VoidAllomone/okf/concepts/governance.md).

**One idea is excellent and shipped this session. One is a good organizing frame.
Two rest on a foundation that does not fit and would break a commitment already
paid for.**

### The diagnosis that reorganized the rest

**Allomone does not know what a GUI looks like, and this is not an omission.**
The memo defines a Harmony Score `Φ` over "GUI mantles," scoring consistent
spacing high and flickering low. But Allomone emits **annotations** — `color`,
`weight`, `glow` are **opaque strings it never interprets** — and it has no
concept of spacing, overlap or a rectangle. A visual `Φ` would make the library
learn a rendering model, which is exactly the quarantine that lets a host use it
for `temperature` and get everything free.

Two facts finish the motivating example off: **flickering is impossible at tier
0** (the merge is a pure function; output cannot oscillate), and **when it
becomes possible it is already covered exactly** by the effect-graph cycle check.

The instinct survives in a form that fits: the library can measure **its own**
structural health without knowing about pixels — conflict density, influence
concentration, effect-graph acyclicity. A *visual* Φ is a host concern, and if
one is ever needed the seam is a host-supplied `HarmonyFn`, same shape as
`PredicateFn`.

### Adopted and built: the Sentinel's instruments

The memo's strongest idea — *"which script is actually doing the work?"* — is
real, was unanswerable, and turns out to be **cheaper than proposed**. The memo
suggests leave-one-out re-merging; but every cell already carries its
contributors and its winner, so the common case is **one pass over the existing
result, no re-derivation**:

- `influence(Merged)` — per source: cells won, offered, overridden, in conflict,
  plus win `share`. A human ruling appears as a source too, which is correct:
  it is a person exercising influence.
- `influence_concentration(Merged)` — Herfindahl index over win shares. 1.0 =
  one script decides everything; the memo's "High-Energy Agent" test as a number.
- `diff(before, after)` — the **counterfactual**, which is a genuinely different
  question: influence counts cells a source *won*, while the diff also reveals
  what would **surface from underneath** it. Leave-one-out is merge-without-it
  diffed against merge-with-it, cheap because merge is pure.

All read-only. The split that keeps governance compatible with derive-only is
that **the Sentinel only reports and the Weaver only proposes** — neither writes.
Surfaced in the demo as an `influence` tab, including the live counterfactual for
the selected script.

One correction to the memo folded in: inferring *user focus* from script
influence is the wrong signal — influence measures what the **rules** do, not
what the **person** does. The user graph is the right input for that.

### Adopted as framing: Kernel / Sentinel / Weaver

Genuinely clarifying, because the three have different failure modes and trust
levels, and the autonomy slider adjusts exactly one: **Kernel** (built; failure =
a wrong answer; trust total, it is a pure function), **Sentinel** (analysis;
failure = a false alarm or a missed hazard; it only reports), **Weaver** (the
proposer; not built; failure = runaway generation; calibrated by the slider).
Names adopted as *concept labels*, not committed as type names — two of the three
do not exist, and naming code before writing it is how vocabulary drifts.

The memo's closing ask — *the Sentinel must approve any materialized script* — is
**accepted and strengthened** to a precondition, matching the standing rule.

Also settled: *"Intellisense is the wrong word"* is right, but it is **two
features**. Editor **completion** genuinely is intellisense and keeps the name.
The **proposer** ("these two rules disagree; here is a rule that would settle
it") is a different thing — that is the Weaver.

### Adopted, sharply narrowed: neighbourhood exploration

`N_k(M)` over all mantles within k mutations is **combinatorial** — at 10³ runes
and 20 tags, `k=1` is ~2×10⁴ and `k=2` ~4×10⁸. But the narrowed version is better
than the general one for a precise reason: **the AST tells us the only mutations
that can matter.** A mutation touching nothing any condition mentions changes
nothing, provably, because conditions are the only way a rule sees anything. So
explore **condition-relevant candidates only** — `O(rules × subjects)`, the size
of the derivation we already run — and report something computable and
explainable: *"adding tag `danger` here would create a ⊤ between `theme` and
`rival` on `color`."*

### Rejected: the bad-script library

Four reasons, the first decisive: **it replaces a theorem with a heuristic.** The
memo's own example — *"infinite recursion in constraints"* — is **exactly** what
the effect-graph cycle check decides, with a report naming the scripts in the
cycle. Having a decision procedure and choosing fuzzy similarity is a downgrade.

The mathematics does not support it either: **graph edit distance is NP-hard**,
and the **Weisfeiler–Lehman kernel is incomplete** — there are families of
non-isomorphic graphs its test cannot distinguish, and its equality-based
comparison is noted in the literature as too rigid to define a good similarity
measure. Intractable or unsound. Plus "resembles something bad" is unexplainable
(the same objection that parks graph embeddings), and it needs a labelled corpus
that cannot exist before the system has users.

### Rejected: autonomy level 3

Levels 0 (manual) and 1 (propose, human accepts) are adopted — they are the
existing propose-don't-generate ruling, better packaged as a **user-facing,
per-application** setting. Level 2 (auto-apply to Drafts if Φ holds) is gated
hard on a host Φ *and* the effect-graph check. **Level 3 — continuously
self-modify to climb a gradient — is rejected**: that is a completion procedure,
completion diverges, and a system that edits its own rules invalidates the static
analysis keeping it safe, because the thing being analysed changes underneath
the analysis.

## 2026-08-06 (cont.) — The effect graph ships: the analyzer, before the capability

The Sentinel's core piece is built — `voidmaiz/sentinel.hpp` +
`src/allomone/sentinel.cpp`, pinned by `tests/sentinel_smoke.cpp`, surfaced as
the demo's *effect graph* panel. This honours the standing rule from
[emergence](/../../VoidAllomone/okf/concepts/emergence.md): **no write tier ships before the
analyzer that bounds it, and no analyzer ships without a test that makes it
fire.** The analyzer now exists and there is still no write tier — which is the
correct order.

### What it decides, and what it does not pretend to

**"Does this rule set terminate?" is undecidable** — termination of term
rewriting is undecidable, graph rewriting likewise, rewriting systems are
Turing-complete. The header says so, because a tool that quietly overpromises
here is worse than no tool.

**"Can rules feed each other in a loop?" is decidable**, and that is the question
we actually need: build `S1 -> S2` iff `writes(S1)` intersects `reads(S2)`, then
look for cycles. Acyclic implies termination — Datalog stratification, answered
by Tarjan SCC in one pass.

### The pieces

- **`effect_node()`** extracts a footprint statically from the AST — no subjects,
  no evaluation. Conditions yield `tag:` / `kind:` / `name:` / `device:` /
  `coherence:` / `host:` reads; effects yield `prop:` writes. Sets are sorted and
  deduped, so **reordering a script's rules produces a byte-identical
  footprint** — the non-linearity property extended into the analyzer, and it is
  a test.
- **`build_effect_graph()`** does edges, Tarjan SCC, and Kahn strata. Two
  decisions worth recording: a **self-loop counts as a cycle** (a rule feeding
  its own condition is the hazard, not an exemption), and **`strata` is empty
  when cyclic** — a cyclic system genuinely has no stratification, which is
  exactly why it may not terminate, so representing it as "no layers" is honest
  rather than a placeholder.
- **`EffectEdge::via`** carries the shared symbol. That is what makes a cycle
  report *actionable* — "D writes `rune`, which B reads" instead of an alarm.
- **`effect_verdict()`** so a host, a CLI and a test all say the same words.

### The deliberate chaos test

The author's own four-script example is constructed on purpose: A generates
runes, B modifies runes, C creates scripts when a rune is modified, D modifies a
rune when a script changes. The analyzer must name `{B, C, D}` **and exclude A**,
which feeds the loop without being in it.

That test failed the first time — and instructively: the *engine* was right and
the *test* was wrong. I had B reading `rune` while D wrote `rune.field`, so the
loop never closed and the analyzer correctly found the smaller `{C, D}` cycle.
Fixing the model to match the author's description gave `{B, C, D}` as intended.
A useful reminder that the symbol vocabulary is the load-bearing part: string
equality is what an edge means, so granularity is a modelling decision, not a
detail.

### The baseline is green, and that is the design

Derive-only scripts write `prop:*` and read everything else, so the sets cannot
intersect and the graph is **always acyclic**. Confirmed by test, and shown in
the demo. **The analyzer starts life passing and stays passing until something
real changes** — which is precisely what makes it a precondition rather than a
retrofit.

### One soft spot, recorded rather than discovered later

**Host predicates are opaque.** A registered predicate is a C++ function, so we
cannot see what it reads; it contributes `host:<name>`, which nothing can
currently write. That is exact **today** and becomes **unsound the moment rules
can write**, because a predicate reading a rune field would have an invisible
incoming edge.

The fix is due with the first write tier: either a **declared read-set on
registration**, or treat undeclared predicates as **reading everything** — the
conservative choice, which also gives hosts an incentive to declare. The
signature already takes the registry, so adding it needs no change at the call
sites. Written into `sentinel.hpp` and
[emergence](/../../VoidAllomone/okf/concepts/emergence.md) so it cannot be forgotten.

### Still to come

Runtime guards (step/fuel budget, write budget, generation depth), the debugging
kit (dry run, cascade trace, written-by provenance), neighbourhood conflict
prediction, and **rendering the graph on `edit_canvas`** — which remains the one
place a node canvas is genuinely the right tool, since unlike an AST a
script-depends-on-script graph really is a directed graph with real cycles.

## 2026-08-06 (cont.) — Editor UX pass: `then`, zoom/LOD, and the commit bug that made colour edits do nothing

Five items from the author, hands-on with the demo. One was a real bug, one was a
syntax ruling, three were features.

### The bug: editing a colour appeared to do nothing

The author's report — *"when i change a color in the 'theme' script, nothing
changes?"* — was a genuine defect, and an instructive one.

The wheel spliced the new hex into the buffer correctly and marked the editor
dirty. But **commit required focus to leave the whole editor**, and clicking the
popup's "done" button did not do that. So the `set` never dispatched, the model
never changed, and the playground never restyled. Everything worked except the
one step that made it real.

**Fix: closing the picker IS the release.** That is the VLS #14b staging
discipline applied at the right granularity — stage during the gesture, flush ONE
command when the gesture *ends* — and the gesture ends when the widget closes,
not when you leave the editor. The lesson worth keeping: **getting the
granularity wrong is indistinguishable from the feature being broken.**

Also added an explicit **Apply** button with a dirty marker, because
"it committed when focus left" is invisible, and a user cannot debug an
invariant they cannot see.

### `then`, not `->` (author's call)

*"i dont quite like the '->' syntax, i prefer lua conventions."* Adopted:

    when tag "danger" then color "#f85149", weight 2

It fits — the language already leans on Lua's `and` / `not`, and Hormiga's own
design described itself as "Lua-flavored". `when … then …` reads as a sentence
where `->` read as an operator, and a **keyword is something the editor can
colour, complete and eventually explain** — an arrow is punctuation forever.

**`->` remains an accepted alias**, same AST, so nothing existing breaks; it is
simply no longer the house style, and the diagnostic now names `then`. Both
spellings are pinned by a test that merges them and compares the result.

### Zoom and level-of-detail

Ctrl+Wheel zooms (plain wheel still scrolls, so the common gesture is
undisturbed). Below a threshold of pixels-per-line the editor **stops drawing
glyphs and draws each coloured token as a filled bar**, whitespace skipped so
indentation survives as gaps.

The author's framing was the right one: zoomed out, a script becomes a readable
**shape** — where the colours are, how the rules mass — rather than illegible
grey. It is close to a minimap already; a side-by-side overview pane would be a
small step from it.

### Click-away dismissal

No confirm button on inline widgets. Click elsewhere or press Escape. Needing to
confirm the end of a gesture you already finished is friction, and — per the bug
above — it was actively harmful, because the confirm button was the thing
standing between the edit and the commit.

### Edit delta

*"if a script changes, the delta graph of that script change should correspond to
the script change."* Built as an **edit delta** panel: snapshot the derived state
just before an edit lands, diff it against the state after, and show which cells
moved. Free, because merge is pure and the previous result is already in hand.

It reuses `diff()` — the same instrument as the counterfactual, pointed at **time
instead of at absence**. The counterfactual asks *"what if this script vanished?"*;
the edit delta asks *"what did my change just do?"* Same machinery, two questions.

### A settings tab

Editor zoom, LOD threshold, and the user-graph frame-idle limit — plus the host's
declared merge laws, shown read-only so the vocabulary is discoverable.
Everything in it is **view state**: never dispatched, never persisted, because
none of it passes the save/quit/reload razor.

### An OKF repair worth recording

Five pages in `concepts/allomone/` had lost their `tags:` key to a stray `\x01`
byte — a regex backreference evaluated at string-literal time in an earlier
scripted edit. The validator had therefore been reading them as **untagged**, so
the status marking added earlier was never actually in effect for
`composition`, `editor`, `host-protocol`, `language` and `user-graph`. Repaired;
`freshness` went from 5 to 6 recognized `resource:` links, still 0 errors.

Two lessons, both cheap: **scripted frontmatter edits need verification that
reads the field back**, not just a diff that looks plausible; and a
CONFORMANT result is only as strong as the fields the validator could actually
parse.

## 2026-08-06 (cont.) — `mantle enter` is not a verb: one bad string, four dead features, and the test that should have existed

The author, testing the demo: *"i cant subtract or add tags to widgets… when i
change the script i still don't see any reaction. the apply button doesnt work. i
can't even add new scripts."* Four unrelated-looking failures, **one root cause.**

### The bug

**`mantle enter <name>` is not a Void Core verb.** The `mantle` family is exactly
`new` / `rm` / `rename`; switching the active mantle is **`use <name>`**
(`verbs_edit.c`). So every `mantle enter` dispatched was rejected.

The seed ends `mantle new resolutions` → `mantle enter playground`. That second
command failed, so the app sat in **`resolutions`** for its entire life — and
from there:

- tagging a control → *"no rune matches"* (controls live in `playground`);
- editing a script → the `use scripts` failed, so `set theme source …` failed;
- **apply** → same;
- **+ script** → `rune new allo-script` succeeded but minted the rune in
  `resolutions`, so it never appeared in a list that projects `scripts`.

Four symptoms, one wrong string. Verified with a throwaway probe against a real
core before changing anything, which is the only reason the diagnosis took
minutes rather than a bisect.

### It was a LIBRARY bug, not just the demo

`compile_resolution()` (`annotate.cpp`) and `UserGraph::compile()`
(`usergraph.cpp`) both emitted `mantle enter <mantle>`. **Every host that used
either would have hit this.** Fixed in the library, the demo, and the tests.

### The tests were green, and that is the real finding

`annotate_smoke` asserted `cmds[0] == "mantle enter allo"`. `usergraph_smoke`
asserted `cmds.front() == "mantle enter userplay"`. Both **passed**, both pinned
a command the core rejects.

> **A string-equality test proves the code produced a string. It cannot prove the
> string means anything.**

So a new test exists: **`command_smoke.cpp` — every command the library EMITS
must actually DISPATCH.** It runs `compile_resolution()` and
`UserGraph::compile()` output through a real `Core`, requires `ok` on every line,
reports the core's own message on failure, and then reads the values back to
confirm the model really changed rather than merely accepting the commands. It
also pins the mistake itself as a live assertion — `mantle enter` must fail,
`use` must succeed — so nobody "fixes" it back.

**The rule it encodes: if a library function returns dispatcher commands,
something must dispatch them in a test.**

### It immediately caught a second shipped bug

On its first run, `command_smoke` found that
**`UserGraph::affordance_glyph()` used `"name"` and `"content"` where the core's
descriptor wants `"glyph"` and `"fields"`.** The glyph never registered, so every
`rune new allomone-affordance …` failed with *"unknown glyph"* — meaning
**materializing the user graph had never worked**, in the library, unnoticed,
since it was written. Fixed.

Two real bugs on the first run of one test is a fair verdict on the gap it was
filling.

### A smaller lesson, recorded

`describe` prints **facets** in its human lines and puts content in `data`. Two
of the new test's assertions read `.text()` looking for field values and failed
for that reason alone. Corrected to `get <rune> <field>`, which is the honest way
to ask. Worth knowing before writing the next assertion against a verb's prose.

## 2026-08-06 (cont.) — Hands-on feedback: two more bugs, Core's vocabulary, and clicks as places

Seven items from the author, testing the demo. Four landed this pass; three are
scoped and queued.

### The `use` bug had a second hiding place

**Toggling a script on or off did nothing.** Same root cause as the last round —
the checkbox dispatched `setjson <script> enabled …` while the active mantle was
`playground`, and scripts live in `scripts`. The previous fix repaired the paths
I looked at; this one I missed.

So it now has a **helper rather than a comment**: `dispatch_in(mantle, cmd)`
switches, dispatches, and returns. Every runtime write to a script goes through
it. Making the same mistake twice is the signal that the shape was wrong, not the
attention.

### Colour detection was too strict

The author typed `color "58a6ff"` — no `#` — and got no wheel. Correct by the old
matcher, useless as behaviour. Now **permissive on input, canonical on output**:
`#rrggbb`, bare `rrggbb`, and `#rgb` all get a wheel, and the wheel always writes
back `#rrggbb`, so a script converges on one spelling as you edit it. Fixed in the
demo's span detector *and* in the library's colour kit, which had the same
`#`-only assumption in two places.

### Void Core's vocabulary in the language

The author's ask: *"i like to use the rune, mantle, and holiday terminology in the
programing language itself."* Hormiga went the same way — their language.md
records *"the language now speaks Void Core's vocabulary — a rune is an instance,
typed by its glyph"*.

Added as **aliases**, parsing to the same `Term::Kind` because they mean the same
thing:

| Core spelling | generic spelling |
|---|---|
| `has "danger"` | `tag "danger"` |
| `glyph "slider"` | `kind "slider"` |
| `rune "Mute"` | `name "Mute"` |
| `mantle "library"` | *(new capability)* |

`mantle` is the one that is not an alias — it scopes a rule to one mantle, and
`Subject` gained a `mantle` field to carry it. The effect graph learned the
`mantle:` read symbol to match.

**`holiday` was deliberately not added.** A holiday is Core's term for effectful
host I/O, and Allomone is derive-only — there is nothing for it to name yet. It
belongs to [materialization](/../../VoidAllomone/okf/concepts/materialization.md), if anywhere.

### Every click is an action, and a place

Two of the author's items are one mechanism. *"clicks count as actions for the
user graph too, like any random click"* and *"detect which areas the mouse is in
most often… divide the areas of the screen into a cell… each of those areas are a
rune in the action graph."*

Exactly right, and it needed no new structure: **the screen is divided into cells
and each cell is an ordinary affordance** (`cell:3,7`, kind `region`). Coherence
between a place and a control then falls out for free — they were touched in the
same frame. A heatmap overlay reads the touch counts directly; there is no
separate store.

**On click, not per frame.** Sampling the cell under the cursor every frame would
measure *dwell*, which is a clock, and a clock in the record is what
[non-linearity](/../../VoidAllomone/okf/concepts/non-linearity.md) forbids. A click is a
discrete act, so counting clicks is frequency — order-free and honest. The
author's phrasing was "areas the mouse is in most often", and *often* is
frequency, which is the version that fits.

### A C++ hazard worth recording

Adding `mantle` to `Subject` **between** `name` and `tags` silently broke
positional aggregate init at every call site: `{"mute","button","Mute",{"danger",
"muted"}}` re-bound the tag list to the new string field, matched
`std::string`'s two-**iterator** constructor, and **segfaulted**. It compiled
without a warning.

Fixed by moving the test's fixtures to **designated initializers**
(`.id = …, .tags = …`). `Subject` is public API and will grow again, so naming
the fields is the version that survives.

### Queued, with the author's framing preserved

- **The Weaver — visible response.** *"i don't see any mirror or symmetrical
  scripts… i dont see any 'response' from my making a new script."* Fair: it is
  documented ([governance](/../../VoidAllomone/okf/concepts/governance.md)) and unbuilt. The
  first honest cut is **propose-don't-generate at autonomy level 1**: for each ⊤,
  compute the sharper rule that would settle it and offer it with an "insert"
  button. Critical-pair completion, human-gated.
- **Smarter conflict resolution.** *"for color disputes, that's not a bug, that's
  a feature… allomone will decide in a smart way."* Hormiga's conflicts.md gives
  the model: subsumption, then logical depth, then **recency**.

  One finding already, from reading it against our grammar: **for purely
  conjunctive conditions, subsumption is EXACTLY term-count.** If A's terms are a
  strict superset of B's, then `|A| > |B|`, so `strength` already implements the
  principled order — the two coincide, and no extra machinery is needed. What
  genuinely adds resolving power is **recency**, which we do not have. The plan
  is a declared per-property `ConflictPolicy` (`Surface` today, `Recency`
  opt-in), so "decide smartly" is a choice a host makes rather than a silent
  default that would destroy the conflict-surfacing property.

## 2026-08-06 (cont.) — The Weaver ships, and conflicts get a declared policy

The two items queued from the author's hands-on pass. Both are the same theme:
**the system should respond, without ever deciding for you.**

### ConflictPolicy — "decide smartly" as a choice, not a default

The author: *"for color disputes, that's not a bug, that's a feature… the whole
point is that then allomone will decide in a smart way."*

Reading Hormiga's `conflicts.md` against our grammar produced one finding worth
keeping, because it saved building the wrong thing:

> **For purely conjunctive conditions, subsumption IS term-count.** If rule A's
> terms are a strict superset of B's, then `|A| > |B|`, so `strength` already
> implements the principled entailment order — Hormiga's "subsumption first" and
> our `strength` coincide exactly. Building subsumption separately would have
> been redundant machinery.

What genuinely adds resolving power is the thing we did *not* have: **recency**.
So `ConflictPolicy` is now declared per property (or as a default):

- **`Surface`** (default, unchanged) — a genuine tie is ⊤ and a human decides.
- **`Recency`** — the newest source wins, Hormiga's final tiebreak.

Made a **host choice** rather than a library default deliberately: silently
resolving everything would destroy the conflict-surfacing property the author
said they liked, while never resolving anything ignores a real ask. A switch is
the honest answer to "both".

Two guards, both tested. **Equal recency does not resolve** — all-zero means the
host supplied no opinion, and inventing an order there is exactly the silent
guess ⊤ exists to prevent. And the result stays **order-independent**: a policy
must not smuggle arrival order back in.

`MergedCell::settled_by` now accounts for every cell — `strength`, `agreement`,
`arithmetic`, `recency`, `resolution`, `conflict`. A derived value nobody can
account for is what this design exists to avoid, so each one carries its own
account. In the demo, position in the scripts mantle *is* the recency: `rune new`
appends, so a later script is a newer one — no clock, no stored timestamp, using
the identity Hormiga's conflicts.md notes we already keep.

### The Weaver — the response the author could not see

*"i don't see any mirror or symmetrical scripts… i dont see any 'response' from
my making a new script."* Fair, and now built:
`voidmaiz/weaver.hpp` + `src/allomone/weaver.cpp`, with a **weaver** tab.

**It proposes; it never writes.** A rule set that writes rules to restore its own
symmetry is a Knuth–Bendix completion procedure, and completion **diverges** —
the ripple the author predicted. So each finding is a line and a button.
Accepting one inserts into the **editor buffer**, not the model, so it lands as
an ordinary staged edit you can read, change or abandon before applying. That is
autonomy level 1 from [governance](/../../VoidAllomone/okf/concepts/governance.md), exactly.

Three asymmetries, each computable and explainable:

- **`settle`** — a ⊤ conflict. The offered rule targets the subject with
  `rune "x"` padded by tags it genuinely carries, until it out-specifies the tie.
  Every term is true of the subject, so the rule matches exactly what it claims.
- **`unheard`** — a tag the data carries that **no rule anywhere mentions**. This
  is the honest, computable version of "symmetry": the rule set should mirror the
  vocabulary the data uses, and a tag nothing responds to is a distinction the
  person made and the rules ignore.
- **`gap`** — a property *most* subjects have and some do not. Only where
  coverage is already the norm; a property one subject has is *selective*, which
  is a design choice, not an omission, and nagging about it would make the panel
  worthless.

**The test is the round-trip, not the appearance.** `weaver_smoke` parses each
offered line, adds it as a source, re-merges, and requires that the conflict is
actually gone and settled `by strength` — a suggestion that fails when taken is
worse than no suggestion. Also pinned: full tag coverage proposes no `unheard`,
full property coverage proposes no `gap`, and the list order is determinate so
the panel does not reshuffle while you read it.

### A process note

Two builds were broken this session by the same thing: **Python heredocs writing
C++ string literals**, where `\\n` in the heredoc became a real newline and split
the literal. Both times the compiler caught it, but it cost a cycle each. Edits
to C++ string literals now go through the editor rather than a script — the class
of mistake is mechanical and avoidable.

## 2026-08-06 (cont.) — Completion ships; and one roadmap entry retired because it was already done

### Completion — the vocabulary offered rather than memorized

The roadmap called this *"highest value per unit of work in this folder"*, and
the vocabulary work of the last two passes made it close to mandatory: between
Core's aliases (`has`/`glyph`/`rune`/`mantle`), host predicates, and whatever
tags the data happens to carry, **nobody can hold the vocabulary in their head.**
The host knows all of it, so the host offers it.

**The seam: `CompletionSet`, with the byte range to replace supplied by the
HOST.** That is the one design decision worth defending — the editor could guess
a word boundary, but only the host knows whether the caret sits in a bare word or
*inside a quoted string*, and guessing would be wrong for exactly the cases that
matter (a tag name in quotes is not a word-boundary problem the editor can solve
generically).

The popup owns **four keys while open** — Tab/Enter accept, Up/Down choose,
Escape dismisses until the caret next moves. Everything else still edits, so
typing never stalls waiting on a suggestion. Implemented with three targeted
guards rather than a `goto`, which would have jumped over initializations.

The demo's provider reads context from the token stream — which token the caret
is in, and what came before it — and completes accordingly: **tags** after
`tag`/`has`, **glyphs** after `kind`/`glyph`, **rune names** after `name`/`rune`,
**mantles** after `mantle`, **modalities** after `device`, **affordances** after
`with`, **registered predicates** in condition position, **properties** after
`then`. Each carries a `detail` hint — "4 subjects", "host predicate" — which is
what turns a list of words into something choosable by someone who does not
already know the answer.

### A roadmap entry retired for being already true

`roadmap.md` listed **"subsumption specificity"** under *designed, not built*,
claiming `strength` was a mere term-count proxy for the principled entailment
order. Working through it against our grammar showed the entry was **wrong, and
had been since it was written**:

> **For purely conjunctive conditions, subsumption IS term-count.** If rule A's
> terms strictly contain B's, then `|A| > |B|`. So `strength` already implements
> the entailment order exactly, and Hormiga's "subsumption first, logical depth
> second" collapses to one number for this grammar.

Building it would have been redundant machinery, and the "incomparable rules"
case is not a weakness in the scoring — it is where no entailment exists to
appeal to. Both [roadmap](/../../VoidAllomone/okf/concepts/roadmap.md) and
[foundations §6](/../../VoidAllomone/okf/concepts/foundations.md) corrected, with the condition
that makes the claim true stated out loud: it holds **only while the grammar
stays conjunctive**. An `or` would let a longer condition match a *larger* set
and break the correspondence — one more reason
[language](/../../VoidAllomone/okf/concepts/language.md) declines it.

This is the second time this session that reading Hormiga's design carefully
*subtracted* work rather than adding it. Worth noticing: the honest audit of a
borrowed model is as likely to retire an item as to create one.

## 2026-08-06 (cont.) — Colours that MIX: a third answer to disagreement

The author, thinking about property interaction: *"suppose one script says tag
'bob' then color 'blue' and another makes it red… red and blue make purple
right?"*

That is a genuinely different answer from the two the engine had. Until now a
disagreement could be **surfaced** (⊤) or **won** (strength, recency,
resolution). It can now also be **mixed** — and for a property like colour that
is often the *right* answer rather than a compromise.

### The design decision: a host-supplied join, not a `Blend` lattice

The obvious implementation is `Lattice::Blend` that parses hex and averages.
**It would have broken the quarantine everything else respects**: `color` is an
opaque string the library transports without understanding
([host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md)), and a colour-mixing law
makes the library learn what a colour is.

So the shape is `Lattice::Custom` plus a **`JoinFn` the host supplies** — the
same pattern as `PredicateFn`, `SpanRenderer` and the rest. It keeps the
quarantine intact *and* generalizes: blend colours, average vectors, union sets,
whatever the domain means by "combine".

**The JoinFn receives the whole SET at once — sorted and deduped — and that is
load-bearing, not a convenience.** A pairwise average folded left is **not
associative**: `avg(avg(0,10),50) = 27.5` while `avg(0,avg(10,50)) = 15`. A host
writing the obvious two-argument blend would have silently broken the merge law
and made the result depend on grouping. Handing over the entire set removes the
chance to get it wrong, and the test proves it by using a deliberately
non-associative operation: `mean{0,10,50} = 20` in every ordering, never 27.5.

Two further guards, both tested: values arrive **deduped**, so two sources
agreeing is one value and reports `agreement` rather than a blend of a thing with
itself; and a `Custom` law declared with **no function disputes** rather than
silently emitting nothing.

### The colour theory, and why the space matters

Implemented in the demo, where colour knowledge belongs. **OKLab**, not sRGB:

- averaging the hex digits directly (sRGB) is the obvious thing and looks wrong —
  sRGB is gamma-encoded, so red+blue lands on a dark muddy plum;
- averaging in **linear light** is physically right for emitted light and reads
  too bright;
- **OKLab** is built so a numeric midpoint looks like a *perceptual* midpoint,
  which is what "red and blue make purple" means to a person.

Toggle in settings: *blend colours instead of disputing*. Off by default, because
surfacing stays the conservative answer and mixing is a host's declaration.

### Also this pass

**The colour wheel dismissal bug, properly fixed.** Clicking *inside* the wheel
closed it. The cause was a hand-rolled "did the click land outside?" hit test
that could not reliably distinguish a drag inside a colour picker from a click on
the editor. Replaced with **ImGui's own popup** (`OpenPopup`/`BeginPopup`), which
gets outside-click-closes and inside-click-keeps for free. The lesson is the
ordinary one: reimplementing a framework primitive to avoid reading its API is
how you get a subtly wrong version of it.

**Right-click does something now.** Right-click puts the caret under the cursor
*first*, so the menu always acts on the word you pointed at — no selecting
required, which is the whole point of asking about a word you do not recognize.
It offers an explanation of that word plus cut / copy / paste / select-all. The
new `explain` callback is deliberately separate from `hover`: hover fires
constantly and must stay terse, while this is asked for deliberately and can
afford to teach. **Completion tells you a word exists; explain tells you what it
does** — the difference between a vocabulary you can type and one you can learn.

**Completion got context.** `when` is now offered only where a rule can actually
begin — the test is simply whether the current line already contains one, which
is exactly the author's *"we wouldn't type `when when` right?"*. And `not` no
longer offers itself. Value positions now offer real literals: Tab after `color`
gives `"#ffffff"` (plus red/blue/green), because half the point of the wheel is
that you never type a hex code by hand, and completion should not make you.

### A process failure worth naming

Three builds this session were broken by the same thing: **Python heredocs
writing C++ string literals**, where `\n` in the heredoc becomes a real newline
and splits the literal. I said I would stop after the second and did it again.
It is now a rule rather than an intention: **string-literal edits go through the
editor, never through a script.**

## 2026-08-06 (cont.) — Why the blend never fired: a combining law was being starved

Three bugs and a language design, from the author testing the colour blend.

### The blend bug — a genuine semantic gap, not an oversight

The author had two rules in the SAME script — `when tag "danger" then color
"#58a6ff"` and `… "#f85149"` — and saw no purple.

**The merge was collapsing each source to ONE opinion before the join ran.** That
collapse is right for `Unique`: two rules in one script is the
defaults-and-exceptions idiom, and the sharper one wins silently. It is exactly
wrong for a combining law, which is *defined* by wanting all of them. The blend
only ever saw one colour, so it had nothing to blend.

The fix separates the two families properly. A source's contribution is now a
**set of values** (value → highest strength within that source), and the law
decides what to do with it:

- **overriding laws** (`Unique`, `Any`) take the sharpest — unchanged;
- **combining laws** (`Sum`, `Max`, `Min`, `All`, `Custom`) take **all of them**,
  across every source and every rule.

**Dedup is what keeps this idempotent.** Values are a set, so re-supplying the
same source cannot inflate a `Sum`, and two rules stating the same value is one
constraint said twice. Verified: blue + red in one script now gives `#bb8aab`,
`settled_by = join`, and the contributor line reads `#58a6ff + #f85149` — so a
blended cell can be explained by its own provenance, which it could not before.

Two behaviours changed and are now pinned: `Sum` over two rules in one script
adds them (1 + 4 = 5), and a one-value join reports `agreement` rather than
`arithmetic`, because nothing was actually combined.

### The colour popup, again — and the actual cause this time

Last pass I switched to `ImGui::BeginPopup` and said it was fixed. It was worse:
the popup became **impossible to dismiss**, because the draw loop called
`OpenPopup` whenever `open_span` was set. ImGui would close it on an outside
click and the next frame put it straight back.

`OpenPopup` now fires exactly once, **in the click handler that asked for it**.
The draw loop only ever calls `BeginPopup`. Worth noting the shape of the
mistake: the first version was a hand-rolled hit test that got dismissal subtly
wrong, and the second was the right primitive driven from the wrong place. The
fix is not "use the framework" but "let the framework own the state" — an
`OpenPopup` in a render loop is a state machine with two writers.

### Tab and Enter now do different things

The author: *"'enter' and 'tab' are different in function, please make sure those
are correctly distinguished."* Right — completion was accepting on both.

**Tab accepts. Enter is always a newline.** Stealing Enter means a suggestion you
were ignoring eats the line break you actually wanted, and there is no way for
the editor to know which you meant. Two keys, two jobs.

### The language expansion, designed

The author asked to start on variables, lists and functions, Lua-style, *"easily
accessible to people"*. Designed in [values](/../../VoidAllomone/okf/concepts/values.md), not
yet built.

The tension is real: those three are the classic ways to smuggle order into a
language (assignment, accumulation, call order). **The useful half of each
survives under the functional reading** — which is also the easier one to teach,
so accessibility and safety pull the same direction here rather than against each
other:

- **`let` is a DEFINITION, not an assignment** — one binding per name, order of
  definition irrelevant, re-binding an error. A spreadsheet cell, not a slot.
- **Lists are SETS with algebra** (`union`/`intersect`/`minus`/`in`/`count`). A
  list has an index, an index is a position, a position is an order. Ordered
  sequences need a *declared* sort key, never insertion order.
- **Functions are pure macros over conditions** — `define risky(t) = has t and
  has "danger"`. Expansion adds to `strength`, so a call cannot secretly make a
  rule broader than it looks. This is the highest value per unit of risk: the
  repetition in a real rule set is conditions, and naming one is how a person
  builds their own vocabulary.

Refused, with reasons: reassignment, accumulator loops, early return, any mutable
state.

**The pleasing part: the one new hazard is already solved.** A `let` that
transitively references itself is a definition cycle — which is an SCC, which the
[effect graph](/../../VoidAllomone/okf/concepts/emergence.md) already decides. The analyzer
built before the write path covers this too: same check, new symbols. Building
the safety layer first keeps paying.

### The interaction-net reading, recorded

The author's aside — *"color mixing should be thought of as like, an interaction
rule, and port mapping"* — is exact, and it explains a design choice that would
otherwise look arbitrary. A `(subject, property)` cell **is a port**; two
annotations meeting there are an **active pair**; the merge law is the
**interaction rule** that reduces them; the merged state is a **normal form**.

And it justifies the whole-set join: net reduction must be **confluent** — the
same normal form whichever redex fires first — and a pairwise blend folded left
is not associative, so it would not be confluent. The analogy breaks at exactly
the point where it would have mattered, which is a decent sign it is a real
correspondence and not decoration.

## 2026-08-06 (cont.) — Completion learns restraint; and the discovery engine turns out to already exist

### Three editor fixes

**Completion was ambient rather than responsive.** It appeared on empty lines and
after every space, which is something you spend the day dismissing. It now shows
**only while a word is actually being typed** (a non-empty replace range), or on
**Ctrl+Space** when asked. The author's phrasing was exact: *"i should be able to
at least click somewhere empty for it to go away."*

**`when tag ` kept offering `tag` again.** The context test fell through to "offer
conditions" whenever the caret sat in whitespace, regardless of what preceded it.
Now a condition keyword is recognized as such and the *next* thing offered is a
**value** — the real tags, glyphs, rune names or modalities, already quoted and
ready to accept. The filter had to learn to ignore those quotes so typing `au`
still matches `"audio"`.

**The right-click menu now scales with the editor's zoom.** The author's reason is
the right one: the menu is *about* the text, so reading one at 300% and the other
at 100% is a jarring change of scale in the middle of one thought. (And it points
somewhere larger — a script you pan and zoom around, rather than a text box.)

### Discovery: the engine is already written, and pointed elsewhere

The author's ask — *"i dont want to manually create things for EVERY SINGLE
property of a GUI rune. Rather, it should be DISCOVERED… HOWEVER what we DO need
to define is the interaction of these rules!"* — comes with its own answer, and
that split is now the organizing principle of
[discovery](/../../VoidAllomone/okf/concepts/discovery.md):

> **Discover the vocabulary. Declare the interactions.**

Working it through, there are three vocabularies and **none of them needs a
hand-written list**:

- **what a rune HAS** — glyph descriptors already declare `fields` and `hints`,
  and the [surface census](/concepts/surface-census.md) already harvests exactly
  that. **The discovery engine the author is asking for is already written**; it
  is currently pointed at documentation instead of at the language.
- **what a rune can be ANNOTATED with** — discoverable from the *rules
  themselves*. A property exists **because some rule writes it**; `then sound
  "ping"` in any script *is* the declaration of `sound`.
- **what is in play right now** — `Merged::cells`, already there.

**Built this pass:** the demo's completion no longer carries a hardcoded
`{color, weight, glow, label}`. It harvests every `then <property>` across every
parsed script, and shows each with its use count and its merge law. Write `then
sound "ping"` once and `sound` completes everywhere afterwards, with no
registration step.

**What discovery cannot do, stated so it does not get oversold:** it finds that a
property *exists*, never what it *means*, how it *combines*, or what edits it.
`sound` might be `Unique` (one sound), `All` (play both) or a `Custom` join (mix
them) — no inspection reveals which, because that is a design decision about the
domain. The graceful default is what makes this safe: an undeclared property is
`Unique`, edits as text, and renders however the host chooses to ignore it.
Declaring only ever *adds*.

Consequence for [host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md): a host must
declare merge laws where combination matters and decide what to render, but
**need not enumerate its properties**. The list is a projection of the rule set,
not a configuration file.

### Rules as runes, and a second graph

Two designs recorded, both from the author.

**A rule should be a rune.** Today a *script* is a rune and rules are lines in its
text. Inverting that — rules in their own mantle, scripts as groupings — is the
more honest model, because it is already how the system *behaves*: rules apply
application-wide and which script one came from is bookkeeping. It buys identity,
**per-rule recency** (today the tiebreak is per script, so editing one rule makes
every rule in that file look equally new), and rules that carry properties —
explicit weight as data, per-rule enable, tags.

**The hard part is named up front: rule identity must survive editing.** If
changing a colour deletes and recreates the rule, it loses its id, its recency,
and every resolution that named it. Ids-in-the-text is rejected for the same
reason Hormiga rejected it for rune references; content matching is fragile; the
honest answer is that **the editor edits rules, not free text** — which is where
"a widget canvas disguised as a text editor" was always heading. Phased so the
cheap part lands first: rule-level provenance in `ConstraintMap` needs no model
change at all and immediately makes conflicts name rules instead of files.

**The interaction graph is a different graph from the effect graph**, and the
author's definition is precise: `A ~ B` iff their matched sets overlap *and* they
write the same property. Effect graph edges are **directed** and mean *A feeds
B*; interaction edges are **undirected** and mean *A and B meet*. Today an
interaction is only visible when it produces a ⊤ — a rule silently overridden, or
two colours quietly blending, leaves no trace. The interaction graph would show
every meeting labelled with the law that resolves it, which is the author's
*"what operation are we calling for?"* answered per edge. It is also the
interaction-net reading made literal: an undirected edge between two rules
meeting at a cell is a wire between principal ports.

### The rule that came out of all three

> **If the model can be asked, do not keep a second copy.**

The same principle as owning no truth, applied to *vocabulary* rather than state
— and it is the thread connecting the three sections: rules should be runes
because the rule set is already a graph; interaction should be computed because
the overlap is already implied by the rules; properties should be harvested
because they are already in the AST.

## 2026-08-06 (cont.) — The OKF audit: what a day of building did to the docs, and one page to replace reading thirteen

The author, correctly: *"you are an llm, sometimes you forget context or forget
previously written things or small details. let's do another look over the okf as
it relates to allomone, and lets make sure that things look more or less
correct."*

The folder had drifted **within a single day**, which is worth stating plainly
because the drift was not sloppiness — it was the ordinary consequence of
building faster than the prose describing the build. Thirteen documents and about
ten hours of code produced **one code defect, one test defect and six stale
claims**, all found by reading each page against the function it names.

### The code defect: an accepted registration that could never fire

`PredicateRegistry::is_kernel_predicate` listed five names —
`tag`/`kind`/`name`/`device`/`with` — while the parser routes **nine** words to
kernel terms, the extra four being `mantle` and Core's spellings
`has`/`glyph`/`rune`. A host registering `has` therefore got its function
**accepted and stored, and never called**: the parser turns `has` into a `Tag`
term before the registry is ever consulted.

That is worse than the shadowing the check exists to prevent. A refused
registration is a fact you can discover; a stored one that never fires is a
function you will debug for an hour. **Silent dead code beats nothing, so refuse
it at the door.** All nine names are now refused, and `allomone_smoke` loops over
every one rather than spot-checking `tag`.

### The test defect: three assertions that had been passing by luck

`weaver_smoke` failed on a rebuild that touched nothing near it. The cause:

    const Proposal* gap = of_kind(propose(m, {s}, subjects()), "gap");

`propose` returns by value; `of_kind` returns a pointer **into that temporary**,
which dies at the end of the full expression. Every field read afterwards was a
dangling read. It had been passing since it was written, and stopped only because
an unrelated header change shifted the allocator.

Two things worth keeping. First the generalizable lesson: **a test that passes by
luck is indistinguishable from one that passes, until it is not** — and this one
guarded the Weaver's most user-visible output. Second, the diagnosis discipline
that found it in one step: rather than assume the library had broken, reproduce
the claim in a five-line scratch program against the same library. It printed
`when all then color ""` — the exact string the test said it could not find,
which localizes the fault to the test immediately.

(The `== nullptr` comparisons two blocks down use the same pattern and are
genuinely fine: they never dereference. Left as they are, with a comment saying
why, because rewriting correct code to match a rule it does not need is how a
codebase loses the ability to say what its rules are for.)

### Six stale claims, and the one that mattered

- **[composition](/../../VoidAllomone/okf/concepts/composition.md) described a merge the code
  no longer performs.** The page still said stage 1 reduces every source to one
  opinion — which is precisely the behaviour that made two colour rules in one
  script unable to blend. The normative page was describing the bug. Rewritten
  around the real split: **overriding laws take the sharpest opinion, combining
  laws take every distinct value**, with the asymmetry justified rather than
  merely stated (a combining law has already declared the answer is *all of
  them*, so a specificity contest would silently discard contributions).
- **[editor](/concepts/allomone/editor.md) still listed Enter as an accept key**
  after the author had asked for exactly the opposite. Also missing: `complete`
  and `explain` in the seam block, the typing-gated popup, the zoom-scaled
  right-click menu.
- **[language](/../../VoidAllomone/okf/concepts/language.md)'s grammar omitted `mantle` and all
  three aliases** — four words the parser accepts and the spec did not mention.
- **[host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md)** had a `Subject` missing
  its `mantle` field, an example using the retired `->`, and the
  property-vocabulary requirement that `discovery.md` had already ruled should be
  softened. Now softened, with the designated-initializer warning attached to the
  struct that caused a segfault by not having one.
- **[index](/../../VoidAllomone/okf/index.md)** filed `governance` as `planned` when
  its own frontmatter says `draft` and the Weaver is built, and named editor
  completion and the effect-graph analyzer as "the next queue" after both had
  shipped.
- **[values](/../../VoidAllomone/okf/concepts/values.md)** illustrated `define` with an example
  using `or` — a keyword the same page refuses and explicitly leaves open inside
  `define`. An illustration is not the place to quietly assume an unsettled
  decision.

Also corrected: "six lattices" (there are seven) and "5 predicates" (six).

### The new front page

The author's second ask: *"a document that's almost like a front page large
overview of allomone that'd be useful for some other ai agent to read such that
they could theoretically begin programming with allomone."*

[start-here](/../../VoidAllomone/okf/concepts/start-here.md) — deliberately self-contained, so
an agent that reads only that page can write correct scripts and predict their
behaviour. **Every code sample and every derivation in it was run through the
real engine and shows its actual output**, which is the only way a page like this
stays true; an orientation document that quietly invents its examples is worse
than none, because it is confidently wrong at exactly the moment someone is least
able to check.

Writing it surfaced a behaviour nobody had written down, found by running the
example rather than by reasoning about it:

> **A `when all` default is free under an overriding law and is not free under a
> combining one.** Under `Unique`, a strength-0 default is harmlessly overridden.
> Switch that property to a `Custom` blend and the default **becomes an
> ingredient** — the worked example produces a three-way blend where the author
> expected two. If you declare a combining law, audit your `when all` rules.

That is now §5's gotcha, and it is the strongest argument for the run-it-first
discipline: it is not deducible from any page in the folder, and it would
otherwise have been discovered by a confused user.

### On what "emergent" is allowed to mean

The page's §6 splits it in two, because the word invites the wrong expectation
and the author has now asked about reflection scripts twice.

**What genuinely emerges is compositional**: layered outcomes no author
specified, cross-script blending, conflicts appearing as data changes,
user-responsive derivation through the graph, coverage patterns the Weaver
notices. **What does not emerge is temporal**: rules do not trigger each other,
nothing accumulates between evaluations, nothing is stochastic, nothing runs in
the background. The stigmergy is in the topology and explicitly not in the
physics.

And the direct answer to *"idk if we have fully implemented a 'reflection script'
concept yet?"* — **no, and not by omission.** A reflection script is write tier 3,
and the ruling against it is a theorem: a system that generates rules to restore
its own symmetry is a Knuth–Bendix completion procedure, and completion's known
central property is that it may diverge. What exists instead is
**propose-don't-generate** — the Weaver computes the asymmetry and offers a rule
line for a human to accept, edit or dismiss. That shipped; the thing it replaces
is refused rather than pending.

### One list instead of five

[roadmap](/../../VoidAllomone/okf/concepts/roadmap.md) gained a single ranked index of
everything still open, with the Void Palabra-dependent items separated out so
they can be ignored when planning our own work. Items 1–6 — `define`,
rules-as-runes phase 1, the interaction graph, the conflict inspector, wiring
glyph-field discovery in, and the effect graph on `edit_canvas` — are all small
and all independent, which makes them the honest next tranche.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **34 concepts, 0 errors, CONFORMANT**.

## 2026-08-06 (cont.) — Testing gets a folder, and a stress plan gets taken apart

The author, relaying a research agent's stress-testing programme: *"tests and
debugging at this scale will at the minimum require its own folder… the tests may
require us to write scripts in other languages to generate content… these engines
and debugging tools, the script generators or graph generators should not be
present in the shipped or deployed version of hormiga."*

Both halves are right, and the second is the load-bearing one. A generator that
ends up linked into `voidmaiz` is a dependency every downstream application
inherits for no benefit — and the person who pays is a client who never asked for
a test suite.

### The boundary rule, and the shape that makes it free

> **Nothing that GENERATES a test may be reachable from a shipped target.**

The obvious implementation — Python drives the engine through bindings — is
wrong, because it makes Python a dependency of verification, then of CI, then of
anyone checking the repo out. Ground rule 5 forbids package managers and
acquiring one for the test suite is acquiring one.

**The right shape is already in the family**: Void Core ships
`conformance/reduce/cases/*.json` plus a runner, and we already consume it. So —
generators emit **JSON cases** that get checked in, and a small C++ runner
replays them. CI needs no Python; a failure is reproducible by anyone forever,
without the generator that found it; the generators may be as slow and ugly as
they like because they run when a developer goes looking. And the boundary rule
becomes checkable in one line: **delete `harness/` and the build stays green.**

The discipline that goes with it:

> **The harness discovers. The C++ suite remembers.**

A randomized harness that finds a bug and forgets it is worse than none, because
the bug returns and the next run may not reproduce it. Every failing seed shrinks
to a checked-in case. A failure that cannot be reduced to a deterministic case is
not fixed — it is a live bug with a known trigger.

### The independence rule

The plan proposed *"scrape the `src/allomone/*.cpp` … to build a formal model."*
That inverts the value, and the counter-example was found the same day:

> **A model derived from the implementation cannot falsify the implementation.**

A scraped model reproduces the code's decisions **including its mistakes**, so
differential testing against it finds crashes and never wrong answers. This
morning's `is_kernel_predicate` defect is exactly the shape — a scraped model
would have scraped the same five names and agreed with the bug perfectly.

So the model is written from `composition.md` and `language.md`, which exist at
reproduce-exactly precision *for this purpose*. Then model ≠ code means one of
them is wrong and which is a real question; and if the spec is ambiguous enough
that the model could have gone either way, **that is the most valuable of the
three outcomes**, because it is the failure that bites a second implementer years
later.

### Three claims wrong against the code

Recorded because a triage that lists only the adoptions is not a triage.

**It is a join, not a meet.** The plan's first sentence says *"a meet (∧)
operation over a join-semilattice"* — duals, contradicting themselves in one
line. Nothing in the testing depends on it, but it must not reach the harness's
naming, because "meet" inverts every intuition someone later brings to the code.

**Associativity is not expressible.** `merge(merge(A,B),C) == merge(A,merge(B,C))`
does not type-check: `merge` takes `vector<ConstraintMap>` and returns `Merged`,
which cannot be fed back in. There is no binary composition, so no grouping can
be written, so the law has nothing to quantify over. Stated positively —
**associativity is enforced by the signature rather than sampled by a test**,
which is stronger than what was asked for. The obligation this transfers is now
written down: *if a binary merge is ever added, for incremental derivation or a
Palabra peer join, A3 stops being structural and becomes real and breakable.*

**There is no nesting to blow a stack with.** The plan wants 100-level nested
`and` and parser recursion depth. A condition is a **flat `vector<Term>`**; `and`
is a separator, not a tree constructor; there are no parentheses and no recursive
productions, and the tokenizer and parser are flat `while` loops.

**But the instinct was right and pointed at the wrong file.** Recursion exists in
exactly one place — `strongconnect` in `sentinel.cpp`, Tarjan's SCC, **recursive
on effect-graph depth**. A generated chain of 100,000 scripts each feeding the
next recurses 100,000 frames. That is a real stack-exhaustion bug in the analyzer
that gates the entire write path, and it is precisely what a generator is for:
trivial to emit, nobody would hand-write it. Adopted, retargeted.

### A performance target that reverses a decision

The plan sets *100k subjects, merge < 16 ms*. That makes a red test out of a
designed limitation: full recompute is documented as fine at 10²–10³ with
incremental derivation recorded as *"not to be built speculatively — wait for a
client that feels it."* A pass/fail threshold would silently reverse the author's
call.

It also conflates three costs: 100k subjects × 10k rules is 10⁹ match operations
in `allo_eval` before `merge` is called at all.

**Adapted:** record the *curve* for parse, eval and merge separately at 10¹–10⁵
and check the numbers in. The only failure is a **2× regression against the
recorded curve** — never an absolute millisecond. The data then exists when a
client does feel it, and staying non-incremental stays a decision rather than an
oversight.

### The scenario worth stealing, and the one that teaches nothing

**Best in the plan: the Weaver round trip.** Accept a proposal, re-merge,
re-propose — the asymmetry must be gone, and **no new proposal of the same kind
may replace it.** That second half is the mechanical form of the anti-divergence
guarantee behind propose-don't-generate: a proposal that provokes another
proposal forever is the Knuth–Bendix behaviour the whole ruling exists to prevent.
Nothing tests it today.

**Rejected: the "custom join race."** An impure `JoinFn` incrementing a global,
run from many threads, to *"prove why side-effects are forbidden."* It tests a
host breaking a documented contract; the outcome is known in advance; it would
fail by design, so nothing about the library could change to make it pass or fail
differently. Replaced with a contract harness that *is* testable and does
regress: an instrumented join asserting its input is sorted, deduped and
non-empty, plus running eval and merge **twice on identical inputs in every
generated case** — the cheapest possible impurity detector.

**And one impossible scenario replaced by a better one.** The plan wanted to
prove `merge` never sees a half-parsed state under rapid keystrokes. There is no
such hazard — parsing is single-threaded and total, so a half-typed script is
*fewer rules plus diagnostics*, a valid `Script` rather than a corrupt one. But
the editor genuinely does re-parse half-typed text every frame, so the real test
is **prefix fuzzing**: for a corpus of valid scripts, take *every prefix* and
assert totality, token-offset sanity and finite diagnostics. Every prefix is
exactly the sequence a person types through, so the corpus is representative
rather than adversarial, and it covers the mid-token and mid-string states that
are hardest to reason about.

### A finding for the byte fuzz, with a decision attached

Outside string literals `ident_char` is `isalnum || '_' || '-'`, so **every byte
≥ 0x80 becomes its own one-byte `Invalid` token.** A multi-byte codepoint outside
a string therefore yields N tokens whose boundaries **split the codepoint** — and
the editor addresses spans by byte offset. A concrete reachable rendering fault
with a known trigger. The likely right fix is for the tokenizer to consume a whole
UTF-8 sequence into one `Invalid` token; inside strings bytes are copied verbatim
so UTF-8 already survives, and that asymmetry should be pinned either way.

### Also noticed

**`concepts/allomone/index.md` is invisible to the OKF engine.** The bundle
loader's `RESERVED = {"index.md", "log.md"}` filters by **basename**, at any
depth — so the folder index that thirteen documents link to is not a concept, is
not link-checked, and does not appear in `ls` or `analyze`. The new
`testing/index.md` inherits the same fate. Kept for consistency rather than
renamed, because the convention is worth more here than graph membership and
renaming would break every inbound link — but it is a real gap in what
CONFORMANT covers, and the second time in two days that the answer has been
*"the validator could not see the field you thought it was checking."*

### Recorded

Four documents under `okf/concepts/allomone/testing/` —
[index](/../../VoidAllomone/okf/concepts/testing/index.md) (charter, boundary rule, and the
instruments we already have and under-use),
[invariants](/../../VoidAllomone/okf/concepts/testing/invariants.md) (28 properties, each with
the exact condition under which it holds, because an invariant asserted too
broadly is a false-alarm generator),
[harness](/../../VoidAllomone/okf/concepts/testing/harness.md) (the case factory and the JSON
format) and [plan](/../../VoidAllomone/okf/concepts/testing/plan.md) (the triage). **Q21**
filed: may the harness have dependencies — lean stdlib-only.

Sequencing puts the JSON case runner first, because nothing else can land before
the thing that makes a generated finding permanent. Worth noticing at the end of
it: **steps 1–3 need no Python at all.** A real fraction of this programme is
just C++ tests we have not written yet, and the harness only starts paying at
step 4.

**Verified:** OKF **37 concepts, 0 errors, CONFORMANT**; nested folders load
correctly.

## 2026-08-06 (cont.) — Two developers, one working agreement, and a manual for the agent

The author, thinking out loud rather than instructing: *"i think the development
of allomone matters too… however, this might be redundant. im not sure if this is
needed, because a developer would supposidly have access to the whole okf, so
what would be the point of the dev start here?"*

**The doubt was half right, and identifying which half is the whole entry.**

### The redundancy was real, and so was the gap underneath it

A *mirror* of [start-here](/../../VoidAllomone/okf/concepts/start-here.md) for kernel
developers would have been redundant, and the author's reasoning for why is
exactly correct: `start-here` is self-contained because a host developer may read
**one page**, and a kernel developer is never in that situation. They have the
index, the roadmap, and a `non-linearity` page that already announces itself as
the thing to read first. A second front page is a table of contents with extra
steps — and a second place to drift, which this folder has now demonstrated it
does within a day.

**But underneath the redundant half sits the largest gap in the OKF.** The
standing rules of *how work happens here* — "no write tier before its analyzer",
"the harness discovers and the suite remembers", "string-literal edits never go
through a script", "propose, don't generate" — existed only as prose scattered
through 3,600 lines of this log. Reconstructing them cost a full read, which
means an agent starting fresh would simply **re-make the mistakes**, and several
of them are mistakes that pass tests.

So: same filename the author proposed, so it is findable where they expect it;
different document. [dev-start-here](/../../VoidAllomone/okf/concepts/dev-start-here.md) is a
**working agreement**, and it opens by saying plainly why it is not a mirror.

Its §3 is the part that earns the page — every failure mode already paid for,
in a table, each with *why it survived*. That column is the point:

- `mantle enter` — **the tests pinned the wrong command**, and agreed with the
  bug perfectly;
- `affordance_glyph` — nothing ever dispatched it, so it had never worked;
- the dangling `Proposal*` — dangling reads returned the right bytes until an
  allocator moved;
- `#ifdef` on an enum — the guard compiled and was always false;
- the `\x01` byte — **CONFORMANT was reported while the validator could not read
  the field it was checking**;
- heredocs mangling string literals, three times;
- mid-struct field addition turning a tag list into `std::string`'s two-iterator
  constructor.

Most of that list *passed something* before it was caught. That is the property
that makes it worth writing down rather than trusting anyone to re-derive.

### The two developers, made queryable

The author's other observation: *"im seeing 2 things emerge. a developer that is
USING allomone to develop stuff. and a developer of allomone itself."*

That distinction was already in the prose of every page ("Audience: **both**",
"Audience: **us**") and **not** in the tags, where `audience:dev` covered both
meanings across 34 documents — the exact collapse the author noticed. Since the
validator constrains only `confidence:`, the audience vocabulary was free to
split:

| tag | who | pages |
|---|---|---|
| `audience:host` | building **on** Allomone | 14 |
| `audience:kernel` | building **Allomone itself** | 17 |
| `audience:all` | genuinely both | 3 |

Most pages carry **both**, which is correct rather than lazy — the merge law is
normative for us *and* a contract for them. The single-tag pages are the
interesting ones: `host-protocol` is host-only, the whole testing folder is
kernel-only. `okf query audience:host` now returns an application developer's
reading list exactly, which is the observation turned into a tool.

Applied to the Allomone folder only. Elsewhere `audience:dev` stands until the
distinction actually bites — a half-applied convention is worse than a scoped
one, and the scope here is "where a base language has two kinds of client."

### A manual for the agent

The author, correctly and cheerfully: *"YOU will be a very big component in
actually generating test cases… so honestly, you should have some documentation
for yourself haha."*

[agent-authoring](/../../VoidAllomone/okf/concepts/testing/agent-authoring.md). Not general
advice about being careful — the specific, already-observed failure modes of
LLM-authored tests **in this repository**.

Its thesis:

> **The characteristic LLM failure here is plausibility, not sloppiness.** A
> model is very good at producing what a correct assertion *looks like*, and a
> wrong-but-well-formed assertion is exactly the kind that passes review and pins
> a bug.

`mantle enter` is the canonical case: readable, well-formed, and asserting
precisely what the buggy code produced. A human would probably have run the
command once.

**And writing it resolved a tension between two rules already in the OKF** —
which was not the plan, but is the most useful thing on the page. *"Run it, then
write it down"* and *"a model derived from the implementation cannot falsify the
implementation"* appear to contradict, and each alone produces a distinct,
already-observed failure:

| approach | failure | incident |
|---|---|---|
| observe only | you pin the bug | `mantle enter`, `affordance_glyph` |
| predict only | you pin a fiction | token offsets 22/28, wrong subject ids, the mis-modelled cycle |

> **Derive the expectation from the SPEC, then run it — and when they disagree,
> that disagreement is the finding, not a number to correct.**

The order is what makes it work: deriving first commits you to an expectation, so
a mismatch carries information. Running first makes whatever came out the
expectation, and nothing has been learned. And the three-way branch on
disagreement matters — code wrong (the bug), spec wrong (fix the page too), or
**spec ambiguous enough that either derivation was defensible**, which is the
most valuable of the three because it is what bites a second implementer years
later.

That also, retroactively, explains why `composition.md` is written to
reproduce-exactly precision: not as a flourish, but so that step one is possible
at all. *If you cannot derive an expectation from a normative page, that is a
defect in the page.*

The other rule worth carrying beyond this project: **make it fail once, on
purpose.** An assertion that has never been red is not known to be connected to
the behaviour it names — and read the failure message while it is red, because a
test that fails with `CHECK(cells.size() == 4)` tells the next reader nothing.

### Recorded

Two documents: [dev-start-here](/../../VoidAllomone/okf/concepts/dev-start-here.md) (the
working agreement) and
[testing/agent-authoring](/../../VoidAllomone/okf/concepts/testing/agent-authoring.md) (the
manual). Audience tags split across the Allomone folder. Both front pages are now
linked from [index](/../../VoidAllomone/okf/index.md) as a matched pair — *"building
on it"* and *"building it"*.

**Verified:** OKF **39 concepts, 0 errors, CONFORMANT**; `okf query
audience:host` and `audience:kernel` both return correct reading lists.

## 2026-08-06 (cont.) — `define` ships, and the manual written this morning immediately catches its author three times

Roadmap item 1, and the last outstanding piece of the author's *"lets get stuff
like functions and all that jazz (again, in a lua style)"*.

    define risky(t) = has t and has "danger"

    when risky("muted") then weight 2

### The design held, which is worth saying because it usually does not

[values §3](/../../VoidAllomone/okf/concepts/values.md) predicted that `define` would need **no
new value types and no evaluation machinery** — pure expansion into the existing
AST. That is exactly what it needed. `allo_eval` never learns definitions exist,
and `effect_node` sees straight through an expansion, because by the time either
runs there are only terms. The strongest evidence is a one-line test: **a call
produces a byte-identical rule to writing the condition out by hand** — same
terms, same strengths, same matches. A definition is a way of *saying*
something, not a different kind of thing.

### Two constraints the design had not anticipated, both theorems

**`not f(...)` is only legal when `f` expands to exactly one term.** Negating a
conjunction distributes into a disjunction — `not (A and B)` is `not A or not
B` — and Allomone has no `or`. There is nothing correct to expand a negated
multi-term call into. The design had anticipated *recursion* being refused; it
had not noticed that **De Morgan refuses something too**, and the diagnostic
carries the actual argument rather than a shrug. Double negation collapses by
XOR, at every level of nesting, so `not no("x")` where `no(t) = not has t` is
plain `has "x"`.

**Definitions are script-local, permanently.** Not a first cut to be relaxed:
cross-script definitions would make parsing one script depend on another, and
*`allo_parse` being a pure function of text* is what lets a foreign script stay
readable, colourable and diffable. Locality is what preserves that.

A third decision, smaller but load-bearing: **parentheses distinguish a call
from a host predicate.** `risky("x")` is a call, `louder "0.5"` is a predicate,
and `louder("0.5")` is an error that says so. The ambiguity is resolved
syntactically rather than by consulting a registry, so *parsing stays
registry-free* — the property that lets a script written for another domain
still parse here.

Resolution is by **Kahn's algorithm over the definition graph**, not recursive
expansion. A cycle has to be *reported* rather than recursed into — and, having
spent the morning writing up that Tarjan's SCC recurses on graph depth as a real
stack hazard, adding a second one to a path that runs on **every keystroke**
would have been a poor look.

### The manual written this morning caught its author three times, in one hour

[agent-authoring](/../../VoidAllomone/okf/concepts/testing/agent-authoring.md) was written
today. Building `define` immediately produced three of the exact failure modes
it lists, which is either embarrassing or the best possible validation of the
page; recording it as the latter.

**1. Make it fail once — and the break came back GREEN.** Inverting the
definition-side negation XOR (`!=` to `||`) on purpose left the whole suite
passing. That is the page's central claim demonstrated live: *an assertion that
has never been red is not known to be connected to the behaviour it names.* The
line was genuinely **uncovered** — negation composing *between definitions* is a
different code path from negation at a *call site*, and only the latter had a
test. Added both: a two-level case and a three-level one, so an
OR-instead-of-XOR bug cannot pass by coincidence. Re-broke, confirmed red at the
right line, restored.

**2. Wrong fixture.** The first `allo_matches` assertion used `has "audio"` —
which is `weaver_smoke`'s fixture. This file's `subjects()` has no `audio` tag at
all. Failed immediately, cost a minute, and is listed on the page as *"wrong
subject ids."*

**3. Predicting the escaping instead of observing it.** Extending
`command_smoke` to round-trip a script through a real Core, I escaped newlines
into `\n` — and `cmd_arg` does the opposite: it passes **real newlines through**
single quotes, because Core's tokenizer takes double-quoted text literally. That
is *the same bug the quoting discipline was invented to prevent*, re-committed
by the person who documented it. The script came back as one line.

Two more from the same test, both worth keeping:

- **`get <rune> <field>` returns a JSON-quoted display form**, not the raw
  field. The demo reads through the **projection** instead. Generalizing:
  **a round-trip test that reads through a different door than the application
  proves nothing about the application** — so the test now reads exactly the way
  the demo does.
- **A glyph descriptor's `fields` is an ARRAY of names.** Registering it as an
  object succeeds, and then projects nothing — the same silent-success shape as
  the `affordance_glyph` bug. Two occurrences now; the pattern is *"Core accepted
  my descriptor"* not being evidence of anything.

### Why command_smoke grew a case

`define` introduced three characters the language had never emitted through the
dispatcher: `(`, `)` and `=`. "The parser accepts it" is not evidence that "the
author can save it", so the new case stores a script, reads it back through the
projection, and **re-parses what came back** — checking meaning rather than
string equality. It found two real problems before finding none.

### Also

The demo completes `define`, offers this script's definitions as calls (with
their arity and expanded term count as the detail hint), and **right-clicking a
call shows what it expands to** — the expansion is the documentation, and it
cannot go stale. The seed script deliberately places a definition *below* its
use, so order-independence is visible rather than merely claimed.

Filed to [backlog](/backlog.md) T2 rather than fixed in passing:
`field_of(SceneNode, key)` now has **three private copies** across the repo, and
the JSON un-escaping inside it should have exactly one implementation.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT**, and the seed
script parses to 4 rules + 1 definition with `risky("muted")` at strength 2.

## 2026-08-07 — Rule-level provenance, and the phase-1 plan that would have broken the cascade

Roadmap item 2. The goal was the author's: a conflict should name *the rules that
disagree*, not just the files. It shipped — but the plan for it, written in
`discovery.md` yesterday, was **unsound**, and finding that out first is the
substance of this entry.

### The plan said one ConstraintMap per rule. That destroys the CSS split.

[discovery §1](/../../VoidAllomone/okf/concepts/discovery.md) prescribed: *"emit one
`ConstraintMap` per rule instead of per script, id'd `script#n`."* It reads like
free provenance — no model change, better messages.

**The merge groups opinions BY SOURCE.** So splitting rules into separate sources
converts every *within-script* overlap into an *across-source* disagreement, and
the within/across split — silent inside one source, surfaced across independent
authors — is a load-bearing commitment borrowed from the CSS cascade, not a
detail.

Measured before building anything, on the demo's own seed:

    when tag "audio"  then color "#58a6ff"
    when tag "danger" then color "#f85149"     -- one subject has both tags

    per-SCRIPT : conflicts=0  value=#58a6ff
    per-RULE   : conflicts=1  value=""

The seed script carries a **comment celebrating that silent tiebreak**. The
defaults-and-exceptions idiom is the entire point of writing rules, and this
scheme would have turned every script using it into a wall of ⊤. The plan was
written by the same author as the commitment it violated, one day apart, which is
worth noting without drama: **a design that is right in isolation can still be
wrong against a system**, and the only reliable way to find out is to run it.

Per [agent-authoring §2](/../../VoidAllomone/okf/concepts/testing/agent-authoring.md), the
derived expectation came first and the measurement confirmed it. The
disagreement was between two *documents*, not between doc and code, which is a
case that page had not anticipated — noted, and it resolves the same way: one of
them is wrong, decide which.

### What shipped instead: provenance inside the annotation

`Annotation::origin` and `MergedCell::origin` — a free-form host label saying
*which part of the source* said this. Allomone writes `"line 12"` (1-based, to
match what the editor and diagnostics show). Grouping did not change at all, so
the algebra is untouched, and all of the intended value survives:

- conflicts read **"theme (line 4) vs rival (line 2)"**;
- a settled cell points at the rule that actually won, not merely the file;
- under a combining law, contributor lines join origins alongside values —
  `#7d8590 + #f00` from `line 1 + line 2`, so **even a blended cell is
  traceable**, which is more than the original plan would have given.

**Origin is display-only, and that is what makes it free.** Regenerated every
frame, never stored, and **a `Resolution` may not name one**: a resolution names
a source `id`, which is a rune and survives editing, whereas a line number does
not survive editing at all. That is exactly the identity problem `discovery.md`
names as the hard part of phase 2 — and phase 1 gets to *sidestep* it by
refusing to persist anything. Phase 2 will not have that luxury, and the page now
says so.

### The invariant that nearly slipped

First implementation kept the origin of whichever cell was appended first when
two rules stated the **same value at the same strength**. Harmless-looking:
origin is display-only and cannot change a value.

But it means reordering a source's cells changes the output — and
[composition](/../../VoidAllomone/okf/concepts/composition.md) claims `merge` depends only on
the **set** of its inputs. A display-only field is still output.

Ties now break lexicographically on `origin`. Pinned by a test that reorders two
cells and asserts both the origin and the whole canonical form are unchanged, and
that test was **confirmed red** by disabling the tiebreak. The general form is
worth keeping:

> **"It cannot affect the result" is not the same as "it is not part of the
> result."** An invariant about the whole output does not get an exception for
> the fields nobody looks at.

### Verification

Five expectations derived from the design and then run: conflict contributors
carry the right lines; the sharper rule's line wins within a source; combining
laws leave the cell's origin empty while joining contributor origins; an explicit
`Resolution` value clears origin (no rule produced it) while a `winner`
resolution adopts that source's; and reordering is invariant. All five matched.

Both deliberate breaks — the tie-break and, yesterday, the definition-side
negation XOR — went red at the right assertion and were restored.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT** with the five
freshness warnings resolved by truing up the pages whose code moved.

## 2026-08-07 (cont.) — The interaction graph, and the outcome nothing could report

Roadmap item 3, and the author's own question answered per edge: *"What happens
when a rule interacts? If the set of runes that a rule is modifying is the same…
if it's modifying the same property, then hopefully there is some existing
definition on how these properties interact."*

    A ~ B   iff   matched(A) ∩ matched(B) ≠ ∅   and   property(A) = property(B)

Undirected, per-rule, in `sentinel.hpp` beside the effect graph — deliberately
co-located so the contrast stays in front of whoever reads either one. They are
not variations on a theme: one is **directed** and asks *does A feed B*, the
other is **undirected** and asks *do A and B meet*; one is per-script and about
termination, the other is per-rule and about explanation. Neither subsumes the
other, and a cycle in one says nothing about the other.

### The outcome that justifies the whole feature

Five outcomes — `agreement`, `override`, `arithmetic`, `blend`, `conflict` — and
the interesting one is **`override`**, because it is the only outcome that
**silently discards something an author wrote** *and* the only one the system had
no way to report at all:

- a ⊤ gets a conflict panel;
- a blend produces a visibly combined value;
- an override produces a perfectly ordinary-looking cell, and the losing rule
  leaves **no trace anywhere**.

You could not previously ask *"what else had an opinion about this?"* without
already suspecting the answer. Now every meeting is listed with the law that
resolves it.

### Three decisions worth keeping

**`agreement` outranks every law**, and is checked before the law is consulted.
Two rules asserting the same value are not overriding each other and not really
combining either — calling it a blend or an override would be false.

**`same_script` is carried, and the surface must respect it.** Within one script
an override *is* the defaults-and-exceptions idiom, the entire point of writing
rules; nagging about it would make the intended thing feel like an error — the
same mistake the within/across split exists to avoid. Across scripts the
identical shape may be a genuine surprise. The graph reports both and lets the
UI decide, which is the right division: the analysis should not have taste.

**The law genuinely decides what a meeting MEANS.** Declare `color` as `Custom`
and all three colour pairs in the demo stop being overrides and become blends —
including the *within-script* pair. That is the 2026-08-06 blending fix seen from
the other side, and it is a pleasing confirmation that the two mechanisms agree
about the same underlying fact.

### Cost, kept honest

`allo_matches` runs **once per rule**, not inside the O(rules²) pair loop, and
rules that match nothing are dropped before the loop starts — they have no cell
to meet anyone at. The pair loop is then set intersections over sorted vectors.

Canonical order (`a < b`, output sorted by `(property, a, b)`) is not cosmetic:
without it the report would depend on which script was passed first, and the
order-independence test catches exactly that — **disabling the swap turned three
assertions red, including one that had nothing to do with ordering**, because a
non-canonical edge simply cannot be found by the lookup that expects it.

### Verified the usual way

Seven expectations derived from `discovery.md` and `composition.md` **before**
running, all seven matched: within-script override, across-script conflict,
agreement across scripts, no edge across different properties, no edge across
disjoint subjects, combining laws reclassifying every pair, and order
independence. The cross-check that matters most is that the graph's `conflict`
count and `merge().conflicts()` agree — two analyses of the same rule set must
not disagree about ⊤, and the test asserts both.

Both deliberate breaks — the `agreement` short-circuit and the canonical
ordering — went red at the right assertions and were restored.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT**.

Roadmap items 1–3 are now done; 4 (the conflict inspector) is next and has been
made materially easier by `origin` landing first.

## 2026-08-07 (cont.) — Allmusely, and where taste is allowed to live

Design only; nothing built, at the author's explicit direction. Recorded in
[horizons](/horizons.md), which is precisely the file for *"a destination that
must not be foreclosed, and what it demands of Void Maiz today."*

The author, connecting two threads: Allomone is being built deliberately without
taste (*"the analysis shouldn't have taste"*), while Allmusely — the UI authoring
tool on the horizon — would be *"dedicated towards the creation of a GUI based on
rules and interactions with a specific **motivation** or **goal** in mind."*
Flagged as possibly out of scope, *"but I do feel it's worth considering early
on."*

**It is not out of scope. It is the predicted client of a seam already designed
and deliberately left unbuilt.** Yesterday's
[governance](/../../VoidAllomone/okf/concepts/governance.md) triage rejected the Sentinel
memo's Harmony Score **as a library concept** — *"Allomone does not know what a
GUI is, and must not"* — and in the same breath re-homed it: *"Where a visual Φ
belongs: the host… a host-supplied `HarmonyFn(const Merged&) -> double`… not
needed until a host asks."* **Allmusely is the host that asks.** The boundary was
drawn from one side yesterday and approached from the other side today, and it
holds.

### The constraint worth settling early

A **goal** and a **lattice** are different mathematical objects, and that decides
the layering rather than being a technicality:

- the merge is a **join semilattice** — a partial order with least upper bounds,
  producing ⊤ where two opinions are *incomparable*. Refusing to rank
  incomparable things is not a limitation; it **is** what ⊤ is;
- a goal is a **preference order / objective function** — it ranks. Strictly
  stronger, and not extractable from a lattice.

So Allmusely cannot be "Allomone with a goal knob": an objective in the merge
makes everything comparable and ⊤ disappears by construction. The rule adopted:

> **Taste belongs in the ranking of proposals, never in the merge.**

The Weaver already has exactly that shape — compute asymmetries, offer, let a
human accept — so a goal adds an opinion about *which proposals to show first*
and touches nothing else.

**And a goal makes propose-don't-generate MORE load-bearing, not less.** Once a
score exists the tempting move is to hill-climb: generate rules, keep the ones
that raise Φ. That is Knuth–Bendix completion with a heuristic attached, and
completion diverges. The existing ruling gets *stronger* here rather than
relaxing — which is a pleasing result, because it means the safety argument
already written covers a case it was not written for.

### Where a goal would come from, and the mechanism already filed

Declared by the designer, or **learned from resolutions** — and the second is the
interesting one. Every settled ⊤ is a recorded human preference between two
sources, and resolutions are already model content: logged, attributed,
replayable. A set of resolutions is a **pairwise comparison graph**, which is
exactly what **HodgeRank** decomposes into a consistent global priority plus its
inconsistency.

That is *learning the designer's taste from what they keep choosing*, using a
mechanism the 2026-08-06 vocabulary audit already identified and filed at low
priority — low **because nothing today needs it**. Allmusely is what would
promote it, and the note exists so nobody retires it as speculative first.

### Three forcing functions, named before they surprise us

1. **Stable rule identity.** A tool reasoning about *which rule to change* must
   name one across edits. `Annotation::origin` is display-only **on purpose** and
   cannot carry that. Allmusely is the client that forces the identity problem
   [discovery](/../../VoidAllomone/okf/concepts/discovery.md) phase 1 was designed to dodge.
2. **A host-extensible Weaver.** `settle`/`unheard`/`gap` are hardcoded; a design
   tool wants its own kinds (*"these two buttons have different padding —
   unify?"*). Same registry shape as `PredicateRegistry`.
3. **Relations as annotation targets.** Allomone annotates `(subject, property)`,
   but spacing, alignment and hierarchy are **between** subjects. A host can
   encode the pair as a subject (`subject` is an opaque host string, so `"a|b"`
   is legal) — expressible, awkward, and better recognized now as a structural
   question than discovered later as a workaround.

### The thing that must not happen

**Do not design Allomone for Allmusely.** The engine is good precisely because it
does not know what a GUI is — that is what lets the same merge serve
`temperature`, `priority` and a contact list. A goal primitive in the kernel
would buy one application and cost every other one, and it does not come back.

**Net effect today: nothing changes**, which is the answer `horizons.md` exists
to give. No current decision forecloses it, the one seam it needs is already
specified, and both mechanisms it would want are recorded as
unbuilt-because-unneeded rather than unconsidered.

## 2026-08-07 (cont.) — The conflict inspector, and a rule that failed three times and became a type

Roadmap item 4, which completes the small-and-independent tranche (items 1–4 of
the ranked list are now done).

### `explain_cell()` — and why it is the library's job

The pane is a browser's *computed styles*: the winner on top, everything it beat
struck through beneath, each row saying **why** it ended up there. Seven
verdicts: `winner`, `agreed`, `overridden`, `tiebreak`, `tied`, `contributed`,
`superseded`.

The classification belongs in `annotate.hpp` rather than in each host, and the
argument is concrete rather than tidy-minded. A host sorting contributors by
strength and calling the top one the winner is wrong in exactly three places:

- a **combining law** has no winner at all — the answer belongs to everyone;
- a **resolution**'s winner may not be in the contributor list;
- **agreement** is not an override — two sources saying the same thing lost
  nothing.

Those are the interesting cases, and getting them wrong produces a *confidently
misleading* explanation, which is worse than none. So `explain_cell` reads
`settled_by` — the merge's own record of how the cell resolved — instead of
re-inferring from the numbers. Inferring would be a second implementation of the
merge's reasoning, and the two would drift. Same rule as ever: **if the model can
be asked, do not keep a second copy.**

Two decisions worth keeping:

**`tied` is deliberately NOT struck through.** A ⊤ discarded nothing; it declined
to choose. Only `overridden`, `tiebreak` and `superseded` mean *something an
author wrote is not in the answer*, which is the single fact the pane exists to
convey — and striking a tied row would say the opposite.

**The pane works on ANY cell, not only conflicted ones.** *"Why is this the
colour?"* is worth answering when nothing is wrong, so the demo's derived-cells
table is now expandable per row. A debugging tool that only appears once things
are broken is a tool you have to already suspect something to reach for.

**Where the devtools analogy stops**, recorded so it is not mistaken for a bug:
there is **one row per source, not per rule** — `contributors` is already each
source's single best opinion, so a script's own default beaten by its own sharper
rule never appears. That is correct (within-source overriding is the idiom, not
an event), and the per-rule view is the interaction graph built earlier today.
Two panes, two questions: *why does this cell say what it says* versus *which
rules meet, and what happens.*

### The rule that failed three times

Writing the tests, `annotate_smoke` **crashed with `bad_alloc`**. The cause:

    const MergedCell* c = merge({sharp, weak, same}).find("x", "c");   // DANGLING

`merge` returns by value; `find` returns a pointer into it; the temporary dies at
the end of the statement. This is the **same bug as `weaver_smoke`'s**, which was
found yesterday, written up in
[agent-authoring](/../../VoidAllomone/okf/concepts/testing/agent-authoring.md) as a never-do —
and then committed again, by the author of that page, the day after writing it.

Auditing the codebase found **24 call sites** of the pattern, of which several
predated today and were live dangling reads that happened to pass.

**So it stopped being a rule and became a compile error.** `Merged::find` and
`Merged::conflicts` are now **lvalue-only**, with the rvalue overloads
`= delete`. `merge({a, b}).find(...)` no longer compiles at all; the compiler
enumerated all 24 sites in one pass.

This also forbids the *genuinely safe* one-expression use, and that is the right
trade: the safe and unsafe forms are **visually identical**, and naming the
`Merged` costs one line. `value()` returns by value and is unaffected.

The general lesson, now in the authoring manual:

> **When discipline fails three times, stop writing discipline and change the
> type.** A guideline an LLM re-reads and re-breaks is worth less than a
> signature that cannot be misused — and here the language enforces it for free.
>
> Corollary: **when you catch yourself repeating a mistake you have already
> documented, the documentation is not the fix.** Look for the structural change
> first.

The test-side repair is the same idea one level up: a `cell(maps, subject,
property, opts)` helper that returns the `MergedCell` **by value**, which removes
the lifetime question rather than routing around it, and keeps the one-line
assertions one line.

### Verified

Seven expectations derived from the design before running — sharper-wins,
agreement, ⊤ with a below-strength loser, combining, resolution-by-value,
resolution-by-winner, recency — all seven matched.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT**.

Ranked items 1–4 done. Remaining small ones: **5** (wire the census's
glyph-field discovery into Allomone) and **6** (the effect graph on
`edit_canvas`).

## 2026-08-07 (cont.) — The third vocabulary, and a roadmap item that cited something unimplemented

Roadmap item 5, and the second item this week whose *stated mechanism* turned
out not to be available. The pattern is worth naming as much as the feature.

### The citation that did not hold

[discovery §3a](/../../VoidAllomone/okf/concepts/discovery.md) said glyph-field discovery was
free because *"the surface census already harvests exactly this… already
written, currently pointed at documentation instead of at the language."*

**The census is not written.** `census.hpp` is a contract — it says
`NOT YET IMPLEMENTED` in its own second line — and there is no `src/census/`, no
CMake target, no test. The page had quoted the header's *description* of tier 0
and turned it into a claim about existing code.

Nothing was blocked, because **the projection already carries the same
information**. `project_scene` is handed the glyph descriptors, so every
`SceneNode` arrives with its declared fields, and the vocabulary is a fold:

    for (node : scene.nodes)
        for (field : node.fields) glyph_fields[field.key].insert(node.glyph);

Three lines, in exactly the shape the other two vocabularies already use. The
census remains the right eventual home; it was never the only road.

`surface-census.md` now opens with a **NOT IMPLEMENTED** banner. Its
`status:draft` and missing `resource:` were technically honest and evidently not
loud enough — which is the transferable bit: **the honesty tags protect a reader
who checks them, not a reader who quotes the prose.** A page describing a
contract should say so in words, at the top, where a citation will land.

### What the third vocabulary actually buys

Not a list of names — **a collision**. `label` is declared by every widget glyph
*and* written by a rule, and because Allomone is derive-only, `then label "!!
danger"` does **not** write the rune's field. It derives an annotation this host
renders in preference to the stored one.

That is a legitimate design and an easy thing to misread, so it is surfaced
three times rather than left to be discovered: completion marks the property
(*"also a field on 4 glyphs"*), `explain` states the consequence, and the
diagnostics tab lists every shadowed property in the current script.

**It is not an error.** It is the derive-only commitment becoming *visible* at
exactly the moment someone might expect a write — and the payoff is stated in
the same breath: disable the script and the original labels come straight back,
with nothing to undo. A commitment nobody can see is indistinguishable from a
bug; this makes it legible at the point of confusion.

The seed script now carries the case, because a warning that never fires in the
demo is a warning nobody will believe:

    # `label` is ALSO a field every widget glyph declares.
    # deriving it does NOT write that field - allomone is
    # derive-only. disable this script and the original
    # labels come straight back, with nothing to undo.
    when has "danger" then label "!! danger"

`explain` for a property also stopped consulting a hardcoded list of four names
and now answers from what is discovered — its merge law, and whether any glyph
declares a field of that name. One fewer second copy of something the model can
be asked for.

### Two roadmap items, two bad citations, one lesson

Item 2 (rules-as-runes phase 1) prescribed a mechanism that was **unsound** —
one `ConstraintMap` per rule would have broken the CSS cascade split. Item 5
cited a component that was **unbuilt**. Both were written by the same author,
days apart, and both were caught the same way: **by trying to use the stated
mechanism before trusting it.**

The roadmap's own promise is *"if a claim elsewhere is not listed as built here,
it is not built"* — which protects against a *page* over-claiming, not against a
page citing another page's aspiration. The cheap habit that catches both:
**before building a roadmap item, check that the thing it names exists and does
what the item assumes.** Two for two so far.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT**, and the seeded
theme script parses to 3 rules + 1 definition with `label` correctly derived on
the danger-tagged subjects only.

Ranked items 1–5 done; **6** (the effect graph on `edit_canvas`) is the last of
the small tranche.

## 2026-08-07 (cont.) — The effect graph, drawn; and the tranche closes

Roadmap item 6, which completes items 1–6 — the small, independent tranche
identified on 2026-08-06.

### The third correction in three items, and it is getting cheaper to find

[emergence](/../../VoidAllomone/okf/concepts/emergence.md) asked for the graph on
**`edit_canvas`**. Checking the named mechanism first — now a habit rather than
a reaction — found the wrong verb:

**`draw_canvas`, not `edit_canvas`.** The effect graph is **derived from the
scripts' text**. There is nothing in it a person could meaningfully drag, link
or delete, and `edit_canvas` would offer exactly those gestures, compiling
commands that could not be honoured.

> **Offering a gesture whose command cannot be honoured is worse than offering
> none.**

That is the same principle as ⊤ refusing to name a winner and `Merged::value()`
returning `""` for a conflicted cell: **the surface must not imply a capability
the model does not have.** A projection of an analysis is read-only for exactly
the reason the derived cells are, and `draw_canvas` — render-only, already in
the library — is the tool that says so.

**And it needs no Core and no mantle.** `Scene` is plain data, so
`effect_scene(graph)` is a **pure function** — no commands, no doc mantle, no
I/O, testable with no ImGui present. Materializing the analysis into runes to
display it would have been the census's shape, and would have meant *writing in
order to report*, which the Sentinel layer specifically must not do.

Three items, three corrections — unsound (item 2), unbuilt (item 5), wrong tool
(item 6) — and all three were caught the same way, before any code. The habit is
paying for itself and costs about two minutes.

### The layout is the explanation

This is the argument for drawing the graph at all rather than listing it, and it
is why the picture is worth more than the verdict string:

- **columns are STRATA.** Stratum 0 reads nothing any script writes, 1 reads
  only 0, and so on — so **left-to-right IS the evaluation order acyclicity
  buys.** The picture states the guarantee rather than illustrating it.
- **a cycle has no strata by definition**, so its members fall into one column.
  *"These could not be given an order"* drawn as literally that is the
  diagnosis, not a decoration of one.
- **cycle members are coloured and tagged `cycle`**; the wires closing a loop
  are marked `active`, which is the canvas's own emphasis channel used for the
  thing it exists for.
- **each wire carries the shared symbol as its `relation`**, so *"D writes
  prop:color, which B reads"* rides on the edge instead of living in a message
  somewhere else.

Over derive-only scripts the picture is a tidy left-to-right pipeline, every
time. That is the correct baseline: it starts life legible and stays legible
until something real changes — the same argument as the analyzer starting life
passing.

The deliberate break confirmed the claim rather than the code: flattening the
column arithmetic turned the two strata assertions red and nothing else, which
is what a test of *the layout's meaning* should do.

### The tranche is closed

Items 1–6 done: `define`, rule-level provenance, the interaction graph, the
conflict inspector, glyph-field discovery, and the effect graph drawn. What
remains on the ranked list is medium and large — `let` and set algebra, spectral
clustering, rules-as-runes phases 2–3 (which still carry the identity problem),
name↔id references, the write tiers.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT**.

## 2026-08-07 (cont.) — "You sound dogmatic": the tone note, and the false claim it uncovered

The author: *"just based on your language, you sound very dogmatic? I would like
to remind you to ground yourself in mathematical theory and keep those
foundations."*

Correct, and the useful response was not to soften the prose. Re-reading the last
several entries, the problem is specific: **design choices were being written in
the same imperative register as theorems.** These sat side by side, indistinguishable:

- *"negating a conjunction needs `or`"* — De Morgan, a theorem;
- *"completion procedures diverge"* — Knuth–Bendix, a known result;
- *"offering a gesture whose command cannot be honoured is worse than offering
  none"* — a **design preference**, defensible, derived from nothing;
- *"taste belongs in the ranking of proposals"* — a **choice**.

Writing all four as maxims launders the last two into necessities. The fix is to
mark which is which, and the way to find where it had done damage was to check
the most-repeated mathematical claim.

### The claim did not hold

[foundations §1](/../../VoidAllomone/okf/concepts/foundations.md) has said since it was written
that the merge is a **join semilattice over values, with `⊤` as the top element**,
and listed associativity among the axioms it satisfies.

**That is false**, and the counterexample is small enough to have been checked at
any point. With "highest strength wins; equal strength and distinct values gives
⊤", take `a=(x,2)`, `b=(y,1)`, `c=(z,1)`, `y ≠ z`:

    (b ⊔ c) = ⊤        equal strength, distinct values
    a ⊔ ⊤   = ⊤        if ⊤ absorbs
    (a ⊔ b) = a  and  a ⊔ c = a        because 2 > 1

`⊤ ≠ (x,2)`. **Not associative.** Confirmed against the engine: `merge{b,c}`
conflicts, `merge{a,b,c}` settles on `x`.

### The engine was right; the description had drifted

The domain is not values. It is **`(strength, set-of-values-at-that-strength)`**,
and `⊤` is not an element at all — it is the predicate `|V| > 1` read off the
final element:

    (s₁,V₁) ⊔ (s₂,V₂)  =  (s₁,V₁)        if s₁ > s₂
                          (s₂,V₂)        if s₂ > s₁
                          (s₁, V₁ ∪ V₂)  if s₁ = s₂

Commutative, idempotent, **and associative** — the counterexample evaporates
because `b ⊔ c = (1,{y,z})` is an ordinary element that a strength-2 opinion
still beats, rather than an absorbing top.

`Best::values` is literally a value→strength map and the merge takes the top
strength before comparing holders, so **the implementation has always realized
the correct lattice.** Only the prose was wrong — which is the worst place for it
to be wrong, because the prose is what a second implementer would build from.

Two behaviours that read as special cases turn out to be consequences: a sharper
rule from a third script silently clears a would-be conflict (it wins the
strength comparison, so `V` is *replaced* rather than joined), and a `Resolution`
settles one without touching the algebra (it is a rendering choice over the final
element).

### The smuggest paragraph was the wrong one

[invariants §A3](/../../VoidAllomone/okf/concepts/testing/invariants.md) said associativity was
*"enforced by the type signature instead of by a test"* and *"a better guarantee
than a passing test, because a test can only sample."*

Wrong twice. **A signature that cannot express a question does not answer it** —
and under the domain the page itself described, the operation was genuinely
non-associative, so the API was *hiding* a real failure rather than preventing
one. Rewritten with what is actually assertable, and with a stronger obligation
than before: **if a binary `merge(Merged, Merged)` is ever added, it must operate
on `(strength, value-set)`, not on rendered cells** — a conflicted cell that has
already collapsed to "⊤" behaves as an absorbing element, and joining two of
those across peers would depend on grouping. Recorded in
[multi-user](/../../VoidAllomone/okf/concepts/multi-user.md) too, because "derived state never
syncs" is exactly the kind of boundary that gets relaxed later as an
optimization.

### Tested, not just described

`annotate_smoke` gained the case that distinguishes the correct domain from the
naive one: **a strength-2 opinion beats a tied pair.** A re-implementation with
an absorbing `⊤` fails precisely there, and nothing else in the suite would have
caught it.

### What to carry forward

The tone note found a real defect, which is the argument for taking it as
technical rather than stylistic. Two habits out of it:

- **Mark the register.** A theorem, a measurement and a preference are three
  different kinds of claim and should not share a sentence shape. "We chose X
  because Y" is not weaker than "X is required" — it is more accurate, and it
  tells the next person what is open.
- **The most-repeated claim deserves the most checking.** This one survived
  because it was load-bearing and quotable, not because it had been verified. The
  frequency was doing the work confidence should have been doing.

**Verified:** build clean, 11/12 tests (only the environmental
`reduce_conformance`), OKF **39 concepts, 0 errors, CONFORMANT**.

## 2026-08-07 (cont.) — A sources folder, and the uncomfortable finding about token cost

The author, after the dogmatism note: sources should be recorded and
occasionally verified, *"maybe you hallucinate later and say 'Albert Einstein was
the creator of the Pythagorean theorem', well then hopefully someday later a
verification scan will catch that"* — plus a larger question about whether the
OKF needs more structure so agents stop burning tokens, and whether that is a
Void Core matter.

### Two failure modes, and they need different tools

Separating them was the useful part, because conflating them spends effort on the
wrong one:

| failure | example | tool |
|---|---|---|
| **wrong attribution** | wrong author, year or result | a **source rune** — recorded once, checkable later |
| **wrong reasoning** | a true theorem applied to the wrong domain | a **counterexample** — checked in place |

Yesterday's semilattice error was the second kind, and **no citation would have
caught it**: the axioms were quoted correctly and applied to a domain they do not
hold over. A three-line counterexample caught it in a minute.

So [sources](/sources/index.md) adopts a narrower rule than "cite everything":

> **Cite what you cannot check locally. Check what you can.**

A citation on a locally-checkable claim is ceremony — it costs tokens and buys
nothing, because the citation would have been correct and the claim still wrong.

### Citation by body link, and why that specific choice

`cites:` in frontmatter was considered and rejected. The spec's *"tolerate
unknown keys"* rule means it would have worked — but `bundle.py` extracts links
from the **body**, so frontmatter citations would be invisible to the graph.

Citing by ordinary markdown link earns two things with no new machinery:

- `okf get sources/<name>` lists everything citing it under `linked from`, and
  **that list is the blast radius**. If a source is wrong, the graph names
  exactly which pages inherit the error. *A bibliography tells you what was
  cited; a graph tells you what breaks.*
- `okf analyze` treats sources as nodes, so an over-leaned-on source shows up as
  high-centrality for free.

Verified against our own bundle: `get sources/knuth-bendix-completion` correctly
reports `linked from ← concepts/allomone/materialization`.

### Everything starts `asserted`, and that is the point

The glossary's `confidence:` vocabulary already fits, with a reading specific to
sources: `verified` means *someone opened the artifact*. **None of the nine
seeded sources are verified** — they were recalled, not checked. Marking them
`asserted` makes `okf query confidence:asserted` the work queue for a later pass;
a folder that called its own recollections `verified` would be worse than none.

Each source therefore ends with **what a verification pass should check**, stated
specifically. *"Check this is right"* is not actionable; *"confirm the
pheromone/allomone distinction is interspecific-and-benefits-the-emitter, because
the naming argument in index.md depends on which allelochemical it is"* is. Two
were flagged as most-likely-wrong on their own merits: the CALM proof attribution
(*"later proved by…"* is exactly the clause that comes out fluent), and
Mimram & Di Giusto, which we cite **second-hand** through Void Palabra's summary
rather than from the paper.

Seeded with nine load-bearing works — the ones where a wrong attribution would
mean a design decision rested on something that does not exist. Knuth–Bendix is
the heaviest: the entire propose-don't-generate ruling rests on completion's
divergence.

### The uncomfortable finding on token cost

The author's concern is real and the honest answer is not flattering: **the
tooling largely exists and I was not using it.**

`okf ls --tag`, `okf query <tag-expr>`, and `get`'s `links →` / `linked from ←`
are a genuine navigation surface. I had been reading whole concept files with a
generic file tool — often 300+ lines to confirm one sentence. That is a
discipline gap, not a missing feature, and it means the highest-value upstream
work is making the existing verbs *reliable* rather than adding new ones.

Which produced the one thing that genuinely blocks it — a **bug**:

> `okf get` raises `UnicodeEncodeError` on any concept containing a non-cp1252
> character. `⊤` appears throughout the Allomone pages, so **the
> token-efficient read path is unusable on Windows today** and the fallback is
> exactly the expensive behaviour it exists to prevent. `bundle.py` reads UTF-8
> correctly; the failure is on output, where `print` inherits the console
> codepage.

Sent upstream with a one-line suggested fix, alongside a `get --head` request
(the header is already computed; the body is what costs), the source convention
offered rather than requested, and a note that
`context-optimization.md`'s *"read verbs return summaries, not dumps"* principle
is scoped to dispatcher verbs and applies equally to the bundle CLI — which is
where the large documents actually live.

Void Core **is** thinking about this: `context-optimization.md` (2026-07-01,
`status:planned`) argues altitude-before-summarization and names chunked listing
and context budgets as buildable now. It simply has not been pointed at the OKF
CLI.

### On the author's "the OKF is a mantle, documents are runes"

Worth recording that this is not an analogy. Void Core's OKF engine **produces**
a bundle from a mantle and **consumes** one back, losslessly — concept ↔ rune,
link ↔ layout edge, tags ↔ tags. So sources get tags, links and `analyze` for
free precisely because they are runes, which is why the blast-radius query needed
no new code.

**Verified:** OKF **48 concepts, 0 errors, CONFORMANT**; tests unchanged.

## 2026-08-09 — Void Core replied with four fixes; one correction back, and the convention caught us

`MESSAGE_FOR_VOIDMAIZ_core-cli-fixes-and-source-provenance-2026-08-09.md`. All
four asks answered, nothing deferred. Verified each against our own bundle before
acting on it.

### What they fixed

**The crash.** Fixed at the CLI entry point rather than in `get`, so every
command is covered. Confirmed: pages full of `⊤` now render, and their header's
`·` separator — which had been silently mangling — comes through clean. They
noted we **undersold** it: `ls` and `validate` only survived because *our titles*
happened to be cp1252-safe, and a `⊤` in a title kills those too.

**`get --head`.** Header, `description:`, tags, `resource:`, and the link graph
both ways. Measured on `concepts/allomone/composition`:

| | bytes |
|---|---|
| full `get` | 17,412 |
| raw file — **what I was actually reading** | 16,679 |
| `--json` | 1,656 |
| `--head` | 1,344 |

### The correction I owe them

**`--json` never carried the body**, and my §2 claim that it "still includes the
body" was false. Worse than the correction: I was using neither. I had **two**
adequate header-only tools available and read raw files instead, at **12×** the
cost of `--head`.

That is the same shape as the finding I sent them — *the tooling exists and I was
not using it* — except this time I had also written a page about it. Knowing a
tool exists and reaching for it are different skills, and only the second one
saves anything.

### Two corrections back, on their #1 verify item

While writing their own Lafont page they found that the **γγ-swapped /
δδ-straight asymmetry in the reduce contract did not come from Lafont** — it came
from us, and they adopted it on the strength of the argument. It is now normative
for every conforming implementation, so they rank it #1 to verify. Both
corrections come from our own log, checked rather than accepted:

- **The date is 2026-07-14**, not 07-13.
- **It was not unattributed — it was attributed to Lafont, by us, from memory.**
  Our log records the fix as *"`swap: true` … — **Lafont's γγ**, the
  parallel-arcs drawing between mirrored bodies"*, and the author's diagnosis was
  literally *"that's δδ."* A correction **toward** Lafont, not an invention.

The second matters because it changes what the fix is. "Unattributed" invites
*add a citation*; what actually happened is `confidence:asserted` in the exact
sense the new spec defines, and the repair is a yes/no question:

> Does Lafont's γγ connect `x_i ≡ y_{n+1−i}` (swapped) and δδ connect
> `x_i ≡ y_i` (straight)? Or the other way round?

If reversed, cases 11–14 encode a mistake and every conforming implementation
inherits it. Recorded as #1 on our own [Lafont page](/sources/lafont-interaction-nets.md)
with the same ranking.

### Adopted: `type: Source`, and a lifecycle that fell out

They ruled `Source` distinct from `Reference` — *a `Reference` is material we
wrote to be referred to; a `Source` is an external work we are trusting*, and
only the second can be wrong in a way we would not notice. All nine converted.

They also excluded `Source` from `validate`'s doc-type exemption, so a source
without a `resource:` is flagged. **Checked rather than assumed: ours are not
flagged**, because that rule fires on `status:current` and every source here is
`status:draft`. That is not a loophole — the two axes compose into a lifecycle:

    draft   + asserted + no locator     — recalled, never opened
       │  someone opens the artifact
    current + verified + resource:      — and the honesty rule now guards it

A source whose attribution has never been checked **is** a draft, and it acquires
its locator by the same act that verifies it.

**Only OKLab has a locator, deliberately.** A DOI recalled rather than looked up
is exactly the artefact this folder exists to catch — plausible, well-formed,
unverifiable, and then *trusted more* than the prose around it because it looks
primary. Nothing missing is visibly missing; a wrong DOI is visibly present and
wrong in the most authoritative-looking place on the page.

### The convention caught us within the hour, too

Re-reading our testing pages after their reply:
**`testing/plan.md` §2 still asserted that associativity is "guaranteed by the
signature rather than sampled by a test"** — corrected in
[invariants §A3](/../../VoidAllomone/okf/concepts/testing/invariants.md) **two days earlier**
and never propagated. The page spent two days contradicting the page it links to.

It is the same failure as their Lafont note, in miniature, and it names a **limit
of the convention we both just adopted**:

> **The blast radius only covers what was cited.** A claim *paraphrased* into a
> second page is invisible to the link graph, because there is no edge to walk.

Neither project has a tool for that residue. Recorded in the correction itself
rather than quietly rewritten, and sent back to them, because it is the honest
qualification on a convention that otherwise looks complete.

### On their closing ruling

They declined to add a *"what do I need to read for task X?"* verb, on our own
reasoning: `--head` just made triage ~9× cheaper, so the honest test is whether
`ls --tag` + `get --head` used **with discipline** is now enough. *"We would
rather build the verb you can describe from experience than the one we guessed
at."* Correct, and the burden is ours — the tool has been fixed; the habit has
not been demonstrated.

**Verified:** OKF **48 concepts, 0 errors, CONFORMANT** (`external=1, fresh=10`);
C++ suite unchanged at 11/12.

## 2026-08-09 — Prefix fuzzing: a real bug, and two of my own invariants refuted

Step 2 of the testing programme, and the **first piece of it actually built**:
`tests/fuzz_smoke.cpp`. No Python, no generator, no model — a corpus and a loop.

### Taken out of order, and that was right

The sequencing said *"the JSON case runner… **nothing else can land before
this**, because it is what makes a generated finding permanent."* True of
anything the **harness** produces. **Not** true of a hand-written C++ loop, which
needs nothing the runner would supply. Treating it as a blocker would have
deferred a cheap win behind an expensive one; the sequencing note now says so.

### The bug it found on the first run

Non-ASCII **outside a string literal** produced **one `Invalid` token per BYTE**.
`café` gave two tokens for `é`, an emoji four — each boundary landing *inside* a
codepoint. Since [the token offsets are normative](/../../VoidAllomone/okf/concepts/language.md)
and the editor draws `CodeSpan`s as byte ranges, a span would have ended
mid-character and the renderer would have drawn a broken glyph.

Fixed: `utf8_len()` consumes a whole sequence into one token, clamped to the
bytes actually present, and returns ≥1 always so the tokenizer cannot loop on
malformed input. Inside a string, UTF-8 was always safe — bytes are copied
verbatim — and that asymmetry is now pinned too.

A pleasing coincidence of timing: this is the **second Unicode-on-output bug in
two days**, after Void Core's `okf get` crash. Both had the same shape — code
that reads UTF-8 correctly and then hands out byte-level pieces that are not
character-aligned.

### And two of my own invariants were wrong

Both refuted by the corpus within a minute, both recorded rather than quietly
edited, because [invariants](/../../VoidAllomone/okf/concepts/testing/invariants.md) opens with
*an invariant stated too broadly is worse than no invariant* and these are the
demonstration.

**"No token may begin on a continuation byte."** Too strict. `\x80\x80\x80` is
three stray continuation bytes belonging to **no character at all** — there is no
correct grouping and no boundary there can split anything. The checker now scans
the buffer, marks the interior offsets of **well-formed** sequences only, and
tests against those. The precise property was never "no boundary on a
continuation byte"; it is **"no boundary inside a well-formed codepoint."**

**"Rule count never decreases as the prefix grows."** Simply false, and the
design says so:

    when tag "danger" then color "#f85149"     -> 1 rule
    when tag "danger" then color "#f85149",    -> 0 rules

The trailing comma opens an effect that is not there yet, so the rule is
malformed and dropped — invariant D3, *one broken rule costs exactly one rule*.
Monotonicity was an appealing property the design **deliberately does not have**,
and it came from intuition rather than from `language.md`. Replaced with the
weaker true one: *a prefix never yields more rules than the whole script*, which
still catches a destructive resynchronizer.

Worth naming the pattern, since it is the same one as the semilattice error two
days ago: **the invariant that feels obvious is the one written without checking
the spec.** Both times the code was right.

### What the test covers

466 prefixes over an 8-script corpus, plus ~24 hostile inputs: unterminated
strings, lone backslashes, an embedded NUL, truncated UTF-8 at every length,
stray continuation bytes, a 5,000-character identifier, and a **10,000-term
condition** (valid and cheap — the grammar has no nesting, so it grows a vector
rather than a stack, and `strength()` comes back 10001).

Deliberate break confirmed the UTF-8 assertions bite: reverting `utf8_len` to a
one-byte step turned five checks red, including two prefix cases and the `⊤`
corpus entry.

### Also

**Q22 filed** — should identifiers accept non-ASCII? Today a *value* can be any
UTF-8 (they are quoted strings) but a property or predicate *name* cannot, so
`then température "18"` does not parse. Lean: ASCII-only for now. Widening is
**additive** so it breaks nothing later, and doing it correctly is not one line —
`isalnum` is locale-dependent, so an honest version needs XID_Start/XID_Continue
plus a normalization decision, or `é` composed and `é` decomposed become two
different properties.

**Verified:** 13 tests, 12 passing (only the environmental `reduce_conformance`);
OKF **48 concepts, 0 errors, CONFORMANT**.

## 2026-08-10 — Adoption readiness: the audit, the last gap, and Hormiga told

The author's call: *"try to get something usable by Void Maiz based
applications… Allomone is still incomplete, but we have something working well
enough."* Plus the right instruction for how to check: *"if you believe it's
already ready, then double check the OKF or something."*

So the pass answered one question — **could a host sit down today and build a
domain Allomone?** — by testing rather than asserting. Answer: yes, after two
fixes, both in the first place an adopter would land.

### The gap: the library emitted commands naming a glyph it did not ship

`compile_resolution()` emits `rune new allomone-resolution …` and four `set`s,
and **shipped no descriptor for that glyph**. A first host had to
reverse-engineer the name and its exact field list from our source.

`UserGraph::affordance_glyph()` exists *because that identical mistake already
shipped once* — a descriptor built with `name`/`content` instead of
`glyph`/`fields` registered nothing, and materializing the user graph had never
worked. **The rule had been learned and then half-applied.** Now stated
generally, in the header:

> **Wherever the library emits a command that names a glyph, the library ships
> that glyph.**

### The test that should have caught it was complicit

`command_smoke` **hand-wrote the descriptor**. So it proved the commands work
*given* the right glyph — never that a host could obtain one. The knowledge the
library failed to ship was sitting in the test, which is the subtler half of the
failure: a test can encode the very context whose absence is the bug.

Rewritten to register **only** `resolution_glyph()`.

### And my first repair asserted the wrong mechanism

I wrote that a missing field declaration makes `set` *"silently do nothing"* —
then broke the descriptor on purpose and **the test stayed green**. `set` and
`get` accept undeclared fields perfectly happily.

The real mechanism is adjacent and worse: **`project_scene` drops them.** A
`SceneNode` carries only the fields the glyph declares, and reading model
content back through the projection is what a host actually does
(`field_of(n, "property")`). So a descriptor right in name and short by one
field yields a Resolution with a silently empty member.

Same shape as the `fields`-as-object bug from earlier the same week:
registration succeeds, `set` succeeds, the projection quietly carries nothing.
The assertion now reads through the projection, and breaking the descriptor
turns it red.

Three for three this week on *derive the expectation, then run it* — and all
three times the first expectation was wrong in an instructive way.

### The deliverable: `examples/allomone_minimal.cpp`

`host-protocol.md` had been pointing adopters at the playground — **1,500 lines
of showcase**, which teaches the pattern badly. The minimal integration is the
whole loop in ~200 headless lines: glyphs (including the library's own),
subjects from a projection, merge laws, a host predicate, parse → check → eval →
merge, render, a real ⊤, and settling it through the dispatcher.

Two deliberate properties:

- **Registered as a test.** It is the first thing a client copies, so it must
  not be allowed to rot.
- **Links no view module.** That is a claim worth pinning rather than repeating:
  rungs 2 and 3 of the opt-out ladder ("use the merge, not the language", "use
  the language, hide the editor") are *genuinely* available — a CLI, a build
  step or an agent can use all of this with no window.

It passed first run and printed a real conflict, a real `explain_cell` trace,
and a real settled value.

### Also checked, because "ready" should mean something

**All nine public headers compile standalone** (`-fsyntax-only`, one include, no
ordering requirement) — a papercut an adopter hits in the first ten minutes and
which nobody had ever tested.

### Hormiga told

`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-allomone-ready-to-adopt-2026-08-10.md`
— four days after they proposed the move, which is longer than it should have
been and is said so in the message.

It carries the five-step adoption path (with **"you can stop after step 2"**
stated plainly, since the merge lives below the language), the four decisions of
theirs we adopted, the one we changed with the reason (their `conflicts.md`
silent-vs-surfaced contradiction, resolved by the CSS within/across split), and
— the part worth writing — **three things we got wrong that they will hit**:
their subsumption proposal collapsing to term-count for this grammar, the
AST-overlay we declined on the strength of their own later message, and the
`resolution_glyph()` bug they would have met on day one.

Q17's genuinely open half went back to them: *should the constraint map just BE
a `Scene` decoration channel rather than a parallel structure?* They have the
real domain; we have a demo.

**Verified:** 14 tests, 13 passing (the fourteenth is the environmental
`reduce_conformance` DLL-load failure); OKF **48 concepts, 0 errors,
CONFORMANT**.

## 2026-08-18 — Hormiga+Reyna's standing asks, a month-old "environmental" failure that was two real bugs, and headless

Two pieces of work: consuming Void Hormiga's superseding message, and founding
[headless](/concepts/headless.md) on the author's direction.

### The message: every ask built, nothing deferred

Hormiga's `hormiga+reyna-standing-asks-2026-08-17` **replaced every earlier
message they had sent us** and adopted a one-open-message-at-a-time convention;
we have adopted the same toward them. Answered in full by
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-asks-answered-and-reduce-was-real-2026-08-18.md`.

- **A1 `SceneWire::weight`** — the core stored it, the CLI reported it, the
  projection silently dropped it. Two lines, but the rule it settles is the
  point and is now on the field: *every edge attribute the core stores makes the
  trip; anything richer belongs on a rune.*
- **A2 `code_editor`** — [host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md)
  grew a **"What you do NOT have to write"** section *before* the obligations,
  opening with their story (a transparent `InputTextMultiline` under a coloured
  overlay, listed as "not taken" without ever being weighed; swapping to
  `code_editor` was mostly deletion). Their diagnosis was the useful part: the
  editor *reads like a feature of the demo and is actually a feature of the
  adoption*.
- **A3.1 `field_of` / `find_field`, A3.4 `arg()`** — both off the backlog into
  the library. Their `null == absent` answer taken with their reason. `arg()`
  ships because **two codebases got the same four lines wrong in two languages,
  both silently corrupting content** (Hormiga's doubled escape truncating a
  script at `don't`; Reyna's Python `s.replace` of a quote with a quote, a
  no-op, leaving a corpus of municipal policy full of escaped apostrophes).
  Verified against `args.c:39` rather than recalled — only a backslash
  *immediately before* an apostrophe is an escape, so escaping backslashes would
  itself have been a bug.
- **A3.2 bare nullary predicates** — `when isolated then …` parses now. Kernel
  predicates still require their argument (`when tag then …` is meaningless, not
  terse), and the sugar fires only at a legal continuation so a typo cannot
  become two terms. Pinned: bare and quoted forms produce the *identical* rule.
- **A3.3 the `.` separator** — grammar unchanged (they did not ask, and `-`
  works), but "Invalid token" was the wrong thing to say to someone whose
  instinct was `map.color`, so the parser now names `-` as the house separator
  at the point of failure, in both the effect and condition positions.
- **Q17 CLOSED their way** — the constraint map stays **parallel** to `Scene`.
  The argument that decided it was their third: *folding them makes one object
  with two reasons to be rebuilt, and the one that changes more often wins.*
  Built the cheap half instead — `decoration` / `decoration_cell` /
  `for_each_decoration`, free functions over two independent projections, with
  `node.name` pinned as the address convention.
- **R2 / R3** — the four-coordinate predicate table (containment / time / space
  / dimensions, with the graph split out) and a **"The Frame is where your
  data-sensitivity rule goes"** section, both into host-protocol with their
  provenance intact, because the provenance is what makes them credible.

### R7: "environmental, not real" was wrong twice over

The correction we owe them, and the reason to keep reporting a failure nobody
believes. `maiz_reduce_conformance` had been failing for a month and was
recorded on our side as environmental. It was **two real bugs, both ours**.

**One — a loader-order bug.** Windows searches the executable's own directory
before `PATH`; Git Bash puts Git-for-Windows' `/mingw64/bin` ahead of the UCRT64
toolchain that built our binaries, so a foreign `libstdc++-6.dll` loaded and the
process died before `main`. What made it read as a fluke for a month is that it
hit **exactly one target**: `reduce_conformance` is the only binary here using
`<filesystem>`, and `std::filesystem`'s symbols are precisely what an older
libstdc++ lacks — every other test links fine against the wrong DLL because it
never asks for anything the wrong DLL is missing. `0xc0000139` is
`STATUS_ENTRYPOINT_NOT_FOUND`, not "DLL not found" (`0xc0000135`), and we had
been reading it as the latter. **A partial ABI match is worse than no match: it
fails one target at a time and looks like the environment.** Fixed by staging
the toolchain runtime next to the binaries so the directory search wins.

**Two — once it actually ran, case 15 failed for real.** Fresh-agent identity
became normative upstream on 2026-07-27 (Void Palabra's ask): a rule-created
agent must be named from the redex — `H(sorted(glyphs), sorted(parent ids),
ordinal)` — never from a counter. We were minting a running `_rN`, which names
*the first agent the first firing happened to create*: a fact about the schedule
rather than about the net. Implemented: the derived minter (both components
sorted, so the id does not depend on which side the executor called A),
**BLAKE2b-48 matching the reference byte for byte** (the README says the hash is
not normative and we may re-pin against ourselves; we matched anyway, because
"not normative" only buys a Void Maiz that agrees with *itself* and the property
worth having is one that agrees with **Void Core**), written from RFC 7693
rather than vendored and pinned against the RFC's own vectors, plus `pin_ids`
and the cross-schedule id check in our harness — which we had also never
implemented. **15/15 cases; the suite is 15/15 tests.**

One harness fix fell out: the failure printer showed the first 200 bytes of each
side, and case 15's mismatch lives past byte 200, so it printed two
identical-looking lines. It now finds the first divergence. *A diff you cannot
read is not a diff.*

### Headless — founded, and it cost almost nothing, which is the finding

The author's secondary objective: *"I want to give an agent a task, and I want
that agent to USE stuff from these applications to actually complete the task,
likely from a real terminal"* — for a **client who installed a binary**, not a
developer — *"and when I open up Hormiga, it should be updated."*

This does not contradict the GUI-first stance; it is its consequence.
Commitment 2 has said *"humans, scripts, and AI agents share ONE interaction
surface"* since day one, and we had been writing "and AI agents" for a month
without handing one a terminal. So the claim [headless](/concepts/headless.md)
makes is small: **a Void Maiz application is a dispatcher, a glyph vocabulary,
an action vocabulary and a state document; the GUI is one front-end over that,
and headless is the same application with none attached.** Had it needed a
second code path, a sync protocol or an export format, that would have been
evidence the architecture was wrong.

Built (Phase H0–H2): `voidmaiz_headless` — `HostApp` (declared once, used by
both front-ends), `Session` (load → dispatch → save, attributed, advisory-locked,
journalled), `capabilities()` (the agent briefing), `run_cli()` (one-shot,
script file, stdin, REPL, `--json`, `--atomic`), and `examples/headless_app.cpp`
as a whole application whose `main` is one line — and as the `headless_smoke`
test. Links no ImGui.

**The briefing turned out to be a READER, not a new feature.** Every field comes
from something that already existed: `help` / `glyphs` / `mantles` from the core,
and `ActionRegistry::manifest()` — built 2026-07-21 for Hormiga's Territory map
so *"a volunteer's click and an agent's command become the SAME transcript
entry"*, four weeks before anyone asked for headless.

**Two claims the concept page made were wrong, and driving the binary corrected
them.** Recorded rather than quietly fixed, because the shape is the useful part
— *the durable half of Void Core's history is model content and the convenient
half is not*:

1. **`undo` does not survive the process.** The undo stack and log are
   session-scoped; the document carries the model, not the history of how it got
   there. Two consequences, both built: a **journal** appended beside the
   document in the core's own line format (the core does no file I/O by
   contract, so persisting the spine is the front-end's job), and the discovery
   that **`_baseline` IS model content** — so the real cross-process affordance
   is `status` / `diff` / `revert`, which suits the question better anyway. That
   makes `Session::save()` (write the document) vs the `save` **verb** (also
   snapshot the baseline) load-bearing rather than pedantic: snapshotting would
   erase the diff and make an agent's work indistinguishable from the user's.
2. **An agent writing a task file by hand hits the quoting trap.** A script line
   quoting an apostrophe naively truncates silently. `arg()` cannot help — there
   is no C++ between the agent and the text — so the rule is now stated in the
   briefing's house rules. **Three independent hits on one four-line rule** (C++,
   Python, and now a plain text file) says it must be stated wherever text meets
   the dispatcher, not just wherever C++ does.

Opened **Q20** (should a live GUI accept an agent's commands, or stay
single-writer?) with a lean toward staying single-writer and making the
collision loud, because the alternative turns every application into a server.

### Headless H3 — the effect seam, and the one-way door

Same day, continuing. H3 was "`save`/`deploy`/`build` from headless, so *also
update the website* is one command rather than a developer's errand." The
plumbing was already there (`HostApp::effects` reaches SPEC §9); the actual work
turned out to be a **boundary**, not a pipe.

**The insight the phase is built on.** Everything else a headless agent does
lands in the state document, which means a person can see it with `diff` and
throw it away with `revert`. An effect is where that stops being true. So the
line is not between reading and writing — an agent should write freely — it is
between **work and publication**, and those are two different grants. That maps
exactly onto the author's own example: the database work and the newsletter
draft are document work an agent should do unattended, and *updating the website
is the part a person wanted to look at first anyway.* The gate is not a
restriction on the workflow; it is the shape the workflow already had.

Built:

- **`EffectOp`** — the host declares what its effect handler answers to (name,
  doc, `reversible`, and `consequence`). `Core::EffectHandler` is one opaque
  `std::function`, so nothing could enumerate what an application can do to the
  world, let alone what it would mean. This is the same gap `ActionDescriptor`
  closed for a view's gestures, at the other seam, closed the same way: name it,
  document it, let the library enumerate and never interpret. `consequence` is
  the field a host will leave empty and the only one a person actually needs.
- **`EffectPolicy`** — `Refuse` (default) / `DryRun` / `Allow`, with
  `--allow-effects[=a,b]` and `--dry-run-effects`. A refusal quotes the host's
  own `consequence` back at the caller and names the flag that would grant it.
- **The gate is at the VERB, not the handler**, and that is forced by two things
  read out of `verbs_lifecycle.c`: `save` calls `vc_snapshot_baseline`
  **unconditionally** (so a handler-level refusal would still move `_baseline`
  and erase the review diff), and an effect handler **cannot fail a dispatch**
  (`res = res_make(1)` regardless — the handler's value becomes `data`, never
  `ok`, so a handler-level refusal would be reported to the caller as a
  *success*). A backstop gate still sits inside the handler wrapper for routes
  the verb gate cannot read; `batch` payloads are parsed and gated, because an
  atomic batch is the obvious way to hide a `save` behind an innocent leading
  verb. Verified: `--script sneak.vs --atomic` with a `deploy` inside is refused
  and the innocent command does not land either.
- The briefing grew an `effects` section (policy, grant list, declared ops with
  their consequences), so an agent learns the door is shut *before* planning a
  task that ends at it.

**Tests witness rather than assume.** The example's handler appends to a file, so
the self-test can assert that a refused `deploy` genuinely did not run — an
effect gate you cannot observe is a gate you cannot trust. Also pinned: a refused
`save` leaves the baseline where it was (the check that the verb-level gate is
doing the thing the handler-level one could not), and a named grant permits
exactly one op.

Both upstream behaviours went to Void Core as **item 2b** of the open message,
as questions rather than demands — with a backward-compatible suggestion (let a
handler return `{"ok":false,"error":"…"}` and have the core fail the command),
and a note that if every host wanting a failing effect must build a verb
interceptor, that deserves a line in the SPEC. Flagged 2b(b) as the one worth
their time: it is the case where current behaviour is actively misleading to an
automated caller rather than merely undocumented.

Suite: 15/15.

### Headless H4 — a person outranks an agent, streaming effects, and the adoption guide

Closing the headless track for now, on three author directions.

**1. "A person should always have more power than an agent, even in the headless
mode."** The first cut had a plain advisory lock — first come, first served —
which quietly encoded the opposite rule: an agent that started at 09:00 could
keep someone locked out of their own application all morning. The author was
explicit that this is *not* about rogue agents (*"I don't assume rogue agents or
anything"*), just precedence.

The rule is one line: **a Human session may take the lock from an Agent session;
nothing else preempts anything.** Two humans collide normally, an agent never
displaces anyone.

The design work was all in the second half. **Taking a lock is the easy and
unimportant part** — the property that matters is that the evicted agent cannot
overwrite the person's document afterwards, or precedence is a courtesy message
and the agent still wins at save time. So the lock file became a JSON claim
carrying a **token**, and a session asks *"is the lock still mine?"* rather than
*"is there a lock file?"*, which stays true after someone else takes it. Then
`dispatch` checks before running (a long agent run stops at the next step
instead of doing another four hundred into a document it can no longer write),
`save` refuses, `close` writes nothing and does not delete a lock that is no
longer ours, and the flag is **sticky** — even if the person closes and the lock
frees, whatever they did in between IS the document and the agent's copy is a
fork.

Losing an agent's uncommitted half-hour is a far smaller harm than silently
overwriting the document a person is editing, and it is the only one of the two
that can be explained afterwards.

Two conservative details, one of them found by driving it: an unparseable lock
file is treated as held by an unknown **human** (precedence must never evict a
person on the strength of a corrupt file), and the refusal message keys on **who
is holding it** rather than who is asking — an agent refused by another agent
was briefly being told *"a person is using this document"*, which is both false
and unactionable.

The consequence for every GUI host is one line — `SessionKind::Human` — and it
is the only step of the adoption guide we call mandatory.

**2. "Yes, please allow for log effects of course."** SPEC §9 says a long
operation streams line-by-line; `Core::EffectHandler` cannot, because it returns
one string when it is finished. Added `StreamingEffectHandler` — the same
function plus an `emit` callback — and everything it emits reaches the journal
and the caller's sink as it happens, flushed per line for the same reason the
journal is: the run that gets killed is exactly the run whose last lines matter.
`run_cli` sends them to **stderr**, so a `--json` caller's stdout stays
parseable while a person watching still sees the deploy scroll past.

One gap found and reported rather than papered over: emitted lines **cannot**
reach Void Core's own `log` buffer, because the C ABI exposes no host log-write
(`vc_set_log_sink` is read-only from the host's side). They live in our journal.

**3. "We should be able to implement this to the other Void Maiz based
applications."** Wrote [headless adoption](/concepts/headless-adoption.md) — the
five steps, what it costs (a declaration, not a port), and a *"mistakes we can
predict because we made them"* section: the `undo` assumption, dispatching
`save` and erasing the review diff, naive quoting in task files, anonymous
gestures being invisible to `--describe` by construction, and a GUI that forgets
`SessionKind::Human`.

Then a message to each client agent, tailored rather than broadcast:

- **Void Hormiga** — our second open message to them today, deliberately not
  merged with the asks reply. Their `ActionRegistry` IS the agent's tool list,
  and they are the only client with a rich one, which makes them the best
  adopter and the best falsifier of "a registered action is usable without the
  view". Flagged the `save`/baseline trap as the place we expect them to trip,
  given their export path.
- **Void Loops Studio** — the author's own second example (*"set up a basic
  looping drum pattern"*), which stresses something Hormiga's does not: a
  musical result has to be RIGHT, not merely present. If placing a step or
  setting a tempo are anonymous gesture handlers, an agent falls back to raw
  `rune new`/`link` — which works and produces something a musician would not
  have built. Also: audio is an effect, and `preview start` spawning a child
  from a one-shot session is a genuine footgun.
- **InteractionCombinators** — warned that **derived ids changed under them
  today**: rule-created agents are now named from the redex rather than a
  counter, so anything pinned to `_r1`/`_r2` breaks, and should.
- **NodeBlocks** — the one with a real wrinkle. A block editor's whole point is
  that structure IS the interaction, so a snap is a spatial gesture with no
  obvious argument list. Our guess is that the action an agent wants is
  `attach-after(target, glyph)` — the *intent* rather than the gesture — which
  would make the `ActionDescriptor` an agent uses different from the one the
  mouse handler calls, and put a real exception in "one definition, two
  front-ends". Asked them to falsify it early.

Suite: 15/15. The headless track is at a natural stopping point; what remains is
a worked client task against a real backend, and Q20.

## 2026-08-25 — The §6.1 codec, adopted: Void Maiz stops implementing quoting

Void Hormiga measured `run_cli` from outside and found that **every argument
containing a space was truncated, with `ok:true`**
(`MESSAGE_FOR_VOIDMAIZ_hormiga-run-cli-argv-quoting-2026-08-25.md`). Their
table, reproduced here and now fixed:

| passed | stored before | stored now |
|---|---|---|
| `Outreach Coordinator` | `Outreach` | `Outreach Coordinator` |
| `Ana's Place` | `Anas Place` | `Ana's Place` |
| `x --json` | `x`, plus a flag from nowhere | `x --json` (after `--`) |
| `` (empty) | field never written | field written empty |

The cause is four lines in `run_cli`: the OS had already split argv, and we
re-joined it on spaces and handed the result to a tokenizer that split it again.
*"Set this person's role to two words"* is close to the most common thing a CLI
caller does.

**But the ask was not "fix the join", and the fix is not the interesting part.**
Hormiga's argument — which is Void Core's, quoted back to us — is that §6.1 is a
specification standing in for a component: every host implements it twice (once
to encode, once to decode), five implementations have now been wrong, and the
fifth was the reference core the other four were copying from. So the ask was
**stop implementing §6.1 in Void Maiz at all, in both directions**, and call the
codec Core 0.2.7 exports. We did.

### What we deleted

Four separate §6.1 implementations, all ours, all in the library:

1. **`maiz::arg()`** — our own four lines, shipped in August on Hormiga's ask,
   correct on the day and reviewed twice. It has a bug we did not find and they
   did not hit: a value ending in a **backslash** puts that backslash against
   the closing quote, rule 3 reads the pair as an escaped apostrophe, and the
   argument never closes. `C:\` became `C:'` and ate the rest of the line.
   Core's `vc_arg_quote` has an explicit clause for exactly this. *That* is the
   argument for adoption, better than the join was: a careful reimplementation
   is the dangerous kind, because it is right when written and goes quietly
   wrong when the rule moves — which rule 5 did four days ago.
2. **`run_cli`'s argv join** — now `maiz::command_line()`, one `vc_arg_quote`
   per element. An OS argv element is already one argument; quoting each one is
   how you say so.
3. **`script_line()`** — the per-`getline` reader. It cut on newlines *before*
   any quote was considered, so a value containing a newline was split across
   two "commands" and its tail executed. Against Core 0.2.6 that was a **command
   injection**; 0.2.7's rule 5 turned it into a hard error, which is a good
   failure and still meant **a multi-line value could not be scripted at all** —
   and multi-line values are ordinary (Hormiga has three `multiline:` field
   editors). Replaced by one `vc_transcript_split_json` over the whole file.
4. **`leading_verb()` and the effect gate's hand-rolled batch unquoting.** This
   one was a **security hole, not an ergonomic one**: the gate read the verb by
   skipping whitespace and taking the next run, so `'deploy'` presented as the
   eight-character token `'deploy'`, matched no effect verb, and walked through
   a gate whose entire job is to stop that. The tokenizer that *runs* the
   command strips those quotes. A gate and a dispatcher disagreeing about what
   the verb is, is the only thing a gate must never do. It now asks
   `split_argv` — the same tokenizer — and reads the decoded `batch` payload
   instead of hunting for the first and last `'` in the line.

### What we added

`arg`, `command_line`, `split_argv`, `split_transcript` in
[embed.hpp](../include/voidmaiz/embed.hpp) — **the whole codec, encoder and
decoder, forwarding to Core and deciding nothing.** The decoder half is the one
nobody thinks to export and the one a host actually needs: `--script` now knows
what a transcript will do before running it, `flat` says whether it contains
control flow, and an unterminated quote is reported with the line it opened on.

`command_smoke` re-checks Core's law — `split_argv(arg(v)).argv == {v}` — on
**this** side of the ABI, over a corpus of the values that actually broke a
host. Not because the rule might be wrong (we no longer implement it) but
because the **marshalling** might: a `string_view` that was not NUL-terminated
before crossing, an envelope decoded wrong, a library string freed with the host
allocator.

Two CLI fixes fall out of the same work. **`--` ends the options**, which
answers Hormiga's one open question — they suspected `cli.words` might lose
information *before* the join, and it does: `set n notes --json` had its value
eaten by the option parser. (The empty-string row was the join, not the parser;
an empty element contributed a separator and no argument.) And **the REPL now
accumulates lines until they form a complete statement**, so a multi-line value
can be typed; a blank line abandons a pending one, because otherwise a stray
quote swallows the session with no way out but a kill.

### Found while adopting, reported upstream

Core's transcript splitter advances its line counter only at a statement
boundary, so **newlines swallowed inside a quoted value are never counted** and
every later line number drifts low by exactly that many. The line number is the
whole selling point of the new reader, and it is wrong precisely when multi-line
values — the feature it exists for — are used. Reported as
`MESSAGE_FOR_VOIDCORE_maiz-transcript-line-drift-2026-08-25.md`; the test pins
the current drifted value rather than the wanted one, because a green test
asserting the fix does not exist is how the fix gets lost.

Suite: 15/15.

## 2026-08-29 — A project that declined to adopt found the layer picture lying, and a closed question that wasn't

Three unrelated threads landed together. The one worth the space is Void Unity's
(`MESSAGE_FOR_VOIDMAIZ_voidunity-extracting-allomone-2026-08-28.md`), which is a
report from a project that read `host-protocol.md` carefully and **decided not
to adopt** — and is more useful than an adoption would have been.

### The layer picture was false at the build level

Our own page has said for weeks:

> The merge lives one layer below the language: a host can compose annotation
> maps it builds by hand and never link a parser.

and offered opt-out rung 2, *"use the merge, not the language."* Both were true
about the *code* and false about the *build*: `merge()` shipped inside
`voidmaiz`, which is the GUI library, so taking the algebra meant linking
projection, gestures, cJSON and Void Core. Void Unity's words: *"taking it still
means linking a library whose reason for existing is an interface we will not
draw."*

`voidmaiz_merge` is now its own target. It cost six lines of CMake, because
`src/annotate/annotate.cpp` depends on **nothing** — not Void Core, not cJSON,
not ImGui, not another Void Maiz header. Which is also the measure of how long
it should have been separate: nothing was holding it in, and nobody inside the
project noticed, because from inside there is no difference between "the layers
are separate" and "the layers would separate cleanly if anyone tried."
`annotate_smoke` now links **only** that target, so the claim is checked by the
linker rather than by a paragraph.

**The general lesson, which is the reason this is a log entry and not a commit
message: a layering claim that no build target tests is a comment, not an
architecture.** We have three more of those written down. They are worth an
afternoon each.

### `Sum` — a "closed question" that was closed on a wrong reason

`composition.md` said, flatly:

> If you want them to total 4 you are asking for a multiset, which is not
> idempotent and therefore not a lattice.

Void Unity's canonical case is **two identical rings of +3**, where a stat sheet
must read **+6**. They accepted our sentence at first, then worked the algebra
and pushed back, and **they are right**:

> The law is a function of the lattice element, not the join operator.

The element is not the set of values. It is a finite map **source id → values**,
and union of such maps is idempotent, commutative and associative on its own.
Our own invariant A2 already said exactly this — *"any source list equals its
deduplication **by source id**; two different ids with identical content are
agreement, not duplication"* — so the refutation was sitting in our test
document, one page from the claim.

And the code had it too: `merge()` builds `groups[{subject,property}][source_id]`
and always has. **Idempotence was secured by the keying, one step before any
value was ever deduplicated.** The cross-source `unique()` on top was a second,
independent choice we had mistaken for a requirement — which is why the sentence
was not merely wrong but wrong in the most expensive direction: it told every
reader that the modelling error was theirs.

`Lattice::Tally` is the law that was always available: each source contributes
the total of its own distinct values, and those subtotals are added. Two rings
→ 6; the same ring twice → 3. `Sum` is untouched and still right for its own
question. `annotate_smoke` pins the **algebra** rather than the arithmetic —
commutativity, idempotence, associativity — because if `Tally` broke a law the
objection would have been correct after all.

One thing they did not report and we found reproducing it: **`Custom` cannot
route around the dedup either.** A `JoinFn` receives the cross-source
deduplicated set, so the escape hatch was closed by the same choice that closed
the front door. Recorded as Q23 rather than fixed, because widening `JoinFn`
changes a signature Void Hormiga has shipped against.

### What we did NOT do, and why

Their proposals (b) a C ABI for the merge and (c) extraction to a sibling repo
are **Q24, both leaning no on this evidence**. (b) is the only one that would
make them a consumer and they said plainly *"we are not waiting for it"* — they
are shipping their own C# implementation now, so building an ABI for a host that
has already built the thing is the wrong order. (c) is worse: Palabra earned its
repo by having consumers who needed it independently, and an extracted merge
would have none — and (c) without (b) helps nobody, since Void Unity cannot link
a C++ repo either. The trigger to revisit is a **second** asker.

Their §4 (chains want strata) is a roadmap item, not a question, and their read
is right that `sentinel.hpp` already runs Tarjan SCC and Kahn strata and reports
`1 strata` because nothing yet produces more.

### The other two threads

**Void Core 0.2.9 fixed the transcript line drift we reported on 2026-08-25**,
using the patch we suggested. Our `command_smoke` had pinned the *drifted* value
on purpose, with a comment saying a green test asserting a nonexistent fix is
how the fix gets lost. It went red on their release, which is what it was built
to do; trued up to `line: 4`.

**Void Core 0.2.10 made the derived-id digest normative — and picked SHA-256,
not the BLAKE2b we had matched byte for byte.** We asked for normativity
(2026-08-18) and argued for it; they agreed and then chose differently, for a
reason invisible from C++: Void Unity built the second Reduce executor in C#,
and .NET/Mono have no BLAKE2b, so blessing ours would have forced them to vendor
a primitive whose test vectors are RFC 7693's rather than Void Core's — the one
thing their charter forbids. Between two digests equally fine at naming, the one
every standard library has wins. `blake2b.hpp` deleted, `sha256.hpp` written
from FIPS 180-4, and the minter **promoted out of a lambda** into
`maiz::reduce::derived_id` so their new `16-minter.json` key→id vectors can be
run against the real thing rather than a rebuilt copy of the rule. 16/16.

And one latent bug of our own, exposed rather than caused by 0.2.10:
`embed_smoke` compared versions as **strings**, so `"0.2.10" >= "0.2.4"` was
false. Wrong since the day it was written, fireable only once a component
reached double digits, and exactly the kind of thing that gets diagnosed as "the
dependency broke."

Suite: 15/15, 16/16 reduce cases.

## 2026-08-29 (later) — Allomone is its own repository

The author's call, overruling the Q24 lean recorded this morning: *"let's
separate allomone into a separate Void Allomone, mostly because Void Unity will
wanna use it… of course, it still needs to work equally for Void Maiz (and also
for Void Hormiga to still be able to use it too)."*

Our lean had been "not on this evidence" — no consumer, and (c) without (b)
helps nobody. The author's answer took the second half of that seriously and the
first half not at all: **do both**, and the consumer is the reason. That is the
right correction. Our objection was that an extracted C++ repo would not help
Void Unity, and the fix for that is the C ABI, not staying put.

Full history is in `../VoidAllomone/okf/log.md`. What belongs here is what Void
Maiz kept, what it gave up, and what it cost its own adopter.

### Four questions, asked before moving anything

Worth recording that the boundary was asked about rather than guessed, because
three of the four had answers we would not have picked blind:

1. **Scope** — the language *and* the merge, not just the merge. A repo holding
   only `merge()` would not be "Allomone", it would be Void Lattice.
2. **`device` / `with`** — demoted to host predicates; the attention graph stays
   here. Void Unity had flagged this as *"the actual work… only you can do."*
3. **C ABI** — the merge only, JSON in and out.
4. **Build** — compiled from source by C++ hosts, never as a prebuilt.

Plus a fifth item the author added unprompted, which turned out to matter more
than any of them (below).

### What we gave up, and what came back

`merge`, the grammar, the Weaver and the Sentinel's analysis all left. **One
function was cut rather than moved**: `effect_scene()`, which lays an effect
graph out as a `Scene`. A `Scene` is our projection type, so the analysis is
upstream and the picture is ours — `src/allomone/sentinel_scene.cpp`, with its
three test blocks in `tests/effect_scene_smoke.cpp`.

That one function is the entire cost, out of ~3,400 lines, and it is the honest
measure of how well the boundary was already drawn.

**`device` and `with` came back as ours**, which is the interesting half. They
were kernel predicates, and the only two in the kernel that read something
outside a `Subject` — our `UserGraph`. `PredicateFn` lost its `const UserGraph*`
parameter entirely, so **the language now has no opinion at all about what host
state looks like**, and we capture a graph in two lambdas instead
(`register_user_graph_predicates`). Every existing script still parses, because
parsing was always registry-free.

The demotion *added* two things nobody planned: a host can now **replace** them
(the kernel could never be shadowed), and there is now **one silence rule
instead of two** — an unregistered `with` is silent for the same reason every
unknown predicate is, not for a special null-graph reason of its own.

### The attention graph, generalized — the author's fifth item

> *"the user graph needs to be more abstract on what it is being applied to. for
> example, currently it is for a GUI, however, now we don't know what mantle
> will be classified as a 'user graph'."*

The header said, in these words, that the library *"records the modality and
never interprets it"* — directly above
`enum class Device { Pointer, Touch, Pen, Gamepad, Voice, XR }`. **A closed
enumeration is an interpretation.** It decided in advance that attention arrives
through a hand on a device, which is true of a GUI and of nothing else: an
agent's arrives through the dispatcher, a harvest's through a pipeline stage.

`Device` is now an open `channel` string, under the same quarantine `kind` and
`property` already live under, with the six GUI spellings kept as constants — a
vocabulary, not a type. And `kind`/`channel` default to **empty** rather than
`"widget"`/`"pointer"`: the old defaults meant every non-GUI host's affordance
silently claimed to have been clicked. `device ""` now matches "the host did not
say", which is a usable answer where a fabricated `pointer` was not.

The mantle half of the author's note is genuinely open and is **Q25**, leaning
"one per attender, defaulting to one shared" — it interacts with Q20 and should
be answered with it.

### Two naming calls, one of them wrong for an hour

The `allo_` prefix went upstream (`allomone::parse`, not
`allomone::allomone_parse`); it only ever existed because `maiz::` was the sole
namespace and the prefix was doing the namespacing by hand. We re-export the
prefixed spellings, since two hosts type them.

**The namespace was `allo::` until building Hormiga broke.** They have
`namespace allo = hormiga::allomone;` — they took the short name for their own
domain language long before this repo existed. Renamed to `allomone::`, which is
better regardless. The rule worth keeping: **a library extracted out of its host
does not get to claim the short name; the host was there first.** We would not
have found this by reading, only by compiling their tree, which is the argument
for doing that before shipping rather than after.

### The requirement that shaped everything: Hormiga compiles unchanged

`voidmaiz/allomone.hpp`, `annotate.hpp`, `weaver.hpp` and `sentinel.hpp` are now
forwarding headers that re-export into `maiz::`. `voidmaiz_allomone` survives as
an INTERFACE target because their CMake links it by name.

Two shims needed judgement rather than mechanics:

- `PredicateRegistry::add` takes the old three-argument predicate and passes
  `nullptr` for the graph. Honest — there is no graph to pass, and by their own
  account none of their twenty-two read it.
- `allo_eval(script, subjects, graph, preds)` **honours the graph**: it builds a
  registry that is `*preds` plus `with`/`device`. Ignoring the pointer would
  have been three lines shorter and would have silently turned every responsive
  rule into one that never fires. **A compatibility shim that changes behaviour
  is not compatibility** — the exact failure mode we have spent two sessions
  writing messages to other agents about.

Verified rather than assumed: **Void Hormiga builds 83/83 and passes 33/33 with
zero edits to their source.**

Suite: 12/12 here (was 15 — four test files moved to Void Allomone, one split in
two, two new ones for what stayed), 6/6 in Void Allomone including the C ABI.

## 2026-09-01 — Preparing to publish: what the probe caught, and three claims the README could not support

Void Maiz goes public at `migriv24/VoidMaiz`, following the conventions Void
Hormiga set on 2026-08-31 rather than inventing new ones: a pure MIT `LICENSE`
with the vendored index split out into `THIRD-PARTY-NOTICES.md` (appending it to
the license is what makes GitHub classify a repo as "Other"), a `void.json`
family manifest, and a `.gitattributes` written *before* the first commit.

### De-identification found nothing to remove, which is itself the finding

This family is built alongside a real partner organization, and nothing public
may name them. A case-sensitive sweep for the organization, its domain, its
street address, its phone number, its staff first names, and every one of the
315 proper names in the private sibling's data store returned **zero hits** in
this tree. That is not luck — Void Maiz is a library, and its fixtures were
invented from the start (`maria`, `attacker@evil.example`, the synthetic notes
app in `examples/headless_app.cpp`). The only near-misses were a generic job
title (`Outreach Coordinator`, used to demonstrate that a *quoted argument
survives the round trip*) and a published researcher cited in `okf/sources/`.
The finding worth keeping: **a library whose examples were never drawn from real
data needs no redaction pass.** Applications are where the risk lives.

No secrets either — no key material, no `.env`, no tokens, and no absolute path
under the author's home directory anywhere in the tree.

### The probe found two things the .gitignore did not

`git --git-dir=<tmp> --work-tree=. add -An .` into a throwaway git-dir, which
stages nothing and writes nothing here, is the only honest way to read a
`.gitignore`. It caught `Testing/Temporary/LastTest.log` (CTest's scratch,
written into the *source* tree whenever ctest runs from the repo root, carrying
absolute local paths) and `.claude/settings.json` (108 lines of this machine's
permission allowlist — workflow, not project). Both now ignored, plus `build*/`
so the stray `build3/` cannot come back. Re-run after the change: 189 paths,
105 of them `vendor/`, nothing that is not source or design.

`.gitattributes` matters here for a specific reason. Four files in this tree are
CRLF — `include/voidmaiz/code.hpp`, `src/view/code.cpp` and the two surviving
`okf/concepts/allomone/` pages — and everything else is LF. One ordinary
`sed -i` on any of them rewrites the whole file while changing one line. Before
the first push that is invisible; after it, it is a whole-file diff in someone
else's review, forever.

### Three doc claims that did not survive being checked

The instruction that a doc overstating a defect costs more trust than the defect
does cuts both ways, and here it cut the other way — the README *understated*:

1. **"15/15 tests green" was wrong twice.** The suite is 12 cases (four test
   files went to Void Allomone on 2026-08-29; the log's own last line already
   said 12/12), and **one of them is red**. `reduce_conformance` passes 17 of
   25. Void Core extended the portable reduce contract *today* with boxes
   (cases 17-20, 23, 24), a reserved separator (22) and a `patch` rule (25),
   and this port predates all of it. Nothing regressed; the contract grew. The
   README now says so in the words a stranger needs: flat nets conform, boxed
   ones return `unknown rule`.

   The stale count had propagated: `reduce.hpp` said "fourteen pinned cases",
   `interaction-connections.md` said 14, `log-first-inheritance.md` said 10.
   All three now name the number *and the date it was true*, which is the only
   form of that sentence that ages honestly.

2. **111 broken links, all one cause.** Extracting Allomone on 2026-08-29 moved
   its concept bundle to `../VoidAllomone/okf/concepts/`, and `okf/index.md`
   says so plainly — but the 111 inbound links from `log.md`,
   `developer_questions.md` and `horizons.md` were never retargeted. They now
   point at the sibling, uniformly, via the OKF's own root-relative form. The
   two pages Void Maiz kept (`editor`, `user-graph`) were left alone.

3. **InteractionCombinators was described as "not yet created."** It has been a
   built sibling since 2026-07-14, the day it shipped an Android APK. `vendor/
   README.md` still called the static library `voidnode`, seven weeks after the
   rename.

Every one of these was written when it was true. That is the whole hazard: a
status line is a claim with an expiry date and no way to notice it passed.

## 2026-09-03 — Void Core 0.2.14 adopted: kinds, quantities, and a weight that means a value

Void Core shipped three rune kinds, split schema from presentation, and made an
edge weight optionally an attribute's VALUE
(`MESSAGE_FOR_VOIDMAIZ_voidcore-0.2.14-rune-kinds-and-the-glyph-split-2026-09-03.md`).
Their verdict on us was **reship** — additive, nothing breaks — and it held: the
suite is unchanged at 11/12 with `reduce_conformance` still at its known 17/25.
The whole reasoning is a new concept page,
[rune kinds and quantities](/concepts/rune-kinds.md). Four things are worth the
log.

### The projection reads all three, and the canvas draws one of them

`SceneNode::kind` (entity/act/measure, default entity), `SceneNode::quantity`
(from the RUNE, because `health` and `speed` share one measure glyph and differ
only in what they measure), `SceneField::quantity` (from the glyph's `kinds`
map), and `SceneWire::is_value` — true when `to` resolves to a measure rune,
decided by the target alone because the direction is normative.

The rendering consequence is the one that mattered: **an assertion is labelled,
never thickened.** A strength and a value are not commensurable, so drawing
*900 rpm* nine hundred times heavier than *"supports, 1.0"* would be a lie the
renderer told on the model's behalf. `value_label(scene, wire)` formats it once
— "5 m/s", trailing zeros trimmed, "" for a wire that is not an assertion —
because the unit lives on the far node and a two-place lookup is exactly the
shape `field_of` diverged on before it shipped.

Nothing was drawn for an ordinary weight, and that is deliberate: a strength has
no canonical rendering yet, and inventing one here would answer a question the
canvas has not been asked.

### A quantity is schema, so it may pick an editor — and a hint still outranks it

Until today the only way to get a knob instead of a text box was
`hints.editors`: a presentation hint, host-private, written once per glyph per
host. `kinds` is **schema**, so `widget_field` now falls back through it — a
bounded RATIO field becomes a knob, an unbounded one a clamped drag-number, and
interval/ordinal/nominal get neither, because a sweep from `min` on a quantity
with no true zero would draw a proportion that does not exist.

The ordering is the discipline, not the feature: the inference sits **after**
the `field.editor` branch and can never overrule a glyph author who said what
they wanted.

### presentations.canvas layers over hints PER KEY

Void Maiz is the canvas modality, so `presentations.canvas` is our object. We
layer rather than switch: an all-or-nothing read would make a descriptor holding
both silently lose half its look, and a glyph author migrates keys one at a
time. Canvas wins where both speak; every descriptor in this repo's own examples
and tests still renders identically through `hints`.

### Two things the message did not ask for, found while checking what it did

**Their §2(c) worry does not apply, and their §8 step 2 was already done.** Void
Maiz never reconstructs a state document — `Session::save` writes
`export_state()` verbatim and `Core(state_json)` hands it straight back to
`vc_create` — so `state.glyphs` round-trips untouched. And the library keeps no
schema of its own to delete: `project_scene` has read `fields` off the
descriptor since the projection existed. Neither is a virtue we practiced; both
are [commitment 1](/index.md) paying out.

**The Android source list was missing `journal.c`.** Desktop links the prebuilt
DLL and never walks that list, so the file Void Core added in 0.2.8–0.2.12 has
been absent from `CMakeLists.txt` for weeks and only a cross-compile could have
noticed. Fixed, with a comment saying why the list exists at all. The general
shape is one this log has recorded before in another accent: **a claim no build
target tests is a comment, not an architecture** — and a file list only one
platform reads is a platform that is not being built.

### What we declined

`kind:<k>` did **not** go into the filter bag, though `filter_bag` already
offers `glyph:<g>` and it would have dropped straight in. `kind:` is an ordinary
application namespace on the `what` axis, so an app tagging `kind:vegetable`
would find the canvas filter box matching runes it never tagged — which is the
exact reason Void Core refused to reserve it, and it binds a library that
promises ONE tag grammar across every surface harder than it binds them.
Kind-filtering is `ls --kind`.

The reduce contract is untouched, as they predicted: `signatures` maps glyph →
aux-port count and lives in the reduce spec, `kind` is a different key on the
glyph descriptor, and neither reads the other. γ, δ and ε still mean what
`reduce.hpp` says — which is the reason the kinds are not named after them, and
the reason is that Void Hormiga read our header before agreeing to a name.

Reply: `../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-0.2.14-adopted-and-an-act-rune-is-a-drawing-2026-09-03.md`.

## 2026-09-08 — Three messages: a platform question answered with a runner, a bug report whose mechanism was wrong, and a name collision guarded

### Void Hormiga asked whether we build on Linux and macOS, and we could not answer

Both of their binaries link us, so nothing of theirs reaches a non-Windows
desktop without us on it. They offered three answers and asked for one:
*it builds*, *it should but nobody tried*, or *it does not*. The honest answer
was **(2)**, and the useful thing was to stop it being (2) permanently.

**The field they read is the story.** `void.json` said
`["windows-x64", "android-arm64"]`, which could mean *what has been built* or
*what compiles*, and they had to guess. It meant the former. That ambiguity is
now written down instead of implied — [platforms](/concepts/platforms.md), and a
`platforms_note` in the manifest — with the two claims kept apart: **shipped** is
a GUI binary that ran in front of a person, **compiles** is measured by CI.

**The audit expected a yes**, and mostly got one: exactly one `_WIN32` in
`src/` (properly `#else`-guarded), two `if(WIN32)` blocks in CMake, GLFW 3.4
vendored as source with its own Cocoa/X11/Wayland selection,
`find_package(OpenGL)` rather than a hardcoded `opengl32`. Even `VC_DLL` — which
we define unconditionally on desktop and which looks like the obvious breakage —
is safe, because `voidcore.h` guards it as `_WIN32 && VC_DLL`.

**One real defect, and its shape is the lesson.** All three examples requested an
OpenGL 3.0 context with a `#version 130` shader. macOS has no 3.0: legacy 2.1,
or 3.2+ core with forward-compat, nothing between. The request does not fail —
`glfwCreateWindow` succeeds with 2.1 — and then the shader will not compile, so
**the window opens and stays blank**, presenting as a rendering bug in the
host's own code. In a file the host wrote by copying ours: the build's own
comment says the examples are *"the first thing a new client copies."*

So the fix is a library function, not three edits.
[`glhost.hpp`](/../include/voidmaiz/glhost.hpp)'s `gl_context_hints()` sets the
hints **and returns the matching GLSL version string**, so the two halves cannot
drift — there is no longer a second place to write either. Windows behaviour is
byte-identical (still 3.0 / `#version 130`).

**And a runner is the second computer.** Neither project owns a Linux or macOS
machine; that constraint has blocked Hormiga's phase F exit test since
2026-08-27. `.github/workflows/ci.yml` builds Core, the library, the tests and
the GUI examples on all three desktops and runs the headless suite. Verified
cold locally first — fresh configure, fresh build, 11/11 on the gating run — so
the workflow is not a guess about our own build either. It is `fail-fast: false`
(a red Windows leg must not hide a green Linux one) and it **excludes**
`reduce_conformance` from gating while still running it: a knowingly-red test
cannot double as a portability signal, or every platform is red for a reason
that has nothing to do with the platform.

**What CI still cannot say:** that a window opened. No runner has a display. The
macOS context fix is reasoned and compiled, not witnessed, and the array stays
two platforms long until somebody looks at a Mac.

### Void Mago reported a wire bug, and the mechanism is wrong

Their graph is the first that is **entirely loose wires** — one rune per Void
project, no ports, no principals — and they reported that `wire_endpoints()`
leaves `ta = tb = (0,-1)` on the loose branch, so `draw_wire()` puts both bezier
control points above the line and every wire humps upward.

**The first half is true and the second cannot be.** Loose wires have drawn as
`AddLine(a, b)` since the initial commit — confirmed by `git log -S` — and all
three loose paths ignore the tangents: the base render, the hover highlight, and
`hit_wire`'s linear walk. `ta`/`tb` are **dead values** for `Kind::Loose`. So
the humps they describe cannot come from the code they quote.

What they are almost certainly seeing is their own §2.1: a centre-to-centre line
starts underneath the node it belongs to and emerges from the far side, so it
reads as passing *through* it and crosses unrelated bodies on the way. That is
real, it is ours, and it is the half they ranked most useful.

**Both are fixed.** Loose endpoints now clip to the body boundary along the run
(`body_edge`, with the 0.44f shaped radius extracted so the anchor math, the
body drawing and the wire clip cannot drift apart), and the tangents are derived
from the run rather than left pointing up. The second is a **latent** fix, not a
visible one, and worth doing precisely because their own §2.2 asks for a curved
loose wire — the first person to add one would have inherited a dead wrong value
and a bug that looked new.

The general shape, which this log has met before: **a correct observation with
an incorrect mechanism.** They found the ugliness by looking at their screen and
the cause by reading the source, and the two did not meet. Reading is how you
find a latent bug; it is not how you find the live one.

Their §2.3 (`WireRouting` on `CanvasStyle`) is public API and a real design
question, so it is **Q27** with a lean rather than a patch.

### Void Allomone guarded a name collision that would have failed silently

Void Core 0.2.14 gave a rune a `kind`; Allomone has always had a `kind`
condition meaning *whatever attribute the host puts on a Subject*. Nothing links
anything, so there is no compatibility surface — **the entire hazard is in the
adapter, which is ours.** `.kind = n.glyph` is correct and must stay; mapping
0.2.14's `SceneNode::kind` there instead would stamp `entity` on nearly every
subject and stop every `kind "…"` rule from matching. It does not error, and a
rule that stops matching produces no annotation to notice.

That is a trap our own last session made *more* live by adding `SceneNode::kind`
in the first place. Both adapter sites now carry the reason inline, where the
plausible-looking edit would be made. `runekind` is the spelling if a script
ever needs the rune kind.

**Q24 closed** — not answered, overtaken. All three of its steps happened, and
the C ABI (b) shipped with zero consumers, ahead of its own stated trigger.
Recorded with what the lean got right (the order) and wrong (treating "zero
consumers" as decisive about *whether*, when it was only ever decisive about
*when* — cost moved it, and a lean that weighs only demand cannot see a price
change). The author's 2026-08-29 call overruling our own (c) lean is kept in the
record rather than tidied away.

Suite: 11/11 gating, `reduce_conformance` unchanged at 17/25.

## 2026-09-13 — The mobile side, given deliberate attention: a touch layer that is the library's, not each shell's

The author's direction: *"lets begin some more development on the mobile side of
Void Maiz… as if we were preparing for Void Hormiga to go mobile. However, i
would want a bit more mobile specific GUI stuff… think outside the box too, like
things on a 3ds or other touch screen and some novel ways they were able to deal
with that."*

### What the audit found: the touch layer was never in the library

The InteractionCombinators APK shipped 2026-07-14 and looked like proof that
touch was handled. It was proof that touch was handled *once*. Every piece of it
lives in `../InteractionCombinators/android/src/main_android.cpp` and
`src/app.cpp`: the two-finger camera, the pointer release when a second finger
lands, the 40-px degenerate-pinch guard, the swallow-until-all-up rule, a
`toast_until` float standing in for a snackbar, and `touch_mode ? 14 : 6` for a
splitter's thickness.

Void Hormiga going mobile would have rewritten all of it — and rewritten the
subtle parts wrong, because **the subtle parts are invisible until a hand finds
them.** That is the same argument that moved `gl_context_hints()` into the
library five days ago, and it carries further here: a touch layer that lives in
a shell **cannot be tested at all** without a phone in somebody's hand, which is
why the APK's version never was.

### The deferred press, and the one decision that paid for the session

A press on glass is ambiguous for a few hundred milliseconds. A shell that
forwards the contact to the pointer UI immediately **has already committed to
"drag" before it knows** — so every long press first starts a node move, and
every tap is a zero-distance drag the canvas has to un-interpret. So the
recognizer latches the press and delivers a SYNTHETIC pointer once the gesture is
decided: past slop → left-down *at the original press point*; lifted early →
down-then-up (a tap); held → right-down; second finger → released immediately.

Two details separate working from nearly-working, and both are the kind that
only show up in use: the down must land at the **press point** (by the time slop
is exceeded the finger has slid, and a down where it now is hit-tests the wrong
node), and a release must land **on screen** (the canvas reads the pointer on the
release frame — its click-slop test and its hit test both do — so parking it
off-viewport first makes the gesture end nowhere).

**The decision worth keeping is three lines: a long press synthesizes a clean
right-CLICK.** Every context menu the desktop canvas already has then opens on
glass — node, wire, empty canvas, and every host entry hung off `ContextMenuFn` —
with **no change to `edit_canvas` at all**. The alternative was a touch-specific
menu path, which means a second menu implementation, a second set of host hooks,
and two places for a host's entries to be forgotten. It is the same shape as the
geographic-view ruling: absorb the substrate at the boundary and leave the thing
above it alone.

`Parked` is the APK's "swallow until all up" with a name and a reason: a pinch
ends with the fingers leaving one at a time, so the instant the first leaves
there is exactly one contact on the glass — which, without it, latches as a fresh
press and finishes as a stray tap on whatever was underneath.

### Millimetres, not pixels

Every threshold in `TouchProfile` is physical. A 6-px slop is a different gesture
on a 160-dpi tablet and a 560-dpi phone, so **a library that ships pixel
constants ships a different feel per device and calls it one library.** The shell
supplies `dp` and the budgets convert.

The same thinking corrected `port_hit_radius`, which was the right idea at the
wrong number: 9 px of grab radius under a contact patch nine *millimetres* wide.
`apply_touch_canvas` sets it to about half a fingertip and **leaves the drawn
marker alone** — the hit target grows, the picture does not.

And it deliberately does NOT scale `node_w`/`header_h`/`port_row`, which is the
plausible-looking edit: those are **world** units, and scaling them by display
density would make a phone's graph a different graph — the same document, opened
on two devices, laying out differently. Screen size for world geometry is the
camera's job. `click_slop` gets *smaller* on touch for a related reason: the
recognizer already decided tap-vs-drag, so the canvas is handed a decision, not
an ambiguity.

### The mobile chrome, and the borrowings

`voidmaiz/mobile.hpp`: bottom sheet with detents (the inspector on a phone; the
detent flushes to the config tier on `settled`, exactly as the camera does),
snackbar **with an UNDO action**, FAB + speed dial, segmented control, stepper
with hold-to-repeat, swipe-actionable list row, `apply_touch_canvas`.

The snackbar is the one where the architecture shows. "Deleted. UNDO" is the
mobile confirmation idiom and it cost a widget and one line of host code, because
commitment 2 had already done the work: every gesture is a command and `undo` is
a verb. The library still dispatches nothing — `draw_snackbar` returns a bool.

The author asked for out-of-the-box sources, and the [touch](/concepts/touch.md)
census carries them. The two that changed what got built: **the DS's discipline**
(single-touch, no hover — proof that losing the hover channel is survivable if
the vocabulary is *designed* rather than ported, which is what `hover_tooltips`
had already half-admitted), and **stroke gestures** from *Phantom Hourglass* /
*Kirby Canvas Curse* — draw a stroke across wires to CUT them. That last one is
now the next thing worth building: it is a better primary gesture than a precise
port grab, it compiles to `unlink` with no new command shapes, and it is the same
feature as the long-standing T2 "wire cut gesture (Ctrl-drag)" arriving from the
other side.

The third is a claim rather than a build, and worth saying out loud because
nobody else can say it: **a phone can be the control surface for a desktop
session** — the Wii U gamepad, and headless already proved two front-ends over
one state document with no sync and no import.

### What this session does NOT prove

`examples/mobile_window.cpp` is a phone-proportioned desktop binary where the
mouse is a finger and Alt adds a mirrored second one, so the kit can be looked at
without an APK. It **was built and not run**: the pre-flight screenshot found the
author live on the machine (a Google Doc mid-save, a video playing), so the
interactive pass was skipped by the standing rule rather than forgotten.

So the honest status is the [platforms](/concepts/platforms.md) shape again:
81 assertions pin the recognizer, the view module compiles clean under
`-Wall -Wextra`, and **nobody has touched any of it with a finger.** A recognizer
is exactly the kind of code that can be right in every test and wrong in the
hand. First job next session.

### Two questions rather than two frameworks

**Q28 — does the library lay out panes per substrate?** Leaning **no**. DockSpace
(Q11's ruling) solved the desktop and does not reach a 430-px portrait screen,
where the arrangement is not a smaller desktop but a different one. What shipped
is the *pieces* a host arranges itself. A layout engine would be the first thing
here that is genuinely a framework, and the vendor-don't-depend instinct that
favoured *more ImGui* at Q11 points the other way, because ImGui has no
responsive layout to turn on. Ask Void Hormiga after they build a phone shell —
a second host writing the same twenty lines is the evidence that moved touch
recognition this session, and it would move this too.

**Q29 — how does a soft keyboard reach a Void Maiz application?** The APK's
oldest known gap, and on a phone it is not polish: without an IME, the command
bar — commitment 2's showpiece — is desktop-only. It is a question because it
trades a **ground rule** against a headline feature: Android's IME means JNI into
the platform's Java runtime, and the APK's proudest structural fact is that it
shipped with zero Java and zero Gradle. Leaning: an **ImGui-drawn keyboard**
first, because a command bar is short, ASCII and has a known vocabulary — the one
case where a custom keyboard is *better* than the platform's, not merely
possible.

Suite: 12/12 gating (touch_smoke new: 11 before this session's start, plus
the one added here), `reduce_conformance` unchanged at 17/25.

## 2026-09-13 (second pass) — The Allomone wiring, verified; and the channel that was declared rather than observed

The author, after the touch layer landed: *"this also would raise a decent amount
of possibilities for the user graph in Allomone. Allomone is a separate
application now… lets just make sure all the wiring is in place, and that these
things still function well together."*

Both halves were worth doing, and the second half found something.

### The wiring: verified, not assumed

- **Cold configure and build from an empty directory** — not the incremental
  build that had been proving nothing since the extraction. `add_subdirectory`
  into `../VoidAllomone` resolves, the whole tree compiles clean under
  `-Wall -Wextra`, and the suite is green from a build directory that never
  existed before. That is the claim CI makes on three desktops, made locally in
  a form that could actually fail.
- **No API drift.** Void Allomone's working tree has two modified headers; both
  diffs are **comment-only** — the `kind`-collision guard from 2026-09-08. The
  re-export surface (`maiz::Script`, `maiz::merge`, `maiz::Lattice`,
  `maiz::PredicateRegistry`, the `allo_` free functions) is untouched.
- **The compatibility shims still honour the graph.** `allo_eval(script,
  subjects, graph, preds)` remains deliberately non-no-op: it builds a registry
  that is `*preds` plus `with`/`device` and evaluates against it. Now pinned
  from the touch side as well as the old side.

### The hole: `device` was matching a claim, not the input

`UserGraph::touch` takes a `channel`. Allomone's `device "pen"` reads it. And the
only host that fed it — `examples/allomone_playground.cpp` — got the value from a
**combo box the person picked from** (`const char* devs[] = {"pointer", "touch",
"pen", …}`).

Every piece was individually correct, which is why every test passed and why this
survived from 2026-08-06 to now. But the composition was not: **`device "touch"`
could only ever match a self-report.** No host could have done better, because
until this morning there was nothing else to fill it with.

The recognizer is that something else, and the fix is two fields:
`TouchPoint::tool` (finger / stylus / eraser / mouse — every touch platform
reports it, and nothing had ever asked) and `TouchFrame::channel`, the
`maiz::channel::` spelling of whatever actually drew the contact. A host writes
`ugraph.touch(id, "widget", frame.channel)` and the condition becomes a question
about the world.

Three details, each chosen the same way the graph's own defaults were:

- **`channel` is empty while nothing is being touched**, so a host writes a
  channel only when there was something to observe. Defaulting to a modality
  nobody used is exactly the lie `touch()`'s empty `kind`/`channel` defaults were
  changed to avoid on 2026-08-29.
- **`TouchTool` defaults to `Finger`**, because a recognizer consuming contacts
  is on a touch surface by construction. A shell that says nothing is a touch
  shell; that is a better default than pretending not to know.
- **An eraser reports `pen`.** It is a pen held the other way up. A host needing
  the distinction puts it in the affordance's `kind`, which is where host
  vocabulary lives.

**What it opens, which is the author's point.** A stylus is not a finger, and on
a device that has both, the difference is real intent — annotating versus
navigating. `when device "pen" -> annotate 1` is now a rule that can fire, and it
is the first responsive rule whose condition a person cannot accidentally
misreport. The same seam delivers `xr` the day Void Maiz XR exists: a string from
the substrate that knows, rather than a setting.

### The test that is the actual answer to "do these still function well together"

`tests/touch_usergraph_smoke.cpp` drives the whole chain in one file —
recognizer → observed channel → `UserGraph` → `register_user_graph_predicates` →
**the sibling repository's** evaluator → a merged constraint — including a real
two-finger gesture framed as one piece of work, so `with` reads coherence that a
hand actually produced.

It deliberately spells everything `maiz::` rather than `allomone::`, which makes
it a **re-export regression test as a side effect**: if the seam ever stops
re-exporting, this fails here instead of in Void Hormiga's 12k-line application.

That is the general lesson, and it is one this log has met before in a different
costume (*a layering claim that no build target tests is a comment, not an
architecture*): **an integration that no test crosses is an assumption.** Three
green suites over three correct components said nothing about the composition,
and the composition was where the defect was.

### Documentation drift, found while doing it

`okf/concepts/allomone/user-graph.md` still opened its device section with
`enum class Device { Pointer, Touch, Pen, Gamepad, Voice, XR };` — the closed
enum replaced by an open `channel` string on 2026-08-29. The header had been
trued up and the concept had not, which is backwards for a project whose first
ground rule is *code follows concepts*. Rewritten, with the reason the enum was
wrong kept rather than deleted.

### No message to Void Allomone

Deliberate. Nothing here is the language's: `with` and `device` have been **host**
predicates since the extraction, the change is entirely on our side of that line,
and their kernel cannot notice. Rule 4 is for needs and gaps, not for news that
costs the recipient nothing. The touch message to Void Hormiga gained a short
section instead, because they are the user-graph-heavy client and the one with
stylus-capable hardware in scope.

Suite: 13/13 gating (touch_usergraph_smoke new), `reduce_conformance` unchanged
at 17/25, and the same 13/13 from a cold build directory.

## 2026-09-18: Void Hormiga asks whether networking belongs in Void Maiz. Yes, staged, and here is the line

`MESSAGE_FOR_VOIDMAIZ_hormiga-networking-belongs-in-maiz-2026-09-19.md` (dated a
day ahead of our clock). Hormiga 0.1.3 shipped LAN sharing: a device profile, an
approved and sealed join, a presence beacon, and members kept in sync through
Void Palabra's replica. The author tested it on a Windows machine and a Linux
machine, found it worked, and concluded: *"void maiz should own networking … a
general universal networking ability should exist for any void maiz based
application."*

### The argument is ours even where the code is not

The reason given is **consistency**. Data rows got presence marks, the Builder
got outlines, the Map got nothing, and the Antfarm did not sync, because each
view decided for itself. None of these are network bugs. Each is a view that was
never told the rule, and making rules universal is the job of this library: a
canvas move is always a command because the library is the only path. Presence
can work the same way.

### The line: looks versus is

Answering required reading what the message did not include. Void Palabra's
charter already claims transport (*Speaking*, pillar 2). **Three days earlier,
on 09-16, Hormiga had asked Palabra** for signed utterances, the transport and an
ephemeral presence channel, and called its own `src/sync/peer.*` a stand-in to be
deleted. The 09-19 proposal moves presence, the profile and the sync loop into
Maiz. The two messages overlap, and that overlap is the real question.

The donated code sorted itself. Everything that touches libsodium is transport
and keys: `peer.cpp` (51 uses), `lan_wire.cpp` (12), `profile.cpp` (11).
Everything that does not (`share.cpp`, and the shape of `lan_presence.cpp`) is
presentation. So the proposed line is: **Maiz owns what networking looks like,
Palabra owns what networking is, and the host owns what is shared.**

**This is narrower than the author's sentence**, and it is recorded as Q30
rather than settled here. The four reasons are in
[networking](/concepts/networking.md). The one most likely to hold up is that a
keypair and the signatures it makes cannot sit on opposite sides of a trust
boundary. The one closest to the author's own rule is that the line makes
*"if networking … void palabra will be a required library"* true by
construction.

### The surface registry: a new home, not the widget registry

Hormiga's §4.2 (*"tagging the UI itself"*) asks for any surface to declare which
rune it shows and how presence draws on it. The widget registry's unit is the
field editor, which takes a projection and emits a command. A declaration emits
nothing, so it gets its own UI-free registry. It earns that because **three
consumers want it**: presence, the surface census (waiting for a harvest source
since July), and the user graph. Hormiga's §4.3, "which tab someone is in", asks
the same thing as "what is this person attending to." One declaration read by
three consumers is the author's *"universal rules."* It assumes no geometry,
because the author explicitly said the Builder, Calendar and Map have not reached
their final shape.

### The point the message missed: two switches

The author's worry about showing which tabs people are in was **clutter**, which
is a receiver-side filter. A filter on someone else's screen is not privacy. The
**sender** needs a separate switch, "do not broadcast which surfaces I have
open." This joins presence to **Q20** (whose attention the user graph records).
Presence is the live, ephemeral form of that question, it is already running
between two real machines, and it is safe only as long as it never enters
history. That is one more reason it belongs on Palabra's unversioned channel.

### Staging, so the migration starts now

Stage **A** (surface registry, presence model, one uniform overlay, the canvas
registering its nodes) needs **no network and no Palabra**, because Hormiga
feeds it from its working LAN code. Stage **B** is profile presentation and the
Networking settings section. Stage **C** is a `voidmaiz_net` target that links
Palabra, and Hormiga's stand-in transport is deleted then. Only C creates the
optional target, so the author's condition (a non-networking application never
sees Palabra) is enforced by the build. That follows the lesson from the Allomone
extraction: a layering claim that no build target tests is only a comment.

**Nothing was built this session.** Hormiga asked *"what you think before
anything moves,"* the line narrows the author's direction, and stage A is the
first real code. It waits on the author's go-ahead rather than on Q30, because
it is correct under either answer.

### Three filing problems, found while answering

- **Hormiga's companion message to Palabra is misfiled**: it is in Hormiga's own
  root, so Palabra has not seen it. Palabra would first learn about this from a
  title suggesting that transport is moving away from them. We sent a short note
  from our side
  (`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-networking-the-transport-stays-yours-2026-09-18.md`),
  and asked Hormiga to move its file.
- **Our 09-13 touch message to Hormiga was never delivered.** It was never
  committed, it is gone from their root, and nothing in their tree refers to it.
  It is the first message in this family known to have been **lost** rather than
  misfiled. The reply supersedes it, following the one-open-message convention,
  and carries its asks forward. The lesson: an uncommitted message in someone
  else's working tree survives only as long as their next cleanup. Unique titles
  prevent a stale message from being mistaken for a current one; they do nothing
  about deletion.
- **Palabra's "zero dependencies" rule and libsodium.** Under our line the
  donated crypto goes to Palabra, which has promised to depend on nothing.
  Raised with them, not decided for them.

Suite unchanged (13/13 gating, `reduce_conformance` 17/25). No code changed.

## 2026-09-18 (second pass): Q30 ruled, and networking built for ANY application

The author, on the line drawn earlier the same day: *"dont think about hormiga
when actually constructing this … think more like your building a generalizable
networking module … for any void maiz application in the future … meaning you
might have to make significant alterations to how hormiga does it, especially
considering that hormiga has the antfarm."* And: *"i guess you own the visuals
and the tags for networking, the highlights, and all that. how networking kinda
fits with the registry, allomone, etc."*

That ruled **Q30**: Void Maiz owns the half an application shows, generalized.
On whether Palabra has to fix anything first: **no**. Stages A and B are Void
Maiz-only, and only stage C (transport) waits on Palabra.

### Designing without the client in the room

Building from one client's working code gives you that client's decisions under
a general name. So the Antfarm was read once, to understand *why* it works (its
sharing is **visible, wirable model content**, not hidden settings), and then set
aside. The general form of that insight is **one question the library asks and
never answers**, "may this rune leave this device?", which any application answers
however it likes: a tag convention, an Allomone rule, or a sharing graph that
compiles into the same function. An Antfarm keeps what makes it good (you can see
and wire what flows where) and stops being where per-view networking behaviour is
decided. That is the significant alteration the author predicted, and it is
recorded in [networking](/concepts/networking.md) §"Migrating an application
that already networks" rather than designed in.

### What was built

- `voidmaiz/presence.hpp` (UI-free): an immediate-mode **surface registry**,
  `PresenceState` (the wire payload's content), `SharePolicy` (sender) versus
  `PresenceDisplay` (receiver), `NetSettings`, `ShareFilter`, a defensive JSON
  codec, and a `Roster`.
- `voidmaiz/netview.hpp`: **one renderer** (outline, badge, tint, a padlock drawn
  from primitives), `presence_item` / `presence_rect` /
  `presence_surface_badges`, a member list, a profile editor, and the Networking
  settings with the sender and receiver groups visibly apart.
- `CanvasNet` on `edit_canvas`: the canvas declares its **on-screen** nodes by id,
  becomes the focused surface while its window has focus, and marks and locks
  nodes. With no `CanvasNet` it behaves exactly as before.
- The Allomone bridge: `present` / `present "<peer>"`, and `share_by_annotation`
  (a conflict keeps a rune local; the Merged is copied so the filter cannot
  dangle).
- `examples/network_window.cpp`: a canvas, a list and a custom-drawn board, each
  with one declaration per thing shown; two simulated peers going through the
  real codec on loopback.

### Three things the tests caught, and one the screen did

- **The id trap.** Presence is id-keyed, and both of this library's own Allomone
  examples key Subjects on the rune name. A naive `present` never matches and
  fails silently. That is Hormiga's §4.1 bug (matching by name) reintroduced one
  layer up, by the module written to prevent it. `register_presence_predicates`
  takes the Scene and resolves names to ids, and the silent failure without it
  is pinned *as a failure* so nobody "simplifies" the parameter away.
- **The documented example was not valid Allomone.** The header said
  `when tag "draft" -> share false`. The language has no bare booleans, so the
  rule did not parse, and a rule that does not parse keeps nothing private. The
  first test run failed three assertions for a reason that turned out to be the
  **documentation**, not the filter. Corrected to `share 0`, and the test now
  asserts both that its scripts parse and that `share false` does not.
- **Allomone's own guard caught a dangling pointer in the new test.** It was
  `merge(…).find(…)` on a temporary, and the deleted rvalue overload made it a
  compile error. The guard written on 2026-08 against exactly this paid off here.
- **On screen:** the padlock was first drawn at a node's top-left, on top of the
  canvas's port marker. It moved to the top-right, and the reason that corner is
  safe is structural rather than cosmetic: a private rune is never named in
  presence, so no peer can have it selected, and no badge will ever be drawn
  there.

**Seen working:** the author was watching this session, so the demo was launched
(no scripted input) and captured. Cy's pink appears as an outline on the canvas,
a `CP` badge in the list and a tint on the board. Bo's orange does the same on
another rune. `diary` is padlocked in all three views, and the outgoing payload
names no private rune.

### Still not proven

No bytes through this module have crossed a real network. Avatars are always
initials discs, because there is no texture path. Presence on a phone has not
been drawn.

Suite: 14/14 gating (`presence_smoke` new), `reduce_conformance` unchanged at
17/25.

## 2026-09-19: Stage C. `voidmaiz_net`, real Cores syncing through Void Palabra

Void Palabra wrote (`MESSAGE_FOR_VOIDMAIZ_palabra-ready-sync-session-2026-09-19.md`)
that every seam we named exists: presence as its own opaque, never-versioned
message kind, `Host::share` as the export set, cautious fetch with a `deferred`
state, and a sans-IO sync `Session` measured under a hostile network. A real
transport is still gated on their trust model. Everything above the byte pipe
is not, and the author said to continue.

### What was built

- **`Core::replace_state`** in the embed layer. Void Core has no in-place load, so
  a splice builds a new manager, and two things would be lost silently without
  this: the host's log sink and effect handler, and every glyph registered
  through `register_glyph`. The second was **measured, not assumed**: an exported
  state carries `"glyphs":{}` after a register, so the canvas would have lost
  every node style on the first merge. The Core now remembers what was
  registered through it and replays it.
- **`voidmaiz/net.hpp`, target `voidmaiz_net`**: the only target that links
  Palabra, built only when `../VoidPalabra` exists. It is embedded as a target,
  as Palabra asks, never as a copied source list, because the one list we did
  copy (Void Core's, for Android) went months without a file. `maiz::Network`
  holds one replica, one session per link, the loop in Palabra's order with
  `persist` as a required callback, the splice, presence through Palabra's
  ephemeral channel into our Roster, and the ShareFilter as the ExportSet.
- **`tests/net_smoke.cpp`** runs real `maiz::Core` instances over an in-memory
  wire, up to three devices at 30% loss.
- **CI** clones Palabra and **reports** whether the networked configuration ran.
  Palabra's sync commits are not pushed yet, so today CI would configure without
  `voidmaiz_net`. A silent skip would have read as a pass.

### Decisions worth keeping

- **Splice only when the merged slice differs** (order-insensitive). A splice
  must clear the undo history: Void Core's undo is memento-based, so a snapshot
  from before a peer's changes would revert them on undo, and the next sync would
  broadcast the revert as this device's act. So the rule became: **an idle peer
  never costs the user their undo.** Both halves are pinned: a device's own edit
  that round-trips through a peer leaves undo intact, and a real incoming change
  clears it.
- **Observe again immediately before splicing**, so an edit made since the last
  tick is recorded rather than overwritten (Palabra's step-5 warning, enforced
  rather than documented).
- **The Roster files presence under the session's replica id, never the
  payload's claim.** A test has one device claim to be the other; it is still
  filed under its own replica id.

### What the tests found

- **No phantom loop.** This was the risk that most worried us: observing Void
  Core's re-export of a splice could have minted changes on every tick,
  forever, on every peer. It records zero. Ten idle simulated seconds after
  convergence produced zero observations and zero splices. It is pinned, and a
  splice that fails to round-trip raises a warning note.
- **Independent founders conflict forever.** Every device ran `mantle new team`.
  Palabra keys mantles by name, so three random ids became a standing conflict
  on the mantle's `id`, reported on every merge. The test was wrong, not the
  module: joiners must **receive** a mantle, never create one with the same
  name. Every networked application needs that guidance, so it is written into
  the concept. It was also put to Palabra as a question: whether this belongs
  in anomalies ("asks for an edit") rather than conflicts ("asks which value").
- **"Diverged" was key order.** The first comparison used printed JSON and
  flagged `{"image","text"}` against `{"text","image"}`. The test now compares
  with Palabra's canonical `slice_hash`. The module itself never had this
  problem, because it compares order-insensitively before splicing.
- A test that needed a log sink could not use `LogEntry`, because that type
  lives in the view module and this test links no UI. The layering held here
  too.

### Two configurations, both measured

**15/15 with Palabra; 14/14 with `VOIDPALABRA_ROOT` pointed at nothing, and no
`voidmaiz_net` target built.** "An application that never networks never needs
Palabra" is now a property a build checks.

### Still not proven

No real network. There is no conflict or anomaly UI: counts reach the log as
notes, and `replica().conflicts()` is readable, but nothing shows them or offers
`resolve`. That is the next networking view.

Reply sent:
`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-voidmaiz-net-built-on-your-session-2026-09-19.md`.
It asks for nothing, confirms the Void Core fixed point from outside, and raises
three observations: the mantle-id conflict, `FetchPolicy` fixed per session,
and their unpushed commits.

### The last gap: a conflict is a question, so it needed a surface

Counts in a log are not an answer. `voidmaiz_net` now projects Palabra's
conflicts and anomalies as **plain rows** — strings only, no Palabra types —
which is what lets `voidmaiz_view` draw them while linking no sync library at
all. The same trick the presence payload uses, for the same reason.

- `Network::conflicts()` / `anomalies()` / `resolve(id, side, now)`. The id is
  Palabra's content address for the conflict, so **both devices see the same
  question under the same id** (pinned).
- `draw_conflicts` returns the choice and nothing else; `Network::resolve`
  records it as **this device's own act**, so it syncs like any other change and
  the other device sees the question *answered* rather than re-asked (pinned).
  Resolving a stale conflict is refused rather than overwriting a value nobody
  saw.
- Deliberately **not a modal**. A merge can arrive mid-sentence, and a dialog
  that steals the frame would make automatic sync feel like an interruption
  every time two people work at once.
- The wording is the point: *"content.text on `roadmap` was changed on two
  devices at once"*, and for the dangerous one, *"was deleted on one device and
  edited on another"* — the case a silent merge loses.

Tested by partitioning two devices, editing the same field on each, healing, and
answering. Seen on screen in `maiz_network` (sample rows: that window simulates
peers, so nothing in it can really disagree).

**Sent to Void Hormiga**: `MESSAGE_FOR_VOIDHORMIGA_maiz-networking-is-built-here-is-how-to-adopt-it-2026-09-19.md`
— a migration guide, not an announcement. What to delete and what replaces it,
the frame loop in full, five things that bite (joiners must not create the
shared mantle; a merge clears undo; presence keyed on the session's identity;
`persist` is not optional; their asset names already work), and what the Antfarm
becomes: it keeps being the place sharing is visible and wirable, and its output
becomes two values (a `ShareFilter` and `NetSettings`) instead of per-view
behaviour. Their 09-18 message is retired into it — one open message at a time,
their convention, and the touch asks carried forward in §9.

Suite: 15/15 gating, `reduce_conformance` unchanged at 17/25.

## 2026-09-20: The collaborative canvas, researched. What happens when several people edit one node graph

The author turned from networking in general to **the node graph in
particular**: *"how colaboration in a node graph could look like with multiple
people … node placements matter, the way wires are routed matter. the entry
fields and stuff"*, demonstrated by Interaction Combinators across desktop and
Android over LAN. Research only. Nothing built. Written up as
[collaborative canvas](/concepts/collaborative-canvas.md), with five questions
(Q31–Q35).

**The organising idea is three channels.** *Committed* (commands, synced by
Palabra), *in flight* (presence: gestures before they become commands) and
*local* (camera, panels). The canvas already stages every gesture and commits one
command on release, so collaboration mostly asks us to **show the staged half**
over presence: cursors, drag ghosts (one offset for the whole selection, not a
position per node), pending wires, typing, claims and pings. All three founding
commitments survive, because a gesture in flight changes nothing. Presence gains
geometry only in a canvas-scoped block, and `SurfaceDecl` stays geometry-free.

**What the existing commands do when they race** was checked against Palabra's
SPEC §5 rather than assumed. Fields are multi-value registers, so **two people
dragging one node produce a conflict, not last-writer-wins**. Tags and edges are
add-wins sets. A concurrent `+red`/`+blue` on one agent therefore merges into
both tags, and IC's pigment net mixes them into purple, which is the demo's best
single moment.

**Five findings that change the design, all measured in code:**

1. **Two disjoint redexes that share a wire commute in the maths but not as
   edits.** Stepped concurrently, the merge leaves two dangling half-wires, and
   the joined wire no peer wrote is missing. Strong confluence belongs to the
   net, not to the edit operations. That is Q31, with a path (claims, then derived
   ids upstream, then a deterministic re-knot) that could make concurrent
   reduction merge-safe everywhere.
2. **IC names minted agents by first-free local search** and `rune new` mints
   random ids, while edges address names. Concurrent steps and adds therefore
   collide on names. Void Core's SPEC §3.1 already carves out derived ids for
   reduction-minted agents, but IC's step is a plain `batch` and cannot use it.
3. **IC's `init` seeds `mantle new lafont` and a starter net.** A joiner doing
   the same reproduces exactly the bug `net_smoke` first had.
4. **Every merge clears undo** (correctly), so undo stops working in an active
   session. The proposal is a local history of compensating commands (Q32), to be
   decided with Q15.
5. **The APK declares no Android permissions, not even `INTERNET`**, and IC has
   no transport of its own. Hormiga's LAN yes was given to Hormiga (Q34).

Wire routes (Q35): waypoints on an edge value would duplicate wires under
concurrency, because an edge is a whole OR-set element. The lean is per-port
route content stamped with the partner.

**Received, not yet answered**: two Void Hormiga messages dated today, left in
*their* root: `…hormiga-adopted-all-three-stages-2026-09-20.md` (stages A–C
adopted, LAN yes recorded, and a question: should the Roster be fed by the host's
beacon or only by `Network`?) and `…hormiga-the-console-is-a-shared-surface-2026-09-20.md`
(offering their source-tagged console to the library). The console bears on this
research's L2, since each peer becomes a log source.

## 2026-09-20 (second pass): The author rules, and the collaborative canvas gets its foundations

The author answered the first pass the same day: *"i trust your leans"*, with
three rulings that changed it (all recorded in
[collaborative canvas](/concepts/collaborative-canvas.md) §0; Q31–Q35 closed).

1. **No hard conflicts for view state.** *"i don't see an issue with last one
   wins."* **Palabra had already built the right mechanism**: `JoinPolicy`, a
   read-time projection, where `Pick` keeps both values and every peer shows the
   same one. It replaced the auto-resolve writes the first pass proposed.
   `NetOptions::joins` defaults to `presentational_joins()`: `placement`,
   `content.pos`, `content.size`, `content.collapsed` and `content.route.*` pick,
   and everything else conflicts. Pinned: a partitioned drag converges silently,
   and a partitioned text edit still asks on both devices.
   **On "last":** Palabra refuses wall-clock LWW on principle, and they are right,
   but causally ordered writes already are last-writer-wins. A Lamport-ordered
   `Latest` was proposed to them for the concurrent case.
2. **Selection comes first, for agents too.** *"whoever selected this specific
   port on the node first, is the one who is doing stuff with it."* Built as
   `voidmaiz/claims.hpp`: a Lamport clock in presence, then person over agent, then
   the smaller stamp, then the smaller id. Ports of one node do not contend, and a
   node claim covers its ports. `gate()` is how an agent's `link` gets refused
   before it lands.
3. **Interaction nets are the strongest case, not a trap.** The research
   bore it out. Every scenario the author named (gone, missing, duplicated, split,
   transformed, plus overlap) has an answer. The shared-wire case is **HVM2 §5**
   (Taelin): wires as variables, each end written only by the rewrite consuming
   its agent. One principal port means one writer per end, and that is no conflict
   in Palabra's registers. Sent to Palabra as a research ask with the literature
   (Lafont; DPO parallel independence and Critical Pair Analysis; IPA and
   Explicit Consistency; Kleppmann's move operation):
   `../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-concurrent-rewrites-are-our-strongest-case-2026-09-20.md`.

**Not sent: the derived-id ask to Void Core.** Palabra's own notes record that
Core deliberately re-mints random ids when a step is *committed* ("committing is
authoring"), so asking Core to commit with derived ids would contradict a
decision they made for a reason. The question now lives inside the Palabra
thread (§3.5, item 4), where the merge-by-reduction design already sits.

**Android LAN.** *"if it doesn't exist, it SHOULD."* Split on the Q30 line: the
transport is Palabra's (a `voidpalabra_lan` companion target lifted from
Hormiga's portable sealed session is requested), and the platform facts are ours.
`voidmaiz/lan.hpp` (target `voidmaiz_lan`, opens no socket) ranks interfaces for
the LAN (it picks `Wi-Fi 10.0.0.169/24` on the author's machine), encodes a
**join code** (`"023-47801"` on a /24) so a phone joins from a digits keypad
without the Java keyboard, and holds Android's **Wi-Fi multicast lock** through
JNI. The lock is opt-in, because the research found discovery mostly does not
need it: Android filters incoming *broadcast* but never unicast, so "every device
beacons, and a hearer answers by unicast" gets phone and desktop together with no
lock, and only phone to phone needs it. Compiled clean with the NDK for arm64 and
**not yet run on a device**. The IC APK now declares its four install-time
permissions (it had none).

**Also built:** presence carries the in-flight half of every canvas gesture
(`CanvasPresence`: cursor, visible rect, a drag as ONE offset, pending wire,
marquee, resize, typing with a capped preview, ping) plus participant kind,
device, a compat token and recent command lines. It is filtered by rule 3 end to
end: nothing in flight can name a private rune, pinned down to the bytes.
`Network::tick(…, CollabOut)` carries all of it through a real Palabra session,
including the mantle-wide crank claim. One bug was found on the way: our first
attempt read mantle names as object keys, but `mantles` is an array (Core SPEC
§3.4), and the test caught it.

**For the author:** a [user testing guide](/testing/collaborative-canvas-user-tests.md),
with 30 scenarios that each name the decision they test, so a verdict lands on a
number. Most wait on the drawing and the IC integration (stages N0–N4).

Suite: **17/17 gating** (new: `collab_smoke`, `lan_smoke`, and three blocks in
`net_smoke`), `reduce_conformance` unchanged at 17/25.

## 2026-09-21: Palabra answered, fusion is measured through the whole stack, and the sockets need a home

Palabra's reply (`MESSAGE_FOR_VOIDMAIZ_palabra-concurrent-structure-answered-2026-09-20.md`)
answered the research with code. They built `links.hpp`: equivalence (the
partition lattice), capacity and acyclic rules, checked after a merge, reported and
never auto-repaired. They made **wires as runes plus `"="` fusion** the normative
encoding. They also built a Lamport `FieldJoin::Latest`, now `placement`'s default,
`writers()`, a stream envelope, and `Timing::coalesce`. **They declined the socket
layer**, on the author's lean that Palabra should not own Android networking.

**Adopted the same day.** `presentational_joins()` now declares view state
`Latest`: the author's "last one wins", with no wall clock. `voidmaiz/wires.hpp`
reads wire classes out of a projected scene with a local union-find (so a net draws
in a build with no Palabra) and collapses them into node-to-node wires. Each drawn
wire carries `via` so gestures compile to attach, detach and fuse, and a class with
more than two ends is drawn `contested` rather than hidden. It also compiles the
encoding.

**Measured end to end** (`net_smoke`): the exact shared-wire case, two partitioned
devices each firing one of two adjacent redexes, healed over a link dropping 30 %,
converges on the wire **`a2.1–c2.1` that neither device wrote**, with zero anomalies,
zero conflicts, and Palabra's `check_links` (IC's rules) clean on both. The
plain-edge contrast, kept so the first block cannot pass vacuously, converges on a
net missing the wire and reports `link_broken`. One detail matched Palabra
independently: a removed agent's attachment must be skipped, not counted, or it
becomes a phantom third end. Evidence sent back:
`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-fusion-measured-through-the-whole-stack-2026-09-21.md`.

**Q36 opened**: where the LAN socket layer lives. Lean: grow `voidmaiz_lan` into it.
It gates stage N3 only.

Suite: **18/18 gating** (new: `wires_smoke`, and two `net_smoke` blocks),
`reduce_conformance` unchanged at 17/25.

## 2026-09-21 (second pass): Stage N0. Interaction Combinators collaborates, and a bench to feel it on

**IC now stores its connections as wire runes**, through three library pieces:

- `CanvasStyle::wires`. The canvas compiles every wire gesture (drop, rewire,
  cut, add-and-link) through a `WireWriter`. It is empty by default (plain edges,
  unchanged for every other host). `reified_writer` writes segments, attachments
  and cuts, with each rewire still one batch.
- `reduce::to_net(const Scene&)`, so a host reduces what it draws: the collapsed
  scene, not mantle JSON that now holds wire runes.
- `compile_upgrade`, a one-time conversion of plain `i:j` edges (every saved
  project, and IC's starter net) into wire runes. Pinned: the net read back after
  the upgrade equals the net the mantle reader read before it.

IC's rewrite step is the interesting part. Consumed agents are removed and new
ones minted (names scoped by device), and **every boundary is inherited by a new
segment FUSED onto the old wire**. An annihilation that joins two boundaries just
fuses their two wires. Nothing shared is edited in place, so two devices' adjacent
steps compose. That is the result measured in `net_smoke` yesterday, now in the
app itself.

**Sessions.** IC has Solo, Host and Join roles. A joiner starts with **no mantle
and no starter net** and `use`s the shared mantle when the first merge brings it
(E12). The log's actor comes from the profile (E19, it was hard-coded `lafont`).
There is a status pill, `CanvasNet` selection marks, and **remote changes play
instead of teleporting**: a merge that removed a facing pair and minted agents
runs the rewrite animation, and anything else tweens (250 ms, per the UI
decisions). Networking compiles only when `voidmaiz_net` exists (`IC_NET`), so the
solo app and the Android build are unchanged. The APK builds, with the four
permissions confirmed in the package.

**The duo bench** (`interaction_combinators_duo`): two windows in one process,
each its own IC with its own Core, replica and Palabra session, joined by an
in-memory link with latency, loss and a partition switch. Two Dear ImGui contexts
(1.92's GLFW backend supports them). It needs no sockets, so it exists before Q36.
**`--selftest`** drives the real app code in hidden windows: the joiner
converges, a remote step follows, two steps fired on *different* pairs while
partitioned heal into one valid net, and reducing to normal form ends identical,
with zero anomalies, conflicts or broken wires throughout (17/17). Launched
visibly and screenshotted: both windows render the same net, "in sync with 1".
Two defects were found that way and fixed: the colour dot is a glyph the bundled
font lacks (now drawn), and the windows assumed an 1840-px screen (now half the
monitor's work area each).

**For the author:** the testing guide's N0 row is *ready*, with launch steps and
a new scenario (P1, work apart then meet). C1, C3, C5 and B4 are runnable on the
bench, without the N1 drawing.

Suite: Void Maiz **18/18 gating**, `reduce_conformance` unchanged at 17/25; duo
selftest 17/17.

## 2026-09-21 (third pass): Interaction Combinators 0.2.0 is released, for Windows and Android

The author: *"let's just continue development on the interaction combinators
application. releasing for apk and desktop devices."* IC had never been under
version control. It is now **public at https://github.com/migriv24/InteractionCombinators**,
with release **v0.2.0**: a Windows x64 zip (the app, the duo bench, and the four
DLLs they need) and the Android APK (versionName 0.2.0, versionCode 200, the four
LAN permissions in the package).

- One `VERSION` file feeds both builds and the window title. The APK's
  versionCode is derived from it, so it only increases.
- `tools/package_release.ps1` builds an optimized desktop build, stages it,
  **runs the duo selftest from the staged folder with no toolchain on PATH** (to
  prove the folder is self-contained), zips it, and builds the APK. The released
  zip was then downloaded from GitHub, unzipped fresh and self-tested again: green.
- The signing key, APKs, build folders, layout files and inter-agent messages
  are ignored. The pre-commit dry run staged exactly the 18 source files.

**A test bug the release pipeline caught.** The duo selftest waited a fixed number
of *frames* for a partition to heal. It passed in the unoptimized dev build and
failed in the optimized release build, whose hidden windows run so much faster
that 240 frames were shorter than Palabra's 2 s resend. It now waits on time.
The sync was never wrong; the test assumed frames were time.

Also pushed the same day: Void Maiz `1f0bcb1` (the collaborative canvas work) and
Void Palabra's local commit `17293db` (unchanged), which Void Maiz's new code
depends on, so a fresh clone builds.

Known limits, stated in the release notes: the APK is solo (no LAN transport,
Q36), collaboration shows changes but not yet cursors or in-flight gestures (N1),
and there is no packaged Linux or macOS build.
