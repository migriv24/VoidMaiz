/* host_predicates_smoke.cpp — `with` and `device` after they left the kernel.
 *
 * WHAT THIS FILE IS FOR. When Allomone was extracted (2026-08-29), these two
 * were the only predicates that read something outside a `Subject` — Void
 * Maiz's UserGraph — so they were demoted from kernel predicates to host
 * predicates that Void Maiz registers. The claim made at the time was that this
 * cost NO BEHAVIOUR, only a registration call.
 *
 * These are the tests that used to live in `allomone_smoke.cpp` and prove the
 * kernel versions worked. They are unchanged except for the one line that
 * registers them. If the demotion cost anything, this file is where it shows.
 */
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"

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

static std::vector<maiz::Subject> subjects() {
    return {{"volume", "slider", "volume", "m", {}},
            {"mute", "button", "mute", "m", {}},
            {"preset", "control", "preset", "m", {}}};
}

int main() {
    using namespace maiz;

    // ── `with` reads the WORK, not the data ─────────────────────────────────
    {
        Script s = allo_parse("responsive", "when with \"volume\" -> glow 1\n");
        CHECK(s.ok());

        // NOT REGISTERED: a script that asks about attention gets silence, not
        // an error — the same silence it used to get from a null UserGraph, and
        // now the same silence any unregistered predicate gives. One rule
        // instead of two, which is a simplification the demotion bought.
        CHECK(allo_eval(s, subjects()).cells.empty());

        UserGraph g;
        g.open_frame();
        g.touch("volume", "widget", std::string(channel::touch));
        g.touch("mute", "widget", std::string(channel::touch));
        g.close_frame();

        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);

        Merged m = merge({allo_eval(s, subjects(), &preds)});
        CHECK(m.value("mute", "glow") == "1");        // coherent with volume
        CHECK(m.find("preset", "glow") == nullptr);   // never co-occurred
        CHECK(m.find("volume", "glow") == nullptr);   // not coherent with itself
    }

    // ── `device` reads the channel a touch arrived through ──────────────────
    {
        Script s = allo_parse("dev", "when device \"touch\" -> big 1\n");
        UserGraph g;
        g.touch("volume", "widget", std::string(channel::touch));
        g.touch("preset", "widget", std::string(channel::pointer));

        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);

        Merged m = merge({allo_eval(s, subjects(), &preds)});
        CHECK(m.value("volume", "big") == "1");
        CHECK(m.find("preset", "big") == nullptr);
        CHECK(m.find("mute", "big") == nullptr); // never touched at all
    }

    // ── the channel is an OPEN string now, not a six-value enum ─────────────
    //
    // The generalization the author asked for (2026-08-29): attention does not
    // only arrive through a hand on a device. An agent's arrives through the
    // dispatcher, a harvest's through a pipeline stage — and `device "<x>"`
    // matches whatever the host actually said, with no library vocabulary in
    // the way.
    {
        Script s = allo_parse("agentic", "when device \"dispatcher\" -> traced 1\n");
        UserGraph g;
        g.touch("volume", "rune", "dispatcher");
        g.touch("mute", "rune", "harvest-stage-3");

        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);

        Merged m = merge({allo_eval(s, subjects(), &preds)});
        CHECK(m.value("volume", "traced") == "1");
        CHECK(m.find("mute", "traced") == nullptr);
    }

    // ── a host that does not say HOW must not be reported as a pointer ──────
    //
    // The old default was `Device::Pointer`, which meant a non-GUI host's every
    // affordance claimed to have been clicked. An unstated channel is now empty
    // and matchable as such, which is the honest answer and a usable one.
    {
        UserGraph g;
        g.touch("volume"); // no kind, no channel
        const Affordance* a = g.find("volume");
        CHECK(a != nullptr);
        if (a) {
            CHECK(a->channel.empty());
            CHECK(a->kind.empty());
            CHECK(a->touches == 1);
        }

        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);
        Script s = allo_parse("unstated", "when device \"\" -> plain 1\n");
        Merged m = merge({allo_eval(s, subjects(), &preds)});
        CHECK(m.value("volume", "plain") == "1");

        // A later touch that does not say how it arrived must not ERASE what an
        // earlier one knew.
        g.touch("volume", "widget", std::string(channel::pen));
        g.touch("volume");
        const Affordance* b = g.find("volume");
        CHECK(b && b->channel == channel::pen);
    }

    // ── they are ordinary host predicates now, and provably so ──────────────
    {
        CHECK(!PredicateRegistry::is_kernel_predicate("with"));
        CHECK(!PredicateRegistry::is_kernel_predicate("device"));

        // A host may replace them — which the kernel versions could never allow.
        // That is a capability the demotion ADDED: a host whose "attention"
        // means something else entirely can say so.
        PredicateRegistry preds;
        UserGraph g;
        register_user_graph_predicates(preds, g);
        CHECK(preds.find("with") != nullptr);
        CHECK(preds.names().size() == 2);

        // `check` reports them as known once registered, and as unknown before —
        // the diagnostic a script author actually reads.
        Script s = allo_parse("q", "when with \"x\" -> a 1\n");
        CHECK(allo_check(s, &preds).empty());
        PredicateRegistry empty;
        CHECK(allo_check(s, &empty).size() == 1);
    }

    // ── the legacy 3-argument registration still compiles ───────────────────
    //
    // Void Hormiga has twenty-two predicates taking the old
    // `(Subject, arg, const UserGraph*)` shape. The compatibility overload in
    // voidmaiz/allomone.hpp adapts them so their build does not break on our
    // refactor; this is the test that the adapter actually runs the function.
    {
        PredicateRegistry preds;
        preds.add("legacy", [](const Subject& sub, std::string_view arg,
                               const UserGraph* graph) {
            CHECK(graph == nullptr); // there is no graph to pass any more
            return sub.id == std::string(arg);
        });
        Script s = allo_parse("l", "when legacy \"mute\" -> hit 1\n");
        Merged m = merge({allo_eval(s, subjects(), &preds)});
        CHECK(m.value("mute", "hit") == "1");
        CHECK(m.find("volume", "hit") == nullptr);
    }

    if (failures == 0) {
        std::cout << "OK - host predicates smoke passed\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
