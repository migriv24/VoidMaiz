/*
 * src/touch/touch.cpp — the touch recognizer (voidmaiz/touch.hpp).
 *
 * A five-state machine over the complete contact set:
 *
 *   Idle ──1 finger──→ Latched ──past slop──→ Dragging ──lift──→ Idle
 *                         │  └─lift early──→ (Tap) ──────────────→ Idle
 *                         │  └─held────────→ (LongPress) → Parked → Idle
 *                         └──2nd finger───→ Multi ──any lift──→ Parked → Idle
 *
 * Parked is the state IC's shell called "swallow until all up", and it exists
 * for one reason: a two-finger pinch ends with the fingers leaving one at a
 * time, and the instant the first leaves there is exactly one contact on the
 * glass — which, without Parked, latches as a fresh press and finishes as a
 * stray tap on whatever was under it.
 */
#include "voidmaiz/touch.hpp"

#include <algorithm>
#include <cmath>

namespace maiz {
namespace {

const TouchPoint* find(const TouchPoint* pts, int n, int id) {
    for (int i = 0; i < n; ++i)
        if (pts[i].id == id) return &pts[i];
    return nullptr;
}

float dist(float ax, float ay, float bx, float by) {
    return std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
}

} // namespace

std::string_view channel_of(TouchTool tool) {
    switch (tool) {
    case TouchTool::Stylus:
    case TouchTool::Eraser: return channel::pen;
    case TouchTool::Mouse: return channel::pointer;
    case TouchTool::Finger: break;
    }
    return channel::touch;
}

void TouchRecognizer::cancel() {
    phase_ = Phase::Idle;
    id0_ = ida_ = idb_ = -1;
    tap_stage_ = 0;
    longpress_click_ = false;
    vx_ = vy_ = 0;
    edge_ = TouchEdge::None;
    tap_count_ = 0;
    last_tap_t_ = -1e9;
}

TouchFrame TouchRecognizer::update(const TouchPoint* pts, int n, double now) {
    TouchFrame f;

    const float slop = prof_.px(prof_.tap_slop_mm);
    const float dbl_slop = prof_.px(prof_.double_tap_slop_mm);
    const float fling_min = prof_.px(prof_.fling_min_mm_s);
    const float pinch_min = prof_.px(prof_.pinch_min_mm);
    const float band = prof_.px(prof_.edge_mm);

    float dt = (float)(now - last_t_);
    last_t_ = now;
    if (!(dt > 0.0f) || dt > 0.25f) dt = 0.0f; // a stalled frame is not a fling

    /* Which edge band, if any, a press at (x,y) started in. Horizontal edges
     * win: a swipe in from the side is the navigation gesture people mean.
     * `edge_mm` defaults to 0 (no band), so a host opts into the reserved
     * strip rather than discovering that the leftmost node cannot be dragged. */
    auto edge_of = [&](float x, float y) {
        if (band <= 0.0f) return TouchEdge::None;
        if (x <= band) return TouchEdge::Left;
        if (vw_ > 0.0f && x >= vw_ - band) return TouchEdge::Right;
        if (y <= band) return TouchEdge::Top;
        if (vh_ > 0.0f && y >= vh_ - band) return TouchEdge::Bottom;
        return TouchEdge::None;
    };

    auto latch = [&](const TouchPoint& p) {
        id0_ = p.id;
        tool0_ = p.tool;
        px0_ = cx_ = p.x;
        py0_ = cy_ = p.y;
        press_t_ = now;
        edge_ = edge_of(p.x, p.y);
        vx_ = vy_ = 0;
        phase_ = Phase::Latched;
    };

    auto anchor_multi = [&](const TouchPoint& a, const TouchPoint& b) {
        tool0_ = a.tool;
        ida_ = a.id;
        idb_ = b.id;
        ax_ = a.x;
        ay_ = a.y;
        bx_ = b.x;
        by_ = b.y;
        phase_ = Phase::Multi;
    };

    auto track_velocity = [&](float dx, float dy) {
        if (dt <= 0.0f) return;
        // exponential smoothing: one jittery frame must not invent a fling
        vx_ = vx_ * 0.6f + (dx / dt) * 0.4f;
        vy_ = vy_ * 0.6f + (dy / dt) * 0.4f;
    };

    auto emit_fling_if_fast = [&](float x, float y, int fingers) {
        if (std::sqrt(vx_ * vx_ + vy_ * vy_) < fling_min) return;
        TouchEvent e;
        e.kind = TouchGesture::Fling;
        e.x = x;
        e.y = y;
        e.vx = vx_;
        e.vy = vy_;
        e.fingers = fingers;
        f.events.push_back(e);
    };

    // `drag_from_press` holds the synthetic pointer at the PRESS point for the
    // one frame the left button goes down, so the canvas hit-tests where the
    // finger landed rather than where it has already slid to.
    bool drag_from_press = false;

    /* A lift or an abort must release the button AT THE LAST POSITION, never at
     * the off-screen park: the canvas reads the pointer on the release frame
     * (its click-slop test and its hit test both do), so a release delivered
     * nowhere is a gesture that ends nowhere. */
    bool release_here = false;
    float rx = 0, ry = 0;

    switch (phase_) {
    case Phase::Idle:
        if (n == 1) latch(pts[0]);
        else if (n >= 2) anchor_multi(pts[0], pts[1]);
        break;

    case Phase::Latched: {
        if (n >= 2) { // the camera takes over before the press ever decided
            anchor_multi(pts[0], pts[1]);
            break;
        }
        const TouchPoint* p = (n == 1) ? find(pts, n, id0_) : nullptr;
        if (!p) { // lifted (or swapped ids) without ever passing slop: a TAP
            bool again = (now - last_tap_t_) <= prof_.double_tap_s &&
                         dist(last_tap_x_, last_tap_y_, px0_, py0_) <= dbl_slop;
            tap_count_ = again ? tap_count_ + 1 : 1;
            last_tap_t_ = now;
            last_tap_x_ = px0_;
            last_tap_y_ = py0_;

            TouchEvent e;
            e.kind = TouchGesture::Tap;
            e.x = px0_;
            e.y = py0_;
            e.taps = tap_count_;
            f.events.push_back(e);

            // the pointer replays it as a click over the next two frames
            tap_stage_ = 1;
            tap_x_ = px0_;
            tap_y_ = py0_;
            phase_ = Phase::Idle;
            break;
        }
        cx_ = p->x;
        cy_ = p->y;
        if (dist(px0_, py0_, cx_, cy_) > slop) {
            phase_ = Phase::Dragging;
            drag_from_press = true;
            TouchEvent e;
            e.kind = TouchGesture::DragBegin;
            e.x = px0_;
            e.y = py0_;
            f.events.push_back(e);
            if (edge_ != TouchEdge::None) {
                /* An edge swipe is reported ALONGSIDE the drag, not instead of
                 * it, so a host tracking DragBegin/Drag/DragEnd never sees an
                 * unbalanced stream. The pointer stays parked for the whole
                 * gesture (below): the band is reserved for navigation, which
                 * is the only reason a host would turn it on. */
                TouchEvent s;
                s.kind = TouchGesture::EdgeSwipe;
                s.x = cx_;
                s.y = cy_;
                s.dx = cx_ - px0_;
                s.dy = cy_ - py0_;
                s.edge = edge_;
                f.events.push_back(s);
            }
        } else if (now - press_t_ >= prof_.long_press_s) {
            TouchEvent e;
            e.kind = TouchGesture::LongPress;
            e.x = px0_;
            e.y = py0_;
            f.events.push_back(e);
            /* The synthetic right-CLICK is the whole trick: every context menu
             * the desktop canvas already has — node, wire, empty canvas, and
             * every host entry hung off ContextMenuFn — opens on glass with no
             * canvas change whatsoever. It rides two frames (down, then up at
             * the same point) because the canvas's own slop test reads the
             * pointer position on the RELEASE frame. */
            longpress_click_ = prof_.synthesize_right_click;
            tap_stage_ = longpress_click_ ? 1 : 0;
            tap_x_ = px0_;
            tap_y_ = py0_;
            phase_ = Phase::Parked;
        }
        break;
    }

    case Phase::Dragging: {
        if (n >= 2) { // a second finger aborts the drag, exactly as IC's shell did
            TouchEvent e;
            e.kind = TouchGesture::DragEnd;
            e.x = cx_;
            e.y = cy_;
            f.events.push_back(e);
            release_here = true;
            rx = cx_;
            ry = cy_;
            anchor_multi(pts[0], pts[1]);
            break;
        }
        const TouchPoint* p = (n == 1) ? find(pts, n, id0_) : nullptr;
        if (!p) {
            TouchEvent e;
            e.kind = TouchGesture::DragEnd;
            e.x = cx_;
            e.y = cy_;
            f.events.push_back(e);
            emit_fling_if_fast(cx_, cy_, 1);
            release_here = true;
            rx = cx_;
            ry = cy_;
            phase_ = Phase::Idle;
            break;
        }
        float dx = p->x - cx_, dy = p->y - cy_;
        cx_ = p->x;
        cy_ = p->y;
        track_velocity(dx, dy);
        if (dx != 0.0f || dy != 0.0f) {
            TouchEvent e;
            e.kind = TouchGesture::Drag;
            e.x = cx_;
            e.y = cy_;
            e.dx = dx;
            e.dy = dy;
            e.edge = edge_;
            f.events.push_back(e);
        }
        break;
    }

    case Phase::Multi: {
        if (n < 2) {
            // draining: one finger left on the glass is NOT a new press
            emit_fling_if_fast((ax_ + bx_) * 0.5f, (ay_ + by_) * 0.5f, 2);
            phase_ = (n == 0) ? Phase::Idle : Phase::Parked;
            break;
        }
        const TouchPoint* a = find(pts, n, ida_);
        const TouchPoint* b = find(pts, n, idb_);
        if (!a || !b) {
            // the pair changed (a third finger, a swap): re-anchor and emit
            // nothing, so the camera does not jump by the difference
            anchor_multi(pts[0], pts[1]);
            break;
        }
        float mx0 = (ax_ + bx_) * 0.5f, my0 = (ay_ + by_) * 0.5f;
        float mx1 = (a->x + b->x) * 0.5f, my1 = (a->y + b->y) * 0.5f;
        float d0 = dist(ax_, ay_, bx_, by_), d1 = dist(a->x, a->y, b->x, b->y);

        float dx = mx1 - mx0, dy = my1 - my0;
        track_velocity(dx, dy);
        if (dx != 0.0f || dy != 0.0f) {
            TouchEvent e;
            e.kind = TouchGesture::Pan;
            e.x = mx1;
            e.y = my1;
            e.dx = dx;
            e.dy = dy;
            e.fingers = 2;
            f.events.push_back(e);
        }
        /* The degenerate guard IC discovered by hand: two contacts a few pixels
         * apart produce a distance ratio that is mostly sensor noise, and an
         * angle that is entirely noise. Below the threshold the gesture is a
         * pan and nothing else. */
        if (d0 > pinch_min && d1 > pinch_min) {
            if (d1 != d0) {
                TouchEvent e;
                e.kind = TouchGesture::Pinch;
                e.x = mx1;
                e.y = my1;
                e.scale = d1 / d0;
                e.fingers = 2;
                f.events.push_back(e);
            }
            float a0 = std::atan2(by_ - ay_, bx_ - ax_);
            float a1 = std::atan2(b->y - a->y, b->x - a->x);
            float da = a1 - a0;
            while (da > 3.14159265f) da -= 6.28318531f;
            while (da < -3.14159265f) da += 6.28318531f;
            if (da != 0.0f) {
                TouchEvent e;
                e.kind = TouchGesture::Rotate;
                e.x = mx1;
                e.y = my1;
                e.rotation = -da; // screen y grows downward; report CCW positive
                e.fingers = 2;
                f.events.push_back(e);
            }
        }
        ax_ = a->x;
        ay_ = a->y;
        bx_ = b->x;
        by_ = b->y;
        break;
    }

    case Phase::Parked:
        if (n == 0 && tap_stage_ == 0) phase_ = Phase::Idle;
        break;
    }

    // ── the synthetic pointer ────────────────────────────────────────────────
    PointerOut& p = f.pointer;
    if (release_here) {
        p.present = true;
        p.x = rx;
        p.y = ry;
        f.gesture_owns_input = (phase_ == Phase::Multi);
        f.channel = channel_of(tool0_);
        return f;
    }
    switch (phase_) {
    case Phase::Latched:
        // present but not pressed: the finger hovers, so a node under it can
        // highlight while the press is still undecided
        p.present = true;
        p.x = px0_;
        p.y = py0_;
        break;
    case Phase::Dragging:
        if (edge_ != TouchEdge::None) break; // the band is navigation, not input
        p.present = true;
        p.left = true;
        p.x = drag_from_press ? px0_ : cx_;
        p.y = drag_from_press ? py0_ : cy_;
        break;
    case Phase::Idle:
    case Phase::Parked:
        /* The two-frame click replay, shared by the tap and the long press:
         * frame 1 presses at the recorded point, frame 2 releases at the SAME
         * point. Both frames keep the pointer present, because the canvas tests
         * hover and click-slop on the release frame. */
        if (tap_stage_ == 1) {
            p.present = true;
            p.x = tap_x_;
            p.y = tap_y_;
            p.left = !longpress_click_;
            p.right = longpress_click_;
            tap_stage_ = 2;
        } else if (tap_stage_ == 2) {
            p.present = true;
            p.x = tap_x_;
            p.y = tap_y_;
            tap_stage_ = 0;
            longpress_click_ = false;
        }
        break;
    case Phase::Multi:
        break; // parked: releasing the button is what aborts an in-flight drag
    }
    f.gesture_owns_input = (phase_ == Phase::Multi);
    /* Observed, not declared: empty when nothing is on the glass, so a host
     * writes a channel only when there was something to observe. */
    if (n > 0 || phase_ != Phase::Idle) f.channel = channel_of(tool0_);
    return f;
}

// ── camera application ───────────────────────────────────────────────────────

bool camera_pan(Camera& cam, float dx_px, float dy_px) {
    if (dx_px == 0.0f && dy_px == 0.0f) return false;
    cam.x -= dx_px / cam.zoom;
    cam.y -= dy_px / cam.zoom;
    return true;
}

bool camera_pinch(Camera& cam, float scale, float focus_x, float focus_y, float origin_x,
                  float origin_y, float min_zoom, float max_zoom) {
    if (!(scale > 0.0f) || scale == 1.0f) return false;
    // the world point under the fingers must not move: read it, rescale, put
    // it back — the same anchoring the wheel zoom uses, so the two agree
    float wx = cam.x + (focus_x - origin_x) / cam.zoom;
    float wy = cam.y + (focus_y - origin_y) / cam.zoom;
    float z = std::clamp(cam.zoom * scale, min_zoom, max_zoom);
    if (z == cam.zoom) return false;
    cam.zoom = z;
    cam.x = wx - (focus_x - origin_x) / cam.zoom;
    cam.y = wy - (focus_y - origin_y) / cam.zoom;
    return true;
}

bool camera_apply(const TouchEvent& e, Camera& cam, float origin_x, float origin_y,
                  float min_zoom, float max_zoom) {
    if (e.kind == TouchGesture::Pan) return camera_pan(cam, e.dx, e.dy);
    if (e.kind == TouchGesture::Pinch)
        return camera_pinch(cam, e.scale, e.x, e.y, origin_x, origin_y, min_zoom, max_zoom);
    return false;
}

// ── what kind of screen this is ─────────────────────────────────────────────

LayoutClass classify_layout(float width_px, float height_px, float scale, bool touch) {
    LayoutClass c;
    if (scale <= 0.0f) scale = 1.0f;
    c.width_dp = width_px / scale;
    c.height_dp = height_px / scale;
    float short_side = std::min(c.width_dp, c.height_dp);
    c.orientation = c.width_dp >= c.height_dp ? Orientation::Landscape : Orientation::Portrait;
    c.compact = short_side < 600.0f;
    c.narrow = c.width_dp < 600.0f;
    c.form = !touch ? FormFactor::Desktop : (c.compact ? FormFactor::Phone : FormFactor::Tablet);
    return c;
}

// ── fitting a row of actions ────────────────────────────────────────────────

ActionPlan plan_action_bar(const std::vector<ActionSpec>& items, float available,
                           float overflow_width, float spacing) {
    ActionPlan plan;
    const int n = (int)items.size();
    float all = 0.0f;
    for (int i = 0; i < n; ++i) all += items[i].width + (i ? spacing : 0.0f);
    if (all <= available) {
        for (int i = 0; i < n; ++i) plan.visible.push_back(i);
        return plan;
    }
    // something overflows: the overflow button takes its place first
    std::vector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        if (items[a].pinned != items[b].pinned) return items[a].pinned;
        return items[a].priority < items[b].priority;
    });
    float used = overflow_width;
    std::vector<bool> shown(n, false);
    for (int i : order) {
        float w = items[i].width + spacing; // every shown item sits before the button
        if (used + w <= available) {
            used += w;
            shown[i] = true;
        }
    }
    for (int i = 0; i < n; ++i) (shown[i] ? plan.visible : plan.overflow).push_back(i);
    return plan;
}

} // namespace maiz
