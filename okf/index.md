---
type: Index
title: Void Maiz
description: The front door — what Void Maiz is, the founding commitments, and where every other document lives.
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-10T00:00:00Z
---

# Void Maiz

**The node-graph UI library for Void Core applications.** C++20, founded
2026-07-09 by the author's call at the end of Void Loops Studio's Python era:
*"we might want to start a SEPARATE project. a node UI library, but that can be
more generalizable for other types of applications… completely compatible with the
void core (where the CLI is expanded to the point where EVERYTHING is logged)."*

One sentence: **Void Maiz draws a Void Core mantle as a node graph and turns every
gesture into a dispatcher command.**

> **Name.** Founded as *Void Node*; renamed **Void Maiz** (*maíz* — corn) on
> 2026-07-13 to shed the generic "node" label, matching Void Core's
> abstract-root spirit. The family: **Void Maiz** (2D), **Void Maiz XR** (3D
> spatial), **Void Maiz NE** (non-euclidean). Historical docs — the [log](/log.md),
> the archived draft, the inter-agent `MESSAGE_*` files — keep the old name by
> design (they are dated records). The physical repo folder was renamed
> `VoidNode/` → `VoidMaiz/` on 2026-07-13.

# The founding commitments

1. **The model lives in Void Core; Void Maiz owns no truth.** The library holds
   caches and projections, never the source of anything — including view state
   like node positions, which are model content (proven in VLS, 2026-07-09).
   See [log-first inheritance](/concepts/log-first-inheritance.md).
2. **Every gesture is a visible, replayable command.** Wiring, moving, knob
   tweaks — all of it flows through `vc_dispatch` and appears in the log/CLI.
   Humans, scripts, and AI agents share ONE interaction surface.
   See [total observability](/concepts/total-observability.md).
3. **Views are projections, and pluggable.** The 2D canvas is the first view, not
   the definition; the same mantle can render as a timeline, a tree, an
   instrument face. Rendering never holds truth, so views are swappable.
   See [views as projections](/concepts/views-as-projections.md).
4. **General library, opinionated boundary.** Domain-agnostic like the core
   (glyphs carry the domain), but explicitly NOT for every Void Core app —
   see [scope & clients](/concepts/scope-and-clients.md).

# Why ours? (the differentiation question)

The author asked it directly: *"there are other node libraries out there, so why
ours?"* Answered in [differentiation](/concepts/differentiation.md) — the short
version: every existing node library **owns the graph** and makes you sync with
it; Void Maiz owns nothing, so observability, agent collaboration, undo, and
persistence are inherited from Void Core rather than bolted on.

# Map

- [Log-first inheritance](/concepts/log-first-inheritance.md) — what the original
  draft wanted to build vs what Void Core already IS; the honest mapping.
- [Total observability](/concepts/total-observability.md) — commitment 2, made
  precise (which gestures are model commands vs view commands).
- [Views as projections](/concepts/views-as-projections.md) — the renderer
  architecture; Dear ImGui as first backend; node faces & widgets.
- [Interaction connections](/concepts/interaction-connections.md) — linguine vs
  fettuccine wires, the port model, rewrite rules, the port substitution
  morphism; the Rule Workshop non-goal.
- [Widget registry](/concepts/widget-registry.md) — node widgets vs host
  widgets; the protocol (CLI representation, tags, undo, responsiveness hooks).
- [Canvas actions](/concepts/canvas-actions.md) — a custom view's interaction
  vocabulary as named, param-schema'd, introspectable actions; one compile
  serving a gesture and a CLI verb (draft; Void Hormiga's Territory map).
- [Surface census](/concepts/surface-census.md) — the GUI documents itself:
  harvest panes, glyph vocabulary, actions, widgets and OBSERVED command flow
  into concept runes, and let Void Core's OKF engine produce the markdown (no
  new writer, no rewrite of shipping apps) — draft.
- **Allomone — NOW ITS OWN REPOSITORY** (`../VoidAllomone`, extracted
  2026-08-29 on the author's call). The base derivation language Void Maiz
  applications build on: independent sources state what should be true, a
  lattice merge composes them with no order, and irreconcilable cells surface as
  first-class conflicts. Its OKF moved with it — read
  `../VoidAllomone/okf/index.md`, where `concepts/start-here.md` is still the
  one page to hand a newcomer.

  **Void Maiz kept exactly two of its pages, because it kept the two things they
  describe:** [user graph](/concepts/allomone/user-graph.md) (the attention
  graph, whose data the `with`/`device` host predicates read) and
  [editor](/concepts/allomone/editor.md) (the ImGui script editor — a widget
  canvas disguised as a text editor). Both are Void Maiz's, not the language's.

  **What a Void Maiz host sees is unchanged**: `voidmaiz/allomone.hpp`,
  `annotate.hpp`, `weaver.hpp` and `sentinel.hpp` re-export everything into
  `maiz::`, so `maiz::Script`, `maiz::merge` and `maiz::Lattice` still resolve
  and Void Hormiga compiles with no edits. The one real change is that `device`
  and `with` are host predicates rather than kernel ones — call
  `maiz::register_user_graph_predicates(preds, graph)` and they behave exactly
  as before. See the [log](/log.md), 2026-08-29.
- [Platforms](/concepts/platforms.md) — what `void.json`'s `platforms` claims
  (a **shipping record**: a GUI binary ran there in front of a person) versus
  what CI measures (**compiles**, on all three desktops, headless suite green).
  Written because the ambiguity cost Void Hormiga a message. Carries the audit,
  the macOS GL-context defect and the Linux system-package prerequisite.
- [Rune kinds and quantities](/concepts/rune-kinds.md) — Void Core 0.2.14's
  entity/act/measure kinds, quantity annotations and the schema/presentation
  split, read as a node-graph library reads them: an **act rune is an
  interaction-net agent** (the mechanism we already draw, arriving under a
  modelling name); a **measure rune** is the one whose incoming edges carry
  values rather than strengths, and those are LABELLED, never thickened; a
  **quantity** is the first thing a glyph has said about a number that a widget
  can act on. Also what we declined — `kind:<k>` stays out of the filter bag.
- [Node geometry](/concepts/node-geometry.md) — shape is notation: glyph-
  declared bodies (triangle/circle/polygon), perimeter port anchors,
  auto-orientation toward the principal partner.
- [Node Blocks](/concepts/node-blocks.md) — the Scratch-like demo target (the
  active work): snap = link, hidden-wire adjacency rendering, connector-shape-
  as-type; the Blockly mapping, the block-as-net model, Phase A/B staging.
- **[Touch](/concepts/touch.md)** — what a finger changes and what it does not
  (opened 2026-09-13, ahead of Void Hormiga going mobile). The **deferred
  press**: tap, drag and long press are told apart BEFORE a pointer is
  delivered, because a shell that forwards the contact immediately has already
  committed to "drag" before it knows. The trick the page is really about — a
  **long press synthesizes a clean right-CLICK**, so every context menu the
  desktop canvas already has opens on glass with no canvas change at all.
  Thresholds in **millimetres, not pixels** (a pixel constant ships a different
  feel per device and calls it one library), and the **hit/draw asymmetry** that
  makes dense UI survive a finger. Carries the census of mobile-native chrome
  with what is built, and the handheld borrowings the author asked for — the
  3DS's two screens, the DS's no-hover discipline, stroke-to-cut-a-wire,
  shake-to-undo. Ends with what it does NOT prove: nobody has touched any of it
  with a finger.
- **[Networking](/concepts/networking.md)** (stages A and B built 2026-09-18, stage C 2026-09-19):
  networking for ANY Void Maiz application, ruled by the author (Q30). **Void
  Maiz owns what networking looks like, Palabra owns what networking is, and the
  application answers one question: may this rune leave this device?**
  Networking goes wrong one view at a time (the Map had no presence because each
  view decided for itself), so views **declare** what they show, in immediate
  mode, and **one renderer** marks it: outline, badge, tint, a privacy padlock.
  The sender decides what is broadcast, private runes are never named, presence
  is keyed on ids and never feeds the user graph (Q20), a conflict keeps a rune
  home, and the codec refuses rather than truncates. Allomone gets `present`,
  plus `share_by_annotation` so a rule can answer the one question. The **id
  trap** (id-keyed presence against name-keyed Subjects) is pinned by a test.
  **Stage C** (`voidmaiz_net`, the only target that links Palabra, built only
  when Palabra is present) runs real Cores through Palabra's sync session over an
  in-memory lossy mesh. It has no phantom loop, an idle peer never costs undo, and
  joiners must never create the shared mantle. A real network transport waits on
  the trust model.
- **[Collaborative canvas](/concepts/collaborative-canvas.md)** (2026-09-20,
  decided by the author the same day; foundations built, drawing not): several
  people and agents editing one node graph, with Interaction Combinators over LAN
  (desktop and Android) as the demo. **Rulings:** view state never asks (Palabra's
  read-time `Pick`), first to select holds it (claims, Lamport-ordered, binding
  agents too), and interaction nets are the strongest case (HVM2's
  wires-as-variables, sent to Palabra as research). Built: `claims.hpp`,
  `lan.hpp` (Android's multicast lock, the join code), and gestures in flight in
  presence. **Stage N0 built 2026-09-21**: IC stores connections as wire runes,
  collaborates (Host/Join), plays remote changes, and has a two-window **duo
  bench** (`interaction_combinators_duo`) the author can test on alone. The
  author's [user testing guide](/testing/collaborative-canvas-user-tests.md)
  names the decision each scenario tests. **Three
  channels**: committed (commands, synced), in flight (presence carries the
  staged half of every gesture: cursors, drag ghosts, pending wires, typing,
  claims) and local (camera). A tiered UI/UX catalogue. What each existing
  command does when two people race (moves CONFLICT, since fields are registers,
  not last-writer-wins). The interaction-net trap: two redexes sharing a wire
  commute in the maths but not as edits. Undo in a session. Wire routes as
  content. 33 numbered edge cases. Questions Q31–Q35.
- **[Updates](/concepts/updates.md)** (2026-09-21, on the author's call "all
  applications should be able to update themselves"): Void Mago owns what a release
  IS (`void-updates.json`, build-time only); Void Maiz owns the in-app client
  (consent, check, a prompt with behavior changes, download, digest check, apply
  as installer / archive beside / Android package); the app owns who it is. Never
  check unasked, never install untold. First host: Interaction Combinators 0.3.0.
- [Substrates & dimensions](/concepts/substrates.md) — what a visual
  representation may assume (not a rectangle, not 2D, not euclidean); the
  planned targets: mobile, VR/3D, block languages; the language-boundary
  policy.
- **[Headless](/concepts/headless.md)** — the same application with no
  front-end, so an agent in a real terminal can USE it (author's secondary
  objective, 2026-08-18). Not a contradiction of the GUI-first stance but its
  consequence: commitment 2 already said humans, scripts and agents share one
  interaction surface, and commitment 1 means a headless session and a GUI
  session are two front-ends over ONE state document — no sync, no import. Also
  records the two claims that driving the binary corrected (`undo` is
  session-scoped; `revert` against the persisted baseline is the real
  cross-process review affordance), the **one-way door** (effects are the only
  thing an agent does that `revert` cannot undo, so they are refused by default
  and granted per run), and the precedence rule — **a person outranks an
  agent**, and the evicted agent cannot write afterwards.
- [Headless adoption](/concepts/headless-adoption.md) — the five steps an
  existing Void Maiz application takes to gain a headless front-end (~40 lines,
  no port), what a GUI host MUST change, and the mistakes we can predict because
  we made them. The page the client agents consume.
- [Differentiation](/concepts/differentiation.md) — why this library needs to exist.
- [Scope & clients](/concepts/scope-and-clients.md) — who builds on it (VLS
  first), who doesn't (FaultSack, ESP32-class).
- [Roadmap](/roadmap.md) — the phase plan (concept → parity with the VLS
  prototype, piece by piece).
- [Backlog](/backlog.md) — the long feature list, tiered T1/T2/T3, including
  the Blender/LiteGraph feature matrix; knocked out one by one.
- [Horizons](/horizons.md) — the long-range destinations: the library family
  (Void Maiz "2D in nature" / XR / NE), time as the fourth axis, Latin-OS —
  and what each demands of Void Maiz today.
- [Developer questions](/developer_questions.md) — open decisions with leans.
- **[Sources](/sources/index.md)** — the provenance layer, opened 2026-08-07.
  One rune per external work we lean on: what it is, what *our* design uses it
  for, why it is credible, and what a verification pass should check. **Citation
  is by ordinary body link**, so a source's `linked from` list is its blast
  radius — if an attribution turns out to be wrong, the graph names exactly
  which pages inherit the error. Everything starts `confidence:asserted`
  (recalled, not checked against the artifact), which makes
  `okf query confidence:asserted` the work queue.
- [Log](/log.md) — the running history.
- [References](/references/voidnode-draft-v1.md) — the pre-Void-Core AI draft,
  archived with a critique.

# Inter-agent messages

**Titling convention (2026-07-21).** Every message to or from another agent is
a **uniquely titled** file in the recipient's repo root:
`MESSAGE_FOR_<RECIPIENT>_<sender>-<topic>-<YYYY-MM-DD>.md`, with a matching
self-identifying H1. **Never** a bare `MESSAGE_FOR_<RECIPIENT>.md` — a generic
name lets an agent mistake a stale message for the live one (several agents
relay concurrently). Unique titles let dated messages coexist as history
safely; **this index tracks open vs consumed** — retire a thread here, not by
deleting the file. (The old generic files were retired 2026-07-21 when the
convention landed.)

**Open: Void Maiz → Void Mago**, 2026-09-21:
`../VoidMago/MESSAGE_FOR_VOIDMAGO_maiz-updates-read-your-feed-now-and-two-asks-2026-09-21.md`
(every Void app now reads their feed through Void Maiz; asks for manifest-declared
artifact names, since IC ships a .zip and an .apk, and reports a `scan` column bug).

**Consumed 2026-09-21: Void Palabra → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_palabra-concurrent-structure-answered-2026-09-20.md` (in our
root). Every scenario has an answer, and they built it: `links.hpp` (equivalence,
capacity and acyclic rules over the partition lattice), **wires as runes plus `"="`
fusion as the normative encoding**, a Lamport `FieldJoin::Latest` (now `placement`'s
default), `writers()`, a stream envelope, and `Timing::coalesce`. **They declined
`voidpalabra_lan`**: the author leans toward Palabra *not* owning Android
networking, so the socket layer's home is **Q36**. Adopted the same day (`Latest` in
`presentational_joins()`, and `voidmaiz/wires.hpp`), with the evidence they asked
for sent back: **Open (no reply needed): Void Maiz → Void Palabra**, 2026-09-21,
`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-fusion-measured-through-the-whole-stack-2026-09-21.md`.

**Retired by their answer: Void Maiz → Void Palabra**, 2026-09-20:
`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-concurrent-rewrites-are-our-strongest-case-2026-09-20.md`
— the author asked Palabra to lead the research on concurrent rewrites. The
message covers every scenario with its answer, the shared-wire case as HVM2's
wires-as-variables mapped onto their existing registers, and the literature. Two
small seams follow: a Lamport-ordered `Latest` as the honest "last one wins", and
a read of who wrote a value. It also asks for the transport as a
`voidpalabra_lan` companion target, with everything Android taught us ("beacon
out, unicast back"; the multicast lock; per-interface binding; the join code). It
supersedes nothing: their 09-19 thread stays consumed.

**Received 2026-09-20, NOT YET ANSWERED (misfiled in Void Hormiga's root)**:
two from **Void Hormiga → Void Maiz**:
`../VoidHormiga/MESSAGE_FOR_VOIDMAIZ_hormiga-adopted-all-three-stages-2026-09-20.md`
(stages A–C adopted; the author's LAN-only yes for their sealed session; three
transport defects of their own; and a question: should the Roster be fed by the
host's UDP beacon, or is presence link-level only?) and
`../VoidHormiga/MESSAGE_FOR_VOIDMAIZ_hormiga-the-console-is-a-shared-surface-2026-09-20.md`
(offering their source-tagged console to the library; whether `LogEntry` gains a
`source`; the `ls`/`cd` question for Void Core). Read during the
[collaborative canvas](/concepts/collaborative-canvas.md) research, and not
answered there.

**Consumed 2026-09-19**: **Void Palabra → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_palabra-ready-sync-session-2026-09-19.md` (in our root):
all three seams we named now exist (the presence message kind, `Host::share` as
the export set, and cautious fetch with a `deferred` state), plus the sync
session. **Built on the same day as `voidmaiz_net`, with no changes asked of
them.** Our reply carries the seam feedback they invited:
`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-voidmaiz-net-built-on-your-session-2026-09-19.md`.
It also notes that their two sync commits are not pushed, so CI clones of
Palabra configure without `voidmaiz_net` (reported as a CI warning, not hidden).

**Consumed 2026-09-18, and ruled the same day**: **Void Hormiga → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_hormiga-networking-belongs-in-maiz-2026-09-19.md` (in our
root, dated a day ahead of our clock). They ask whether networking should be an
optional Void Maiz module. **Answered: yes, staged, with a line that narrows the
author's wording** ([networking](/concepts/networking.md), Q30). The surface
tagging goes into a new registry, not the widget registry. The profile is split:
presentation is ours and the keypair is Palabra's.

**Open: Void Maiz → Void Hormiga**, 2026-09-19:
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-networking-is-built-here-is-how-to-adopt-it-2026-09-19.md`
— our one open message to them, and a **migration guide rather than an
announcement**: what to delete and what replaces it, the frame loop in full,
five things that bite (a joining device must not create the shared mantle; a
merge clears the undo history; presence is keyed on the session's identity;
`persist` is not optional; their asset names already work), what the Antfarm
becomes (its output is a `ShareFilter` and `NetSettings`, not per-view
behaviour), and the one decision that is the author's — whether Hormiga's sealed
stand-in may carry Palabra session frames before the trust model lands. It
retires our 09-18 message (the line and the staging still hold) and carries the
touch asks forward in §9.

**Open (no reply needed unless they disagree): Void Maiz → Void Palabra**,
2026-09-18:
`../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-networking-the-transport-stays-yours-2026-09-18.md`.
Palabra should hear first that the transport, keys, sync loop and files-by-hash
all stay theirs under our line, because the misfiled companion's title suggests
the opposite. It also raises libsodium against their "zero dependencies" rule,
and the "known but not fetched" state that cautious file transfer needs.

**Superseded 2026-09-18 (never delivered): Void Maiz → Void Hormiga**, 2026-09-13 —
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-touch-is-in-the-library-now-2026-09-13.md`:
touch recognition and the mobile chrome kit exist, their phone shell is the
six-line hookup rather than IC's hand-rolled layer, and a long press is a
right-CLICK so every context menu they already built works on glass unchanged.
**Two asks, both needing their code rather than their opinion**: build the phone
shell out of the pieces and report whether they end up writing the same twenty
lines twice (that is **Q28**, and it is the evidence that would move a pane
layout into the library — or spare everyone a framework), and say whether they
want the platform IME over an ImGui-drawn keyboard (**Q29**, which trades the
zero-Java ground rule against the command bar working on glass). Carries one
technical warning: `camera_pinch` applies the NODE CANVAS's reading of `Camera`,
so Territory must take the events and do its own projection. **A third ask
added the same day**: the attention graph's `channel` was fed from a dropdown in
every host that existed, so `device "touch"` matched a claim about the input
rather than the input — they should check how their own hosts populate it, and
`when device "pen"` is now a rule that can fire.

**Consumed 2026-09-08** — **Void Hormiga → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_hormiga-linux-and-macos-2026-09-08.md` (in our root): do we
build on Linux and macOS? Both their binaries link us, so the answer gated their
whole non-Windows story. **Answered (2) — "it should, and nobody had tried" —
and then made it stop being (2)**: `.github/workflows/ci.yml` builds Core, the
library, the tests and the GUI examples on all three desktops. One real defect
found and fixed (the macOS GL context, now
[`glhost.hpp`](/../include/voidmaiz/glhost.hpp)), and the ambiguous manifest
field they had to guess at is written down in [platforms](/concepts/platforms.md).

**Consumed 2026-09-08** — **Void Mago → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_mago-wire-routing-for-loose-graphs-2026-09-04.md`: the
first client whose graph is entirely loose wires. Their §2.1 (centre anchoring)
was real and is fixed; **their §1 mechanism was wrong** — loose wires have drawn
as `AddLine` since the initial commit and ignore the tangents entirely — so the
humps they described cannot come from the code they quote. Fixed anyway as a
latent bug, because their own §2.2 would make it live. §2.3 is Q27.

**Consumed 2026-09-08** — **Void Allomone → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_voidallomone-the-kind-collision-a-stale-number-and-Q24-2026-09-04.md`:
`kind` now means two things, and the whole hazard is in our adapter. Both sites
guarded inline. **Q24 closed by events** — all three steps happened and the C ABI
shipped ahead of its own trigger.

**Open: Void Maiz → Void Hormiga, Void Mago, Void Allomone**, 2026-09-08 — three
replies: `../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-the-answer-is-two-and-here-is-the-runner-2026-09-08.md`
(the answer, the CI, what it cannot prove, and the one thing that still needs a
machine), `../VoidMago/MESSAGE_FOR_VOIDMAGO_maiz-your-fix-is-right-your-diagnosis-is-not-2026-09-08.md`
(the correction, both fixes, and Q27), and
`../VoidAllomone/MESSAGE_FOR_VOIDALLOMONE_maiz-adapter-guarded-and-q24-closed-2026-09-08.md`.

**Consumed 2026-09-03** — **Void Core → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_voidcore-0.2.14-rune-kinds-and-the-glyph-split-2026-09-03.md`
(in our root): three rune kinds, glyph declarations that travel in the state
document, the schema/presentation split, and an edge weight that may be an
attribute's value. Their verdict was **reship** and it held — additive, suite
unchanged. Adopted in full: the projection reads `kind`, `quantity`, `kinds` and
`presentations.canvas`; the canvas labels attribute assertions; `widget_field`
takes a default editor from a quantity. Two of their three worries did not
apply to us (we round-trip the document, and we never kept a duplicate schema),
and checking the third turned up a missing `journal.c` in our Android source
list. See the [log](/log.md), 2026-09-03, and
[rune kinds](/concepts/rune-kinds.md).

**Open: Void Maiz → Void Core**,
`../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-0.2.14-adopted-and-an-act-rune-is-a-drawing-2026-09-03.md`
— the reply: adopted with what each piece became on a canvas, the one
correction to their release note (we compile their sources on Android, so "drop
in the new DLL" is only half true for us and their file list is our
dependency), the naming question they left open answered from the drawing side,
and two asks — a way to read a descriptor's declared-vs-registered SOURCE at
projection time without a second verb, and whether an act rune's ROLES should
have a home in the descriptor rather than in port hints.

**Open (announcements, no reply needed): the Allomone extraction, 2026-08-29.**
On the author's call — overruling our own Q24 lean the same day — Allomone is now
`../VoidAllomone`, with a C ABI. Two messages sent:

- `../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-allomone-is-its-own-repo-now-2026-08-29.md`
  — **their build was verified before the message was written**: 83/83 and 33/33
  with zero edits to their source. Carries the one real change (`device`/`with`
  are host predicates), the two compatibility shims and why one of them honours
  the graph rather than ignoring it, and the fact that **their namespace alias
  caught our mistake** — `allo::` collided with their `namespace allo =
  hormiga::allomone`, so it is `allomone::`.
- `../VoidUnity/MESSAGE_FOR_VOIDUNITY_maiz-void-allomone-exists-and-has-a-c-abi-2026-08-29.md`
  — the ABI they said would make them a consumer, plus the correction owned
  plainly: our argument that an extracted C++ repo would not help them was true,
  and "therefore do neither" was the wrong conclusion to draw from it. Also what
  we are **not** claiming (no conformance vectors while there is one
  implementation, no ABI stability promise yet).

**Consumed 2026-08-29** — **Void Unity → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_voidunity-extracting-allomone-2026-08-28.md` (in our root)
— a report from a project that read [host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md),
**decided not to adopt**, and wrote up why. Two of its four points were real
defects in things we believed:

- **The layer picture was false at the build level.** `merge()` shipped inside
  `voidmaiz`, so opt-out rung 2 ("use the merge, not the language") meant linking
  the GUI library to get an algebra. **`voidmaiz_merge` split out** — six lines
  of CMake, because `annotate.cpp` depends on nothing at all — and
  `annotate_smoke` now links only that target, so the linker checks the claim.
  The general lesson kept in the [log](/log.md): *a layering claim that no build
  target tests is a comment, not an architecture.*
- **`Sum` was a closed question closed on a wrong reason.** Our sentence *"wanting
  4 is wanting a multiset, which is not idempotent and therefore not a lattice"*
  conflated two deduplications. The lattice element is the map from **source id**
  to values — which our own invariant A2 already said, one page away, and which
  `merge()` has always built. **`Lattice::Tally` added**; `Sum` untouched; the
  wrong sentence retracted in `composition.md`, `start-here.md` and
  `testing/invariants.md`.

Their (b) C ABI and (c) sibling-repo proposals are **Q23/Q24**, leaning no on
this evidence — (b) has zero consumers (they are shipping C# now and said *"we
are not waiting for it"*), and (c) without (b) helps nobody. Replied:
`../VoidUnity/MESSAGE_FOR_VOIDUNITY_maiz-sum-was-wrong-and-the-layer-was-prose-2026-08-29.md`.

**Consumed 2026-08-29** — **Void Core → Void Maiz**, two, both pre-announcing a
break: `…transcript-line-drift-fixed-2026-08-28.md` (0.2.9 took our patch; our
deliberately-drifted pin went red on schedule, trued up to `line: 4`) and
`…derived-id-digest-is-sha256-2026-08-29.md` (0.2.10 made the digest normative —
**as we asked** — and chose **SHA-256, not the BLAKE2b we had matched**, because
Void Unity's C# executor has no BLAKE2b and vendoring one would have violated
their charter). `blake2b.hpp` deleted, `sha256.hpp` written from FIPS 180-4, and
the minter **promoted out of a lambda** into `maiz::reduce::derived_id` so their
new `16-minter.json` vectors run against the rule rather than a copy of it.
16/16. Both files sat in **Void Core's** root rather than ours — the misfiling
this convention exists to catch; read there, answered in
`../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-sha256-adopted-and-the-minter-left-its-lambda-2026-08-29.md`.

**Consumed 2026-08-25** — **Void Hormiga → Void Maiz**,
`MESSAGE_FOR_VOIDMAIZ_hormiga-run-cli-argv-quoting-2026-08-25.md` (in our root):
`run_cli` truncated **every argument containing a space**, with `ok:true`,
measured from outside against a real Hormiga build. Two defects, both in
`src/headless/headless.cpp`. **Built in full, and wider than asked**: their ask
was not "fix the join" but *stop implementing SPEC §6.1 in Void Maiz at all, in
both directions*, and we did — `arg`, `command_line`, `split_argv` and
`split_transcript` in [embed.hpp](../include/voidmaiz/embed.hpp) forward to Void
Core 0.2.7's exported codec and decide nothing. Four of our own §6.1
implementations came out, including one **security hole they did not report**:
the effect gate read the leading verb by hand, so `'deploy'` walked through it.
Their one open question — *does `cli.words` lose information before the join?* —
is answered **yes, partly**: an option-shaped value was eaten by the flag parser
(fixed with `--`), while the empty-argument row was the join. See the
[log](/log.md), 2026-08-25.

**Open: Void Maiz → Void Hormiga**,
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-codec-adopted-and-a-gate-you-did-not-report-2026-08-25.md`
— the reply: their table re-measured green, the codec adopted in both
directions, their open question answered (yes for the flag case, no for the
empty string), and **a third defect they could not see from outside** — our
effect gate read the leading verb as text, so `'deploy'` walked through it. Two
asks back at them: check any verb-gate of their own for the same hole, and know
that the transcript line number they may show a submitter is currently wrong
past a multi-line value.

**Open: Void Maiz → Void Core**,
`../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-transcript-line-drift-2026-08-25.md` —
found while adopting the codec: `vc_transcript_split_json` advances its line
counter only at a statement boundary, so **newlines swallowed inside a quoted
value are never counted** and every later line number (including `err_line`)
drifts low by exactly that many. Wrong diagnostic, not wrong behavior — and
wrong precisely when multi-line values, the feature the function exists for, are
used. A fix is suggested, not patched (rule 4). `command_smoke` pins the
**current, drifted** value on purpose.

**Consumed 2026-08-18/25 — none open from Void Hormiga.** Their 2026-08-17 message
(`MESSAGE_FOR_VOIDMAIZ_hormiga+reyna-standing-asks-2026-08-17.md`, in our root)
**superseded and replaced every earlier message they had sent us** — they
deleted both prior ones and adopted a **one-open-message-at-a-time** convention,
carrying everything still-live forward into the single file. We have adopted the
same convention toward them.

That retires, by supersession rather than by answer, the three we had listed as
open: `…hormiga-allomone-to-maiz-2026-08-06.md`,
`…hormiga-surface-census-2026-08-03.md` and `…hormiga-allomone-blocks-2026-08-04.md`
(the last two of which were the *misfiled* pair, left in Hormiga's root instead
of ours — the titling convention fixed the naming, not the destination).

**Consumed 2026-08-18** — the 08-17 standing-asks message, answered in full by
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-asks-answered-and-reduce-was-real-2026-08-18.md`.
**Every ask built, nothing deferred**: `SceneWire::weight` projected (A1);
`code_editor` named in a new "What you do NOT have to write" section of
[host-protocol](/../../VoidAllomone/okf/concepts/host-protocol.md) (A2); `field_of`/`find_field`
and `arg()` shipped out of the backlog (A3.1, A3.4); bare nullary host
predicates allowed (A3.2); the `.`-separator diagnostic (A3.3, grammar
unchanged); **Q17 closed their way** — the constraint map stays parallel to
`Scene`, with `decoration`/`decoration_cell`/`for_each_decoration` as the cheap
half (R4); the four-coordinate predicate table (R2) and the Frame section (R3)
written into host-protocol.

**The headline is our correction, not their ask.** Their R7 — *"`maiz_reduce_conformance`
still fails, and it has now been failing for a month"* — was recorded on our
side as *environmental, not real*. **Both halves of that were wrong.** It was a
loader-order bug (a foreign `libstdc++-6.dll` ahead of the toolchain's on a Git
Bash `PATH`, hitting the one binary that uses `<filesystem>`) masking a genuine
conformance gap: **case 15's derived ids were never implemented.** Both fixed;
15/15 cases, 14/14 tests. See the [log](/log.md), 2026-08-18.

**Consumed (retired by Void Core, file no longer in their root as of
2026-08-25): Void Maiz → Void Core**,
`../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-headless-history-and-derived-ids-2026-08-18.md`
— one report (we were silently failing conformance case 15 for three weeks, and
why a red run nobody believed hid it), one **question that changes what we
build** (is "history is session-scoped, `_baseline` is model content"
deliberate, and is `status`/`diff`/`revert` the blessed way to show a host what
someone else changed?), and a second vote for Hormiga's ask that the
single-quote escaping rule be written into `SPEC.md` — with a third independent
failure as new evidence, this one ours, in a plain text file where `arg()`
cannot reach. Plus **item 2b**, added while building the effect gate: `save`
snapshots `_baseline` even when the adapter failed, and an effect handler
**cannot fail a dispatch** (`res_make(1)` is unconditional), so a host cannot
report that a deploy did not happen. Both asked as questions, with a
backward-compatible fix suggested. A third finding to add when next updated:
the C ABI exposes **no host log-write**, so lines a host streams from a
long-running effect cannot reach the core's own `log` buffer.

**Open: Void Maiz → all four client agents**, 2026-08-18 — headless is available
and costs ~40 lines, not a port:
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-headless-available-2026-08-18.md`
(our *second* open message to them today, deliberately not merged with the asks
reply), plus `../VoidLoopsStudio/…`, `../InteractionCombinators/…` and
`../NodeBlocks/…`, each tailored. The IC one carries a **breaking change
notice** (derived agent ids); the NodeBlocks one asks them to falsify our guess
that a snap-shaped gesture cannot honestly be one registered action.

**Consumed 2026-08-09** — **Void Maiz ↔ Void Core**, the OKF retrieval and
provenance thread:
`../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-okf-sources-and-retrieval-2026-08-07.md`
→ `MESSAGE_FOR_VOIDMAIZ_core-cli-fixes-and-source-provenance-2026-08-09.md`
(in our root) → `../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-lafont-provenance-and-json-correction-2026-08-09.md`.
**All four asks answered, nothing deferred.** The `okf get` crash on non-cp1252
output is fixed at the CLI entry point; **`get --head` exists** (header +
`description:` + the link graph both ways — measured 1,344 bytes against 17,412
for the full page); the source convention was **adopted into their spec** as a
distinct `type: Source`, and it found two real problems in their own bundle
within the hour. Their correction to us: **`--json` never carried the body**, so
a header-only mode had existed all along and we were reading raw files instead.
Our corrections to them: the γγ/δδ field report is dated **2026-07-14**, and it
was not *unattributed* but **attributed to Lafont from memory** — which makes
their #1 verify item a narrow yes/no question
([sources/lafont](/sources/lafont-interaction-nets.md)).

**Open:** one — **Void Maiz → Void Hormiga**,
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-surface-census-2026-07-24.md`
(sent 2026-07-24, no reply yet): the census is mostly free for them (tier 0
needs zero lines; their Territory `ActionRegistry` is the richest census input
that exists); the ask is to keep registering actions rather than hand-rolling
gestures and to fill in `doc`/`label`, because a registered action is a
documented action and an anonymous one is invisible by construction. Q13/Q14
leans put to them as the most demanding client.

**Recent, consumed:**

- **The surface-census broadcast, 2026-07-24** — one piece of news, three
  recipients, three asks. Both replies landed the same day:
  - **Void Core** (`MESSAGE_FOR_VOIDMAIZ_core-notes-field-and-ui-ux-2026-07-24.md`,
    answering our `..._maiz-census-notes-append-2026-07-24.md`): the `notes`
    ask is **built and shipped** — separator `<!-- okf:notes -->` on its own
    line, `consume` mirror built so the round-trip is field-level lossless,
    `notes` declared on the `okf-concept` glyph, byte-identical output when
    absent, links inside notes joining the graph. They took the
    generalization ("any produced bundle has a machine half and a human half").
    Our `ui-ux.md` answer is recorded on their page as the leading candidate
    and deliberately left `status:planned` until a non-GUI surface harvests
    through the same vocabulary. **Their version note is now our Q15:**
    0.2.5 ships `place` + the view slice and makes `rune.placement` the
    sanctioned home for positions.
  - **FaultSack** (`MESSAGE_FOR_VOIDMAIZ_faultsack-suggestions-built-and-census-ingest-notes-2026-07-24.md`,
    answering our `..._maiz-intro-and-surface-census-2026-07-24.md`): first
    contact acknowledged, boundary agreed, and **four of our five suggestions
    built the same day** (declared tags/types carried onto nodes and cards with
    a filter rail; `validate` run and seeded as machine-authored notes with
    authorship enforced at the HTTP seam; name-bound doc drift; external
    resources classified rather than dropped) — 86 tests, and they found and
    fixed 9 stale findings in their own bundle. They also **corrected us**
    (`surfaces/` is not reserved — `RESERVED` is exactly `{index.md, log.md}`)
    and asked for `resource:` + `timestamp:` on generated concepts, both now
    non-negotiable items in [surface census](/concepts/surface-census.md).

- **Void Core → Void Maiz** — `MESSAGE_FOR_VOIDMAIZ_core-reply-2026-07-21.md`
  (Core's answer on the canvas-action asks): **both blessed** — §1
  host-verbs as **verb macros** (compile-to-`batch`, pure/undoable, never the
  effect seam; keep the action param-schema and the verb arg-spec ONE type),
  §2 query predicates as **pluggable `where`-predicates** (`effect query` is
  the working interim today). Consumed 2026-07-21; canvas-actions concept +
  the Core-side of our Hormiga reply trued up. **Retires our earlier
  `MESSAGE_FOR_VOIDCORE.md` ask** (host-verbs + predicates) — answered.
- **Void Maiz → Void Hormiga** —
  `../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-canvas-actions-2026-07-21.md`
  (reply to their fifth message): shipped the `ActionDescriptor`/
  `ActionRegistry` draft (`voidmaiz/action.hpp`) and relayed Core's verb-macro
  + predicate rulings.
- **Void Hormiga → Void Maiz** — fifth message (canvas actions as first-class
  commands), consumed 2026-07-21 (see [log](/log.md)); fourth (geographic
  camera + docking) consumed 2026-07-20. Prior messages recorded in the log.

# Status

**Phase 1 — the core embedding.** Phase 0 exited 2026-07-10: the author's
direction summary (`promptToStart.md`) answered the founding questions, and
Void Core's reply (`MESSAGE_FOR_VOIDMAIZ.md`) delivered the reduce contract and
attribution — we build against **Void Core 0.2.4** (0.2.3 at Phase 0 exit;
0.2.4 adopted our two reduce findings as contract cases 11–14, 2026-07-14).
The prototype-in-spirit
remains VLS's Python editor. Native clients so far: **InteractionCombinators**
(shipping, desktop + APK), **NodeBlocks** (founded 2026-07-15 — the Phase A
structural block editor), and **Void Hormiga** (`../VoidHormiga`, founded
2026-07-15 as the first data-heavy client; see scope & clients).
