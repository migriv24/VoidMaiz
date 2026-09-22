/* wires_smoke.cpp — connections as wire runes, on one device (voidmaiz/wires.hpp).
 * The two-device case (the shared wire under concurrent rewrites) is net_smoke's.
 *
 * Everything here goes through a real Core and a real projection: the encoding is
 * only worth anything if the commands it compiles are the ones the dispatcher
 * accepts and the projection reads back. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/reduce.hpp"
#include "voidmaiz/wires.hpp"

#include <iostream>
#include <map>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz;

static bool ok(Core& core, const std::string& cmd) {
    Result r = core.dispatch(cmd);
    if (!r.ok) std::cerr << "  dispatch failed: " << cmd << "\n    " << r.text() << "\n";
    return r.ok;
}

static const SceneWire* wire_between(const Scene& s, const std::string& a, int ap,
                                     const std::string& b, int bp) {
    for (const auto& w : s.wires)
        if ((w.from == a && w.from_port == ap && w.to == b && w.to_port == bp) ||
            (w.from == b && w.from_port == bp && w.to == a && w.to_port == ap))
            return &w;
    return nullptr;
}

int main() {
    WireEncoding enc;
    Core core;
    core.register_glyph(R"({"glyph":"gamma","label":"gamma","fields":[]})");
    core.register_glyph(R"({"glyph":"wire","label":"wire","fields":[]})");
    CHECK(ok(core, "mantle new net"));
    CHECK(ok(core, "use net"));
    CHECK(ok(core, "rune new gamma a"));
    CHECK(ok(core, "rune new gamma c"));

    // ── a wire between two ports ──────────────────────────────────────────────
    CHECK(ok(core, compile_wire(enc, fresh_wire_name("dev1", 1), {"a", 1}, {"c", 2})));
    {
        Scene raw = project_scene(core);
        std::vector<WireClass> classes;
        Scene s = collapse_wires(raw, enc, &classes);
        CHECK(classes.size() == 1);
        CHECK(classes.size() == 1 && classes[0].ends.size() == 2);
        CHECK(s.find("w-dev1-1") == nullptr); // the wire rune is not drawn as a node
        const SceneWire* w = wire_between(s, "a", 1, "c", 2);
        CHECK(w != nullptr);
        CHECK(w && w->via == "w-dev1-1" && !w->contested && !w->directed);
        CHECK(w && w->kind == SceneWire::Kind::Linguine && w->relation == "1:2");
        CHECK(s.wires.size() == 1);
    }

    // ── a rewrite consumes `a` and inherits its boundary by FUSION ────────────
    // (never by editing the old segment: mint, attach, fuse, remove the consumed)
    CHECK(ok(core, "rm a"));
    CHECK(ok(core, "rune new gamma a2"));
    CHECK(ok(core, compile_segment(enc, "w-dev1-2")));
    CHECK(ok(core, compile_attach("w-dev1-2", {"a2", 1})));
    CHECK(ok(core, compile_fuse(enc, "w-dev1-2", "w-dev1-1")));
    {
        std::vector<WireClass> classes;
        Scene s = collapse_wires(project_scene(core), enc, &classes);
        CHECK(classes.size() == 1);
        CHECK(classes.size() == 1 && classes[0].segments.size() == 2);
        CHECK(wire_between(s, "a2", 1, "c", 2) != nullptr);
        CHECK(s.wires.size() == 1); // the removed a's attachment is not a phantom end
        // the representative is the least spirit.id, not the least name
        const WireClass* c = class_of(classes, {"c", 2});
        CHECK(c && (c->rep == "w-dev1-1" || c->rep == "w-dev1-2"));
    }

    // ── principal to principal is an interaction wire ─────────────────────────
    CHECK(ok(core, "rune new gamma p"));
    CHECK(ok(core, "rune new gamma q"));
    CHECK(ok(core, compile_wire(enc, "w-dev1-3", {"p", 0}, {"q", 0})));
    {
        Scene s = collapse_wires(project_scene(core), enc);
        const SceneWire* w = wire_between(s, "p", 0, "q", 0);
        CHECK(w && w->kind == SceneWire::Kind::Fettuccine);
    }

    // ── detaching an end leaves a free port and keeps the segment ─────────────
    CHECK(ok(core, compile_detach("w-dev1-3", {"q", 0})));
    {
        std::vector<WireClass> classes;
        Scene s = collapse_wires(project_scene(core), enc, &classes);
        CHECK(wire_between(s, "p", 0, "q", 0) == nullptr);
        const WireClass* c = class_of(classes, {"p", 0});
        CHECK(c && c->ends.size() == 1 && c->segments.size() == 1);
    }

    // ── three ends on one class: drawn, and marked contested ──────────────────
    CHECK(ok(core, "rune new gamma x"));
    CHECK(ok(core, compile_attach("w-dev1-3", {"q", 0})));
    CHECK(ok(core, compile_attach("w-dev1-3", {"x", 0})));
    {
        Scene s = collapse_wires(project_scene(core), enc);
        int contested = 0;
        for (const auto& w : s.wires) contested += w.contested ? 1 : 0;
        CHECK(contested == 2);
    }

    // ── a plain edge between two agents passes through untouched ──────────────
    CHECK(ok(core, "link c x --relation 0:1"));
    {
        Scene s = collapse_wires(project_scene(core), enc);
        const SceneWire* w = wire_between(s, "c", 0, "x", 1);
        CHECK(w && w->via.empty());
    }

    // ── the upgrade: plain edges become wire runes, and the net is unchanged ──
    {
        Core old;
        old.register_glyph(R"({"glyph":"gamma","label":"gamma","fields":[]})");
        old.register_glyph(R"({"glyph":"wire","label":"wire","fields":[]})");
        CHECK(ok(old, "mantle new lafont"));
        CHECK(ok(old, "use lafont"));
        for (const char* n : {"g1", "g2", "g3"}) CHECK(ok(old, std::string("rune new gamma ") + n));
        CHECK(ok(old, "link g1 g2 --relation 0:0 --undirected"));
        CHECK(ok(old, "link g1 g3 --relation 1:2 --undirected"));
        std::map<std::string, int> sig = {{"gamma", 2}};
        reduce::Net before = reduce::to_net(extract_mantle(old.export_state()), sig);

        unsigned long k = next_wire_counter(project_scene(old), "up");
        CHECK(k == 1);
        std::vector<std::string> up =
            compile_upgrade(project_scene(old), enc, [&] { return fresh_wire_name("up", k++); });
        CHECK(up.size() == 8); // two edges: unlink, segment, attach, attach
        CHECK(ok(old, compile_batch(up)));
        CHECK(compile_upgrade(project_scene(old), enc, [&] { return std::string("x"); }).empty());
        CHECK(next_wire_counter(project_scene(old), "up") == 3);

        Scene raw = project_scene(old);
        Scene s = collapse_wires(raw, enc);
        for (const auto& w : s.wires) CHECK(!w.via.empty()); // no plain edge left
        // the net read from the collapsed scene is the net the old edges described
        reduce::Net after = reduce::to_net(s, sig);
        CHECK(after.link == before.link);
        CHECK(after.agents.size() == before.agents.size());
    }

    // ── the canvas's writer: link and cut through wire runes ──────────────────
    {
        std::vector<WireClass> classes;
        unsigned long n = 100;
        WireWriter ww = reified_writer(
            enc, [&] { return fresh_wire_name("dev1", n++); },
            [&]() -> const std::vector<WireClass>& { return classes; });
        CHECK(static_cast<bool>(ww));
        CHECK(ok(core, "rune new gamma m"));
        CHECK(ok(core, "rune new gamma o"));
        CHECK(ok(core, compile_batch(ww.link({"m", 1, false, ""}, {"o", 2, false, ""}))));
        Scene s = collapse_wires(project_scene(core), enc, &classes);
        const SceneWire* w = wire_between(s, "m", 1, "o", 2);
        CHECK(w && !w->via.empty());
        if (w) {
            SceneWire cut = *w;
            CHECK(ok(core, compile_batch(ww.unlink(cut))));
            Scene after = collapse_wires(project_scene(core), enc, &classes);
            CHECK(wire_between(after, "m", 1, "o", 2) == nullptr);
        }
        // a plain edge is still cut as a plain edge
        SceneWire plain;
        plain.from = "c";
        plain.to = "x";
        plain.relation = "0:1";
        CHECK(ww.unlink(plain).size() == 1 && ww.unlink(plain)[0].rfind("unlink c x", 0) == 0);
    }

    if (failures) {
        std::cerr << "wires_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "wires_smoke: all ok\n";
    return 0;
}
