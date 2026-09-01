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
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <unordered_map>

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

void apply_glyph_hints(SceneNode& node, const cJSON* descriptor, const cJSON* content) {
    if (!descriptor) return;
    node.label = gstr(descriptor, "label", node.glyph.c_str());
    const cJSON* hints = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(descriptor), "hints");
    if (!cJSON_IsObject(hints)) return;

    /* Shape (okf/concepts/node-geometry.md): notation bodies. */
    if (const cJSON* shape = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(hints), "shape");
        cJSON_IsObject(shape)) {
        std::string_view kind = gstr(shape, "kind");
        if (kind == "triangle") {
            node.shape = NodeShape::Polygon;
            node.shape_sides = 3;
        } else if (kind == "circle") {
            node.shape = NodeShape::Circle;
        } else if (kind == "polygon") {
            node.shape = NodeShape::Polygon;
            const cJSON* sides = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(shape), "sides");
            node.shape_sides = cJSON_IsNumber(sides) ? std::max(3, (int)sides->valuedouble) : 3;
        } else if (kind == "block") {
            node.shape = NodeShape::Block; // statement block (node-blocks.md)
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
    if (const char* enter_field = gstr(hints, "enter", nullptr); enter_field && content) {
        const cJSON* v =
            cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(content), enter_field);
        if (cJSON_IsString(v)) node.enter_mantle = v->valuestring;
    }

    unsigned rgb = 0;
    if (read_hex_color(gstr(hints, "color", nullptr), rgb)) {
        node.rgb = rgb;
        node.has_color = true;
    }
    if (const cJSON* face = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(hints), "face");
        cJSON_IsObject(face)) {
        const cJSON* w = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(face), "w");
        const cJSON* h = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(face), "h");
        if (cJSON_IsNumber(w)) node.w = (float)w->valuedouble;
        if (cJSON_IsNumber(h)) node.h = (float)h->valuedouble;
    }
    /* Ports: net index 0 is the principal; auxiliaries take 1..n in
     * declaration order (skipping the principal entry wherever it appears). */
    const cJSON* ports = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(hints), "ports");
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
    const cJSON* hints = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(descriptor), "hints");
    const cJSON* editors =
        hints ? cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(hints), "editors") : nullptr;
    const cJSON* labels =
        hints ? cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(hints), "labels") : nullptr;
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
    /* Depth = longest directed-wire distance from a source; relaxation with a
     * node-count bound so cycles terminate. */
    std::unordered_map<std::string, int> depth;
    for (const auto& n : scene.nodes) depth[n.name] = 0;
    const size_t bound = scene.nodes.size();
    for (size_t pass = 0; pass < bound; ++pass) {
        bool changed = false;
        for (const auto& w : scene.wires) {
            if (!w.directed) continue;
            auto f = depth.find(w.from), t = depth.find(w.to);
            if (f == depth.end() || t == depth.end()) continue;
            if (f->second + 1 > t->second && (size_t)(f->second + 1) <= bound) {
                t->second = f->second + 1;
                changed = true;
            }
        }
        if (!changed) break;
    }
    std::map<int, int> rows; // column → next free row (unplaced nodes only)
    for (auto& n : scene.nodes) {
        if (n.placed) continue;
        int col = depth[n.name];
        int row = rows[col]++;
        n.x = opts.origin_x + col * opts.col_gap;
        n.y = opts.origin_y + row * opts.row_gap;
    }
}

} // namespace

Scene project_scene(std::string_view state_json, std::string_view glyphs_json,
                    const ProjectOptions& opts) {
    Scene scene;
    CJson state{cJSON_ParseWithLength(state_json.data(), state_json.size())};
    if (!state || !cJSON_IsObject(state.p)) return scene;

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

    /* Pick the mantle: named, or the active one. */
    std::string want = opts.mantle;
    if (want.empty()) {
        const cJSON* active = cJSON_GetObjectItemCaseSensitive(state.p, "active");
        want = gstr(active, "mantle");
    }
    const cJSON* mantle = nullptr;
    const cJSON* mantles = cJSON_GetObjectItemCaseSensitive(state.p, "mantles");
    const cJSON* mt = nullptr;
    cJSON_ArrayForEach(mt, mantles) {
        if (want == gstr(mt, "name")) { mantle = mt; break; }
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

        SceneNode* from_node = nullptr;
        SceneNode* to_node = nullptr;
        for (auto& n : scene.nodes) {
            if (n.name == wire.from) from_node = &n;
            if (n.name == wire.to) to_node = &n;
        }
        if (!from_node || !to_node) continue; // dangling

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

Scene project_scene(Core& core, const ProjectOptions& opts) {
    Result glyphs = core.dispatch("glyphs");
    return project_scene(core.export_state(), glyphs.data, opts);
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
