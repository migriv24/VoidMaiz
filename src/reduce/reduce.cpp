/* reduce.cpp — the interaction-net executor (port of reduce/net.py +
 * reduce/reduce.py against the conformance contract).
 *
 * Serialization note: conformance compares canonical JSON TEXT, and both the
 * expected value and our result pass through THIS file's serializer, so what
 * matters is determinism and sorted object keys, not byte-compatibility with
 * Python's json.dumps. Integers print without a decimal point (matching the
 * reference for the integer-only case content); non-integral numbers use
 * cJSON's shortest form. */
#include "voidmaiz/reduce.hpp"

#include "sha256.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>

namespace maiz::reduce {

namespace {

struct CJson {
    cJSON* p = nullptr;
    ~CJson() { cJSON_Delete(p); }
    explicit operator bool() const { return p != nullptr; }
};

void escape_into(std::string& out, std::string_view s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        case '\r': out += "\\r"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += (char)c; // UTF-8 passes through (both sides use this)
            }
        }
    }
    out += '"';
}

void canon_into(std::string& out, const cJSON* v) {
    if (!v || cJSON_IsNull(const_cast<cJSON*>(v))) { out += "null"; return; }
    if (cJSON_IsBool(const_cast<cJSON*>(v))) {
        out += cJSON_IsTrue(const_cast<cJSON*>(v)) ? "true" : "false";
        return;
    }
    if (cJSON_IsNumber(const_cast<cJSON*>(v))) {
        double d = v->valuedouble;
        if (d == std::floor(d) && std::fabs(d) < 9.0e15) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%lld", (long long)d);
            out += buf;
        } else {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.17g", d);
            out += buf;
        }
        return;
    }
    if (cJSON_IsString(const_cast<cJSON*>(v))) {
        escape_into(out, v->valuestring);
        return;
    }
    if (cJSON_IsArray(const_cast<cJSON*>(v))) {
        out += '[';
        bool first = true;
        const cJSON* it = nullptr;
        cJSON_ArrayForEach(it, v) {
            if (!first) out += ',';
            first = false;
            canon_into(out, it);
        }
        out += ']';
        return;
    }
    if (cJSON_IsObject(const_cast<cJSON*>(v))) {
        std::vector<const cJSON*> members;
        const cJSON* it = nullptr;
        cJSON_ArrayForEach(it, v) members.push_back(it);
        std::sort(members.begin(), members.end(), [](const cJSON* a, const cJSON* b) {
            return std::string_view(a->string ? a->string : "") <
                   std::string_view(b->string ? b->string : "");
        });
        out += '{';
        bool first = true;
        for (const cJSON* m : members) {
            if (!first) out += ',';
            first = false;
            escape_into(out, m->string ? m->string : "");
            out += ':';
            canon_into(out, m);
        }
        out += '}';
        return;
    }
    out += "null";
}

std::string canon_of(const cJSON* v) {
    std::string out;
    canon_into(out, v);
    return out;
}

const cJSON* member(const cJSON* o, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(o), key);
}

std::string str_of(const cJSON* v, const char* fallback = "") {
    return cJSON_IsString(const_cast<cJSON*>(v)) ? v->valuestring : fallback;
}

/* strict "i:j" */
bool parse_rel(const std::string& rel, int& i, int& j) {
    size_t colon = rel.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= rel.size()) return false;
    for (size_t k = 0; k < rel.size(); ++k)
        if (k != colon && !std::isdigit((unsigned char)rel[k])) return false;
    i = std::atoi(rel.substr(0, colon).c_str());
    j = std::atoi(rel.substr(colon + 1).c_str());
    return true;
}

std::pair<std::string, std::string> pair_key(const std::string& a, const std::string& b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

} // namespace

// ── Net ──────────────────────────────────────────────────────────────────────

void Net::add(Agent a) {
    if (agents.count(a.id)) throw NetError("duplicate agent id '" + a.id + "'");
    agents.emplace(a.id, std::move(a));
}

static void check_port(const Net& n, const Port& p) {
    auto it = n.agents.find(p.first);
    if (it == n.agents.end())
        throw NetError("port references unknown agent '" + p.first + "'");
    if (p.second < 0 || p.second > it->second.arity)
        throw NetError("port index out of range for '" + p.first + "'");
}

void Net::connect(const Port& p, const Port& q) {
    check_port(*this, p);
    check_port(*this, q);
    if (link.count(p) || link.count(q))
        throw NetError("linearity violation: port already wired");
    link[p] = q;
    link[q] = p;
}

const Port* Net::partner(const Port& p) const {
    auto it = link.find(p);
    return it == link.end() ? nullptr : &it->second;
}

void Net::remove_agent(const std::string& id) {
    auto it = agents.find(id);
    if (it == agents.end()) return;
    for (int i = 0; i <= it->second.arity; ++i) {
        auto l = link.find({id, i});
        if (l != link.end()) {
            link.erase(l->second);
            link.erase({id, i});
        }
    }
    agents.erase(it);
}

std::vector<Port> Net::free_ports() const {
    std::vector<Port> out;
    for (const auto& [id, ag] : agents)
        for (int i = 0; i <= ag.arity; ++i)
            if (!link.count({id, i})) out.push_back({id, i});
    return out;
}

void Net::check() const {
    for (const auto& [p, q] : link) {
        check_port(*this, p);
        check_port(*this, q);
        auto back = link.find(q);
        if (back == link.end() || back->second != p)
            throw NetError("non-symmetric wire");
    }
}

// ── spec & adapter ───────────────────────────────────────────────────────────

std::string canon_text(std::string_view json) {
    CJson doc{cJSON_ParseWithLength(json.data(), json.size())};
    if (!doc) throw std::invalid_argument("canon_text: unparseable JSON");
    return canon_of(doc.p);
}

Spec spec_from_json(std::string_view spec_json) {
    CJson doc{cJSON_ParseWithLength(spec_json.data(), spec_json.size())};
    if (!doc || !cJSON_IsObject(doc.p))
        throw std::invalid_argument("reducer spec must be an object");
    Spec spec;
    if (const cJSON* sigs = member(doc.p, "signatures"); cJSON_IsObject(sigs)) {
        const cJSON* s = nullptr;
        cJSON_ArrayForEach(s, sigs)
            spec.signatures[s->string ? s->string : ""] = (int)s->valuedouble;
    }
    std::set<std::pair<std::string, std::string>> seen;
    if (const cJSON* rules = member(doc.p, "rules"); cJSON_IsArray(rules)) {
        const cJSON* r = nullptr;
        cJSON_ArrayForEach(r, rules) {
            const cJSON* glyphs = member(r, "glyphs");
            if (!cJSON_IsArray(glyphs) || cJSON_GetArraySize(const_cast<cJSON*>(glyphs)) != 2)
                throw std::invalid_argument("reducer spec rule: `glyphs` must be [a, b]");
            Spec::Rule rule;
            rule.a = str_of(cJSON_GetArrayItem(const_cast<cJSON*>(glyphs), 0));
            rule.b = str_of(cJSON_GetArrayItem(const_cast<cJSON*>(glyphs), 1));
            std::string kind = str_of(member(r, "rule"));
            if (kind == "annihilate") {
                rule.kind = Spec::RuleKind::Annihilate;
                rule.swap = cJSON_IsTrue(member(r, "swap"));
            } else if (kind == "commute") rule.kind = Spec::RuleKind::Commute;
            else if (kind == "fuse") {
                rule.kind = Spec::RuleKind::Fuse;
                rule.into = str_of(member(r, "into"));
                if (rule.into.empty())
                    throw std::invalid_argument("reducer spec rule: fuse needs `into` glyph");
            } else throw std::invalid_argument("reducer spec rule: unknown rule '" + kind + "'");
            if (!seen.insert(pair_key(rule.a, rule.b)).second)
                throw std::invalid_argument("duplicate rule for a glyph pair (confluence guard)");
            spec.rules.push_back(std::move(rule));
        }
    }
    return spec;
}

Net to_net(std::string_view mantle_json, const std::map<std::string, int>& signatures) {
    CJson doc{cJSON_ParseWithLength(mantle_json.data(), mantle_json.size())};
    if (!doc || !cJSON_IsObject(doc.p)) throw NetError("mantle must be a JSON object");
    Net net;
    if (const cJSON* runes = member(doc.p, "runes"); cJSON_IsArray(runes)) {
        const cJSON* r = nullptr;
        cJSON_ArrayForEach(r, runes) {
            Agent a;
            a.id = str_of(member(member(r, "spirit"), "name"));
            a.glyph = str_of(member(r, "glyph"));
            auto sig = signatures.find(a.glyph);
            a.arity = sig == signatures.end() ? 0 : sig->second;
            const cJSON* content = member(r, "content");
            a.content = (content && !cJSON_IsNull(const_cast<cJSON*>(content)))
                            ? canon_of(content)
                            : "{}";
            if (const cJSON* tags = member(r, "tags"); cJSON_IsArray(tags)) {
                const cJSON* t = nullptr;
                cJSON_ArrayForEach(t, tags)
                    if (cJSON_IsString(const_cast<cJSON*>(t))) a.tags.push_back(t->valuestring);
            }
            net.add(std::move(a));
        }
    }
    const cJSON* edges = member(member(doc.p, "layout"), "edges");
    const cJSON* e = nullptr;
    cJSON_ArrayForEach(e, edges) {
        std::string rel = str_of(member(e, "relation"));
        int i = 0, j = 0;
        if (!parse_rel(rel, i, j))
            throw NetError("edge relation '" + rel +
                           "' is not \"i:j\" — the strict adapter never guesses ports");
        net.connect({str_of(member(e, "from")), i}, {str_of(member(e, "to")), j});
    }
    net.check();
    return net;
}

// ── the reducer ──────────────────────────────────────────────────────────────

namespace {

struct RuleEntry {
    std::string a, b; // registered orientation
    Spec::RuleKind kind;
    std::string into;  // Fuse only
    bool swap = false; // Annihilate only
};

using RuleTable = std::map<std::pair<std::string, std::string>, RuleEntry>;

RuleTable rule_table(const Spec& spec) {
    RuleTable t;
    for (const auto& r : spec.rules)
        t[pair_key(r.a, r.b)] = {r.a, r.b, r.kind, r.into, r.swap};
    return t;
}

bool is_opaque(const Agent& a, const std::set<std::string>& opaque) {
    return opaque.count(a.id) || opaque.count(a.glyph);
}

std::vector<std::pair<std::string, std::string>>
active_pairs(const Net& net, const RuleTable& rules, const std::set<std::string>& opaque) {
    std::set<std::pair<std::string, std::string>> out; // sorted + deduped
    for (const auto& [id, a] : net.agents) {
        if (is_opaque(a, opaque)) continue;
        const Port* q = net.partner({id, 0});
        if (!q || q->second != 0 || q->first == id) continue; // self-loop never active
        const Agent& b = net.agents.at(q->first);
        if (is_opaque(b, opaque)) continue;
        if (!rules.count(pair_key(a.glyph, b.glyph))) continue;
        out.insert(pair_key(id, b.id));
    }
    return {out.begin(), out.end()};
}

void fire(Net& net, const RuleTable& rules, const std::string& a_id, const std::string& b_id,
          bool strict_locality) {
    Agent a = net.agents.at(a_id); // copies: the agents are about to be removed
    Agent b = net.agents.at(b_id);
    const RuleEntry& entry = rules.at(pair_key(a.glyph, b.glyph));
    if (a.glyph != entry.a || b.glyph != entry.b) std::swap(a, b); // registered order

    // snapshot partners of the redex's aux ports BEFORE deletion — external
    // ports, or INTERNAL ones (a redex wired to itself: legal Lafont nets;
    // resolving them is the contract DEFAULT since 0.2.4; the opt-in
    // strict-locality subset rejects them)
    std::map<int, Port> extA, extB;
    for (int i = 1; i <= a.arity; ++i)
        if (const Port* p = net.partner({a.id, i})) extA[i] = *p;
    for (int i = 1; i <= b.arity; ++i)
        if (const Port* p = net.partner({b.id, i})) extB[i] = *p;

    auto is_internal = [&](const Port& p) { return p.first == a.id || p.first == b.id; };
    if (strict_locality)
        for (const auto& m_ : {extA, extB})
            for (const auto& [i, p] : m_)
                if (is_internal(p))
                    throw ReduceError(ReduceError::Kind::Locality,
                                      "rule referenced an internal redex wire (locality)");

    net.remove_agent(a.id);
    net.remove_agent(b.id);

    /* THE FRESH-ID MINTER, DERIVED FROM THE REDEX (conformance README §2,
     * normative since 2026-07-27; case 15 pins it).
     *
     *     id = "_r" + sha256_48( sorted(glyph_a, glyph_b)
     *                            + sorted(id_a, id_b)
     *                            + ordinal,   joined by \x1f )
     *
     * SHA-256 since Void Core 0.2.10 (2026-08-29), when the digest became
     * normative — see sha256.hpp for why it is not the BLAKE2b we had matched.
     *
     * BOTH COMPONENTS ARE SORTED, so the id does not depend on which side this
     * executor happened to call A — the `std::swap` above puts the redex in
     * REGISTERED order, which is a fact about the rule table, not about the
     * net, and sorting removes it from the answer.
     *
     * WHY NOT A COUNTER (which is what this was until 2026-08-18). `_r1` names
     * "the first agent the first firing happened to create" — a fact about the
     * SCHEDULE. Confluence lets two peers pick different, equally valid, redex
     * orders; with a counter they reach structurally identical nets whose
     * agents have different names, and a name is a rune's `spirit.name`, which
     * `layout.edges` references and tag expressions match. Deriving from the
     * redex removes the schedule from the equation: by induction from the input
     * agents (identical on both peers), every derived id is too.
     *
     * The ordinal is per-rewrite and stays deterministic only because each rule
     * below calls `fresh()` a fixed number of times in a fixed order for a
     * given (a, b). That is a REQUIREMENT ON RULES, not an accident — a rule
     * that mints conditionally on iteration order breaks the whole property.
     *
     * Still pure: no clock, no RNG, no state outside this rewrite. */
    long ordinal = 0;
    auto fresh = [&a, &b, &ordinal] {
        return derived_id(a.glyph, b.glyph, a.id, b.id, ++ordinal);
    };

    if (entry.kind == Spec::RuleKind::Annihilate) {
        if (a.arity != b.arity)
            throw ReduceError(ReduceError::Kind::Other,
                              "annihilate needs equal arity: " + a.glyph + "/" + b.glyph);
        /* Wire equations x_i ≡ y_σ(i) — σ = identity (δδ's look: crossing
         * between mirrored bodies) or the index-swap σ(i) = n+1-i (Lafont's
         * γγ: parallel between mirrored bodies). Internal wires join the
         * equation set; each connected component with two external ends gets
         * a wire, one end stays free, zero ends (a closed loop) vanishes. */
        int n = a.arity;
        // slots: 0..n-1 = a's aux 1..n; n..2n-1 = b's aux 1..n
        std::vector<int> dsu(2 * n);
        for (int s = 0; s < 2 * n; ++s) dsu[s] = s;
        std::function<int(int)> find = [&](int s) {
            return dsu[s] == s ? s : dsu[s] = find(dsu[s]);
        };
        auto unite = [&](int p, int q) { dsu[find(p)] = find(q); };
        auto slot_of = [&](const Port& p) {
            return p.first == a.id ? p.second - 1 : n + p.second - 1;
        };
        std::vector<const Port*> ext(2 * n, nullptr);
        for (const auto& [i, p] : extA)
            if (is_internal(p)) unite(i - 1, slot_of(p));
            else ext[i - 1] = &p;
        for (const auto& [i, p] : extB)
            if (is_internal(p)) unite(n + i - 1, slot_of(p));
            else ext[n + i - 1] = &p;
        for (int i = 1; i <= n; ++i) {
            int j = entry.swap ? n + 1 - i : i;
            unite(i - 1, n + j - 1);
        }
        std::map<int, std::vector<const Port*>> comps;
        for (int s = 0; s < 2 * n; ++s)
            if (ext[s]) comps[find(s)].push_back(ext[s]);
        for (const auto& [root, ends] : comps)
            if (ends.size() == 2) net.connect(*ends[0], *ends[1]);
        return;
    }

    if (entry.kind == Spec::RuleKind::Fuse) {
        // the pair becomes ONE `into` agent adopting a's boundaries 1..m then
        // b's 1..n (arity-0 fusions — color mixing — leave it free-floating);
        // an internal wire simply joins two of the new agent's own aux ports
        Agent c{fresh(), entry.into, a.arity + b.arity, "{}", {}};
        std::string cid = c.id;
        net.add(std::move(c));
        auto fuse_slot = [&](const Port& p) {
            return p.first == a.id ? p.second : a.arity + p.second;
        };
        std::set<std::pair<int, int>> internal_done;
        auto attach = [&](int slot, const Port& p) {
            if (!is_internal(p)) {
                net.connect({cid, slot}, p);
                return;
            }
            // materialize before the temporaries die — minmax returns REFERENCES
            std::pair<int, int> key = std::minmax(slot, fuse_slot(p));
            if (internal_done.insert(key).second)
                net.connect({cid, slot}, {cid, fuse_slot(p)});
        };
        for (const auto& [i, p] : extA) attach(i, p);
        for (const auto& [i, p] : extB) attach(a.arity + i, p);
        return;
    }

    // commute: α (arity m) meets β (arity n) → m β-copies + n α-copies, m×n grid
    int m = a.arity, n = b.arity;
    std::vector<std::string> bcopies, acopies;
    for (int k = 0; k < m; ++k) {
        Agent c{fresh(), b.glyph, n, b.content, {}}; // copies start tagless
        bcopies.push_back(c.id);
        net.add(std::move(c));
    }
    for (int j = 0; j < n; ++j) {
        Agent c{fresh(), a.glyph, m, a.content, {}};
        acopies.push_back(c.id);
        net.add(std::move(c));
    }
    /* Each boundary port maps to a copy's principal; an internal wire between
     * two boundary ports wires the corresponding copy principals together —
     * a fresh active pair, exactly Lafont's picture for a self-wired redex. */
    auto copy_port = [&](const Port& p) -> Port {
        return p.first == a.id ? Port{bcopies[p.second - 1], 0}
                               : Port{acopies[p.second - 1], 0};
    };
    std::set<std::pair<Port, Port>> internal_done;
    auto attach = [&](const Port& boundary, const Port& partner) {
        if (!is_internal(partner)) {
            net.connect(copy_port(boundary), partner);
            return;
        }
        auto key = std::minmax(boundary, partner);
        if (internal_done.insert({key.first, key.second}).second)
            net.connect(copy_port(boundary), copy_port(partner));
    };
    for (const auto& [i, p] : extA) attach({a.id, i}, p);
    for (const auto& [i, p] : extB) attach({b.id, i}, p);
    for (int j = 0; j < n; ++j)
        for (int k = 0; k < m; ++k)
            net.connect({acopies[j], k + 1}, {bcopies[k], j + 1});
}

} // namespace

std::vector<std::pair<std::string, std::string>>
active_pairs(const Spec& spec, const Net& net, const std::set<std::string>& opaque) {
    return active_pairs(net, rule_table(spec), opaque);
}

Net step(const Spec& spec, const Net& net, const std::pair<std::string, std::string>& redex,
         bool strict_locality) {
    Net work = net;
    work.check();
    /* No collision bookkeeping any more: ids derive from the redex, so a
     * re-step of the SAME redex on the same net mints the same names — which
     * is idempotence, not a clash. The old scan for the highest `_rN` existed
     * only to keep a counter from repeating itself. */
    fire(work, rule_table(spec), redex.first, redex.second, strict_locality);
    return work;
}

Net reduce(const Spec& spec, const Net& net, int max_steps,
           const std::set<std::string>& opaque,
           const std::function<size_t(size_t)>& pick, bool strict_locality) {
    RuleTable rules = rule_table(spec);
    Net work = net; // pure: the source is never mutated
    work.check();
    int steps = 0;
    while (true) {
        auto pairs = active_pairs(work, rules, opaque);
        if (pairs.empty()) return work;
        if (++steps > max_steps)
            throw ReduceError(ReduceError::Kind::Termination,
                              "exceeded max_steps (non-terminating rule set on this net)");
        size_t idx = pick ? pick(pairs.size()) % pairs.size() : 0;
        fire(work, rules, pairs[idx].first, pairs[idx].second, strict_locality);
    }
}

// ── the canonical form ───────────────────────────────────────────────────────

std::string canonical(const Net& net) {
    auto endp_text = [&](const Port& p) {
        std::string out = "[";
        escape_into(out, net.agents.at(p.first).glyph);
        out += ',' + std::to_string(p.second) + ']';
        return out;
    };

    std::vector<std::string> agents;
    for (const auto& [id, a] : net.agents) {
        std::string sig = "[";
        escape_into(sig, a.glyph);
        sig += ',' + a.content + ",[";
        std::vector<std::string> tags = a.tags;
        std::sort(tags.begin(), tags.end());
        for (size_t i = 0; i < tags.size(); ++i) {
            if (i) sig += ',';
            escape_into(sig, tags[i]);
        }
        sig += "]]";
        agents.push_back(std::move(sig));
    }
    std::sort(agents.begin(), agents.end());

    std::vector<std::string> wires;
    for (const auto& [p, q] : net.link) {
        if (!(p <= q)) continue; // each undirected wire once
        std::string e1 = endp_text(p), e2 = endp_text(q);
        if (e2 < e1) std::swap(e1, e2); // endpoints sorted by canonical text
        wires.push_back("[" + e1 + "," + e2 + "]");
    }
    std::sort(wires.begin(), wires.end());

    std::vector<std::string> free;
    for (const Port& p : net.free_ports()) free.push_back(endp_text(p));
    std::sort(free.begin(), free.end());

    auto join = [](const std::vector<std::string>& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out += ',';
            out += v[i];
        }
        return out + "]";
    };
    // keys in sorted order — every comparison goes through canon_text anyway
    return "{\"agents\":" + join(agents) + ",\"free\":" + join(free) +
           ",\"wires\":" + join(wires) + "}";
}


/* The one implementation of README §2's minter. Kept here rather than in the
 * rewriter's body so that conformance case 16 can call it with nothing else in
 * the way. */
std::string derived_id(std::string_view glyph_a, std::string_view glyph_b,
                       std::string_view parent_a, std::string_view parent_b,
                       long ordinal) {
    auto sorted_pair = [](std::string_view x, std::string_view y) {
        std::string lo(x), hi(y);
        if (hi < lo) std::swap(lo, hi);
        return lo + "\x1f" + hi;
    };
    const std::string key = sorted_pair(glyph_a, glyph_b) + "\x1f" +
                            sorted_pair(parent_a, parent_b) + "\x1f" +
                            std::to_string(ordinal);
    return "_r" + detail::sha256_hex(key, 6);
}

} // namespace maiz::reduce
