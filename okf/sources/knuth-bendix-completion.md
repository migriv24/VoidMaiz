---
type: Source
title: Knuth & Bendix — completion, and its divergence
description: "The 1970 completion procedure for term-rewriting systems, and the fact that it may not terminate. This is the single most load-bearing external result in Void Maiz: the propose-don't-generate ruling rests on it entirely."
tags: [status:draft, audience:kernel, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Donald Knuth and Peter Bendix, "Simple word problems in universal algebras"
(1970)**, in *Computational Problems in Abstract Algebra*.

The completion procedure takes a set of rewrite rules, finds **critical pairs**
(overlaps where two rules could disagree), and **adds new rules** to resolve
them, aiming at a confluent system.

**The property we depend on: completion may not terminate.** It halts with a
finite confluent system, fails, or *runs forever producing more and more rules*.
Divergence is the generic behaviour rather than an implementation defect, and
there is a literature on partially escaping it.

# What we use it for

**The propose-don't-generate ruling**
([materialization](/../../VoidAllomone/okf/concepts/materialization.md)), and with it write
tier 3 being refused rather than deferred
([start-here §8](/../../VoidAllomone/okf/concepts/start-here.md)).

The author's own intuition was the finding: *"what would it mean if a script made
another script? I feel like Allomone would respond with a script that also makes
another script, and so then there would be a ripple effect."* A system that
inspects its rules and generates rules to restore symmetry **is** a completion
procedure, so the ripple is the expected outcome, not the unlucky one.

It also strengthens rather than weakens with a goal attached
([horizons](/horizons.md)): hill-climbing toward a score by generating rules is
completion with a heuristic.

# Why it is credible

A foundational, heavily-cited result; completion is standard material in
term-rewriting texts (Baader & Nipkow, *Term Rewriting and All That*, is the
usual modern reference). The non-termination property is not obscure — it is the
first caveat any treatment states.

# What a verification pass should check

- Knuth and Bendix, **1970**, and the paper title above.
- That the procedure is correctly described as **generating new rules from
  critical pairs**, not merely orienting existing ones.
- That **non-termination is a property of the procedure in general**, not a
  consequence of a particular ordering choice. *(This is the specific claim our
  ruling rests on; if it is weaker than stated, the ruling needs re-argument
  rather than repair.)*
