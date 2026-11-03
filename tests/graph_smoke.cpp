/* graph_smoke.cpp — the graph algorithms against graphs whose answers are known.
 *
 *  - Zachary's karate club (1977), the field's standard: the best known
 *    modularity is 0.4198; Leiden should land at or very near it;
 *  - a ring of cliques: each clique is a community, and every community must
 *    be internally connected (Leiden's guarantee);
 *  - the same graph twice gives the same partition (replay);
 *  - components, PageRank's sum, NMI's extremes, a hierarchy's depth. */
#include "voidmaiz/graph.hpp"

#include <algorithm>
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

using namespace maiz::graph;

int main() {
    // Zachary's karate club, 34 members, 78 ties (1-based in the paper)
    static const int karate[][2] = {
        {1,2},{1,3},{1,4},{1,5},{1,6},{1,7},{1,8},{1,9},{1,11},{1,12},{1,13},{1,14},{1,18},{1,20},{1,22},{1,32},
        {2,3},{2,4},{2,8},{2,14},{2,18},{2,20},{2,22},{2,31},{3,4},{3,8},{3,9},{3,10},{3,14},{3,28},{3,29},{3,33},
        {4,8},{4,13},{4,14},{5,7},{5,11},{6,7},{6,11},{6,17},{7,17},{9,31},{9,33},{9,34},{10,34},{14,34},{15,33},
        {15,34},{16,33},{16,34},{19,33},{19,34},{20,34},{21,33},{21,34},{23,33},{23,34},{24,26},{24,28},{24,30},
        {24,33},{24,34},{25,26},{25,28},{25,32},{26,32},{27,30},{27,34},{28,34},{29,32},{29,34},{30,33},{30,34},
        {31,33},{31,34},{32,33},{32,34},{33,34}};
    std::vector<Edge> ke;
    for (const auto& e : karate) ke.push_back({(std::uint32_t)e[0] - 1, (std::uint32_t)e[1] - 1, 1.0});
    const Graph kg = Graph::from_edges(34, ke);
    CHECK(kg.total == 78);
    const Partition kp = leiden(kg);
    std::cout << "karate: " << kp.count << " communities, Q = " << kp.modularity << "\n";
    CHECK(kp.modularity > 0.41 && kp.modularity <= 0.4199);
    CHECK(kp.count >= 3 && kp.count <= 5);
    CHECK(leiden(kg).community == kp.community); // replayable

    // a ring of 8 cliques of 6, joined by single edges
    std::vector<Edge> re;
    const std::uint32_t K = 8, S = 6;
    for (std::uint32_t c = 0; c < K; ++c) {
        for (std::uint32_t i = 0; i < S; ++i)
            for (std::uint32_t j = i + 1; j < S; ++j) re.push_back({c * S + i, c * S + j, 1.0});
        re.push_back({c * S, ((c + 1) % K) * S + 1, 1.0});
    }
    const Graph rg = Graph::from_edges(K * S, re);
    const Partition rp = leiden(rg);
    CHECK(rp.count == K);
    std::vector<std::uint32_t> truth(K * S);
    for (std::uint32_t v = 0; v < K * S; ++v) truth[v] = v / S;
    CHECK(std::fabs(nmi(rp.community, truth) - 1.0) < 1e-9);
    // every community connected: its induced subgraph has one component
    for (std::uint32_t c = 0; c < rp.count; ++c) {
        std::vector<std::uint32_t> local(K * S, UINT32_MAX), members;
        for (std::uint32_t v = 0; v < K * S; ++v)
            if (rp.community[v] == c) { local[v] = (std::uint32_t)members.size(); members.push_back(v); }
        std::vector<Edge> ie;
        for (const auto& e : re)
            if (local[e.a] != UINT32_MAX && local[e.b] != UINT32_MAX) ie.push_back({local[e.a], local[e.b], 1});
        const auto comp = components(Graph::from_edges((std::uint32_t)members.size(), ie));
        CHECK(*std::max_element(comp.begin(), comp.end()) == 0);
    }

    // components and PageRank
    const Graph two = Graph::from_edges(5, {{0, 1, 1}, {1, 2, 1}, {3, 4, 1}});
    const auto cc = components(two);
    CHECK(cc[0] == 0 && cc[2] == 0 && cc[3] == 1 && cc[4] == 1);
    const auto pr = pagerank(kg);
    double sum = 0;
    for (double x : pr) sum += x;
    CHECK(std::fabs(sum - 1.0) < 1e-6);
    CHECK(pr[33] > pr[11] && pr[0] > pr[11]); // the two leaders outrank a leaf

    CHECK(nmi({0, 0, 1, 1}, {5, 5, 9, 9}) > 0.999);
    CHECK(nmi({0, 1, 0, 1}, {0, 0, 1, 1}) < 1e-9);

    // nodes with no edges, and a graph with none at all (2026-10-27: a host
    // leaves value edges out of a clustering, so a measure rune is isolated)
    {
        std::vector<Edge> es = {{0, 1}, {1, 2}, {2, 0}, {3, 4}, {4, 5}, {5, 3}}; // node 6 alone
        const Graph lone = Graph::from_edges(7, es);
        const Hierarchy hl = hierarchy(lone, 2);
        CHECK(hl.path.size() == 7);
        CHECK(!hl.path[6].empty());
        const Graph none = Graph::from_edges(5, {});
        const Partition pn = leiden(none);
        CHECK(pn.community.size() == 5);
        const Hierarchy hn = hierarchy(none, 2);
        CHECK(hn.path.size() == 5);
        const Hierarchy empty = hierarchy(Graph::from_edges(0, {}), 2); // crashed before 2026-10-27
        CHECK(empty.path.empty() && empty.depth == 0);
    }

    // a hierarchy splits only what is larger than the target
    const Hierarchy h = hierarchy(rg, 10);
    CHECK(h.depth == 1);
    for (const auto& p : h.path) CHECK(p.size() == 1);

    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "OK - graph: components, PageRank, Leiden, hierarchy, NMI\n";
    return failures ? 1 : 0;
}
