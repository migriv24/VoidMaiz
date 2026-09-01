---
type: Concept
title: Total observability
description: "The founding commitment made precise: every gesture is a dispatcher command — with a deliberate two-tier answer for model gestures vs view gestures."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-09T00:00:00Z
---

The author's requirement that shaped the whole stack decision: *"libraries…
that have enough control to actually be able to log things like node movements…
the CLI is expanded to the point where EVERYTHING is logged."* Immediate-mode
rendering (Dear ImGui family) was chosen precisely because every interaction
passes through our code every frame — nothing happens behind the library's back.

# The principle

**A gesture that changes anything compiles to a dispatcher command.** The user's
session is therefore a readable CLI transcript: replayable, undoable, scriptable,
and — because agents issue the *same* commands — a shared language between the
human, the log, and any AI collaborator. VLS lived this for a week and it changed
how bugs get found: the 2026-07-09 "mix doesn't work" report was diagnosed by
*replaying the author's transcript*, which proved the engine innocent in minutes.

# Two tiers of gesture — the lasagna model (the precision the slogan needs)

The author's name for this layering (2026-07-10): **the lasagna model**. You
undo the top layers (meaningful, model-changing actions), never the view-layer
noise underneath — though everything is still logged. An optional per-application
**micro-undo** setting may later expose granular undo of view actions.

Not every gesture deserves an undo frame. The VLS position work surfaced the
tension: moves-as-model-content made layout visible and persistent — and made
`undo` sometimes pop a *move* when the user expected it to pop a *note*. The
draft answer (open question #3 refines it):

- **Model gestures** — anything that changes what the graph *is*: add/remove
  node, wire/unwire, set a field, add a note, group into a subgraph. These are
  dispatcher commands in the undo slice. Non-negotiable.
- **View gestures** — anything that changes how the graph is *seen from here*:
  positions, collapse state, window sizes, camera, active view. These MUST still
  be logged and persisted (that's the point of Void Maiz). Where they sit:
  upstream's 2026-07-09 reply endorsed **positions-as-content in the main slice
  for now** (the VLS pattern) and proposed a `place` verb writing the reserved
  `rune.placement` field — logged, attributed, persisted, **outside the undo
  slice** (the proven `config` tier semantics, applied per-rune) — pending the
  author's sign-off. Design rule until then: view writes must be
  *distinguishable* commands, so the gesture compiler can retarget them to
  `place` the day it lands. Batches of node drags coalesce into one undo step
  either way.
- **Ephemera** — hover, marquee-in-progress, half-dragged wires. Not logged by
  default; optionally streamable (a `--verbose-input` faucet) for session
  research and agent training data. Never state.

## The author's criterion for what gets logged (2026-07-13)

Stated when hover highlights landed unlogged: **"things that need to be logged
are things that change states — when you save a project, quit, and log back
in, what do you expect to be the same?"** Node colors, yes. Positions and
where you dragged them, yes. What you last *hovered*? No — nobody cares. The
save/quit/reload test is the working razor for sorting a new gesture into the
lasagna.

With the author's own caveat, recorded deliberately: *"TECHNICALLY,
theoretically it could matter"* — so where hover (and ephemera generally)
falls is **a choice each application makes, not a Void Maiz law**.
InteractionCombinators chose unlogged hover; an application doing session
research or attention analytics could stream it (the `--verbose-input` faucet
above is exactly that door). The library's obligation is only that the
*default* passes the save/quit/reload test and that the door exists.

## Two hard cases, sorted by the razor (2026-07-14)

- **Rewrite animations are ephemera.** The model changes the instant the step
  batch dispatches (one log line, one undo frame); the smush-fuse-emerge
  choreography is the VIEW replaying that change. It rides `CanvasFx`
  (canvas.hpp) — per-frame transform overrides and ghosts the host rebuilds
  from its own clock — and dropping the struct snaps the picture to the model.
  Save/quit/reload: the animation was never state. Its *settings* (on/off,
  speed) DO pass the razor and ride the config tier (`view.anim`), like the
  camera and the panel layout (`view.panels`).
- **Live physics is a drag the computer performs.** Continuous relaxation
  dispatching a move per frame would drown the log, so it borrows the drag
  gesture's staging discipline verbatim: positions iterate view-side as
  ephemera (`relax_step`), and land as ONE `batch` when the system settles —
  or immediately before any other command dispatches, so the transcript never
  interleaves a settle inside another action's story. The one-shot form
  (`compile_relax`, Edit > Relax layout) is clean's sibling: an explicit
  placement act, one undo frame.

# Attribution (implemented upstream, 2026-07-09)

The upstream ask landed in Void Core **0.2.3** (SPEC §9): `config set actor
<name>` stamps `who` on every log record (`[ISO] LEVEL op (who): message`) and
on every undo frame (`history` shows a trailing `[who]`); every successful
top-level mutating command logs itself (`batch` logs once; failed commands log
nothing). The actor is state (`state.config.actor`) — session-scoped, survives
export/import, switches instantly mid-session. Void Maiz adopts upstream's
suggested **`kind:name` convention** (`human:kris`, `agent:claude`,
`holiday:midi`) as its OKF standard so attribution stays parseable across apps.

# What this buys (the draft's §3, grounded)

- **Agents are first-class users** with zero extra machinery: they call
  `vc_dispatch` like the mouse does. Void Maiz's job is only to guarantee the
  mouse never does anything an agent couldn't.
- **The transcript is the tutorial**: "how did I do that?" has an answer you can
  paste.
- **Session replay** is state + command list — bug reports become reproducible
  by construction.
