---
type: Source
title: Lafont — interaction nets and interaction combinators
description: "Graph rewriting with principal ports, active pairs and local interaction rules; and the combinators result that the system is Turing-complete. The substrate Void Core reduces, and the reading that makes a merge law an interaction rule."
tags: [status:draft, audience:kernel, audience:host, confidence:asserted, source]
timestamp: 2026-08-09T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Yves Lafont, "Interaction Nets" (POPL 1990)** and **"Interaction Combinators"
(*Information and Computation*, 1997)**.

Interaction nets are a graph-rewriting formalism: each cell has one **principal
port** and some auxiliary ports; two cells joined principal-to-principal form an
**active pair**; a rewrite rule replaces that pair locally. Reduction is
**strongly confluent** — the normal form does not depend on which redex you pick.

The combinators paper shows a **fixed set of three cells is universal**, which
also makes the formalism Turing-complete.

# What we use it for

Two different things, and they should not be conflated:

- **The substrate.** Void Core's model is interaction-net shaped; Void Maiz
  renders it, and `voidmaiz_reduce` consumes the reduce conformance contract.
  The Turing-completeness is why Void Core's termination guard exists, which our
  [emergence](/../../VoidAllomone/okf/concepts/emergence.md) argument inherits.
- **A reading of the merge** ([values §5](/../../VoidAllomone/okf/concepts/values.md)): a
  `(subject, property)` cell is a port, two annotations meeting there are an
  active pair, and the property's merge law is the interaction rule. The
  author's framing, and it is genuinely illuminating — **but it is an analogy,
  and the page says so.** Allomone derives over the net; it does not rewrite it.

The confluence property is the load-bearing half: it is why a `JoinFn` receives
the whole set at once rather than being folded pairwise.

# The γγ ≠ δδ asymmetry — the claim most worth checking, and it is ours

Void Core's 2026-08-09 reply raised this while writing their own Lafont source
page, and it concerns us directly because **we are where the claim came from.**

Their finding: the γγ-swapped / δδ-straight asymmetry in the reduce contract
*"did not come from Lafont at all — it came from your field report, and we
adopted it on the strength of the argument rather than the citation."* It is now
a **normative contract other projects implement against**, and they rank it #1
to verify.

**Two refinements from our own record**, which sharpen the target rather than
dispute it:

- **The date is 2026-07-14**, not 07-13 — [log](/log.md), *"Two mathematics bugs
  from hands-on use: γγ≠δδ, and self-wired redexes."*
- **It was not unattributed — it was attributed to Lafont, by us, from
  memory.** The log records the fix as *"`swap: true` annihilation flavor
  (x_i ≡ y_{n+1−i}) — **Lafont's γγ**, the parallel-arcs drawing between
  mirrored bodies."* The author's diagnosis was explicitly *"that's δδ"*, i.e. a
  correction **toward** Lafont, not an independent invention.

That distinction matters for what a verification pass should do. The claim is
not *"someone made this up"*; it is `confidence:asserted` in the precise sense
this folder defines — **stated from recall, never checked against the paper.**
So the check is well-posed and narrow:

> Does Lafont's γγ annihilation connect `x_i ≡ y_{n+1−i}` (swapped), and δδ
> connect `x_i ≡ y_i` (straight)? Or is it the other way round?

If it is the other way round, the reduce contract's cases 11–14 encode a
mistake, every conforming implementation inherits it, and
[interaction-connections](/concepts/interaction-connections.md) is wrong where it
says the asymmetry *"buys universality."* That is the whole reason this is #1.

**Also worth noting for honesty:** our own page has always attributed the finding
to our hands-on use rather than to the paper, so the drift was in the sentence
that named Lafont as the *authority for the rule*, not in the provenance of the
finding. A wrong attribution and an unrecorded one fail differently, and this was
the first kind.

# Why it is credible

POPL and *Information and Computation* are both top-tier; interaction
nets/combinators are established formalisms with a substantial literature
(optimal reduction, Lamping/Asperti). Lafont is the primary source.

**Credibility of the work is not the question here.** The question is whether
*our recollection of one specific rule* matches it — which is exactly the failure
mode a well-regarded source does nothing to protect against, and is worth
stating because "Lafont is authoritative" is the sentence that stopped anyone
checking.

# What a verification pass should check

**#1 — the γγ/δδ orientation.** Does γγ annihilation connect `x_i ≡ y_{n+1−i}`
(swapped) and δδ connect `x_i ≡ y_i` (straight)? Everything else on this list is
housekeeping by comparison; this one is normative for other projects. See the
section above.

Then:

- **POPL 1990** for interaction nets; **1997, *Information and Computation***
  for interaction combinators.
- That **three** is the correct number of universal combinators.
- That **strong confluence** is the right property name for "the normal form is
  independent of reduction order" in this setting.
- That we have not overstated the analogy: our merge is a *reduction over a set*,
  not net rewriting, and the distinction is stated wherever the analogy is used.
