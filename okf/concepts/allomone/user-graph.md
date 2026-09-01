---
type: Concept
title: Allomone — the user action graph
description: "The symmetric, timeless record of what a person touches together: affordances as nodes, frames as hyperedges, device as a first-class type, and the `with` operator that makes a rule responsive to the user rather than to the data. Why it is allowed to exist under the ephemera rule, and the Mylyn precedent we deliberately depart from."
resource: src/usergraph/usergraph.cpp
tags: [status:current, audience:kernel, audience:host, confidence:asserted]
timestamp: 2026-08-06T00:00:00Z
---

Part of [Allomone](/../../VoidAllomone/okf/index.md). Audience: **both**.

Implemented in `include/voidmaiz/usergraph.hpp` + `src/usergraph/`, pinned by
`tests/usergraph_smoke.cpp`. It lives in **base `voidmaiz`**, not the language
target — only Void Maiz sees gestures, and a host may want the structure without
any scripting.

# What it is, and what it is emphatically not

The author's framing, verbatim: *"an action map of user things, not a list of
previous actions, not trying to predict user actions, but a big graph that
represents user actions."*

Three refusals, each deliberate:

- **Not a log.** Nodes are **affordances** — the things a person *can* touch — not
  events. The graph answers "what goes with what," never "what happened."
- **Not a predictor.** Nothing here ranks, scores intent, or suggests a next
  action. It reports adjacency; interpretation is the rule author's.
- **Not ordered.** No timestamps, no direction, no sequence. That information is
  discarded **at the moment of recording**, not filtered out downstream.

# The structure

    Affordance  = { id, kind, device, touches }
    Coincidence = { a, b, weight }          -- canonically a < b
    Frame       = { members[], weight }     -- a set that recurred

A **frame** is a working window: everything touched between `open_frame()` and
`close_frame()` is considered one piece of work. Closing a frame contributes:

1. the **hyperedge** — the whole member set, retained intact, because a triple is
   genuinely not three pairs;
2. its **pairwise projection**, which is what the cheap coherence query reads.

Frames of fewer than two members contribute no edge (nothing co-occurred) but
their touches still count. Nesting flattens — a frame is a set, and a stack would
reintroduce the ordering the structure exists to refuse.

**The canonical edge ordering (`a < b`) is doing real work.** It makes "no
direction" a property of the representation rather than a convention someone must
remember; there is no field in which a direction could be written down.

# Device is a first-class type

    enum class Device { Pointer, Touch, Pen, Gamepad, Voice, XR };

A tap is not a click and a gaze is neither. Recording the modality a touch
actually arrived through is what lets a mobile substrate be honest instead of
pretending to be a mouse — the "new type for mobile" the author asked for, and
the reason it is an enum rather than a bool.

Per [substrates](/concepts/substrates.md)' quarantine: **the library records the
modality and never interprets it.** What `touch` implies for sizing, spacing or
affordance is entirely the host's business. `XR` is present so
[Void Maiz XR](/horizons.md) inherits the vocabulary rather than inventing a
parallel one.

# Why it is allowed to exist

[Total observability](/concepts/total-observability.md) rules hover and selection
**ephemera — "Never state."** A persisted attention graph would be state
accumulated from things declared never-state, which is a real contradiction and
not a technicality.

The resolution is the [surface census](/concepts/surface-census.md) pattern,
verbatim: **accumulate in-session, `compile()` to dispatcher commands.**

- By default nothing is persisted. A session that never materializes leaves no
  trace, so the ephemera rule holds untouched.
- When a host *does* call `compile()`, the graph becomes **runes in a mantle** —
  logged, attributed, replayable, undoable, prunable by ordinary command, and
  **queryable by Allomone itself**.

That last property is why this shape is better than a private library structure
rather than merely more compliant: a materialized user graph is just data, so a
rule can read it the way it reads anything else, and a person can inspect and
delete it with the tools they already have.

Materializing emits `allomone-affordance` runes plus `link … --relation
coherence:<w> --undirected` edges, in id order — so **two sessions that touched
the same things emit the same transcript regardless of the order the person
worked in.** Undirected is not decoration: the claim genuinely has no direction.

# The `with` operator

    when with "volume" -> glow 1

Every other predicate reads the data. `with` reads the **user**: it matches
subjects that share coherence with the named affordance. That is the difference
between a stylesheet and a system in continuous conversation with the person
using it — a rule can say *"whatever is coherent with the thing I'm working on"*
without naming a tag anyone had to apply.

`device "touch"` is the other user-reading predicate, matching on how a subject
was last reached.

**With no graph supplied, both simply never match** — silence, not an error. An
application that opts out of attention tracking still runs everyone's scripts.

# The precedent, and the departure

**Eclipse Mylyn** shipped this idea: a *degree-of-interest* model over program
elements, built from interaction events and used to filter the IDE down to what a
task actually touches. It is the one large-scale deployment of "the UI watches
what you work on and reshapes itself," and it worked.

Mylyn weights by **frequency and recency**. **We keep frequency and drop
recency**, deliberately — recency is a clock, and a clock inside the record is
exactly what makes the structure ordered.

**The cost is real and we are not hiding it:** without decay, stale coherence
accumulates. Something you worked on intensively once stays coherent forever.
Mylyn's experience says decay matters.

Two honest mitigations, and one non-answer:

- the graph is **model content**, so pruning is an ordinary command a user or a
  maintenance job can issue — visible policy rather than hidden;
- **frame weight** is a usable proxy for strength, so a host can threshold
  (`coherent_with(id, min_weight)`) without consulting time;
- the non-answer: adding a half-life would be simple and is **rejected**, because
  it would put a clock in the record. If decay proves necessary, the version that
  passes the [non-linearity](/../../VoidAllomone/okf/concepts/non-linearity.md) test is a
  host-side pruning *policy* that deletes edges — a mutation of the data, logged
  like any other — not a time-varying weight the graph computes.

# Framing policy is the host's

The library never decides when a frame opens or closes; it exposes
`open_frame()` / `close_frame()` and counts what arrives. The demo uses a 2.5s
idle timer.

**This is where a clock is legitimate.** A *policy* may consult time to decide
what belongs together; the *record* must not retain it. Other reasonable
policies: frame per selection, per undo group, per pane focus, or explicit "start
/ end a task" like Mylyn's task activation — which is probably the best of them,
because it asks the person rather than guessing.
