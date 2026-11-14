/* voidmaiz/overview.hpp — a graph too large to draw, drawn in bounded work
 * (okf/concepts/overview.md).
 *
 * A force-directed picture of every node stops being a picture long before it
 * stops being affordable: at 6×10^4 nodes the all-pairs repulsion a small view
 * uses costs 3.6×10^9 steps an iteration, and the screen shows a hairball
 * anyway. This header is the two halves that make such a view bounded by what
 * it SHOWS, not by the graph:
 *
 *  - A TREE of groups over the graph's nodes (hierarchical aggregation,
 *    Elmqvist & Fekete, IEEE TVCG 2010; "overview first, zoom and filter,
 *    details on demand", Shneiderman 1996). Communities come from Leiden
 *    (voidmaiz/graph.hpp), split until a group shows at most `leaf_size`
 *    things, then merged upwards along their strongest ties, never past a
 *    size cap, until at most `roots` bodies show with nothing open. Modularity never merges what is not linked, so what is left
 *    above `roots` is BUNDLED (groups side by side, nothing between them), and
 *    a group that still has too many children is SLICED. Both are marked, so
 *    a view can say which groups mean something and which are only packing.
 *  - A LAYOUT step for the bodies a view shows: Fruchterman–Reingold forces
 *    with Barnes–Hut repulsion (Barnes & Hut, Nature 324, 1986), O(n log n + m)
 *    an iteration, with masses so a group of a thousand takes the room of one.
 *
 * Opening a group is view state: `cut` gives the bodies to draw for a set of
 * open groups, `body_of` which body each node is inside, and `lift` the links
 * between bodies, summed. Nothing here is model state, dispatches anything, or
 * draws: positions are ephemera, like a camera. Deterministic: the same graph
 * gives the same tree and the same layout on every machine.
 *
 * UI-free (no ImGui types). */
#pragma once

#include <cstdint>
#include <vector>

namespace maiz::overview {

struct Link {
    std::uint32_t a = 0, b = 0;
    float w = 1.0f;
};

/* Ids [0, items) are the graph's own nodes; ids from `items` up are groups. */
struct Tree {
    static constexpr std::uint32_t kNone = 0xffffffffu;
    enum Kind : std::uint8_t {
        Community = 0, // found from the links: its members belong together
        Bundle = 1,    // packing: groups with no links between them, side by side
        Slice = 2,     // packing: one community's members, cut to fit a screen
    };
    std::uint32_t items = 0;
    std::vector<std::uint32_t> parent;                // per id; kNone at a root
    std::vector<std::vector<std::uint32_t>> children; // per id, largest first; empty for a node
    std::vector<std::uint32_t> size;                  // nodes under the id (1 for a node)
    std::vector<std::uint8_t> kind;                   // per id (Community for a node)
    std::vector<std::uint32_t> roots;                 // largest first

    std::uint32_t count() const { return (std::uint32_t)parent.size(); }
    bool is_node(std::uint32_t id) const { return id < items; }
};

struct TreeOptions {
    std::uint32_t leaf_size = 250; // most children a group shows when it is opened
    std::uint32_t roots = 300;     // most bodies shown with nothing open
    std::uint64_t seed = 1;
};

/* The tree over `n` nodes. A graph of at most `roots` nodes gets no groups: its
 * roots are its nodes. Links with an end >= n are ignored. */
Tree build_tree(std::uint32_t n, const std::vector<Link>& links, const TreeOptions& options = {});

/* The bodies to draw: the roots, with every group whose `open[id]` is set
 * replaced by its children, recursively. `open` may be shorter than count(). */
std::vector<std::uint32_t> cut(const Tree& tree, const std::vector<std::uint8_t>& open);

/* For each node, the index in `cut` of the body it is inside. */
std::vector<std::uint32_t> body_of(const Tree& tree, const std::vector<std::uint32_t>& cut);

/* The links between bodies: each link moved to the bodies its ends are inside
 * (`body_of`, indexed by node), parallel links summed, links inside one body
 * dropped. a < b in every result, sorted. */
std::vector<Link> lift(const std::vector<Link>& links, const std::vector<std::uint32_t>& body_of);

// ── the layout ───────────────────────────────────────────────────────────────

struct Body {
    float x = 0, y = 0;
    float mass = 1.0f;   // a heavier body pushes harder and moves less
    bool pinned = false; // held (a drag): pushes and pulls, never moves
};

struct LayoutParams {
    float spacing = 70.0f;   // K, the distance two linked bodies of mass 1 settle at
    float theta = 0.9f;      // Barnes–Hut: a cell this small for its distance counts as one body
    float gravity = 0.005f;  // pull towards the origin, as a fraction of position an iteration
};

/* One iteration. No body moves further than `step` (the temperature: a host
 * cools it, large to small, as the old view did). Returns the largest move.
 * With `component` (per body, from `components`), bodies repel only within
 * their own component and each is pulled to its own middle: then where a
 * component sits is `pack`'s to decide, and a crowd of small pieces is not
 * pushed by everything else into a ring far from the rest. */
float layout_step(std::vector<Body>& bodies, const std::vector<Link>& links, float step,
                  const LayoutParams& params = {}, const std::vector<std::uint32_t>* component = nullptr);

/* Connected components of `n` bodies by `links`, numbered by first appearance. */
std::vector<std::uint32_t> components(std::uint32_t n, const std::vector<Link>& links);

/* Move whole components so they sit side by side: the largest in the middle,
 * the rest in rings round it, largest first, `gap` apart, none overlapping
 * (`radius` per body: how large it is drawn). The layout inside each
 * component is unchanged. */
void pack(std::vector<Body>& bodies, const std::vector<std::uint32_t>& component, const std::vector<float>& radius,
          float gap = 20.0f);

/* Place bodies on a deterministic disc of `radius` around (cx, cy). */
void seed_disc(std::vector<Body>& bodies, float cx, float cy, float radius, std::uint64_t seed = 1);

} // namespace maiz::overview
