---
type: Source
title: Saraswat — concurrent constraint programming
description: "The constraint store as a shared medium agents add to rather than message each other through, with meaning as a closure operator over a lattice of partial information. The model Allomone instantiates most directly."
tags: [status:draft, audience:kernel, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Vijay Saraswat**, *Concurrent Constraint Programming* (MIT Press; the thesis
work is late 1980s), and **Saraswat, Rinard & Panangaden, "Semantic foundations
of concurrent constraint programming" (POPL 1991)**.

The model: concurrent agents do not exchange messages. They **tell** constraints
into a shared store and **ask** whether the store entails something. The store's
meaning is a **closure operator** over a lattice of partial information —
idempotent, extensive, monotone.

# What we use it for

[foundations §3](/../../VoidAllomone/okf/concepts/foundations.md): the claim that Allomone
*instantiates* this model rather than resembling it. Two consequences we lean on:

- **adding a constraint only ever refines** — so enabling a script cannot
  invalidate what other scripts concluded, which is what makes scripts
  independently authorable;
- **no message passing** — which is the same shape as the stigmergic reading
  ([Grassé](/sources/grasse-stigmergy.md)) arrived at from another direction.

# Why it is credible

POPL is a top-tier programming-languages venue; CCP is an established model with
decades of follow-on work (cc(FD), timed CCP, soft constraints). Saraswat is the
primary source for the framework's own terminology.

# What a verification pass should check

- Author names and the **POPL 1991** venue for the semantic-foundations paper.
- That **closure operator** is the correct characterization of the store's
  meaning (idempotent, extensive, monotone), rather than a related-but-different
  notion.
- That **tell/ask** are the primitives as we describe them.
- Whether the monotonicity we attribute to the store is the *same* monotonicity
  used in [CALM](/sources/hellerstein-calm.md), or two distinct notions we have
  been treating as one. *(Worth checking specifically — they are used adjacently
  in `foundations.md`.)*
