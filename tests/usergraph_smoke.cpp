/* usergraph_smoke.cpp — the user action graph (voidmaiz/usergraph.hpp).
 *
 * The point of this structure is what it REFUSES to know: no order, no
 * direction, no time. So the checks that matter are the symmetry ones — a
 * session worked backwards produces the same graph — plus the census-pattern
 * property that ephemera only become model content when a host asks. UI-free. */
#include "voidmaiz/usergraph.hpp"

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

static std::string form(const maiz::UserGraph& g) {
    std::string s;
    for (const auto& c : g.compile("m")) s += c + "\n";
    return s;
}

int main() {
    using namespace maiz;

    // ── a frame is a SET: working it backwards changes nothing ──────────────
    {
        UserGraph a, b;
        a.open_frame();
        a.touch("volume", "widget", std::string(channel::touch));
        a.touch("mute", "widget", std::string(channel::touch));
        a.touch("preset", "widget", std::string(channel::touch));
        a.close_frame();

        b.open_frame();
        b.touch("preset", "widget", std::string(channel::touch));
        b.touch("mute", "widget", std::string(channel::touch));
        b.touch("volume", "widget", std::string(channel::touch));
        b.close_frame();

        CHECK(form(a) == form(b)); // no sequence survives into the structure
    }

    UserGraph g;
    g.open_frame();
    g.touch("volume", "widget", std::string(channel::touch));
    g.touch("mute", "widget", std::string(channel::touch));
    g.close_frame();

    g.open_frame();
    g.touch("volume", "widget", std::string(channel::touch));
    g.touch("mute", "widget", std::string(channel::touch));
    g.close_frame();

    g.open_frame();
    g.touch("volume", "widget", std::string(channel::touch));
    g.touch("preset", "widget", std::string(channel::pointer));
    g.close_frame();

    // ── edges are symmetric and weighted ────────────────────────────────────
    CHECK(g.coherence("volume", "mute") == 2);
    CHECK(g.coherence("mute", "volume") == 2); // the query has no direction
    CHECK(g.coherence("volume", "preset") == 1);
    CHECK(g.coherence("mute", "preset") == 0); // never shared a frame
    CHECK(g.coherence("volume", "volume") == 0);
    CHECK(g.coherence("volume", "nothing") == 0);

    // ── coherent_with: strongest first, then determinate ────────────────────
    {
        std::vector<std::string> c = g.coherent_with("volume");
        CHECK(c.size() == 2);
        if (c.size() == 2) {
            CHECK(c[0] == "mute");   // weight 2
            CHECK(c[1] == "preset"); // weight 1
        }
        CHECK(g.coherent_with("volume", 2).size() == 1);
        CHECK(g.coherent_with("nothing").empty());
    }

    // ── affordances count touches and carry their modality ──────────────────
    {
        const Affordance* v = g.find("volume");
        CHECK(v != nullptr);
        if (v) {
            CHECK(v->touches == 3);
            CHECK(v->kind == "widget");
            CHECK(v->channel == std::string(channel::touch));
        }
        const Affordance* p = g.find("preset");
        CHECK(p && p->channel == std::string(channel::pointer)); // a mobile host stays honest
        CHECK(g.find("nothing") == nullptr);
    }

    // ── the hypergraph half: a triple is not three pairs ────────────────────
    {
        UserGraph h;
        h.open_frame();
        h.touch("a"); h.touch("b"); h.touch("c");
        h.close_frame();
        h.open_frame();
        h.touch("c"); h.touch("b"); h.touch("a"); // same SET, recorded again
        h.close_frame();

        CHECK(h.frames().size() == 1);      // one hyperedge...
        CHECK(h.frames()[0].weight == 2);   // ...that recurred
        CHECK(h.frames()[0].members.size() == 3);
        CHECK(h.frames()[0].members[0] == "a"); // canonical order
        CHECK(h.coincidences().size() == 3);    // and its pairwise projection
    }

    // ── a frame of one co-occurs with nothing, but still counts ─────────────
    {
        UserGraph h;
        h.open_frame();
        h.touch("solo");
        h.close_frame();
        CHECK(h.coincidences().empty());
        CHECK(h.frames().empty());
        CHECK(h.find("solo") && h.find("solo")->touches == 1);
        // A touch outside any frame counts too, and joins no set.
        h.touch("loose");
        CHECK(h.find("loose") && h.find("loose")->touches == 1);
        CHECK(h.coincidences().empty());
    }

    // ── duplicate touches inside one frame don't inflate the edge ───────────
    {
        UserGraph h;
        h.open_frame();
        h.touch("a"); h.touch("b"); h.touch("a"); h.touch("a");
        h.close_frame();
        CHECK(h.coherence("a", "b") == 1);
        CHECK(h.find("a")->touches == 3); // ...but the touches are real
    }

    // ── compile(): ephemera become model content only when asked ────────────
    {
        std::vector<std::string> cmds = g.compile("userplay");
        CHECK(!cmds.empty());
        CHECK(cmds.front() == "use userplay");

        std::string all;
        for (const auto& c : cmds) all += c + "\n";
        CHECK(all.find("rune new allomone-affordance volume") != std::string::npos);
        CHECK(all.find("set volume device \"touch\"") != std::string::npos);
        CHECK(all.find("setjson volume touches 3") != std::string::npos);
        // Undirected, because the claim genuinely has no direction.
        CHECK(all.find("link mute volume --relation coherence:2 --undirected") !=
              std::string::npos);

        UserGraph empty;
        CHECK(empty.compile("userplay").empty()); // nothing touched, nothing said
        CHECK(empty.empty());
    }

    /* ── the channel is its own name ─────────────────────────────────────────
     *
     * There is nothing left to round-trip. `device_name`/`device_from_name`
     * converted between a closed six-value enum and its spelling; since
     * 2026-08-29 the spelling IS the value, so the conversion is the identity
     * and an unknown channel is simply that channel rather than being coerced
     * to `pointer`. That coercion was the bug the generalization removed: a
     * host whose attention arrives through a dispatcher was reported as having
     * used a mouse. */
    CHECK(std::string(channel::touch) == "touch");
    {
        UserGraph h;
        h.touch("x", "rune", "dispatcher");
        const Affordance* a = h.find("x");
        CHECK(a && a->channel == "dispatcher"); // NOT silently "pointer"
    }

    // ── clear() puts it back to nothing ─────────────────────────────────────
    g.clear();
    CHECK(g.empty() && g.coincidences().empty() && g.frames().empty());

    if (failures == 0) {
        std::cout << "OK — usergraph smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
