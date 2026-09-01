---
type: Source
title: Ottosson — OKLab
description: "A perceptual colour space designed so that Euclidean distance and linear interpolation behave the way an eye expects. What the demo's colour blend interpolates in, and the reason the blend lives in the host rather than the library."
resource: https://bottosson.github.io/posts/oklab/
tags: [status:draft, audience:host, confidence:asserted, source]
timestamp: 2026-08-07T00:00:00Z
---

Part of [sources](/sources/index.md).

# What it is

**Björn Ottosson, "A perceptual color space for image processing" (2020)**,
published as a blog post with the transform matrices given in full. **OKLab** is
a Lab-style space tuned so that lightness, chroma and hue behave predictably —
in particular, **interpolating between two colours does not pass through the
muddy or oversaturated regions** that sRGB and CIELAB interpolation are known
for.

# What we use it for

The demo's `Lattice::Custom` join for `color`
([composition](/../../VoidAllomone/okf/concepts/composition.md)): convert sRGB → linear →
OKLab, average, convert back. It is what makes *"red and blue make purple"*
produce a colour a person would call purple.

**It is also the argument for where the seam is.** A blend belongs to the host
and not the library precisely because it requires knowing what a colour *is* —
a transfer function, a matrix, a perceptual model. The library carries `color`
as an opaque string, so putting OKLab inside it would break the quarantine that
lets the same merge serve `temperature` or `priority`
([host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md)).

# Why it is credible

Not peer-reviewed — it is a blog post — and that is worth stating plainly rather
than dressing up. Its credibility is **adoption and reproducibility**: the
matrices are published in full, the derivation is shown, and it has been taken up
widely (CSS Color 4 specifies `oklab()`/`oklch()`, and several colour libraries
implement it). For our purpose the standard is *"does interpolation look right"*,
which is checkable directly rather than by authority.

# What a verification pass should check

- **Ottosson**, **2020**, and that OKLab is his.
- That our transform in `allomone_playground.cpp` matches the published matrices
  — this is checkable **against the source numerically**, which is a stronger
  verification than any citation.
- That **CSS Color 4 does specify `oklab()`** as claimed above. *(An adoption
  claim, and the easiest sentence here to have overstated.)*
