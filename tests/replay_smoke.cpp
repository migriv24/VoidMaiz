/* replay_smoke.cpp — the Phase 3 exit test.
 * Build a patch purely through compiled gesture commands (the exact strings
 * the mouse produces: add-box, drags, rewires, field edits, chrome, undo),
 * record the transcript, then REPLAY it into a fresh core: the projected
 * scene must be identical. "The transcript is the session" — the founding
 * commitment, automated. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/project.hpp"

#include <iostream>
#include <sstream>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

static void register_glyphs(maiz::Core& core) { // host config, per-session
    core.register_glyph(
        R"({"glyph":"osc","label":"Oscillator","fields":["freq","wave"],)"
        R"("hints":{"color":"#5a4fcf","ports":[{"name":"prin","principal":true},)"
        R"({"name":"freq","dir":"in","type":"float"},{"name":"out","dir":"out","type":"audio"}]}})");
    core.register_glyph(
        R"({"glyph":"out","label":"Output","fields":[],)"
        R"("hints":{"ports":[{"name":"prin","principal":true},{"name":"in","dir":"in","type":"audio"}]}})");
}

/* An id-independent fingerprint of what the user sees. */
static std::string fingerprint(const maiz::Scene& scene) {
    std::ostringstream out;
    out << "mantle:" << scene.mantle << "\n";
    // scene.nodes order is state-document order, which the transcript fixes
    for (const auto& n : scene.nodes) {
        out << "node:" << n.name << "|" << n.glyph << "|" << n.x << "," << n.y << "|"
            << n.w << "x" << n.h << "|c" << n.collapsed << "|";
        for (const auto& t : n.tags) out << "@" << t;
        out << "|";
        for (const auto& f : n.fields) out << f.key << "=" << f.value_json << ";";
        out << "\n";
    }
    for (const auto& w : scene.wires)
        out << "wire:" << w.from << ":" << w.from_port << "->" << w.to << ":" << w.to_port
            << "|" << (int)w.kind << "|" << w.directed << "\n";
    return out.str();
}

int main() {
    // ── the transcript: every line is a compiled gesture or typed command ───
    std::vector<std::string> transcript;
    auto say = [&](std::string cmd) { transcript.push_back(std::move(cmd)); };

    say("config set actor human:replay-test");
    say("mantle new patch");
    say(maiz::compile_add("osc", "osc-1", 80, 80));            // Shift+A
    say(maiz::compile_add("osc", "osc-2", 80, 260));
    say(maiz::compile_add("out", "speakers", 420, 160));
    say(maiz::compile_link({"osc-1", 2, true, "audio"}, {"speakers", 1, false, "audio"}));
    // rewire: osc-2 takes the input (drop on an occupied port)
    maiz::SceneWire occupied;
    occupied.from = "osc-1";
    occupied.to = "speakers";
    occupied.from_port = 2;
    occupied.to_port = 1;
    occupied.relation = "2:1";
    say(maiz::compile_rewire({occupied}, {"osc-2", 2, true, "audio"},
                           {"speakers", 1, false, "audio"}));
    say(maiz::compile_moves({{"osc-1", {100, 90}}, {"osc-2", {100, 300}}})); // group drag
    say(maiz::compile_set("osc-1", "freq", "440"));                          // inspector edit
    say(maiz::compile_set("osc-1", "wave", "saw"));
    say("tag osc-1 +voice +status:draft");                                 // command bar
    say(maiz::compile_collapse("osc-2", true));                              // chrome
    say(maiz::compile_resize("speakers", 200, 90));
    say(maiz::compile_link({"osc-1", 0, false, ""}, {"osc-2", 0, false, ""})); // fettuccine
    say("undo");                                                           // Ctrl+Z
    say("redo");                                                           // Ctrl+Y
    say(maiz::compile_deletes({"osc-1"}));                                   // Delete key
    say("undo"); // and take it back — undo/redo are part of the transcript

    // ── session A: live editing ──────────────────────────────────────────────
    maiz::Core a;
    register_glyphs(a);
    for (const auto& cmd : transcript) a.dispatch(cmd);
    std::string fp_a = fingerprint(maiz::project_scene(a));

    // ── session B: a fresh core replays the transcript ──────────────────────
    maiz::Core b;
    register_glyphs(b);
    for (const auto& cmd : transcript) b.dispatch(cmd);
    std::string fp_b = fingerprint(maiz::project_scene(b));

    CHECK(!fp_a.empty() && fp_a.find("node:") != std::string::npos);
    CHECK(fp_a.find("wire:") != std::string::npos); // non-trivial scene
    CHECK(fp_a == fp_b);
    if (fp_a != fp_b)
        std::cerr << "--- A ---\n" << fp_a << "--- B ---\n" << fp_b;

    // ── and once more from the exported state (replay-by-state also holds) ──
    maiz::Core c(a.export_state());
    register_glyphs(c);
    CHECK(fingerprint(maiz::project_scene(c)) == fp_a);

    if (failures == 0) {
        std::cout << "OK — transcript replay is exact (" << transcript.size()
                  << " commands)\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
