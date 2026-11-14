---
type: Concept
title: The overview — a graph too large to draw, drawn in bounded work
description: "voidmaiz/overview.hpp (2026-11-05, Void Hormiga's ask): a tree of groups over a graph (Leiden, split to a leaf size, merged upwards under a size cap, the unlinked bundled, the too-wide sliced), a cut for what is open, links lifted to the bodies drawn, and a Barnes–Hut force step laid out per connected piece and packed. UI-free and deterministic."
resource: include/voidmaiz/overview.hpp
tags: [status:current, audience:dev, audience:library, confidence:measured]
timestamp: 2026-11-05T00:00:00Z
---

# Why

A force-directed picture of every node stops being a picture long before it
stops being affordable. Void Hormiga's connections view held 59,719 runes when
its stress database opened (2026-11-04), and its all-pairs repulsion cost
3.6×10^9 steps an iteration. The screen would have shown a hairball anyway. The
view must cost what it **shows**, and it must show something a person can
read: Shneiderman's *overview first, zoom and filter, details on demand*,
done by hierarchical aggregation (Elmqvist & Fekete, IEEE TVCG 2010).

This is a view library's job, not a host's, and not the database's. It needs a
graph, not a store. The database's part (summaries of its own clusters, for
an overview of what is NOT loaded) is Void Rancho's.

# What it is

**The tree** (`build_tree`). Ids below `items` are the graph's nodes, and the
ids above are groups. It is built in four steps:
1. **Communities**: Leiden (`voidmaiz/graph.hpp`, Rancho's), split recursively
   until each group opens onto at most `leaf_size` things.
2. **Merged upwards** along the strongest ties per member, under a size cap of
   max(largest community, 4·n/roots), until at most `roots` bodies show with
   nothing open. *Not* by lowering Leiden's resolution: that is a true
   partition and fused a 6×10^4-node part into one group of 56,611 and a ring of
   crumbs.
3. **Bundled.** Modularity never merges what nothing links, so what is still
   above `roots` goes into packing groups (`Tree::Bundle`): the largest stand
   alone, the rest share.
4. **Sliced.** A group still wider than `leaf_size` is cut into `Tree::Slice`
   groups.

A view can say which groups mean something (`Community`) and which are only
packing. A group of one is its member. A graph of at most `roots` nodes gets no
groups.

**What is open** is view state: `cut(tree, open)` gives the bodies to draw,
`body_of` which body each node is inside, and `lift` the links between bodies,
summed.

**The layout** (`layout_step`). Fruchterman–Reingold forces with Barnes–Hut
repulsion (a quadtree, θ = 0.9), O(n log n + m) an iteration. Mass is a body's
weight: a heavy body pushes harder and moves less. Given `components`, bodies
repel only within their own connected piece, and `pack` then places the pieces
side by side, the largest in the middle and the rest in rings round it.
Without that, a crowd of small unlinked pieces is pushed by everything else
into a ring far from the picture. No body moves further than the step a host
passes, and a host cools it.

# What it is not

- **Model state.** Positions are ephemera, like a camera, and nothing is
  dispatched. Contrast the node editor's relax gesture (`gesture.hpp`), whose
  output is positions as model content.
- **Drawing.** It holds no ImGui types; the host draws.

# Measured

`tests/overview_smoke.cpp`:
- a ring of 40 cliques of 50 gives 20 roots, no clique split, and the lifted
  weight equal to the crossing weight;
- 10^4 unlinked nodes give 100 roots, 50 of them bundles;
- one iteration over 3×10^4 bodies takes 28–49 ms (all-pairs: 9×10^8 steps);
- 71 pieces pack with none overlapping, close round the largest.

In Void Hormiga, 58,674 linked runes show as 382 bodies with 6 ms frames.
