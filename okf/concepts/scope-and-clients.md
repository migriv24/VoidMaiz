---
type: Concept
title: Scope & clients
description: "Who builds on Void Maiz (most UI-bearing Void Core apps, VLS first) and who explicitly doesn't (FaultSack-class abstract apps, ESP32-class embedded)."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-09T00:00:00Z
---

The author's framing, founding day: *"i imagine that MOST applications will build
on top of this. this is very much for applications that are software in nature,
and will have a UI of some sort"* — with explicit exceptions.

# The layer picture

    application        (VLS, future tools)          — domain semantics + compute
    Void Maiz          (this project, C++20)        — node UI: projection + gestures
    Void Core          (../VoidCore, C ABI)         — model, dispatcher, tags, undo, log

An application may also bypass Void Maiz entirely and sit on the core alone —
that's not a failure mode, it's the design.

# Clients (in intended order)

1. **Void Loops Studio (native)** — the first client and the forcing function.
   The production order (author, 2026-07-09): *recreate what the Python VLS
   already has — better — piece by piece*. Every Void Maiz v1 feature is
   justified by a VLS-parity need; VLS's Python editor + its 16 green test files
   are the behavioral reference.
   *(In practice the demos arrived first: InteractionCombinators shipped
   2026-07-11 as the exit-test vehicle, Node Blocks followed 2026-07-15.)*
2. **Void Hormiga** (`../VoidHormiga`, founded 2026-07-15) — the ground-up
   native C++20 rebuild of Hormiga (outreach/newsletter app): contacts/events/
   images as tagged runes, newsletters and websites as Blockly-style statement
   stacks built on Node Blocks Phase A. The author's call reversed the old
   web-adoption lean (see [substrates](/concepts/substrates.md)); it conforms
   the way any host does — needs arrive as `MESSAGE_*` files, never as edits.
   Its weight: the first *data-heavy* client (10²–10³ runes, table view, tag
   grammar as primary navigation, volunteers as users).
3. **The visual sister app** (TouchDesigner/Cables lineage, planned in VLS's
   vision doc) — the client that keeps Void Maiz honest about being
   domain-agnostic: same substrate, different signal types and faces.
4. **Future authoring tools** — anything node-shaped: pipeline editors, patching
   tools, the draft's wilder ideas (device graphs, system graphs) as they earn
   their way in.

# Explicit non-clients

- **FaultSack-class applications** — abstract in nature; a study/critique tool's
  UI is not a node canvas. It stays directly on the core.
- **Embedded / IoT (ESP32-class)** — a device running Void Core has no UI at
  all; Void Maiz's rendering stack would be dead weight. (A *desktop* tool for
  configuring such devices could be a Void Maiz client — the device itself never
  is.)
- **Anything needing an embedded execution engine** — see
  [differentiation](/concepts/differentiation.md) non-goals.

# What "generalizable" must mean concretely

The v1 canvas will be built against VLS's needs; generality is enforced by rules,
not hope:

- **No audio/music concepts in the library.** Signal types, port names, colors,
  face meanings arrive as glyph metadata registered by the host. (The core's own
  test: "categories are domain-agnostic; signal types carry the domain.")
- **The second client test**: any feature that can't be described without saying
  "note", "clip", or "plugin" belongs in VLS, not Void Maiz.
- **C++ core embedding stands alone**: the RAII wrapper over the C ABI must be
  usable by a client that never links the UI (this also gives FaultSack-class
  C++ apps something without dragging in rendering).
