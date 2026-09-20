/* canvas.cpp — the 2D canvas: rendering + the interaction grammar (Phase 3).
 *
 * The discipline throughout: rendering is downstream of the Scene, gestures
 * are upstream of the dispatcher, and nothing here writes model state. A drag
 * stages positions in EditorState and draws them as an override; the release
 * compiles ONE command (batch for multi-select) into CanvasIO. If the core
 * rejects it, the next re-projection simply snaps the picture back — there is
 * no local truth to reconcile. */
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/netview.hpp"
#include "voidmaiz/face.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/project.hpp" // value_label: an attribute assertion draws as its value

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace maiz {

CanvasTheme CanvasTheme::dark() {
    CanvasTheme t;
    t.canvas_bg = IM_COL32(24, 24, 28, 255);
    t.grid = IM_COL32(255, 255, 255, 10);
    t.node_bg = IM_COL32(42, 43, 50, 240);
    t.node_border = IM_COL32(15, 15, 18, 255);
    t.node_border_sel = IM_COL32(255, 210, 130, 255);
    t.header_default = IM_COL32(70, 74, 92, 255);
    t.text_dim = IM_COL32(200, 200, 205, 255);
    t.tag_text = IM_COL32(150, 160, 190, 255);
    t.principal = IM_COL32(235, 160, 70, 255);
    t.linguine = IM_COL32(190, 190, 200, 210);
    t.loose = IM_COL32(150, 150, 160, 70);
    t.marquee_fill = IM_COL32(120, 160, 255, 30);
    t.marquee_line = IM_COL32(120, 160, 255, 180);
    t.wire_ok = IM_COL32(120, 220, 130, 230);
    t.wire_bad = IM_COL32(230, 90, 80, 230);
    t.wire_neutral = IM_COL32(200, 200, 210, 200);
    t.chrome = IM_COL32(255, 255, 255, 90);
    t.hover = IM_COL32(130, 180, 255, 200);
    return t;
}

CanvasTheme CanvasTheme::light() {
    CanvasTheme t;
    t.canvas_bg = IM_COL32(243, 243, 246, 255);
    t.grid = IM_COL32(0, 0, 0, 14);
    t.node_bg = IM_COL32(255, 255, 255, 245);
    t.node_border = IM_COL32(150, 150, 158, 255);
    t.node_border_sel = IM_COL32(215, 120, 20, 255);
    t.header_default = IM_COL32(206, 210, 222, 255);
    t.text_dim = IM_COL32(70, 70, 80, 255);
    t.tag_text = IM_COL32(90, 100, 140, 255);
    t.principal = IM_COL32(210, 130, 30, 255);
    t.linguine = IM_COL32(95, 95, 110, 220);
    t.loose = IM_COL32(120, 120, 130, 110);
    t.marquee_fill = IM_COL32(70, 110, 220, 26);
    t.marquee_line = IM_COL32(70, 110, 220, 170);
    t.wire_ok = IM_COL32(40, 160, 60, 240);
    t.wire_bad = IM_COL32(200, 60, 50, 240);
    t.wire_neutral = IM_COL32(100, 100, 110, 220);
    t.chrome = IM_COL32(0, 0, 0, 70);
    t.hover = IM_COL32(50, 110, 230, 200);
    return t;
}

namespace {

/* Black or white, whichever reads against `bg` (headers carry host colors). */
ImU32 contrast_text(ImU32 bg) {
    float lum = 0.299f * (bg & 0xFF) + 0.587f * ((bg >> 8) & 0xFF) +
                0.114f * ((bg >> 16) & 0xFF);
    return lum > 150.0f ? IM_COL32(25, 25, 30, 255) : IM_COL32(240, 240, 245, 255);
}

struct NodeGeom {
    ImVec2 pos;  // screen top-left
    ImVec2 size; // screen size
    float zoom;
};

/* Stable small palette for port types (untyped = gray). */
ImU32 type_color(const std::string& type) {
    if (type.empty()) return IM_COL32(150, 150, 155, 255);
    static const ImU32 palette[] = {
        IM_COL32(230, 170, 60, 255),  IM_COL32(96, 190, 120, 255),
        IM_COL32(90, 160, 235, 255),  IM_COL32(200, 110, 200, 255),
        IM_COL32(220, 100, 90, 255),  IM_COL32(110, 205, 205, 255),
        IM_COL32(180, 190, 90, 255),  IM_COL32(150, 130, 230, 255),
    };
    unsigned h = 2166136261u;
    for (char c : type) h = (h ^ (unsigned char)c) * 16777619u;
    return palette[h % (sizeof(palette) / sizeof(palette[0]))];
}

/* The host's declared style for a type, if any (the port style registry). */
const PortStyle* port_style(const CanvasStyle& s, const std::string& type) {
    if (type.empty() || s.port_types.empty()) return nullptr;
    auto it = s.port_types.find(type);
    return it == s.port_types.end() ? nullptr : &it->second;
}

/* One port marker: the declared shape (or circle) in the type's color, sized
 * to match a radius-r circle's visual weight. Every body kind draws its aux
 * ports through here so the registry cannot be half-applied. */
void draw_port_marker(ImDrawList* dl, const CanvasStyle& s, const std::string& type,
                      const ImVec2& c, float r) {
    const PortStyle* ps = port_style(s, type);
    ImU32 col = ps && ps->color ? ps->color : type_color(type);
    switch (ps ? ps->shape : PortShape::Circle) {
    case PortShape::Circle:
        dl->AddCircleFilled(c, r, col);
        break;
    case PortShape::Ring:
        dl->AddCircle(c, r, col, 0, std::max(1.5f, r * 0.45f));
        break;
    case PortShape::Square: {
        float h = r * 0.9f;
        dl->AddRectFilled(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), col);
        break;
    }
    case PortShape::Diamond: {
        float h = r * 1.25f;
        dl->AddQuadFilled(ImVec2(c.x, c.y - h), ImVec2(c.x + h, c.y), ImVec2(c.x, c.y + h),
                          ImVec2(c.x - h, c.y), col);
        break;
    }
    case PortShape::Triangle: { // pointing up, centroid-centered
        float h = r * 1.2f;
        dl->AddTriangleFilled(ImVec2(c.x, c.y - h), ImVec2(c.x + h * 0.87f, c.y + h * 0.6f),
                              ImVec2(c.x - h * 0.87f, c.y + h * 0.6f), col);
        break;
    }
    }
}

/* The type a linguine reads as: the from-port's, or the to-port's when the
 * from end is an untyped principal (a passive principal→aux wire). */
std::string wire_type_of(const Scene& scene, const SceneWire& w) {
    auto port_type = [&](const std::string& node, int index) -> std::string {
        const SceneNode* n = scene.find(node);
        if (!n || index <= 0) return {};
        for (const auto& p : n->outputs)
            if (p.index == index) return p.type;
        for (const auto& p : n->inputs)
            if (p.index == index) return p.type;
        return {};
    };
    std::string t = port_type(w.from, w.from_port);
    return t.empty() ? port_type(w.to, w.to_port) : t;
}

constexpr float kPi = 3.14159265358979f;

/* Canvas text size: the WORLD's font (base size × zoom), independent of the
 * UI's global font scaling. A touch shell scales chrome 3–4× for fingers;
 * node names must scale with the CAMERA, not the chrome — otherwise phone
 * text renders UI-scale × zoom and buries the nodes. */
float canvas_font(float zoom) {
    float ui = ImGui::GetIO().FontGlobalScale * ImGui::GetStyle().FontScaleMain;
    if (ui <= 0.0f) ui = 1.0f;
    return ImGui::GetFontSize() / ui * zoom;
}

/* Node-name text can overflow a small body onto the canvas; white contrast
 * text then vanishes on a light background. A dark halo keeps it readable
 * wherever it lands (dark text needs none — both canvas themes are lighter
 * than it). */
void text_with_halo(ImDrawList* dl, float font, ImVec2 pos, ImU32 color,
                    const char* text) {
    bool is_light_text = (color & 0xFF) > 150; // contrast_text: near-white or near-black
    if (is_light_text) {
        ImU32 halo = IM_COL32(20, 20, 25, 160);
        dl->AddText(nullptr, font, ImVec2(pos.x + 1.5f, pos.y + 1.5f), halo, text);
    }
    dl->AddText(nullptr, font, pos, color, text);
}

float node_height(const SceneNode& n, const CanvasStyle& s) {
    if (n.shape == NodeShape::Block) // statement blocks: short and wide
        return n.h > 0 ? n.h : s.block.default_h;
    if (n.shape != NodeShape::Window) // notation bodies: a square-ish box
        return n.h > 0 ? n.h : (n.w > 0 ? n.w : 84.0f);
    if (n.collapsed) return s.header_h;
    size_t rows = std::max<size_t>(std::max(n.inputs.size(), n.outputs.size()), 1);
    float computed = s.header_h + rows * s.port_row + (n.tags.empty() ? 6.0f : 22.0f);
    return std::max(n.h, computed);
}

float node_width(const SceneNode& n, const CanvasStyle& s) {
    if (n.w > 0) return n.w;
    if (n.shape == NodeShape::Block) return s.block.default_w;
    return n.shape == NodeShape::Window ? s.node_w : 84.0f;
}

/* World position of a node, honoring an in-flight drag's staged override. */
ImVec2 world_pos(const SceneNode& n, const EditorState* ed) {
    if (ed) {
        auto it = ed->staged.find(n.name);
        if (it != ed->staged.end()) return ImVec2(it->second.first, it->second.second);
    }
    return ImVec2(n.x, n.y);
}

/* This frame's fx override for a node, if any. */
const NodeFx* node_fx(const CanvasFx* fx, const SceneNode& n) {
    if (!fx) return nullptr;
    auto it = fx->nodes.find(n.name);
    return it == fx->nodes.end() ? nullptr : &it->second;
}

/* World CENTER of a node — the anchor animations transform around. */
ImVec2 world_center(const SceneNode& n, const CanvasStyle& s, const EditorState* ed,
                    const CanvasFx* fx) {
    if (const NodeFx* f = node_fx(fx, n)) return ImVec2(f->cx, f->cy);
    ImVec2 wp = world_pos(n, ed);
    return ImVec2(wp.x + node_width(n, s) * 0.5f, wp.y + node_height(n, s) * 0.5f);
}

NodeGeom geom(const SceneNode& n, const Camera& cam, const ImVec2& origin,
              const CanvasStyle& s, const EditorState* ed, const CanvasFx* fx) {
    NodeGeom g;
    g.zoom = cam.zoom;
    float w = node_width(n, s);
    float h = node_height(n, s);
    if (ed && !n.collapsed && ed->drag == EditorState::Drag::Resize &&
        ed->resize_node == n.name) { // staged resize wins mid-gesture
        w = ed->resize_w;
        h = ed->resize_h;
    }
    ImVec2 c = world_center(n, s, ed, fx);
    if (const NodeFx* f = node_fx(fx, n)) {
        w *= f->scale;
        h *= f->scale;
    }
    g.pos = ImVec2(origin.x + (c.x - w * 0.5f - cam.x) * cam.zoom,
                   origin.y + (c.y - h * 0.5f - cam.y) * cam.zoom);
    g.size = ImVec2(w * cam.zoom, h * cam.zoom);
    return g;
}

/* A shaped node's orientation (radians, screen convention): auto → point the
 * principal at its wire partner's center; free/declared → the stored angle.
 * World-space, zoom-independent. */
float node_theta(const Scene& scene, const SceneNode& n, const CanvasStyle& s,
                 const EditorState* ed, const CanvasFx* fx) {
    if (n.shape == NodeShape::Window || n.shape == NodeShape::Block) return 0.0f;
    if (n.rot_auto) {
        const SceneNode* other = nullptr;
        for (const auto& w : scene.wires) {
            if (w.kind == SceneWire::Kind::Loose) continue;
            if (w.from == n.name && w.from_port == 0) { other = scene.find(w.to); break; }
            if (w.to == n.name && w.to_port == 0) { other = scene.find(w.from); break; }
        }
        if (other) {
            ImVec2 a = world_center(n, s, ed, fx);
            ImVec2 b = world_center(*other, s, ed, fx);
            if (a.x != b.x || a.y != b.y) return std::atan2(b.y - a.y, b.x - a.x);
        }
    }
    return n.rot * kPi / 180.0f;
}

/* Perimeter anchor of a shaped node's port: the principal at the apex,
 * auxiliaries along the opposite feature (a triangle's base edge; the
 * opposite arc otherwise), in index order. */
/* The drawn radius of a notation body (circle/polygon). One definition,
 * because the anchor math, the body drawing and the wire clip must agree —
 * they were three copies of 0.44f before a wire needed to land on the edge. */
float shaped_radius(const NodeGeom& g) {
    return std::min(g.size.x, g.size.y) * 0.44f;
}

/* Where a wire drawn toward `dir` (a unit vector out of the body's centre)
 * meets that body's boundary. Used for LOOSE wires, which have no port to ask.
 *
 * Reported by Void Mago 2026-09-04, whose graph is entirely loose wires: a
 * centre-to-centre line starts underneath the node it belongs to and emerges
 * from the far side, so it reads as passing THROUGH the node rather than out
 * of it, and it crosses unrelated bodies on the way. Clipping to the boundary
 * is most of the readability win and needs no routing at all. */
ImVec2 body_edge(const SceneNode& n, const NodeGeom& g, const ImVec2& dir) {
    ImVec2 c(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y * 0.5f);
    if (n.shape == NodeShape::Circle || n.shape == NodeShape::Polygon) {
        /* A polygon takes its circumradius, so a wire meeting a flat side
         * stops a hair outside it. A small gap reads as a wire touching the
         * body; the alternative (exact edge intersection per side count) is
         * more math than the difference is worth at these sizes. */
        float r = shaped_radius(g);
        return ImVec2(c.x + dir.x * r, c.y + dir.y * r);
    }
    /* Window and Block: the ray from the centre, clipped to the half-extents. */
    const float eps = 1e-4f;
    float hx = g.size.x * 0.5f, hy = g.size.y * 0.5f;
    float tx = hx / std::max(std::fabs(dir.x), eps);
    float ty = hy / std::max(std::fabs(dir.y), eps);
    float t = std::min(tx, ty);
    return ImVec2(c.x + dir.x * t, c.y + dir.y * t);
}

ImVec2 shaped_anchor(const SceneNode& n, const NodeGeom& g, int index, float theta) {
    ImVec2 c(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y * 0.5f);
    float r = shaped_radius(g);
    auto at = [&](float ang, float rad) {
        return ImVec2(c.x + rad * std::cos(ang), c.y + rad * std::sin(ang));
    };
    if (index <= 0) return at(theta, r);
    int arity = (int)(n.inputs.size() + n.outputs.size());
    float f = (float)index / (float)(arity + 1);
    if (n.shape == NodeShape::Polygon && n.shape_sides == 3) {
        ImVec2 v1 = at(theta + 2.0f * kPi / 3.0f, r); // the base edge
        ImVec2 v2 = at(theta - 2.0f * kPi / 3.0f, r);
        return ImVec2(v1.x + (v2.x - v1.x) * f, v1.y + (v2.y - v1.y) * f);
    }
    float spread = kPi * 0.9f; // opposite arc
    return at(theta + kPi - spread * 0.5f + spread * f, r);
}

/* Anchor for a net port index: 0 = principal (top-center); auxiliary inputs
 * sit on the left edge, outputs on the right, in list order. `prefer_out`
 * breaks the tie when an index exists on both sides. */
ImVec2 port_anchor(const SceneNode& n, const NodeGeom& g, int index, bool prefer_out,
                   const CanvasStyle& s, float theta = 0.0f) {
    if (n.shape == NodeShape::Block) {
        // the same world math the snap gesture uses, scaled by the camera —
        // gesture and drawing can never disagree about a connector
        float ax, ay;
        if (index > 0 &&
            (block_anchor(n, index, prefer_out, g.pos.x, g.pos.y, g.size.x, g.size.y,
                          g.zoom, s.block, ax, ay) ||
             block_anchor(n, index, !prefer_out, g.pos.x, g.pos.y, g.size.x, g.size.y,
                          g.zoom, s.block, ax, ay)))
            return ImVec2(ax, ay);
        // the principal: reserved for Phase-B execution; undrawn, left edge
        return ImVec2(g.pos.x, g.pos.y + g.size.y * 0.5f);
    }
    if (n.shape != NodeShape::Window) return shaped_anchor(n, g, index, theta);
    if (index <= 0) return ImVec2(g.pos.x + g.size.x * 0.5f, g.pos.y);
    auto row_y = [&](size_t row) {
        if (n.collapsed) // folded: aux wires gather at the edge midpoints
            return g.pos.y + g.size.y * 0.5f;
        return g.pos.y + (s.header_h + (row + 0.5f) * s.port_row) * g.zoom;
    };
    const auto& first = prefer_out ? n.outputs : n.inputs;
    const auto& second = prefer_out ? n.inputs : n.outputs;
    for (size_t r = 0; r < first.size(); ++r)
        if (first[r].index == index)
            return ImVec2(prefer_out ? g.pos.x + g.size.x : g.pos.x, row_y(r));
    for (size_t r = 0; r < second.size(); ++r)
        if (second[r].index == index)
            return ImVec2(prefer_out ? g.pos.x : g.pos.x + g.size.x, row_y(r));
    return ImVec2(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y); // unknown: bottom-center
}

void draw_grid(ImDrawList* dl, const ImVec2& origin, const ImVec2& size,
               const Camera& cam, const CanvasStyle& s) {
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                      s.theme.canvas_bg);
    const ImU32 line = s.theme.grid;
    float step = s.grid_step * cam.zoom;
    if (step < 4.0f) return;
    float x0 = std::fmod(-cam.x * cam.zoom, step);
    if (x0 < 0) x0 += step;
    for (float x = x0; x < size.x; x += step)
        dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), line);
    float y0 = std::fmod(-cam.y * cam.zoom, step);
    if (y0 < 0) y0 += step;
    for (float y = y0; y < size.y; y += step)
        dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), line);
}

/* Outward unit tangent at a port anchor: shaped bodies use the perimeter
 * normal; window nodes keep their conventions (principal up, aux sideways). */
ImVec2 port_tangent(const SceneNode& n, const NodeGeom& g, int index, const ImVec2& anchor) {
    ImVec2 c(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y * 0.5f);
    if (n.shape == NodeShape::Block) { // out of whichever edge holds the anchor
        if (anchor.y <= g.pos.y + 1.0f) return ImVec2(0, -1);
        if (anchor.y >= g.pos.y + g.size.y - 1.0f) return ImVec2(0, 1);
        return ImVec2(anchor.x <= g.pos.x + 1.0f ? -1.0f : 1.0f, 0);
    }
    if (n.shape != NodeShape::Window) {
        float dx = anchor.x - c.x, dy = anchor.y - c.y;
        float len = std::sqrt(dx * dx + dy * dy);
        return len > 0.001f ? ImVec2(dx / len, dy / len) : ImVec2(0, -1);
    }
    if (index <= 0) return ImVec2(0, -1);
    return ImVec2(anchor.x > c.x ? 1.0f : -1.0f, 0.0f);
}

void draw_wire(ImDrawList* dl, const SceneWire& w, const ImVec2& a, const ImVec2& ta,
               const ImVec2& b, const ImVec2& tb, float zoom, const CanvasTheme& t,
               ImU32 linguine_col) {
    float dist = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    float reach = std::max(30.0f * zoom, dist * 0.38f);
    ImVec2 c1(a.x + ta.x * reach, a.y + ta.y * reach);
    ImVec2 c2(b.x + tb.x * reach, b.y + tb.y * reach);
    switch (w.kind) {
    case SceneWire::Kind::Fettuccine: {
        /* Principal-to-principal, leaving along each body's pointing
         * direction — facing apexes connect near-straight. A host-marked
         * ACTIVE pair (a live redex) gets a hot halo. */
        if (w.active) {
            ImU32 hot = (t.node_border_sel & 0x00FFFFFF) | (110u << 24);
            dl->AddBezierCubic(a, c1, c2, b, hot, 12.0f * zoom);
        }
        ImU32 halo = (t.principal & 0x00FFFFFF) | (70u << 24);
        dl->AddBezierCubic(a, c1, c2, b, halo, 7.0f * zoom);
        dl->AddBezierCubic(a, c1, c2, b, t.principal, 3.5f * zoom);
        break;
    }
    case SceneWire::Kind::Linguine: {
        dl->AddBezierCubic(a, c1, c2, b, linguine_col, 2.0f * zoom);
        if (w.directed) { // arrowhead pointing into the destination, along -tb
            float r = 5.0f * zoom;
            ImVec2 perp(-tb.y, tb.x);
            dl->AddTriangleFilled(ImVec2(b.x + tb.x * r - perp.x * r * 0.7f,
                                         b.y + tb.y * r - perp.y * r * 0.7f),
                                  ImVec2(b.x + tb.x * r + perp.x * r * 0.7f,
                                         b.y + tb.y * r + perp.y * r * 0.7f),
                                  b, linguine_col);
        }
        break;
    }
    case SceneWire::Kind::Loose:
        dl->AddLine(a, b, t.loose, 1.0f * zoom);
        break;
    }
}

/* A notation body (triangle/polygon/circle): filled glyph-colored shape, the
 * principal marked at the apex, aux dots along the opposite feature, the
 * name centered. No chrome — these are drawings, not windows. */
void draw_shaped_node(ImDrawList* dl, const SceneNode& n, const NodeGeom& g,
                      const CanvasStyle& s, bool is_selected, float theta) {
    const CanvasTheme& t = s.theme;
    float z = g.zoom;
    ImVec2 c(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y * 0.5f);
    float r = std::min(g.size.x, g.size.y) * 0.44f;
    ImU32 fill = n.has_color
                     ? IM_COL32((n.rgb >> 16) & 0xFF, (n.rgb >> 8) & 0xFF, n.rgb & 0xFF, 255)
                     : t.header_default;
    ImU32 text = contrast_text(fill);
    ImU32 border = is_selected ? t.node_border_sel : t.node_border;
    float border_w = is_selected ? 2.5f : 1.5f;

    if (n.shape == NodeShape::Circle) {
        dl->AddCircleFilled(c, r, fill);
        dl->AddCircle(c, r, border, 0, border_w);
    } else {
        ImVec2 pts[16];
        int k = std::min(n.shape_sides, 16);
        for (int i = 0; i < k; ++i) {
            float a = theta + 2.0f * kPi * (float)i / (float)k;
            pts[i] = ImVec2(c.x + r * std::cos(a), c.y + r * std::sin(a));
        }
        dl->AddConvexPolyFilled(pts, k, fill);
        dl->AddPolyline(pts, k, border, ImDrawFlags_Closed, border_w);
    }

    // principal marker at the apex; aux dots along the opposite feature
    ImVec2 pp = shaped_anchor(n, g, 0, theta);
    dl->AddCircleFilled(pp, (s.port_radius + 1.0f) * z, t.principal);
    for (const auto& lists : {&n.inputs, &n.outputs})
        for (const auto& port : *lists)
            draw_port_marker(dl, s, port.type, shaped_anchor(n, g, port.index, theta),
                             s.port_radius * z);

    float font = canvas_font(z);
    std::string title = n.name;
    if (!n.enter_mantle.empty()) title += " >";
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, title.c_str());
    text_with_halo(dl, font, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), text,
                   title.c_str());
    if (!n.tags.empty()) {
        std::string line;
        for (const auto& tg : n.tags) {
            if (!line.empty()) line += ' ';
            line += '@' + tg;
        }
        float small = font * 0.78f;
        ImVec2 ls = ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, line.c_str());
        dl->AddText(nullptr, small, ImVec2(c.x - ls.x * 0.5f, g.pos.y + g.size.y - ls.y),
                    t.tag_text, line.c_str());
    }

    // resize grip at the bounding-box corner, selected shapes only (that's
    // also the gesture's arming rule — the corner lies outside the body)
    if (is_selected) {
        ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
        dl->AddTriangleFilled(ImVec2(br.x - 1, br.y - 10 * z), ImVec2(br.x - 1, br.y - 1),
                              ImVec2(br.x - 10 * z, br.y - 1), t.chrome);
    }
}

/* A statement block (okf/concepts/node-blocks.md): glyph-colored rounded
 * body whose SILHOUETTE is the notation — a notch on top where a "prev"
 * input exists, a tab below where a "next" output does (connector shape =
 * port type, drawn geometrically). Value sockets dot the right edge, a
 * reporter's plug the left. No window chrome; the prev/next connectors have
 * no dots — the notch/tab IS the marker, and their wires render as
 * adjacency. */
void draw_block_node(ImDrawList* dl, const SceneNode& n, const NodeGeom& g,
                     const CanvasStyle& s, bool is_selected) {
    const CanvasTheme& t = s.theme;
    float z = g.zoom;
    ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
    float round = s.rounding * z;
    ImU32 fill = n.has_color
                     ? IM_COL32((n.rgb >> 16) & 0xFF, (n.rgb >> 8) & 0xFF, n.rgb & 0xFF, 255)
                     : t.header_default;
    ImU32 text = contrast_text(fill);
    ImU32 border = is_selected ? t.node_border_sel : t.node_border;

    bool has_prev = false, has_next = false;
    for (const auto& p : n.inputs)
        if (p.name == "prev") has_prev = true;
    for (const auto& p : n.outputs)
        if (p.name == "next") has_next = true;

    float notch = std::min(s.block.notch_x * z, g.size.x * 0.45f);
    float nw = s.block.notch_w * z * 0.5f, nh = s.block.notch_h * z;

    dl->AddRectFilled(g.pos, br, fill, round);
    if (has_next) // the tab protrudes below the body
        dl->AddRectFilled(ImVec2(g.pos.x + notch - nw, br.y - 1),
                          ImVec2(g.pos.x + notch + nw, br.y + nh), fill, nh * 0.5f);
    dl->AddRect(g.pos, br, border, round, 0, is_selected ? 2.5f : 1.5f);
    if (has_prev) // the notch indents the top edge (canvas shows through)
        dl->AddRectFilled(ImVec2(g.pos.x + notch - nw, g.pos.y - 1),
                          ImVec2(g.pos.x + notch + nw, g.pos.y + nh), t.canvas_bg,
                          nh * 0.5f);

    // value sockets (right edge) and a reporter's plug (left edge) keep dots
    for (const auto& p : n.inputs)
        if (p.name != "prev") {
            float ax, ay;
            if (block_anchor(n, p.index, false, g.pos.x, g.pos.y, g.size.x, g.size.y,
                             z, s.block, ax, ay))
                draw_port_marker(dl, s, p.type, ImVec2(ax, ay), s.port_radius * z);
        }
    for (const auto& p : n.outputs)
        if (p.name != "next") {
            float ax, ay;
            if (block_anchor(n, p.index, true, g.pos.x, g.pos.y, g.size.x, g.size.y,
                             z, s.block, ax, ay))
                draw_port_marker(dl, s, p.type, ImVec2(ax, ay), s.port_radius * z);
        }

    float font = canvas_font(z);
    std::string title = n.label.empty() ? n.name : n.label;
    if (!n.enter_mantle.empty()) title += " >"; // C-block: double-click enters
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.0f, title.c_str());
    text_with_halo(dl, font, ImVec2(g.pos.x + 10 * z, g.pos.y + (g.size.y - ts.y) * 0.5f),
                   text, title.c_str());

    if (!n.tags.empty()) {
        std::string line;
        for (const auto& tg : n.tags) {
            if (!line.empty()) line += ' ';
            line += '@' + tg;
        }
        float small = font * 0.72f;
        ImVec2 ls = ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, line.c_str());
        dl->AddText(nullptr, small,
                    ImVec2(br.x - ls.x - 10 * z, g.pos.y + (g.size.y - ls.y) * 0.5f),
                    t.tag_text, line.c_str());
    }

    if (is_selected) { // resize grip, same rule as shaped bodies
        dl->AddTriangleFilled(ImVec2(br.x - 1, br.y - 10 * z), ImVec2(br.x - 1, br.y - 1),
                              ImVec2(br.x - 10 * z, br.y - 1), t.chrome);
    }
}

void draw_node(ImDrawList* dl, const SceneNode& n, const NodeGeom& g,
               const CanvasStyle& s, bool is_selected, float theta) {
    if (n.shape == NodeShape::Block) {
        draw_block_node(dl, n, g, s, is_selected);
        return;
    }
    if (n.shape != NodeShape::Window) {
        draw_shaped_node(dl, n, g, s, is_selected, theta);
        return;
    }
    float z = g.zoom;
    ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
    float round = s.rounding * z;
    const CanvasTheme& t = s.theme;
    ImU32 header = n.has_color
                       ? IM_COL32((n.rgb >> 16) & 0xFF, (n.rgb >> 8) & 0xFF, n.rgb & 0xFF, 255)
                       : t.header_default;
    ImU32 header_text = contrast_text(header);

    dl->AddRectFilled(g.pos, br, t.node_bg, round);
    dl->AddRectFilled(g.pos, ImVec2(br.x, g.pos.y + s.header_h * z), header, round,
                      ImDrawFlags_RoundCornersTop);
    if (is_selected)
        dl->AddRect(g.pos, br, t.node_border_sel, round, 0, 2.5f);
    else
        dl->AddRect(g.pos, br, t.node_border, round, 0, 1.5f);

    float font = canvas_font(z);

    /* Collapse dot (title-bar chrome): filled = collapsed, ring = expanded. */
    ImVec2 dot(g.pos.x + 12 * z, g.pos.y + s.header_h * 0.5f * z);
    if (n.collapsed)
        dl->AddCircleFilled(dot, 4.0f * z, header_text);
    else
        dl->AddCircle(dot, 4.0f * z, header_text, 0, 1.5f * z);

    std::string title = n.name;
    if (!n.enter_mantle.empty()) title += "  >" + n.enter_mantle; // subgraph marker
    text_with_halo(dl, font, ImVec2(g.pos.x + 22 * z, g.pos.y + 5 * z), header_text,
                   title.c_str());

    /* Principal port: a diamond on the top edge — every node has exactly one. */
    ImVec2 pp(g.pos.x + g.size.x * 0.5f, g.pos.y);
    float pr = (s.port_radius + 1.5f) * z;
    dl->AddQuadFilled(ImVec2(pp.x, pp.y - pr), ImVec2(pp.x + pr, pp.y),
                      ImVec2(pp.x, pp.y + pr), ImVec2(pp.x - pr, pp.y), t.principal);

    if (n.collapsed) return; // header-only chrome: no ports, tags, or grip

    /* Resize grip, bottom-right. */
    dl->AddTriangleFilled(ImVec2(br.x - 10 * z, br.y), ImVec2(br.x, br.y - 10 * z), br,
                          t.chrome);

    float small = font * 0.82f;
    for (size_t r = 0; r < n.inputs.size(); ++r) {
        float y = g.pos.y + (s.header_h + (r + 0.5f) * s.port_row) * z;
        draw_port_marker(dl, s, n.inputs[r].type, ImVec2(g.pos.x, y), s.port_radius * z);
        dl->AddText(nullptr, small, ImVec2(g.pos.x + 8 * z, y - small * 0.5f), t.text_dim,
                    n.inputs[r].name.c_str());
    }
    for (size_t r = 0; r < n.outputs.size(); ++r) {
        float y = g.pos.y + (s.header_h + (r + 0.5f) * s.port_row) * z;
        draw_port_marker(dl, s, n.outputs[r].type, ImVec2(br.x, y), s.port_radius * z);
        ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, n.outputs[r].name.c_str());
        dl->AddText(nullptr, small, ImVec2(br.x - ts.x - 8 * z, y - small * 0.5f), t.text_dim,
                    n.outputs[r].name.c_str());
    }

    if (!n.tags.empty()) {
        std::string line;
        for (const auto& tg : n.tags) {
            if (!line.empty()) line += ' ';
            line += '@' + tg;
        }
        dl->AddText(nullptr, small, ImVec2(g.pos.x + 8 * z, br.y - 17 * z), t.tag_text,
                    line.c_str());
    }
}

/* A wire's screen endpoints + outward tangents (loose wires run center to
 * center). Shared by the render pass and hover detection. */
bool wire_endpoints(const Scene& scene, const SceneWire& w, const Camera& cam,
                    const ImVec2& origin, const CanvasStyle& style, const EditorState* ed,
                    const CanvasFx* fx, ImVec2& a, ImVec2& ta, ImVec2& b, ImVec2& tb) {
    const SceneNode* fn = scene.find(w.from);
    const SceneNode* tn = scene.find(w.to);
    if (!fn || !tn) return false;
    NodeGeom fg = geom(*fn, cam, origin, style, ed, fx);
    NodeGeom tg = geom(*tn, cam, origin, style, ed, fx);
    ta = tb = ImVec2(0, -1);
    if (w.kind == SceneWire::Kind::Loose) {
        /* Centres first, then out to each body's boundary along the run.
         *
         * THE TANGENTS ARE DERIVED HERE RATHER THAN LEFT AT (0,-1), and that
         * half is a latent fix rather than a visible one: every loose path
         * draws a straight line (draw_wire's Loose case, the hover highlight,
         * and hit_wire's linear walk all ignore ta/tb), so the upward default
         * was dead for this kind. Void Mago read it as the cause of humped
         * wires on 2026-09-04; it cannot be, but a dead value that is wrong
         * becomes a live bug the first time anyone curves a loose wire — which
         * is exactly what their own §2.2 asks for. Correct now, cheaply.
         *
         * The VISIBLE fix is the anchoring below. */
        ImVec2 fc(fg.pos.x + fg.size.x * 0.5f, fg.pos.y + fg.size.y * 0.5f);
        ImVec2 tc(tg.pos.x + tg.size.x * 0.5f, tg.pos.y + tg.size.y * 0.5f);
        ImVec2 d(tc.x - fc.x, tc.y - fc.y);
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len > 0.001f) {
            ta = ImVec2(d.x / len, d.y / len);
            tb = ImVec2(-ta.x, -ta.y);
            a = body_edge(*fn, fg, ta);
            b = body_edge(*tn, tg, tb);
        } else { // stacked nodes: no direction to derive, so keep the centres
            a = fc;
            b = tc;
        }
    } else {
        a = port_anchor(*fn, fg, w.from_port, /*prefer_out=*/true, style,
                        node_theta(scene, *fn, style, ed, fx));
        b = port_anchor(*tn, tg, w.to_port, /*prefer_out=*/false, style,
                        node_theta(scene, *tn, style, ed, fx));
        ta = port_tangent(*fn, fg, w.from_port, a);
        tb = port_tangent(*tn, tg, w.to_port, b);
    }
    return true;
}

void render_scene(ImDrawList* dl, const Scene& scene, const Camera& cam,
                  const ImVec2& origin, const ImVec2& avail, const CanvasStyle& style,
                  const EditorState* ed, const CanvasFx* fx) {
    draw_grid(dl, origin, avail, cam, style);
    for (const auto& w : scene.wires) { // wires beneath nodes
        ImVec2 a, b, ta, tb;
        if (!wire_endpoints(scene, w, cam, origin, style, ed, fx, a, ta, b, tb)) continue;
        if (w.style == SceneWire::Style::Adjacency) {
            // adjacency IS the visible connection: flush endpoints draw
            // nothing; separated-but-linked blocks get a dim honest tether
            float dx = b.x - a.x, dy = b.y - a.y;
            float eps = 3.0f * cam.zoom + 1.5f;
            if (dx * dx + dy * dy <= eps * eps) continue;
            dl->AddLine(a, b, (style.theme.linguine & 0x00FFFFFF) | (90u << 24),
                        1.5f * cam.zoom);
            continue;
        }
        ImU32 ling = style.theme.linguine; // typed wires read as their type
        if (w.kind == SceneWire::Kind::Linguine)
            if (const PortStyle* ps = port_style(style, wire_type_of(scene, w));
                ps && ps->color)
                ling = ps->color;
        draw_wire(dl, w, a, ta, b, tb, cam.zoom, style.theme, ling);

        /* AN ATTRIBUTE ASSERTION IS LABELLED, NOT THICKENED (SPEC §3.7.1).
         * When `to` is a measure rune the weight is the attribute's VALUE, and
         * a value is not comparable to the strengths on the rest of the canvas
         * — drawing "900 rpm" nine hundred times heavier than "supports, 1.0"
         * would be a lie the renderer told on the model's behalf. So the number
         * is written out, with its unit, at the midpoint of the wire it belongs
         * to. Nothing is drawn for an ordinary weight: a strength has no
         * canonical rendering yet, and inventing one here would decide a
         * question the canvas has not been asked. */
        if (w.is_value && cam.zoom > 0.45f) {
            std::string vl = value_label(scene, w);
            if (!vl.empty()) {
                float small = ImGui::GetFontSize() * 0.85f * cam.zoom;
                ImVec2 ts = ImGui::CalcTextSize(vl.c_str());
                ts.x *= small / ImGui::GetFontSize();
                ImVec2 mid(0.5f * (a.x + b.x), 0.5f * (a.y + b.y));
                text_with_halo(dl, small, ImVec2(mid.x - ts.x * 0.5f, mid.y - small * 0.5f),
                               style.theme.text_dim, vl.c_str());
            }
        }
    }
    // ghosts: nodes the model no longer holds, drawn mid-animation (a rewrite's
    // smush). Above wires, below live nodes; explicit rot (no scene partner).
    if (fx)
        for (const auto& gh : fx->ghosts) {
            if (gh.scale <= 0.02f) continue;
            NodeGeom g;
            g.zoom = cam.zoom;
            float w = node_width(gh.node, style) * gh.scale;
            float h = node_height(gh.node, style) * gh.scale;
            g.pos = ImVec2(origin.x + (gh.cx - w * 0.5f - cam.x) * cam.zoom,
                           origin.y + (gh.cy - h * 0.5f - cam.y) * cam.zoom);
            g.size = ImVec2(w * cam.zoom, h * cam.zoom);
            draw_node(dl, gh.node, g, style, false, gh.node.rot * kPi / 180.0f);
        }
    for (const auto& n : scene.nodes) {
        if (const NodeFx* f = node_fx(fx, n); f && f->scale <= 0.02f) continue;
        draw_node(dl, n, geom(n, cam, origin, style, ed, fx), style,
                  ed && ed->selected(n.name), node_theta(scene, n, style, ed, fx));
    }
}

/* Topmost node under a screen point (last drawn wins). Shaped bodies hit
 * their true geometry, not the box. */
const SceneNode* hit_test(const Scene& scene, const Camera& cam, const ImVec2& origin,
                          const CanvasStyle& style, const EditorState* ed,
                          const CanvasFx* fx, ImVec2 p) {
    for (auto it = scene.nodes.rbegin(); it != scene.nodes.rend(); ++it) {
        if (const NodeFx* f = node_fx(fx, *it); f && f->scale <= 0.02f) continue;
        NodeGeom g = geom(*it, cam, origin, style, ed, fx);
        if (p.x < g.pos.x || p.x > g.pos.x + g.size.x || p.y < g.pos.y ||
            p.y > g.pos.y + g.size.y)
            continue;
        if (it->shape == NodeShape::Window || it->shape == NodeShape::Block) return &*it;
        ImVec2 c(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y * 0.5f);
        float r = std::min(g.size.x, g.size.y) * 0.44f;
        if (it->shape == NodeShape::Circle) {
            float dx = p.x - c.x, dy = p.y - c.y;
            if (dx * dx + dy * dy <= r * r) return &*it;
            continue;
        }
        // convex polygon: consistent cross-product sign
        float theta = node_theta(scene, *it, style, ed, fx);
        int k = std::min(it->shape_sides, 16);
        bool inside = true;
        for (int i = 0; i < k && inside; ++i) {
            float a0 = theta + 2.0f * kPi * (float)i / (float)k;
            float a1 = theta + 2.0f * kPi * (float)(i + 1) / (float)k;
            ImVec2 v0(c.x + r * std::cos(a0), c.y + r * std::sin(a0));
            ImVec2 v1(c.x + r * std::cos(a1), c.y + r * std::sin(a1));
            float cross = (v1.x - v0.x) * (p.y - v0.y) - (v1.y - v0.y) * (p.x - v0.x);
            if (cross < 0) inside = false;
        }
        if (inside) return &*it;
    }
    return nullptr;
}

struct PortHit {
    const SceneNode* node = nullptr;
    int port = 0;          // 0 = principal
    bool is_output = false;
    std::string type;
    ImVec2 anchor;
};

/* Topmost port under a screen point (checked before node bodies, since port
 * dots straddle the node edge). */
bool hit_port(const Scene& scene, const Camera& cam, const ImVec2& origin,
              const CanvasStyle& style, const EditorState* ed, const CanvasFx* fx,
              ImVec2 p, PortHit& out) {
    float r = std::max(style.port_hit_radius, style.port_radius * cam.zoom + 4.0f);
    auto near = [&](ImVec2 a) {
        float dx = p.x - a.x, dy = p.y - a.y;
        return dx * dx + dy * dy <= r * r;
    };
    for (auto it = scene.nodes.rbegin(); it != scene.nodes.rend(); ++it) {
        if (const NodeFx* f = node_fx(fx, *it); f && f->scale <= 0.02f) continue;
        NodeGeom g = geom(*it, cam, origin, style, ed, fx);
        float theta = node_theta(scene, *it, style, ed, fx);
        if (it->shape != NodeShape::Block) { // a block's principal is reserved
            ImVec2 pp = port_anchor(*it, g, 0, false, style, theta); // principal
            if (near(pp)) {
                out = {&*it, 0, false, {}, pp};
                return true;
            }
        }
        for (const auto& port : it->inputs) {
            ImVec2 a = port_anchor(*it, g, port.index, /*prefer_out=*/false, style, theta);
            if (near(a)) {
                out = {&*it, port.index, false, port.type, a};
                return true;
            }
        }
        for (const auto& port : it->outputs) {
            ImVec2 a = port_anchor(*it, g, port.index, /*prefer_out=*/true, style, theta);
            if (near(a)) {
                out = {&*it, port.index, true, port.type, a};
                return true;
            }
        }
    }
    return false;
}

/* The linguine currently feeding an input port, if any (inputs hold one wire;
 * dropping on an occupied one rewires). */
const SceneWire* wire_into(const Scene& scene, const std::string& node, int port) {
    for (const auto& w : scene.wires)
        if (w.kind == SceneWire::Kind::Linguine && w.to == node && w.to_port == port)
            return &w;
    return nullptr;
}

/* A principal's standing fettuccine, if any (strictly one-to-one). */
const SceneWire* fettuccine_of(const Scene& scene, const std::string& node) {
    for (const auto& w : scene.wires)
        if (w.kind == SceneWire::Kind::Fettuccine && (w.from == node || w.to == node))
            return &w;
    return nullptr;
}

/* The wire nearest a screen point, within max_dist px of its drawn curve
 * (beziers sampled; loose wires as a straight run). Hover highlighting today;
 * wire selection later. */
const SceneWire* hit_wire(const Scene& scene, const Camera& cam, const ImVec2& origin,
                          const CanvasStyle& style, const EditorState* ed,
                          const CanvasFx* fx, ImVec2 p, float max_dist) {
    const SceneWire* best = nullptr;
    float best_d2 = max_dist * max_dist;
    for (const auto& w : scene.wires) {
        ImVec2 a, b, ta, tb;
        if (!wire_endpoints(scene, w, cam, origin, style, ed, fx, a, ta, b, tb)) continue;
        // the same control points draw_wire uses
        float dist = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
        float reach = std::max(30.0f * cam.zoom, dist * 0.38f);
        ImVec2 c1(a.x + ta.x * reach, a.y + ta.y * reach);
        ImVec2 c2(b.x + tb.x * reach, b.y + tb.y * reach);
        ImVec2 prev = a;
        const int kSeg = 24;
        for (int i = 1; i <= kSeg; ++i) {
            float t = (float)i / kSeg;
            ImVec2 pt;
            if (w.kind == SceneWire::Kind::Loose) {
                pt = ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
            } else {
                float u = 1.0f - t;
                pt.x = u * u * u * a.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x +
                       t * t * t * b.x;
                pt.y = u * u * u * a.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y +
                       t * t * t * b.y;
            }
            float vx = pt.x - prev.x, vy = pt.y - prev.y;
            float wx = p.x - prev.x, wy = p.y - prev.y;
            float len2 = vx * vx + vy * vy;
            float tt = len2 > 0 ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
            float dx = wx - vx * tt, dy = wy - vy * tt;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = &w;
            }
            prev = pt;
        }
    }
    return best;
}

/* Every wire holding a node's principal port — its fettuccine or any passive
 * principal↔aux linguine. A net port holds ONE wire end, so linking a
 * principal clears all of these first (principals are strictly
 * single-occupancy; aux outputs keep their relaxed fan-out). */
std::vector<SceneWire> wires_on_principal(const Scene& scene, const std::string& node) {
    std::vector<SceneWire> out;
    for (const auto& w : scene.wires)
        if (w.kind != SceneWire::Kind::Loose &&
            ((w.from == node && w.from_port == 0) || (w.to == node && w.to_port == 0)))
            out.push_back(w);
    return out;
}

} // namespace

void draw_canvas(const char* str_id, const Scene& scene, const Camera& cam,
                 const CanvasStyle& style) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 32 || avail.y < 32) return;
    ImGui::BeginChild(str_id, avail, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);
    render_scene(dl, scene, cam, origin, avail, style, nullptr, nullptr);
    dl->PopClipRect();
    ImGui::EndChild();
}

CanvasIO edit_canvas(const char* str_id, const Scene& scene, EditorState& ed,
                     const CanvasStyle& style, const AddPalette* palette,
                     const FaceRegistry* faces, const ContextMenuFn& context_menu,
                     const CanvasFx* fx, const CanvasNet* net) {
    CanvasIO out;
    ed.hover_wire_valid = false; // re-derived by this frame's hover pass
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 32 || avail.y < 32) return out;

    ImGui::BeginChild(str_id, avail, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::SetNextItemAllowOverlap(); // face widgets drawn later take precedence
    ImGui::InvisibleButton("##surface", avail,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    Camera& cam = ed.cam;

    auto to_world = [&](ImVec2 s) {
        return ImVec2(cam.x + (s.x - origin.x) / cam.zoom, cam.y + (s.y - origin.y) / cam.zoom);
    };
    auto flush_camera = [&] {
        out.commands.push_back(compile_camera(cam));
        ed.cam_dirty = false;
    };

    // ── camera: wheel zoom to cursor; middle/right drag pan ─────────────────
    if (hovered && io.MouseWheel != 0.0f && ed.drag == EditorState::Drag::None) {
        ImVec2 anchor = to_world(io.MousePos);
        cam.zoom = std::clamp(cam.zoom * std::pow(1.1f, io.MouseWheel), style.min_zoom,
                              style.max_zoom);
        cam.x = anchor.x - (io.MousePos.x - origin.x) / cam.zoom;
        cam.y = anchor.y - (io.MousePos.y - origin.y) / cam.zoom;
        ed.cam_dirty = true;
        ed.last_zoom_time = ImGui::GetTime();
    }
    if (hovered && ed.drag == EditorState::Drag::None &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        ed.drag = EditorState::Drag::Pan;
        ed.pan_sx = io.MousePos.x;
        ed.pan_sy = io.MousePos.y;
        ed.pan_right = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        ed.pan_left = false;
    }
    if (ed.drag == EditorState::Drag::Pan) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
            (ed.pan_left && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
            cam.x -= io.MouseDelta.x / cam.zoom;
            cam.y -= io.MouseDelta.y / cam.zoom;
            if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) ed.cam_dirty = true;
        } else {
            // a clean right-CLICK (no drag past slop) opens the context menu —
            // on a node, a WIRE (the connection itself is a target), or canvas
            float dx = io.MousePos.x - ed.pan_sx, dy = io.MousePos.y - ed.pan_sy;
            if (ed.pan_right && hovered &&
                dx * dx + dy * dy <= style.click_slop * style.click_slop) {
                const SceneNode* hit = hit_test(scene, cam, origin, style, &ed, fx, io.MousePos);
                ed.ctx_node = hit ? hit->name : "";
                ed.ctx_is_wire = false;
                if (!hit) {
                    if (const SceneWire* hw =
                            hit_wire(scene, cam, origin, style, &ed, fx, io.MousePos, 6.0f)) {
                        ed.ctx_is_wire = true;
                        ed.ctx_wire = *hw;
                    }
                }
                ImVec2 w = to_world(io.MousePos);
                ed.ctx_wx = w.x;
                ed.ctx_wy = w.y;
                ImGui::OpenPopup("vm-canvas-ctx");
            }
            ed.drag = EditorState::Drag::None;
            ed.pan_left = false;
            if (ed.cam_dirty) flush_camera();
        }
    }
    // wheel-idle flush: the zoom gesture "ends" after a moment of silence
    if (ed.cam_dirty && ed.drag == EditorState::Drag::None &&
        ImGui::GetTime() - ed.last_zoom_time > style.camera_flush_idle)
        flush_camera();

    // ── double-click: enter a subgraph (on a node with an enter hint) or
    //    exit back up the mantle stack (on empty canvas) ─────────────────────
    bool dblclick_handled = false;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (const SceneNode* hit = hit_test(scene, cam, origin, style, &ed, fx, io.MousePos)) {
            if (!hit->enter_mantle.empty()) {
                ed.mantle_stack.push_back(scene.mantle);
                ed.selection.clear();
                out.commands.push_back(compile_use(hit->enter_mantle));
                dblclick_handled = true;
            }
        } else if (!ed.mantle_stack.empty()) {
            std::string back = ed.mantle_stack.back();
            ed.mantle_stack.pop_back();
            ed.selection.clear();
            out.commands.push_back(compile_use(back));
            dblclick_handled = true;
        }
        if (dblclick_handled) ed.drag = EditorState::Drag::None; // cancel the press's drag
    }

    // ── left button: chrome / wire / select / move / marquee ────────────────
    // Alt+drag pans (laptop-friendly — no middle button needed). Space left
    // the chord on 2026-07-14: the author reassigned it to "fire an
    // interaction" (a host-side key over ed.hover_wire).
    if (hovered && !dblclick_handled && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ed.drag == EditorState::Drag::None && io.KeyAlt) {
        ed.drag = EditorState::Drag::Pan;
        ed.pan_sx = io.MousePos.x;
        ed.pan_sy = io.MousePos.y;
        ed.pan_right = false;
        ed.pan_left = true;
    }
    if (hovered && !dblclick_handled && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ed.drag == EditorState::Drag::None) {
        ed.press_sx = io.MousePos.x;
        ed.press_sy = io.MousePos.y;
        PortHit ph;
        const SceneNode* body_hit = hit_test(scene, cam, origin, style, &ed, fx, io.MousePos);
        bool chrome_handled = false;
        if (body_hit && body_hit->shape == NodeShape::Window) { // shapes have no chrome
            NodeGeom g = geom(*body_hit, cam, origin, style, &ed, fx);
            float z = cam.zoom;
            // collapse dot?
            ImVec2 dot(g.pos.x + 12 * z, g.pos.y + style.header_h * 0.5f * z);
            float dx = io.MousePos.x - dot.x, dy = io.MousePos.y - dot.y;
            if (dx * dx + dy * dy <= (7 * z) * (7 * z)) {
                out.commands.push_back(compile_collapse(body_hit->name, !body_hit->collapsed));
                chrome_handled = true;
            }
            // resize grip?
            ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
            if (!chrome_handled && !body_hit->collapsed && io.MousePos.x > br.x - 12 * z &&
                io.MousePos.y > br.y - 12 * z) {
                ed.drag = EditorState::Drag::Resize;
                ed.resize_node = body_hit->name;
                ed.resize_w = g.size.x / z;
                ed.resize_h = g.size.y / z;
                chrome_handled = true;
            }
        }
        // resize grip on a SELECTED shaped node's bounding-box corner — the
        // corner sits OUTSIDE the true shape, so check before body hit-testing
        // (grip drawn only when selected; same rule here)
        if (!chrome_handled) {
            float z = cam.zoom;
            for (auto it = scene.nodes.rbegin(); it != scene.nodes.rend(); ++it) {
                if (it->shape == NodeShape::Window || !ed.selected(it->name)) continue;
                NodeGeom g = geom(*it, cam, origin, style, &ed, fx);
                ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
                if (io.MousePos.x > br.x - 12 * z && io.MousePos.x < br.x + 4 &&
                    io.MousePos.y > br.y - 12 * z && io.MousePos.y < br.y + 4) {
                    ed.drag = EditorState::Drag::Resize;
                    ed.resize_node = it->name;
                    ed.resize_w = g.size.x / z;
                    ed.resize_h = g.size.y / z;
                    chrome_handled = true;
                    break;
                }
            }
        }
        if (chrome_handled) {
            // fallthrough to nothing: the chrome consumed the press
        } else if (hit_port(scene, cam, origin, style, &ed, fx, io.MousePos, ph)) {
            // Grabbing an occupied 1:1 end detaches its wire for re-routing:
            // a principal's fettuccine, or the linguine feeding an input.
            ed.drag = EditorState::Drag::Wire;
            ed.wire_detach = false;
            if (ph.port == 0) {
                auto held = wires_on_principal(scene, ph.node->name);
                if (const SceneWire* fw = fettuccine_of(scene, ph.node->name)) {
                    ed.wire_detach = true;
                    ed.detached = *fw;
                    ed.wire_node = fw->from == ph.node->name ? fw->to : fw->from;
                    ed.wire_port = 0;
                    ed.wire_is_output = false;
                    ed.wire_type.clear();
                } else if (!held.empty()) {
                    // a passive principal↔aux wire: detach for re-routing; the
                    // fixed end is the aux endpoint on the other node.
                    const SceneWire& pw = held.front();
                    ed.wire_detach = true;
                    ed.detached = pw;
                    bool grabbed_is_from = pw.from == ph.node->name && pw.from_port == 0;
                    ed.wire_node = grabbed_is_from ? pw.to : pw.from;
                    ed.wire_port = grabbed_is_from ? pw.to_port : pw.from_port;
                    // principal was the source ⇒ the fixed aux end is an input;
                    // principal was the sink ⇒ the fixed aux end is the output.
                    ed.wire_is_output = !grabbed_is_from;
                    ed.wire_type.clear();
                    if (const SceneNode* other = scene.find(ed.wire_node)) {
                        const auto& ports = ed.wire_is_output ? other->outputs : other->inputs;
                        for (const auto& p : ports)
                            if (p.index == ed.wire_port) ed.wire_type = p.type;
                    }
                } else {
                    ed.wire_node = ph.node->name;
                    ed.wire_port = 0;
                    ed.wire_is_output = false;
                    ed.wire_type.clear();
                }
            } else if (!ph.is_output) {
                if (const SceneWire* lw = wire_into(scene, ph.node->name, ph.port)) {
                    ed.wire_detach = true;
                    ed.detached = *lw;
                    // the pending wire's fixed end becomes the original SOURCE
                    ed.wire_node = lw->from;
                    ed.wire_port = lw->from_port;
                    ed.wire_is_output = true;
                    const SceneNode* src = scene.find(lw->from);
                    ed.wire_type.clear();
                    if (src)
                        for (const auto& p : src->outputs)
                            if (p.index == lw->from_port) ed.wire_type = p.type;
                } else {
                    ed.wire_node = ph.node->name;
                    ed.wire_port = ph.port;
                    ed.wire_is_output = false;
                    ed.wire_type = ph.type;
                }
            } else {
                ed.wire_node = ph.node->name; // outputs fan out: always a new wire
                ed.wire_port = ph.port;
                ed.wire_is_output = true;
                ed.wire_type = ph.type;
            }
        } else if (const SceneNode* hit =
                       hit_test(scene, cam, origin, style, &ed, fx, io.MousePos)) {
            ed.press_node = hit->name;
            if (io.KeyShift)
                ed.toggle(hit->name);
            else if (!ed.selected(hit->name))
                ed.selection = {hit->name};
            if (ed.selected(hit->name)) {
                ed.drag = EditorState::Drag::Move;
                ed.moved = false;
                ed.staged.clear();
                for (const auto& n : scene.nodes)
                    if (ed.selected(n.name)) ed.staged[n.name] = {n.x, n.y};
                // grabbing a block grabs its stack: the chain below and the
                // reporters plugged in ride along (selection stays honest —
                // only the staged set grows)
                for (const auto& n : scene.nodes) {
                    if (n.shape != NodeShape::Block || !ed.selected(n.name)) continue;
                    for (const auto& rider : block_stack_below(scene, n.name))
                        if (!ed.staged.count(rider))
                            if (const SceneNode* rn = scene.find(rider))
                                ed.staged[rider] = {rn->x, rn->y};
                }
            }
        } else {
            ed.press_node.clear(); // an empty-canvas press names no node
            ed.drag = EditorState::Drag::Marquee;
            ed.marquee_additive = io.KeyShift;
            ImVec2 w = to_world(io.MousePos);
            ed.marquee_x0 = ed.marquee_x1 = w.x;
            ed.marquee_y0 = ed.marquee_y1 = w.y;
        }
    }
    if (ed.drag == EditorState::Drag::Move) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
                for (auto& [name, pos] : ed.staged) {
                    pos.first += io.MouseDelta.x / cam.zoom;
                    pos.second += io.MouseDelta.y / cam.zoom;
                }
                float dx = io.MousePos.x - ed.press_sx, dy = io.MousePos.y - ed.press_sy;
                if (dx * dx + dy * dy > style.click_slop * style.click_slop) ed.moved = true;
            }
        } else {
            if (ed.moved) {
                bool any_block = false;
                for (const auto& n : scene.nodes)
                    if (n.shape == NodeShape::Block && ed.staged.count(n.name))
                        any_block = true;
                if (any_block) {
                    // the snap-to-connect release: moves + link + tears +
                    // splice/heal + re-flow, ONE batch = one undo frame
                    std::string cmd = compile_block_release(scene, ed.staged, style.block);
                    if (!cmd.empty()) out.commands.push_back(cmd);
                } else {
                    MoveList moves;
                    for (const auto& n : scene.nodes) { // scene order → stable transcript
                        auto it = ed.staged.find(n.name);
                        if (it != ed.staged.end())
                            moves.push_back({n.name, {it->second.first, it->second.second}});
                    }
                    out.commands.push_back(compile_moves(moves));
                }
            } else if (!io.KeyShift && !ed.press_node.empty()) {
                ed.selection = {ed.press_node}; // plain click collapses the selection
            }
            ed.staged.clear();
            ed.drag = EditorState::Drag::None;
        }
    }
    if (ed.drag == EditorState::Drag::Resize) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // shaped bodies allow much smaller minimums than window chrome
            const SceneNode* rn = scene.find(ed.resize_node);
            bool shaped = rn && rn->shape != NodeShape::Window;
            ed.resize_w = std::max(shaped ? 24.0f : 80.0f,
                                   ed.resize_w + io.MouseDelta.x / cam.zoom);
            ed.resize_h = std::max(shaped ? 24.0f : style.header_h + style.port_row,
                                   ed.resize_h + io.MouseDelta.y / cam.zoom);
        } else {
            out.commands.push_back(compile_resize(ed.resize_node, ed.resize_w, ed.resize_h));
            ed.resize_node.clear();
            ed.drag = EditorState::Drag::None;
        }
    }
    if (ed.drag == EditorState::Drag::Marquee) {
        ImVec2 w = to_world(io.MousePos);
        ed.marquee_x1 = w.x;
        ed.marquee_y1 = w.y;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float x0 = std::min(ed.marquee_x0, ed.marquee_x1);
            float x1 = std::max(ed.marquee_x0, ed.marquee_x1);
            float y0 = std::min(ed.marquee_y0, ed.marquee_y1);
            float y1 = std::max(ed.marquee_y0, ed.marquee_y1);
            if (!ed.marquee_additive) ed.selection.clear();
            for (const auto& n : scene.nodes) {
                float w_ = node_width(n, style);
                float h_ = node_height(n, style);
                bool overlap = n.x < x1 && n.x + w_ > x0 && n.y < y1 && n.y + h_ > y0;
                if (overlap && !ed.selected(n.name)) ed.selection.push_back(n.name);
            }
            ed.drag = EditorState::Drag::None;
        }
    }

    // ── wire drag: release compiles link / rewire-batch / unlink ────────────
    PortHit hover_port;
    bool has_hover_port = ed.drag == EditorState::Drag::Wire &&
                          hit_port(scene, cam, origin, style, &ed, fx, io.MousePos, hover_port);
    if (ed.drag == EditorState::Drag::Wire && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        PortRef src{ed.wire_node, ed.wire_port, ed.wire_is_output, ed.wire_type};
        bool same_as_detached = false;
        bool linked = false;
        if (has_hover_port) {
            PortRef dst{hover_port.node->name, hover_port.port, hover_port.is_output,
                        hover_port.type};
            if (ed.wire_detach) {
                // dropping back where the grabbed wire already sat is a no-op
                if (ed.detached.kind == SceneWire::Kind::Fettuccine) {
                    same_as_detached = dst.port == 0 && (ed.detached.from == dst.node ||
                                                         ed.detached.to == dst.node);
                } else {
                    // the grabbed end is whichever endpoint isn't the fixed one
                    bool fixed_is_from = ed.detached.from == ed.wire_node &&
                                         ed.detached.from_port == ed.wire_port;
                    same_as_detached =
                        fixed_is_from
                            ? ed.detached.to == dst.node && ed.detached.to_port == dst.port
                            : ed.detached.from == dst.node && ed.detached.from_port == dst.port;
                }
            }
            WireVerdict v = check_wire(src, dst);
            if (!same_as_detached && v != WireVerdict::Reject) {
                PortRef from = src, to = dst;
                if (v == WireVerdict::Linguine) {
                    // the source end leads: an aux output, or the principal
                    // when it feeds an aux input (0:j; aux output ⇒ i:0).
                    bool from_is_source = from.port == 0 ? !to.is_output : from.is_output;
                    if (!from_is_source) std::swap(from, to);
                }
                std::vector<SceneWire> unlinks;
                auto add_unlink = [&](const SceneWire& w) {
                    for (const auto& u : unlinks)
                        if (u.from == w.from && u.to == w.to && u.relation == w.relation)
                            return;
                    unlinks.push_back(w);
                };
                if (ed.wire_detach) add_unlink(ed.detached);
                // occupancy: an aux input holds one feed; a principal holds ONE
                // wire of ANY kind (fettuccine or passive) — net ports are
                // single-occupancy, only aux outputs fan out.
                for (const PortRef* end : {&from, &to})
                    if (end->port == 0)
                        for (const auto& w : wires_on_principal(scene, end->node))
                            add_unlink(w);
                if (v == WireVerdict::Linguine && to.port != 0)
                    if (const SceneWire* occ = wire_into(scene, to.node, to.port))
                        add_unlink(*occ); // an input holds one wire: rewire
                out.commands.push_back(compile_rewire(unlinks, from, to));
                linked = true;
            }
        }
        // a detached wire dropped on nothing (or an invalid port) is removed
        if (!linked && !same_as_detached && ed.wire_detach)
            out.commands.push_back(compile_unlink(ed.detached));
        // a FRESH wire dropped on empty canvas offers a quick add-and-link:
        // the add box opens at the drop point; the pick mints the node AND
        // wires it back to the dragged port (one batch, one undo frame)
        if (!linked && !ed.wire_detach && !has_hover_port && palette && hovered &&
            !hit_test(scene, cam, origin, style, &ed, fx, io.MousePos)) {
            ImVec2 w = to_world(io.MousePos);
            ed.add_x = w.x;
            ed.add_y = w.y;
            ed.add_filter[0] = '\0';
            ed.add_open = true;
            ed.add_link = true;
            ed.add_link_node = ed.wire_node;
            ed.add_link_port = ed.wire_port;
            ed.add_link_is_output = ed.wire_is_output;
            ed.add_link_type = ed.wire_type;
            ImGui::OpenPopup("vn-add");
        }
        ed.drag = EditorState::Drag::None;
        ed.wire_detach = false;
    }

    // ── delete key → rm (one batch for a multi-selection) ───────────────────
    if (hovered && !ed.selection.empty() && ed.drag == EditorState::Drag::None &&
        ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        out.commands.push_back(compile_deletes(ed.selection));
        ed.selection.clear();
    }

    // ── undo/redo surfacing: Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y ─────────────────
    if (hovered && ed.drag == EditorState::Drag::None && io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z))
            out.commands.push_back(io.KeyShift ? "redo" : "undo");
        else if (ImGui::IsKeyPressed(ImGuiKey_Y))
            out.commands.push_back("redo");
    }

    // ── add box (Shift+A at the cursor) ─────────────────────────────────────
    if (palette && hovered && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A) &&
        ed.drag == EditorState::Drag::None) {
        ImVec2 w = to_world(io.MousePos);
        ed.add_x = w.x;
        ed.add_y = w.y;
        ed.add_filter[0] = '\0';
        ed.add_open = true;
        ed.add_link = false;
        ImGui::OpenPopup("vn-add");
    }
    if (palette && ImGui::BeginPopup("vn-add")) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool entered = ImGui::InputTextWithHint("##filter", "add node…", ed.add_filter,
                                                sizeof ed.add_filter,
                                                ImGuiInputTextFlags_EnterReturnsTrue);
        auto lc = [](std::string s) {
            for (char& c : s) c = (char)tolower((unsigned char)c);
            return s;
        };
        std::string filter = lc(ed.add_filter);
        // filtered entries, grouped by category when any entry declares one
        // (uncategorized first, then categories in first-appearance order)
        std::vector<const AddPalette::Entry*> visible;
        bool any_cat = false;
        for (const auto& entry : palette->entries) {
            if (!filter.empty() && lc(entry.glyph).find(filter) == std::string::npos &&
                lc(entry.label).find(filter) == std::string::npos)
                continue;
            visible.push_back(&entry);
            if (!entry.category.empty()) any_cat = true;
        }
        const AddPalette::Entry* first = visible.empty() ? nullptr : visible.front();
        const AddPalette::Entry* picked = nullptr;
        auto entry_row = [&picked](const AddPalette::Entry& entry) {
            std::string row = entry.label + "  (" + entry.glyph + ")";
            if (ImGui::Selectable(row.c_str())) picked = &entry;
        };
        if (!any_cat) {
            for (const auto* e : visible) entry_row(*e);
        } else {
            std::vector<std::string> cats{""};
            for (const auto* e : visible)
                if (std::find(cats.begin(), cats.end(), e->category) == cats.end())
                    cats.push_back(e->category);
            for (const auto& cat : cats) {
                bool headed = false;
                for (const auto* e : visible) {
                    if (e->category != cat) continue;
                    if (!cat.empty() && !headed) {
                        ImGui::SeparatorText(cat.c_str());
                        headed = true;
                    }
                    entry_row(*e);
                }
            }
        }
        if (entered && !picked) picked = first;
        if (picked) {
            std::string name = unique_name(scene, picked->glyph);
            if (ed.add_link) {
                // mint + place + link back to the dragged port, one batch.
                // The new node's PRINCIPAL is the target: fettuccine from a
                // principal, passive linguine from an aux — always legal.
                PortRef src{ed.add_link_node, ed.add_link_port, ed.add_link_is_output,
                            ed.add_link_type};
                PortRef dst{name, 0, false, ""};
                PortRef from = src, to = dst;
                if (check_wire(src, dst) == WireVerdict::Linguine && !from.is_output &&
                    from.port != 0)
                    std::swap(from, to); // an aux input is fed BY the new principal
                out.commands.push_back(
                    compile_batch({"rune new " + picked->glyph + " " + name,
                                   compile_move(name, ed.add_x, ed.add_y),
                                   compile_link(from, to)}));
            } else {
                out.commands.push_back(compile_add(picked->glyph, name, ed.add_x, ed.add_y));
            }
            ed.add_open = false;
            ed.add_link = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        ed.add_open = false;
        ed.add_link = false;
    }

    // ── render ───────────────────────────────────────────────────────────────
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);
    render_scene(dl, scene, cam, origin, avail, style, &ed, fx);

    // ── networking: declare what is shown, mark who is on it ────────────────
    if (net && net->surfaces) {
        Surfaces& sf = *net->surfaces;
        sf.declare(net->surface_id, "canvas", net->mark);
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            sf.focus(net->surface_id);
        float mark_scale = std::max(cam.zoom, 0.6f);
        for (const auto& n : scene.nodes) {
            if (const NodeFx* f = node_fx(fx, n); f && f->scale <= 0.02f) continue;
            NodeGeom g = geom(n, cam, origin, style, &ed, fx);
            ImVec2 mn = g.pos, mx(g.pos.x + g.size.x, g.pos.y + g.size.y);
            // an off-screen node is not SHOWN: a surface declares what a person
            // can actually see, or "who is looking at this" means nothing
            if (mx.x < origin.x || mx.y < origin.y || mn.x > origin.x + avail.x ||
                mn.y > origin.y + avail.y)
                continue;
            sf.show(net->surface_id, n.id);
            float round = n.shape == NodeShape::Circle ? 0.5f * std::min(g.size.x, g.size.y)
                                                       : style.rounding * cam.zoom;
            if (net->roster && net->display.marks)
                draw_presence_mark(dl, mn, mx, net->mark, net->roster->on_rune(n.id), round,
                                   mark_scale);
            if (net->shareable && net->display.private_marks && !net->shareable(n))
                draw_private_mark(dl, mn, mx, mark_scale);
        }
    }

    // ── snap preview: while a block drags, show where it would connect ──────
    if (ed.drag == EditorState::Drag::Move && ed.moved) {
        SnapCandidate snap = find_snap(scene, ed.staged, style.block);
        if (snap.valid) {
            ImU32 ok = style.theme.wire_ok;
            ImVec2 target(origin.x + (snap.ax - cam.x) * cam.zoom,
                          origin.y + (snap.ay - cam.y) * cam.zoom);
            dl->AddCircle(target, 8.0f * cam.zoom, ok, 0, 2.5f);
            if (const SceneNode* dn = scene.find(snap.dragged)) {
                // ghost outline where the dragged block would land, aligned
                auto it = ed.staged.find(snap.dragged);
                if (it != ed.staged.end()) {
                    float w = node_width(*dn, style), h = node_height(*dn, style);
                    ImVec2 a(origin.x + (it->second.first + snap.dx - cam.x) * cam.zoom,
                             origin.y + (it->second.second + snap.dy - cam.y) * cam.zoom);
                    ImVec2 b(a.x + w * cam.zoom, a.y + h * cam.zoom);
                    dl->AddRect(a, b, ok, style.rounding * cam.zoom, 0, 2.0f);
                }
            }
        }
    }

    // ── hover highlights + tooltips: outline what the cursor would act on ───
    const SceneNode* dwell_node = nullptr;
    if (hovered &&
        (ed.drag == EditorState::Drag::None || ed.drag == EditorState::Drag::Wire)) {
        ImU32 hov = style.theme.hover;
        float z = cam.zoom;
        PortHit hp;
        if (hit_port(scene, cam, origin, style, &ed, fx, io.MousePos, hp)) {
            // the ring takes a declared type color (the type shows here too);
            // undeclared types keep the theme's hover accent
            ImU32 ring = hov;
            if (const PortStyle* ps = port_style(style, hp.type); ps && ps->color)
                ring = ps->color;
            dl->AddCircle(hp.anchor, style.port_radius * z + 4.0f, ring, 0, 2.0f);
            if (style.hover_tooltips &&
                ed.drag == EditorState::Drag::None) { // name : type, immediately
                std::string tip;
                if (hp.port == 0) {
                    tip = hp.node->principal_name.empty()
                              ? "principal"
                              : hp.node->principal_name + "  (principal)";
                } else {
                    const auto& list = hp.is_output ? hp.node->outputs : hp.node->inputs;
                    for (const auto& pr : list)
                        if (pr.index == hp.port) {
                            tip = pr.name;
                            if (!pr.type.empty()) tip += " : " + pr.type;
                            tip += hp.is_output ? "  (out)" : "  (in)";
                        }
                }
                if (!tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
            }
        } else if (ed.drag == EditorState::Drag::None) {
            if (const SceneNode* hn = hit_test(scene, cam, origin, style, &ed, fx, io.MousePos)) {
                dwell_node = hn;
                NodeGeom g = geom(*hn, cam, origin, style, &ed, fx);
                if (hn->shape == NodeShape::Block) {
                    ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
                    dl->AddRect(ImVec2(g.pos.x - 2, g.pos.y - 2), ImVec2(br.x + 2, br.y + 2),
                                hov, style.rounding * z + 2.0f, 0, 2.0f);
                } else if (hn->shape == NodeShape::Window) {
                    ImVec2 br(g.pos.x + g.size.x, g.pos.y + g.size.y);
                    dl->AddRect(ImVec2(g.pos.x - 2, g.pos.y - 2), ImVec2(br.x + 2, br.y + 2),
                                hov, style.rounding * z + 2.0f, 0, 2.0f);
                    // chrome accents: the collapse dot / resize grip under the cursor
                    ImVec2 dot(g.pos.x + 12 * z, g.pos.y + style.header_h * 0.5f * z);
                    float ddx = io.MousePos.x - dot.x, ddy = io.MousePos.y - dot.y;
                    if (ddx * ddx + ddy * ddy < 64.0f)
                        dl->AddCircle(dot, 7.0f * z, hov, 0, 2.0f);
                    if (!hn->collapsed && io.MousePos.x > br.x - 12 * z &&
                        io.MousePos.y > br.y - 12 * z)
                        dl->AddTriangleFilled(ImVec2(br.x - 2, br.y - 12 * z),
                                              ImVec2(br.x - 2, br.y - 2),
                                              ImVec2(br.x - 12 * z, br.y - 2), hov);
                } else {
                    ImVec2 c(g.pos.x + g.size.x * 0.5f, g.pos.y + g.size.y * 0.5f);
                    float r = std::min(g.size.x, g.size.y) * 0.44f + 3.0f;
                    if (hn->shape == NodeShape::Circle) {
                        dl->AddCircle(c, r, hov, 0, 2.0f);
                    } else {
                        float theta = node_theta(scene, *hn, style, &ed, fx);
                        ImVec2 pts[16];
                        int k = std::min(hn->shape_sides, 16);
                        for (int i = 0; i < k; ++i) {
                            float ang = theta + 2.0f * kPi * (float)i / (float)k;
                            pts[i] = ImVec2(c.x + r * std::cos(ang), c.y + r * std::sin(ang));
                        }
                        dl->AddPolyline(pts, k, hov, ImDrawFlags_Closed, 2.0f);
                    }
                }
            } else if (const SceneWire* hw =
                           hit_wire(scene, cam, origin, style, &ed, fx, io.MousePos, 6.0f)) {
                ed.hover_wire_valid = true; // hosts read this (Space-to-fire)
                ed.hover_wire = *hw;
                ImVec2 a, b, ta, tb;
                if (wire_endpoints(scene, *hw, cam, origin, style, &ed, fx, a, ta, b, tb)) {
                    if (hw->style == SceneWire::Style::Adjacency) {
                        // the hidden link, made visible under the cursor: a
                        // ring at the seam (flush) or an accent line (apart)
                        float ddx = b.x - a.x, ddy = b.y - a.y;
                        if (ddx * ddx + ddy * ddy < 16.0f)
                            dl->AddCircle(a, 7.0f * z, hov, 0, 2.0f);
                        else
                            dl->AddLine(a, b, hov, 2.5f * z);
                    } else if (hw->kind == SceneWire::Kind::Loose) {
                        dl->AddLine(a, b, hov, 2.5f * z);
                    } else {
                        float dist = std::sqrt((b.x - a.x) * (b.x - a.x) +
                                               (b.y - a.y) * (b.y - a.y));
                        float reach = std::max(30.0f * z, dist * 0.38f);
                        dl->AddBezierCubic(
                            a, ImVec2(a.x + ta.x * reach, a.y + ta.y * reach),
                            ImVec2(b.x + tb.x * reach, b.y + tb.y * reach), b, hov,
                            (hw->kind == SceneWire::Kind::Fettuccine ? 5.0f : 3.5f) * z);
                    }
                    if (style.hover_tooltips && !hw->relation.empty())
                        ImGui::SetTooltip("%s — %s  (%s)", hw->from.c_str(), hw->to.c_str(),
                                          hw->relation.c_str());
                }
            }
        }
    }
    // node tooltip after a short dwell (ports/wires tip immediately, nodes
    // wait so the canvas doesn't flicker cards while the mouse crosses it)
    if (dwell_node && style.hover_tooltips) {
        if (ed.hover_node != dwell_node->name) {
            ed.hover_node = dwell_node->name;
            ed.hover_since = ImGui::GetTime();
        } else if (ImGui::GetTime() - ed.hover_since > 0.55) {
            ImGui::BeginTooltip();
            ImGui::Text("%s — %s", dwell_node->name.c_str(), dwell_node->label.c_str());
            if (!dwell_node->tags.empty()) {
                std::string tags;
                for (const auto& t : dwell_node->tags) tags += (tags.empty() ? "" : " ") + t;
                ImGui::TextDisabled("tags: %s", tags.c_str());
            }
            for (const auto& f : dwell_node->fields)
                ImGui::TextDisabled("%s = %s", f.key.c_str(), f.value_json.c_str());
            if (!dwell_node->enter_mantle.empty())
                ImGui::TextDisabled("double-click: enter %s", dwell_node->enter_mantle.c_str());
            ImGui::EndTooltip();
        }
    } else {
        ed.hover_node.clear();
    }

    if (ed.drag == EditorState::Drag::Wire) {
        // the pending wire: verdict-tinted (valid target = green, invalid = red)
        const SceneNode* src_node = scene.find(ed.wire_node);
        if (src_node) {
            NodeGeom g = geom(*src_node, cam, origin, style, &ed, fx);
            ImVec2 a = port_anchor(*src_node, g, ed.wire_port, ed.wire_is_output, style,
                                   node_theta(scene, *src_node, style, &ed, fx));
            ImVec2 b = io.MousePos;
            // in open space the pending wire reads as its TYPE (declared
            // color); at a candidate port the verdict tint takes over
            ImU32 col = style.theme.wire_neutral;
            if (const PortStyle* ps = port_style(style, ed.wire_type); ps && ps->color)
                col = ps->color;
            if (has_hover_port) {
                PortRef src{ed.wire_node, ed.wire_port, ed.wire_is_output, ed.wire_type};
                PortRef dst{hover_port.node->name, hover_port.port, hover_port.is_output,
                            hover_port.type};
                col = check_wire(src, dst) == WireVerdict::Reject ? style.theme.wire_bad
                                                                  : style.theme.wire_ok;
                b = hover_port.anchor; // snap to the candidate port
            }
            ImVec2 ta = port_tangent(*src_node, g, ed.wire_port, a);
            float dist = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
            float reach = std::max(30.0f * cam.zoom, dist * 0.38f);
            dl->AddBezierCubic(a, ImVec2(a.x + ta.x * reach, a.y + ta.y * reach),
                               ImVec2(b.x - (b.x - a.x) * 0.2f, b.y - (b.y - a.y) * 0.2f), b,
                               col, (ed.wire_port == 0 ? 3.0f : 2.0f) * cam.zoom);
        }
    }
    if (ed.drag == EditorState::Drag::Marquee) {
        ImVec2 a(origin.x + (std::min(ed.marquee_x0, ed.marquee_x1) - cam.x) * cam.zoom,
                 origin.y + (std::min(ed.marquee_y0, ed.marquee_y1) - cam.y) * cam.zoom);
        ImVec2 b(origin.x + (std::max(ed.marquee_x0, ed.marquee_x1) - cam.x) * cam.zoom,
                 origin.y + (std::max(ed.marquee_y0, ed.marquee_y1) - cam.y) * cam.zoom);
        dl->AddRectFilled(a, b, style.theme.marquee_fill);
        dl->AddRect(a, b, style.theme.marquee_line);
    }
    dl->PopClipRect();

    // ── faces: per-glyph body renderers, drawn as real widgets on top ───────
    if (faces) {
        for (const auto& n : scene.nodes) {
            // window faces need chrome; block faces sit inline after the label
            // (a block's arguments — `move [10]` — ARE its face widgets)
            if (n.collapsed ||
                (n.shape != NodeShape::Window && n.shape != NodeShape::Block))
                continue;
            auto it = faces->by_glyph.find(n.glyph);
            if (it == faces->by_glyph.end() || !it->second) continue;
            NodeGeom g = geom(n, cam, origin, style, &ed, fx);
            float z = cam.zoom;
            ImVec2 fpos, fsize;
            if (n.shape == NodeShape::Block) {
                fpos = ImVec2(g.pos.x + g.size.x * 0.42f, g.pos.y + 5 * z);
                fsize = ImVec2(g.size.x * 0.58f - 16 * z, g.size.y - 10 * z);
            } else {
                size_t rows = std::max(n.inputs.size(), n.outputs.size());
                float top = (style.header_h + rows * style.port_row + 2.0f) * z;
                float bottom = (n.tags.empty() ? 4.0f : 18.0f) * z;
                fpos = ImVec2(g.pos.x + 10 * z, g.pos.y + top);
                fsize = ImVec2(g.size.x - 20 * z, g.size.y - top - bottom);
            }
            if (fsize.x < 24 || fsize.y < 12) continue; // no room, no face
            ImGui::SetCursorScreenPos(fpos);
            ImGui::PushID(n.name.c_str());
            ImGui::BeginGroup();
            FaceContext ctx{n, scene, fpos, fsize, cam.zoom, out.commands};
            it->second(ctx);
            ImGui::EndGroup();
            ImGui::PopID();
        }
    }

    // ── right-click context menu: host entries above, built-ins beneath.
    //    Every entry compiles to the same dispatcher commands as the gestures —
    //    the menu is just another compiler frontend. ─────────────────────────
    if (ImGui::BeginPopup("vm-canvas-ctx")) {
        const SceneNode* target = ed.ctx_node.empty() ? nullptr : scene.find(ed.ctx_node);
        const SceneWire* wire_target = ed.ctx_is_wire ? &ed.ctx_wire : nullptr;
        if (context_menu) context_menu(target, wire_target, out);
        if (target) {
            if (context_menu) ImGui::Separator();
            if (ImGui::MenuItem(target->collapsed ? "expand" : "collapse"))
                out.commands.push_back(compile_collapse(target->name, !target->collapsed));
            if (ImGui::MenuItem("delete")) {
                // a selected target deletes the whole selection (one batch);
                // an unselected one deletes just itself
                if (ed.selected(target->name) && ed.selection.size() > 1) {
                    out.commands.push_back(compile_deletes(ed.selection));
                    ed.selection.clear();
                } else {
                    out.commands.push_back(compile_deletes({target->name}));
                }
            }
        } else if (wire_target) {
            if (context_menu) ImGui::Separator();
            if (ImGui::MenuItem("unlink"))
                out.commands.push_back(compile_unlink(*wire_target));
        } else if (palette && !palette->entries.empty()) {
            if (context_menu) ImGui::Separator();
            if (ImGui::BeginMenu("add")) {
                // same category grouping as the add box (headers, not submenus —
                // a two-level hunt is slower than a scan for palette-sized lists)
                bool any_cat = false;
                for (const auto& entry : palette->entries)
                    if (!entry.category.empty()) any_cat = true;
                auto add_item = [&](const AddPalette::Entry& entry) {
                    if (ImGui::MenuItem(entry.label.c_str()))
                        out.commands.push_back(compile_add(
                            entry.glyph, unique_name(scene, entry.glyph), ed.ctx_wx, ed.ctx_wy));
                };
                if (!any_cat) {
                    for (const auto& entry : palette->entries) add_item(entry);
                } else {
                    std::vector<std::string> cats{""};
                    for (const auto& entry : palette->entries)
                        if (std::find(cats.begin(), cats.end(), entry.category) == cats.end())
                            cats.push_back(entry.category);
                    for (const auto& cat : cats) {
                        bool headed = false;
                        for (const auto& entry : palette->entries) {
                            if (entry.category != cat) continue;
                            if (!cat.empty() && !headed) {
                                ImGui::SeparatorText(cat.c_str());
                                headed = true;
                            }
                            add_item(entry);
                        }
                    }
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    return out;
}

} // namespace maiz
