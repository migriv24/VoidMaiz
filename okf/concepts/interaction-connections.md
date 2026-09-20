---
type: Concept
title: Interaction connections
description: "The two wire kinds — linguine (auxiliary dataflow) and fettuccine (principal interaction wires) — the port model, the rewrite rule structure, and the port substitution morphism. The mathematics Void Maiz visualizes."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-10T00:00:00Z
---

Direction set by the author's 2026-07-10 summary (`promptToStart.md`): Void Maiz
is *the visual manifestation of Void Core*, and its mathematical foundation is
**interaction nets (Lafont)**. The distinction that matters lives in the
**connections**, not the nodes.

# The two wire kinds (pasta terminology, adopted)

The author proposed pasta terms as a working suggestion and invited better ones.
Verdict after sitting with it: **keep linguine/fettuccine** — they are short,
memorable, visually suggestive (flat ribbon = the wide "active" wire), and give
the two topologically distinct roles unmistakably distinct names, which
"edge/wire/link" overloading never would. **Drop "ravioli"** — a node is just a
node (the on-screen projection of a rune); inventing a term for something that
needs no distinction would be terminology for its own sake.

## Linguine — auxiliary connections

- Port-to-port dataflow/control connections (what every node editor has).
- Connect **auxiliary ports**; directional (output → input).
- Fan-out allowed (one output feeds many inputs).
- Type-checked by host conventions (glyph metadata declares port types; the UI
  enforces them at wire-drag time — the VLS pattern).

## Fettuccine — interaction wires

- Principal-to-principal connections: Lafont's **active wires**.
- Undirected (symmetric), strictly one-to-one — no fan-out, ever.
- Two nodes joined by a fettuccine form an **active pair**: they may rewrite
  (annihilate, commute, expand) according to the mantle's rules — executed by
  the core's Reduce, never by Void Maiz.

## An edge weight may be a VALUE (Void Core 0.2.14)

Neither wire kind changes, but one reading of a wire's `weight` does. When the
`to` end resolves to a rune whose glyph is `kind: "measure"`, SPEC §3.7.1 makes
the edge an **attribute assertion** — `player --[weight 5]--> speed` says the
player's speed is 5 m/s, with the unit on the measure rune. `SceneWire::is_value`
marks it, decided by the target alone because the direction is normative.

**The canvas labels an assertion; it never thickens one.** A strength and a value
are not commensurable, so a renderer that mapped both onto line weight would
claim 900 rpm is nine hundred times stronger than "supports, 1.0". Use
`value_label(scene, wire)`. Full reasoning:
[rune kinds and quantities](/concepts/rune-kinds.md).

The same release makes the **act rune** an explicit thing, and it is worth
noticing here rather than anywhere else: an act — a verb reified as a node with
typed ports for its roles, because an edge label can only carry a binary
relation — **is an interaction-net agent**. It is the structure this page has
described since 2026-07-10, arriving under a modelling name.

## The port model

- Every node has **exactly one principal port** (directionless, untyped).
- Every node has **zero or more auxiliary ports** (directed, typed).
- A node can carry both kinds of connection simultaneously; there are no "node
  types" for this — the wire kind is the distinction.

## Passive principal↔aux wires (draggable since 2026-07-13)

In the mathematics a wire may join *any* two ports; only principal↔principal
forms an active pair — everything else is a **passive wire**. Wiring an
agent's aux port into another agent's *principal* is how real interaction-net
programs are built (the author, from hands-on IC use: rejecting it "is what
made making an interaction combinators program so difficult"). So the drag
grammar allows it:

- principal ↔ aux compiles as a **linguine** (relation `0:j` when the
  principal feeds an aux input, `i:0` when an aux output feeds the
  principal); the projection already classified those relations as linguine.
- The principal is untyped (wildcard) and directionless; the aux end decides
  the compiled direction. No active pair forms — reduce only fires on `0:0`.
- **Principals are strictly single-occupancy** (a net port holds one wire
  end): linking a principal clears whatever wire held it — fettuccine or
  passive — in the same rewire batch. Aux-output fan-out remains a deliberate
  UI relaxation for dataflow hosts; nets being reduced shouldn't lean on it.
- Grabbing a principal that holds a passive wire detaches it for re-routing,
  exactly like grabbing an occupied input.

# The rewrite rule structure

An interaction rule is data (it rides in the state document as
`config.transform.reduce`; semantics are pinned by the upstream contract — see
below):

1. **LHS** — a pattern of nodes joined by fettuccine wires.
2. **RHS** — a replacement net.
3. **Port mapping** — the **port substitution morphism**: for every LHS port,
   whether it is **mapped** (becomes a specific RHS port), **dropped**
   (disappears; attached linguine are severed), or **promoted** (exposed on an
   aggregate node, for nesting).
4. **Conditions** — optional tag filters (`@filter`) gating applicability.
5. **Identity** — name/ID for logging, debugging, referencing.

The port substitution morphism is the load-bearing concept: it is what makes
graph rewriting generalizable rather than hard-coded per rule.

# Where the semantics live (not here)

Rewriting is **Void Core's job**. The executor contract is
`VoidCore/conformance/reduce/` — a language-neutral README, 25 pure-JSON pinned
cases (14 at first contact 2026-07-14, grown upstream since; 17 green here as of
2026-09-01), and a ~100-line Python reference runner. Void Maiz (or the host's compile
pipeline) ports the runner to C++20 and passes when it produces the same
canonical forms and error kinds; the Python implementation stays the oracle.
Ambiguities found while porting go upstream as minimal case requests (Void Core
asked for exactly this — we are the contract's first real consumer). The loop
works: our two 2026-07-14 findings from hands-on use — the γγ≠δδ asymmetry and
internal-redex-wire legality — were adopted in Void Core 0.2.4 as cases 11–14,
with our `swap` spelling taken verbatim and our permissive resolution promoted
to the contract **default** (strict locality became a per-case opt-in key,
case 09). Closed loops on a redex vanish, normatively — a host that wants
Lafont's loops-as-values counts them host-side. Our `fuse` rule kind remains a
Void Maiz extension (upstream: noted, not adopted until a second consumer).

Void Maiz's own responsibilities are strictly visual/protocol:

- **Render** the two wire kinds distinctly and enforce their topology at gesture
  time (no fettuccine fan-out; principals strictly single-occupancy; passive
  principal↔aux wires allowed).
- **Surface active pairs** (two principals joined = highlightable, steppable).
- **Carry the morphism as protocol** — types/structs describing rule LHS/RHS/port
  mappings so hosts and faces can build on them.

# The Rule Workshop is a holiday (explicit non-goal)

The author's call, verbatim in spirit: the UI for *editing* port mappings — the
once-brainstormed "Rule Workshop" widget — is **not part of Void Maiz**. No
clean, generalizable visual concept for port substitution morphisms exists in
any prior art (the author's intuition, and ours); a novel visual concept or
domain-specific ones would be required. So: Void Maiz provides the protocols,
functions, and capabilities for port mapping; each application builds its own
mapping UI (or ships pre-defined rules). This keeps Void Maiz the visual
foundation, not the rule-editing tool. The proving ground for a first
host-built rule UI is the InteractionCombinators demo (see
[scope & clients](/concepts/scope-and-clients.md) and the [roadmap](/roadmap.md)).
