/* usergraph.cpp — the symmetric, timeless user action graph.
 *
 * Every container here is kept in a canonical sorted order rather than in
 * arrival order: an edge is stored with a < b, a frame's members are sorted and
 * deduped, and lookups tiebreak on id. That is not tidiness — it is the
 * structure refusing to encode a sequence it was explicitly not supposed to
 * have. */
#include "voidmaiz/usergraph.hpp"

#include <algorithm>

namespace maiz {

/* NO `device_name` / `device_from_name` ANY MORE. They converted between a
 * closed six-value enum and its spelling; `channel` is the spelling now, so the
 * conversion is the identity and the functions were pure ceremony around a
 * decision the library should not have been making (see usergraph.hpp). Hosts
 * that want the GUI vocabulary use `maiz::channel::touch` and friends. */

Affordance& UserGraph::node_for(std::string_view id, std::string_view kind,
                                std::string_view channel) {
    for (auto& n : nodes_) {
        if (n.id == id) {
            // Last channel wins, but only when one was actually given: a later
            // touch that does not say how it arrived must not erase what an
            // earlier one knew.
            if (!channel.empty()) n.channel = std::string(channel);
            if (n.kind.empty() && !kind.empty()) n.kind = std::string(kind);
            return n;
        }
    }
    nodes_.push_back({std::string(id), std::string(kind), std::string(channel), 0});
    return nodes_.back();
}

void UserGraph::open_frame() {
    // Nesting flattens: a frame is a set of things touched together, and a
    // stack would reintroduce exactly the ordering this structure refuses.
    if (!framing_) open_.clear();
    framing_ = true;
}

void UserGraph::touch(std::string_view id, std::string_view kind,
                      std::string_view channel) {
    if (id.empty()) return;
    Affordance& a = node_for(id, kind, channel);
    ++a.touches;
    if (framing_ &&
        std::find(open_.begin(), open_.end(), id) == open_.end())
        open_.emplace_back(id);
}

void UserGraph::close_frame() {
    if (!framing_) return;
    framing_ = false;

    std::vector<std::string> members = open_;
    open_.clear();
    std::sort(members.begin(), members.end());
    members.erase(std::unique(members.begin(), members.end()), members.end());
    if (members.size() < 2) return; // nothing co-occurred

    // The hyperedge: a triple is not three pairs, so the whole set is kept.
    bool merged = false;
    for (auto& f : frames_) {
        if (f.members == members) { ++f.weight; merged = true; break; }
    }
    if (!merged) frames_.push_back({members, 1});

    // ...and its pairwise projection, for the cheap coherence query.
    for (size_t i = 0; i < members.size(); ++i) {
        for (size_t j = i + 1; j < members.size(); ++j) {
            const std::string& a = members[i]; // already sorted, so a < b and
            const std::string& b = members[j]; // the edge has one representation
            auto it = std::find_if(edges_.begin(), edges_.end(),
                                   [&](const Coincidence& e) {
                                       return e.a == a && e.b == b;
                                   });
            if (it != edges_.end()) ++it->weight;
            else edges_.push_back({a, b, 1});
        }
    }
}

const Affordance* UserGraph::find(std::string_view id) const {
    for (const auto& n : nodes_)
        if (n.id == id) return &n;
    return nullptr;
}

int UserGraph::coherence(std::string_view a, std::string_view b) const {
    if (a == b) return 0;
    std::string lo(a), hi(b);
    if (hi < lo) std::swap(lo, hi); // symmetric: the query has no direction
    for (const auto& e : edges_)
        if (e.a == lo && e.b == hi) return e.weight;
    return 0;
}

std::vector<std::string> UserGraph::coherent_with(std::string_view id,
                                                  int min_weight) const {
    std::vector<std::pair<int, std::string>> hits;
    for (const auto& e : edges_) {
        if (e.weight < min_weight) continue;
        if (e.a == id)      hits.push_back({e.weight, e.b});
        else if (e.b == id) hits.push_back({e.weight, e.a});
    }
    std::sort(hits.begin(), hits.end(), [](const auto& l, const auto& r) {
        if (l.first != r.first) return l.first > r.first; // strongest first
        return l.second < r.second;                       // then determinate
    });
    std::vector<std::string> out;
    out.reserve(hits.size());
    for (auto& h : hits) out.push_back(std::move(h.second));
    return out;
}

void UserGraph::clear() {
    nodes_.clear();
    edges_.clear();
    frames_.clear();
    open_.clear();
    framing_ = false;
}

std::string_view UserGraph::affordance_glyph() {
    // `glyph` and `fields` — the core's descriptor keys. An earlier version used
    // `name`/`content`, which registered nothing and made every command below
    // fail with "unknown glyph"; caught by command_smoke, which dispatches what
    // this file emits instead of merely comparing it to a string.
    return R"({"glyph":"allomone-affordance","label":"Affordance",)"
           R"("fields":["kind","device","touches"],)"
           R"("hints":{"editors":{"kind":"text","device":"text","touches":"number"}}})";
}

std::vector<std::string> UserGraph::compile(std::string_view mantle) const {
    std::vector<std::string> cmds;
    if (nodes_.empty()) return cmds;

    cmds.push_back("use " + std::string(mantle));

    // Nodes in id order, so two sessions that touched the same things emit the
    // same transcript regardless of the order the person worked in.
    std::vector<const Affordance*> ordered;
    ordered.reserve(nodes_.size());
    for (const auto& n : nodes_) ordered.push_back(&n);
    std::sort(ordered.begin(), ordered.end(),
              [](const Affordance* l, const Affordance* r) { return l->id < r->id; });

    for (const Affordance* n : ordered) {
        cmds.push_back("rune new allomone-affordance " + n->id);
        cmds.push_back("set " + n->id + " kind \"" + n->kind + "\"");
        cmds.push_back("set " + n->id + " device \"" + n->channel + "\"");
        cmds.push_back("setjson " + n->id + " touches " +
                       std::to_string(n->touches));
    }

    std::vector<Coincidence> sorted = edges_;
    std::sort(sorted.begin(), sorted.end(), [](const Coincidence& l, const Coincidence& r) {
        if (l.a != r.a) return l.a < r.a;
        return l.b < r.b;
    });
    for (const auto& e : sorted)
        // --undirected because the claim genuinely has no direction: these were
        // touched together, and "a then b" is not something this graph knows.
        cmds.push_back("link " + e.a + " " + e.b + " --relation coherence:" +
                       std::to_string(e.weight) + " --undirected");

    return cmds;
}

} // namespace maiz
