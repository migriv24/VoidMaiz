---
type: Roadmap
title: Roadmap
description: The phase plan — concept first, then a C++20 core embedding, then the canvas built piece by piece against VLS parity, then the native VLS on top.
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-07-09T00:00:00Z
---

Replaces the archived draft's §10 timeline (which planned an execution engine,
BaaS sync, and code generation — all cut or reassigned by
[log-first inheritance](/concepts/log-first-inheritance.md)). Phases are gated by
*client need* (VLS parity), not by calendar. The production emphasis, per the
author: **first recreate what we already have — better — piece by piece.**

# Phase 0 — Concept (EXITED 2026-07-10)

The OKF you are reading. Exit conditions met: the author's direction summary
(`promptToStart.md`, 2026-07-10) answered the founding questions, and Void
Core's reply (2026-07-09) resolved the upstream message — the reduce gap is now
a **portable contract** (`VoidCore/conformance/reduce/`) to port, attribution is
implemented, and we built against **0.2.3** (now **0.2.4**, which adopted our
reduce findings as contract cases 11–14 — see the log, 2026-07-14).

# Phase 1 — The core embedding (C++, no UI yet) (NOW)

A thin C++20 RAII layer over `voidcore.h` (manager lifetime, string ownership,
`{ok,lines,data}` and state-document round-trips, glyph registration, log-sink /
effect-handler binding as `std::function`). Usable by ANY C++ Void Core app,
UI or not. Plus the build skeleton: CMake, the vendored-deps policy, and a smoke
test driving a real mantle through dispatch from C++.
*Exit test: a C++ program replays a saved VLS project state and walks its nodes.*

# Phase 2 — The projection engine + first window

State document → scene graph (nodes, ports, wires from link relations),
incremental re-projection after each dispatch (the one-sync rule), depth
auto-layout for placeless nodes; Dear ImGui vendored; a window with a flat-color
canvas drawing dumb boxes and wires from a real state document. No interaction.
*Exit test: a saved VLS patch renders recognizably.*

# Phase 3 — The interaction grammar, piece by piece (the VLS-parity climb, Blender-flavored)

Prep: the **Blender vs LiteGraph feature matrix** (open question #7) — sort
both editors' features into universal vs application-specific; Blender's UX is
the favored model, LiteGraph supplies the already-learned gesture vocabulary.
Then each piece lands with its gesture→command compilation and its log line, in
roughly this order (mirroring how the VLS editor actually got used):

1. select / marquee / camera (pan/zoom) — view gestures, logged per open Q#3
2. node drag → `/move`-equivalent batch (positions as model content)
3. wire drag with type checking + multi-inlet spare slots → `link`/`unlink`
4. add-search box at cursor (Shift+A) → `rune new`; delete → `rm`
5. field editing (inspector panel) → `set`/`setjson`
6. collapse/maximize window chrome; subgraph enter/exit (mantle switching)
7. undo/redo surfaced; the log strip; the command bar (the CLI *inside* the UI)

*Exit test: rebuild the VLS starter patch from nothing, mouse-only, and the
resulting CLI transcript replays to an identical state document.*

# Phase 4 — Faces & widgets (the VLS signature features, generalized)

Per-glyph face renderers (the host registers a `project`/`compile` pair for a
glyph); face widgets (slider/drag-number/combo with the staging discipline);
nodes-as-windows sizing. VLS's piano roll becomes the proving face — implemented
in VLS-native code against Void Maiz's face API, NOT inside Void Maiz.
*Exit test: a third-party-style face (not written by us in the library) works
without library changes.*

# The proving client — InteractionCombinators (rides phases 2–4)

The first host application (`../InteractionCombinators`, not yet created): a
shortcut-launchable desktop showcase rendering Lafont's γ/δ/ε agents and their
six rules — fettuccine rewriting live — alongside an ordinary dataflow patch
(linguine), exercising faces/chrome, the gesture→command log strip, lasagna
undo, and tag filtering. It is the *exit-test vehicle* for phases 2–4 (open
question #8 fixes its scope) and the permanent developer sandbox for new
widgets and faces. It also hosts the **reduce port**: the C++20 port of
`VoidCore/conformance/reduce/run.py`, checked against the pinned cases by
canonical-form comparison (17/25 as of 2026-09-01; boxes not yet ported) — application-side, per the compute boundary, though
the port may live in this repo as a reusable example.

# Phase 5 — VLS-native rides (parallel track)

The native VLS begins consuming Void Maiz in earnest: its glyph set, its faces,
its compile pipeline (Reduce/Scry — consuming the same portable reduce
contract), the streaming audio engine (miniaudio-class) and the native
RemoteVstPlugin client. Tracked in VLS's own OKF; Void Maiz only tracks the API
pressure it produces.

# Phase H — Headless (parallel track, founded 2026-08-18)

Not a stage of the GUI climb but a **track beside it**, because it consumes the
same seam from the other end (the author's secondary objective;
[headless](/concepts/headless.md)). Staged independently of the Phase 1–5
numbering on purpose: nothing here waits on the canvas, and the canvas waits on
none of it.

- **H0 — session and front-end** *(built)*: `HostApp`, `Session` (load →
  dispatch → save, attributed, advisory-locked, journalled), `run_cli`.
- **H1 — the briefing** *(built)*: `capabilities()`, assembled from `help`,
  `glyphs`, `mantles`, `ActionRegistry::manifest()` and the OKF root.
- **H2 — actions from the terminal** *(built)*: invoking a registered
  `ActionDescriptor` by name, so an agent uses a *view's* vocabulary without the
  view.
- **H3 — the effect seam** *(built)*: `save` / `deploy` / `build` driven
  headlessly, behind an explicit grant. `EffectOp` makes the holiday seam
  enumerable (it was one opaque `std::function`); `EffectPolicy` defaults to
  **refuse**, because everything else an agent does is reversible by `revert`
  and an effect is not. Gated at the **verb** rather than the handler, since the
  core snapshots `_baseline` regardless of the adapter and cannot fail a
  dispatch from one. Still missing: a worked client task against a real backend,
  against a real backend.
- **H4 — precedence and streaming** *(built)*: a Human session may take the
  document from an Agent session, and the evicted agent cannot write afterwards
  (the author's rule — a person always outranks an agent, even headless);
  streaming effects emit line-by-line to the terminal and the journal, per
  SPEC §9. Plus [the adoption guide](/concepts/headless-adoption.md) and
  messages to all four client agents.
- **Later** — true concurrency with a live GUI (**Q20**, leaning single-writer);
  a dry-run that prints the whole transcript without dispatching; emitted effect
  lines reaching Void Core's own `log` buffer (the C ABI exposes no host
  log-write today, so they live in our journal only).

*Exit test for the track, and the author's own: hand an agent a folder of source
material and a sentence, and find the result waiting — and editable — when the
GUI is next opened.*

# Later / research (kept from the draft, honestly labeled)

- **Second view**: timeline or tree — the projection-architecture test.
- **Input holidays**: MIDI controllers etc. emitting dispatcher commands.
- **Branch/merge of histories** — upstream research; not v1.
- **Vulkan backend** — the renderer keeps a thin abstraction seam, but no
  second backend until a client needs one.
- **Micro-undo** — optional per-application granular undo of view actions
  (the lasagna model's bottom-layer opt-in).
- 3D/spreadsheet/code views, device graphs, system-as-node — client-driven only.
