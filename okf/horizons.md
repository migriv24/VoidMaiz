---
type: Horizons
title: Horizons
description: "The long-range destinations Void Maiz must not foreclose: the library family (Void Maiz / XR / NE), time as the fourth axis, and Latin-OS. Each entry records what it is and what it demands of Void Maiz TODAY."
tags: [status:planned, audience:all, confidence:asserted]
timestamp: 2026-08-07T00:00:00Z
---

Complementary to the [backlog](/backlog.md) (features we will build) and
[substrates](/concepts/substrates.md) (the abstraction axes): this file
records **what Void Maiz will eventually be used for**, so that no near-term
decision quietly forecloses a horizon. Entries move out of here when they
become projects with their own OKFs.

# The library family (decided 2026-07-13)

One substrate-free foundation (`voidmaiz` + `voidmaiz_reduce` + the OKF
conventions as normative spec), consumed by sibling view libraries with
explicit, HARD charters:

- **Void Maiz** — *2D in nature*. Rectangular display, 2D euclidean
  interaction space, pointer or touch. It may freely use **3D graphics** —
  depth, perspective, nodes as 3D-looking buttons — the way modern 2D Mario
  games are rendered in 3D while remaining 2D games: **the model space is a
  plane; the pixels may pretend otherwise.** Rendering dimension ≠
  interaction dimension.
- **Void Maiz XR** — visualizing Void Core in **three interaction
  dimensions**: OpenXR, solids (cone = 3D triangle), surface port anchors,
  quaternion auto-orientation, spatial gestures, the haptic feedback sink.
  Own repo when work starts. (Decided — was Q#12.)
- **Void Maiz NE** — **non-euclidean spaces** (hyperbolic focus+context for
  large graphs; H3/Munzner is the map). Deliberately back-burnered: no
  client needs it soon; recorded so its possibility shapes nothing today
  except the rule that camera/metric live in the view layer.

# Time — the fourth axis (vocabulary, not yet design)

The author's intuition ("nodes evolve over time and are NOT static") has
names: **temporal graphs** / **dynamic network visualization**, and for our
foundation specifically, a **graph rewriting trace** — a reduction IS a time
series of nets. The load-bearing observation: **Void Core's log already IS
the time axis.** A session is a timestamped sequence of graph states;
undo/redo is scrubbing; the InteractionCombinators demo already walks
reductions backwards. What "dynamic nodes" decomposes into, when a use case
arrives:

1. **History visualization** — timeline views, onion-skinning, trace
   playback (already sketched in the FaultSack considerations and "later
   views"). Pure view work over the existing log.
2. **Live-evolving nodes** — agents/holidays dispatching commands over time;
   nodes "evolve" because something speaks the dispatcher language. Already
   possible; no new machinery.
3. **Motion** — the animation/tween layer (backlog), which is presentation,
   not model time.

No design now — no use case yet, by the author's own read. The interaction
-net foundation is well suited when one appears (rewrites are discrete time
steps by construction).

# Latin-OS (far horizon; its own project someday)

A Void Core-based operating system — named for Latin as the root language of
many others, as Void Core is — with Void Maiz as the primary graphics engine
and **windows/panels as nodes**: everything on screen a node, applications
interacting through port mappings.

Placing it in the display-stack landscape (for orientation): Xorg/Wayland
are **display servers/compositors** (they own pixels and route input);
GTK/Qt are **toolkits** (widgets inside a window). What Latin-OS describes
is both at once: a **compositor whose scene graph is a Void Core mantle**
(windows are runes; focus, stacking, and inter-app connections are
dispatched commands) with Void Maiz as the toolkit above it. The likely
shape is a hybrid of Void Maiz and Void Maiz XR — flat panels composed in a
spatial scene, in the spirit of
[SimulaVR](https://github.com/SimulaVR/Simula) (a VR window manager, running
on [monado](https://monado.dev), the open OpenXR runtime) — proof that small
teams build unconventional compositors.

**Security is the declared primary concern** (no application sandboxing;
everything interconnectable). One honest note for that future design: the
dispatcher is a natural **security choke point** — every inter-app
interaction is a logged, attributed command (the actor convention), so
auditability exists *by construction*, and capability scoping could ride
tags/actors. Total observability is a security primitive, not just a UX one.
The rest is a whole other project's OKF.

# UI/UX authoring on Void Maiz — Allmusely & Kingsterson (brainstorm 2026-07-18)

The [widget registry](/concepts/widget-registry.md) already names a
"Qt-Designer / Figma on Void Maiz" as a **host application** built on the
registry, never library. This is that horizon, given its shape and its names.

**The motivating "why" (the author's, and it sets the whole direction):**
LLM-generated GUIs are sometimes weak, and there is no interface to tweak
them. A design tool whose file IS a Void Core state document dissolves that —
an agent can generate a UI and a human can edit *the same artifact* through
the same dispatcher seam, because design becomes just another observable,
log-first, replayable mantle. That requirement is the load-bearing one, and
it decides everything below.

- **The path NOT taken — VMF (a Figma/Qt-Designer wrapper).** Considered and
  set aside. A design exported from another tool is a **dead snapshot in that
  tool's model**; the moment you annotate it, add a tag system, or an agent
  edits it, you've forked from the source and the round-trip breaks. It
  cannot be log-first, and the tags/interactions the author actually wants to
  explore don't exist in Figma's model. Parsing `.fig`/REST JSON is easy; the
  blocker is that it isn't a Void Core state document and never will be. Keep
  Figma at most as an import-once on-ramp, never the foundation.
- **Allmusely** (pron. "all muse-ly", named for a UI/UX professor) — **our
  own authoring tool, the lean.** Scoped deliberately as a Void Maiz
  *authoring surface*, **not a Figma-class graphics editor** (that way lies a
  multi-year vector/constraints/font-shaping project). What you place are
  **real widget-registry components and node-graph layouts, not rectangles**;
  the file you save is a Void Core mantle; the "AI generates it, human
  tweaks it" loop is free because both sides emit dispatcher commands against
  one state doc. The commitment that keeps it honest: real widgets from the
  start, never pretty placeholders reconciled later.
- **Kingsterson** (named for a VR professor) — **the XR authoring tool**, the
  same idea in [Void Maiz XR](#the-library-family-decided-2026-07-13)'s
  modality: spatial 3D layouts as part of the design, motion gestures
  first-class. Genuinely heavier — it needs the XR interaction substrate
  under it — so it sequences *after* Allmusely proves the pattern, not
  alongside.

**The FaultSack boundary** (the author's own line, kept verbatim):
FaultSack is **inward / analytic** — study and refine the core concepts of
apps that already exist. Allmusely is **forward / generative** — author UI
that doesn't exist yet. The overlap is small ("annotations over a canvas"),
so the shared substrate is at most an annotation/tag library both consume;
the apps stay separate. This is why FaultSack is (and remains) a
[non-client](/concepts/scope-and-clients.md) of Void Maiz.

**Relation to Latin-OS:** its default applications would be authored in
Allmusely (flat) and Kingsterson (spatial) — the authoring tools produce the
mantles the OS composes. Allmusely / Kingsterson become their own projects
with their own OKFs when they start; **nothing is being built now** (ground
rule 1). Captured here only so the direction is not lost, and so no near-term
Void Maiz decision forecloses "the design tool is itself a Void Maiz app."

## Allmusely as a tool with a MOTIVATION (author's thought, 2026-08-07)

> *"Allomone is essentially a UI reactive program… perhaps Allmusely will be an
> application dedicated towards the creation of a GUI based on rules and
> interactions with a specific 'motivation' or 'goal' in mind. That might
> currently be outside the scope of our thinking, but I do feel it's worth
> considering early on."*

**It is not outside the scope — it is the predicted client of a seam we already
designed and deliberately left unbuilt.**
[Governance](/../../VoidAllomone/okf/concepts/governance.md) rejected the Sentinel memo's
Harmony Score **as a library concept** ("Allomone does not know what a GUI is,
and must not"), and in the same breath re-homed it: *"Where a visual Φ belongs:
the host… a host-supplied `HarmonyFn(const Merged&) -> double`… not needed until
a host asks."* **Allmusely is the host that asks.** An application whose entire
subject matter is GUIs is exactly where "what makes a good GUI" is legitimate
domain knowledge rather than contamination.

### A goal is a different mathematical object from a lattice

This is the part worth settling early, because it decides the layering.

- The merge is a **join semilattice**: a partial order with least upper bounds.
  Where two opinions are *incomparable* it produces ⊤ — and refusing to rank
  incomparable things is not a limitation, it **is** what ⊤ is.
- A goal is a **preference order / objective function**: it ranks. That is a
  strictly stronger structure, and you cannot extract one from a lattice.

So Allmusely cannot be "Allomone with a goal knob." Adding an objective to the
merge would destroy ⊤ by construction — everything would become comparable, and
the one behaviour the whole design exists to produce would vanish. The rule that
follows is short and worth adopting now:

> **Taste belongs in the RANKING OF PROPOSALS, never in the merge.**

The [Weaver](/../../VoidAllomone/okf/concepts/governance.md) already has exactly that shape:
compute the asymmetries, offer rules, let a human accept. A goal adds an opinion
about *which proposals to show first* — and touches nothing else.

### A goal makes propose-don't-generate MORE load-bearing, not less

The tempting move, once a score exists, is to hill-climb: generate rules, keep
the ones that raise Φ. **That is Knuth–Bendix completion with a heuristic
attached, and completion diverges** — the exact result
[materialization](/../../VoidAllomone/okf/concepts/materialization.md) already refuses. A goal
makes generation both more tempting and more dangerous, so the ruling gets
*stronger* here rather than relaxing.

### Where a goal would come from — and the mechanism is already filed

Two sources, and the second is the interesting one:

- **Declared** by the designer — "this should be calm", "this should be dense".
  Straightforward, and just a `HarmonyFn` with parameters.
- **Learned from resolutions.** Every time a human settles a ⊤ they record a
  preference between two sources, and **resolutions are already model content** —
  logged, attributed, replayable. A set of resolutions is a pairwise comparison
  graph, and **HodgeRank** decomposes it into a consistent global priority plus
  the inconsistency ([vocabulary](/../../VoidAllomone/okf/concepts/vocabulary.md)).

  That is *learning the designer's taste from what they keep choosing*, using a
  mechanism already identified and filed on the
  [roadmap](/../../VoidAllomone/okf/concepts/roadmap.md) at low priority — low **because
  nothing today needs it**. Allmusely is what would promote it, and this note
  exists so nobody retires it as speculative first.

### What Allmusely would demand that we do not have

Three forcing functions, named now so they are not surprises:

1. **Stable rule identity** ([discovery §1](/../../VoidAllomone/okf/concepts/discovery.md)
   phases 2–3). A tool reasoning about *which rule to change* needs to name one
   across edits. `Annotation::origin` is display-only **on purpose** and cannot
   carry this. Allmusely is the client that forces the identity problem phase 1
   was designed to dodge.
2. **A host-extensible Weaver.** The three proposal kinds (`settle`, `unheard`,
   `gap`) are hardcoded in `weaver.cpp`. A design tool wants its own — *"these
   two buttons have different padding; unify?"* — which is the same registry
   shape as `PredicateRegistry`. Cheap to note, not to build.
3. **Relations as annotation targets.** Allomone annotates `(subject,
   property)`. Spacing, alignment and hierarchy are **between** subjects, not
   properties of one. A host can encode the pair as a subject (`subject` is an
   opaque host string, so `"a|b"` is legal), but the awkwardness is real and
   should be recognized as a *structural* question rather than discovered as a
   workaround.

### The thing that must NOT happen

**Do not design Allomone for Allmusely.** The engine is good precisely because
it does not know what a GUI is — that is what lets the same merge serve
`temperature`, `priority` and a contact list. The moment a goal primitive enters
the kernel to serve a hypothetical design tool, that generality is gone and it
does not come back.

**Net effect on Void Maiz today: nothing changes.** No current decision
forecloses any of this, the one seam it needs is already specified, and the two
mechanisms it would want (`HarmonyFn`, HodgeRank) are both recorded as
unbuilt-because-unneeded rather than unconsidered. Which is the answer this file
exists to give.

# What the horizons demand of Void Maiz TODAY

The actual "keep in mind" list — cheap disciplines, all already in force or
now adopted:

1. **Conventions are a spec, not an implementation detail** — hints, wire
   kinds, gesture→command grammar, staging discipline get written as if a
   second implementation will read them (XR, NE, web all will).
2. **View modules are replaceable wholesale**; nothing above them acquires
   dimension-, metric-, or device-specific types. Camera and hit-testing are
   view property, never Scene property.
3. **No global/singleton state in the library** — Latin-OS means many
   cores, many views, one process; `maiz::Core` instances and EditorStates
   already compose this way. Keep it true.
4. **Attribution everywhere** (`kind:name` actors) — multi-app futures make
   "who did this" load-bearing, not cosmetic.
5. **Performance headroom is a feature** — the 10⁴-rune diff seam
   (flagged upstream) matters to an OS-scale scene graph long before it
   matters to a patch editor.
6. **The log is the time axis** — never design a feature that couldn't be
   replayed.
