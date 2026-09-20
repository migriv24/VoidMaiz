---
type: Concept
title: Rune kinds and quantities
description: "What Void Core 0.2.14's entity/act/measure kinds mean for a node-graph library: an act rune is an interaction-net agent, a measure rune is the one whose incoming edges carry values rather than strengths, and a quantity annotation is the first thing a glyph has ever said about a number that a widget can act on."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-03T00:00:00Z
---

Void Core 0.2.14 (2026-09-03) gave a glyph descriptor three new things to say:
what **kind** of thing its runes are (`entity` / `act` / `measure`), what a
number **is** (`kinds` on a field, `quantity` on a measure rune), and how a rune
**appears per modality** (`presentations`). All three are additive, all three
default to what Void Maiz already had, and all three are now read by
`project_scene`. This page is why each one matters to a library that draws
graphs, and — as importantly — where we declined to act on it.

# The argument is about arity, and the mechanism is ours already

> **An edge label can only ever express a binary relation.** *"Superman flies
> across the sky"* is at least ternary — an agent, an act, and a path — and
> there is no way to write it as one labelled edge without losing a participant.

The standard repair is to **reify**: make the verb a node with typed ports for
its roles and wire the participants into those ports. That is RDF reification,
and neo-Davidsonian event semantics, and — exactly — **an interaction-net
agent**, which is the thing this library has drawn since day one and which
`layout.edges` already stores with `i:j` port pairs
([interaction connections](/concepts/interaction-connections.md)).

So the `act` kind does not add a mechanism here. It **names** the one that was
already underneath, in vocabulary a modelling tool can use — which means a host
that has never heard of interaction nets can now reach for the right structure
by asking for a verb. That is the sentence worth keeping from the whole release:
*rune kinds gave the family a word for a node shape Void Maiz could already
draw, and asked for no new drawing.*

# What each kind changes on a canvas

| kind | what it is | what the canvas can do with it |
|---|---|---|
| `entity` | an explicit thing | nothing new — every rune drawn before 0.2.14 is one |
| `act` | a change; a relation with roles | the node with ROLES: its aux ports are participants, not dataflow |
| `measure` | a dimension something has an amount of | its incoming edges carry **values**, not strengths |

`SceneNode::kind` carries it, defaulting to `Entity` — including when no
descriptor was supplied at all, because a projection degrades rather than
blanks.

# The one that changes rendering: a weight may be a value

When an edge's `to` end is a measure rune, SPEC §3.7.1 says the edge is an
**attribute assertion** and its weight *is* the value:

```
player --[weight 5.0]--> speed          "the player's speed is 5 m/s"
```

`SceneWire::is_value` marks it, decided by the **target only** — the direction
is normative, an assertion has an owner and a dimension and they are not
interchangeable. The unit lives on the measure rune (`SceneNode::quantity`), so
formatting one takes a scene lookup, which is exactly the shape two hosts
diverge on. `value_label(scene, wire)` is the one spelling, for the same reason
`field_of` is.

**And this is a rendering rule, not just a data one.** A strength and a value
are not commensurable, and the canvas must not pretend otherwise: drawing
*900 rpm* nine hundred times heavier than *"supports, 1.0"* would be a lie the
renderer told on the model's behalf. So an assertion is **labelled at the wire's
midpoint, never thickened**. Void Core's §7 says the same thing about analytics
— a mantle holding both kinds of weight has no meaningful weighted degree — and
the visual half of that caution is the one Void Maiz owns.

Nothing is drawn for an ordinary weight. A strength has no canonical rendering
yet, and inventing one here would answer a question the canvas has not been
asked.

# A quantity is the first thing a glyph has said about a number that a widget can use

`{level, unit, min, max}` reaches a Scene from two places in the same shape, on
purpose: `SceneField::quantity` (from the glyph's `kinds` map) and
`SceneNode::quantity` (from a measure rune's own `quantity`). An application
that keeps a value in a field and one that puts it on an edge are describing the
same quantity and must not have to say it twice.

The payoff is in the [widget registry](/concepts/widget-registry.md). Until now
the only way to get a knob instead of a text box was `hints.editors` — a
**presentation** hint, host-private, written once per glyph per host. A quantity
is **schema**, so it can pick a default:

- a **bounded ratio** field is a magnitude with a true zero and a full sweep,
  which is what a knob draws honestly;
- an unbounded ratio field gets the drag-number, clamped by whichever of
  `min`/`max` exists;
- **interval** (a date, a temperature) gets neither. There is no true zero, so a
  sweep from `min` would draw a proportion that does not exist;
- **nominal** and **ordinal** are not numbers a drag control should touch.

**A declared `editor` always wins**, and that ordering is the whole discipline:
presentation outranks an inference drawn from schema, so a glyph author who has
said what they want is never overruled by what we guessed.

# Points and vectors, which is why this stops where it does

Void Core's `okf/concepts/quantity.md` carries the reasoning and we do not
restate it, but the consequence is ours: **values-on-edges is right for
ratio-scale vector quantities and wrong for points.** A date and a coordinate
live in an affine space — you may subtract two points and add a vector to a
point, but never add or scale two points — so they have position, not magnitude,
and a weight is a magnitude. Positions in particular were never candidates: they
are points, and they already have `placement`.

The rule for a host choosing where a number goes:

> **If a number is read by RULES that produce new structure, it belongs on an
> edge where the rules can see it. If it is read only by RENDERERS, it belongs
> in a field.**

Most numbers in most applications should stay in fields, forever.

# Presentation split: the canvas is a modality, and we are it

A descriptor used to answer two questions in one object — *what is this rune*
(schema: `fields`, `kind`, `kinds`) and *how does it appear* (presentation).
They are in bijection only while a rune has exactly one representation. 0.2.14
split them: `presentations` is a map keyed by modality that the core stores and
never interprets, and `canvas` is the conventional home for what a host has been
calling `hints`.

Void Maiz **is** the canvas modality, so `presentations.canvas` is our object,
layered over `hints` **per key** rather than instead of it. That choice has one
reason: a glyph author moves keys across one at a time, and an all-or-nothing
switch would make a descriptor holding both silently lose half its look. Canvas
wins where both speak; `hints` keeps working untouched where it does not — which
is every descriptor written before today, including every one in this repo's own
examples and tests.

The **rename is not done and should not be**: under the original intent the
*rendering* is what deserves to be called the glyph, and renaming across the ABI
would break every host at once. Void Core split under a new name and left the
naming open (their SPEC §12); we hold the same position.

# What we deliberately did NOT do

**We did not add `kind:<k>` to the filter bag.** It is the tempting change —
`filter_bag` already offers `glyph:<g>` alongside tags and the name, and
`kind:measure` would drop straight in. Void Core refused the same move for a
reason that binds us harder than it binds them: `kind:` is an ordinary
application namespace on the `what` axis, so an app that already tags
`kind:vegetable` would find the canvas filter box quietly matching runes it
never tagged. **One tag grammar filters every surface** is the
[widget registry](/concepts/widget-registry.md)'s clause, and the grammar is
Void Core's, not ours to extend. Kind-filtering is `ls --kind` and
`glyphs --kind`, which is where the core put it.

**We did not touch the reduce contract.** `signatures` maps glyph → aux-port
count and lives in the reduce spec; `kind` is a different key on the glyph
*descriptor*, and neither reads the other. γ, δ and ε keep meaning what
`reduce.hpp` says they mean — Void Core proposed those letters for the three
kinds and withdrew them on exactly this evidence, ε being an *eraser* (arity
zero, terminates a wire) and therefore close to the opposite of "a concept that
carries a value". The conformance cases are unchanged and still pass as stored.

**We did not delete a duplicate schema, because we never had one.** Void Core's
§8 step 2 asks hosts to read `fields` from `glyphs` instead of a hand-maintained
table. `project_scene` has read the descriptor's `fields` since the projection
existed — the library owns no field list at all, which is
[commitment 1](/index.md) doing its job rather than a virtue we practiced.

# Status

`current` (2026-09-03). `RuneKind`, `Quantity`, `SceneNode::kind`,
`SceneNode::quantity`, `SceneField::quantity`, `SceneWire::is_value` and
`value_label` are in `scene.hpp` and `project.hpp`; the canvas draws assertion
labels; `widget_field` takes its default from a quantity. Pinned in
`tests/project_smoke.cpp`, including a document reopened by a core that
registered nothing. What is **not** built: an act-rune role affordance (drawing
a verb's participants as roles rather than as generic aux ports), which is the
[backlog](/backlog.md) item this release opened. Sources: Void Core's
`MESSAGE_FOR_VOIDMAIZ_voidcore-0.2.14-rune-kinds-and-the-glyph-split-2026-09-03.md`,
SPEC §3.3/§3.3.1/§3.3.2/§3.3.3/§3.7.1, and their `okf/concepts/quantity.md`.
