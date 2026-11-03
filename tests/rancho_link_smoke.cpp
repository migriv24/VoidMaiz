/* rancho_link_smoke.cpp — a maiz::Core driving a Void Rancho working set
 * through maiz::RanchoLink (voidmaiz/rancho.hpp), as Void Hormiga will.
 *
 * Built only with Void Rancho beside us (MAIZ_WITH_RANCHO, CMakeLists.txt).
 * What it pins, one block each:
 *  - the round trip: a store imported from the host's Core, loaded back into
 *    that SAME Core (no second Core), keeps the host's glyph registrations;
 *  - an edit through the working set lands in the store by the targeted path;
 *  - Rancho's sync reads (`cat`, `links`, bare `config`) do not move the
 *    host's revision, so the export the host already holds survives a sync —
 *    "one export per change" (embed.hpp) still holds with Rancho in the loop;
 *  - a change made BEHIND the working set's back (straight on the Core) is
 *    seen, through revision(), and caught by a full comparison;
 *  - clustering through the store runs Void Maiz's Leiden (voidmaiz_graph). */
#include "voidmaiz/rancho.hpp"

#include "rancho/plan.hpp"
#include "rancho/store.hpp"
#include "rancho/working_set.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

namespace fs = std::filesystem;

static bool has(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

int main() {
    const fs::path path = fs::temp_directory_path() / "maiz-rancho-link-smoke.store";
    fs::remove(path);
    {
        maiz::Core core;
        CHECK(core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})"));
        CHECK(core.dispatch("mantle new org").ok);
        std::vector<std::string> cmds;
        for (int g = 0; g < 4; ++g)
            for (int i = 0; i < 10; ++i) cmds.push_back("rune new thing r" + std::to_string(g * 10 + i));
        for (int g = 0; g < 4; ++g)
            for (int i = 0; i < 10; ++i)
                for (int k = 1; k < 4; ++k)
                    cmds.push_back("link r" + std::to_string(g * 10 + i) + " r" + std::to_string(g * 10 + (i + k) % 10));
        CHECK(core.dispatch(rancho::batch(cmds)).ok);

        rancho::Store store(path);
        {
            rancho::JsonStream in = rancho::JsonStream::from_text(*core.state_text());
            CHECK(store.import(in).ok);
        }

        // ── the round trip, into the host's own Core ─────────────────────────
        maiz::RanchoLink link(core);
        rancho::WorkingSet ws(store, link);
        CHECK(ws.load(rancho::Selection::all()));
        CHECK(core.dispatch("rune new thing fresh").ok); // the registration survived replace_state
        CHECK(core.dispatch("rune rm fresh").ok);

        // ── an edit through the working set, the targeted path ──────────────
        CHECK(ws.sync()); // the two edits above went behind its back: a full comparison
        const long long full_before = ws.stats.full;
        CHECK(ws.dispatch("tag r3 +status:called").ok);

        // ── a sync's reads leave the host's export in place ──────────────────
        const std::uint64_t rev = core.revision();
        const auto held = core.state_text();
        CHECK(ws.sync());
        CHECK(ws.stats.full == full_before); // targeted: it wrote what the command named
        CHECK(ws.stats.targeted >= 1);
        CHECK(core.revision() == rev);       // `cat`, `links`, `config` are reads
        CHECK(core.state_text().get() == held.get()); // the same export, not a second one
        CHECK(has(store.select("org", "status:called"), "r3"));

        // ── behind its back: seen by revision, caught by a full comparison ──
        CHECK(core.dispatch("tag r4 +status:called").ok);
        CHECK(ws.sync());
        CHECK(ws.stats.full == full_before + 1);
        CHECK(has(store.select("org", "status:called"), "r4"));

        // ── clustering: the store runs Leiden ───────────────────────────────
        // Graph analysis is Rancho's since 2026-11-02 (Q43): the settings name
        // rancho/graph.hpp after the move and voidmaiz/graph.hpp before it, so
        // the check is that it was Leiden, not whose header said so.
        const auto stats = store.cluster("org", 10);
        CHECK(stats.ok);
        CHECK(store.cluster_settings("org").find("leiden") != std::string::npos);
        CHECK(!store.cluster_of("org", "r0").empty());
        CHECK(store.cluster_of("org", "r0") == store.cluster_of("org", "r1")); // one planted group
        CHECK(store.cluster_of("org", "r0") != store.cluster_of("org", "r10"));
    }
    fs::remove(path);

    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "OK - rancho link: round trip, targeted sync, export kept, full catch, Leiden\n";
    return failures ? 1 : 0;
}
