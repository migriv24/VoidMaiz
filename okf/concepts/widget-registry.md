---
type: Concept
title: Widget registry
description: "The widget strategy: Void Maiz ships node-specific widgets and a registry/protocol through which hosts integrate their own — CLI representation, tag awareness, and undo participation as the contract. Protocol drafted 2026-07-16 (voidmaiz/widget.hpp) for Void Hormiga, the forcing client."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

Direction set by the author (2026-07-10, `promptToStart.md` §6), refining the
founding widgets-as-nodes feature (VLS #14). The line: **Void Maiz provides
node-specific widgets and an integration path; general-purpose widgets are the
host's responsibility.**

# What Void Maiz provides

- **Node rendering** — boxes, ports, wires, faces.
- **Canvas interactions** — pan, zoom, marquee selection.
- **Node window chrome** — collapse/maximize, face renderers.
- **The gesture→command compiler** — select, move, wire, add, delete, field edit.
- **Default node widgets** — the widgets nodes themselves need for field editing
  on faces: sliders, drag-numbers, combos (with the VLS staging discipline).
- **The widget protocol + registry** — the contract a widget implements to gain
  Void Maiz capabilities, and the place hosts register/discover/instantiate them.

# What Void Maiz does NOT provide

- General-purpose widgets (buttons, text inputs, panels, dropdowns) — unless a
  node face specifically needs them.
- The Rule Workshop (see [interaction connections](/concepts/interaction-connections.md)).
- Application-level UI (menus, toolbars, status bars).

# The contract (what registration buys)

A registered widget — custom-drawn, ImGui-composed, or a wrapped Qt widget —
must satisfy, and in return gains:

1. **CLI representation** — every interaction with the widget compiles to a
   dispatcher command ([total observability](/concepts/total-observability.md)
   extends into widgets; a widget that mutates silently is disqualified).
2. **Tag awareness** — widgets can be tagged and filtered (`vc_tag_match` at
   render time, the core's one filter grammar).
3. **Undo participation** — widget interactions are model commands (or view
   commands, per the lasagna tiers) and ride the core's undo like any gesture.
4. **Responsiveness hooks** — the interface exposes graph-state feedback (e.g.
   data from Scry projections) so visuals can react: centrality-aware glow,
   activity heatmaps, sliders that restyle by the data they control. These
   *examples* are host-level; Void Maiz supplies the hooks and the surface.

# The host's three paths

1. **Use ImGui directly** for its own panels — no registration, no Void Maiz
   involvement (and no CLI/tag/undo integration either; that is the trade).
2. **Wrap existing widgets** (Qt, etc.) via an adapter implementing the protocol.
3. **Build custom widgets** from scratch against the protocol, with fully
   dynamic visuals.

The far-horizon "Qt Designer / Figma on Void Maiz" vision is a **host
application** built on this registry — never part of the library.

# The protocol, drafted (2026-07-16 — `voidmaiz/widget.hpp`)

Void Hormiga volunteered as the forcing client (its Data section: forms,
detail panes, wizards, tables — all "traditional desktop UI that is still
CLI-complete"), and the draft shipped. The three deferred engineering
questions, answered:

**The minimal protocol surface is the context, not an interface.** A widget
is a function receiving `WidgetContext` — the projected `Scene` (read), a
`commands` vector (write), the active tag-filter expression, a width hint —
and drawing with ImGui. Draw and hit-test are ImGui's; command emission is
pushing compiled lines into the context (the host dispatches and re-projects,
the one-sync rule — `edit_canvas`'s seam, widget-sized); staging is the
widget's own duty under the VLS #14b discipline (stage locally, flush ONE
command per gesture, abandon compiles nothing). Contract clauses 1 and 3
(CLI representation, undo participation) fall out of the shape — a widget
*has no other way* to change anything. Clause 2 (tag awareness) is
`node_matches(filter, node)` over the **one filter bag** — tags + name-as-tag
+ `glyph:<g>` — now fixed library-side (project.hpp) so every surface (canvas
filter box, table, host widgets) matches identically. Clause 4
(responsiveness) is re-projection itself.

**Registry entries bind to glyphs through field declarations.** The
registrable unit is the FIELD EDITOR: a glyph's `hints.editors` maps a
declared field key to an editor spec (`"date"`, `"number:0.1,0,10"`,
`"combo:a,b,c"`, or a host kind like `"color"`); the projection surfaces it
as `SceneField.editor`; `WidgetRegistry` maps kind → renderer. One
registration makes the editor appear on EVERY field surface — the inspector
(now a registry client itself), node faces, `widget_form` detail panes, and
the table's cells when it lands. The library kit ships as
`WidgetRegistry::defaults()`: text, number, multiline, combo, date, and knob
(VLS-native's rotary, absorbed 2026-07-16) — one implementation each
(widget.cpp); the face widgets are thin frames over the same cores. The
kit's disabled-when-wired check matches an input port named `<key>` OR
`p:<key>` — the params-as-ports convention proven in the Python VLS (#14a),
blessed library-wide 2026-07-16. A missing/unknown spec falls back — first through the field's
QUANTITY, then to staged text — and a declared field is never uneditable.

**A quantity annotation may pick the editor a hint did not declare** (Void Core
0.2.14; [rune kinds](/concepts/rune-kinds.md)). `hints.editors` is
*presentation*: host-private, written once per glyph per host. A glyph's `kinds`
map is *schema* — `{level, unit, min, max}` per field, surfaced as
`SceneField.quantity` — and it says enough to choose honestly: a **bounded
ratio** field is a magnitude with a true zero and a full sweep, so it becomes a
knob; an unbounded ratio field becomes a drag-number clamped by whichever bound
exists; **interval** (a date, a temperature) gets neither, because with no true
zero a sweep from `min` draws a proportion that does not exist; nominal and
ordinal are not numbers a drag control should touch. The inference runs strictly
AFTER the `field.editor` branch, so **presentation always outranks schema** and
a glyph author who said what they wanted is never overruled by what we guessed. Widgets that aren't field-shaped (domain buttons,
drop zones, whole panes) skip the registry: implementing the context shape by
hand IS speaking the protocol. Faces keep their own per-glyph registry
(`FaceRegistry`) — a face is a whole-body renderer, not a field editor.

**Registration is per registry object, host-owned** — like `FaceRegistry`:
no globals, no per-manager coupling (widgets are view config, re-established
by the host at boot exactly like glyph registration). Typically one per
application, handed to whichever surfaces render fields.

The kit is **ImGui-composed** (Hormiga's lean and ours; the author's ruling
on ever sanctioning a wrapped-toolkit adapter is
[developer questions](/developer_questions.md) Q12). The contract is
toolkit-agnostic on purpose: an adapter would have to fit the same door —
same context, same one-command commits — but no bridging machinery ships
until a sanctioned client needs it.

# The kit's first bruises (2026-07-18 — Void Hormiga, forcing client)

Hormiga built its Data section against the draft (forms + detail panes
through `draw_inspector` with a registry) and reported four edges; all closed
the same session. Each generalizes cleanly — none is Hormiga-specific:

- **Per-field labels.** A form must not show raw field keys (`text_en` where
  a volunteer reads "Text (English)"). `hints.labels` — a glyph-declared
  key→label map, twin to `hints.editors` — surfaces as `SceneField.label`,
  and the kit shows it with the control's ID kept keyed on the field key
  (`"label##key"`, so relabeling never resets a live edit). Absent/empty →
  the raw key (unchanged). ONE declaration lights every surface, the same
  payoff as editors. Node faces pass no label (inline chrome); the registry
  threads it.
- **The `path`/file kind.** `widget_field_path` = a staged text box (edit by
  hand, ONE `set`, like text) plus a "…" browse button. The **library owns
  the box and the commit; the host owns the OS file dialog** — a
  `PathBrowseFn` callback returns the chosen path, the widget commits ONE
  `set`. It is NOT a `defaults()` entry (no portable dialog ships — the same
  boundary as `face_image`, which hands an image click back to the host);
  `WidgetRegistry::add_path(browse)` registers it in one line, and then file
  pickers appear on faces, forms, and future table cells uniformly. The
  host-owns-the-world-facing-half rule (save/effect handlers, textures)
  extends to file dialogs.
- **The bool/checkbox kind.** `widget_field_bool`, registered under `"bool"`
  and `"checkbox"`. Unlike the staged editors, a checkbox click is atomic —
  no local stage to abandon — so it commits ONE `setjson <key> true|false`
  immediately. Disabled-when-wired like every field editor.
- **The multi-field batch commit helper.** `maiz::compile_commit(commands)`
  (gesture) collapses a set of compiled commands into the one thing to
  dispatch — "" / the lone command / a `batch` — the single/many/none
  discipline `compile_moves`/`compile_deletes` already used, exposed for
  hosts' compound flows (a wizard's finish, a `+ New` that mints a rune and
  tags it) so they land as ONE undo frame.

Deliberately still open: number formatting/units editor args (Hormiga hasn't
needed them). And a working note against the one filter bag: `node_matches`'s
"malformed → matches all" default is right for a filter box (a mid-keystroke
typo must not blank the view) but wrong inside a *query block* (a typo'd query
should match nothing); hosts guard that host-side today, and it becomes a
library concern only if the filter grammar grows a strict-parse entry point.
