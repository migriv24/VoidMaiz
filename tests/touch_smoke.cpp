/*
 * tests/touch_smoke.cpp — the touch recognizer, driven with literal contact
 * sets. No window, no device, no Android.
 *
 * This file is the argument for keeping the recognizer on the UI-free side. A
 * touch layer that lives in a shell can only be tested by putting a phone in
 * somebody's hand, which is why the InteractionCombinators APK's version was
 * never tested at all — and why the cases below (the deferred press, the
 * pinch-release that latches a stray tap, the release delivered nowhere) are
 * exactly the bugs a person holding a phone reports as "it feels wrong".
 */
#include "voidmaiz/touch.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace maiz;

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "  ok  " : "  FAIL", what.c_str());
    if (!ok) ++failures;
}

#define CHECK(cond) check((cond), #cond) // the expression is the description

static bool has(const TouchFrame& f, TouchGesture g) {
    for (const auto& e : f.events)
        if (e.kind == g) return true;
    return false;
}

static const TouchEvent* get(const TouchFrame& f, TouchGesture g) {
    for (const auto& e : f.events)
        if (e.kind == g) return &e;
    return nullptr;
}

/* A profile with dp=1 so one millimetre is 6.3 px and the numbers below are
 * readable: slop ≈ 12.6 px, the edge band (when on) ≈ 31 px. */
static TouchProfile test_profile() {
    TouchProfile p;
    p.dp = 1.0f;
    return p;
}

// ── the deferred press ───────────────────────────────────────────────────────

static void test_tap_is_not_a_drag() {
    TouchRecognizer r(test_profile());
    double t = 100.0;

    // finger down: the press is UNDECIDED, so the button must NOT go down yet.
    auto f = r.update({{1, 50, 50}}, t);
    check(f.pointer.present, "tap: pointer present while the press is latched");
    check(!f.pointer.left, "tap: button stays UP while the press is undecided");

    // lifted 80 ms later without moving
    t += 0.08;
    f = r.update({}, t);
    const TouchEvent* tap = get(f, TouchGesture::Tap);
    check(tap != nullptr, "tap: a quick lift reports Tap");
    check(tap && tap->taps == 1, "tap: first tap counts 1");
    check(f.pointer.left, "tap: the click is replayed, button DOWN on frame 1");
    check(f.pointer.x == 50 && f.pointer.y == 50, "tap: replayed at the press point");

    f = r.update({}, t + 0.016);
    check(f.pointer.present && !f.pointer.left, "tap: button UP on frame 2, still on screen");
    check(f.pointer.x == 50 && f.pointer.y == 50, "tap: released at the SAME point");

    f = r.update({}, t + 0.032);
    check(!f.pointer.present, "tap: pointer parks once the click is replayed");
}

static void test_double_tap_counts() {
    TouchRecognizer r(test_profile());
    double t = 10.0;
    r.update({{1, 80, 80}}, t);
    auto f = r.update({}, t += 0.05);
    check(get(f, TouchGesture::Tap)->taps == 1, "double tap: first is 1");

    r.update({{2, 82, 81}}, t += 0.1); // within double_tap_s and within slop
    f = r.update({}, t += 0.05);
    check(get(f, TouchGesture::Tap)->taps == 2, "double tap: second is 2");

    r.update({{3, 82, 81}}, t += 1.0); // too late
    f = r.update({}, t += 0.05);
    check(get(f, TouchGesture::Tap)->taps == 1, "double tap: the window expires");

    r.update({{4, 300, 300}}, t += 0.1); // in time but far away
    f = r.update({}, t += 0.05);
    check(get(f, TouchGesture::Tap)->taps == 1, "double tap: slop is enforced");
}

static void test_drag_presses_at_the_press_point() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 100, 100}}, t);

    // small move, still inside slop: nothing decided
    auto f = r.update({{1, 104, 100}}, t += 0.016);
    check(!has(f, TouchGesture::DragBegin), "drag: slop is not exceeded by 4 px");
    check(!f.pointer.left, "drag: button still up inside slop");

    // past slop
    f = r.update({{1, 130, 100}}, t += 0.016);
    check(has(f, TouchGesture::DragBegin), "drag: DragBegin past slop");
    check(f.pointer.left, "drag: button goes down");
    check(f.pointer.x == 100 && f.pointer.y == 100,
          "drag: the DOWN lands at the press point, not where the finger slid to");

    f = r.update({{1, 160, 110}}, t += 0.016);
    const TouchEvent* d = get(f, TouchGesture::Drag);
    check(d && d->dx == 30 && d->dy == 10, "drag: delta is this frame's movement");
    check(f.pointer.x == 160 && f.pointer.y == 110, "drag: the pointer then follows");

    f = r.update({}, t += 0.016);
    check(has(f, TouchGesture::DragEnd), "drag: DragEnd on lift");
    check(f.pointer.present && !f.pointer.left, "drag: released ON SCREEN, not at the park");
    check(f.pointer.x == 160 && f.pointer.y == 110, "drag: released at the last position");
}

static void test_long_press_is_a_right_click() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 200, 150}}, t);

    auto f = r.update({{1, 201, 150}}, t += 0.2); // held, barely moved
    check(!has(f, TouchGesture::LongPress), "long press: not yet at 200 ms");

    f = r.update({{1, 201, 150}}, t += 0.25); // past 400 ms
    check(has(f, TouchGesture::LongPress), "long press: fires past long_press_s");
    check(f.pointer.right, "long press: synthesizes the RIGHT button");
    check(!f.pointer.left, "long press: the left button never went down");
    check(f.pointer.x == 200 && f.pointer.y == 150, "long press: at the press point");

    f = r.update({{1, 201, 150}}, t += 0.016);
    check(f.pointer.present && !f.pointer.right,
          "long press: right released the next frame, still on screen");
    check(f.pointer.x == 200 && f.pointer.y == 150,
          "long press: released at the same point (the canvas slop test reads it)");

    // the finger is still down, but the gesture is spent: no drag, no tap
    f = r.update({{1, 260, 200}}, t += 0.016);
    check(!f.pointer.present, "long press: the pointer parks for the rest of the contact");
    check(f.events.empty(), "long press: sliding afterwards emits nothing");

    f = r.update({}, t += 0.016);
    check(f.events.empty(), "long press: the lift is not a tap");
}

static void test_long_press_can_be_turned_off() {
    TouchProfile p = test_profile();
    p.synthesize_right_click = false;
    TouchRecognizer r(p);
    double t = 0.0;
    r.update({{1, 10, 10}}, t);
    auto f = r.update({{1, 10, 10}}, t += 0.5);
    check(has(f, TouchGesture::LongPress), "long press: event still reported");
    check(!f.pointer.right, "long press: …but no synthetic click when opted out");
}

// ── two fingers ──────────────────────────────────────────────────────────────

static void test_second_finger_aborts_the_drag() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 100, 100}}, t);
    auto f = r.update({{1, 140, 100}}, t += 0.016);
    check(f.pointer.left, "abort: a drag is in flight");

    f = r.update({{1, 140, 100}, {2, 240, 100}}, t += 0.016);
    check(has(f, TouchGesture::DragEnd), "abort: the drag is closed out");
    check(!f.pointer.left, "abort: the button releases the instant the 2nd finger lands");
    check(f.pointer.x == 140 && f.pointer.y == 100, "abort: released at the last position");
}

static void test_pinch_and_pan() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 100, 100}, {2, 200, 100}}, t); // anchor frame: no events
    auto f = r.update({{1, 100, 100}, {2, 200, 100}}, t += 0.016);
    check(f.events.empty(), "two fingers: a still pair emits nothing");
    check(f.gesture_owns_input, "two fingers: the gesture owns the input");
    check(!f.pointer.present, "two fingers: the pointer is parked");

    // spread to 200 px apart around the same midpoint
    f = r.update({{1, 50, 100}, {2, 250, 100}}, t += 0.016);
    const TouchEvent* pinch = get(f, TouchGesture::Pinch);
    check(pinch && std::fabs(pinch->scale - 2.0f) < 1e-4f, "pinch: scale is the distance ratio");
    check(pinch && pinch->x == 150 && pinch->y == 100, "pinch: focus is the midpoint");
    check(!has(f, TouchGesture::Pan), "pinch: a pure spread is not a pan");

    // translate both by (20, 5)
    f = r.update({{1, 70, 105}, {2, 270, 105}}, t += 0.016);
    const TouchEvent* pan = get(f, TouchGesture::Pan);
    check(pan && pan->dx == 20 && pan->dy == 5, "pan: the midpoint delta");
    check(!has(f, TouchGesture::Pinch), "pan: a pure translation is not a pinch");
}

static void test_pinch_release_does_not_leave_a_stray_tap() {
    // The bug Parked exists to prevent: fingers leave one at a time, so the
    // instant the first lifts there is exactly one contact on the glass.
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 100, 100}, {2, 200, 100}}, t);
    r.update({{1, 90, 100}, {2, 210, 100}}, t += 0.016);

    auto f = r.update({{2, 210, 100}}, t += 0.016); // one finger lifts
    check(!f.pointer.present, "drain: one remaining finger is not a new press");
    check(!has(f, TouchGesture::Tap), "drain: and not a tap");

    f = r.update({{2, 260, 140}}, t += 0.016); // …and it slides before lifting
    check(!f.pointer.present, "drain: nor a drag");
    check(f.events.empty(), "drain: nothing is emitted while draining");

    f = r.update({}, t += 0.016);
    check(f.events.empty(), "drain: the final lift is silent");

    f = r.update({{3, 400, 400}}, t += 0.016);
    check(f.pointer.present, "drain: the NEXT press latches normally");
}

static void test_rotation() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 100, 100}, {2, 200, 100}}, t);
    // rotate the pair 90° about the midpoint: b moves from +x to -y (upward on
    // screen), which is counter-clockwise as a person sees it
    auto f = r.update({{1, 150, 150}, {2, 150, 50}}, t += 0.016);
    const TouchEvent* rot = get(f, TouchGesture::Rotate);
    check(rot != nullptr, "rotate: reported");
    check(rot && std::fabs(rot->rotation - 1.5707963f) < 1e-3f,
          "rotate: +90° CCW despite screen y growing downward");
}

static void test_degenerate_pinch_is_only_a_pan() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    // two contacts 10 px apart — under pinch_min (6 mm ≈ 37.8 px)
    r.update({{1, 100, 100}, {2, 110, 100}}, t);
    auto f = r.update({{1, 105, 100}, {2, 119, 100}}, t += 0.016);
    check(has(f, TouchGesture::Pan), "degenerate: still pans");
    check(!has(f, TouchGesture::Pinch), "degenerate: no scale from sensor noise");
    check(!has(f, TouchGesture::Rotate), "degenerate: no rotation from sensor noise");
}

// ── edges, flings, cancellation ──────────────────────────────────────────────

static void test_edge_swipe_is_opt_in() {
    TouchProfile p = test_profile();
    TouchRecognizer off(p);
    double t = 0.0;
    off.update({{1, 3, 300}}, t);
    auto f = off.update({{1, 60, 300}}, t += 0.016);
    check(!has(f, TouchGesture::EdgeSwipe), "edge: no band by default");
    check(f.pointer.left, "edge: so a press at x=3 is an ordinary drag");

    p.edge_mm = 5.0f; // ≈ 31 px
    TouchRecognizer on(p);
    on.set_viewport(1080, 1920);
    t = 0.0;
    on.update({{1, 3, 300}}, t);
    f = on.update({{1, 60, 300}}, t += 0.016);
    const TouchEvent* sw = get(f, TouchGesture::EdgeSwipe);
    check(sw && sw->edge == TouchEdge::Left, "edge: left band recognized when opted in");
    check(has(f, TouchGesture::DragBegin), "edge: the drag stream stays balanced");
    check(!f.pointer.present, "edge: the reserved band never reaches the pointer UI");

    t = 0.0;
    on.cancel();
    on.update({{1, 1077, 300}}, t);
    f = on.update({{1, 1000, 300}}, t += 0.016);
    sw = get(f, TouchGesture::EdgeSwipe);
    check(sw && sw->edge == TouchEdge::Right, "edge: right band needs the viewport");
}

static void test_fling() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 500, 500}}, t);
    for (int i = 1; i <= 6; ++i) // ~1900 px/s
        r.update({{1, 500 + 30.0f * i, 500}}, t += 0.016);
    auto f = r.update({}, t += 0.016);
    const TouchEvent* fl = get(f, TouchGesture::Fling);
    check(fl != nullptr, "fling: a fast release flings");
    check(fl && fl->vx > 1000.0f && std::fabs(fl->vy) < 1.0f, "fling: velocity is directional");

    TouchRecognizer slow(test_profile());
    t = 0.0;
    slow.update({{1, 500, 500}}, t);
    for (int i = 1; i <= 6; ++i)
        slow.update({{1, 500 + 3.0f * i, 500}}, t += 0.1); // ~30 px/s
    f = slow.update({}, t += 0.1);
    check(!has(f, TouchGesture::Fling), "fling: a slow release does not");
}

static void test_cancel_releases_everything() {
    TouchRecognizer r(test_profile());
    double t = 0.0;
    r.update({{1, 100, 100}}, t);
    auto f = r.update({{1, 200, 100}}, t += 0.016);
    check(f.pointer.left, "cancel: a drag is in flight");
    r.cancel();
    f = r.update({}, t += 0.016);
    check(!f.pointer.present && !f.pointer.left, "cancel: nothing is left half-committed");
    check(f.events.empty(), "cancel: and nothing is emitted");
}

// ── the camera: one gesture, the same command a wheel would compile ──────────

static void test_camera_pan_and_pinch() {
    Camera cam; // x=0, y=0, zoom=1
    check(camera_pan(cam, 20, 10), "camera: a pan moves it");
    check(cam.x == -20 && cam.y == -10, "camera: dragging right moves the world right");
    check(!camera_pan(cam, 0, 0), "camera: a still pan is not a change");

    cam = Camera{};
    // pinch about a focus 100 px into the canvas: the world point under the
    // fingers must not move
    float fx = 100, fy = 50, ox = 0, oy = 0;
    float wx_before = cam.x + (fx - ox) / cam.zoom;
    float wy_before = cam.y + (fy - oy) / cam.zoom;
    check(camera_pinch(cam, 2.0f, fx, fy, ox, oy, 0.2f, 3.0f), "camera: a pinch zooms");
    check(std::fabs(cam.zoom - 2.0f) < 1e-5f, "camera: zoom multiplies by the ratio");
    float wx_after = cam.x + (fx - ox) / cam.zoom;
    float wy_after = cam.y + (fy - oy) / cam.zoom;
    check(std::fabs(wx_before - wx_after) < 1e-3f && std::fabs(wy_before - wy_after) < 1e-3f,
          "camera: the world point under the fingers is anchored");

    check(!camera_pinch(cam, 10.0f, fx, fy, ox, oy, 0.2f, 2.0f),
          "camera: a pinch already at max_zoom is not a change");

    // and the event-shaped convenience routes each kind
    cam = Camera{};
    TouchEvent pan;
    pan.kind = TouchGesture::Pan;
    pan.dx = 8;
    pan.dy = 4;
    check(camera_apply(pan, cam, 0, 0) && cam.x == -8, "camera_apply: routes Pan");
    TouchEvent tapev;
    tapev.kind = TouchGesture::Tap;
    check(!camera_apply(tapev, cam, 0, 0), "camera_apply: ignores what is not a camera gesture");
}

static void test_profile_is_physical() {
    TouchProfile p;
    p.dp = 1.0f;
    float one_mm_ldpi = p.px(1.0f);
    p.dp = 3.5f; // a modern phone
    float one_mm_hdpi = p.px(1.0f);
    check(std::fabs(one_mm_hdpi / one_mm_ldpi - 3.5f) < 1e-4f,
          "profile: a millimetre costs more pixels on a denser screen");
    check(std::fabs(one_mm_ldpi - 6.2992f) < 1e-3f, "profile: 1 mm = 160/25.4 dp");
}

int main() {
    std::printf("touch_smoke\n");
    test_tap_is_not_a_drag();
    test_double_tap_counts();
    test_drag_presses_at_the_press_point();
    test_long_press_is_a_right_click();
    test_long_press_can_be_turned_off();
    test_second_finger_aborts_the_drag();
    test_pinch_and_pan();
    test_pinch_release_does_not_leave_a_stray_tap();
    test_rotation();
    test_degenerate_pinch_is_only_a_pan();
    test_edge_swipe_is_opt_in();
    test_fling();
    test_cancel_releases_everything();
    test_camera_pan_and_pinch();
    test_profile_is_physical();

    // ── screens, classified in dp (the 0.2.0 APK laid a 317-dp phone out like
    //    a desktop) ───────────────────────────────────────────────────────────
    {
        // a 1080x2400 phone at the Android shell's scale (420/160 * 1.3 = 3.41)
        LayoutClass p = classify_layout(1080, 2400, 3.41f, true);
        CHECK(p.form == FormFactor::Phone && p.orientation == Orientation::Portrait);
        CHECK(p.compact && p.narrow);
        CHECK(p.width_dp > 300 && p.width_dp < 330);
        // …rotated: still a phone (the short side decides), now landscape, not narrow
        LayoutClass l = classify_layout(2400, 1080, 3.41f, true);
        CHECK(l.form == FormFactor::Phone && l.orientation == Orientation::Landscape);
        CHECK(l.compact && !l.narrow);
        // a tablet
        LayoutClass t = classify_layout(1600, 2560, 2.0f, true);
        CHECK(t.form == FormFactor::Tablet && !t.compact);
        // a duo-bench half on a desktop: not touch, but narrow
        LayoutClass d = classify_layout(560, 1500, 1.0f, false);
        CHECK(d.form == FormFactor::Desktop && d.narrow);
        CHECK(classify_layout(100, 100, 0.0f, false).width_dp == 100); // bad scale = 1
    }

    // ── a row of actions: never clipped, the important ones stay ────────────
    {
        // step(0) undo(1) redo(2) reduce(3) add(4) clean(6), 60 px each, 8 px gaps
        std::vector<ActionSpec> items = {{60, 0}, {60, 3}, {60, 1}, {60, 2}, {60, 4}, {60, 6}};
        ActionPlan all = plan_action_bar(items, 1000, 40, 8);
        CHECK(all.visible.size() == 6 && all.overflow.empty());

        // 250 px: the overflow button (40) plus three items (3 * 68 = 204)
        ActionPlan some = plan_action_bar(items, 250, 40, 8);
        CHECK(some.visible == (std::vector<int>{0, 2, 3})); // priorities 0,1,2, original order
        CHECK(some.overflow == (std::vector<int>{1, 4, 5}));
        CHECK(some.visible.size() + some.overflow.size() == items.size());

        // pinned beats priority
        items[5].pinned = true;
        ActionPlan pinned = plan_action_bar(items, 250, 40, 8);
        CHECK(pinned.visible == (std::vector<int>{0, 2, 5}));

        // nothing fits beside the button: everything overflows, nothing is lost
        ActionPlan none = plan_action_bar(items, 50, 40, 8);
        CHECK(none.visible.empty() && none.overflow.size() == 6);

        // exactly fits without an overflow button: no button
        std::vector<ActionSpec> two = {{50, 0}, {50, 1}};
        ActionPlan exact = plan_action_bar(two, 108, 40, 8);
        CHECK(exact.visible.size() == 2 && exact.overflow.empty());
    }

    if (failures) {
        std::printf("touch_smoke: %d FAILED\n", failures);
        return 1;
    }
    std::printf("touch_smoke: all ok\n");
    return 0;
}
