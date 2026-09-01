---
type: Reference
title: Sources — what we cite, and how far to trust it
description: "The provenance layer. Every external claim Void Maiz leans on gets a source rune: what it is, what we use it FOR, why it is credible, and how confident we are that the attribution itself is correct. Citation is by ordinary body link, so a source's blast radius is its linked-from list and needs no new machinery. Includes the verification-pass protocol for catching a wrong attribution later."
tags: [status:draft, audience:all, confidence:asserted, reference]
timestamp: 2026-08-09T00:00:00Z
---

Opened 2026-08-07 at the author's direction, after a session in which a
mathematical framing was repeated confidently across four documents and turned
out to be **wrong about its own domain**
([foundations §1](/../../VoidAllomone/okf/concepts/foundations.md)).

> **Adopted upstream 2026-08-09.** Offered to Void Core as a local convention;
> they took both decisions into `/references/okf-spec.md`, applied it to their
> own bundle, and **it found two real problems there within the hour** — an
> unattributed borrowed term, and a normative contract shipped against a note
> that had explicitly said *"we should read the paper before committing"* and was
> never closed. So this is no longer ours: **`type: Source` is a spec type**, and
> citation-by-body-link is the documented convention.
>
> One ruling of theirs we adopted rather than invented: **`Source` is distinct
> from `Reference`.** *A `Reference` is material we wrote to be referred to; a
> `Source` is an external work we are trusting* — and only the second kind can be
> wrong in a way we would not notice. `Source` is also **not** exempt from the
> `resource:` honesty rule, so a source with no locator is flagged, which is
> correct: a citation that cites nothing is worth catching.
>
> *(One detail checked rather than assumed: ours are **not** flagged today,
> because the honesty rule fires on `status:current` and every source here is
> `status:draft`. That turns out to be the right lifecycle — see *Locators*.)*

# The problem this folder addresses

An agent writing documentation produces *plausible* prose. A surname, a year and
a theorem name are exactly the kind of thing that comes out fluent and can be
wrong — and once written, a wrong attribution is **invisible**: it reads
correctly, it is never re-derived, and nothing checks it.

The author's framing: *"maybe you hallucinate later and say 'Albert Einstein was
the creator of the Pythagorean theorem', well then hopefully someday later a
verification scan of the sources will catch that."*

# Two different failure modes, two different tools

Worth separating, because conflating them wastes effort on the wrong one:

| failure | example | the tool |
|---|---|---|
| **Wrong attribution** | naming the wrong author, year, or result | **a source rune** — recorded once, checkable later |
| **Wrong reasoning** | a domain error that makes a true theorem inapplicable | **a counterexample** — checked in place |

The 2026-08-07 semilattice error was the *second* kind, and **no citation would
have caught it**: the axioms were quoted correctly and applied to the wrong
domain. A three-line counterexample caught it in a minute.

So the rule this folder adopts is narrower than "cite everything":

> **Cite what you cannot check locally. Check what you can.**

Attribution, empirical results and other people's theorems get a source. Anything
that can be settled by constructing a case — associativity, a merge outcome, a
parse — gets constructed instead. A citation on a locally-checkable claim is
ceremony: it costs tokens and buys nothing, because the citation would have been
correct and the claim still wrong.

# The convention: cite by linking

**A citation is an ordinary markdown link to a source rune**, in the body. No new
frontmatter key, no new machinery — and it earns two things for free:

- the OKF engine already extracts body links into the concept graph, so
  `okf get sources/<name>` lists **everything that cites it** under *linked
  from*. That list **is the blast radius**: if a source turns out to be wrong,
  it names exactly which pages inherit the error.
- `okf analyze` treats sources as nodes, so an over-leaned-on source shows up as
  a high-centrality one.

Frontmatter (`cites:`) was considered and rejected: the spec permits unknown keys
so it would have *worked*, but link extraction reads the body, so it would have
produced citations invisible to the graph — a provenance record that cannot
answer "what depends on this."

# Confidence means something specific here

The glossary vocabulary is reused as-is, with a reading particular to sources:

| tag | means, for a source |
|---|---|
| `confidence:exploratory` | we are unsure the attribution is right at all |
| `confidence:asserted` | written from a model's knowledge and *not* checked against the artifact |
| `confidence:verified` | someone opened the actual paper/page and confirmed author, year and claim |

**Every source in this folder starts at `asserted`.** None were checked against
the artifacts when written — they were recalled. Saying so is the entire point:
`asserted` is a standing invitation for the verification pass, and a folder that
called its own recollections `verified` would be worse than no folder.

# Locators: why most of these have no `resource:`

`Source` is not exempt from the honesty rule — but that rule fires on
`status:current`, and every source here is `status:draft`, so nothing is flagged
yet. **Only [OKLab](/sources/ottosson-oklab.md) has a locator.**

That is not a loophole; it is the lifecycle, and the two axes turn out to
compose:

    draft   + asserted + no locator     — recalled, never opened
       │
       │  someone opens the artifact
       ▼
    current + verified + resource:      — and the honesty rule now guards it

A source page whose attribution has never been checked **is** a draft, and it
acquires a locator by the same act that promotes it to `verified`. So the day one
goes `current` without a `resource:`, `validate` correctly objects — a finished
citation that cites nothing.

**Why not add locators sooner** is the folder's own premise. A URL or DOI
recalled rather than looked up is **exactly the artefact this exists to catch** —
plausible, well-formed, unverifiable, and then *trusted more* than the prose
around it because it looks primary. A DOI written from memory would be worse than
none: nothing missing is visibly missing, and a wrong one is visibly present and
wrong in the most authoritative-looking place on the page.

# The verification pass

Occasional, cheap, and it does not require reading Void Maiz at all:

1. `okf query "confidence:asserted source"` — the work queue, and a real query
   now that `Source` is a type.
2. For each: check **author, year, and the specific claim we attribute**. The
   third is the one that matters — a real paper by the right author can still not
   say what we said it says.
3. Promote to `confidence:verified` **and `status:current`, and add the
   `resource:` locator you just used** — or correct the page.
4. **If a source was wrong, walk its `linked from` list.** Every page there
   inherited the error, and the fix is not done until they are checked.

Step 4 is why the linking convention was chosen over a bibliography. A
bibliography tells you what was cited; a graph tells you what breaks.

# What a source rune contains

Short on purpose — a source is an index card, not a summary of the work:

- **what it is** — author, year, venue, and the result in one sentence;
- **what we use it for** — the specific claim in *our* design it backs, linked;
- **why it is credible** — venue, standing, or the fact that it is a primary
  source for its own terminology;
- **what a verification pass should check** — the precise attribution to confirm,
  because "check this is right" is not actionable and "confirm Lafont 1990
  introduced interaction nets at POPL" is.

# What is NOT a source

- Our own design decisions. Those are concepts, and inventing a citation for them
  would be worse than leaving them unsourced.
- Void Core, Void Palabra and Void Hormiga documents. Those are siblings and get
  ordinary links; a source rune is for things **outside** the Void projects.
- Anything used only as a passing analogy. If removing the name would not change
  a design decision, it does not need provenance.

# Status

Seeded 2026-08-07 with the sources that back **load-bearing** claims — the ones
where a wrong attribution would mean a design decision rested on something that
does not exist. Terms used in passing (Rete, Mylyn, MPS, differential dataflow,
H3/Munzner, HodgeRank, Fiedler) are named in
[vocabulary](/../../VoidAllomone/okf/concepts/vocabulary.md) and are not yet source runes; the
spec's *"tolerate broken links — not-yet-written knowledge"* rule covers the gap
honestly.
