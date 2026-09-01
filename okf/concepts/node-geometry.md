---
type: Concept
title: Node geometry
description: "The node's shape is notation, not decoration: glyphs can declare geometric bodies (triangles, circles, polygons) with ports anchored on the perimeter and auto-orientation toward the principal partner. The window-chrome rect stays the default."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-11T00:00:00Z
---

Prompted by the author (2026-07-11), from using the InteractionCombinators
demo: *"interaction combinators uses triangles for gamma and delta, and
circles for epsilon… the actual geometric transformation kinda matters for
the user experience… I learned to DRAW the stuff: when gamma and delta point
to each other and interact, we get 4 triangles facing away from each other,
with their ports connected in a criss-cross way."*

# The principle: shape is notation

In some domains the node's shape carries the semantics. Interaction-net
notation is the proving case: the triangle's **apex IS the principal port**,
"two agents point at each other" is how a redex is *read*, and a rewrite's
correctness is checked visually by how the resulting shapes face. Rendering
every agent as a titled window rectangle erases the notation the user thinks
in. So geometry joins color and ports as something a **glyph declares** —
semantics from the host, rendering from the library, exactly the existing
hints contract.

# The abstraction (generalizable, not an enum)

A node body is one of:

- **Window** (default): today's rect — header, collapse dot, resize grip,
  fields, faces. Unchanged.
- **Polygon(n)**: a regular n-gon inscribed in the node's box, with a
  designated **principal vertex**. `triangle` = Polygon(3).
- **Circle**: the degenerate/smooth case (ε's notation).

Ports anchor on the **perimeter as a function of orientation**:

- The principal sits AT the principal vertex (polygon) or at angle θ on the
  circumference (circle).
- Auxiliary ports 1..n distribute along the **opposite feature** — the base
  edge for a triangle, the opposite arc otherwise — in index order, so port
  order stays app knowledge made visible.

# Orientation: `rot: "auto"` is the payoff

A shape has a rotation θ. Declared `"rot": "auto"` (the default for shaped
nodes), **the body rotates so its principal points at its principal-wire
partner**; a free principal points up (or at a declared angle). This single
rule makes the notation self-maintaining:

- Two agents wired principal-to-principal literally **point at each other** —
  the redex is visually a collision about to happen.
- After a γδ commute, each copy's principal attaches to an *outward*
  boundary port, so the four triangles **face away from each other** with the
  criss-cross grid between their bases — the textbook picture, emerging from
  the wiring with no layout code knowing the rule.

Rotation is view-derived (a pure function of the wiring), so it needs no
model field; a future explicit `rot` content field (for free-principal
posing) would ride the same view-state-as-content tier as `pos`.

# Boundaries

- Shaped nodes are **compact notation bodies**: no header/dot/grip/fields/
  faces; the name centers in the body; tags render small beneath. Window
  chrome remains the home of faces and field widgets.
- Hit-testing is the true shape (point-in-polygon / radius), not the box;
  selection is an outline stroke; move/marquee still operate on the box.
- The hints convention (host-declared, zero core involvement):
  `"shape": {"kind": "triangle"|"circle"|"polygon", "sides": n, "rot": "auto"|degrees}`.
- **"Does it have to be a shape?"** (the author's horizon question): the far
  end of this axis is glyph-supplied custom bodies — which is the face API's
  territory (a face renderer already draws arbitrary pixels and takes input).
  If notation ever needs non-geometric bodies (images, SDFs, text-as-node),
  it should arrive as a face variant, not as more shape enum — recorded as a
  T3 backlog line, not designed yet.
