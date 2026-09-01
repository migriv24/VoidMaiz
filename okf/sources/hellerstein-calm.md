---
type: Source
title: Hellerstein — CALM, and monotonicity as the coordination test
description: "Consistency As Logical Monotonicity: a program has a coordination-free distributed implementation iff it is monotone. The reason derive-only is a guarantee rather than a preference."
tags: [status:draft, audience:kernel, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Joseph M. Hellerstein, "The Declarative Imperative" (PODS 2010 keynote /
SIGMOD Record)** — the **CALM** conjecture: *Consistency As Logical
Monotonicity*. A program has a coordination-free, eventually-consistent
distributed implementation **if and only if** it is monotone. Non-monotone
constructs (negation over a whole set, aggregation, deletion) are exactly the
points that require coordination.

It was later **proved**, in the Datalog setting, by Ameloot, Neven & Van den
Bussche. *(That attribution is one to check — see below.)*

# What we use it for

[foundations §8](/../../VoidAllomone/okf/concepts/foundations.md), and it is the reason
derive-only is described as buying something rather than merely costing
something.

Allomone's evaluation is monotone: adding a rule adds annotations, never retracts
them, so composition needs no coordination. Concretely this is what keeps
**parallel evaluation, background derivation and collaborative editing**
available without a locking story — and it is the specific property that
materialization would put at risk, which is why the write tiers are gated
([materialization](/../../VoidAllomone/okf/concepts/materialization.md)).

# Why it is credible

A PODS keynote by a database researcher of standing; CALM is widely cited and has
a formal proof in the literature, which is a stronger position than a conjecture.

# What a verification pass should check

- **Hellerstein**, PODS **2010**, and that *"Consistency As Logical
  Monotonicity"* is the correct expansion of CALM.
- **The proof attribution** — Ameloot, Neven & Van den Bussche, and the setting
  it holds in. *(Flagged deliberately: "later proved" is exactly the kind of
  clause that is easy to state confidently and get wrong, and the strength of
  our claim depends on whether it is a theorem or still a conjecture.)*
- Whether the monotonicity here is the same notion as the closure-operator
  monotonicity from [Saraswat](/sources/saraswat-ccp.md), or two that we have
  been treating as one.
