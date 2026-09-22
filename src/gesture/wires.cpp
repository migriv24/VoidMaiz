/* wires.cpp — connections as wire runes (voidmaiz/wires.hpp). */
#include "voidmaiz/wires.hpp"

#include "voidmaiz/embed.hpp"   // arg(): quote a name for a command line
#include "voidmaiz/gesture.hpp" // compile_batch

#include <algorithm>
#include <map>
#include <set>

namespace maiz {

bool is_wire(const SceneNode& n, const WireEncoding& enc) { return n.glyph == enc.glyph; }

namespace {

/* Union-find over segment names. The representative is chosen AFTER the classes
 * are known (least spirit.id), so it never depends on the order links were read. */
struct Dsu {
    std::map<std::string, std::string> parent;
    std::string find(const std::string& x) {
        auto it = parent.find(x);
        if (it == parent.end()) {
            parent[x] = x;
            return x;
        }
        if (it->second == x) return x;
        std::string r = find(it->second);
        parent[x] = r;
        return r;
    }
    void unite(const std::string& a, const std::string& b) {
        std::string ra = find(a), rb = find(b);
        if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
    }
};

} // namespace

std::vector<WireClass> wire_classes(const Scene& raw, const WireEncoding& enc) {
    std::map<std::string, const SceneNode*> wires; // name → node
    for (const auto& n : raw.nodes)
        if (is_wire(n, enc)) wires[n.name] = &n;

    Dsu dsu;
    for (const auto& [name, n] : wires) dsu.find(name);
    // A link whose endpoint names no live node is skipped, as Palabra's
    // resolved_links does: a removed agent's dangling attachment is a
    // `link_broken`, not a phantom third end.
    std::set<std::string> live;
    for (const auto& n : raw.nodes) live.insert(n.name);
    std::vector<std::pair<std::string, WireEnd>> attach; // segment, end
    for (const auto& w : raw.wires) {
        if (!live.count(w.from) || !live.count(w.to)) continue;
        bool fw = wires.count(w.from) > 0, tw = wires.count(w.to) > 0;
        if (w.relation == enc.fuse) {
            if (fw && tw) dsu.unite(w.from, w.to);
            continue;
        }
        // an attachment: node → wire, "i:0" (either direction is read, so a link
        // written the other way round still counts rather than vanishing)
        if (fw == tw || w.from_port < 0 || w.to_port < 0) continue;
        if (tw) attach.push_back({w.to, {w.from, w.from_port}});
        else attach.push_back({w.from, {w.to, w.to_port}});
    }

    std::map<std::string, WireClass> by_root;
    for (const auto& [name, n] : wires) by_root[dsu.find(name)].segments.push_back(name);
    std::map<std::string, std::vector<std::pair<WireEnd, std::string>>> ends;
    for (const auto& [seg, end] : attach) ends[dsu.find(seg)].push_back({end, seg});

    std::vector<WireClass> out;
    for (auto& [root, c] : by_root) {
        std::sort(c.segments.begin(), c.segments.end());
        // representative: least spirit.id — the same on every peer, independent of names
        c.rep = *std::min_element(c.segments.begin(), c.segments.end(),
                                  [&](const std::string& a, const std::string& b) {
                                      return wires[a]->id < wires[b]->id;
                                  });
        auto& e = ends[root];
        std::sort(e.begin(), e.end());
        e.erase(std::unique(e.begin(), e.end(),
                            [](const auto& x, const auto& y) { return x.first == y.first; }),
                e.end());
        for (const auto& [end, seg] : e) {
            c.ends.push_back(end);
            c.end_segment.push_back(seg);
        }
        out.push_back(std::move(c));
    }
    std::sort(out.begin(), out.end(),
              [](const WireClass& a, const WireClass& b) { return a.rep < b.rep; });
    return out;
}

const WireClass* class_of(const std::vector<WireClass>& classes, const WireEnd& end) {
    for (const auto& c : classes)
        if (std::find(c.ends.begin(), c.ends.end(), end) != c.ends.end()) return &c;
    return nullptr;
}

static SceneWire drawn(const WireEnd& a, const WireEnd& b, const std::string& via) {
    SceneWire w;
    w.from = a.node;
    w.to = b.node;
    w.from_port = a.port;
    w.to_port = b.port;
    w.relation = std::to_string(a.port) + ":" + std::to_string(b.port);
    w.kind = (a.port == 0 && b.port == 0) ? SceneWire::Kind::Fettuccine
                                          : SceneWire::Kind::Linguine;
    w.directed = false;
    w.via = via;
    return w;
}

Scene collapse_wires(const Scene& raw, const WireEncoding& enc,
                     std::vector<WireClass>* classes_out) {
    std::vector<WireClass> classes = wire_classes(raw, enc);
    std::set<std::string> wire_names;
    for (const auto& n : raw.nodes)
        if (is_wire(n, enc)) wire_names.insert(n.name);

    Scene out;
    out.mantle = raw.mantle;
    for (const auto& n : raw.nodes)
        if (!is_wire(n, enc)) out.nodes.push_back(n);
    for (const auto& w : raw.wires)
        if (!wire_names.count(w.from) && !wire_names.count(w.to)) out.wires.push_back(w);
    for (const auto& c : classes) {
        if (c.ends.size() == 2) {
            out.wires.push_back(drawn(c.ends[0], c.ends[1], c.rep));
        } else if (c.ends.size() > 2) {
            for (std::size_t k = 1; k < c.ends.size(); ++k) {
                SceneWire w = drawn(c.ends[0], c.ends[k], c.rep);
                w.contested = true;
                out.wires.push_back(std::move(w));
            }
        }
    }
    if (classes_out) *classes_out = std::move(classes);
    return out;
}

// ── compiling the encoding ────────────────────────────────────────────────────

std::string fresh_wire_name(std::string_view device_tag, unsigned long counter) {
    return "w-" + std::string(device_tag) + "-" + std::to_string(counter);
}

static std::string port_rel(int port) { return std::to_string(port) + ":0"; }

std::string compile_attach(std::string_view segment, const WireEnd& end) {
    return "link " + arg(end.node) + " " + arg(segment) + " --relation " + port_rel(end.port);
}

std::string compile_detach(std::string_view segment, const WireEnd& end) {
    return "unlink " + arg(end.node) + " " + arg(segment) + " --relation " + port_rel(end.port);
}

std::string compile_fuse(const WireEncoding& enc, std::string_view newer, std::string_view older) {
    return "link " + arg(newer) + " " + arg(older) + " --relation " + arg(enc.fuse);
}

std::string compile_segment(const WireEncoding& enc, std::string_view name) {
    return "rune new " + arg(enc.glyph) + " " + arg(name);
}

std::string compile_wire(const WireEncoding& enc, std::string_view wire_name, const WireEnd& a,
                         const WireEnd& b) {
    return compile_batch({compile_segment(enc, wire_name), compile_attach(wire_name, a),
                          compile_attach(wire_name, b)});
}

unsigned long next_wire_counter(const Scene& raw, std::string_view device_tag) {
    std::string prefix = "w-" + std::string(device_tag) + "-";
    unsigned long next = 1;
    for (const auto& n : raw.nodes) {
        if (n.name.compare(0, prefix.size(), prefix) != 0) continue;
        std::string tail = n.name.substr(prefix.size());
        if (tail.empty() || tail.find_first_not_of("0123456789") != std::string::npos) continue;
        next = std::max(next, std::stoul(tail) + 1);
    }
    return next;
}

std::vector<std::string> compile_upgrade(const Scene& raw, const WireEncoding& enc,
                                         const std::function<std::string()>& fresh) {
    std::set<std::string> wire_names;
    for (const auto& n : raw.nodes)
        if (is_wire(n, enc)) wire_names.insert(n.name);
    std::vector<std::string> cmds;
    for (const auto& w : raw.wires) {
        if (w.from_port < 0 || w.to_port < 0) continue; // Loose: not a net wire
        if (wire_names.count(w.from) || wire_names.count(w.to)) continue;
        std::string seg = fresh();
        cmds.push_back("unlink " + arg(w.from) + " " + arg(w.to) + " --relation " + arg(w.relation));
        cmds.push_back(compile_segment(enc, seg));
        cmds.push_back(compile_attach(seg, {w.from, w.from_port}));
        cmds.push_back(compile_attach(seg, {w.to, w.to_port}));
    }
    return cmds;
}

WireWriter reified_writer(WireEncoding enc, std::function<std::string()> fresh,
                          std::function<const std::vector<WireClass>&()> classes) {
    WireWriter ww;
    ww.link = [enc, fresh](const PortRef& from, const PortRef& to) {
        std::string seg = fresh();
        return std::vector<std::string>{compile_segment(enc, seg),
                                        compile_attach(seg, {from.node, from.port}),
                                        compile_attach(seg, {to.node, to.port})};
    };
    ww.unlink = [classes](const SceneWire& w) {
        if (w.via.empty()) return std::vector<std::string>{compile_unlink(w)};
        // cut the wire: detach both drawn ends from the segments they hang on.
        // The segments stay (another device may be fusing onto them).
        std::vector<std::string> cmds;
        for (const auto& c : classes()) {
            if (c.rep != w.via) continue;
            for (std::size_t k = 0; k < c.ends.size(); ++k) {
                const WireEnd& e = c.ends[k];
                bool drawn_end = (e.node == w.from && e.port == w.from_port) ||
                                 (e.node == w.to && e.port == w.to_port);
                if (drawn_end) cmds.push_back(compile_detach(c.end_segment[k], e));
            }
        }
        return cmds;
    };
    return ww;
}

} // namespace maiz
