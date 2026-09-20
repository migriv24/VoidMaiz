/* presence_smoke.cpp — networking, the half an application shows. No socket,
 * no second machine, no Palabra.
 *
 * Each block pins one rule voidmaiz/presence.hpp says it ENFORCES rather than
 * recommends. That distinction is the point of the module: every one of these
 * rules was, before it existed, something each view of each application had to
 * remember — and "the Map had no presence" is what remembering looks like.
 */
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/presence.hpp"

#include <iostream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz;

static SceneNode node(const char* id, const char* name, std::vector<std::string> tags = {}) {
    SceneNode n;
    n.id = id;
    n.name = name;
    n.glyph = "card";
    n.tags = std::move(tags);
    return n;
}

/* Three runes. `diary` is private by the default convention. The ids look
 * nothing like the names on purpose: a test that passes only because id == name
 * proves nothing about rule 1. */
static Scene scene3() {
    Scene s;
    s.mantle = "m";
    s.nodes = {node("r-001", "budget"), node("r-002", "roadmap"),
               node("r-003", "diary", {std::string(net_tag::private_)})};
    return s;
}

static PresenceState peer(const char* id, const char* name, std::vector<std::string> sel,
                          std::vector<std::string> surfaces = {}, const char* focus = "") {
    PresenceState p;
    p.who.id = id;
    p.who.name = name;
    p.selection = std::move(sel);
    p.surfaces = std::move(surfaces);
    p.focus = focus;
    return p;
}

int main() {
    std::cout << "presence_smoke\n";
    const Scene scene = scene3();

    // ── surfaces are immediate mode: a closed view simply stops declaring ────
    {
        Surfaces sf;
        sf.begin_frame();
        sf.declare("canvas", "canvas", Mark::Outline);
        sf.show("canvas", "r-001");
        sf.show("canvas", "r-001"); // drawn twice, shown once
        sf.show("table", "r-001");
        sf.focus("table");
        CHECK(sf.list().size() == 2);
        CHECK(sf.find("canvas")->runes.size() == 1);
        CHECK(sf.focused() && sf.focused()->id == "table");
        CHECK(sf.surfaces_showing("r-001") == (std::vector<std::string>{"canvas", "table"}));

        sf.focus("canvas"); // at most one focus
        CHECK(sf.focused()->id == "canvas" && !sf.find("table")->focused);

        sf.begin_frame(); // the table window closed on this device
        sf.show("canvas", "r-002");
        CHECK(sf.find("table") == nullptr);
        CHECK(sf.surfaces_showing("r-001").empty());
    }

    // ── the one question: may this rune leave? ───────────────────────────────
    {
        ShareFilter f = share_by_tag();
        CHECK(shareable_runes(scene, f) == (std::vector<std::string>{"r-001", "r-002"}));
        CHECK(shareable_runes(scene, nullptr).size() == 3); // no filter = all
        CHECK(shareable_runes(scene, share_by_tag("draft")).size() == 3);
    }

    // ── rule 1: presence is keyed on the id; selection arrives by name ───────
    {
        CHECK(selection_ids(scene, {"roadmap", "nope"}) == (std::vector<std::string>{"r-002"}));
    }

    // ── rules 2 and 3: the SENDER decides, and private runes are not named ───
    {
        Surfaces sf;
        sf.begin_frame();
        sf.show("canvas", "r-001");
        sf.focus("canvas");
        Profile me{"dev-a", "Ana", 0x33aa66, ""};
        auto sel = selection_ids(scene, {"budget", "diary"});

        PresenceState all = compose_presence(me, sel, sf, {}, scene, share_by_tag());
        CHECK(all.selection == (std::vector<std::string>{"r-001"})); // diary never named
        CHECK(all.surfaces == (std::vector<std::string>{"canvas"}));
        CHECK(all.focus == "canvas");

        SharePolicy quiet;
        quiet.selection = false;
        quiet.surfaces = false;
        PresenceState none = compose_presence(me, sel, sf, quiet, scene, share_by_tag());
        CHECK(none.selection.empty() && none.surfaces.empty() && none.focus.empty());
        CHECK(none.who.id == "dev-a"); // still says who is here, only that

        // a selection id the sender's scene does not hold is not vouched for
        PresenceState ghost = compose_presence(me, {"r-999"}, sf, {}, scene, share_by_tag());
        CHECK(ghost.selection.empty());
    }

    // ── the codec round-trips, and refuses rather than truncates ─────────────
    {
        PresenceState a = peer("dev-b", "Bo \"the\" Builder", {"r-001", "r-002"}, {"map"}, "map");
        a.who.rgb = 0xc0ffee;
        std::string wire = presence_to_json(a);
        PresenceState b;
        std::string err;
        CHECK(presence_from_json(wire, b, &err));
        CHECK(b.who.id == "dev-b" && b.who.name == a.who.name && b.who.rgb == 0xc0ffee);
        CHECK(b.selection == a.selection && b.surfaces == a.surfaces && b.focus == "map");

        PresenceState untouched = peer("keep", "keep", {"x"});
        PresenceState probe = untouched;
        CHECK(!presence_from_json("not json", probe, &err));
        CHECK(!presence_from_json("[]", probe, &err));
        CHECK(!presence_from_json(R"({"selection":["a"]})", probe, &err)); // no `who`
        CHECK(!presence_from_json(R"({"who":{"name":"x"}})", probe, &err)); // no id
        CHECK(!presence_from_json(R"({"who":{"id":"x"},"selection":[1]})", probe, &err));
        CHECK(probe.who.id == "keep" && probe.selection == untouched.selection); // refused whole

        PresenceLimits tight;
        tight.max_ids = 2;
        PresenceState big = peer("dev-c", "C", {"a", "b", "c"});
        CHECK(!presence_from_json(presence_to_json(big), probe, &err, tight));
        tight = {};
        tight.max_bytes = 16;
        CHECK(!presence_from_json(wire, probe, &err, tight));
        tight = {};
        tight.max_str = 4;
        CHECK(!presence_from_json(wire, probe, &err, tight));
    }

    // ── the roster: upsert, self-echo, prune ─────────────────────────────────
    {
        Roster r;
        r.set_self("dev-a");
        CHECK(!r.update(peer("dev-a", "Ana", {"r-001"}), 1.0)); // our own broadcast came back
        CHECK(!r.update(peer("", "anon", {"r-001"}), 1.0));
        CHECK(r.update(peer("dev-b", "Bo", {"r-001"}, {"canvas"}), 1.0));
        CHECK(r.update(peer("dev-c", "Cy", {"r-001", "r-002"}, {}, "map"), 1.0));
        CHECK(r.peers().size() == 2);
        CHECK(r.on_rune("r-001").size() == 2);
        CHECK(r.on_rune("budget").empty()); // names are not keys
        CHECK(r.on_surface("canvas").size() == 1);
        CHECK(r.on_surface("map").size() == 1); // focus counts as open

        CHECK(r.update(peer("dev-b", "Bo", {"r-002"}), 5.0)); // upsert, not append
        CHECK(r.peers().size() == 2 && r.on_rune("r-001").size() == 1);

        CHECK(r.prune(12.0, 10.0) == 1); // dev-c last heard at 1.0
        CHECK(r.find("dev-c") == nullptr && r.find("dev-b") != nullptr);
        r.leave("dev-b");
        CHECK(r.peers().empty());
    }

    // ── Allomone: collaboration is a condition like any other ────────────────
    {
        Roster r;
        r.update(peer("dev-b", "Bo", {"r-002"}), 1.0);

        // subjects keyed on NAME, as both of our own Allomone examples key them
        std::vector<Subject> by_name = {{"budget", "card", "budget", "m", {}},
                                        {"roadmap", "card", "roadmap", "m", {}}};
        Script s = allo_parse("collab", "when present -> ring 1\n");
        CHECK(s.ok());

        PredicateRegistry no_scene;
        register_presence_predicates(no_scene, r);
        Merged blind = merge({allo_eval(s, by_name, &no_scene)});
        CHECK(blind.find("roadmap", "ring") == nullptr);
        // ↑ THE ID TRAP, pinned: without the scene a name-keyed subject never
        //   matches an id-keyed presence. It fails silently — which is why the
        //   scene parameter exists.

        PredicateRegistry with_scene;
        register_presence_predicates(with_scene, r, &scene);
        Merged m = merge({allo_eval(s, by_name, &with_scene)});
        CHECK(m.value("roadmap", "ring") == "1");
        CHECK(m.find("budget", "ring") == nullptr);

        // id-keyed subjects work either way
        std::vector<Subject> by_id = {{"r-002", "card", "roadmap", "m", {}}};
        Merged mid = merge({allo_eval(s, by_id, &no_scene)});
        CHECK(mid.value("r-002", "ring") == "1");

        // a named peer
        Script who = allo_parse("who", "when present \"Bo\" -> bo 1\nwhen present \"dev-x\" -> x 1\n");
        Merged mw = merge({allo_eval(who, by_name, &with_scene)});
        CHECK(mw.value("roadmap", "bo") == "1");
        CHECK(mw.find("roadmap", "x") == nullptr);
    }

    // ── Allomone answers "may this leave?", and a conflict keeps it home ──────
    {
        std::vector<Subject> subj = {{"r-001", "card", "budget", "m", {"draft"}},
                                     {"r-002", "card", "roadmap", "m", {}},
                                     {"r-003", "card", "diary", "m", {}}};
        Script a = allo_parse("a", "when tag \"draft\" -> share 0\n"
                                   "when name \"diary\" -> share 1\n");
        Script b = allo_parse("b", "when name \"diary\" -> share \"false\"\n");
        CHECK(a.ok() && b.ok()); // the filter is only as good as the rules that parse
        // …and the failure worth pinning: a rule that does not parse keeps
        // NOTHING private. The language has no bare booleans.
        CHECK(!allo_parse("bad", "when tag \"draft\" -> share false\n").ok());
        ShareFilter f = share_by_annotation(merge({allo_eval(a, subj), allo_eval(b, subj)}));
        // the Merged above is a temporary: the filter must have copied it

        CHECK(!f(scene.nodes[0]));  // `draft` → share false
        CHECK(f(scene.nodes[1]));   // no rule spoke
        CHECK(!f(scene.nodes[2]));  // two sources disagree → stays on this device
        CHECK(shareable_runes(scene, f) == (std::vector<std::string>{"r-002"}));
    }

    if (failures) {
        std::cerr << "presence_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "presence_smoke: all ok\n";
    return 0;
}
