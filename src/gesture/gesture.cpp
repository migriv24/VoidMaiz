/* gesture.cpp — the UI-free gesture→command compilers. */
#include "voidmaiz/gesture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace maiz {

namespace {

/* Positions round to whole units on flush: drags stage in float, the model
 * keeps a readable transcript ("[120,330]"), and rounding happens once per
 * gesture, so there is no cumulative drift. */
std::string fmt_pos(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.0f", v);
    return buf;
}

/* Escape for a single-quoted batch payload: only ' needs care. */
std::string sq_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\'') out += "\\'";
        else out += c;
    }
    return out;
}

/* JSON string escape for embedding a command inside the batch array. */
std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

} // namespace

std::string compile_batch(const std::vector<std::string>& commands) {
    std::string payload = "[";
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i) payload += ',';
        payload += '"' + json_escape(commands[i]) + '"';
    }
    payload += ']';
    return "batch '" + sq_escape(payload) + "'";
}

std::string compile_commit(const std::vector<std::string>& commands) {
    if (commands.empty()) return {};
    if (commands.size() == 1) return commands.front();
    return compile_batch(commands);
}

std::string compile_move(std::string_view name, float x, float y) {
    return "setjson " + std::string(name) + " pos [" + fmt_pos(x) + "," + fmt_pos(y) + "]";
}

std::string compile_moves(const MoveList& moves) {
    if (moves.empty()) return {};
    if (moves.size() == 1)
        return compile_move(moves[0].first, moves[0].second.first, moves[0].second.second);
    std::vector<std::string> cmds;
    cmds.reserve(moves.size());
    for (const auto& [name, pos] : moves)
        cmds.push_back(compile_move(name, pos.first, pos.second));
    return compile_batch(cmds);
}

std::string compile_deletes(const std::vector<std::string>& names) {
    if (names.empty()) return {};
    if (names.size() == 1) return "rm " + names[0];
    std::vector<std::string> cmds;
    cmds.reserve(names.size());
    for (const auto& n : names) cmds.push_back("rm " + n);
    return compile_batch(cmds);
}

std::string compile_camera(const Camera& cam, std::string_view config_key) {
    // %.7g: enough precision for lat/lon (a geographic viewport) while integer
    // world coords still print as integers and float noise stays trimmed.
    char buf[160];
    std::snprintf(buf, sizeof buf, "config set %.*s \"%.7g %.7g %.7g\"",
                  (int)config_key.size(), config_key.data(),
                  (double)cam.x, (double)cam.y, (double)cam.zoom);
    return buf;
}

WireVerdict check_wire(const PortRef& a, const PortRef& b) {
    if (a.node == b.node) return WireVerdict::Reject;
    bool a_prin = a.port == 0, b_prin = b.port == 0;
    if (a_prin && b_prin) return WireVerdict::Fettuccine;
    // principal ↔ aux: a passive wire, legal in nets and draggable since
    // 2026-07-13 (author feedback: it's how real IC programs get built).
    // The principal is untyped (wildcard) and directionless, so only the
    // same-node rule above can reject it.
    if (a_prin || b_prin) return WireVerdict::Linguine;
    if (a.is_output == b.is_output) return WireVerdict::Reject;
    if (!a.type.empty() && !b.type.empty() && a.type != b.type) return WireVerdict::Reject;
    return WireVerdict::Linguine;
}

std::string compile_link(const PortRef& from, const PortRef& to) {
    std::string cmd = "link " + from.node + " " + to.node + " --relation " +
                      std::to_string(from.port) + ":" + std::to_string(to.port);
    if (from.port == 0 && to.port == 0) cmd += " --undirected";
    return cmd;
}

std::string compile_unlink(const SceneWire& wire) {
    return "unlink " + wire.from + " " + wire.to + " --relation " + wire.relation;
}

std::string compile_rewire(const std::vector<SceneWire>& unlinks, const PortRef& from,
                           const PortRef& to) {
    if (unlinks.empty()) return compile_link(from, to);
    std::vector<std::string> cmds;
    cmds.reserve(unlinks.size() + 1);
    for (const auto& w : unlinks) cmds.push_back(compile_unlink(w));
    cmds.push_back(compile_link(from, to));
    return compile_batch(cmds);
}

std::string compile_set(std::string_view node, std::string_view field, std::string_view text) {
    return "set " + std::string(node) + " " + std::string(field) + " '" + sq_escape(text) + "'";
}

std::string compile_setjson(std::string_view node, std::string_view field,
                            std::string_view json) {
    return "setjson " + std::string(node) + " " + std::string(field) + " '" + sq_escape(json) +
           "'";
}

std::string compile_tag(std::string_view node, std::string_view tag, bool add) {
    return "tag " + std::string(node) + " " + (add ? "+" : "-") + std::string(tag);
}

std::string compile_collapse(std::string_view node, bool collapsed) {
    return "setjson " + std::string(node) + " collapsed " + (collapsed ? "true" : "false");
}

std::string compile_resize(std::string_view node, float w, float h) {
    return "setjson " + std::string(node) + " size [" + fmt_pos(w) + "," + fmt_pos(h) + "]";
}

std::string compile_use(std::string_view mantle) {
    return "use " + std::string(mantle);
}

std::string unique_name(const Scene& scene, std::string_view glyph,
                        std::string_view device_tag) {
    std::string prefix = std::string(glyph) + "-" + std::string(device_tag);
    for (int n = 1;; ++n) {
        std::string candidate = prefix + std::to_string(n);
        if (!scene.find(candidate)) return candidate;
    }
}

std::string compile_add(std::string_view glyph, std::string_view name, float x, float y) {
    return compile_batch({"rune new " + std::string(glyph) + " " + std::string(name),
                          compile_move(name, x, y)});
}

std::string compile_clean(const Scene& scene, float default_w, float default_h,
                          float margin) {
    struct Box {
        const SceneNode* n;
        float x, y, w, h;
    };
    std::vector<Box> boxes;
    boxes.reserve(scene.nodes.size());
    for (const auto& n : scene.nodes)
        boxes.push_back({&n, n.x, n.y, n.w > 0 ? n.w : default_w, n.h > 0 ? n.h : default_h});

    // iterative pairwise separation: push overlapping boxes apart along the
    // axis of least penetration, half each side, until stable (or capped)
    for (int pass = 0; pass < 48; ++pass) {
        bool any = false;
        for (size_t i = 0; i < boxes.size(); ++i)
            for (size_t j = i + 1; j < boxes.size(); ++j) {
                Box& a = boxes[i];
                Box& b = boxes[j];
                float ox = std::min(a.x + a.w + margin, b.x + b.w + margin) -
                           std::max(a.x, b.x);
                float oy = std::min(a.y + a.h + margin, b.y + b.h + margin) -
                           std::max(a.y, b.y);
                if (ox <= 0 || oy <= 0) continue;
                any = true;
                if (ox < oy) {
                    float push = ox * 0.5f;
                    if (a.x + a.w * 0.5f <= b.x + b.w * 0.5f) {
                        a.x -= push;
                        b.x += push;
                    } else {
                        a.x += push;
                        b.x -= push;
                    }
                } else {
                    float push = oy * 0.5f;
                    if (a.y + a.h * 0.5f <= b.y + b.h * 0.5f) {
                        a.y -= push;
                        b.y += push;
                    } else {
                        a.y += push;
                        b.y -= push;
                    }
                }
            }
        if (!any) break;
    }

    MoveList moves;
    for (const auto& box : boxes) {
        float dx = box.x - box.n->x, dy = box.y - box.n->y;
        if (dx * dx + dy * dy < 0.25f) continue; // unmoved
        moves.push_back({box.n->name, {box.x, box.y}});
    }
    return compile_moves(moves);
}

float relax_step(const Scene& scene, PositionMap& pos, const RelaxParams& p) {
    struct Body {
        const SceneNode* n;
        float cx, cy, w, h; // center + extents
        float fx = 0, fy = 0;
    };
    std::vector<Body> bodies;
    bodies.reserve(scene.nodes.size());
    for (const auto& n : scene.nodes) {
        auto it = pos.find(n.name);
        if (it == pos.end()) it = pos.emplace(n.name, std::make_pair(n.x, n.y)).first;
        float w = n.w > 0 ? n.w : p.default_w;
        float h = n.h > 0 ? n.h : p.default_h;
        bodies.push_back({&n, it->second.first + w * 0.5f, it->second.second + h * 0.5f, w, h});
    }

    // short-range repulsion: peak push at contact, linear falloff to the range
    // edge (distant nodes feel nothing — a spread layout is already at rest)
    for (size_t i = 0; i < bodies.size(); ++i)
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            Body& a = bodies[i];
            Body& b = bodies[j];
            float dx = b.cx - a.cx, dy = b.cy - a.cy;
            float d = std::sqrt(dx * dx + dy * dy);
            // coincident bodies get a deterministic nudge apart
            if (d < 1.0f) { dx = 1.0f; dy = (float)((i + j) % 3) - 1.0f; d = std::sqrt(dx * dx + dy * dy); }
            // bodies "touch" at the sum of their half-diagonals; the range
            // extends beyond that so neighbors keep breathing room
            float contact = 0.5f * (std::sqrt(a.w * a.w + a.h * a.h) +
                                    std::sqrt(b.w * b.w + b.h * b.h)) * 0.5f;
            float reach = contact + p.repulse_range * 0.5f;
            if (d >= reach) continue;
            float f = p.repulse * (1.0f - (d - contact) / (reach - contact));
            f = std::min(std::max(f, 0.0f), p.repulse * 2.0f); // overlapping: up to 2×
            float ux = dx / d, uy = dy / d;
            a.fx -= ux * f; a.fy -= uy * f;
            b.fx += ux * f; b.fy += uy * f;
        }

    // wires are springs: stretch pulls the ends together, compression pushes
    // apart gently (loose semantic links don't participate)
    auto body_of = [&](const std::string& name) -> Body* {
        for (auto& b : bodies)
            if (b.n->name == name) return &b;
        return nullptr;
    };
    for (const auto& w : scene.wires) {
        if (w.kind == SceneWire::Kind::Loose) continue;
        Body* a = body_of(w.from);
        Body* b = body_of(w.to);
        if (!a || !b || a == b) continue;
        float dx = b->cx - a->cx, dy = b->cy - a->cy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < 1.0f) continue; // the repulsion nudge handles coincidence
        float f = p.spring_k * (d - p.spring_len);
        float ux = dx / d, uy = dy / d;
        a->fx += ux * f;
        a->fy += uy * f;
        b->fx -= ux * f;
        b->fy -= uy * f;
    }

    // position-based integration with a displacement clamp: stable by
    // construction (no velocity to accumulate)
    float max_disp = 0.0f;
    for (auto& b : bodies) {
        float dx = std::clamp(b.fx, -p.max_step, p.max_step);
        float dy = std::clamp(b.fy, -p.max_step, p.max_step);
        b.cx += dx;
        b.cy += dy;
        float disp = std::sqrt(dx * dx + dy * dy);
        max_disp = std::max(max_disp, disp);
        pos[b.n->name] = {b.cx - b.w * 0.5f, b.cy - b.h * 0.5f};
    }
    return max_disp;
}

std::string compile_relax(const Scene& scene, int max_iterations, const RelaxParams& p) {
    PositionMap pos;
    for (int i = 0; i < max_iterations; ++i)
        if (relax_step(scene, pos, p) < 0.2f) break;
    MoveList moves;
    for (const auto& n : scene.nodes) {
        auto it = pos.find(n.name);
        if (it == pos.end()) continue;
        float dx = it->second.first - n.x, dy = it->second.second - n.y;
        if (dx * dx + dy * dy < 1.0f) continue; // unmoved
        moves.push_back({n.name, {it->second.first, it->second.second}});
    }
    return compile_moves(moves);
}

// ── blocks: snap-to-connect + stack machinery (okf/concepts/node-blocks.md) ──

bool block_anchor(const SceneNode& n, int port, bool is_output, float x, float y,
                  float w, float h, float scale, const BlockMetrics& m, float& ax,
                  float& ay) {
    if (n.shape != NodeShape::Block || port <= 0) return false; // principal: Phase B
    const auto& list = is_output ? n.outputs : n.inputs;
    const ScenePort* found = nullptr;
    int ordinal = 0, sockets = 0; // position among value sockets, list order
    for (const auto& p : list) {
        bool chain = p.name == (is_output ? "next" : "prev");
        if (p.index == port) {
            found = &p;
            if (!chain) ordinal = sockets;
        }
        if (!chain) ++sockets;
    }
    if (!found) return false;
    float notch = std::min(m.notch_x * scale, w * 0.45f);
    if (!is_output && found->name == "prev") { // the top notch
        ax = x + notch;
        ay = y;
        return true;
    }
    if (is_output && found->name == "next") { // the bottom tab
        ax = x + notch;
        ay = y + h;
        return true;
    }
    if (!is_output) { // a value socket: stacked down the right edge
        ax = x + w;
        ay = y + h * (float)(ordinal + 1) / (float)(sockets + 1);
        return true;
    }
    ax = x; // a reporter's plug: left edge, centered
    ay = y + h * 0.5f;
    return true;
}

namespace {

struct BRect {
    float x, y, w, h;
};

BRect block_rect(const SceneNode& n, const StagedMap& staged, const BlockMetrics& m) {
    auto it = staged.find(n.name);
    return {it != staged.end() ? it->second.first : n.x,
            it != staged.end() ? it->second.second : n.y,
            n.w > 0 ? n.w : m.default_w, n.h > 0 ? n.h : m.default_h};
}

const ScenePort* find_aux(const SceneNode& n, int index, bool is_output) {
    for (const auto& p : is_output ? n.outputs : n.inputs)
        if (p.index == index) return &p;
    return nullptr;
}

/* True when both wire ends resolve to block connectors — the wires the
 * snap/tear/layout story owns. */
bool block_wire(const Scene& scene, const SceneWire& w, const BlockMetrics& m,
                const SceneNode*& fn, const SceneNode*& tn) {
    if (w.kind != SceneWire::Kind::Linguine) return false;
    fn = scene.find(w.from);
    tn = scene.find(w.to);
    if (!fn || !tn) return false;
    float ax, ay;
    return block_anchor(*fn, w.from_port, true, 0, 0, m.default_w, m.default_h, 1, m,
                        ax, ay) &&
           block_anchor(*tn, w.to_port, false, 0, 0, m.default_w, m.default_h, 1, m,
                        ax, ay);
}

/* Both connector anchors of a block wire, world space, staged overrides win. */
bool wire_anchors(const Scene& scene, const SceneWire& w, const StagedMap& pos,
                  const BlockMetrics& m, float& ax, float& ay, float& bx, float& by) {
    const SceneNode* fn = nullptr;
    const SceneNode* tn = nullptr;
    if (!block_wire(scene, w, m, fn, tn)) return false;
    BRect fr = block_rect(*fn, pos, m), tr = block_rect(*tn, pos, m);
    block_anchor(*fn, w.from_port, true, fr.x, fr.y, fr.w, fr.h, 1, m, ax, ay);
    block_anchor(*tn, w.to_port, false, tr.x, tr.y, tr.w, tr.h, 1, m, bx, by);
    return true;
}

/* A statement wire stacks blocks (…next → prev…): the TO block follows its
 * leader. A value wire plugs a reporter into a socket: the FROM block
 * (the reporter) follows the socket's owner. */
bool to_follows(const Scene& scene, const SceneWire& w) {
    const SceneNode* tn = scene.find(w.to);
    const ScenePort* tp = tn ? find_aux(*tn, w.to_port, false) : nullptr;
    return tp && tp->name == "prev";
}

/* Re-flow: iterate block wires, moving each follower flush against its
 * leader; `fixed` positions (a drag's staged set) never move. Bounded passes
 * so linked cycles terminate. Returns the nodes that ended ≥1px from their
 * scene position. */
MoveList layout_moves(const Scene& scene, const std::vector<SceneWire>& wires,
                      const StagedMap& fixed, const BlockMetrics& m) {
    StagedMap pos;
    for (const auto& n : scene.nodes) {
        auto it = fixed.find(n.name);
        pos[n.name] = it != fixed.end() ? it->second : std::make_pair(n.x, n.y);
    }
    const size_t bound = scene.nodes.size() + 1;
    for (size_t pass = 0; pass < bound; ++pass) {
        bool moved = false;
        for (const auto& w : wires) {
            float ax, ay, bx, by;
            if (!wire_anchors(scene, w, pos, m, ax, ay, bx, by)) continue;
            bool follow_to = to_follows(scene, w);
            const std::string& follower = follow_to ? w.to : w.from;
            if (fixed.count(follower)) continue;
            float dx = follow_to ? ax - bx : bx - ax;
            float dy = follow_to ? ay - by : by - ay;
            if (dx * dx + dy * dy < 0.25f) continue;
            pos[follower].first += dx;
            pos[follower].second += dy;
            moved = true;
        }
        if (!moved) break;
    }
    MoveList out;
    for (const auto& n : scene.nodes) {
        if (fixed.count(n.name)) continue;
        auto [px, py] = pos[n.name];
        float dx = px - n.x, dy = py - n.y;
        if (dx * dx + dy * dy < 1.0f) continue;
        out.push_back({n.name, {px, py}});
    }
    return out;
}

} // namespace

SnapCandidate find_snap(const Scene& scene, const StagedMap& staged,
                        const BlockMetrics& m) {
    SnapCandidate best;
    float best_d2 = m.snap_radius * m.snap_radius;
    auto already_linked = [&](const PortRef& from, const PortRef& to) {
        for (const auto& w : scene.wires)
            if (w.kind == SceneWire::Kind::Linguine && w.from == from.node &&
                w.from_port == from.port && w.to == to.node && w.to_port == to.port)
                return true;
        return false;
    };
    for (const auto& d : scene.nodes) {
        if (d.shape != NodeShape::Block || !staged.count(d.name)) continue;
        BRect dr = block_rect(d, staged, m);
        for (const auto& t : scene.nodes) {
            if (t.shape != NodeShape::Block || staged.count(t.name)) continue;
            BRect tr = block_rect(t, staged, m);
            for (int dout = 0; dout < 2; ++dout) {
                for (const auto& dp : dout ? d.outputs : d.inputs) {
                    // a connector wired INSIDE the dragged set is rigid (the
                    // stack's own chain) — never offered for re-snapping
                    bool internal = false;
                    for (const auto& w : scene.wires) {
                        if (w.kind != SceneWire::Kind::Linguine) continue;
                        bool on_port = dout ? (w.from == d.name && w.from_port == dp.index)
                                            : (w.to == d.name && w.to_port == dp.index);
                        if (on_port && staged.count(dout ? w.to : w.from)) internal = true;
                    }
                    if (internal) continue;
                    for (const auto& tp : dout ? t.inputs : t.outputs) {
                        PortRef dref{d.name, dp.index, dout != 0, dp.type};
                        PortRef tref{t.name, tp.index, dout == 0, tp.type};
                        if (check_wire(dref, tref) != WireVerdict::Linguine) continue;
                        float dax, day, tax, tay;
                        if (!block_anchor(d, dp.index, dout != 0, dr.x, dr.y, dr.w,
                                          dr.h, 1, m, dax, day) ||
                            !block_anchor(t, tp.index, dout == 0, tr.x, tr.y, tr.w,
                                          tr.h, 1, m, tax, tay))
                            continue;
                        const PortRef& from = dout ? dref : tref;
                        const PortRef& to = dout ? tref : dref;
                        // dropping back where the wire already sits: plain move
                        if (already_linked(from, to)) continue;
                        float dx = tax - dax, dy = tay - day;
                        float d2 = dx * dx + dy * dy;
                        if (d2 >= best_d2) continue;
                        best_d2 = d2;
                        best.valid = true;
                        best.from = from;
                        best.to = to;
                        best.dragged = d.name;
                        best.dx = dx;
                        best.dy = dy;
                        best.ax = tax;
                        best.ay = tay;
                    }
                }
            }
        }
    }
    return best;
}

std::vector<std::string> block_stack_below(const Scene& scene, const std::string& head) {
    std::vector<std::string> out{head};
    std::unordered_set<std::string> in{head};
    const BlockMetrics m;
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto& w : scene.wires) {
            const SceneNode* fn = nullptr;
            const SceneNode* tn = nullptr;
            if (!block_wire(scene, w, m, fn, tn)) continue;
            bool follow_to = to_follows(scene, w);
            const std::string& leader = follow_to ? w.from : w.to;
            const std::string& follower = follow_to ? w.to : w.from;
            if (in.count(leader) && !in.count(follower)) {
                in.insert(follower);
                out.push_back(follower);
                grew = true;
            }
        }
    }
    return out;
}

std::string compile_block_release(const Scene& scene, const StagedMap& staged,
                                  const BlockMetrics& m) {
    if (staged.empty()) return {};
    SnapCandidate snap = find_snap(scene, staged, m);

    // final positions: the snap's alignment translates the WHOLE dragged set,
    // so a grabbed sub-stack lands rigid
    StagedMap fin = staged;
    if (snap.valid)
        for (auto& [name, p] : fin) {
            p.first += snap.dx;
            p.second += snap.dy;
        }

    std::vector<SceneWire> removed;
    std::vector<std::string> unlink_cmds;
    auto add_unlink = [&](const SceneWire& w) {
        for (const auto& u : removed)
            if (u.from == w.from && u.to == w.to && u.relation == w.relation) return false;
        removed.push_back(w);
        unlink_cmds.push_back(compile_unlink(w));
        return true;
    };

    // occupancy: block connectors hold ONE wire on EITHER end (a block has one
    // predecessor and one successor; a socket holds one plug; a reporter plugs
    // into one socket) — the displaced neighbors splice back below/above
    std::vector<SceneWire> displaced;
    if (snap.valid)
        for (const auto& w : scene.wires) {
            if (w.kind != SceneWire::Kind::Linguine) continue;
            if ((w.from == snap.from.node && w.from_port == snap.from.port) ||
                (w.to == snap.to.node && w.to_port == snap.to.port))
                if (add_unlink(w)) displaced.push_back(w);
        }

    // tears: a block wire whose connectors ended up pulled apart detaches —
    // dragging a block out of a stack IS the unlink gesture
    std::vector<SceneWire> torn;
    float tear = m.snap_radius * m.tear_factor;
    for (const auto& w : scene.wires) {
        bool from_moved = fin.count(w.from) != 0, to_moved = fin.count(w.to) != 0;
        if (from_moved == to_moved) continue; // rigid pair, or bystanders
        float ax, ay, bx, by;
        if (!wire_anchors(scene, w, fin, m, ax, ay, bx, by)) continue;
        float dx = bx - ax, dy = by - ay;
        if (dx * dx + dy * dy > tear * tear)
            if (add_unlink(w)) torn.push_back(w);
    }

    std::vector<SceneWire> added;
    std::vector<std::string> link_cmds;
    auto add_link = [&](const PortRef& from, const PortRef& to) {
        SceneWire w;
        w.from = from.node;
        w.to = to.node;
        w.from_port = from.port;
        w.to_port = to.port;
        w.kind = SceneWire::Kind::Linguine;
        w.relation = std::to_string(from.port) + ":" + std::to_string(to.port);
        added.push_back(w);
        link_cmds.push_back(compile_link(from, to));
    };
    auto chain_port = [&](const std::string& node, bool next) -> const ScenePort* {
        const SceneNode* n = scene.find(node);
        if (!n || n->shape != NodeShape::Block) return nullptr;
        for (const auto& p : next ? n->outputs : n->inputs)
            if (p.name == (next ? "next" : "prev")) return &p;
        return nullptr;
    };
    // walk the dragged stack's statement chain to its head or tail
    auto staged_chain_end = [&](std::string cur, bool down) {
        std::unordered_set<std::string> seen{cur};
        bool grew = true;
        while (grew) {
            grew = false;
            for (const auto& w : scene.wires) {
                const SceneNode* fn = nullptr;
                const SceneNode* tn = nullptr;
                if (!block_wire(scene, w, m, fn, tn) || !to_follows(scene, w)) continue;
                const std::string& next = down ? w.to : w.from;
                if ((down ? w.from : w.to) != cur) continue;
                if (!fin.count(next) || seen.count(next)) continue;
                cur = next;
                seen.insert(next);
                grew = true;
                break;
            }
        }
        return cur;
    };
    auto port_free = [&](const std::string& node, int port, bool is_output) {
        for (const auto& w : scene.wires) {
            if (w.kind != SceneWire::Kind::Linguine) continue;
            bool taken = is_output ? (w.from == node && w.from_port == port)
                                   : (w.to == node && w.to_port == port);
            if (!taken) continue;
            bool was_removed = false;
            for (const auto& u : removed)
                if (u.from == w.from && u.to == w.to && u.relation == w.relation)
                    was_removed = true;
            if (!was_removed) return false;
        }
        return true;
    };

    if (snap.valid) {
        add_link(snap.from, snap.to);
        // splice: a displaced STATEMENT neighbor re-attaches to the dragged
        // stack's free end (dropping a block on a seam inserts it)
        for (const auto& w : displaced) {
            const SceneNode* tn = scene.find(w.to);
            const ScenePort* tp = tn ? find_aux(*tn, w.to_port, false) : nullptr;
            if (!tp || tp->name != "prev") continue; // value plugs just displace
            if (w.from == snap.from.node && w.from_port == snap.from.port) {
                // the old successor hangs below the dragged stack's tail
                std::string tail = staged_chain_end(snap.dragged, /*down=*/true);
                const ScenePort* tnext = chain_port(tail, /*next=*/true);
                if (tnext && w.to != tail && port_free(tail, tnext->index, true))
                    add_link({tail, tnext->index, true, tnext->type},
                             {w.to, w.to_port, false, tp->type});
            } else {
                // the old predecessor feeds the dragged stack's head
                std::string head = staged_chain_end(snap.dragged, /*down=*/false);
                const ScenePort* hprev = chain_port(head, /*next=*/false);
                if (hprev && w.from != head && port_free(head, hprev->index, false)) {
                    const SceneNode* fnn = scene.find(w.from);
                    const ScenePort* fp = fnn ? find_aux(*fnn, w.from_port, true) : nullptr;
                    add_link({w.from, w.from_port, true, fp ? fp->type : ""},
                             {head, hprev->index, false, hprev->type});
                }
            }
        }
    }

    // heal: tearing a run out of a stack's middle joins its old neighbors —
    // exactly one torn feed from a resting block and one torn exit to one
    const SceneWire* torn_in = nullptr;
    const SceneWire* torn_out = nullptr;
    int ins = 0, outs = 0;
    for (const auto& w : torn) {
        const SceneNode* tn = scene.find(w.to);
        const ScenePort* tp = tn ? find_aux(*tn, w.to_port, false) : nullptr;
        if (!tp || tp->name != "prev") continue; // statement chain only
        if (fin.count(w.to) && !fin.count(w.from)) {
            torn_in = &w;
            ++ins;
        } else if (fin.count(w.from) && !fin.count(w.to)) {
            torn_out = &w;
            ++outs;
        }
    }
    if (ins == 1 && outs == 1 && torn_in->from != torn_out->to) {
        const SceneNode* fnn = scene.find(torn_in->from);
        const SceneNode* tnn = scene.find(torn_out->to);
        const ScenePort* fp = fnn ? find_aux(*fnn, torn_in->from_port, true) : nullptr;
        const ScenePort* tp = tnn ? find_aux(*tnn, torn_out->to_port, false) : nullptr;
        if (fp && tp && port_free(torn_in->from, torn_in->from_port, true) &&
            port_free(torn_out->to, torn_out->to_port, false))
            add_link({torn_in->from, torn_in->from_port, true, fp->type},
                     {torn_out->to, torn_out->to_port, false, tp->type});
    }

    // assemble: moves (scene order), unlinks, links, then the re-flow of any
    // resting chains the links changed — ONE command, one undo frame
    std::vector<std::string> cmds;
    for (const auto& n : scene.nodes) {
        auto it = fin.find(n.name);
        if (it != fin.end())
            cmds.push_back(compile_move(n.name, it->second.first, it->second.second));
    }
    cmds.insert(cmds.end(), unlink_cmds.begin(), unlink_cmds.end());
    cmds.insert(cmds.end(), link_cmds.begin(), link_cmds.end());
    if (!added.empty() || !removed.empty()) {
        std::vector<SceneWire> wires;
        for (const auto& w : scene.wires) {
            bool gone = false;
            for (const auto& u : removed)
                if (u.from == w.from && u.to == w.to && u.relation == w.relation)
                    gone = true;
            if (!gone) wires.push_back(w);
        }
        wires.insert(wires.end(), added.begin(), added.end());
        for (const auto& [name, p] : layout_moves(scene, wires, fin, m))
            cmds.push_back(compile_move(name, p.first, p.second));
    }
    if (cmds.empty()) return {};
    if (cmds.size() == 1) return cmds[0];
    return compile_batch(cmds);
}

std::string compile_stack_layout(const Scene& scene, const BlockMetrics& m) {
    return compile_moves(layout_moves(scene, scene.wires, {}, m));
}

bool parse_camera(std::string_view value, Camera& out) {
    // tolerate a JSON-quoted payload (config get hands back "\"x y z\"")
    while (!value.empty() && (value.front() == '"' || value.front() == ' ')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == '"' || value.back() == ' ')) value.remove_suffix(1);
    Camera c;
    char extra;
    std::string owned(value);
    int n = std::sscanf(owned.c_str(), "%f %f %f %c", &c.x, &c.y, &c.zoom, &extra);
    if (n != 3 || !(c.zoom > 0.01f && c.zoom < 100.0f) || !std::isfinite(c.x) || !std::isfinite(c.y))
        return false;
    out = c;
    return true;
}

} // namespace maiz
