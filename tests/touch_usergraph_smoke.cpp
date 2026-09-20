/* touch_usergraph_smoke.cpp — the whole chain, end to end.
 *
 * Three pieces that live in three places have to agree for a responsive script
 * to work on a phone:
 *
 *   maiz::TouchRecognizer   (voidmaiz, this repo, added 2026-09-13)
 *        ↓ the OBSERVED channel
 *   maiz::UserGraph         (voidmaiz, this repo — attention as a symmetric graph)
 *        ↓ register_user_graph_predicates
 *   allomone::eval          (../VoidAllomone, a SIBLING REPOSITORY since 2026-08-29)
 *        ↓
 *   `when device "pen" -> …` actually fires
 *
 * Nothing tested them together, and the middle link was broken in a way no
 * single-piece test could see: `UserGraph::touch` takes a channel, Allomone's
 * `device` predicate reads it, and in every host that existed the channel came
 * from a DROPDOWN a person set by hand. The predicate could only ever match a
 * claim about the input, never the input. `TouchTool` and `TouchFrame::channel`
 * are the fix; this file is the proof that the fix reaches all the way through
 * the sibling repo's evaluator.
 *
 * It also re-proves the seam itself on every run: it uses the `maiz::` spellings
 * (`maiz::Script`, `maiz::merge`, `maiz::PredicateRegistry`) rather than
 * `allomone::`, so it fails if the re-export ever stops re-exporting.
 */
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/touch.hpp"

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

/* Drive one complete tap and hand back the channel the recognizer OBSERVED.
 * This is a host's whole integration: the tap decides, and the host writes down
 * what actually happened rather than what a settings combo claimed. */
static std::string tap_channel(TouchTool tool) {
    TouchProfile p;
    p.dp = 1.0f;
    TouchRecognizer r(p);
    double t = 0.0;
    TouchPoint c{1, 50, 50, tool};
    TouchFrame f = r.update(&c, 1, t);
    std::string observed(f.channel);
    r.update(nullptr, 0, t + 0.05); // lift: the tap completes
    return observed;
}

static std::vector<Subject> subjects() {
    return {{"volume", "slider", "volume", "m", {}},
            {"mute", "button", "mute", "m", {}},
            {"preset", "control", "preset", "m", {}}};
}

int main() {
    std::cout << "touch_usergraph_smoke\n";

    // ── 1. the recognizer reports a channel the attention graph can store ────
    {
        CHECK(tap_channel(TouchTool::Finger) == std::string(channel::touch));
        CHECK(tap_channel(TouchTool::Stylus) == std::string(channel::pen));
        CHECK(tap_channel(TouchTool::Mouse) == std::string(channel::pointer));
        // an eraser is a pen held the other way up — same channel on purpose;
        // a host needing the distinction puts it in the affordance's `kind`
        CHECK(tap_channel(TouchTool::Eraser) == std::string(channel::pen));

        // and it says NOTHING when there is nothing to observe, so a host never
        // writes an invented channel into the graph
        TouchProfile p;
        TouchRecognizer idle(p);
        CHECK(idle.update(nullptr, 0, 1.0).channel.empty());
    }

    // ── 2. a stylus reaches a rule a finger does not ─────────────────────────
    {
        Script s = allo_parse("pen-only", "when device \"pen\" -> annotate 1\n");
        CHECK(s.ok());

        UserGraph g;
        g.touch("volume", "widget", tap_channel(TouchTool::Stylus));
        g.touch("mute", "widget", tap_channel(TouchTool::Finger));

        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);
        Merged m = merge({allo_eval(s, subjects(), &preds)});

        CHECK(m.value("volume", "annotate") == "1"); // reached with the pen
        CHECK(m.find("mute", "annotate") == nullptr);   // reached with a finger
        CHECK(m.find("preset", "annotate") == nullptr); // never reached at all
    }

    // ── 3. the same graph, asked the touch question ──────────────────────────
    {
        Script s = allo_parse("touch-only", "when device \"touch\" -> big 1\n");
        UserGraph g;
        g.touch("volume", "widget", tap_channel(TouchTool::Stylus));
        g.touch("mute", "widget", tap_channel(TouchTool::Finger));

        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);
        Merged m = merge({allo_eval(s, subjects(), &preds)});

        CHECK(m.find("volume", "big") == nullptr);
        CHECK(m.value("mute", "big") == "1");
    }

    // ── 4. a real two-finger gesture, framed as one piece of work ────────────
    // The coherence claim is what `with` reads, and a gesture is exactly the
    // unit that should produce one: what the hand did together, together.
    {
        TouchProfile p;
        p.dp = 1.0f;
        TouchRecognizer r(p);
        UserGraph g;
        double t = 0.0;

        g.open_frame();
        TouchPoint two[2] = {{1, 100, 100, TouchTool::Finger}, {2, 200, 100, TouchTool::Finger}};
        TouchFrame f = r.update(two, 2, t);
        g.touch("canvas", "pane", std::string(f.channel));

        two[0].x = 90;
        two[1].x = 210;
        f = r.update(two, 2, t += 0.016);
        CHECK(f.gesture_owns_input); // the camera owns it; no pointer went out
        CHECK(!f.channel.empty());
        g.touch("camera", "view", std::string(f.channel));
        g.close_frame();

        CHECK(g.coherence("canvas", "camera") > 0);
        CHECK(g.find("camera")->channel == std::string(channel::touch));

        Script s = allo_parse("responsive", "when with \"canvas\" -> glow 1\n");
        PredicateRegistry preds;
        register_user_graph_predicates(preds, g);
        std::vector<Subject> subj = {{"camera", "view", "camera", "m", {}},
                                     {"unrelated", "view", "unrelated", "m", {}}};
        Merged m = merge({allo_eval(s, subj, &preds)});
        CHECK(m.value("camera", "glow") == "1");
        CHECK(m.find("unrelated", "glow") == nullptr);
    }

    // ── 5. the pre-extraction call shape still honours the graph ─────────────
    // The compatibility overload was deliberately NOT a no-op adapter: dropping
    // the graph would silently turn every responsive rule into one that never
    // fires. Touch-sourced channels must survive that path too.
    {
        Script s = allo_parse("dev", "when device \"pen\" -> annotate 1\n");
        UserGraph g;
        g.touch("volume", "widget", tap_channel(TouchTool::Stylus));

        Merged m = merge({allo_eval(s, subjects(), &g, nullptr)});
        CHECK(m.value("volume", "annotate") == "1");

        // …and with no graph at all, silence rather than a false negative
        Merged none = merge({allo_eval(s, subjects(), nullptr, nullptr)});
        CHECK(none.find("volume", "annotate") == nullptr);
    }

    // ── 6. the ephemera rule: attention becomes model content only if asked ──
    {
        UserGraph g;
        g.open_frame();
        g.touch("volume", "widget", tap_channel(TouchTool::Stylus));
        g.touch("mute", "widget", tap_channel(TouchTool::Stylus));
        g.close_frame();

        std::vector<std::string> cmds = g.compile("attention");
        CHECK(!cmds.empty());
        bool has_channel = false, has_link = false;
        for (const auto& c : cmds) {
            if (c.find("pen") != std::string::npos) has_channel = true;
            if (c.rfind("link ", 0) == 0) has_link = true;
        }
        // the OBSERVED channel survives into the commands, so a materialized
        // graph is queryable by the same `device` question that matched live
        CHECK(has_channel);
        CHECK(has_link);
    }

    if (failures) {
        std::cerr << "touch_usergraph_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "touch_usergraph_smoke: all ok\n";
    return 0;
}
