---
type: Concept
title: Canvas actions as first-class commands
description: "A custom view's interaction vocabulary declared as NAMED, param-schema'd, introspectable actions — one host-supplied compile serving both a canvas gesture and a CLI/agent verb, so a click and a command are the same transcript entry. Draft 2026-07-21 (voidmaiz/action.hpp) for Void Hormiga's Territory map."
tags: [status:draft, audience:dev, confidence:asserted]
timestamp: 2026-07-21T00:00:00Z
---

Direction from Void Hormiga's fifth message (2026-07-21): its Territory map is
a **pseudo-GUI inside a canvas** — you pick a *tool* and take *actions* (place,
move, draw-region, select-in-radius, measure). Read "map" as one instance; the
generalization must also serve timelines, diagram/patch editors, a fantasy-map
tool — any future canvas app.

# What was already right (the WRITE side — confirmed, not rebuilt)

The [custom-view seam](/concepts/views-as-projections.md) + [total
observability](/concepts/total-observability.md) already make a view's writes
first-class. A map on the seam emits ordinary Core commands — a marker drag is
`set <rune> geo "lat,lon"`, a place is `rune new <glyph>` + `set geo`, a region
is a rune with a shape facet — all logged, undoable, replayable, and **an agent
reproduces them through the very same verbs**. Positions were always model
content ([log-first inheritance](/concepts/log-first-inheritance.md)); geo is
just another facet. So the map's actions are *already* in the transcript. There
is no new write path to build.

# The gap: the vocabulary is not legible

What the seam does NOT make legible is a view's **interaction vocabulary**. A
custom view today hand-rolls all its gesture handling and emits raw commands
**anonymously** — nothing can answer *"what does this canvas afford?"* The
built-in gesture compilers (`compile_move`, `compile_add`, `find_snap`,
`compile_block_release`) are the existence proof that gestures→commands can be
first-class; Hormiga asks for the **host-extensible** version.

# The shape: an ActionDescriptor (draft — `voidmaiz/action.hpp`)

A view registers **named actions**; the library owns naming, enumeration, and
one run() entry point; the host still owns the drawing, the gesture detection,
and what each action MEANS. No truth moves into the library.

- **`ActionDescriptor`** = `{ name, label, doc, params[], gesture, compile }`.
  `params` is the parameter **schema** (`{name, type, required, doc}`, `type` a
  host/agent convention like `"glyph"`/`"geo"`/`"node"` the library surfaces but
  never interprets). `compile(scene, args) -> command line(s)` is the host's —
  **the library never invents commands** (the total-observability rule); an
  empty return declines. `gesture` (`"click"`, `"drag-region"`) is a
  view-interpreted hint, surfaced for discovery — **the library never
  dispatches a gesture** (that stays the view's, like `EditorState`).
- **`ActionRegistry`** = host-owned like `AddPalette`/`WidgetRegistry` (no
  globals). `run(name, scene, args)` looks up + compiles + returns commands (the
  host dispatches and re-projects — the one-sync rule). `manifest()` emits a
  stable JSON list of the actions and their schemas: the machine-legible half,
  what a host and through it an **agent** reads to discover the vocabulary and
  invoke each by name.
- **UI-free on purpose** — descriptors are data + a `Scene→strings` function, so
  the base `voidmaiz` library carries them; an agent/CLI host enumerates and
  invokes with no ImGui present. The VIEW binds gestures to `run(...)`.

# The prize: one definition, two front-ends (Hormiga §3)

The point of naming actions is that **one definition drives two front-ends**:

- a **canvas gesture** (Void Maiz) — click-to-place, drag-a-region — the view
  detects the gesture, assembles `ActionArgs`, calls `run(...)`;
- a **CLI / voidscript verb** (Void Core) — `map place contact @here` — which
  parses tokens into the same `ActionArgs` and calls the same `run(...)`.

Both hit the SAME `compile`, so a volunteer's click and an agent's command are
the *same logged dispatcher command(s)* — one transcript entry, not two
implementations that drift. The `ActionDescriptor` is that single definition.

# The boundary with Void Core — asked, and ANSWERED (2026-07-21)

The gesture front-end and the descriptor are Void Maiz's. The **CLI verb** and
the **query** side are Void Core's — asked in `MESSAGE_FOR_VOIDCORE.md` and
answered in `MESSAGE_FOR_VOIDMAIZ_core-reply-2026-07-21.md`. Core blessed both
as the *same* seam shape (*"Core owns the seam + calling convention; the host
owns the domain computation; the output rides the observable spine"*):

1. **Host-registered CLI verbs → "verb macros" (compile-to-`batch`).** A host
   registers `{verb-name, arg-spec, compile(argv) -> [command lines]}`; the core
   parses, calls the host's `compile`, and dispatches the result **as a `batch`**
   — atomic, one undo frame, attributed, logged, replayable by construction. The
   host's `compile` is the SAME one the gesture calls: one definition, two
   front-ends. **Available today in semantics:** a host command bar that calls
   `batch` with the compiled lines is already the real spine (not a fake); verb-
   table *registration* is the only delta, buying discoverability (`man`/`?`),
   voidscript composability, and parse-once. **Hard rule:** a verb macro emits
   model mutations → pure → undoable → a `batch`; it must **never** route through
   the effect seam (`effect <op>` is for effectful host I/O and is not
   snapshot-undoable). `place` = verb macro; "export tiles to disk" = effect op.
2. **Host-registered query predicates → pluggable `where`-predicates in
   Scry/`ls`.** A host registers `name -> predicate(rune, args) -> bool`; the
   `where` evaluator calls it **alongside** the tag grammar, so it composes
   (`ls --near "lat,lon" --radius 500 AND --tag "contact"`). Core confirmed the
   tag grammar genuinely can't do it (a pure fn over a bare tag bag; a spatial
   test is over a rune's `geo` facet — a different shape, correctly Core's).
   **Interim today:** `effect query "<expr>"` routes a whole query host-side and
   returns runes as `data` — works now, but can't compose inside `where`.
   Boundary: Core owns registration + calling convention + `where` composition;
   distance math and lat/lon types stay host-side (the quarantine holds on both
   sides — no geo type in Core either).

**Design consequence for this header — keep the param-schema and the verb
arg-spec ONE type.** Core recommended it explicitly: their planned voidscript
`def` macros and these verb macros will share the arg-spec + expansion model,
and *"if your action descriptor already carries a param-schema, that schema is a
strong candidate to BE the verb's arg-spec."* So `ActionParam[]` is authored as
the single schema that drives the gesture, the manifest, AND (when Core lands
verbs) the verb arg-spec — never forked. No build waits on Core; when Hormiga's
map is the forcing client, Core will draft the concrete verb/predicate API
against this descriptor type so the two are one from day one.

# What stays out of the library (substrates' quarantine)

The map view, geographic projection, base-map source, tiles, marker drawing,
and rasterization are all the host's. No map/geo type enters Void Maiz — the
geographic camera (2026-07-20) is the only geo-shaped thing and it is just a
viewport ([substrates](/concepts/substrates.md)). `ActionDescriptor` is
domain-agnostic: it carries strings and a compile function, nothing spatial.

# Status

**Draft.** Void Hormiga is the forcing client (as it was for the widget
protocol) and will build its Territory tools against `action.hpp` and report
the bruises. Open until then: whether the view wants library help *routing* a
completed gesture to an action (today the view calls `run` itself), and the
final `type` vocabulary for params (kept open — a convention, not an enum).
