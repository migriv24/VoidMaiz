# Void Maiz

**A node-graph UI library for applications built on
[Void Core](https://github.com/migriv24/void-core).** C++20, founded 2026-07-09.
The OKF comes before the code, as always.

Void Maiz is the *view and interaction* layer for Void Core applications that are
software with a UI: it draws a Void Core mantle as a node graph and turns every
gesture — wiring, moving, tweaking a knob — into a **visible dispatcher command**.
It owns no model state of its own. The graph you see is a *projection* of the core's
state document; the log of what you did *is* the interaction.

- **Clients:** [Void Hormiga](https://github.com/migriv24/VoidHormiga) is the
  one in production use; the **InteractionCombinators demo**
  (`../InteractionCombinators`, built, not yet published) renders Lafont's γ/δ/ε
  agents rewriting live on the canvas and is the exit-test vehicle for the
  roadmap's phases 2–4; **NodeBlocks** (`../NodeBlocks`, likewise) is the third.
- **Next client:** the native rebuild of Void Loops Studio (`../VoidLoopsStudio`,
  the node-based DAW), whose Python editor is this library's proven prototype.
- **Not for everything:** Void Core apps that are abstract in nature (FaultSack) or
  embedded (ESP32-class IoT) don't need Void Maiz — see
  [scope & clients](okf/concepts/scope-and-clients.md).

## Headless (secondary objective, founded 2026-08-18)

The same application, with no front-end attached — so an **agent in a real
terminal** can use an installed application's engine to do real work, and a
person finds the result waiting (and editable) when they next open the GUI:

```
maiznotes --describe                               # what can you do?
maiznotes --actor claude --script task.vs --atomic # do it, all-or-nothing
maiznotes status ; maiznotes diff                  # what did it change?
maiznotes revert                                   # ...and throw it away
```

A person always outranks an agent: opening the GUI takes the document from a
running headless session, whose next command fails and whose save is refused, so
your work is never overwritten by a session that no longer holds the floor.

Document work is unattended; **publication is not**. Effects (`save`, `deploy`,
`build`, …) are the one thing `revert` cannot undo, so they are refused by
default and granted per run — `--dry-run-effects` to rehearse,
`--allow-effects=deploy` to mean it. A refusal quotes the application's own
description of what you were asking for:

```
refused: `deploy` reaches outside the document and this session was not granted
effects (deploy: pushes to the public URL — visible to everyone, immediately).
```

This does not contradict the GUI-first stance; it is its consequence.
Commitment 2 already said humans, scripts and agents share one interaction
surface, and commitment 1 means a headless run and a GUI run are two front-ends
over **one state document** — so there is no sync, no import, and no second code
path. See [headless](okf/concepts/headless.md); the whole front-end of an
application is `maiz::run_cli(app, argc, argv)`, and
[`examples/headless_app.cpp`](examples/headless_app.cpp) is a working one.
Adding this to an existing Void Maiz app is ~40 lines, not a port — see
[headless adoption](okf/concepts/headless-adoption.md).

Start reading at **[okf/index.md](okf/index.md)**. The founding question — *why
build our own node library?* — is answered in
[what makes Void Maiz unique](okf/concepts/differentiation.md).

## Status

**Phase 1 exited; the canvas, Allomone and headless are all live.** The embed
layer, the projection engine, the interaction grammar, the Dear ImGui canvas,
the Allomone derivation language (`voidmaiz_allomone`, now re-exported from the
sibling [`../VoidAllomone`](okf/index.md)) and the headless front-end
(`voidmaiz_headless`) all build clean, and **11 of the 12 ctest cases pass**
(verified 2026-09-01 from a fresh configure).

The one red case is `reduce_conformance`, and it is a real gap, not a flake:
**17 of Void Core's 25 pinned reduce cases pass**. Cases 1–16 and 21 are green,
including case 15's derived agent ids matching the Python reference byte for
byte. The eight failures are all one story — Void Core extended the portable
reduce contract on 2026-09-01 with **boxes** (nesting: cases 17–20, 23, 24), a
**reserved separator** (22) and a **`patch` rule** (25), and this C++20 port
predates all of it. Nothing that used to pass regressed; the contract grew.
Porting the box semantics is the next reduce task. If you build against
`voidmaiz_reduce` today, you get a conforming executor for flat nets and an
`unknown rule` error for boxed ones.

Clients: **Void Hormiga** (`../VoidHormiga`, adopted through Allomone step 3),
InteractionCombinators, NodeBlocks. See the [roadmap](okf/roadmap.md) for the
phase plan and [developer questions](okf/developer_questions.md) for what is
still open. Inter-agent messages are tracked in
[okf/index.md](okf/index.md#inter-agent-messages) — one open message per
correspondent, retired by the index rather than by deletion.

## Building

Needs a C++20 compiler, CMake and Ninja. **Two sibling repositories are
expected beside this one**, which is the Void family's host pattern — a missing
one fails configuration with a message naming the variable to set:

| sibling | why | override |
|---|---|---|
| [`../VoidCore`](https://github.com/migriv24/void-core) | the model, the dispatcher, the state document. Desktop links its prebuilt `libvoidcore.dll`, so **build it first**; Android compiles the C sources in. | `VOIDCORE_ROOT` (default `../VoidCore/core`) |
| `../VoidAllomone` | the derivation language, compiled from source with `add_subdirectory` and re-exported into `maiz::`. **Not yet published** — clone it beside this repo when it is. | `VOIDALLOMONE_ROOT` (default `../VoidAllomone`) |

Everything else — Dear ImGui, GLFW, cJSON — is vendored in `vendor/`. No package
manager, no CDN, no framework.

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
build\bin\maiz_canvas.exe            # the canvas demo
build\bin\maiz_headless.exe --describe   # the headless briefing
```

Layout: `include/voidmaiz/` (public headers) · `src/` — `embed/` (the C++ core
wrapper), `project/` (state document to scene), `gesture/`, `action/`,
`view/` (ImGui), `usergraph/` (the attention graph), `allomone/` (host
predicates + the sentinel scene), `reduce/`, `headless/` · `vendor/` (vendored
third-party, with licenses) · `tests/` · `examples/` · `okf/` (the design).

Targets are separable on purpose: a GUI app that will never run headless links
only `voidmaiz` + `voidmaiz_view`, and a headless one links `voidmaiz_headless`
and no window system at all.
