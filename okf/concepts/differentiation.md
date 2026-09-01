---
type: Concept
title: Differentiation — why build our own node library
description: "The author's direct question answered: what makes Void Maiz unique against LiteGraph, imnodes, Blender nodes, Unreal Blueprints, TouchDesigner, and friends."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-09T00:00:00Z
---

The author, founding day: *"since we are making our own custom node graph library…
what will make it unique? there are other node libraries out there, so why ours?"*
An honest answer, because a dishonest one would doom the project.

# What every existing library does

LiteGraph, Rete, imnodes/ImNodes, ImGui node editors, QNodeEditor, Blender's node
system, Unreal Blueprints, TouchDesigner: **they all own the graph.** The library
holds a Graph object; your application syncs its model with it (or worse, the
library's graph *is* your model, serialized in its format). Consequences we lived
through in the VLS prototype's LiteGraph era:

- Sync is your problem: we rebuilt LiteGraph's graph from `/state` after every
  mutation and null-ed its drag state to avoid crashes — the "auto-repair" trick
  was a workaround for a library that wants to own truth.
- Observability is theirs to grant: logging node *movements* required pushing
  positions into our model because the library's private state was invisible.
- Undo, persistence, scripting, agents: all bolt-ons, reimplemented per app.

# What makes Void Maiz different (each point checkable, not vibes)

1. **It owns nothing.** The graph is a Void Core mantle; Void Maiz is
   projection + gesture compilation ([views](/concepts/views-as-projections.md)).
   There is no sync problem because there is no second copy.
2. **Total observability is structural, not a feature.** Every gesture compiles
   to a dispatcher command; the session is a replayable CLI transcript
   ([total observability](/concepts/total-observability.md)). No other node
   library can even express this, because their gestures mutate private state.
3. **Agents are first-class by construction.** An AI collaborator issues the
   same commands the mouse does, against the same seam, with the same undo. The
   draft called this "native AI collaboration"; here it's just… the dispatcher.
4. **Undo, persistence, batch atomicity, scripting, tags are inherited**, not
   implemented: Void Core provides them once, every client app gets them.
   A tag-filter (`vc_tag_match`, stateless) can drive *visual* selection — "dim
   everything not matching `drums AND NOT muted`" is a render-time one-liner.
5. **Composition is mantles.** Subgraph-as-node (VLS's Loop) is the core's own
   composition unit, not a library feature with its own serialization format.
6. **Faces are the domain's, chrome is ours.** A node can *be* a piano roll, a
   plugin window, a camera feed — per-glyph face renderers with the window model
   proven in VLS.

# What we deliberately do NOT claim

- Not "the fastest node canvas" (ImGui-based tools are fast enough; we compete
  on architecture, not benchmarks).
- Not an execution engine — the application computes (compute boundary). Anyone
  needing Unreal-Blueprints-style embedded execution is not our client.
- Not a standalone app. Void Maiz without a host application does nothing —
  by design.

# The one-line answer

**Other node libraries are canvases that own your graph; Void Maiz is a lens over
a substrate that already knows how to remember, undo, replay, and talk to agents.**
The uniqueness isn't a feature we add — it's everything we refuse to own.
