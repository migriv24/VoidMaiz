---
type: Questions
title: Developer questions
description: Open decisions for the author, each with a lean. Open — Q36 (where the LAN socket layer lives now that Palabra declined it; lean: grow voidmaiz_lan into it, gates stage N3 only), Q29 (how a soft keyboard reaches a Void Maiz application; lean: an ImGui-drawn keyboard first, because the platform IME costs the zero-Java claim), Q28 (does the library lay out panes per substrate; lean: no, keep climbing one rung per real need — ask Void Hormiga after they build a phone shell), Q27 (should wire routing be a CanvasStyle option; lean: Orthogonal yes but probably per-glyph, not per-canvas, and `Direct` is just correct behaviour rather than a mode), Q26 (where an act rune's role list lives; lean: a `roles` key on the descriptor, asked upstream rather than invented here), Q25 (which mantle a compiled attention graph belongs in; the Device enum is already generalized to an open channel string), Q23 (should JoinFn see the per-source grouping; lean yes, additively, when a client asks), Q22 (should identifiers accept non-ASCII; lean ASCII-only for now, widening is additive), Q21 (may the out-of-tree test harness have dependencies; lean stdlib-only Python), Q20 (whose attention the user action graph records — per-peer or shared across Palabra peers; has a privacy cost, decide before anything materializes one), Q16–Q19 (Void Hormiga's four Allomone boundary questions; the engine is built, the calls are the author's), Q12 (widget kit — ImGui-composed vs sanctioned Qt-class adapter), Q13 (surface-census trigger), Q14 (where a census bundle lands), Q15 (retarget moves to `place` now that Core 0.2.5 landed — takes moves out of undo). Q11 (workspace rung) decided 2026-07-20: enable ImGui docking. Q31–Q35 (collaborative canvas) decided 2026-09-20: the author accepted every lean.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-09T00:00:00Z
---

Open decisions, with leans so a non-answer has a sensible default. Answer inline,
in chat, or via FaultSack notes; answers fold into concepts and clear from here.

# Open

- **Q36 — where does the LAN socket layer live?** Raised 2026-09-21 when Void
  Palabra **declined** to build `voidpalabra_lan`, citing the author's lean that
  Palabra should not own Android networking. Palabra now owns everything every
  transport must agree on: the frame, the stream envelope and `StreamReader`, the
  session, coalescing, and lifecycle as "a dead link is just a new session". What
  is left is the sockets: UDP beacon ("beacon out, unicast back"), per-interface
  binding, the TCP stream, the sealed session (X25519, the short authentication
  string, libsodium cross-compiled for Android), and the lessons from Hormiga's
  three defects. Today that code exists only inside Void Hormiga.
  (a) **grow `voidmaiz_lan` into it**, as an optional target, the only one that
  opens sockets; (b) a **new sibling repository** for platform networking; (c)
  each application keeps its own (Hormiga's stays, and IC copies it).
  **Lean: (a).** `voidmaiz_lan` already holds the platform facts (interfaces, the
  join code, the Android multicast lock), every Void GUI app on Android already
  links Void Maiz, and a new repository costs an agent and a relay loop for about
  1,500 lines. It narrows Q30's wording ("Palabra owns transport" becomes "Palabra
  owns the protocol, the sockets sit beside the platform code"), which is exactly
  the revision the author's lean implies. (c) is how a family stops being one.
  **Gates stage N3 only**: N0–N2 need no sockets. The trust gate (the author's
  explicit LAN-only yes for IC) applies wherever it lands.

- **Q29 — how does a soft keyboard reach a Void Maiz application?** Raised
  2026-09-13 while building the touch layer. It is the APK's oldest known gap
  (v1, 2026-07-14: *"no soft keyboard; Save As pre-fills a timestamp name; the
  command bar is desktop-only until an IME shim lands"*), and on a phone it is
  not a polish item — **the command bar is the CLI inside the UI, and without an
  IME commitment 2's showpiece is desktop-only.** Every text field, every rune
  name, every `set` of a string value is behind the same wall.

  The reason it is a question and not a backlog entry is **ground rule 5 and the
  zero-Java claim**. Showing Android's IME means `InputMethodManager`, which is
  Java, reached through JNI. The APK's proudest structural fact is that it
  shipped with *zero Java and zero Gradle* — a `hasCode=false` NativeActivity
  manifest and a packaging script — and [substrates](/concepts/substrates.md)
  records that as the proof the C++20 rule survives. Three answers:

  (a) **JNI call-out from the shell.** ~40 lines of `ANativeActivity`-JNI to
      toggle the IME and pump `AInputQueue` key events into ImGui. No Java
      *source*, but it is logic calling into the platform's Java runtime, so the
      claim becomes "no Java source" rather than "no Java" — a weaker sentence
      that we would have to say honestly every time.

  (b) **An on-screen keyboard drawn in ImGui.** Zero platform surface, works on
      every substrate including a VR panel and a kiosk, and it is a *widget* —
      which is the kind of thing this library already builds. Costs: no
      autocorrect, no swipe, no language layouts, no accessibility integration,
      and users hate a fake keyboard for anything longer than a name. But a
      command bar is not prose: it is short, ASCII, and has a known vocabulary,
      which is the one case where a custom keyboard can be *better* (a verb row
      beats a QWERTY guess).

  (c) **Both, chosen per field** — the platform IME for free text, the compact
      command keyboard for the command bar.

  **Lean: (b) first, then (c) if a host asks for prose entry.** It keeps the
  platform surface at zero, it is testable, it serves substrates that have no
  IME at all, and it makes the command bar *good* on glass rather than merely
  possible. But this is exactly the kind of call the author makes — it trades a
  ground rule against a headline feature, and (a) is what every other project
  would do. See [touch](/concepts/touch.md).

- **Q28 — does the library lay out panes per substrate, or does each host?**
  Raised 2026-09-13; the honest continuation of **Q11**, whose ruling (enable
  ImGui docking, 2026-07-20) solved the desktop half and does not reach a phone.
  DockSpace assumes a surface wide enough to hold two panels side by side; a
  430-px portrait screen holds one, and the arrangement is not a smaller version
  of the desktop's but a different one — stacked, switched by a segmented
  control, with the inspector as a bottom sheet and the log gone entirely.

  The rung would be: a host declares its panels once (`{"canvas", "inspector",
  "log", "table"}` with roles/priorities) and the library arranges them per
  substrate — docked on desktop, stacked-and-switched on glass.

  **Lean: no, not yet — keep climbing one rung per real need.** That was the
  author's rule at Q11 and it has been right twice. What shipped instead is the
  *pieces* a host arranges itself (bottom sheet, segmented control, FAB, swipe
  row, touch profile), which is what docking's ruling did too: the library owns
  the primitive, the host owns which panels exist. A layout engine is the first
  thing on this list that would genuinely be a **framework**, and the
  vendor-don't-depend instinct that favoured *more ImGui* at Q11 points the
  other way here, because ImGui has no responsive layout to turn on.

  **What would change the answer:** Void Hormiga building its phone shell and
  finding that its four workflows plus Territory need the same twenty lines its
  desktop shell needs — i.e. a second host, with the same shape, written twice.
  That is exactly the evidence that moved touch recognition into the library
  this session, and it would move this too. Ask them after they build it, not
  before.

- **Q27 — should wire routing be a `CanvasStyle` option?** Raised 2026-09-04 by
  Void Mago, the first client whose graph is entirely loose wires. They proposed:

  ```cpp
  enum class WireRouting { Tangent, Direct, Orthogonal };
  WireRouting routing = WireRouting::Tangent;   // default = today
  ```

  `Direct` being a run-derived tangent, `Orthogonal` the elbow style dependency
  graphs conventionally use — leave downward, run horizontally, enter from
  above — which for a DAG genuinely is more legible than any curve, because the
  horizontal runs line up and the eye can follow a column.

  **Their §1 and §2.1 are already built** (body-edge anchoring, derived
  tangents); this is only the part that adds public API.

  **Lean: yes for `Orthogonal`, no for the enum as proposed.** Two halves:

  - `Direct` should not be an option, because it is now simply what a loose
    wire does. An enum member for "the correct behaviour" is a migration flag,
    and we have no client to migrate — Mago is the only one with loose wires
    and they asked for the fix.
  - `Orthogonal` is a real second answer to a real question and deserves to be
    selectable. But **routing is a property of a graph's meaning, not of a
    canvas's taste**: a build DAG wants elbows, an association graph wants
    curves, and an application can hold both in one mantle. So the switch may
    belong per-glyph (`presentations.canvas`, which Void Core 0.2.14 gave us a
    home for) rather than on `CanvasStyle`, which is per-canvas and would force
    one answer on every wire in the view.

  Deciding needs a client with two graph shapes in one canvas. Until then the
  cost of waiting is zero and the cost of guessing is a public enum we keep
  forever. Backlogged T2, not built.

- **Q26 — where does an ACT rune's role list live?** Raised 2026-09-03 while
  adopting Void Core 0.2.14. An `act` rune reifies a verb so a ternary fact can
  be expressed — *"Superman flies across the sky"* needs an agent, an act and a
  path — and the participants attach to the act's ports. That is an
  interaction-net agent, which this canvas already draws, so nothing is blocked
  today: a host names the ports through `presentations.canvas.ports` and it
  works.

  The question is whether that is the right home, and the answer looks like no.
  **A role is schema, not presentation.** "Agent", "patient" and "path" are true
  of the act in every modality — a table row, an email, an XR scene and this
  canvas all have the same three participants — while `presentations.canvas` is
  by construction one modality's look. Putting roles there means every modality
  re-declares them and the first two disagree.

  **Lean: ask upstream rather than invent one.** A `roles` key beside `fields`
  and `kinds` on the descriptor is the shape that matches what 0.2.14 just did
  everywhere else (schema in the descriptor, look in `presentations`), and this
  is exactly the kind of thing rule 4 says we message rather than build — a
  role vocabulary invented in a UI library would be a domain model wearing a
  renderer's clothes. Asked in
  `../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-0.2.14-adopted-and-an-act-rune-is-a-drawing-2026-09-03.md`.
  Until answered, port hints stay the interim home and the backlog item stays
  T2-blocked.

- **Q25 — which mantle does a compiled attention graph belong in?** Raised
  2026-08-29 by the author, generalizing the user graph: *"we don't know what
  mantle will be classified as a 'user graph'. the concept itself may need to be
  expanded."*

  The graph accumulates ephemera and `compile(mantle)` turns them into runes, so
  the mantle is the host's argument today and the code is already agnostic. The
  question is what it should MEAN. "The mantle of attention" is obvious when the
  attender is one person at one screen and stops being obvious the moment it is
  not: an agent working through the dispatcher, several people on one document,
  a harvest whose stages co-occur.

  **What already changed, because it did not need an answer:** the closed
  `Device { Pointer, Touch, Pen, Gamepad, Voice, XR }` enum is now an open
  `channel` string, and `kind`/`channel` default to EMPTY rather than to
  `"widget"`/`"pointer"`. The old doc said the library "records the modality and
  never interprets it" while enumerating six HID modalities — a closed
  enumeration IS an interpretation, and it made every non-GUI host's affordance
  claim to have been clicked. That was the concrete half of "more abstract about
  what it is applied to" and it is done.

  **Lean on the mantle itself: one per ATTENDER, named by the host, defaulting
  to one shared mantle.** A per-person mantle makes "whose attention is this?"
  answerable and prunable, which the [multi-user](/../../VoidAllomone/okf/concepts/multi-user.md)
  page will want; a single shared one is the right default because coherence
  across people is a real signal and splitting it by default would destroy it
  before anyone measured whether it matters. Not built — the host still passes a
  name — and it interacts with Q20 (whose attention the graph records), so the
  two should be answered together.

- **Q23 — should `JoinFn` see the per-source grouping?** Raised 2026-08-29 while
  adding `Lattice::Tally`. A `Custom` join receives the **cross-source
  deduplicated** value set, so a host cannot write its own per-source
  accumulation: the two-identical-rings case is unreachable through `Custom`
  even now that `Tally` exists, for any domain that is not numeric (concatenating
  equipment names, unioning tag bags, averaging vectors per contributor).

  **Lean: yes, additively — a second, optional `GroupedJoinFn`**, taking
  `vector<pair<source_id, vector<string>>>` (sorted, each inner set sorted and
  deduped, so it stays structurally order-independent for the same reason
  `JoinFn` does). Not a change to `JoinFn` itself: Void Hormiga has shipped
  against that signature and a silent widening would be exactly the kind of
  quiet semantics change this project keeps writing messages about. Cheap, but
  it is new public surface with no client asking yet, so it waits for one.

- **Q22 — should identifiers accept non-ASCII?** Raised 2026-08-09 while fixing
  the tokenizer's UTF-8 handling ([language](/../../VoidAllomone/okf/concepts/language.md)).
  Today `ident_char` is `isalnum || '_' || '-'`, so **inside a string anything
  goes** — tags, names and values have always been UTF-8-clean — but a property
  or predicate *name* must be ASCII. `then température "18"` does not parse.

  **Lean: leave it ASCII-only for now, and revisit when a domain asks.** Three
  reasons. It is **additive** — widening later breaks no existing script, while
  narrowing later would. The identifier set is small and mostly ours (kernel
  words, host predicates, property names), and a host wanting a localized
  *surface* can label it in its own UI without the AST changing. And widening
  correctly is not one line: `isalnum` is locale-dependent, so the honest
  version needs a real Unicode identifier rule (XID_Start/XID_Continue) or an
  explicit permissive range, plus a normalization decision so `é` and `é`
  (composed vs decomposed) do not become two different properties.

  **What is already fine and should not be confused with this:** every *value*
  is a quoted string, so nothing about non-English data is blocked. This is only
  about the vocabulary words themselves.

- **Q21 — may the out-of-tree test harness have dependencies?** Raised
  2026-08-06 with the [testing folder](/../../VoidAllomone/okf/concepts/testing/index.md).
  Ground rule 5 is *vendor, don't depend; no package managers* — written about
  shipped code, and the harness is developer-only Python that no build target can
  reach ([harness](/../../VoidAllomone/okf/concepts/testing/harness.md)). So the rule may not
  apply, but it is stated flatly enough that stretching it is the author's call.

  **Lean: standard library only.** The generation we need is simple; the part of
  Hypothesis genuinely worth having is **shrinking**, and for our case shapes
  that is a short obvious algorithm (drop a rule, a source, a term, a subject;
  repeat while it still fails). Writing ~150 lines keeps `pip` out of the repo
  entirely, which is what makes *"delete `harness/` and the build is still
  green"* trivially true. If shrinking turns out to be the hard part, the honest
  alternatives are to vendor a single-file library LiteGraph-style, or to declare
  a dev-only exception — both fine, both the author's to make.

- **Q20 — whose attention does the user action graph record: per-peer, or merged
  across peers?** Raised 2026-08-06 while taking Void Palabra's domain into
  account ([multi-user](/../../VoidAllomone/okf/concepts/multi-user.md)). Once Palabra syncs
  state, "a person" stops being obvious, and the choice **changes what `with`
  means**:

  - **per-peer (private)** — `with "volume"` = *"coherent in MY work"*; the graph
    never syncs; scripts stay portable but derive differently per person;
  - **merged (shared)** — `with "volume"` = *"coherent in the TEAM's work"*; the
    graph syncs and — pleasingly — **joins correctly with no new machinery**
    (coincidence weights are non-negative counts under addition, frames are set
    union: already a lawful Palabra citizen).

  The shared reading is genuinely attractive and closer to the original
  stigmergic picture — pheromone is *shared environment* by definition, and a
  colony where each ant reads only its own trail is not a colony. **But a
  materialized attention graph is a record of what a specific person was doing,
  on which device, arriving in a log that is attributed and permanent by
  design.** [Horizons](/horizons.md)' note that total observability is a security
  primitive cuts both ways here.

  **Lean: per-peer by default; shared only as an explicit, per-affordance opt-in
  a host must ask for.** It is the conservative default, the private reading is
  what makes `with` personal and responsive, and sharing can be added later
  without changing the structure — whereas **un-sharing something already synced
  is not possible.** Decide before any host materializes a user graph into a
  synced mantle; nothing today does.

  **Made urgent 2026-09-18 by presence** ([networking](/concepts/networking.md)).
  Void Hormiga shipped live presence, which shares what someone has selected and
  which section they are in, every second. That is the *ephemeral* half of this
  question, and it is live between two real machines. The lean still holds for
  the *materialized* graph, because presence never enters history, so nothing
  needs un-sharing. Presence does add one point: **the sender needs its own
  switch**. The author's worry about showing which tabs people are in was
  clutter, which is a receiver-side filter. A filter on someone else's screen is
  not privacy, so "do not broadcast which surfaces I have open" has to be a
  separate setting on the sending device.

- **Q16–Q19 — Void Hormiga's four Allomone questions** (their 2026-08-06
  message). The engine is BUILT ([Allomone](/../../VoidAllomone/okf/index.md)); these are
  the boundary calls the author still owns, and each has a lean the code already
  follows, so a non-answer is safe.

  - **Q16 — is a host-agnostic Allomone service in Maiz's remit, or should it be
    a sibling library Maiz merely renders?** **Lean, and what shipped: split
    it.** The *merge* is a pure `(scripts, mantle) → annotations` function —
    that is the `project` half of the view seam, so it lives in base `voidmaiz`.
    The *language* is quarantined in its own target `voidmaiz_allomone`, so a
    host wanting a canvas and no scripting links neither. If the author would
    rather the whole thing were a sibling repo, the seam is already cut for it.
  - **Q17 — is a Constraint Map (`RuneID → {property → value}`) the right seam?**
    **Lean: yes, with three changes, all made.** (a) a **declared merge law per
    property**, because without one "merge" has no definition; (b) **provenance
    per cell** — their own hover-explains feature needs it and the conflict UI
    cannot exist without it; (c) keyed on the address space `Scene` already uses,
    so a merged map needs no translation layer. **The open half is CLOSED
    (2026-08-17): keep it parallel.** We put it back to Hormiga as the host with
    the real domain, and they answered no, for three reasons we took whole —
    their subjects are not all Scene nodes and the ones that are are not always
    in the same Scene (their `refresh_allo_rules` projects the data mantle
    independently of which tab has focus, and that decoupling was a bug fix);
    they derive with **no Scene at all** in the export path and the tests; and
    `Merged` has a different invalidation rule, so folding them makes one object
    with two reasons to be rebuilt and the more frequent one wins. Shipped the
    cheap half they asked for instead — `decoration`, `decoration_cell` and
    `for_each_decoration` in `project.hpp`, free functions over two independent
    projections, with `node.name` pinned as the address convention.
  - **Q18 — expose a gesture stream / co-occurrence API, or keep gestures
    private?** **Lean: expose the STREAM, never the graph** — and that is what
    shipped. Gestures are ephemera ("Never state"); a persisted attention graph
    accumulated from them would violate that, so `UserGraph` holds them
    in-session and `compile()`s to commands on the census pattern. The graph is
    then model content Allomone can query, which beats a private API.
  - **Q19 — is a node-overlay-of-an-AST an `edit_canvas` extension or a new
    widget?** **Lean: NEITHER — a new tree-shaped view, and this pass declined
    it.** Hormiga's own 2026-08-04 message proved `edit_canvas` is the wrong
    paradigm for syntax trees; that is three bespoke-view findings in a row
    (map, blocks, AST). The thing worth generalizing is the **tree/outline view**
    [views as projections](/concepts/views-as-projections.md) already names, built
    against the host-view seam. Their tokenizer/AST is offered as the model.

- **Q20 — should a running GUI accept commands from an agent, or stay
  single-writer?** *(opened 2026-08-18 with [headless](/concepts/headless.md).)*
  **Lean: stay single-writer for now, and make the collision loud.** A headless
  session takes an advisory lock beside the state document and refuses to start
  when a GUI holds it, naming the holder — which turns "an agent silently
  overwrote my afternoon" into a refusal. The alternative is genuinely
  attractive: if a live application accepted commands over a local seam, the
  in-app console and the headless agent would become *literally the same thing*,
  which is the cleanest possible expression of commitment 2. But it turns every
  application into a server, with a port, a lifetime, and an authorization
  question, and the author has not asked for that. Worth revisiting the first
  time someone actually wants an agent working alongside them rather than
  instead of them.

- **Q11 — how far does host chrome abstraction go?** The APK forced the first
  pieces into the library (2026-07-14): `apply_touch_metrics` (one call makes
  every ImGui widget finger-sized) and `tool_button` (SmallButton on desktop,
  real Button on touch), and the author asked the general question: *"maybe
  the buttons and panels and such should be abstractable in some way?"* The
  next rung would be a **workspace/pane abstraction** — panes + splitters +
  the config-tier persistence declared once, so a host describes "canvas,
  inspector, log" and the library lays them out per substrate (side-by-side on
  desktop, stacked on portrait glass, log hidden on touch). **Lean: climb one
  rung per real need, not a framework** — metrics and buttons are shared
  because both shells needed them TODAY; the pane layout is still one
  `if (touch_mode)` in one host, and a second host (VLS-native) should exist
  before its shape gets frozen into the library. Revisit when VLS-native's
  workspace gets built.

  **The second host arrived** (Void Hormiga's founding message, 2026-07-15,
  ask §3.4): a full application — canvas + inspector + table view + log strip
  + command bar, user-re-proportioned — asking for the ruling explicitly so
  it builds its shell once. **Proposed ruling (drafted in
  `MESSAGE_FOR_VOIDHORMIGA.md`, awaiting the author):** the library offers
  the PRIMITIVES it already has — `maiz::splitter` (drag + `released` for the
  config-tier flush), `apply_touch_metrics`, `tool_button` — plus the blessed
  host pattern proven in InteractionCombinators (fractions as view state,
  flushed to `config set view.panels` on release, restored on boot); the
  window/pane LAYOUT itself stays host-owned. No docking framework.

  **DECIDED 2026-07-20 — the author OVERRODE the proposed ruling: enable ImGui
  docking.** Void Hormiga's fourth message (a fourth heavy workflow,
  Territory) is the "second host with a real need" the lean gated on, and the
  author wants FL-Studio-style movable/floating/re-dockable panels as a
  genuine usability need. The winning argument: hand-rolling a pane manager
  would be *more* of a framework (reinventing docking, worse) than turning on
  the docking feature of the library we already vendor — vendor-don't-depend
  favors using more ImGui, not less. So: re-vendored `v1.92.1-docking`,
  **DockSpace only** (`DockingEnable`, never multi-viewport — the single-
  surface NDK path can't do viewports), and shipped the "one rung past
  splitter" as `enable_docking` + `begin_dockspace`/`end_dockspace`
  (widgets.hpp). The host still owns WHICH panels exist and their arrangement
  (DockBuilder); the library owns only the fullscreen-host + DockSpace
  boilerplate. The splitter primitive stays (a host may still hand-split a
  pane); the "no docking framework" line is retired. See log 2026-07-20.

- **Q12 — the widget kit: ImGui-composed, or a sanctioned wrapped-toolkit
  (Qt-class) adapter path?** The author asked it directly (relayed through
  Void Hormiga's 2026-07-16 message §1). The protocol itself shipped as a
  draft the same day (`voidmaiz/widget.hpp` — see
  [widget registry](/concepts/widget-registry.md)) and is deliberately
  toolkit-agnostic at the CONTRACT level: projection in, commands out, one
  staged commit per gesture. **Lean, strongly: the ImGui-composed kit is the
  one sanctioned path**, matching Hormiga's own lean — real Qt fights
  vendor-don't-depend (ground rule 5), the one-render-loop reality, and the
  proven NDK/APK build; the "traditional desktop feel" Hormiga wants is a
  styling and completeness problem in the kit, not a toolkit problem. If the
  author ever sanctions a wrapped toolkit, that decision should arrive WITH
  its client, and the adapter design (event-loop bridging, focus and staging
  across the foreign/ImGui boundary) gets done then — the contract's shape is
  the invariant an adapter must satisfy, and nothing in the draft blocks it.
  Until then no bridging machinery ships (the T3 "Qt widget adapter" backlog
  entry stays parked behind this question).

- **Q13 — how is a [surface census](/concepts/surface-census.md) triggered?**
  Tier 0 of the census reads a live core plus the live ImGui window tree, so
  *something* has to be running. Three shapes: (a) an in-process **action** —
  the host registers "census" in its `ActionRegistry`, and a menu item, a CLI
  verb, and an agent all fire the same one (dogfooding [canvas
  actions](/concepts/canvas-actions.md)); (b) a **`--census` flag** that boots
  the app, submits exactly one offscreen frame, writes the state document and
  exits (CI-friendly, no human); (c) **continuous** — the census rides the log
  sink all session and flushes at exit (the only shape that gets tier-3 flow
  for free, and the only one that makes the doc a function of what the user
  actually did). **Lean: (a) + (c), and (b) falls out.** (a) is three lines in
  a host and is the honest framing — documenting yourself is a gesture, so it
  is a command; (c) is the log sink the host already installs; (b) is then
  just (a) fired from a headless frame. Nothing here needs deciding before the
  first implementation — but the ANSWER decides whether a doc describes the
  app's capabilities (a/b) or a session's behavior (c), which is a real
  difference in what the OKF ends up meaning.

- **Q14 — does a census bundle stand alone, or merge into the app's own OKF?**
  A harvested `surfaces/` tree is machine-written; an app's `okf/concepts/` is
  hand-written design truth, and the two must never fight for the same file.
  **Lean: the app's own bundle, in a conventional subtree** (`okf/surfaces/`,
  everything tagged `generated`), linked FROM hand-written concepts but never
  linking INTO them by generation — one bundle to study in FaultSack, one
  `validate` run, zero collisions. The regeneration hazard is already solved
  structurally (harvest in `content.body`, human prose in `content.notes` —
  and Void Core **shipped** the engine half 2026-07-24), so the remaining
  question is only WHERE the tree lands. A separate bundle would be safer still
  and is the fallback if the author wants machine output fully quarantined from
  authored knowledge.

  *(Corrected 2026-07-24, per FaultSack: this said "reserved subtree", which is
  false. The OKF engine's `RESERVED` set is exactly `{index.md, log.md}` — two
  filenames, nothing directory-shaped. `okf/surfaces/` is ours by convention
  only, and nothing needs negotiating with Void Core to use it.)*

- **Q15 — retarget node moves from `setjson pos` to `place`? (Core 0.2.5 has
  landed.)** This is the sign-off [total observability](/concepts/total-observability.md)
  has been waiting on since 2026-07-09, now ripe: *"upstream's reply endorsed
  positions-as-content in the main slice FOR NOW and proposed a `place` verb
  writing the reserved `rune.placement` field — logged, attributed, persisted,
  **outside the undo slice** — pending the author's sign-off. Design rule until
  then: view writes must be distinguishable commands, so the gesture compiler
  can retarget them to `place` the day it lands."* Core's 2026-07-24 message
  says the day came: **0.2.5 ships `place` + the view slice, and positions in
  `rune.placement` are now "the sanctioned pattern rather than a suggestion."**

  The code is ready by design — we read `placement` first already
  (`src/project/project.cpp:320`), and every position write funnels through one
  function (`compile_move`, `src/gesture/gesture.cpp:63`, which emits
  `setjson <name> pos [x,y]`). It is close to a one-line change plus tests.

  **What makes it the author's call, not a refactor: it moves node moves OUT of
  undo.** Today Ctrl+Z pops a drag; after the retarget it never will (`place` is
  logged and persisted but takes no undo frame). That is the *point* — the
  original complaint was `undo` popping a *move* when the user expected it to
  pop a *note* — but it is a visible behavior change in three shipping apps.
  **Lean: retarget, and take the behavior change**, because it is the ruling the
  concept already anticipated, it matches the author's own save/quit/reload
  razor (positions must persist; they needn't be undoable), and staying on
  `content.pos` now means diverging from a sanctioned Core pattern. If the
  author wants drag-undo kept, the honest alternative is Core's proposed
  micro-undo tier, not leaving positions in content. **Also gated on this:**
  bumping our floor from 0.2.4 to 0.2.5 (0.2.6 adds `mantle rm`/`rename`).

# Decided

## By the author, 2026-09-20 (collaborative canvas: "i trust your leans")

All five leans accepted, with three rulings that sharpened them. See
[collaborative canvas](/concepts/collaborative-canvas.md) §0.

- **Q31 (how several people reduce one net): claims plus a crank, integrity check
  always on**, and the author reframed the rest: interaction nets are *"our
  strongest area"*, not a trap. Every concurrent-rewrite scenario has an answer.
  The shared-wire case is HVM2's wires-as-variables, and that research is
  **Palabra's to lead** (message sent 2026-09-20). IC will prototype the
  encoding as runes.
- **Q32 (Ctrl+Z in a session): a local history of compensating commands**, refused
  when someone has since changed what it would touch. Not built. Still to be
  decided together with Q15.
- **Q33 (presentational conflicts): yes, and softer than proposed.** *"i dont
  really like hard conflicts … i don't see an issue with last one wins."* Built as
  Palabra's read-time `JoinPolicy::Pick` on view-state fields
  (`presentational_joins()`), not as writes. Content still conflicts. A
  Lamport-ordered `Latest` was proposed to Palabra as the honest "last".
  **Also ruled: selection comes first.** *"whoever selected this specific port on
  the node first, is the one who is doing stuff with it"*, for agents too, who
  must claim before they assign. Built as `voidmaiz/claims.hpp`.
- **Q34 (IC's LAN transport): a Void Palabra companion target**, and *"we need to
  build some networking abilities for android. if it doesn't exist, it SHOULD."*
  The platform half is built here (`voidmaiz/lan.hpp`: interfaces, the join code,
  Android's multicast lock through JNI, opt-in). The transport is requested from
  Palabra. The IC APK now declares the four install-time permissions. The
  explicit LAN-only yes for IC is to be relayed with the first build that uses a
  real link.
- **Q35 (hand-shaped routes): per-port `content.route.<port>` stamped with the
  partner**, with reroute runes for fan-out graphs. Not built. `content.route.*`
  is already declared `Pick`.

## By the author, 2026-09-18 (networking)

- **Q30: does Void Maiz own networking, or the half of it an application
  shows? The half it shows, in full, and generalized.** The lean was accepted,
  with the author's own sharper framing: build *"any GUI application's
  networking component,"* not Hormiga's, *"meaning you might have to make
  significant alterations to how hormiga does it,"* and Void Maiz owns *"the
  visuals and the tags for networking, the highlights, and … how networking
  kinda fits with the registry, allomone, etc."* Stages A and B were built the
  same day ([networking](/concepts/networking.md)). Stage C (transport) belongs
  to Void Palabra and waits on it; nothing on the Void Maiz side does.

## Closed by events, 2026-09-04 (reported by Void Allomone)

- **Q24 — how far do we extract the merge? — ALL THREE STEPS HAPPENED.** Not
  answered; overtaken. Void Allomone pointed out that the question had outlived
  itself and that closing it was ours to do.

  - **(a) its own target** — done 2026-08-29, as the lean said.
  - **(c) a sibling repository** — done 2026-08-29, on the author's call, which
    **overruled our lean the same day we wrote it**. The lean was "no, and not
    yet"; the author said extract. Recorded plainly because a question doc that
    quietly drops its overruled leans is a worse record than one that keeps
    them.
  - **(b) the C ABI** — shipped, and this is the interesting one:
    `src/allomone_c.cpp` with `capi_smoke` in ctest, **still with zero
    consumers**. Our lean was *"yes eventually, but not on this evidence"*, with
    the stated trigger being a second project asking. No second project asked.
    It shipped because the extraction made it nearly free, not because the
    trigger fired.

  **What was right, and worth keeping:** the ORDER. The lean argued (c) without
  (b) helps nobody, and in the event both landed together, so the objection
  never got tested rather than being proven wrong. **What was wrong:** treating
  "zero consumers" as decisive about *whether* to build, when it was only ever
  decisive about *when*. Cost, not demand, is what actually moved this — the
  extraction changed the price of (b) from "a second ABI to keep stable forever"
  to "a file", and a lean that only weighs demand cannot see a price change.

  Their fourth point, **stratified derivation**, was never a question but a
  roadmap item, and remains one: `sentinel.hpp` runs Tarjan SCC and Kahn strata
  and reports `1 strata` because nothing produces more.

## By the author, 2026-07-13 (open-questions batch cleared)

- **Q3 — view state**: the working split stands (positions = model content in
  the undo slice; camera = `config set view.camera`, undo-exempt config tier;
  selection = local). **`place` verb endorsed** for when upstream lands it.
  Added charge: **abstract view state for the substrates to come** — the
  camera abstraction must generalize so a 3D pose (XR) or a non-euclidean
  viewport (NE) or a touch viewport all fit the same "session-meta, config
  tier, logged, undo-exempt" pattern rather than each inventing its own. The
  `view.*` config namespace is the seam; keep it substrate-shaped, not
  pixel-shaped. (See [substrates](/concepts/substrates.md).)
- **Q6 — the name**: **renamed Void Node → Void Maiz** (*maíz* = corn; an
  abstract Spanish term with little collision, matching Void Core's
  generic-root spirit). Family: **Void Maiz XR**, **Void Maiz NE**.
  Namespace `maiz::`, targets `voidmaiz*`. Terminology policy: **do NOT do a
  broad Spanish/corn rename of other terms** — the only genuinely generic
  term was the library name (now fixed); the pasta wire names
  (linguine/fettuccine) are deliberately distinctive and stay; Void Core's
  vocabulary (mantle/glyph/rune/spirit) is upstream's and not ours to rename
  (ground rule 4). Revisit only if a specific term proves confusing in use.
  (Latin-OS keeps its intended double meaning — "latinos".)
- **Q7 — fundamentals**: the Blender/LiteGraph matrix ([backlog](/backlog.md))
  is the reference and the **fundamentals are covered** — add-search,
  socket-type colors + connection validation, collapse, subgraphs/groups
  (as mantle-enter), properties panel, per-gesture undo, custom node bodies,
  on-node widgets. Confirmed gaps are T2 quality-of-life, not fundamentals:
  wire-selection/deletion, keyboard vocabulary, frames, reroute nodes,
  minimap. Keep the matrix honest as we climb.
- **Q8 — demo scope**: **all four planned demos are kept in mind**, not just
  InteractionCombinators — but they "just need to work well." The focus is a
  **solid Void Maiz library**; the demos are its test surface, driven by a
  **feedback loop: the author uses them, reports what feels wrong, that
  becomes the next backlog entry.** (See [substrates](/concepts/substrates.md)
  for the four-demo matrix.)
- **Q9 — interaction nets first-class**: **yes, first-class in the scene
  model, invisible in the minimal path** — AND the stronger directive: when an
  application builds on Void Maiz, it should **utilize the interaction-net
  structure wherever possible** (principal/aux ports, fettuccine rewriting,
  the reduce executor) rather than treating the graph as inert dataflow. The
  net is the point, not an option.
- **Q10 — Hormiga/web**: a **consideration, not the current focus.** Lean
  stands: protocol-level adoption (JS view over /state+dispatcher) now, WASM
  as research; pin conventions as conformance cases when a second
  implementation appears. *(Superseded for Hormiga specifically, 2026-07-15:
  the author founded **Void Hormiga** as a native C++20 client instead — no
  web shell, no protocol adoption. The web paths stay recorded as general
  options with no client; see [substrates](/concepts/substrates.md).)*
- **Q11 — 3D scene shape**: **extend** (easier) — and moot for the near term
  since all genuinely 3D-*interaction* behavior lives in the separate Void
  Maiz XR project anyway. `pos [x,y,z]` stays compatible today; no
  speculative types.

## By the author, 2026-07-13 (the library family — was Q#12, confirmed and extended)

- **Void Maiz XR is a separate library** (visualizing Void Core in three
  interaction dimensions; consumes `voidmaiz`+`voidmaiz_reduce`, replaces
  `voidmaiz_view`; repo when work starts).
- **Void Maiz NE (non-euclidean) is a separate library too**, deliberately
  back-burnered — no client soon.
- **Void Maiz's charter: "2D in nature"** — 2D euclidean *interaction* space
  on rectangular displays (pointer/touch), while free to use 3D *graphics*
  (the modern-2D-Mario principle: rendering dimension ≠ interaction
  dimension).
- The long-range registry lives in [horizons](/horizons.md) (library family,
  time-as-fourth-axis vocabulary, Latin-OS).

## By the author, 2026-07-13 (the substrates direction)

- **Visual-representation assumptions fixed**: an OS/filesystem, compute, *a*
  visual component — explicitly NOT a rectangular display, not 2D, not
  euclidean space ([substrates](/concepts/substrates.md)).
- **Planned targets recorded**: mobile GUI (Android), VR GUI (Quest 3, 3D
  geometry + haptics), block-language support (Hormiga's Blocks; Scratch-like
  Node Blocks demo); four-demo test matrix across the axes.
- **C++20 rule holds with a footnote**: Android/Quest build the same C++ via
  the NDK; platform shims (Java activity, gradle) are zero-logic scaffolding.
  The web is the one true boundary → question #10.

## By the author's direction summary, 2026-07-10 (`promptToStart.md`)

- **Graphics backend: OpenGL 3 + GLFW now; Vulkan later at most.** Renderer gets
  a thin abstraction seam so a Vulkan backend stays *possible*, but no second
  backend is built until a client needs it (answers founding Q1 and the
  summary's own Vulkan question).
- **Repo & namespace shape confirmed** (founding Q2): `include/voidmaiz/` +
  modular `src/` (`embed/`, `project/`, `gesture/`, `view/`, `face/`),
  `vendor/` with licenses, `examples/`, `tests/`; namespace `maiz::`; C++20;
  CMake mirroring Void Core's. Single-language — no multi-binding surface.
- **Build model: static library** + headers, embed layer usable standalone
  (founding Q5).
- **Gesture vocabulary kept; Blender UX favored over LiteGraph** (founding Q4,
  sharpened): search/add workflow, visual hierarchy, socket-type color coding,
  groups/frames/reroutes as candidates via the feature matrix (open Q7).
- **Pasta terminology for wire kinds**: linguine (auxiliary, directed, fan-out,
  typed) / fettuccine (principal, symmetric, one-to-one, rewrite-bearing);
  "ravioli" dropped — see [interaction connections](/concepts/interaction-connections.md).
- **The Rule Workshop is out of scope** — port-mapping protocol yes, mapping UI
  no; hosts build their own (same concept doc).
- **Widget strategy**: node-specific widgets + a registry/protocol for host
  widgets (CLI representation, tag awareness, undo participation, responsiveness
  hooks) — see [widget registry](/concepts/widget-registry.md).
- **First client: the InteractionCombinators demo** (a real desktop showcase at
  `../InteractionCombinators`, not yet created), with VLS-native as the second
  client — [roadmap](/roadmap.md) updated.

## By upstream (Void Core's 2026-07-09 reply), affecting our surface

- **Reduce**: consume the portable contract at `VoidCore/conformance/reduce/`;
  the C++20 port of the ~100-line runner bootstraps our executor's test suite.
  Ambiguities go upstream as minimal case requests.
- **Attribution**: implemented (0.2.3) — `config set actor`; we standardize the
  `kind:name` actor convention in our OKF.
- **Build against Void Core 0.2.3.** *(Since 2026-07-14: **0.2.4**, which
  adopted our two reduce findings — `swap` annihilation and internal-wire
  resolution as the contract default — as cases 11–14.)*

## Founding day, 2026-07-09, by the author

- **C++20** (VLS question #16 → decided; this project inherits the decision).
- **Separate project** outside VLS, in the Projects folder; own OKF; eventually
  its own dedicated agent.
- **Built on Void Core**, completely compatible; the dispatcher/CLI seam expanded
  until *everything* is logged ([total observability](/concepts/total-observability.md)).
- **Widgets-as-nodes is a founding feature**, carried from VLS #14.
- **Most future UI-bearing Void Core apps build on it**; FaultSack-class and
  ESP32-class apps are explicit exceptions ([scope](/concepts/scope-and-clients.md)).
- **Production order: recreate the existing (Python VLS) capability, better,
  piece by piece** ([roadmap](/roadmap.md) phases 2–4).
- **The pre-Void-Core draft is superseded** by this OKF; archived with critique
  ([references](/references/voidnode-draft-v1.md)).
