/* project.cpp — state document → Scene (the projection engine).
 * cJSON only; no rendering types. Rebuilds the whole scene each call — at
 * VLS/demo scale that is well under a millisecond, and correctness (the
 * one-sync rule) comes first; incremental dirty-tracking is a later
 * optimization, not a different design. */
#include "voidmaiz/project.hpp"
#include "voidmaiz/embed.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace maiz {

namespace {

struct CJson {
    cJSON* p = nullptr;
    ~CJson() { cJSON_Delete(p); }
    explicit operator bool() const { return p != nullptr; }
};

const char* gstr(const cJSON* o, const char* key, const char* fallback = "") {
    const cJSON* it = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(o), key);
    return cJSON_IsString(it) ? it->valuestring : fallback;
}

/* ── ONE MANTLE OUT OF THE WHOLE DOCUMENT, WITHOUT PARSING THE REST ───────────
 *
 * (2026-10-24, Void Hormiga issue #2.) Projecting the 10-rune `kinds` mantle of a
 * 10^5-rune database parsed all 43 MB of state into cJSON first, because the core
 * exports the document whole. This walks the text instead, recording each
 * mantle's byte range and name and the `active` member, then hands cJSON only the
 * mantle that was asked for. A tokenizer that steps over values (strings with
 * their escapes, nested containers) is a memchr-speed pass; a parse builds a tree
 * of every value in the file. Anything unexpected returns false, and the caller
 * parses the whole document as before. */
class Scan {
public:
    explicit Scan(std::string_view t) : t_(t) {}
    void ws() {
        while (i_ < t_.size() && (t_[i_] == ' ' || t_[i_] == '\n' || t_[i_] == '\r' || t_[i_] == '\t')) ++i_;
    }
    bool eat(char c) {
        ws();
        if (i_ >= t_.size() || t_[i_] != c) return false;
        ++i_;
        return true;
    }
    std::size_t pos() const { return i_; }
    // a string's raw content (escapes left as written), cursor after the quote
    bool string(std::string_view& out) {
        if (!eat('"')) return false;
        const std::size_t b = i_;
        for (;;) {
            const std::size_t q = t_.find('"', i_);
            if (q == std::string_view::npos) return false;
            std::size_t k = q;
            while (k > b && t_[k - 1] == '\\') --k;
            i_ = q + 1;
            if ((q - k) % 2 == 0) { out = t_.substr(b, q - b); return true; }
        }
    }
    bool skip() {
        ws();
        if (i_ >= t_.size()) return false;
        std::string_view s;
        if (t_[i_] == '"') return string(s);
        if (t_[i_] == '{' || t_[i_] == '[') {
            int depth = 0;
            while (i_ < t_.size()) {
                const char c = t_[i_];
                if (c == '"') { if (!string(s)) return false; continue; }
                if (c == '{' || c == '[') ++depth;
                else if ((c == '}' || c == ']') && --depth == 0) { ++i_; return true; }
                ++i_;
            }
            return false;
        }
        while (i_ < t_.size() && !std::strchr(",}] \n\r\t", t_[i_])) ++i_;
        return true;
    }

private:
    std::string_view t_;
    std::size_t i_ = 0;
};

/* The byte range of mantle `want` (or of the active one, when `want` is empty,
 * which is then filled in), straight out of an exported state document.
 * Found, or ABSENT (the document scanned cleanly and no mantle has that name:
 * the answer is an empty scene, with no parse at all), or Unsure (anything
 * unexpected: the caller parses the whole document, as before). */
enum class Slice { Found, Absent, Unsure };
Slice slice_mantle(std::string_view state, std::string& want, std::string_view& slice) {
    Scan s(state);
    std::vector<std::pair<std::string_view, std::string_view>> mantles; // name, text
    std::string active;
    if (!s.eat('{')) return Slice::Unsure;
    if (s.eat('}')) return Slice::Unsure;
    constexpr Slice no = Slice::Unsure;
    for (;;) {
        std::string_view key;
        if (!s.string(key) || !s.eat(':')) return no;
        s.ws();
        if (key == "mantles") {
            if (!s.eat('[')) return no;
            if (!s.eat(']')) {
                for (;;) {
                    s.ws();
                    const std::size_t b = s.pos();
                    if (!s.eat('{')) return no;
                    std::string_view name;
                    if (!s.eat('}')) {
                        for (;;) {
                            std::string_view k;
                            if (!s.string(k) || !s.eat(':')) return no;
                            s.ws();
                            if (k == "name") {
                                if (!s.string(name)) return no; // a name is a string, or this is not ours
                            } else if (!s.skip()) {
                                return no;
                            }
                            if (s.eat(',')) continue;
                            if (!s.eat('}')) return no;
                            break;
                        }
                    }
                    mantles.emplace_back(name, state.substr(b, s.pos() - b));
                    if (s.eat(',')) continue;
                    if (!s.eat(']')) return no;
                    break;
                }
            }
        } else if (key == "active") {
            const std::size_t b = s.pos();
            if (!s.skip()) return no;
            CJson a{cJSON_ParseWithLength(state.data() + b, s.pos() - b)};
            if (a && cJSON_IsObject(a.p)) active = gstr(a.p, "mantle");
        } else if (!s.skip()) {
            return no;
        }
        if (s.eat(',')) continue;
        if (!s.eat('}')) return no;
        break;
    }
    if (want.empty()) want = active;
    bool escaped = false;
    for (const auto& [name, text] : mantles) {
        if (name.find('\\') != std::string_view::npos) { escaped = true; continue; }
        if (name == want) { slice = text; return Slice::Found; }
    }
    return escaped ? Slice::Unsure : Slice::Absent; // an escaped name might be it: cJSON decides
}

/* {"x":…,"y":…} or [x,y] → position. */
bool read_pos(const cJSON* v, float& x, float& y) {
    if (!v) return false;
    if (cJSON_IsObject(v)) {
        const cJSON* px = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(v), "x");
        const cJSON* py = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(v), "y");
        if (cJSON_IsNumber(px) && cJSON_IsNumber(py)) {
            x = (float)px->valuedouble;
            y = (float)py->valuedouble;
            return true;
        }
    }
    if (cJSON_IsArray(v) && cJSON_GetArraySize(const_cast<cJSON*>(v)) >= 2) {
        const cJSON* px = cJSON_GetArrayItem(const_cast<cJSON*>(v), 0);
        const cJSON* py = cJSON_GetArrayItem(const_cast<cJSON*>(v), 1);
        if (cJSON_IsNumber(px) && cJSON_IsNumber(py)) {
            x = (float)px->valuedouble;
            y = (float)py->valuedouble;
            return true;
        }
    }
    return false;
}

/* "#rrggbb" → 0xRRGGBB. */
bool read_hex_color(const char* s, unsigned& rgb) {
    if (!s || s[0] != '#') return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(s + 1, &end, 16);
    if (!end || *end != '\0' || end - (s + 1) != 6) return false;
    rgb = (unsigned)v;
    return true;
}

/* Strict "i:j" (the reduce contract's mantle-adapter form). */
bool parse_port_relation(const std::string& rel, int& i, int& j) {
    if (rel.empty()) return false;
    size_t colon = rel.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= rel.size()) return false;
    for (size_t k = 0; k < rel.size(); ++k)
        if (k != colon && !isdigit((unsigned char)rel[k])) return false;
    i = std::atoi(rel.substr(0, colon).c_str());
    j = std::atoi(rel.substr(colon + 1).c_str());
    return true;
}

/* THE CANVAS PRESENTATION (Void Core 0.2.14, SPEC §3.3).
 *
 * A descriptor used to answer two questions in one object: what IS this rune
 * (schema — `fields`, `kind`, `kinds`: true in every mantle, for every output,
 * forever) and how does it APPEAR (presentation — of which there is one per
 * modality). 0.2.14 split them: `presentations` is a map keyed by modality
 * that the core stores and never interprets, and `canvas` is the conventional
 * home for what a host has been calling `hints`.
 *
 * Void Maiz IS the canvas modality, so `presentations.canvas` is our object.
 * We layer it over `hints` PER KEY rather than instead of it, for one reason:
 * a glyph author moves keys across one at a time, and an all-or-nothing switch
 * would make a descriptor that has both silently lose half its look. Canvas
 * wins where both speak; `hints` keeps working untouched where it doesn't,
 * which is every descriptor written before today. */
struct Presentation {
    const cJSON* canvas = nullptr;
    const cJSON* hints = nullptr;

    static Presentation of(const cJSON* descriptor) {
        Presentation p;
        if (!descriptor) return p;
        const cJSON* h = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(descriptor), "hints");
        if (cJSON_IsObject(h)) p.hints = h;
        const cJSON* pres =
            cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(descriptor), "presentations");
        if (cJSON_IsObject(pres)) {
            const cJSON* c = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(pres), "canvas");
            if (cJSON_IsObject(c)) p.canvas = c;
        }
        return p;
    }

    explicit operator bool() const { return canvas || hints; }

    const cJSON* get(const char* key) const {
        if (canvas)
            if (const cJSON* v =
                    cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(canvas), key))
                return v;
        if (hints)
            if (const cJSON* v = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(hints), key))
                return v;
        return nullptr;
    }

    const char* str(const char* key, const char* fallback = "") const {
        const cJSON* v = get(key);
        return cJSON_IsString(v) ? v->valuestring : fallback;
    }
};

/* `{level, unit, min, max}` (SPEC §3.3.2) → Quantity. Read, never enforced:
 * an out-of-range level is stored by the core only if the core accepted it, and
 * a projection is not the place to re-litigate a validation the model already
 * ran. Absent object → `present == false`, which is not the same as a quantity
 * that states nothing. */
Quantity read_quantity(const cJSON* q) {
    Quantity out;
    if (!cJSON_IsObject(q)) return out;
    out.present = true;
    out.level = gstr(q, "level");
    out.unit = gstr(q, "unit");
    if (const cJSON* mn = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(q), "min");
        cJSON_IsNumber(mn)) {
        out.has_min = true;
        out.min = mn->valuedouble;
    }
    if (const cJSON* mx = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(q), "max");
        cJSON_IsNumber(mx)) {
        out.has_max = true;
        out.max = mx->valuedouble;
    }
    return out;
}

/* The descriptor's `kind` (SPEC §3.3.1). Unknown or absent → Entity: every
 * rune written before kinds existed is one, and a projection that blanked a
 * node over a typo in a type it does not own would be worse than one that
 * draws it the way it always did. */
RuneKind read_kind(const cJSON* descriptor) {
    if (!descriptor) return RuneKind::Entity;
    std::string_view k = gstr(descriptor, "kind");
    if (k == "act") return RuneKind::Act;
    if (k == "measure") return RuneKind::Measure;
    return RuneKind::Entity;
}

void apply_glyph_hints(SceneNode& node, const cJSON* descriptor, const cJSON* content) {
    if (!descriptor) return;
    node.label = gstr(descriptor, "label", node.glyph.c_str());
    /* Schema first — `kind` is what the rune IS, and it is read whether or not
     * the glyph ever says how the rune looks. */
    node.kind = read_kind(descriptor);
    const Presentation pres = Presentation::of(descriptor);
    if (!pres) return;

    /* Shape (okf/concepts/node-geometry.md): notation bodies. */
    if (const cJSON* shape = pres.get("shape"); cJSON_IsObject(shape)) {
        std::string_view shape_kind = gstr(shape, "kind"); // the BODY's kind, not the rune's
        if (shape_kind == "triangle") {
            node.shape = NodeShape::Polygon;
            node.shape_sides = 3;
        } else if (shape_kind == "circle") {
            node.shape = NodeShape::Circle;
        } else if (shape_kind == "polygon") {
            node.shape = NodeShape::Polygon;
            const cJSON* sides = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(shape), "sides");
            node.shape_sides = cJSON_IsNumber(sides) ? std::max(3, (int)sides->valuedouble) : 3;
        } else if (shape_kind == "block") {
            node.shape = NodeShape::Block; // statement block (node-blocks.md)
        }
        /* A RUNE may set its own sides (content.sides, a number or a numeric
         * string) over its glyph's: Void Hormiga's polygon test node varies the
         * shape per node to stress body geometry and port anchors across devices
         * (2026-09-25). Clamped, because a person types it. */
        if (node.shape == NodeShape::Polygon && content) {
            const cJSON* s = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), "sides");
            int n = 0;
            if (cJSON_IsNumber(s)) n = (int)s->valuedouble;
            else if (cJSON_IsString(s) && s->valuestring) n = std::atoi(s->valuestring);
            if (n > 0) node.shape_sides = std::clamp(n, 3, 24);
        }
        const cJSON* rot = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(shape), "rot");
        if (cJSON_IsNumber(rot)) {
            node.rot_auto = false;
            node.rot = (float)rot->valuedouble;
        } // "auto" or absent → rot_auto stays true
    }

    /* Subgraph entry: hints.enter names the content field carrying a mantle
     * name (a host convention — the core's mantles are flat; nesting is how
     * a view reads them, VLS's Loop pattern generalized). */
    if (const char* enter_field = pres.str("enter", nullptr); enter_field && content) {
        const cJSON* v =
            cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), enter_field);
        if (cJSON_IsString(v)) node.enter_mantle = v->valuestring;
    }

    unsigned rgb = 0;
    if (read_hex_color(pres.str("color", nullptr), rgb)) {
        node.rgb = rgb;
        node.has_color = true;
    }
    if (const cJSON* face = pres.get("face"); cJSON_IsObject(face)) {
        const cJSON* w = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(face), "w");
        const cJSON* h = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(face), "h");
        if (cJSON_IsNumber(w)) node.w = (float)w->valuedouble;
        if (cJSON_IsNumber(h)) node.h = (float)h->valuedouble;
    }
    /* Ports: net index 0 is the principal; auxiliaries take 1..n in
     * declaration order (skipping the principal entry wherever it appears). */
    const cJSON* ports = pres.get("ports");
    if (!cJSON_IsArray(ports)) return;
    int next_aux = 1;
    const cJSON* pd = nullptr;
    cJSON_ArrayForEach(pd, ports) {
        if (!cJSON_IsObject(pd)) continue;
        const cJSON* pr = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(pd), "principal");
        if (cJSON_IsTrue(pr)) {
            node.principal_name = gstr(pd, "name");
            continue;
        }
        ScenePort port;
        port.index = next_aux++;
        port.name = gstr(pd, "name");
        if (port.name.empty()) port.name = "p" + std::to_string(port.index);
        port.type = gstr(pd, "type");
        port.adjacency = std::string_view(gstr(pd, "render")) == "adjacency";
        port.many = std::string_view(gstr(pd, "max")) == "many";
        if (std::string_view(gstr(pd, "dir", "in")) == "out")
            node.outputs.push_back(std::move(port));
        else
            node.inputs.push_back(std::move(port));
    }
}

/* Editable fields = the glyph's declared `fields`, valued from the rune's
 * content. Undeclared content keys (e.g. our `pos` convention) don't surface —
 * the glyph is the editability registry (SPEC §3.3). `hints.editors` binds a
 * field key to a widget-registry editor spec ("date", "combo:a,b", …) — the
 * glyph declares WHAT edits the field, the registry supplies HOW; `hints.labels`
 * binds a key to a human-facing label ("text_en" → "Text (English)"), so a
 * form never shows raw field keys (okf/concepts/widget-registry.md). */
void fill_fields(SceneNode& node, const cJSON* descriptor, const cJSON* content) {
    if (!descriptor) return;
    const cJSON* keys = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(descriptor), "fields");
    /* `editors` and `labels` are PRESENTATION (how the field appears), so they
     * come through the canvas layer; `kinds` is SCHEMA (what the number is), so
     * it is read straight off the descriptor and is the same in every modality. */
    const Presentation pres = Presentation::of(descriptor);
    const cJSON* editors = pres.get("editors");
    const cJSON* labels = pres.get("labels");
    const cJSON* kinds = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(descriptor), "kinds");
    const cJSON* k = nullptr;
    cJSON_ArrayForEach(k, keys) {
        if (!cJSON_IsString(k)) continue;
        SceneField f;
        f.key = k->valuestring;
        if (editors) {
            const cJSON* e =
                cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(editors), f.key.c_str());
            if (cJSON_IsString(e)) f.editor = e->valuestring;
        }
        if (labels) {
            const cJSON* l =
                cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(labels), f.key.c_str());
            if (cJSON_IsString(l)) f.label = l->valuestring;
        }
        if (kinds)
            f.quantity = read_quantity(
                cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(kinds), f.key.c_str()));
        const cJSON* v =
            content ? cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), f.key.c_str())
                    : nullptr;
        if (!v) {
            f.value_json = "\"\"";
        } else {
            f.is_string = cJSON_IsString(v);
            if (char* printed = cJSON_PrintUnformatted(v)) {
                f.value_json = printed;
                cJSON_free(printed);
            }
        }
        node.fields.push_back(std::move(f));
    }
}

/* Make sure an aux index referenced by a wire exists on the node; synthesize
 * a port if the glyph declared none (port order is app knowledge — we only
 * ever *add* anchors, never reorder). */
void ensure_aux_port(SceneNode& node, int index, bool as_output) {
    if (index <= 0) return; // 0 = principal, always present
    for (const auto& p : node.inputs)
        if (p.index == index) return;
    for (const auto& p : node.outputs)
        if (p.index == index) return;
    ScenePort port;
    port.index = index;
    port.name = "p" + std::to_string(index);
    (as_output ? node.outputs : node.inputs).push_back(std::move(port));
}

void auto_layout(Scene& scene, const ProjectOptions& opts) {
    /* Depth = the longest directed-wire distance from a source, which becomes
     * the column an unplaced node is laid out in.
     *
     * O(N + E) ON ANY GRAPH (2026-10-25). This was a relaxation bounded by the
     * node count: a pass per step of the longest chain on a DAG, and on a graph
     * with a directed cycle N full passes, because a cycle's depths only stop
     * growing at the bound. A linked database is cyclic as a matter of course
     * (two organizations that partner each other, people who know each other):
     * 5×10^4 runes and 8.5×10^4 links took 330 s here, and the bound put every
     * node on a cycle in column N, far off the canvas.
     *
     * Now: the strongly connected components (Tarjan, iteratively, so a long
     * chain cannot overflow the stack), each collapsed to one node, which leaves
     * a DAG; longest paths over it in topological order; every node of a
     * component takes the component's depth. On a DAG every component is one
     * node and the depths are exactly the old ones. */
    const std::size_t n = scene.nodes.size();
    bool any_unplaced = false;
    for (const auto& nd : scene.nodes) any_unplaced = any_unplaced || !nd.placed;
    if (!any_unplaced) return; // depth only places unplaced nodes

    std::unordered_map<std::string_view, std::uint32_t> index;
    index.reserve(n);
    for (std::size_t i = 0; i < n; ++i) index.emplace(scene.nodes[i].name, (std::uint32_t)i);
    std::vector<std::uint32_t> head(n + 1, 0), succ;
    {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> arcs;
        for (const auto& w : scene.wires) {
            if (!w.directed) continue;
            const auto f = index.find(w.from), t = index.find(w.to);
            if (f == index.end() || t == index.end() || f->second == t->second) continue;
            arcs.emplace_back(f->second, t->second);
        }
        for (const auto& a : arcs) ++head[a.first + 1];
        for (std::size_t i = 0; i < n; ++i) head[i + 1] += head[i];
        succ.resize(arcs.size());
        std::vector<std::uint32_t> fill(head.begin(), head.end() - 1);
        for (const auto& a : arcs) succ[fill[a.first]++] = a.second;
    }

    // Tarjan: components come out in REVERSE topological order (sinks first)
    constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    std::vector<std::uint32_t> idx(n, kNone), low(n, 0), comp(n, kNone), stack, order;
    std::vector<char> on(n, 0);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> call; // node, next successor slot
    std::uint32_t counter = 0, ncomp = 0;
    for (std::uint32_t root = 0; root < n; ++root) {
        if (idx[root] != kNone) continue;
        call.emplace_back(root, head[root]);
        idx[root] = low[root] = counter++;
        stack.push_back(root);
        on[root] = 1;
        while (!call.empty()) {
            auto& [v, next] = call.back();
            if (next < head[v + 1]) {
                const std::uint32_t u = succ[next++];
                if (idx[u] == kNone) {
                    idx[u] = low[u] = counter++;
                    stack.push_back(u);
                    on[u] = 1;
                    call.emplace_back(u, head[u]);
                } else if (on[u]) {
                    low[v] = std::min(low[v], idx[u]);
                }
                continue;
            }
            if (low[v] == idx[v]) {
                std::uint32_t u;
                do {
                    u = stack.back();
                    stack.pop_back();
                    on[u] = 0;
                    comp[u] = ncomp;
                } while (u != v);
                ++ncomp;
            }
            const std::uint32_t done = v;
            call.pop_back();
            if (!call.empty()) low[call.back().first] = std::min(low[call.back().first], low[done]);
        }
    }

    // longest paths over the condensation: components in topological order are
    // ncomp-1 down to 0, so a component's predecessors are all settled first
    std::vector<std::vector<std::uint32_t>> members(ncomp);
    for (std::uint32_t v = 0; v < n; ++v) members[comp[v]].push_back(v);
    std::vector<int> cdepth(ncomp, 0);
    for (std::uint32_t c = ncomp; c-- > 0;)
        for (const std::uint32_t v : members[c])
            for (std::uint32_t e = head[v]; e < head[v + 1]; ++e) {
                const std::uint32_t d = comp[succ[e]];
                if (d != c) cdepth[d] = std::max(cdepth[d], cdepth[c] + 1);
            }

    std::map<int, int> rows; // column → next free row (unplaced nodes only)
    for (std::size_t i = 0; i < n; ++i) {
        SceneNode& nd = scene.nodes[i];
        if (nd.placed) continue;
        const int col = cdepth[comp[i]];
        const int row = rows[col]++;
        nd.x = opts.origin_x + col * opts.col_gap;
        nd.y = opts.origin_y + row * opts.row_gap;
    }
}

/* A 64-bit mix over the text, eight bytes at a step: a projection's identity,
 * not a secret (nothing adversarial chooses a mantle to collide with another). */
std::uint64_t mix(std::string_view a, std::uint64_t h) {
    std::size_t i = 0;
    for (; i + 8 <= a.size(); i += 8) {
        std::uint64_t w;
        std::memcpy(&w, a.data() + i, 8);
        h = (h ^ w) * 0x9E3779B97F4A7C15ull;
        h ^= h >> 29;
    }
    for (; i < a.size(); ++i) h = (h ^ (unsigned char)a[i]) * 0x100000001B3ull;
    return (h ^ a.size()) * 0xBF58476D1CE4E5B9ull;
}

std::uint64_t digest_of(std::string_view name, std::string_view slice, std::string_view glyphs,
                        const ProjectOptions& o) {
    char gaps[64];
    const int n = std::snprintf(gaps, sizeof gaps, "%g %g %g %g", o.col_gap, o.row_gap, o.origin_x, o.origin_y);
    std::uint64_t h = mix(name, 0xCBF29CE484222325ull);
    h = mix(slice, h);
    h = mix(glyphs, h);
    h = mix(std::string_view(gaps, n > 0 ? (std::size_t)n : 0), h);
    return h ? h : 1; // 0 means "unknown"
}

} // namespace

std::uint64_t mantle_digest(std::string_view state_json, std::string_view glyphs_json,
                            const ProjectOptions& opts) {
    std::string want = opts.mantle;
    std::string_view slice;
    if (slice_mantle(state_json, want, slice) != Slice::Found) return 0;
    return digest_of(want, slice, glyphs_json, opts);
}

std::uint64_t mantle_digest(Core& core, const ProjectOptions& opts) {
    const auto glyphs = core.glyphs_text();
    const auto state = core.state_text();
    return mantle_digest(*state, *glyphs, opts);
}

Scene project_scene(std::string_view state_json, std::string_view glyphs_json,
                    const ProjectOptions& opts) {
    Scene scene;
    /* The fast path: parse only the mantle asked for (see slice_mantle). The
     * slow path, the whole document, only when the text is not what it expects. */
    std::string want = opts.mantle;
    std::string_view slice;
    CJson one, state;
    const cJSON* mantle = nullptr;
    const Slice found = slice_mantle(state_json, want, slice);
    if (found == Slice::Absent) return scene; // no such mantle, and nothing parsed to learn it
    if (found == Slice::Found) {
        scene.source = digest_of(want, slice, glyphs_json, opts);
        one.p = cJSON_ParseWithLength(slice.data(), slice.size());
        if (one && cJSON_IsObject(one.p)) mantle = one.p;
    }
    if (!mantle) {
        state.p = cJSON_ParseWithLength(state_json.data(), state_json.size());
        if (!state || !cJSON_IsObject(state.p)) return scene;
    }

    /* Glyph registry: name → full descriptor (arbitrary keys preserved). */
    CJson glyphs{glyphs_json.empty()
                     ? nullptr
                     : cJSON_ParseWithLength(glyphs_json.data(), glyphs_json.size())};
    std::unordered_map<std::string, const cJSON*> glyph_map;
    if (glyphs && cJSON_IsArray(glyphs.p)) {
        const cJSON* gd = nullptr;
        cJSON_ArrayForEach(gd, glyphs.p) {
            const char* nm = gstr(gd, "glyph", nullptr);
            if (nm) glyph_map[nm] = gd;
        }
    }

    /* Pick the mantle: named, or the active one (already done on the fast path). */
    if (!mantle) {
        want = opts.mantle;
        if (want.empty()) {
            const cJSON* active = cJSON_GetObjectItemCaseSensitive(state.p, "active");
            want = gstr(active, "mantle");
        }
        const cJSON* mantles = cJSON_GetObjectItemCaseSensitive(state.p, "mantles");
        const cJSON* mt = nullptr;
        cJSON_ArrayForEach(mt, mantles) {
            if (want == gstr(mt, "name")) { mantle = mt; break; }
        }
    }
    if (!mantle) return scene;
    scene.mantle = want;

    /* Runes → nodes. */
    const cJSON* runes = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(mantle), "runes");
    const cJSON* rune = nullptr;
    cJSON_ArrayForEach(rune, runes) {
        SceneNode node;
        const cJSON* spirit = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(rune), "spirit");
        node.id = gstr(spirit, "id");
        node.name = gstr(spirit, "name");
        node.glyph = gstr(rune, "glyph");
        node.label = node.glyph;
        if (const cJSON* tags = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(rune), "tags");
            cJSON_IsArray(tags)) {
            const cJSON* t = nullptr;
            cJSON_ArrayForEach(t, tags)
                if (cJSON_IsString(t)) node.tags.emplace_back(t->valuestring);
        }
        auto it = glyph_map.find(node.glyph);
        const cJSON* descriptor = it != glyph_map.end() ? it->second : nullptr;
        const cJSON* content = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(rune), "content");
        apply_glyph_hints(node, descriptor, content);
        fill_fields(node, descriptor, content);
        /* `quantity` sits beside `content`, not in it (SPEC §3.2) — the core
         * has to be able to read it, and so does every projection of it. Read
         * from the RUNE: `health` and `speed` share one measure glyph and
         * differ only in what they measure. */
        node.quantity = read_quantity(
            cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(rune), "quantity"));

        /* Chrome state, view-state-as-content (the same interim tier as pos):
         * content.size overrides hints.face; content.collapsed folds the node;
         * content.color ("#rrggbb") overrides hints.color — a per-rune,
         * undoable recolor (author feedback 2026-07-13: tracking ε erasers
         * through port rewires). */
        if (content) {
            const cJSON* size = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), "size");
            read_pos(size, node.w, node.h); // {x,y} / [w,h] both accepted
            node.collapsed = cJSON_IsTrue(
                cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), "collapsed"));
            const cJSON* col = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), "color");
            if (unsigned rgb = 0; cJSON_IsString(col) && read_hex_color(col->valuestring, rgb)) {
                node.rgb = rgb;
                node.has_color = true;
            }
        }

        /* Position: placement first (the reserved model home), then the
         * interim positions-as-content convention (content.pos). */
        const cJSON* placement = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(rune), "placement");
        node.placed = read_pos(placement, node.x, node.y);
        if (!node.placed) {
            const cJSON* pos = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), "pos");
            node.placed = read_pos(pos, node.x, node.y);
        }
        scene.nodes.push_back(std::move(node));
    }

    /* layout.edges → wires. Endpoints are rune names (canonicalized by the
     * core when they exist); a dangling edge is skipped, not an error —
     * `validate` reports those, the canvas just doesn't draw them. */
    const cJSON* layout = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(mantle), "layout");
    const cJSON* edges = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(layout), "edges");
    /* ENDPOINTS BY NAME, IN O(1) (2026-10-24). This loop used to find both ends
     * of every edge by scanning every node: O(E·N), invisible at a hundred runes
     * and 10^10 comparisons for a client with 10^5 records and a value edge on
     * each (Void Hormiga issue #2, which is exactly the shape SPEC §3.7.1
     * encourages). One map, built once, is O(N + E). On a duplicate name (a
     * state `validate` reports) the LAST node wins, exactly as the scan's last
     * assignment did, so no existing projection changes. */
    std::unordered_map<std::string_view, std::size_t> by_name;
    by_name.reserve(scene.nodes.size());
    for (std::size_t k = 0; k < scene.nodes.size(); ++k) by_name[scene.nodes[k].name] = k;
    const cJSON* e = nullptr;
    cJSON_ArrayForEach(e, edges) {
        SceneWire wire;
        wire.from = gstr(e, "from");
        wire.to = gstr(e, "to");
        wire.relation = gstr(e, "relation");
        const cJSON* dir = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(e), "directed");
        wire.directed = !cJSON_IsFalse(dir);
        /* SPEC §3.7: `weight` is a number, default 1.0. Absent or non-numeric
         * keeps the default rather than zeroing — a missing key means "not
         * stated", and 0.0 is a stated value with a very different meaning. */
        const cJSON* wt = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(e), "weight");
        if (cJSON_IsNumber(wt)) wire.weight = wt->valuedouble;

        const auto fi = by_name.find(wire.from), ti = by_name.find(wire.to);
        if (fi == by_name.end() || ti == by_name.end()) continue; // dangling
        SceneNode* from_node = &scene.nodes[fi->second];
        SceneNode* to_node = &scene.nodes[ti->second];

        /* SPEC §3.7.1: `to` names a measure rune ⇒ this edge is an ATTRIBUTE
         * ASSERTION and its weight is that attribute's value, not a strength.
         * Decided by the TARGET only, because the direction is normative. */
        wire.is_value = (to_node->kind == RuneKind::Measure);

        int i = -1, j = -1;
        if (parse_port_relation(wire.relation, i, j)) {
            wire.from_port = i;
            wire.to_port = j;
            if (i == 0 && j == 0) {
                wire.kind = SceneWire::Kind::Fettuccine;
                wire.directed = false; // interaction wires are symmetric
            } else {
                wire.kind = SceneWire::Kind::Linguine;
                ensure_aux_port(*from_node, i, /*as_output=*/true);
                ensure_aux_port(*to_node, j, /*as_output=*/false);
                /* Render style rides the port hints: a wire touching an
                 * adjacency-rendered port on EITHER end is shown by the
                 * bodies touching, not drawn (node-blocks.md). */
                auto port_adjacent = [](const SceneNode& n, int idx, bool out) {
                    for (const auto& p : out ? n.outputs : n.inputs)
                        if (p.index == idx) return p.adjacency;
                    return false;
                };
                if (port_adjacent(*from_node, i, true) || port_adjacent(*to_node, j, false))
                    wire.style = SceneWire::Style::Adjacency;
            }
        } else {
            wire.kind = SceneWire::Kind::Loose;
        }
        scene.wires.push_back(std::move(wire));
    }

    auto_layout(scene, opts);
    return scene;
}

/* THE SMALL ONES ARE REMEMBERED (2026-10-25). A host asks for the same small
 * mantle — its rules, its settings, a document — from many places between two
 * edits, and every ask was an export, a scan for the mantle and a parse. Keyed
 * on Core::revision(), which no two states of any two cores share, a scene asked
 * twice at one revision is projected once.
 *
 * Only the SMALL: a copy of a scene of 10^5 nodes is itself the size of the
 * database, and holding one here would be a second copy of it in every host.
 * The big one is the host's to keep — it already holds the scene it is
 * showing — and the export it is projected from is shared (Core::state_text). */
namespace {
constexpr std::size_t kMemoNodes = 5000; // larger scenes are not kept
constexpr std::size_t kMemoSlots = 16;

struct Memo {
    std::uint64_t rev = 0;
    ProjectOptions opts;
    Scene scene;
};

bool same_options(const ProjectOptions& a, const ProjectOptions& b) {
    return a.mantle == b.mantle && a.col_gap == b.col_gap && a.row_gap == b.row_gap &&
           a.origin_x == b.origin_x && a.origin_y == b.origin_y;
}

std::mutex g_memo_mu;
std::vector<Memo> g_memo; // most recently asked last
} // namespace

Scene project_scene(Core& core, const ProjectOptions& opts) {
    const std::uint64_t rev = core.revision();
    {
        std::lock_guard<std::mutex> g(g_memo_mu);
        for (auto it = g_memo.begin(); it != g_memo.end(); ++it)
            if (it->rev == rev && same_options(it->opts, opts)) {
                std::rotate(it, it + 1, g_memo.end());
                return g_memo.back().scene;
            }
    }
    const auto glyphs = core.glyphs_text();
    const auto state = core.state_text();
    Scene scene = project_scene(*state, *glyphs, opts);
    if (scene.nodes.size() <= kMemoNodes && core.revision() == rev) {
        std::lock_guard<std::mutex> g(g_memo_mu);
        /* A core's old revision never comes back, so what is older goes. With
         * two live cores that costs the other one its entries: a miss, never a
         * wrong scene, and the memo stays a few small scenes. */
        std::erase_if(g_memo, [&](const Memo& m) { return m.rev != rev; });
        if (g_memo.size() >= kMemoSlots) g_memo.erase(g_memo.begin());
        g_memo.push_back({rev, opts, scene});
    }
    return scene;
}

std::string value_label(const Scene& scene, const SceneWire& wire) {
    if (!wire.is_value) return {};
    /* %g rather than a fixed precision: the weight came out of JSON as a
     * double and 5 must read back as "5". */
    char buf[48];
    std::snprintf(buf, sizeof buf, "%g", wire.weight);
    std::string out(buf);
    const SceneNode* measure = scene.find(wire.to);
    if (measure && !measure->quantity.unit.empty()) {
        out += ' ';
        out += measure->quantity.unit;
    }
    return out;
}

std::string extract_mantle(std::string_view state_json, std::string_view name) {
    CJson state{cJSON_ParseWithLength(state_json.data(), state_json.size())};
    if (!state || !cJSON_IsObject(state.p)) return {};
    std::string want(name);
    if (want.empty()) {
        const cJSON* active = cJSON_GetObjectItemCaseSensitive(state.p, "active");
        want = gstr(active, "mantle");
    }
    const cJSON* mantles = cJSON_GetObjectItemCaseSensitive(state.p, "mantles");
    const cJSON* mt = nullptr;
    cJSON_ArrayForEach(mt, mantles) {
        if (want == gstr(mt, "name")) {
            char* printed = cJSON_PrintUnformatted(const_cast<cJSON*>(mt));
            std::string out = printed ? printed : "";
            cJSON_free(printed);
            return out;
        }
    }
    return {};
}

const SceneField* find_field(const SceneNode& node, std::string_view key) {
    for (const auto& f : node.fields)
        if (f.key == key) return &f;
    return nullptr;
}

std::string field_of(const SceneNode& node, std::string_view key) {
    const SceneField* f = find_field(node, key);
    if (!f) return {};
    const std::string& j = f->value_json;
    if (j.empty()) return {};

    /* cJSON does the decoding rather than a hand-rolled escape loop, so
     * \uXXXX arrives as UTF-8 and every escape the model can legally store is
     * handled by the same parser that wrote it. The hand-rolled version this
     * replaces silently mangled \u. */
    CJson v{cJSON_ParseWithLength(j.data(), j.size())};
    if (!v) return j;                       // unreadable → verbatim, never lost
    if (cJSON_IsNull(v.p)) return {};       // `null` == absent; see project.hpp
    if (cJSON_IsString(v.p)) return v.p->valuestring ? v.p->valuestring : "";
    return j;                               // number / bool / array / object
}

std::string decoration(const Merged& merged, const SceneNode& node,
                       std::string_view property) {
    return merged.value(node.name, property);
}

const MergedCell* decoration_cell(const Merged& merged, const SceneNode& node,
                                  std::string_view property) {
    return merged.find(node.name, property);
}

void for_each_decoration(
    const Scene& scene, const Merged& merged, std::string_view property,
    const std::function<void(const SceneNode&, const std::string&)>& fn) {
    if (!fn) return;
    for (const SceneNode& n : scene.nodes) {
        const MergedCell* cell = merged.find(n.name, property);
        if (!cell || cell->conflicted || cell->value.empty()) continue;
        fn(n, cell->value);
    }
}

std::vector<std::string> filter_bag(const SceneNode& node) {
    std::vector<std::string> bag = node.tags;
    bag.push_back(node.name);
    bag.push_back("glyph:" + node.glyph);
    return bag;
}

bool node_matches(std::string_view filter, const SceneNode& node) {
    if (filter.empty()) return true;
    try {
        return Core::tag_match(filter, filter_bag(node));
    } catch (const std::invalid_argument&) {
        return true; // malformed mid-keystroke: degrade to "show everything"
    }
}

} // namespace maiz
