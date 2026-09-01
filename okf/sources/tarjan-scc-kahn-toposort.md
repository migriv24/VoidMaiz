---
type: Source
title: Tarjan (SCC) and Kahn (topological sort)
description: "The two graph algorithms the Sentinel is built from: strongly-connected components in one pass, and layered topological ordering. Both are the decidable half of a question whose general form is undecidable."
tags: [status:draft, audience:kernel, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What they are

- **Robert Tarjan, "Depth-first search and linear graph algorithms" (1972)** —
  strongly-connected components in **O(V+E)**, one DFS pass, using an index and
  a low-link value per vertex.
- **Arthur Kahn, "Topological sorting of large networks" (CACM, 1962)** —
  repeatedly emit vertices of in-degree zero. Whatever remains when none are left
  is exactly the part that **cannot be ordered**, i.e. lies in a cycle.

# What we use them for

- **Tarjan** — cycle detection in the effect graph
  ([emergence](/../../VoidAllomone/okf/concepts/emergence.md), `sentinel.cpp`). The SCC **is
  the report**: it names precisely the scripts forming the loop.
- **Kahn** — the strata (`EffectGraph::strata`), and separately the expansion
  order for `define` ([language](/../../VoidAllomone/okf/concepts/language.md)). In the
  parser Kahn was chosen over recursive expansion for two reasons: a cycle must
  be *reported* rather than recursed into, and our Tarjan implementation is
  recursive, which is a known stack hazard we did not want twice
  ([testing/plan](/../../VoidAllomone/okf/concepts/testing/plan.md)).

The reason both matter: *"does this rule set terminate?"* is **undecidable** in
general, but *"is this finite graph acyclic?"* is decidable in linear time. The
whole Sentinel design turns on substituting the second question for the first
and being honest that it is conservative.

# Why they are credible

Both are foundational, textbook algorithms (CLRS covers both), taught
universally, with well-known complexity bounds. Not contested in any respect we
depend on.

# What a verification pass should check

- **Tarjan 1972**, and **O(V+E)**.
- **Kahn 1962, CACM**, and that leftover vertices correspond exactly to cyclic
  ones.
- That **Datalog stratification** is the correct name for what our strata
  implement — the connection is asserted in `foundations.md` and is the one part
  here that is an interpretation rather than an algorithm. *(The usual citations
  are Apt/Blair/Walker and Van Gelder, both 1986–88; neither is currently a
  source rune.)*
