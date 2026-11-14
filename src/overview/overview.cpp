/* overview.cpp — the tree of groups and the Barnes–Hut layout
 * (voidmaiz/overview.hpp, okf/concepts/overview.md). */
#include "voidmaiz/overview.hpp"

#include "voidmaiz/graph.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace maiz::overview {

namespace {

constexpr std::uint32_t kNone = Tree::kNone;

struct Builder {
    Tree& t;

    std::uint32_t group(std::vector<std::uint32_t> kids, std::uint8_t kind) {
        if (kids.size() == 1) return kids[0]; // a group of one is its member
        const std::uint32_t id = t.count();
        std::uint32_t total = 0;
        for (std::uint32_t k : kids) {
            t.parent[k] = id;
            total += t.size[k];
        }
        sort_largest_first(kids);
        t.parent.push_back(kNone);
        t.children.push_back(std::move(kids));
        t.size.push_back(total);
        t.kind.push_back(kind);
        return id;
    }

    void sort_largest_first(std::vector<std::uint32_t>& ids) const {
        std::stable_sort(ids.begin(), ids.end(), [&](std::uint32_t a, std::uint32_t b) {
            return t.size[a] != t.size[b] ? t.size[a] > t.size[b] : a < b;
        });
    }

    /* Group the nodes order[lo, hi), which share a path prefix of `depth`. */
    std::vector<std::uint32_t> from_paths(const std::vector<std::vector<std::uint32_t>>& path,
                                          const std::vector<std::uint32_t>& order, std::size_t lo, std::size_t hi,
                                          std::size_t depth) {
        std::vector<std::uint32_t> out;
        std::size_t i = lo;
        while (i < hi && path[order[i]].size() <= depth) out.push_back(order[i++]); // ends here
        while (i < hi) {
            const std::uint32_t c = path[order[i]][depth];
            std::size_t j = i;
            while (j < hi && path[order[j]].size() > depth && path[order[j]][depth] == c) ++j;
            out.push_back(group(from_paths(path, order, i, j, depth + 1), Tree::Community));
            i = j;
        }
        return out;
    }

    std::uint32_t top_of(std::uint32_t v) const {
        while (t.parent[v] != kNone) v = t.parent[v];
        return v;
    }
};

} // namespace

Tree build_tree(std::uint32_t n, const std::vector<Link>& links, const TreeOptions& options) {
    Tree t;
    t.items = n;
    t.parent.assign(n, kNone);
    t.children.assign(n, {});
    t.size.assign(n, 1);
    t.kind.assign(n, Tree::Community);
    Builder b{t};
    const std::uint32_t leaf = std::max<std::uint32_t>(2, options.leaf_size);
    const std::uint32_t max_roots = std::max<std::uint32_t>(2, options.roots);

    std::vector<std::uint32_t> top;
    if (n <= max_roots) {
        for (std::uint32_t v = 0; v < n; ++v) top.push_back(v);
    } else {
        // 1. communities, split until each holds at most `leaf` nodes
        std::vector<graph::Edge> edges;
        edges.reserve(links.size());
        for (const auto& l : links)
            if (l.a < n && l.b < n && l.a != l.b) edges.push_back({l.a, l.b, (double)l.w});
        const graph::Graph g = graph::Graph::from_edges(n, edges);
        graph::LeidenOptions lo;
        lo.seed = options.seed;
        const graph::Hierarchy h = graph::hierarchy(g, leaf, lo);
        std::vector<std::uint32_t> order(n);
        for (std::uint32_t v = 0; v < n; ++v) order[v] = v;
        std::stable_sort(order.begin(), order.end(),
                         [&](std::uint32_t a, std::uint32_t c) { return h.path[a] < h.path[c]; });
        top = b.from_paths(h.path, order, 0, n, 0);

        /* 2. merged upwards along their strongest ties, none past a size cap.
         * Lowering Leiden's resolution instead fuses whole linked regions into
         * one group (a 6×10^4-node part became one group of 56,611 and a ring
         * of crumbs), which is a true partition and a useless overview. The cap
         * is what keeps an overview's bodies comparable: a few times the mean
         * share of a root, or the largest community if that is bigger. */
        if (top.size() > max_roots) {
            const std::uint32_t T = (std::uint32_t)top.size();
            std::vector<std::uint32_t> local(t.count(), kNone);
            for (std::uint32_t i = 0; i < T; ++i) local[top[i]] = i;
            struct Tie {
                std::uint32_t a, b;
                double w;
            };
            std::vector<Tie> ties;
            for (const auto& e : edges) {
                std::uint32_t a = local[b.top_of(e.a)], c = local[b.top_of(e.b)];
                if (a == c) continue;
                if (a > c) std::swap(a, c);
                ties.push_back({a, c, e.w});
            }
            std::sort(ties.begin(), ties.end(),
                      [](const Tie& x, const Tie& y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
            std::size_t w = 0; // parallel ties summed
            for (std::size_t i = 0; i < ties.size(); ++i) {
                if (w && ties[w - 1].a == ties[i].a && ties[w - 1].b == ties[i].b) ties[w - 1].w += ties[i].w;
                else ties[w++] = ties[i];
            }
            ties.resize(w);
            std::vector<double> sz(T);
            std::uint32_t largest = 0;
            for (std::uint32_t i = 0; i < T; ++i) {
                sz[i] = t.size[top[i]];
                largest = std::max(largest, t.size[top[i]]);
            }
            // strongest first: ties per member, so two crowds joined by one link are not "close"
            std::stable_sort(ties.begin(), ties.end(), [&](const Tie& x, const Tie& y) {
                return x.w / (sz[x.a] * sz[x.b]) > y.w / (sz[y.a] * sz[y.b]);
            });
            const double cap = std::max<double>(largest, 4.0 * (double)n / (double)max_roots);
            std::vector<std::uint32_t> uf(T), usize(T);
            for (std::uint32_t i = 0; i < T; ++i) uf[i] = i, usize[i] = t.size[top[i]];
            auto find = [&](std::uint32_t x) {
                while (uf[x] != x) x = uf[x] = uf[uf[x]];
                return x;
            };
            std::uint32_t clusters = T;
            for (const auto& tie : ties) {
                if (clusters <= max_roots) break;
                std::uint32_t ra = find(tie.a), rb = find(tie.b);
                if (ra == rb || (double)usize[ra] + usize[rb] > cap) continue;
                if (rb < ra) std::swap(ra, rb);
                uf[rb] = ra;
                usize[ra] += usize[rb];
                --clusters;
            }
            std::vector<std::vector<std::uint32_t>> members(T);
            for (std::uint32_t i = 0; i < T; ++i) members[find(i)].push_back(top[i]);
            std::vector<std::uint32_t> next;
            for (auto& m : members)
                if (!m.empty()) next.push_back(b.group(std::move(m), Tree::Community));
            top = std::move(next);
        }

        // 3. what is still too many is bundled: the largest stand alone, the rest share bodies
        if (top.size() > max_roots) {
            b.sort_largest_first(top);
            const std::size_t keep = max_roots / 2, bundles = max_roots - keep;
            const std::size_t rest = top.size() - keep;
            const std::size_t per = (rest + bundles - 1) / bundles;
            std::vector<std::uint32_t> next(top.begin(), top.begin() + (std::ptrdiff_t)keep);
            for (std::size_t i = keep; i < top.size(); i += per) {
                std::vector<std::uint32_t> chunk(top.begin() + (std::ptrdiff_t)i,
                                                 top.begin() + (std::ptrdiff_t)std::min(top.size(), i + per));
                next.push_back(b.group(std::move(chunk), Tree::Bundle));
            }
            top = std::move(next);
        }

        // 4. no group opens onto more than `leaf` children: the rest are sliced
        for (std::uint32_t id = n; id < t.count(); ++id) {
            while (t.children[id].size() > leaf) {
                const std::uint8_t kind = t.kind[id] == Tree::Bundle ? Tree::Bundle : Tree::Slice;
                std::vector<std::uint32_t> kids = std::move(t.children[id]);
                t.children[id].clear();
                std::vector<std::uint32_t> slices;
                for (std::size_t i = 0; i < kids.size(); i += leaf) {
                    std::vector<std::uint32_t> part(kids.begin() + (std::ptrdiff_t)i,
                                                    kids.begin() + (std::ptrdiff_t)std::min(kids.size(), i + leaf));
                    slices.push_back(b.group(std::move(part), kind));
                }
                for (std::uint32_t s : slices) t.parent[s] = id;
                b.sort_largest_first(slices);
                t.children[id] = std::move(slices);
            }
        }
    }
    b.sort_largest_first(top);
    t.roots = std::move(top);
    return t;
}

std::vector<std::uint32_t> cut(const Tree& tree, const std::vector<std::uint8_t>& open) {
    std::vector<std::uint32_t> out, stack(tree.roots.rbegin(), tree.roots.rend());
    while (!stack.empty()) {
        const std::uint32_t id = stack.back();
        stack.pop_back();
        if (!tree.is_node(id) && id < open.size() && open[id])
            for (auto it = tree.children[id].rbegin(); it != tree.children[id].rend(); ++it) stack.push_back(*it);
        else
            out.push_back(id);
    }
    return out;
}

std::vector<std::uint32_t> body_of(const Tree& tree, const std::vector<std::uint32_t>& cut) {
    std::vector<std::uint32_t> index(tree.count(), kNone);
    for (std::uint32_t i = 0; i < cut.size(); ++i) index[cut[i]] = i;
    std::vector<std::uint32_t> out(tree.items, kNone);
    for (std::uint32_t v = 0; v < tree.items; ++v) {
        std::uint32_t x = v;
        while (x != kNone && index[x] == kNone) x = tree.parent[x];
        out[v] = x == kNone ? kNone : index[x];
    }
    return out;
}

std::vector<Link> lift(const std::vector<Link>& links, const std::vector<std::uint32_t>& body_of) {
    std::vector<Link> out;
    out.reserve(links.size() / 4 + 1);
    for (const auto& l : links) {
        if (l.a >= body_of.size() || l.b >= body_of.size()) continue;
        std::uint32_t a = body_of[l.a], b = body_of[l.b];
        if (a == kNone || b == kNone || a == b) continue;
        if (a > b) std::swap(a, b);
        out.push_back({a, b, l.w});
    }
    std::sort(out.begin(), out.end(), [](const Link& x, const Link& y) { return x.a != y.a ? x.a < y.a : x.b < y.b; });
    std::size_t w = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (w > 0 && out[w - 1].a == out[i].a && out[w - 1].b == out[i].b)
            out[w - 1].w += out[i].w;
        else
            out[w++] = out[i];
    }
    out.resize(w);
    return out;
}

// ── Barnes–Hut ───────────────────────────────────────────────────────────────

namespace {

struct Cell {
    float x0, y0, half;        // the square: centre (x0, y0), half its side
    float mx = 0, my = 0;      // mass-weighted position sums
    float mass = 0;
    std::int32_t body = -1;    // the one body in a leaf; -2 for an inner cell
    std::int32_t child[4] = {-1, -1, -1, -1};
};

struct QuadTree {
    std::vector<Cell> cells;
    const std::vector<Body>* bodies = nullptr;

    int quadrant(const Cell& c, float x, float y) const { return (x >= c.x0 ? 1 : 0) + (y >= c.y0 ? 2 : 0); }

    std::int32_t make(float x0, float y0, float half) {
        Cell c;
        c.x0 = x0;
        c.y0 = y0;
        c.half = half;
        cells.push_back(c);
        return (std::int32_t)cells.size() - 1;
    }

    std::int32_t child_of(std::int32_t ci, int q) {
        if (cells[ci].child[q] < 0) {
            const float h = cells[ci].half * 0.5f;
            const float x = cells[ci].x0 + ((q & 1) ? h : -h), y = cells[ci].y0 + ((q & 2) ? h : -h);
            const std::int32_t k = make(x, y, h);
            cells[ci].child[q] = k;
        }
        return cells[ci].child[q];
    }

    void insert(std::int32_t i) {
        const Body& b = (*bodies)[i];
        std::int32_t ci = 0;
        for (int depth = 0;; ++depth) {
            Cell& c = cells[ci];
            c.mx += b.x * b.mass;
            c.my += b.y * b.mass;
            c.mass += b.mass;
            if (c.body == -1 && c.child[0] < 0 && c.child[1] < 0 && c.child[2] < 0 && c.child[3] < 0) {
                c.body = i; // an empty leaf takes it
                return;
            }
            if (depth >= 40) { // coincident bodies: the cell holds them as one mass
                cells[ci].body = -2;
                return;
            }
            if (c.body >= 0) { // a leaf with one body: push that one down first
                const std::int32_t old = c.body;
                cells[ci].body = -2;
                const Body& o = (*bodies)[old];
                const std::int32_t k = child_of(ci, quadrant(cells[ci], o.x, o.y));
                Cell& kc = cells[k];
                kc.mx += o.x * o.mass;
                kc.my += o.y * o.mass;
                kc.mass += o.mass;
                kc.body = old;
            }
            cells[ci].body = -2;
            ci = child_of(ci, quadrant(cells[ci], b.x, b.y));
        }
    }
};

} // namespace

void seed_disc(std::vector<Body>& bodies, float cx, float cy, float radius, std::uint64_t seed) {
    std::uint64_t s = seed * 6364136223846793005ull + 1442695040888963407ull;
    auto rnd = [&s] {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return (float)((s >> 40) & 0xffffff) / (float)(1u << 24);
    };
    for (auto& b : bodies) {
        const float a = rnd() * 6.2831853f, r = radius * std::sqrt(rnd());
        b.x = cx + std::cos(a) * r;
        b.y = cy + std::sin(a) * r;
    }
}

std::vector<std::uint32_t> components(std::uint32_t n, const std::vector<Link>& links) {
    std::vector<std::uint32_t> uf(n);
    for (std::uint32_t i = 0; i < n; ++i) uf[i] = i;
    auto find = [&](std::uint32_t x) {
        while (uf[x] != x) x = uf[x] = uf[uf[x]];
        return x;
    };
    for (const auto& l : links)
        if (l.a < n && l.b < n) {
            const std::uint32_t a = find(l.a), b = find(l.b);
            if (a != b) uf[std::max(a, b)] = std::min(a, b);
        }
    std::vector<std::uint32_t> id(n, kNone), out(n);
    std::uint32_t next = 0;
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t r = find(i);
        if (id[r] == kNone) id[r] = next++;
        out[i] = id[r];
    }
    return out;
}

float layout_step(std::vector<Body>& bodies, const std::vector<Link>& links, float step, const LayoutParams& p,
                  const std::vector<std::uint32_t>* component) {
    const std::size_t n = bodies.size();
    if (n == 0) return 0;
    const float K = p.spacing, K2 = K * K, theta2 = p.theta * p.theta;
    std::vector<float> dx(n, 0.0f), dy(n, 0.0f);

    // the groups repulsion acts within: every body, or one connected component each
    const bool by_component = component && component->size() == n;
    std::vector<std::vector<std::uint32_t>> groups;
    if (by_component) {
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t c = (*component)[i];
            if (c >= groups.size()) groups.resize((std::size_t)c + 1);
            groups[c].push_back((std::uint32_t)i);
        }
    } else {
        groups.assign(1, {});
        for (std::size_t i = 0; i < n; ++i) groups[0].push_back((std::uint32_t)i);
    }
    std::vector<float> gx(groups.size(), 0.0f), gy(groups.size(), 0.0f); // where each group is pulled to
    std::vector<std::uint32_t> group_of(n, 0);

    std::vector<std::int32_t> stack;
    for (std::size_t g = 0; g < groups.size(); ++g) {
        const auto& members = groups[g];
        for (std::uint32_t i : members) group_of[i] = (std::uint32_t)g;
        if (by_component && !members.empty()) { // pulled to its own middle: where it sits is pack's business
            double sx = 0, sy = 0;
            for (std::uint32_t i : members) sx += bodies[i].x, sy += bodies[i].y;
            gx[g] = (float)(sx / (double)members.size());
            gy[g] = (float)(sy / (double)members.size());
        }
        if (members.size() < 2) continue;
        float x0 = bodies[members[0]].x, x1 = x0, y0 = bodies[members[0]].y, y1 = y0;
        for (std::uint32_t i : members) {
            x0 = std::min(x0, bodies[i].x);
            x1 = std::max(x1, bodies[i].x);
            y0 = std::min(y0, bodies[i].y);
            y1 = std::max(y1, bodies[i].y);
        }
        QuadTree q;
        q.bodies = &bodies;
        q.cells.reserve(members.size() * 2);
        q.make((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, std::max(x1 - x0, y1 - y0) * 0.5f + 1.0f);
        for (std::uint32_t i : members) q.insert((std::int32_t)i);

        for (std::uint32_t i : members) {
            const Body& b = bodies[i];
            stack.assign(1, 0);
            while (!stack.empty()) {
                const Cell& c = q.cells[stack.back()];
                stack.pop_back();
                if (c.mass <= 0 || c.body == (std::int32_t)i) continue;
                const float cx = c.mx / c.mass, cy = c.my / c.mass;
                const float ex = b.x - cx, ey = b.y - cy;
                const float d2 = ex * ex + ey * ey;
                const bool leaf = c.body >= 0 || (c.child[0] < 0 && c.child[1] < 0 && c.child[2] < 0 &&
                                                  c.child[3] < 0);
                const float side = c.half * 2.0f;
                if (leaf || side * side < theta2 * d2) {
                    if (d2 < 1e-4f) { // on top of it: a fixed nudge, so nothing divides by zero
                        const float a = (float)i * 2.3999632f;
                        dx[i] += std::cos(a) * K;
                        dy[i] += std::sin(a) * K;
                        continue;
                    }
                    const float f = K2 * c.mass / (d2 * b.mass); // |F| = K²·m/d, and a heavy body moves less
                    dx[i] += ex * f;
                    dy[i] += ey * f;
                } else {
                    for (std::int32_t k : c.child)
                        if (k >= 0) stack.push_back(k);
                }
            }
        }
    }

    for (const auto& l : links) {
        if (l.a >= n || l.b >= n || l.a == l.b) continue;
        Body& a = bodies[l.a];
        Body& b = bodies[l.b];
        const float ex = a.x - b.x, ey = a.y - b.y;
        const float d = std::sqrt(ex * ex + ey * ey) + 0.01f;
        const float f = d / K * l.w; // |F| = d²/K, along the unit vector
        dx[l.a] -= ex * f / a.mass;
        dy[l.a] -= ey * f / a.mass;
        dx[l.b] += ex * f / b.mass;
        dy[l.b] += ey * f / b.mass;
    }

    float moved = 0;
    for (std::size_t i = 0; i < n; ++i) {
        Body& b = bodies[i];
        if (b.pinned) continue;
        const float len = std::sqrt(dx[i] * dx[i] + dy[i] * dy[i]);
        if (len > 1e-6f) {
            const float lim = std::min(len, step);
            b.x += dx[i] / len * lim;
            b.y += dy[i] / len * lim;
            moved = std::max(moved, lim);
        }
        const float cx = gx[group_of[i]], cy = gy[group_of[i]];
        b.x = cx + (b.x - cx) * (1.0f - p.gravity);
        b.y = cy + (b.y - cy) * (1.0f - p.gravity);
    }
    return moved;
}

void pack(std::vector<Body>& bodies, const std::vector<std::uint32_t>& component, const std::vector<float>& radius,
          float gap) {
    const std::size_t n = bodies.size();
    if (n == 0 || component.size() != n || radius.size() != n) return;
    std::uint32_t count = 0;
    for (auto c : component) count = std::max(count, c + 1);
    // each component's middle, and the circle round it
    std::vector<double> sx(count, 0), sy(count, 0);
    std::vector<std::uint32_t> members(count, 0);
    for (std::size_t i = 0; i < n; ++i) {
        sx[component[i]] += bodies[i].x;
        sy[component[i]] += bodies[i].y;
        ++members[component[i]];
    }
    std::vector<float> cx(count, 0.0f), cy(count, 0.0f), r(count, 0.0f);
    for (std::uint32_t c = 0; c < count; ++c)
        if (members[c]) {
            cx[c] = (float)(sx[c] / members[c]);
            cy[c] = (float)(sy[c] / members[c]);
        }
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t c = component[i];
        r[c] = std::max(r[c], std::hypot(bodies[i].x - cx[c], bodies[i].y - cy[c]) + radius[i]);
    }
    std::vector<std::uint32_t> order;
    for (std::uint32_t c = 0; c < count; ++c)
        if (members[c]) order.push_back(c);
    std::stable_sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return r[a] != r[b] ? r[a] > r[b] : a < b;
    });
    // the largest in the middle; the rest in rings round it, each ring as wide as its first (largest)
    std::vector<float> px(count, 0.0f), py(count, 0.0f);
    float inner = 0.0f, width = 0.0f, angle = 0.0f;
    std::size_t k = 0;
    if (!order.empty()) inner = r[order[k++]] + gap;
    while (k < order.size()) {
        const std::uint32_t c = order[k];
        if (width == 0.0f) width = r[c];
        const float rho = inner + width; // the ring's middle
        const float span = 2.0f * std::asin(std::min(1.0f, (r[c] + gap * 0.5f) / rho));
        if (angle > 0.0f && angle + span > 6.2831853f) { // full: the next ring, outside this one
            inner = rho + width + gap;
            width = 0.0f;
            angle = 0.0f;
            continue;
        }
        const float a = angle + span * 0.5f;
        px[c] = std::cos(a) * rho;
        py[c] = std::sin(a) * rho;
        angle += span;
        ++k;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t c = component[i];
        bodies[i].x += px[c] - cx[c];
        bodies[i].y += py[c] - cy[c];
    }
}

} // namespace maiz::overview
