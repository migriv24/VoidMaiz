---
type: Source
title: Mimram & Di Giusto — a categorical theory of patches
description: "Merge is partial because the category of files and patches lacks pushouts; completing it freely ADDS objects, and those objects are the conflicts. The categorical twin of our lattice-theoretic argument that conflicts must be values."
tags: [status:draft, audience:kernel, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Samuel Mimram & Cinzia Di Giusto, "A Categorical Theory of Patches" (2013)**.

Model files as objects and patches as morphisms. Merging two divergent patches is
a **pushout** — and the category does not have all of them, which is exactly why
merge is a partial operation in every version-control system. Taking the **free
finite cocompletion** adds the missing objects, and the added objects are
interpretable as **conflicts**.

# What we use it for

[multi-user](/../../VoidAllomone/okf/concepts/multi-user.md): the observation that Void
Palabra's `join` and Allomone's `merge` reached *conflict-as-object* by two
independent routes — theirs categorical, ours lattice-theoretic
(`{blue} ∪ {red}` has two elements, so the merge stays total;
[foundations §1–2](/../../VoidAllomone/okf/concepts/foundations.md)).

The agreement is the point, and it is a genuine argument rather than a
coincidence worth noting: **"conflicts are errors" is not a simpler design, it is
a broken one** — in the categorical reading the operation is not total without
them, in ours the lattice does not close. Two formalisms refusing the same
shortcut is stronger evidence than either alone.

# Why it is credible

A published paper in an established line of work on version control semantics
(alongside Darcs' patch theory and Angiuli/Morrison/Warren's homotopical
treatment). Cited here **via Void Palabra's** `academic-foundations.md`, which is
where we encountered it.

# What a verification pass should check

- **Mimram & Di Giusto, 2013**, and the title.
- That **pushout** is the correct categorical operation for merge, and that the
  category genuinely **lacks** them in general.
- That **free finite cocompletion** is the right construction, and that the added
  objects are interpreted as conflicts **by the authors** rather than by us.
  *(This is a second-hand citation — we read Palabra's summary, not the paper.
  It is the most likely place in this folder for a detail to have been
  transposed, and it should be checked against the paper directly rather than
  against Palabra's page.)*
