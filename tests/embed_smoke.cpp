/* embed_smoke.cpp — Phase 1 exit test.
 * Drives a real mantle through maiz::Core: glyph registration, attribution,
 * dispatch, links, tags, undo, log-sink capture — then exports the state
 * document, replays it into a fresh Core, and walks its nodes (the roadmap's
 * "replay a saved project state and walk its nodes", self-contained because no
 * saved VLS state exists on disk yet). */
#include "voidmaiz/embed.hpp"

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

static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

/* Compare dotted versions COMPONENT-WISE, not as strings.
 *
 * This used to be `ver.data >= R"("0.2.4")"`, which is a lexicographic compare
 * on text — and it went red the day Void Core shipped **0.2.10**, because "1"
 * sorts before "4". The check was wrong from the day it was written and could
 * only ever fire once a component reached double digits, which is exactly the
 * kind of latent test bug that gets diagnosed as "the dependency broke". */
static bool version_at_least(std::string v, const std::string& floor_) {
    auto parts = [](const std::string& s) {
        std::vector<long> out;
        std::string cur;
        for (char c : s) {
            if (c >= '0' && c <= '9') { cur += c; continue; }
            if (!cur.empty()) { out.push_back(std::stol(cur)); cur.clear(); }
        }
        if (!cur.empty()) out.push_back(std::stol(cur));
        return out;
    };
    const std::vector<long> a = parts(v), b = parts(floor_);
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const long x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x > y;
    }
    return true;
}

int main() {
    std::cout << "voidmaiz " << maiz::kVoidmaizVersion
              << " embedding Void Core " << maiz::Core::core_version() << "\n";

    maiz::Core core;

    // Build against 0.2.4+ (the release that adopted our reduce findings and
    // fixed the vc_version() drift we reported — the C string agrees with the
    // `version` verb now).
    maiz::Result ver = core.dispatch("version");
    CHECK(ver.ok);
    CHECK(version_at_least(ver.data, "0.2.4"));
    CHECK(contains(std::string(maiz::Core::core_version()),
                   ver.data.substr(1, ver.data.size() - 2)));

    // Log sink: every line the core logs lands here, live.
    std::vector<std::string> log;
    core.set_log_sink([&](std::string_view level, std::string_view op, std::string_view msg) {
        log.push_back(std::string(level) + " " + std::string(op) + ": " + std::string(msg));
    });

    // Attribution (SPEC §9), with the kind:name convention from our OKF.
    CHECK(core.dispatch("config set actor agent:voidmaiz-smoke").ok);

    // Glyphs are host config, registered up front.
    CHECK(core.register_glyph(R"({"glyph":"osc","label":"Oscillator","editor":"form","fields":["freq","wave"]})"));
    CHECK(core.register_glyph(R"({"glyph":"gain","label":"Gain","editor":"form","fields":["amount"]})"));
    CHECK(!core.register_glyph("not json"));

    // Build a small patch.
    CHECK(core.dispatch("mantle new smoke-patch").ok);
    CHECK(core.dispatch("rune new osc lfo").ok);
    CHECK(core.dispatch("rune new gain master").ok);
    CHECK(core.dispatch("set lfo freq 2.5").ok);
    CHECK(core.dispatch("tag lfo +modulation +rate:slow").ok);
    CHECK(core.dispatch("link lfo master --relation feeds").ok);
    CHECK(!core.dispatch("rune new bogus-glyph nope").ok); // unknown glyph rejected

    // The envelope carries machine data: links as JSON.
    maiz::Result links = core.dispatch("links lfo");
    CHECK(links.ok);
    CHECK(contains(links.data, "feeds"));

    // Undo pops the last mutation (the link), redo restores it.
    CHECK(core.dispatch("undo").ok);
    CHECK(!contains(core.dispatch("links lfo").data, "feeds"));
    CHECK(core.dispatch("redo").ok);
    CHECK(contains(core.dispatch("links lfo").data, "feeds"));

    // The mutation spine logged our commands; attribution rides `log` records
    // (the sink triple stays who-less by contract — upstream reply, 2026-07-09).
    CHECK(!log.empty());
    CHECK(contains(core.dispatch("log --tail 5").text(), "(agent:voidmaiz-smoke)"));

    // Stateless tag matching — the one filter grammar, callable at render time.
    CHECK(maiz::Core::tag_match("modulation AND rate:slow", {"modulation", "rate:slow", "lfo"}));
    CHECK(!maiz::Core::tag_match("modulation AND NOT lfo", {"modulation", "lfo"}));
    CHECK(!maiz::Core::tag_match("anything", {})); // empty bag: no match, no throw

    // ── The exit test proper: export → replay into a fresh Core → walk nodes ──
    std::string saved = core.export_state();
    CHECK(!saved.empty());

    maiz::Core replayed(saved);
    CHECK(replayed.dispatch("use smoke-patch").ok);
    maiz::Result ls = replayed.dispatch("ls");
    CHECK(ls.ok);
    std::string listing = ls.text();
    CHECK(contains(listing, "lfo"));
    CHECK(contains(listing, "master"));

    // Walk each rune: describe must succeed and the set field must survive.
    CHECK(contains(replayed.dispatch("get lfo freq").text(), "2.5"));
    CHECK(replayed.dispatch("describe lfo").ok);
    CHECK(replayed.dispatch("describe master").ok);
    CHECK(contains(replayed.dispatch("links lfo").data, "feeds"));

    // The actor is state — it survives the round-trip (session-scoped semantics).
    CHECK(contains(replayed.dispatch("config get actor").text(), "agent:voidmaiz-smoke"));

    // Move semantics: a Core is movable, callbacks stay wired.
    maiz::Core moved(std::move(replayed));
    CHECK(moved.dispatch("ls").ok);

    if (failures == 0) {
        std::cout << "OK — " << "embed smoke passed against Void Core "
                  << maiz::Core::core_version() << "\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
