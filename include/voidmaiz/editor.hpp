/*
 * voidmaiz/editor.hpp — view state: the camera and the editor's gesture state.
 *
 * The lasagna tiers, in one header (okf/concepts/total-observability.md):
 *  - selection & camera: view state, held here; the camera is flushed to the
 *    core's undo-exempt config tier on gesture end (logged + persisted, never
 *    popped by undo). Selection stays local pending the Q#3 ruling.
 *  - staged drag positions: ephemera — they exist only mid-gesture and flush
 *    as ONE model command on release (the VLS staging discipline).
 * Nothing in this struct is truth; it is all reconstructible or disposable.
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace maiz {

struct Camera {
    float x = 0, y = 0; // world coordinate at the canvas's top-left
    float zoom = 1.0f;
};

/* Staged drag positions: node name → world top-left, the shape a Move drag
 * stages in and the snap gesture reads (voidmaiz/gesture.hpp). */
using StagedMap = std::unordered_map<std::string, std::pair<float, float>>;

struct EditorState {
    Camera cam;
    std::vector<std::string> selection; // node names, model refs

    bool selected(std::string_view name) const {
        for (const auto& s : selection)
            if (s == name) return true;
        return false;
    }
    void toggle(const std::string& name) {
        for (auto it = selection.begin(); it != selection.end(); ++it)
            if (*it == name) { selection.erase(it); return; }
        selection.push_back(name);
    }

    // ── transient gesture machinery (ephemera; never logged, never persisted) ──
    enum class Drag { None, Pan, Move, Marquee, Wire, Resize };
    Drag drag = Drag::None;

    // subgraph navigation: mantles entered via double-click (view state; the
    // core's mantle list is flat — nesting is this editor's reading of it)
    std::vector<std::string> mantle_stack;

    // resize drag: staged size of the one node being resized
    std::string resize_node;
    float resize_w = 0, resize_h = 0;

    float press_sx = 0, press_sy = 0; // screen position at press (click-slop test)
    std::string press_node;           // node hit at press (click-collapse target)
    bool moved = false;               // exceeded slop during Move

    // staged world positions of the selection during a Move drag
    StagedMap staged;

    float marquee_x0 = 0, marquee_y0 = 0; // world, anchor
    float marquee_x1 = 0, marquee_y1 = 0; // world, current
    bool marquee_additive = false;

    // wire drag: the fixed end of the pending wire (port 0 = principal)
    std::string wire_node;
    int wire_port = 0;
    bool wire_is_output = false;
    std::string wire_type;
    // detach-grab: the existing wire being rewired (unlinked on release)
    bool wire_detach = false;
    SceneWire detached;

    // add-search box (Shift+A), also opened by a wire dropped on empty canvas
    bool add_open = false;
    float add_x = 0, add_y = 0; // world drop position
    char add_filter[64] = {};
    // quick add-and-link: the dangling wire's fixed end (picked node links to
    // it via its principal — always legal, principals are untyped)
    bool add_link = false;
    std::string add_link_node;
    int add_link_port = 0;
    bool add_link_is_output = false;
    std::string add_link_type;

    // context menu (right-click without dragging past click-slop)
    std::string ctx_node;         // target node; empty = wire or canvas
    bool ctx_is_wire = false;     // the click landed on a wire
    SceneWire ctx_wire;           // …that wire (a copy — the scene re-projects)
    float ctx_wx = 0, ctx_wy = 0; // world position of the click (adds land here)
    float pan_sx = 0, pan_sy = 0; // screen position at pan press (slop test)
    bool pan_right = false;       // this pan rode the right button
    bool pan_left = false;        // …or Space/Alt + left (laptop pan)

    // hover (view ephemera): tooltip dwell tracking
    std::string hover_node;
    double hover_since = 0.0;
    // …and the wire under the cursor this frame (set by edit_canvas's hover
    // pass; hosts read it for wire-targeted keys — e.g. Space fires the
    // hovered active pair)
    bool hover_wire_valid = false;
    SceneWire hover_wire;

    // inspector field editing: one field stages at a time (VLS #14b — edits
    // stage locally, flush as ONE command on commit; re-projection never
    // yanks the field mid-edit because the active buffer wins)
    std::string edit_node;
    std::string edit_field;
    char edit_buf[512] = {};
    char tag_add_buf[64] = {}; // the inspector's "+ tag" input

    bool cam_dirty = false;      // camera changed since last flush
    double last_zoom_time = 0.0; // for wheel-idle flushing
};

} // namespace maiz
