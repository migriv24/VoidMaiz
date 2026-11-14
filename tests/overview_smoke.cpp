/* overview_smoke.cpp — the tree of groups and the layout (voidmaiz/overview.hpp).
 *
 *  - a small graph gets no groups: its roots are its nodes;
 *  - a ring of cliques: every bound holds (roots, children per group), sizes
 *    add up, each node is under exactly one root, opening everything shows
 *    every node, and links lift to bodies with nothing lost;
 *  - ten thousand unlinked nodes: bundled, and still within every bound;
 *  - the same graph twice gives the same tree;
 *  - the layout keeps cliques together, never moves further than its step,
 *    and an iteration at 3×10^4 bodies is n log n, not n². */
#include "voidmaiz/overview.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

using namespace maiz::overview;

static std::vector<Link> ring_of_cliques(std::uint32_t cliques, std::uint32_t k) {
    std::vector<Link> links;
    for (std::uint32_t c = 0; c < cliques; ++c) {
        for (std::uint32_t i = 0; i < k; ++i)
            for (std::uint32_t j = i + 1; j < k; ++j) links.push_back({c * k + i, c * k + j, 1.0f});
        links.push_back({c * k, ((c + 1) % cliques) * k + 1, 1.0f});
    }
    return links;
}

static void check_bounds(const Tree& t, const TreeOptions& o) {
    CHECK(t.roots.size() <= o.roots);
    std::uint64_t total = 0;
    for (auto r : t.roots) {
        CHECK(t.parent[r] == Tree::kNone);
        total += t.size[r];
    }
    CHECK(total == t.items);
    for (std::uint32_t id = t.items; id < t.count(); ++id) {
        CHECK(t.children[id].size() <= o.leaf_size);
        CHECK(t.children[id].size() >= 2);
        std::uint64_t s = 0;
        for (auto c : t.children[id]) {
            CHECK(t.parent[c] == id);
            s += t.size[c];
        }
        CHECK(s == t.size[id]);
    }
}

int main() {
    // 1. small: no groups
    {
        auto links = ring_of_cliques(3, 10);
        Tree t = build_tree(30, links);
        CHECK(t.count() == 30);
        CHECK(t.roots.size() == 30);
        auto c = cut(t, {});
        CHECK(c.size() == 30);
    }

    // 2. a ring of 40 cliques of 50
    {
        const std::uint32_t n = 2000;
        auto links = ring_of_cliques(40, 50);
        TreeOptions o;
        o.roots = 20;
        o.leaf_size = 60;
        Tree t = build_tree(n, links, o);
        check_bounds(t, o);
        auto closed = cut(t, {});
        CHECK(closed.size() == t.roots.size());
        auto bo = body_of(t, closed);
        for (auto b : bo) CHECK(b < closed.size());
        auto lifted = lift(links, bo);
        for (const auto& l : lifted) CHECK(l.a < l.b);
        float crossing = 0, lifted_w = 0;
        for (const auto& l : links)
            if (bo[l.a] != bo[l.b]) crossing += l.w;
        for (const auto& l : lifted) lifted_w += l.w;
        CHECK(crossing == lifted_w);
        std::vector<std::uint8_t> all(t.count(), 1);
        CHECK(cut(t, all).size() == n);
        // a clique's members share a body when nothing is open
        int split = 0;
        for (std::uint32_t c = 0; c < 40; ++c)
            for (std::uint32_t i = 1; i < 50; ++i) split += bo[c * 50] != bo[c * 50 + i];
        CHECK(split == 0);
        // determinism
        Tree again = build_tree(n, links, o);
        CHECK(again.parent == t.parent);
        CHECK(again.roots == t.roots);
        std::cout << "ring of cliques: " << t.count() - n << " groups, " << t.roots.size() << " roots, "
                  << lifted.size() << " lifted links\n";
    }

    // 3. nothing linked
    {
        TreeOptions o;
        o.roots = 100;
        o.leaf_size = 250;
        Tree t = build_tree(10000, {}, o);
        check_bounds(t, o);
        int bundles = 0;
        for (std::uint32_t id = t.items; id < t.count(); ++id) bundles += t.kind[id] == Tree::Bundle;
        CHECK(bundles > 0);
        std::cout << "unlinked: " << t.count() - t.items << " groups, " << bundles << " bundles, " << t.roots.size()
                  << " roots\n";
    }

    // 4. the layout: two cliques settle apart, each one together
    {
        std::vector<Link> links;
        for (std::uint32_t c = 0; c < 2; ++c)
            for (std::uint32_t i = 0; i < 30; ++i)
                for (std::uint32_t j = i + 1; j < 30; ++j) links.push_back({c * 30 + i, c * 30 + j, 1.0f});
        std::vector<Body> bodies(60);
        seed_disc(bodies, 0, 0, 300);
        float last = 0;
        for (int it = 0; it < 300; ++it) {
            const float step = 4.0f + 60.0f * (1.0f - it / 300.0f) * (1.0f - it / 300.0f);
            last = layout_step(bodies, links, step);
            CHECK(last <= step + 1e-3f);
        }
        auto dist = [&](int a, int b) { return std::hypot(bodies[a].x - bodies[b].x, bodies[a].y - bodies[b].y); };
        double inside = 0, across = 0;
        int ni = 0, na = 0;
        for (int a = 0; a < 60; ++a)
            for (int b = a + 1; b < 60; ++b)
                if ((a < 30) == (b < 30)) inside += dist(a, b), ++ni;
                else across += dist(a, b), ++na;
        inside /= ni;
        across /= na;
        CHECK(inside < across);
        for (const auto& b : bodies) CHECK(std::isfinite(b.x) && std::isfinite(b.y));
        std::cout << "two cliques: mean distance inside " << inside << ", across " << across << "\n";
    }

    // 5. scale: 3×10^4 bodies, an iteration in n log n
    {
        const std::uint32_t n = 30000;
        std::vector<Body> bodies(n);
        seed_disc(bodies, 0, 0, 3000);
        std::vector<Link> links;
        for (std::uint32_t i = 1; i < n; ++i) links.push_back({i, (i * 7919u) % i, 1.0f});
        const auto t0 = std::chrono::steady_clock::now();
        layout_step(bodies, links, 20.0f);
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << "30000 bodies: one iteration " << s * 1000.0 << " ms\n";
        CHECK(s < 2.0); // all pairs would be 9×10^8 steps; Barnes–Hut is a few million
        for (const auto& b : bodies) CHECK(std::isfinite(b.x) && std::isfinite(b.y));
        // coincident bodies do not divide by zero
        std::vector<Body> same(50);
        layout_step(same, {}, 10.0f);
        for (const auto& b : same) CHECK(std::isfinite(b.x) && std::isfinite(b.y));
    }

    // 6. components laid out apart, then packed: none overlaps, each keeps its own shape
    {
        std::vector<Link> links;
        for (std::uint32_t i = 1; i < 60; ++i) links.push_back({0, i, 1.0f}); // one star of 60
        for (std::uint32_t c = 0; c < 40; ++c) links.push_back({60 + 2 * c, 61 + 2 * c, 1.0f}); // 40 pairs
        const std::uint32_t n = 60 + 80 + 30;                                                 // and 30 alone
        std::vector<Body> bodies(n);
        seed_disc(bodies, 0, 0, 500);
        const auto comp = components(n, links);
        CHECK(comp[0] == comp[59] && comp[60] == comp[61] && comp[60] != comp[62] && comp[0] != comp[n - 1]);
        for (int it = 0; it < 200; ++it) layout_step(bodies, links, 30.0f * (1.0f - it / 200.0f) + 2.0f, {}, &comp);
        std::vector<float> radius(n, 6.0f);
        const float inside_before = std::hypot(bodies[60].x - bodies[61].x, bodies[60].y - bodies[61].y);
        pack(bodies, comp, radius, 10.0f);
        const float inside_after = std::hypot(bodies[60].x - bodies[61].x, bodies[60].y - bodies[61].y);
        CHECK(std::fabs(inside_before - inside_after) < 1e-2f);
        std::uint32_t count = 0;
        for (auto c : comp) count = std::max(count, c + 1);
        std::vector<double> sx(count, 0), sy(count, 0);
        std::vector<int> m(count, 0);
        for (std::uint32_t i = 0; i < n; ++i) sx[comp[i]] += bodies[i].x, sy[comp[i]] += bodies[i].y, ++m[comp[i]];
        std::vector<float> r(count, 0);
        for (std::uint32_t i = 0; i < n; ++i)
            r[comp[i]] = std::max(r[comp[i]], (float)std::hypot(bodies[i].x - sx[comp[i]] / m[comp[i]],
                                                                bodies[i].y - sy[comp[i]] / m[comp[i]]) + 6.0f);
        int overlaps = 0;
        for (std::uint32_t a = 0; a < count; ++a)
            for (std::uint32_t c = a + 1; c < count; ++c)
                overlaps += std::hypot(sx[a] / m[a] - sx[c] / m[c], sy[a] / m[a] - sy[c] / m[c]) < r[a] + r[c] - 0.5;
        CHECK(overlaps == 0);
        float extent = 0;
        for (const auto& b : bodies) extent = std::max(extent, std::hypot(b.x, b.y));
        std::cout << "packed " << count << " components within radius " << extent << ", star radius " << r[comp[0]]
                  << "\n";
        CHECK(extent < 6.0f * r[comp[0]] + 200.0f); // the crumbs sit close round the star, not on a far ring
    }

    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "overview_smoke: ok\n";
    return failures ? 1 : 0;
}
