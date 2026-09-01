/* gesture_smoke.cpp — the gesture→command compilers, end-to-end.
 * The builders are pure, so we test the strings AND dispatch them against a
 * live core: a multi-node move must land as ONE batch (one undo frame — the
 * lasagna model's group-drag-is-one-action), same for multi-delete; the
 * camera must round-trip through the undo-exempt config tier. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/project.hpp"

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

int main() {
    // ── pure builders ────────────────────────────────────────────────────────
    CHECK(maiz::compile_move("lfo", 120.4f, 330.6f) == "setjson lfo pos [120,331]");
    CHECK(maiz::compile_moves({{"a", {1, 2}}}) == "setjson a pos [1,2]");
    CHECK(maiz::compile_moves({{"a", {1, 2}}, {"b", {3, 4}}}) ==
          "batch '[\"setjson a pos [1,2]\",\"setjson b pos [3,4]\"]'");
    CHECK(maiz::compile_deletes({"a"}) == "rm a");
    CHECK(maiz::compile_deletes({"a", "b"}) == "batch '[\"rm a\",\"rm b\"]'");

    // the multi-field commit helper (Void Hormiga §1.3): none/one/many
    CHECK(maiz::compile_commit({}) == "");
    CHECK(maiz::compile_commit({"tag p1 +draft"}) == "tag p1 +draft"); // lone → no batch
    CHECK(maiz::compile_commit({"rune new person p1", "tag p1 +draft"}) ==
          "batch '[\"rune new person p1\",\"tag p1 +draft\"]'"); // wizard finish = one frame

    maiz::Camera cam{-40.0f, 12.0f, 1.25f};
    CHECK(maiz::compile_camera(cam) == "config set view.camera \"-40 12 1.25\"");
    maiz::Camera parsed;
    CHECK(maiz::parse_camera("\"-40 12 1.250\"", parsed));
    CHECK(parsed.x == -40.0f && parsed.y == 12.0f && parsed.zoom == 1.25f);
    CHECK(!maiz::parse_camera("garbage", parsed));
    CHECK(!maiz::parse_camera("1 2 0", parsed)); // zoom must be positive

    // a SECOND view persists its own viewport under its own view.* key, at
    // full precision — a geographic camera (x=lon, y=lat, zoom=map zoom)
    // survives the round-trip where the node camera's integer pixels would.
    maiz::Camera geo{-73.985f, 40.748f, 14.0f};
    CHECK(maiz::compile_camera(geo, "view.map.camera") ==
          "config set view.map.camera \"-73.985 40.748 14\"");
    maiz::Camera geo_back;
    CHECK(maiz::parse_camera("\"-73.985 40.748 14\"", geo_back));
    CHECK(geo_back.x == -73.985f && geo_back.y == 40.748f && geo_back.zoom == 14.0f);

    // ── wire verdicts (the type checker) ─────────────────────────────────────
    using V = maiz::WireVerdict;
    maiz::PortRef a_prin{"a", 0, false, ""};
    maiz::PortRef b_prin{"b", 0, false, ""};
    maiz::PortRef a_out{"a", 2, true, "audio"};
    maiz::PortRef b_in{"b", 1, false, "audio"};
    maiz::PortRef b_in_f{"b", 2, false, "float"};
    maiz::PortRef b_out{"b", 3, true, "audio"};
    maiz::PortRef b_in_any{"b", 4, false, ""};
    CHECK(maiz::check_wire(a_prin, b_prin) == V::Fettuccine);
    CHECK(maiz::check_wire(a_prin, b_in) == V::Linguine);   // principal ↔ aux: passive wire
    CHECK(maiz::check_wire(a_prin, b_out) == V::Linguine);  // either aux direction works
    CHECK(maiz::check_wire(b_in_f, a_prin) == V::Linguine); // principal = untyped wildcard
    CHECK(maiz::check_wire(a_out, b_in) == V::Linguine);  // types match
    CHECK(maiz::check_wire(b_in, a_out) == V::Linguine);  // order-independent
    CHECK(maiz::check_wire(a_out, b_in_f) == V::Reject);  // audio vs float
    CHECK(maiz::check_wire(a_out, b_out) == V::Reject);   // out ↔ out
    CHECK(maiz::check_wire(a_out, b_in_any) == V::Linguine); // untyped wildcard
    CHECK(maiz::check_wire(a_prin, maiz::PortRef{"a", 0, false, ""}) == V::Reject); // self

    CHECK(maiz::compile_link(a_out, b_in) == "link a b --relation 2:1");
    CHECK(maiz::compile_link(a_prin, b_prin) == "link a b --relation 0:0 --undirected");
    // principal↔aux passive wires: principal feeds an aux input (0:j); an aux
    // output feeds the principal (i:0). Directed, no --undirected.
    CHECK(maiz::compile_link(a_prin, b_in) == "link a b --relation 0:1");
    CHECK(maiz::compile_link(b_out, a_prin) == "link b a --relation 3:0");
    maiz::SceneWire old_wire;
    old_wire.from = "c";
    old_wire.to = "b";
    old_wire.from_port = 2;
    old_wire.to_port = 1;
    old_wire.relation = "2:1";
    CHECK(maiz::compile_unlink(old_wire) == "unlink c b --relation 2:1");
    CHECK(maiz::compile_rewire({}, a_out, b_in) == "link a b --relation 2:1");
    CHECK(maiz::compile_rewire({old_wire}, a_out, b_in) ==
          "batch '[\"unlink c b --relation 2:1\",\"link a b --relation 2:1\"]'");

    CHECK(maiz::compile_add("osc", "osc-1", 40.2f, 60.7f) ==
          "batch '[\"rune new osc osc-1\",\"setjson osc-1 pos [40,61]\"]'");

    // field editing: values ride single-quoted, embedded quotes escape
    CHECK(maiz::compile_set("a", "note", "it's here") == "set a note 'it\\'s here'");
    CHECK(maiz::compile_setjson("a", "gain", "2.5") == "setjson a gain '2.5'");

    // tag chips
    CHECK(maiz::compile_tag("a", "red", true) == "tag a +red");
    CHECK(maiz::compile_tag("a", "red", false) == "tag a -red");

    // clean view: overlapping boxes get separating moves, bystanders don't;
    // an already-clean scene compiles nothing
    {
        maiz::Scene s;
        maiz::SceneNode n1, n2, n3;
        n1.name = "a"; n1.x = 0;   n1.y = 0;   n1.w = 100; n1.h = 80;
        n2.name = "b"; n2.x = 20;  n2.y = 10;  n2.w = 100; n2.h = 80;
        n3.name = "c"; n3.x = 500; n3.y = 500; n3.w = 100; n3.h = 80;
        s.nodes = {n1, n2, n3};
        std::string cmd = maiz::compile_clean(s);
        CHECK(!cmd.empty());
        CHECK(cmd.find("setjson a pos") != std::string::npos);
        CHECK(cmd.find("setjson b pos") != std::string::npos);
        CHECK(cmd.find("setjson c pos") == std::string::npos);
        maiz::Scene apart;
        n2.x = 300;
        apart.nodes = {n1, n2, n3};
        CHECK(maiz::compile_clean(apart).empty());
    }

    // physics relax: overlapping bodies separate (repulsion), wired bodies
    // stay near their rest length (springs), a spread scene is at rest, and
    // the whole relax lands as ONE command
    {
        maiz::Scene s;
        maiz::SceneNode n1, n2, n3;
        n1.name = "a"; n1.x = 0;   n1.y = 0;   n1.w = 96; n1.h = 96;
        n2.name = "b"; n2.x = 10;  n2.y = 6;   n2.w = 96; n2.h = 96; // overlapping a
        n3.name = "c"; n3.x = 900; n3.y = 900; n3.w = 96; n3.h = 96; // far bystander
        s.nodes = {n1, n2, n3};
        maiz::SceneWire w; // a—b wired: they separate but stay tethered
        w.from = "a"; w.to = "b"; w.from_port = 0; w.to_port = 0;
        w.kind = maiz::SceneWire::Kind::Fettuccine;
        s.wires = {w};
        std::string cmd = maiz::compile_relax(s);
        CHECK(!cmd.empty());
        CHECK(cmd.rfind("batch ", 0) == 0 || cmd.rfind("setjson ", 0) == 0); // one command
        CHECK(cmd.find("setjson c pos") == std::string::npos); // bystander untouched
        maiz::PositionMap pos;
        for (int i = 0; i < 240; ++i)
            if (maiz::relax_step(s, pos, {}) < 0.2f) break;
        auto center = [&](const char* n, float w_, float h_) {
            return std::pair<float, float>{pos.at(n).first + w_ * 0.5f,
                                           pos.at(n).second + h_ * 0.5f};
        };
        auto [ax, ay] = center("a", 96, 96);
        auto [bx, by] = center("b", 96, 96);
        float d = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        CHECK(d > 96.0f);   // no longer overlapping
        CHECK(d < 400.0f);  // …but the spring kept them together
        // a spread, unwired scene is already at rest: nothing compiles
        maiz::Scene rest;
        n2.x = 400; n2.y = 0;
        rest.nodes = {n1, n2, n3};
        CHECK(maiz::compile_relax(rest).empty());
    }

    // chrome & subgraphs
    CHECK(maiz::compile_collapse("a", true) == "setjson a collapsed true");
    CHECK(maiz::compile_collapse("a", false) == "setjson a collapsed false");
    CHECK(maiz::compile_resize("a", 200.4f, 120.6f) == "setjson a size [200,121]");
    CHECK(maiz::compile_use("inner") == "use inner");

    // ── against a live core ──────────────────────────────────────────────────
    maiz::Core core;
    core.register_glyph(R"({"glyph":"box","label":"Box","fields":[]})");
    core.dispatch("mantle new g");
    core.dispatch("rune new box a");
    core.dispatch("rune new box b");
    core.dispatch("setjson a pos [0,0]");
    core.dispatch("setjson b pos [0,100]");

    // A group drag: one batch, both move, ONE undo restores BOTH.
    maiz::Result r = core.dispatch(maiz::compile_moves({{"a", {50, 60}}, {"b", {70, 80}}}));
    CHECK(r.ok);
    maiz::Scene s = maiz::project_scene(core);
    const maiz::SceneNode* a = s.find("a");
    const maiz::SceneNode* b = s.find("b");
    CHECK(a && a->x == 50 && a->y == 60);
    CHECK(b && b->x == 70 && b->y == 80);
    CHECK(core.dispatch("undo").ok);
    s = maiz::project_scene(core);
    a = s.find("a");
    b = s.find("b");
    CHECK(a && a->x == 0 && a->y == 0);     // both restored by the one undo
    CHECK(b && b->x == 0 && b->y == 100);

    // A group delete: one batch, both gone, one undo resurrects both.
    CHECK(core.dispatch(maiz::compile_deletes({"a", "b"})).ok);
    CHECK(maiz::project_scene(core).nodes.empty());
    CHECK(core.dispatch("undo").ok);
    CHECK(maiz::project_scene(core).nodes.size() == 2);

    // A rewire batch through the core: c->b displaced by a->b in ONE undo frame.
    core.dispatch("rune new box c");
    core.dispatch(maiz::compile_link({"c", 2, true, ""}, {"b", 1, false, ""}));
    maiz::Scene ws = maiz::project_scene(core);
    CHECK(ws.wires.size() == 1 && ws.wires[0].from == "c");
    const maiz::SceneWire* occ = nullptr;
    for (const auto& w : ws.wires)
        if (w.to == "b" && w.to_port == 1) occ = &w;
    CHECK(occ != nullptr);
    if (occ) {
        CHECK(core.dispatch(maiz::compile_rewire({*occ}, {"a", 2, true, ""}, {"b", 1, false, ""})).ok);
        ws = maiz::project_scene(core);
        CHECK(ws.wires.size() == 1 && ws.wires[0].from == "a"); // rewired
        CHECK(core.dispatch("undo").ok);                        // ONE frame…
        ws = maiz::project_scene(core);
        CHECK(ws.wires.size() == 1 && ws.wires[0].from == "c"); // …restores c->b
    }

    // Field edit through the core: quoted payload survives, projection shows it.
    core.register_glyph(R"({"glyph":"txt","label":"Text","fields":["msg"]})");
    core.dispatch("rune new txt t");
    CHECK(core.dispatch(maiz::compile_set("t", "msg", "it's a 'test'")).ok);
    {
        maiz::Scene fs = maiz::project_scene(core);
        const maiz::SceneNode* t = fs.find("t");
        CHECK(t && t->fields.size() == 1);
        if (t && t->fields.size() == 1)
            CHECK(t->fields[0].value_json == "\"it's a 'test'\"");
    }

    // Camera → config tier: logged+persisted, outside the undo slice.
    CHECK(core.dispatch(maiz::compile_camera(cam)).ok);
    maiz::Result got = core.dispatch("config get view.camera");
    CHECK(got.ok);
    maiz::Camera back;
    CHECK(maiz::parse_camera(got.data, back));
    CHECK(back.x == -40.0f && back.y == 12.0f && back.zoom == 1.25f);
    CHECK(core.dispatch("undo").ok);                     // pops a model change…
    CHECK(maiz::parse_camera(core.dispatch("config get view.camera").data, back)); // …not the camera

    // ── blocks: snap-to-connect, tears, heal, stack layout ───────────────────
    {
        auto mk_block = [](const char* name, float x, float y) {
            maiz::SceneNode n;
            n.name = name;
            n.shape = maiz::NodeShape::Block;
            n.x = x;
            n.y = y;
            n.w = 150;
            n.h = 40;
            maiz::ScenePort prev;
            prev.index = 1;
            prev.name = "prev";
            prev.type = "flow";
            prev.adjacency = true;
            maiz::ScenePort next;
            next.index = 2;
            next.name = "next";
            next.type = "flow";
            next.adjacency = true;
            n.inputs = {prev};
            n.outputs = {next};
            return n;
        };
        auto mk_chain = [](const char* from, const char* to) {
            maiz::SceneWire w;
            w.from = from;
            w.to = to;
            w.from_port = 2;
            w.to_port = 1;
            w.kind = maiz::SceneWire::Kind::Linguine;
            w.style = maiz::SceneWire::Style::Adjacency;
            w.relation = "2:1";
            return w;
        };
        const maiz::BlockMetrics m;

        // connector anchors: notch on top, tab below, both at notch_x
        {
            maiz::SceneNode n = mk_block("a", 0, 0);
            float ax = 0, ay = 0;
            CHECK(maiz::block_anchor(n, 1, false, 0, 0, 150, 40, 1, m, ax, ay));
            CHECK(ax == 26.0f && ay == 0.0f); // prev: top edge
            CHECK(maiz::block_anchor(n, 2, true, 0, 0, 150, 40, 1, m, ax, ay));
            CHECK(ax == 26.0f && ay == 40.0f); // next: bottom edge
            CHECK(!maiz::block_anchor(n, 0, false, 0, 0, 150, 40, 1, m, ax, ay)); // principal reserved
        }

        // find_snap: a dragged prev near a resting next snaps, aligned
        {
            maiz::Scene s;
            s.nodes = {mk_block("a", 0, 0), mk_block("b", 500, 500)};
            maiz::StagedMap staged{{"b", {4.0f, 44.0f}}};
            maiz::SnapCandidate snap = maiz::find_snap(s, staged, m);
            CHECK(snap.valid);
            CHECK(snap.from.node == "a" && snap.from.port == 2);
            CHECK(snap.to.node == "b" && snap.to.port == 1);
            CHECK(snap.dragged == "b");
            CHECK(snap.dx == -4.0f && snap.dy == -4.0f);
            // …and the release compiles ONE batch: aligned move + link
            CHECK(maiz::compile_block_release(s, staged, m) ==
                  "batch '[\"setjson b pos [0,40]\",\"link a b --relation 2:1\"]'");
            // out of range: a plain move, no link
            maiz::StagedMap far{{"b", {300.0f, 300.0f}}};
            CHECK(!maiz::find_snap(s, far, m).valid);
            CHECK(maiz::compile_block_release(s, far, m) == "setjson b pos [300,300]");
        }

        // dropping on an occupied seam INSERTS: displace, splice, re-flow
        {
            maiz::Scene s;
            s.nodes = {mk_block("a", 0, 0), mk_block("c", 0, 40), mk_block("b", 500, 500)};
            s.wires = {mk_chain("a", "c")};
            maiz::StagedMap staged{{"b", {4.0f, 44.0f}}};
            CHECK(maiz::compile_block_release(s, staged, m) ==
                  "batch '[\"setjson b pos [0,40]\",\"unlink a c --relation 2:1\","
                  "\"link a b --relation 2:1\",\"link b c --relation 2:1\","
                  "\"setjson c pos [0,80]\"]'");
        }

        // dragging a middle block out TEARS both wires and HEALS the stack
        {
            maiz::Scene s;
            s.nodes = {mk_block("a", 0, 0), mk_block("b", 0, 40), mk_block("c", 0, 80)};
            s.wires = {mk_chain("a", "b"), mk_chain("b", "c")};
            maiz::StagedMap staged{{"b", {400.0f, 400.0f}}};
            CHECK(maiz::compile_block_release(s, staged, m) ==
                  "batch '[\"setjson b pos [400,400]\",\"unlink a b --relation 2:1\","
                  "\"unlink b c --relation 2:1\",\"link a c --relation 2:1\","
                  "\"setjson c pos [0,40]\"]'");
            // grab-the-stack: the chain below rides along
            auto stack = maiz::block_stack_below(s, "a");
            CHECK(stack.size() == 3);
            CHECK(maiz::block_stack_below(s, "b").size() == 2);
        }

        // stack re-flow: linked-but-apart blocks come flush; flush stays put
        {
            maiz::Scene s;
            s.nodes = {mk_block("a", 0, 0), mk_block("b", 200, 90)};
            s.wires = {mk_chain("a", "b")};
            CHECK(maiz::compile_stack_layout(s, m) == "setjson b pos [0,40]");
            maiz::Scene flush;
            flush.nodes = {mk_block("a", 0, 0), mk_block("b", 0, 40)};
            flush.wires = {mk_chain("a", "b")};
            CHECK(maiz::compile_stack_layout(flush, m).empty());
        }

        // …and through a live core: the snap batch is ONE undo frame
        {
            maiz::Core bcore;
            bcore.register_glyph(
                R"({"glyph":"move","label":"move","fields":["steps"],)"
                R"("hints":{"shape":{"kind":"block"},)"
                R"("ports":[{"name":"prev","dir":"in","type":"flow","render":"adjacency"},)"
                R"(          {"name":"next","dir":"out","type":"flow","render":"adjacency"}]}})");
            bcore.dispatch("mantle new prog");
            bcore.dispatch("rune new move m1");
            bcore.dispatch("rune new move m2");
            bcore.dispatch("setjson m1 pos [0,0]");
            maiz::Scene bs = maiz::project_scene(bcore);
            const maiz::SceneNode* m1 = bs.find("m1");
            CHECK(m1 && m1->shape == maiz::NodeShape::Block);
            maiz::StagedMap staged{{"m2", {4.0f, 44.0f}}};
            maiz::Result br = bcore.dispatch(maiz::compile_block_release(bs, staged, m));
            CHECK(br.ok);
            maiz::Scene snapped = maiz::project_scene(bcore);
            CHECK(snapped.wires.size() == 1 &&
                  snapped.wires[0].style == maiz::SceneWire::Style::Adjacency);
            const maiz::SceneNode* m2 = snapped.find("m2");
            CHECK(m2 && m2->placed && m2->x == 0.0f && m2->y == 40.0f);
            CHECK(bcore.dispatch("undo").ok); // ONE frame: link AND position
            maiz::Scene undone = maiz::project_scene(bcore);
            CHECK(undone.wires.empty());
            m2 = undone.find("m2");
            CHECK(m2 && !m2->placed);
        }
    }

    if (failures == 0) {
        std::cout << "OK — gesture smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
