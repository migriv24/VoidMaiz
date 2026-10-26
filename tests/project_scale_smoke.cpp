/* project_scale_smoke.cpp — the projection's 2026-10-24 fast paths agree with
 * the slow ones (Void Hormiga issue #2).
 *
 *  - one mantle sliced out of the text projects exactly as a full parse does,
 *    for the active mantle and for one named explicitly;
 *  - edges resolve their endpoints by map, including a value edge to a measure;
 *  - the topological layout gives a chain the depths the relaxation gave, and a
 *    cycle still terminates;
 *  - Scene::find is right after a projection, after a push, and on a miss. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

static bool same(const maiz::Scene& a, const maiz::Scene& b) {
    if (a.mantle != b.mantle || a.nodes.size() != b.nodes.size() || a.wires.size() != b.wires.size())
        return false;
    for (size_t i = 0; i < a.nodes.size(); ++i)
        if (a.nodes[i].name != b.nodes[i].name || a.nodes[i].x != b.nodes[i].x || a.nodes[i].y != b.nodes[i].y)
            return false;
    for (size_t i = 0; i < a.wires.size(); ++i)
        if (a.wires[i].from != b.wires[i].from || a.wires[i].to != b.wires[i].to ||
            a.wires[i].weight != b.wires[i].weight || a.wires[i].is_value != b.wires[i].is_value)
            return false;
    return true;
}

int main() {
    maiz::Core core;
    core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    core.register_glyph(R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})");
    core.dispatch("mantle new big");
    for (int i = 0; i < 400; ++i) core.dispatch("rune new thing t" + std::to_string(i));
    for (int i = 0; i + 1 < 60; ++i) // a directed chain: depths 0..59
        core.dispatch("link t" + std::to_string(i) + " t" + std::to_string(i + 1) + " --relation next");
    core.dispatch("rune new metric margin");
    core.dispatch("measure margin --level ratio --unit %");
    for (int i = 100; i < 150; ++i)
        core.dispatch("link t" + std::to_string(i) + " margin --weight " + std::to_string(i));
    core.dispatch("mantle new small");
    core.dispatch("rune new thing a");
    core.dispatch("rune new thing b");
    core.dispatch("link a b --relation next");
    core.dispatch("link b a --relation next"); // a cycle
    core.dispatch("use big");

    const std::string state = core.export_state();
    const std::string glyphs = core.dispatch("glyphs").data;
    /* The same document with every mantle name spelled as JSON escapes: equal
     * JSON, but the scanner declines an escaped name (slice_mantle), so this
     * one is projected by the FULL parse. Fast and slow must agree. A leading
     * member with quotes and brackets inside strings exercises the skipper. */
    std::string padded = "{\"zz_pad\":[1,{\"x\":\"]}\\\"\"}]," + state.substr(1);
    for (const auto& [plain, escaped] : {std::pair<std::string, std::string>{"\"name\":\"big\"", "\"name\":\"b\\u0069g\""},
                                         {"\"name\":\"small\"", "\"name\":\"sm\\u0061ll\""}}) {
        const size_t at = padded.find(plain);
        CHECK(at != std::string::npos);
        if (at != std::string::npos) padded.replace(at, plain.size(), escaped);
    }

    for (const char* m : {"", "big", "small", "absent"}) {
        maiz::ProjectOptions o;
        o.mantle = m;
        const maiz::Scene fast = maiz::project_scene(state, glyphs, o);
        const maiz::Scene slow = maiz::project_scene(padded, glyphs, o);
        CHECK(same(fast, slow));
    }

    const maiz::Scene big = maiz::project_scene(core);
    CHECK(big.mantle == "big");
    CHECK(big.nodes.size() == 401);
    CHECK(big.wires.size() == 59 + 50);
    int values = 0;
    for (const auto& w : big.wires) values += w.is_value ? 1 : 0;
    CHECK(values == 50);
    CHECK(big.find("t59") && big.find("t59")->x > big.find("t0")->x); // depth 59 is further right
    CHECK(big.find("nope") == nullptr);

    maiz::ProjectOptions so;
    so.mantle = "small";
    const maiz::Scene small = maiz::project_scene(core, so); // a cycle terminates
    CHECK(small.nodes.size() == 2 && small.wires.size() == 2);
    // a cycle is one strongly connected component: both at its depth, not pushed to column N
    CHECK(small.nodes.size() == 2 && small.nodes[0].x == small.nodes[1].x && small.nodes[0].x < 1000);

    // revision: a listing leaves it, an edit moves it, and nothing reads stale
    {
        const auto r0 = core.revision();
        core.dispatch("glyphs");
        core.dispatch(" mantles ");
        core.dispatch("history");
        core.dispatch("glyphs thing");
        core.dispatch("config get view.camera");
        core.dispatch("ls");
        core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})"); // the same again
        CHECK(core.revision() == r0);
        core.register_glyph(R"({"glyph":"thing","label":"A thing","fields":["note"]})");
        CHECK(core.revision() != r0); // a changed descriptor is a change
        {
            const auto rc = core.revision();
            core.dispatch("config set view.camera 1");
            CHECK(core.revision() != rc); // the write half of `config` is a change
        }
        core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})"); // and back
        CHECK(core.replace_state(core.export_state())); // replays the latest descriptor per glyph
        CHECK(maiz::project_scene(core, {}).nodes.size() == 401);
        const auto s1 = core.state_text(), s2 = core.state_text();
        CHECK(s1 == s2 && *s1 == core.export_state()); // one export, shared
        maiz::ProjectOptions sm;
        sm.mantle = "small";
        const maiz::Scene before = maiz::project_scene(core, sm);
        CHECK(same(before, maiz::project_scene(core, sm))); // remembered: the same scene
        core.dispatch("use small");
        core.dispatch("rune new thing c");
        CHECK(core.revision() != r0);
        CHECK(maiz::project_scene(core, sm).nodes.size() == before.nodes.size() + 1);
        CHECK(*core.state_text() != *s1);
        core.dispatch("rune rm c");
        core.dispatch("use big");
        // the digest: a scene's source, without projecting it
        maiz::ProjectOptions bg;
        bg.mantle = "big";
        const maiz::Scene bs = maiz::project_scene(core, bg);
        CHECK(bs.source != 0 && bs.source == maiz::mantle_digest(core, bg));
        core.dispatch("use small");
        core.dispatch("rune new thing d"); // another mantle changes...
        CHECK(maiz::mantle_digest(core, bg) == bs.source); // ...and big's digest does not
        CHECK(maiz::mantle_digest(core, sm) != maiz::mantle_digest(core, bg));
        core.dispatch("rune rm d");
        core.dispatch("use big");
        core.dispatch("set t0 note hello"); // big itself changes
        CHECK(maiz::mantle_digest(core, bg) != bs.source);
        maiz::ProjectOptions absent;
        absent.mantle = "absent";
        CHECK(maiz::mantle_digest(core, absent) == 0);
        maiz::Core other; // another core's revisions are its own
        CHECK(other.revision() != core.revision());
        CHECK(maiz::project_scene(other, sm).nodes.empty());
    }

    maiz::Scene grown = big;
    maiz::SceneNode extra;
    extra.name = "late";
    grown.nodes.push_back(extra);
    CHECK(grown.find("late") != nullptr); // pushed after projection: still found
    CHECK(grown.find("t3") && grown.find("t3")->name == "t3");

    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "OK - projection fast paths agree with the slow ones\n";
    return failures ? 1 : 0;
}
