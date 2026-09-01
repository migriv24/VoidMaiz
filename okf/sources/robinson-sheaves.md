---
type: Source
title: Robinson — sheaves, and the consistency radius
description: "Applied sheaf theory over data: sections agreeing on overlaps, and consistency radius as a metric of how far apart they are. Cited here mainly for a framing we REJECTED — and for the one narrow place it might still pay."
tags: [status:draft, audience:kernel, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Michael Robinson**, applied topology / sheaf theory over data (*Topological
Signal Processing*, 2014, and subsequent work on sheaves for sensor
integration). A **sheaf** assigns data to open sets with restriction maps;
**global sections** are local pieces that agree on overlaps; obstructions to
gluing live in **cohomology**. Robinson's **consistency radius** relaxes exact
agreement into a *distance*: how far from consistent a set of local sections is.

# What we use it for

**Mostly as a rejection, recorded honestly**
([foundations §7](/../../VoidAllomone/okf/concepts/foundations.md),
[vocabulary](/../../VoidAllomone/okf/concepts/vocabulary.md)). The sheaf framing was evaluated
for the merge and **not adopted**, on two independent grounds:

1. cohomology in the usual sense needs abelian-group-valued data, and our values
   are opaque strings with no group structure — no H¹ to compute;
2. our composition is associative, so the obstruction that cohomology would
   measure vanishes by construction.

Void Palabra's `academic-foundations.md` §9 supplied a second, better-argued
version of the same rejection, which strengthened ours rather than duplicating
it.

**Where it might still pay**: the **consistency radius** specifically, for
properties whose values are *metric* (numbers, positions, perceptual colour). It
would turn "how far apart are these sources?" into a real number and let a host
threshold it — a slider from *only hard conflicts* to *every disagreement*. That
is on the roadmap, gated behind metric value types that do not exist.

# Why it is credible

Robinson is a recognized figure in applied topology; the work is published and
the sheaf-theoretic machinery itself is standard mathematics. **Credibility of
the source was never the issue** — the question was *applicability*, and it is
worth being clear that we rejected a fit, not a body of work.

# What a verification pass should check

- That **consistency radius** is Robinson's term with the meaning we give it.
- That claim (1) is stated accurately: sheaf **cohomology** generally requires
  abelian-group-valued sections, so a set-valued or string-valued sheaf has
  sections and obstructions in a weaker sense but not the cohomology we would
  need. *(This is the load-bearing half of the rejection and the most likely
  place for our summary to be too crude.)*
- That we have not mischaracterized the framing as useless when what we mean is
  *not useful for this*.
