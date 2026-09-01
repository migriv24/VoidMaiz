---
type: Concept
title: Surface census — the GUI documents itself
description: "Harvest a running Void Maiz app's surfaces (panes, glyph vocabulary, actions, widgets, observed flows) into concept runes in a doc mantle, and let Void Core's existing OKF engine produce the markdown. No new markdown writer, no rewrite of existing apps — the GUI becomes a projection into knowledge, the same way the canvas is a projection of the model. Draft 2026-07-24."
tags: [status:draft, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-08-07T00:00:00Z
---

> **NOT IMPLEMENTED — this is a contract, not a component.** `census.hpp` exists
> and says so in its own second line; there is no `src/census/`, no CMake target,
> and no test. Stated here because on 2026-08-07 another page cited the census as
> *"already written"* and built a roadmap item on it. `status:draft` and the
> absence of a `resource:` were technically honest and evidently not loud enough.
>
> Anything needing tier-0 harvesting **today** can read the same source the
> census would: `project_scene` is already handed the glyph descriptors, so a
> `SceneNode` carries its declared fields
> ([discovery §3a](/../../VoidAllomone/okf/concepts/discovery.md) does exactly this in three
> lines). The census is the right eventual home; it is not the only road, and it
> must not be cited as though it were a dependency.

Direction from the author (2026-07-24): *"I think we need an engine or something
in order to register UI and its shape, its flow, etc, into the OKF… this way we
can look at the conceptual behavior of the application separately from the GUI…
there's already some applications that are completely built with Void Maiz, so I
don't want to have to rewrite EVERYTHING to make this work."*

Two hard constraints ride in that sentence: **existing clients must not be
rewritten** (InteractionCombinators, NodeBlocks, Void Hormiga are shipping), and
the mechanism must stay **abstract** — Void Maiz serves canvases, timelines,
maps, block languages and eventually XR ([substrates](/concepts/substrates.md)),
so it cannot presume a pane-and-button GUI.

# The reframe: don't write markdown — Void Core already does

The obvious build is a markdown generator inside Void Maiz. It is the wrong
build. Void Core's **OKF engine** (`VoidCore/okf/components/okf-engine.md`, code
at `VoidCore/holidays/okf/`) already has three jobs, and job 2 is exactly this
one: **produce — mantle → conformant bundle**, proven lossless (19 concepts +
92 links round-trip identical). Its mapping is already the one we want:

| OKF | Void Core |
|---|---|
| Concept | rune, glyph `okf-concept` |
| Concept ID | `spirit.name` (and `/` splits into directories on write) |
| type | a `type:<value>` tag |
| description / resource / timestamp | facets what / where / when |
| body (markdown) | `content.body` (opaque) |
| link | layout edge `{from, to, relation}` |
| Bundle | mantle |

So the missing piece is **not** a writer. It is a **census**: something that
looks at a running GUI and emits the dispatcher commands that build those
concept runes. Markdown falls out of the existing engine afterwards.

```
running app  →  Census (harvest)  →  dispatcher commands  →  a doc mantle
                                                                  ↓  export_state
                                            holidays/okf produce  →  okf/surfaces/*.md
```

This is the same shape as everything else here, one level up: **the census owns
no truth** (it emits commands — ground rule 2), **documenting yourself is a
gesture and therefore a command** (ground rule 3, [total
observability](/concepts/total-observability.md)), and **the markdown is a
projection of a mantle**, not a file anyone hand-maintains
([views as projections](/concepts/views-as-projections.md)). Void Maiz ships no
markdown writer, and inherits validation, tag-filtering, round-tripping,
`analyze`, and FaultSack study for free — *vendor, don't depend* (ground rule
5), applied to knowledge instead of code.

It also answers an **open upstream question**. Void Core's `ui-ux.md` concept
(`status:planned`) asserts *"any Void Core application must describe its UI and
UX… formalizing HOW an app declares its UI/UX (a manifest field? a required
design page?) is open."* Void Maiz is the natural answerer for the GUI-bearing
half of that: a harvested `surfaces/` sub-bundle beside `app.md`. That goes
upstream as a **message, not an edit** (ground rule 4).

# The four harvest tiers — this is the "no rewrite" answer

Fidelity is a ladder, and **tier 0 already yields real documents from apps that
change not one line**. Each tier is additive; a host climbs only as far as it
cares.

## Tier 0 — free. Zero app changes.

Everything here is readable from a live `maiz::Core` and the live ImGui context:

- **`glyphs`** — the whole node vocabulary. Each descriptor already carries
  `label`, `fields`, and `hints`: `ports` (name/dir/type/principal),
  `editors` (which widget edits each field), `labels` (human field names),
  `color`, `face` size, `enter` (subgraph descent). That is a complete
  "what can exist and what can be edited" document, written by nobody.
- **`mantles` / `describe` / `links`** — the document structure the app builds.
- **`help`** — the verb surface the user and any agent share.
- **The live ImGui window tree** (`ImGuiContext::Windows`: names, dock node,
  size, position, visibility). This is the *shape* half — panes, docking,
  what is open — harvested without a single annotation. Reaching into
  `imgui_internal.h` is already sanctioned precedent (the DockBuilder default
  layout in `examples/canvas_window.cpp`).

## Tier 1 — one line each. What the host already built.

The host constructs these registries today; the census just gets a look:

- **`ActionRegistry`** — already has `manifest()` (name, label, doc, params,
  gesture). [Canvas actions](/concepts/canvas-actions.md) built the
  introspection surface *before* there was a consumer; the census is its second
  consumer, and the first one that writes it down.
- **`WidgetRegistry`** — the editor kinds available, including host-registered
  ones ([widget registry](/concepts/widget-registry.md)).
- **`FaceRegistry`** — which glyphs draw a custom body.
- **`AddPalette`** — what a user can bring into existence.

## Tier 2 — opt-in prose. Only what code cannot know.

A `SurfaceNote` supplies *intent* — why a pane exists, what a workflow is for.
Absent notes simply yield thinner docs; nothing breaks. **The census never
invents prose.** A harvested doc says what is there, never what it means.

Note prose lands in `content.notes`, **never** in `content.body` — see
"Regeneration" below. That split is no longer ours alone: Void Core shipped it
engine-side on 2026-07-24, and FaultSack now renders the two halves separately
(*"a study site that renders 'Author's notes' separately from generated body is
a materially better critique surface"*).

## Tier 3 — flow, OBSERVED rather than declared.

Hosts already install a log sink (`canvas_window.cpp:117`). Attribute each
dispatched command to the surface that emitted it — the canvas's `CanvasIO`, an
action's `run()`, the inspector, the command bar — and a session yields a real
**gesture → command → model effect** table. This is the answer to the author's
"its flow": not a diagram someone drew, but the transcript of what the UI
actually did, harvested. Total observability paying a documentation dividend —
the flow doc is `confidence:verified` in the literal sense, because it was
watched, not asserted.

# The standardization, kept minimal

The author asked to stay abstract. The whole convention is six items, and
**none of them require a change to the OKF engine**:

1. **No new glyph.** Reuse `okf-concept`; distinguish with the `type:` tag
   (`type:Surface`, `type:Vocabulary`, `type:Flow`, `type:Action`). The existing
   producer and validator work untouched — OKF's spec mandates tolerating
   unknown types.
2. **Names are paths.** `surfaces/canvas`, `vocabulary/glyph/osc`,
   `flows/place-contact`. `produce` already splits `/` into directories, so the
   tree is free.
3. **A tag vocabulary, all free tags:** `surface:pane|canvas|action|widget`,
   `generated` (bare, the way `reference` is used), plus the standard
   `status:` / `audience:` / `confidence:` axes. `confidence:` values stay
   inside the glossary vocabulary (`verified` / `asserted` / `exploratory` /
   `stale`) — the validator enforces that one.
4. **Links carry the flow.** `link surfaces/canvas vocabulary/glyph/osc
   --relation renders`, `link flows/place actions/place --relation invokes`.
   The bundle graph *is* the UI graph — and it is then studiable in FaultSack
   like any other bundle.
5. **Every generated concept carries a `resource:` where one exists** —
   the glyph concept points at the file registering that glyph, the action
   concept at its `ActionRegistry` entry, the pane at the source that draws it.
   **Non-negotiable, on FaultSack's evidence** (2026-07-24): `resource:` is what
   binds the UI half to the code half in their unified graph — *"without it a
   census is a well-formed subgraph attached to nothing"* — and it is also what
   keeps a generated bundle off the honesty rule, which fires on exactly
   `status:current` with no `resource:`. A census emits `status:current`
   concepts by nature (they describe a program that demonstrably exists), so
   without this a censused app opens in FaultSack on a wall of honesty warnings
   that drown the real findings. The library cannot know these paths — the HOST
   supplies them through one hook (`CensusOptions::resource_for`).
6. **Every generated concept carries a `timestamp:` set at harvest time.**
   FaultSack's whole freshness story keys off it, and drift detection is exactly
   `resource` mtime vs doc timestamp — so an undated census concept is invisible
   to drift forever, *"which is a shame given a census is the one kind of doc
   that always knows exactly when it was true."* Host-supplied, like the paths:
   the library takes no clock.

# Regeneration without clobbering prose

The real hazard: re-running the census must not eat hand-written commentary.
Marker-delimited blocks (`<!-- census:begin -->`) are the usual answer and are a
text-merge problem. **Because the truth is a rune and not a file, there is a
better one:** the harvested markdown lives in `content.body`, hand-authored
prose lives in a *different field* of the same rune (`content.notes`), and
`produce` emits body-then-notes. A re-census overwrites `body` and never touches
`notes`. **No text merging, ever** — a property we get only by routing through
the mantle instead of writing files directly.

**Built upstream, 2026-07-24** (asked and shipped the same day;
`MESSAGE_FOR_VOIDMAIZ_core-notes-field-and-ui-ux-2026-07-24.md`). Core took the
generalization — *"any produced bundle has a machine half and a human half, so
the fix belongs in the engine and not in every producer"* — and went one step
past the ask by building the mirror, so the round-trip is **field-level
lossless** rather than asymmetric. The shipped contract:

- **Separator `<!-- okf:notes -->`**, written by `produce` between the halves;
  invisible in rendered markdown. This is *not* the splicing we objected to —
  `produce` always rewrites the whole file from the rune, and the marker exists
  only so a reader (`consume`, or a human) can see where the machine stopped.
- **Recognized only on a line of its own** — because an inline mention of the
  marker inside prose split a page in half on the new test's first run. Census
  pages that document markers are exactly the case that would have hit this.
- **No notes ⇒ byte-identical output**; **no marker on read ⇒ the whole file is
  body** (permissive consumption). There is no malformed case that fails.
- **Notes participate in the graph** — markdown links inside `notes` are
  extracted like any others, so a human's cross-reference from a surface page
  shows up in `analyze`, in `validate`'s link lint, and in FaultSack.
- **`notes` is declared on the `okf-concept` glyph's field list**, so authoring
  is the ordinary `set <concept> notes "…"`. **No core change, no dispatcher
  change** — `set` already writes arbitrary content fields.

So the census's human half is a solved problem before the census exists.

# The boundary

- **Void Maiz owns:** the harvest, the concept-id/tag convention, and the
  compiled commands. It ships C++ that emits command strings and a state
  document — nothing else.
- **Void Core owns:** the markdown, the validation, the bundle — unchanged apart
  from the `notes` split, which shipped 2026-07-24 and is theirs, not ours.
- **Nothing is "reserved" for us.** The engine's `RESERVED` set is exactly
  `{index.md, log.md}` — two filenames, nothing directory-shaped (FaultSack's
  correction, 2026-07-24). `okf/surfaces/` is a **convention**, not a guarantee;
  saying otherwise would be a `status:current` claim the engine doesn't back —
  the exact drift our own validator flags. (Same footnote: `app.md` isn't
  reserved either, so a bundle's manifest loads as an ordinary concept *and* is
  read by `read_manifest` — it will show up as a study card.)
- **Nobody owns:** intent. The census reports; the author interprets. A doc that
  claims to know why a button exists would be the exact drift the OKF honesty
  convention exists to prevent.
- **Not in scope:** screenshots, a visual sitemap, a diffing "UI changed"
  watchdog. All are downstream of a bundle that exists, and cost nothing to add
  later.

# Status

**Draft.** Contract sketched in `voidmaiz/census.hpp`; not implemented. The
forcing client is whichever shipping app the author points at first —
InteractionCombinators is the smallest, Void Hormiga the most demanding (four
heavy workflows, a table view, a Territory map with its own action registry).
Open decisions in [developer questions](/developer_questions.md) Q13 (harvest
trigger — in-process action vs a `--census` headless frame) and Q14 (where a
census lands: the app's own OKF, always).

**Broadcast 2026-07-24** to three agents (see [index](/index.md) §Inter-agent
messages): Void Core (the `produce` → `content.notes` ask, plus our answer to
their open `ui-ux.md` question), Void Hormiga (their `ActionRegistry` is the
richest census input that exists — register actions, fill in the docs), and
**FaultSack**, which had never heard of Void Maiz at all. FaultSack is the
*reason* this concept exists: the author wants to annotate UI in it, and its
developer notes key off any node id — so a census bundle makes UI annotatable at
concept granularity immediately, with Allmusely still the answer for
widget-level annotation.
