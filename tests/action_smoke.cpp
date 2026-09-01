/* action_smoke.cpp — the canvas-action registry (voidmaiz/action.hpp): a view's
 * interaction vocabulary as named, introspectable actions whose ONE host
 * compile serves both a gesture front-end and a CLI verb. Pins registration,
 * run() (the shared entry point), decline-on-missing-arg, and the introspection
 * manifest. UI-free — no core, no window. */
#include "voidmaiz/action.hpp"

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

static std::string arg(const maiz::ActionArgs& a, const char* k) {
    auto it = a.find(k);
    return it == a.end() ? std::string() : it->second;
}

int main() {
    maiz::ActionRegistry reg;

    // "place": mint a rune at a geo point — reads the scene to name it uniquely,
    // declines if either param is missing (compile owns the meaning).
    reg.add({"place",
             "Place",
             "Create a rune of GLYPH at geographic point AT.",
             {{"glyph", "glyph", true, "the kind to place"},
              {"at", "geo", true, "\"lat,lon\""}},
             "click",
             [](const maiz::Scene& scene, const maiz::ActionArgs& a) -> std::vector<std::string> {
                 std::string glyph = arg(a, "glyph"), at = arg(a, "at");
                 if (glyph.empty() || at.empty()) return {}; // decline
                 std::string name = glyph + "-" + std::to_string(scene.nodes.size() + 1);
                 return {"rune new " + glyph + " " + name, "set " + name + " geo '" + at + "'"};
             }});

    // "move": one command — retarget a rune's geo facet.
    reg.add({"move",
             "Move",
             "Move NODE to geographic point AT.",
             {{"node", "node", true, ""}, {"at", "geo", true, ""}},
             "drag",
             [](const maiz::Scene&, const maiz::ActionArgs& a) -> std::vector<std::string> {
                 std::string node = arg(a, "node"), at = arg(a, "at");
                 if (node.empty() || at.empty()) return {};
                 return {"set " + node + " geo '" + at + "'"};
             }});

    maiz::Scene scene; // empty: place names against size()+1 = 1

    // ── find ────────────────────────────────────────────────────────────────
    CHECK(reg.find("place") != nullptr);
    CHECK(reg.find("move") != nullptr);
    CHECK(reg.find("nonexistent") == nullptr);

    // ── run: the ONE entry point a gesture AND a CLI verb both call ───────────
    {
        std::vector<std::string> cmds =
            reg.run("place", scene, {{"glyph", "contact"}, {"at", "45.5,-122.6"}});
        CHECK(cmds.size() == 2);
        if (cmds.size() == 2) {
            CHECK(cmds[0] == "rune new contact contact-1"); // scene read: unique name
            CHECK(cmds[1] == "set contact-1 geo '45.5,-122.6'");
        }
    }
    {
        std::vector<std::string> cmds =
            reg.run("move", scene, {{"node", "contact-1"}, {"at", "46,-122"}});
        CHECK(cmds.size() == 1 && cmds[0] == "set contact-1 geo '46,-122'");
    }
    // unknown action → empty (no command invented)
    CHECK(reg.run("nope", scene, {{"x", "1"}}).empty());
    // missing required arg → compile declines → empty (nothing logged)
    CHECK(reg.run("place", scene, {{"glyph", "contact"}}).empty());

    // ── manifest: the introspection an agent reads to DISCOVER the vocabulary ─
    std::string m = reg.manifest();
    CHECK(m.front() == '[' && m.back() == ']');
    CHECK(m.find("\"name\":\"place\"") != std::string::npos);
    CHECK(m.find("\"name\":\"move\"") != std::string::npos);
    CHECK(m.find("\"gesture\":\"click\"") != std::string::npos);
    CHECK(m.find("\"type\":\"geo\"") != std::string::npos);
    CHECK(m.find("\"required\":true") != std::string::npos);
    CHECK(m.find("\"doc\":\"the kind to place\"") != std::string::npos);

    if (failures == 0) {
        std::cout << "OK — action smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
