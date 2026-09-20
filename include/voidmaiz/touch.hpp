/*
 * voidmaiz/touch.hpp — the touch recognizer (UI-free half).
 *
 * Touch is an input MODALITY, not a geometry (okf/concepts/substrates.md), so
 * this header sits beside gesture.hpp on the UI-free side: contacts in,
 * recognized gestures out, zero rendering types, testable against a live core
 * with no window. A two-finger pinch and a mouse wheel reach the SAME
 * `config set view.camera` command; that is the whole point.
 *
 * Why it exists at all (okf/concepts/touch.md): the InteractionCombinators APK
 * shipped 2026-07-14 with this logic hand-rolled in its own NativeActivity
 * shell — the two-finger camera, the pointer release when a second finger
 * lands, the degenerate-pinch guard, the swallow-until-all-up rule. Every
 * subsequent mobile host would have rewritten it, and would have rewritten the
 * subtle parts wrong. Void Hormiga is the second, so the logic moves here.
 *
 * ── the deferred press (the thing a naive shell gets wrong) ──────────────────
 * On glass a press is ambiguous for a few hundred milliseconds: it may become a
 * tap, a drag, or a long press. A shell that forwards the contact to the
 * pointer UI immediately has already committed to "drag" before it knows, so
 * every long press first starts a node move and every tap is a zero-distance
 * drag. The recognizer therefore LATCHES the press and delivers a synthetic
 * pointer only once the gesture is decided:
 *
 *   moved past slop  → left-down at the ORIGINAL press point, then follow
 *   lifted early     → left-down then left-up at the press point (a tap)
 *   held past the    → right-down for one frame at the press point (a long
 *   long-press time     press reads as a clean right-CLICK, so every existing
 *                       context menu — canvas, node, wire — works on glass
 *                       with no canvas changes at all), then the pointer parks
 *                       until the finger lifts
 *   second finger    → left released immediately (an in-flight drag aborts),
 *                       the camera takes over, and the pointer stays parked
 *                       until every finger is up
 *
 * Consequently `update()` must be called ONCE PER FRAME, not only when the
 * platform reports input: the timers and the synthetic-pointer queue run there.
 */
#pragma once

#include "voidmaiz/editor.hpp"    // Camera
#include "voidmaiz/usergraph.hpp" // channel:: — the attention graph's vocabulary

#include <string_view>
#include <vector>

namespace maiz {

/* WHAT drew the contact. Android reports it as `AMotionEvent_getToolType`, iOS
 * as `UITouch.type`; every touch platform knows, and until now nothing asked.
 *
 * It is here because of the ATTENTION GRAPH, not because the recognizer cares —
 * every tool behaves identically below. `UserGraph::touch` takes a `channel`
 * and Allomone's `device "pen"` reads it, and in every host that exists the
 * channel came from a dropdown the person set by hand. A self-reported channel
 * is not an observation, so `device` could only ever match a claim. This is
 * where the real answer enters the system. See okf/concepts/allomone/user-graph.md.
 *
 * Default `Finger`, deliberately: a recognizer consuming contacts is on a touch
 * surface by construction, so a shell that says nothing is a touch shell, and
 * that is a better default than pretending not to know. */
enum class TouchTool { Finger, Stylus, Eraser, Mouse };

/* The `maiz::channel::` spelling for a tool — the attention graph's vocabulary,
 * not a new one. An eraser is a pen held the other way up: same channel, and a
 * host that needs the distinction carries it in the affordance's `kind`, which
 * is where host vocabulary belongs. */
std::string_view channel_of(TouchTool tool);

/* One contact, as the shell reports it this frame. `id` must be stable for the
 * life of the contact (Android's pointer id, iOS's UITouch identity). */
struct TouchPoint {
    int id = 0;
    float x = 0, y = 0; // screen px, the same space the pointer UI uses
    TouchTool tool = TouchTool::Finger;
};

/* Which screen edge an EdgeSwipe started from. */
enum class TouchEdge { None, Left, Right, Top, Bottom };

enum class TouchGesture {
    Tap,        // a quick contact that never moved (taps: 1, 2, …)
    LongPress,  // held in place past long_press_s
    DragBegin,  // one finger passed the slop budget
    Drag,       // …and is moving (dx,dy = this frame's screen delta)
    DragEnd,
    EdgeSwipe,  // a drag that BEGAN inside the edge band (carries `edge`)
    Pan,        // two-finger translation (dx,dy)
    Pinch,      // two-finger scale (scale = this frame's ratio, x,y = midpoint)
    Rotate,     // two-finger twist (rotation = this frame's radians)
    Fling,      // a drag or pan released above the velocity threshold
};

struct TouchEvent {
    TouchGesture kind{};
    float x = 0, y = 0;    // focal point: the contact, or the two-finger midpoint
    float dx = 0, dy = 0;  // screen-px movement since the previous frame
    float scale = 1.0f;    // Pinch: ratio since the previous frame
    float rotation = 0.0f; // Rotate: radians since the previous frame (CCW+)
    float vx = 0, vy = 0;  // Fling: screen px/s at release
    TouchEdge edge = TouchEdge::None;
    int taps = 1; // Tap: 1, 2, … within double_tap_s
    int fingers = 1;
};

/* What the shell should hand the pointer UI this frame. Send it unconditionally
 * every frame — ImGui de-duplicates button events, so the shell is six lines:
 *
 *   auto f = touch.update(pts, n, now);
 *   ImGuiIO& io = ImGui::GetIO();
 *   if (f.pointer.present) io.AddMousePosEvent(f.pointer.x, f.pointer.y);
 *   else                   io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
 *   io.AddMouseButtonEvent(0, f.pointer.left);
 *   io.AddMouseButtonEvent(1, f.pointer.right);
 *
 * Position BEFORE buttons: a button event is read at the pointer's current
 * position, and a click delivered at a stale position hit-tests the wrong node.
 */
struct PointerOut {
    bool present = false; // false → park the pointer off-screen (no hover, no capture)
    float x = 0, y = 0;
    bool left = false;
    bool right = false; // synthesized by a long press (see the header comment)
};

struct TouchFrame {
    std::vector<TouchEvent> events;
    PointerOut pointer;
    /* True while a multi-finger gesture owns the input. A host uses it to
     * suppress its own per-frame pointer work (hover tooltips, dwell timers)
     * rather than to re-derive the policy. */
    bool gesture_owns_input = false;

    /* The channel the gesture in flight arrived through — `maiz::channel::touch`
     * or `::pen` or `::pointer` — for a host feeding the attention graph:
     *
     *     if (clicked_widget) ugraph.touch(id, "widget", frame.channel);
     *
     * Empty while nothing is being touched, so a host writes what was OBSERVED
     * and writes nothing when there is nothing to observe. The library still
     * feeds no graph itself: total-observability rules attention ephemera, and
     * materializing stays the host's choice (usergraph.hpp). */
    std::string_view channel;
};

/* ── the profile: physical units, because a finger is a physical object ───────
 * Every threshold here is in MILLIMETRES, not pixels. A 6 px slop is a
 * different gesture on a 160-dpi tablet and a 560-dpi phone, and a library that
 * ships pixel constants ships a different feel per device. The shell supplies
 * ONE number — `dp`, the platform's density scale (Android's
 * AConfiguration_getDensity()/160, iOS's contentScaleFactor) — and the
 * millimetre budgets convert against it.
 *
 * The defaults are the platform conventions: ~2 mm of slop, a 400 ms long
 * press, a 300 ms double-tap window (Android's ViewConfiguration, near enough
 * that a user cannot tell). */
struct TouchProfile {
    float dp = 1.0f;              // px per density-independent pixel (dpi / 160)
    float tap_slop_mm = 2.0f;     // movement budget before a press becomes a drag
    float long_press_s = 0.4f;
    float double_tap_s = 0.3f;
    float double_tap_slop_mm = 4.0f;
    float fling_min_mm_s = 40.0f; // release speed below this is not a fling
    float edge_mm = 0.0f;         // reserved edge band, 0 = off (see EdgeSwipe)
    float pinch_min_mm = 6.0f;    // contacts closer than this give no scale/rotation
    bool synthesize_right_click = true; // long press → a clean right-CLICK

    /* mm → px. 160 dp = 1 inch = 25.4 mm, so one millimetre is 160/25.4 dp. */
    float px(float mm) const { return mm * (160.0f / 25.4f) * dp; }
};

/* ── the recognizer ──────────────────────────────────────────────────────────
 * Pure view ephemera, like EditorState: nothing here is truth, and dropping it
 * loses at most a gesture in flight. Feed it the COMPLETE contact set each
 * frame (empty when nothing is touching) and it diffs internally — so it is
 * shell-agnostic, serving Android's pointer arrays and iOS's touch sets alike,
 * and a test can drive it with a literal array and no window. */
class TouchRecognizer {
  public:
    TouchRecognizer() = default;
    explicit TouchRecognizer(const TouchProfile& p) : prof_(p) {}

    TouchProfile& profile() { return prof_; }
    const TouchProfile& profile() const { return prof_; }

    /* The screen's size in px — only needed for the Right/Bottom edge bands.
     * Leave it unset and only the Left/Top edges are recognized. */
    void set_viewport(float w, float h) {
        vw_ = w;
        vh_ = h;
    }

    /* Once per frame. `now` is any monotonic clock in seconds (ImGui::GetTime()
     * on a GUI host, a steady_clock in a test). */
    TouchFrame update(const TouchPoint* points, int count, double now);
    TouchFrame update(const std::vector<TouchPoint>& points, double now) {
        return update(points.data(), (int)points.size(), now);
    }

    /* Abandon everything in flight (the app lost focus, the window resized, the
     * platform sent a CANCEL). The next frame starts clean and the pointer is
     * released, so no drag is left half-committed. */
    void cancel();

  private:
    enum class Phase { Idle, Latched, Dragging, Multi, Parked };

    TouchProfile prof_{};
    Phase phase_ = Phase::Idle;
    float vw_ = 0, vh_ = 0;

    // the latched single contact
    int id0_ = -1;
    TouchTool tool0_ = TouchTool::Finger;
    float px0_ = 0, py0_ = 0; // press point
    float cx_ = 0, cy_ = 0;   // current point
    double press_t_ = 0;
    TouchEdge edge_ = TouchEdge::None;

    // the two-finger gesture's previous geometry
    int ida_ = -1, idb_ = -1;
    float ax_ = 0, ay_ = 0, bx_ = 0, by_ = 0;

    // velocity (exponentially smoothed, px/s) for the fling verdict
    float vx_ = 0, vy_ = 0;
    double last_t_ = 0;

    // the synthetic-pointer queue: a tap must occupy two frames (down, then up)
    int tap_stage_ = 0;
    float tap_x_ = 0, tap_y_ = 0;

    // the long press holds the synthetic right button for exactly one frame
    bool longpress_click_ = false;

    // double-tap bookkeeping
    double last_tap_t_ = -1e9;
    float last_tap_x_ = 0, last_tap_y_ = 0;
    int tap_count_ = 0;
};

/* ── camera application (the shared half of every touch shell) ────────────────
 * The node canvas reads `cam.x/y` as the WORLD coordinate at the canvas
 * region's top-left and `zoom` as px per world unit; `origin_*` is that
 * region's top-left in the same screen space the contacts are reported in.
 * A geographic view reads the same three floats differently and applies its own
 * projection — which is why these are free functions a view opts into rather
 * than something the recognizer does to a Camera it was handed.
 *
 * Both return true when the camera actually moved (the host's cue to set
 * `cam_dirty` and, on gesture end, flush ONE `compile_camera` command — the
 * config tier: logged and persisted, never popped by undo). */
bool camera_pan(Camera& cam, float dx_px, float dy_px);

bool camera_pinch(Camera& cam, float scale, float focus_x, float focus_y, float origin_x,
                  float origin_y, float min_zoom = 0.2f, float max_zoom = 3.0f);

/* The convenience the two shells would otherwise both write: apply whichever of
 * Pan/Pinch this event is, and ignore the rest. */
bool camera_apply(const TouchEvent& e, Camera& cam, float origin_x, float origin_y,
                  float min_zoom = 0.2f, float max_zoom = 3.0f);

} // namespace maiz
