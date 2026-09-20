/* project_smoke.cpp — the projection engine against a live core.
 * Builds a patch with all three wire kinds, hint-declared ports, a placed and
 * an unplaced node, then asserts the Scene: ports from hints, "i:j" wire
 * classification (0:0 = fettuccine), position conventions, depth auto-layout,
 * and the one-sync property (dispatch → re-project shows/undoes changes). */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <iostream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

int main() {
    maiz::Core core;
    core.register_glyph(
        R"({"glyph":"osc","label":"Oscillator","fields":["freq"],)"
        R"("hints":{"color":"#7a5cff","face":{"w":190,"h":100},)"
        R"("editors":{"freq":"number:0.1,0,10"},)"
        R"("labels":{"freq":"Frequency in Hz"},)" // avoid )" — it closes the raw literal
        R"("ports":[{"name":"prin","principal":true},)"
        R"(          {"name":"freq","dir":"in","type":"float"},)"
        R"(          {"name":"out","dir":"out","type":"audio"}]}})");
    core.register_glyph(R"({"glyph":"gain","label":"Gain","fields":["amount"],)"
                        R"("hints":{"ports":[{"name":"in","dir":"in","type":"audio"},)"
                        R"(                   {"name":"out","dir":"out","type":"audio"}]}})");
    core.register_glyph(R"({"glyph":"agent","label":"Agent","fields":[]})"); // no hints

    core.dispatch("mantle new patch");
    core.dispatch("rune new osc lfo");
    core.dispatch("rune new gain master");
    core.dispatch("rune new agent gamma");
    core.dispatch("rune new agent delta");
    core.dispatch("setjson lfo pos [100,200]"); // positions-as-content (interim convention)
    core.dispatch("set lfo freq 2.5");

    // osc.out(aux 2) -> gain.in(aux 1): linguine.  gamma<->delta principals: fettuccine.
    core.dispatch("link lfo master --relation 2:1");
    core.dispatch("link gamma delta --relation 0:0");
    core.dispatch("link gamma lfo --relation annotates"); // loose semantic link

    maiz::Scene scene = maiz::project_scene(core);
    CHECK(scene.mantle == "patch");
    CHECK(scene.nodes.size() == 4);
    CHECK(scene.wires.size() == 3);

    // Ports from hints: principal named, aux indices 1..n in declaration order.
    const maiz::SceneNode* lfo = scene.find("lfo");
    CHECK(lfo != nullptr);
    if (lfo) {
        CHECK(lfo->label == "Oscillator");
        CHECK(lfo->principal_name == "prin");
        CHECK(lfo->inputs.size() == 1 && lfo->inputs[0].index == 1 && lfo->inputs[0].type == "float");
        CHECK(lfo->outputs.size() == 1 && lfo->outputs[0].index == 2 && lfo->outputs[0].type == "audio");
        CHECK(lfo->has_color && lfo->rgb == 0x7a5cff);
    }

    // content.color overrides hints.color (per-rune undoable recolor); an
    // invalid value falls back to the hint.
    core.dispatch("set lfo color '#ff8800'");
    {
        maiz::Scene recolored = maiz::project_scene(core);
        const maiz::SceneNode* n = recolored.find("lfo");
        CHECK(n && n->has_color && n->rgb == 0xff8800);
    }
    core.dispatch("set lfo color 'not-a-color'");
    {
        maiz::Scene fallback = maiz::project_scene(core);
        const maiz::SceneNode* n = fallback.find("lfo");
        CHECK(n && n->has_color && n->rgb == 0x7a5cff);
    }
    core.dispatch("undo");
    core.dispatch("undo"); // both recolors undone → hint color again
    maiz::Scene scene2 = maiz::project_scene(core);
    lfo = scene2.find("lfo");
    CHECK(lfo != nullptr);
    if (lfo) {
        CHECK(lfo->has_color && lfo->rgb == 0x7a5cff);
        CHECK(lfo->w == 190.0f);
        CHECK(lfo->placed && lfo->x == 100.0f && lfo->y == 200.0f); // content.pos honored
        // Fields: glyph-declared only (pos never surfaces), valued from content.
        CHECK(lfo->fields.size() == 1);
        if (lfo->fields.size() == 1) {
            CHECK(lfo->fields[0].key == "freq");
            CHECK(lfo->fields[0].is_string && lfo->fields[0].value_json == "\"2.5\"");
            // hints.editors binds the field to a widget-registry editor spec;
            // hints.labels binds the human-facing label (else empty → key)
            CHECK(lfo->fields[0].editor == "number:0.1,0,10");
            CHECK(lfo->fields[0].label == "Frequency in Hz");
        }
    }

    // The filter convention (widget-registry tag awareness): ONE bag per node —
    // tags + name-as-tag + "glyph:<g>" — against the core's grammar. Empty and
    // malformed expressions both degrade to "show everything".
    core.dispatch("tag lfo +modulation");
    {
        maiz::Scene fs = maiz::project_scene(core);
        const maiz::SceneNode* n = fs.find("lfo");
        CHECK(n != nullptr);
        if (n) {
            CHECK(maiz::node_matches("", *n));
            CHECK(maiz::node_matches("modulation", *n));
            CHECK(maiz::node_matches("lfo", *n));         // name-as-tag
            CHECK(maiz::node_matches("glyph:osc", *n));   // glyph-as-tag
            CHECK(!maiz::node_matches("glyph:gain", *n));
            CHECK(maiz::node_matches("modulation AND NOT", *n)); // malformed → match
        }
        const maiz::SceneNode* other = fs.find("master");
        CHECK(other && !maiz::node_matches("modulation", *other));
        // fields without an editors/labels hint project the empty fallbacks
        CHECK(other && other->fields.size() == 1 && other->fields[0].editor.empty());
        CHECK(other && other->fields[0].label.empty());
    }
    core.dispatch("undo"); // drop the tag again

    // Wire classification.
    int linguine = 0, fettuccine = 0, loose = 0;
    for (const auto& w : scene.wires) {
        if (w.kind == maiz::SceneWire::Kind::Linguine) {
            ++linguine;
            CHECK(w.from == "lfo" && w.from_port == 2 && w.to == "master" && w.to_port == 1);
            CHECK(w.directed);
        } else if (w.kind == maiz::SceneWire::Kind::Fettuccine) {
            ++fettuccine;
            CHECK(!w.directed); // interaction wires are symmetric
        } else {
            ++loose;
        }
    }
    CHECK(linguine == 1 && fettuccine == 1 && loose == 1);

    // Auto-layout: master is downstream of placed lfo -> deeper column; gamma/delta laid out too.
    const maiz::SceneNode* master = scene.find("master");
    CHECK(master && !master->placed);
    const maiz::SceneNode* gamma = scene.find("gamma");
    const maiz::SceneNode* delta = scene.find("delta");
    CHECK(gamma && delta && !gamma->placed && !delta->placed);
    if (gamma && delta) CHECK(!(gamma->x == delta->x && gamma->y == delta->y)); // no overlap

    // Hint-less node referenced by a fettuccine still has its principal (always present).
    // And synthesized aux anchors appear only where wires demanded them.
    if (gamma) CHECK(gamma->inputs.empty() && gamma->outputs.empty());

    // One-sync: dispatch then re-project — the picture follows the model, both ways.
    core.dispatch("unlink gamma delta --relation 0:0");
    CHECK(maiz::project_scene(core).wires.size() == 2);
    core.dispatch("undo");
    CHECK(maiz::project_scene(core).wires.size() == 3);

    // Chrome + subgraph projection: content.collapsed/size, hints.enter.
    core.register_glyph(
        R"({"glyph":"sub","label":"Sub","fields":["target"],"hints":{"enter":"target"}})");
    core.dispatch("rune new sub s");
    core.dispatch("set s target inner");
    core.dispatch("setjson lfo collapsed true");
    core.dispatch("setjson lfo size [220,140]");
    {
        maiz::Scene cs = maiz::project_scene(core);
        const maiz::SceneNode* sub = cs.find("s");
        CHECK(sub && sub->enter_mantle == "inner");
        const maiz::SceneNode* lfo2 = cs.find("lfo");
        CHECK(lfo2 && lfo2->collapsed);
        CHECK(lfo2 && lfo2->w == 220.0f && lfo2->h == 140.0f); // content.size wins over hints.face
    }

    // Blocks (okf/concepts/node-blocks.md): the "block" shape kind projects,
    // and a wire touching an adjacency-rendered port on either end carries
    // Style::Adjacency — a render style, not a new wire kind.
    core.register_glyph(
        R"({"glyph":"move","label":"move","fields":["steps"],)"
        R"("hints":{"shape":{"kind":"block"},"color":"#4c97ff",)"
        R"("ports":[{"name":"prev","dir":"in","type":"flow","render":"adjacency"},)"
        R"(          {"name":"next","dir":"out","type":"flow","render":"adjacency"},)"
        R"(          {"name":"steps","dir":"in","type":"number"}]}})");
    core.dispatch("rune new move m1");
    core.dispatch("rune new move m2");
    core.dispatch("link m1 m2 --relation 2:1"); // m1.next → m2.prev (hidden chain)
    core.dispatch("link lfo m1 --relation 2:3"); // an ordinary drawn wire into steps
    {
        maiz::Scene bs = maiz::project_scene(core);
        const maiz::SceneNode* m1 = bs.find("m1");
        CHECK(m1 && m1->shape == maiz::NodeShape::Block);
        CHECK(m1 && m1->inputs.size() == 2 && m1->inputs[0].name == "prev" &&
              m1->inputs[0].adjacency);
        CHECK(m1 && m1->inputs[1].name == "steps" && !m1->inputs[1].adjacency);
        int adjacency = 0, drawn_linguine = 0;
        for (const auto& w : bs.wires) {
            if (w.kind != maiz::SceneWire::Kind::Linguine) continue;
            if (w.style == maiz::SceneWire::Style::Adjacency)
                ++adjacency;
            else
                ++drawn_linguine;
        }
        CHECK(adjacency == 1);      // the chain wire hides
        CHECK(drawn_linguine == 2); // lfo→master and lfo→m1.steps stay drawn
    }
    core.dispatch("undo");
    core.dispatch("undo");
    core.dispatch("rm m1");
    core.dispatch("rm m2");

    // placement (the reserved model field) outranks content.pos when present.
    // (No `place` verb yet — simulate via a state round-trip is not possible from
    // dispatch, so this stays projection-level: covered by the pure-form test.)
    maiz::Scene pure = maiz::project_scene(
        R"({"mantles":[{"name":"m","runes":[)"
        R"({"spirit":{"id":"r1","name":"a"},"glyph":"g","tags":[],)"
        R"("content":{"pos":[5,5]},"placement":{"x":9,"y":9}}],)"
        R"("layout":{"edges":[]}}],"active":{"mantle":"m"}})",
        "[]");
    CHECK(pure.nodes.size() == 1);
    if (pure.nodes.size() == 1)
        CHECK(pure.nodes[0].placed && pure.nodes[0].x == 9.0f && pure.nodes[0].y == 9.0f);

    // -- A1: the edge weight makes the trip (Hormiga, 2026-08-17) ------------
    {
        maiz::Core c2;
        c2.register_glyph(R"({"glyph":"g","label":"G","fields":[]})");
        c2.dispatch("mantle new w");
        c2.dispatch("rune new g a");
        c2.dispatch("rune new g b");
        c2.dispatch("rune new g c");
        CHECK(c2.dispatch("link a b --relation supports --weight 0.61").ok);
        c2.dispatch("link b c --relation supports"); // no --weight: the default
        maiz::Scene s2 = maiz::project_scene(c2, {"w"});
        const maiz::SceneWire* ab = nullptr;
        const maiz::SceneWire* bc = nullptr;
        for (const auto& w : s2.wires) {
            if (w.from == "a" && w.to == "b") ab = &w;
            if (w.from == "b" && w.to == "c") bc = &w;
        }
        CHECK(ab != nullptr);
        CHECK(bc != nullptr);
        // The value the dispatcher accepted and `links` reports is the value
        // the projection carries -- the three layers agree now.
        if (ab) CHECK(ab->weight > 0.6 && ab->weight < 0.62);
        // Absent means "not stated", which is the core's 1.0, not 0.0.
        if (bc) CHECK(bc->weight == 1.0);
    }

    // -- A3.1: field_of, and its `null` rule ---------------------------------
    {
        maiz::Scene f = maiz::project_scene(
            R"({"mantles":[{"name":"m","runes":[)"
            R"({"spirit":{"id":"r1","name":"a"},"glyph":"g","tags":[],)"
            R"("content":{"text":"don't ñ","num":2.5,"empty":null,"arr":[1,2]}}],)"
            R"("layout":{"edges":[]}}],"active":{"mantle":"m"}})",
            R"([{"glyph":"g","label":"G","fields":["text","num","empty","arr","missing"]}])");
        CHECK(f.nodes.size() == 1);
        if (f.nodes.size() == 1) {
            const maiz::SceneNode& n = f.nodes[0];
            // A JSON string decodes to its content, quotes gone, \u resolved to
            // UTF-8 -- the part the hand-rolled four-liner got wrong.
            CHECK(maiz::field_of(n, "text") == "don't \xc3\xb1");
            // A number is its own text.
            CHECK(maiz::field_of(n, "num") == "2.5");
            CHECK(maiz::field_of(n, "arr") == "[1,2]");
            // `null` reads as "" -- the SAME answer as absent, deliberately, so
            // a host's "this field has a value" predicate agrees with itself.
            CHECK(maiz::field_of(n, "empty").empty());
            CHECK(maiz::field_of(n, "nosuchkey").empty());
            // A host that must tell them apart still can.
            CHECK(maiz::find_field(n, "empty") != nullptr);
            CHECK(maiz::find_field(n, "nosuchkey") == nullptr);
        }
    }

    // -- R4: walking a Scene and a Merged together, still uncoupled ----------
    {
        maiz::Scene d = maiz::project_scene(
            R"({"mantles":[{"name":"m","runes":[)"
            R"({"spirit":{"id":"r1","name":"a"},"glyph":"g","tags":[],"content":{}},)"
            R"({"spirit":{"id":"r2","name":"b"},"glyph":"g","tags":[],"content":{}},)"
            R"({"spirit":{"id":"r3","name":"c"},"glyph":"g","tags":[],"content":{}}],)"
            R"("layout":{"edges":[]}}],"active":{"mantle":"m"}})",
            R"([{"glyph":"g","label":"G","fields":[]}])");
        CHECK(d.nodes.size() == 3);

        maiz::ConstraintMap one, two;
        one.id = "script-one";   // distinct source ids: without them the two
        two.id = "script-two";   // maps are ONE source and nothing can conflict
        one.set("a", "color", "#f00", 1);
        one.set("c", "color", "#0f0", 1);
        two.set("c", "color", "#00f", 1); // distinct source, equal strength: TOP
        maiz::Merged m = maiz::merge({one, two});

        CHECK(maiz::decoration(m, d.nodes[0], "color") == "#f00");
        CHECK(maiz::decoration(m, d.nodes[1], "color").empty());  // no opinion
        CHECK(maiz::decoration(m, d.nodes[2], "color").empty());  // conflicted
        CHECK(maiz::decoration_cell(m, d.nodes[1], "color") == nullptr);
        const maiz::MergedCell* cc = maiz::decoration_cell(m, d.nodes[2], "color");
        CHECK(cc != nullptr && cc->conflicted);

        // The walk hands back SCENE order and skips both the absent and the
        // conflicted -- a renderer draws what is settled, and asks
        // merged.conflicts() for what is not.
        std::vector<std::string> visited;
        maiz::for_each_decoration(d, m, "color",
                                  [&](const maiz::SceneNode& n, const std::string& v) {
                                      visited.push_back(n.name + "=" + v);
                                  });
        CHECK(visited.size() == 1);
        if (visited.size() == 1) CHECK(visited[0] == "a=#f00");
    }

    // -- 0.2.14: rune kinds, values on edges, and the presentations split ----
    {
        maiz::Core c3;
        // DECLARED, not registered: the descriptors travel in the document, so
        // this is also the shape a host that registered nothing would see.
        CHECK(c3.dispatch("mantle new game").ok);
        // A descriptor is ONE argument, so it goes through the §6.1 quoter
        // (embed.hpp's arg) rather than being pasted into the command text --
        // it is full of the quotes the splitter reads.
        auto declare = [&](const std::string& descriptor) {
            return c3.dispatch("glyph declare " + maiz::arg(descriptor)).ok;
        };
        CHECK(declare(
            R"({"glyph":"stat","label":"Stat","kind":"measure","fields":[]})"));
        CHECK(declare(R"({"glyph":"hero","label":"Hero","fields":["hp"],)"
                      R"("kinds":{"hp":{"level":"ratio","unit":"hp","min":0,"max":100}},)"
                      R"("presentations":{"canvas":{"color":"#ff0000"}},)"
                      R"("hints":{"color":"#00ff00","face":{"w":50,"h":60}}})"));
        CHECK(declare(R"({"glyph":"flies","label":"Flies","kind":"act","fields":[]})"));
        c3.dispatch("rune new stat speed");
        c3.dispatch("measure speed --level ratio --unit m/s --min 0");
        c3.dispatch("rune new hero player");
        c3.dispatch("rune new flies flight");
        c3.dispatch("link player speed --weight 5");
        c3.dispatch("link player flight --relation does"); // an act target is NOT a value

        maiz::Scene g = maiz::project_scene(c3);
        const maiz::SceneNode* speed = g.find("speed");
        const maiz::SceneNode* player = g.find("player");
        const maiz::SceneNode* flight = g.find("flight");
        CHECK(speed && player && flight);

        // Kinds, off the descriptor. Default is entity, and `hero` never said.
        if (speed) CHECK(speed->kind == maiz::RuneKind::Measure);
        if (flight) CHECK(flight->kind == maiz::RuneKind::Act);
        if (player) CHECK(player->kind == maiz::RuneKind::Entity);

        // The measure rune's own quantity -- read from the RUNE, not the glyph.
        if (speed) {
            CHECK(speed->quantity.present);
            CHECK(speed->quantity.unit == "m/s");
            CHECK(speed->quantity.level == "ratio");
            CHECK(speed->quantity.has_min && speed->quantity.min == 0.0);
            CHECK(!speed->quantity.bounded()); // a min alone does not bound it
        }
        if (player) CHECK(!player->quantity.present);

        // The glyph's `kinds` map annotates a FIELD with the same shape.
        CHECK(player && player->fields.size() == 1);
        if (player && player->fields.size() == 1) {
            const maiz::Quantity& q = player->fields[0].quantity;
            CHECK(player->fields[0].key == "hp");
            CHECK(q.present && q.level == "ratio" && q.unit == "hp");
            CHECK(q.bounded() && q.min == 0.0 && q.max == 100.0);
        }

        // presentations.canvas outranks hints PER KEY: the color comes from
        // canvas, the face size from hints, and neither erases the other.
        if (player) {
            CHECK(player->has_color && player->rgb == 0xff0000u);
            CHECK(player->w == 50.0f && player->h == 60.0f);
        }

        // The value edge, and only it. Direction is normative: player->speed
        // asserts, and an edge to an act rune is exactly what it always was.
        const maiz::SceneWire* to_speed = nullptr;
        const maiz::SceneWire* to_flight = nullptr;
        for (const auto& w : g.wires) {
            if (w.to == "speed") to_speed = &w;
            if (w.to == "flight") to_flight = &w;
        }
        CHECK(to_speed && to_speed->is_value);
        CHECK(to_flight && !to_flight->is_value);
        if (to_speed) {
            CHECK(to_speed->weight == 5.0);
            CHECK(maiz::value_label(g, *to_speed) == "5 m/s"); // trimmed, with the unit
        }
        if (to_flight) CHECK(maiz::value_label(g, *to_flight).empty());

        // A document reopened by a host that registered NOTHING still projects
        // its fields and kinds -- which is the whole point of declaring.
        maiz::Core reopened(c3.export_state());
        maiz::Scene r = maiz::project_scene(reopened);
        const maiz::SceneNode* rp = r.find("player");
        CHECK(rp && rp->fields.size() == 1 && rp->kind == maiz::RuneKind::Entity);
        const maiz::SceneNode* rs = r.find("speed");
        CHECK(rs && rs->kind == maiz::RuneKind::Measure && rs->quantity.unit == "m/s");
    }

    if (failures == 0) {
        std::cout << "OK — projection smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
