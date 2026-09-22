/*
 * voidmaiz/canvas.hpp — the 2D canvas view: draw, and (Phase 3) interact.
 *
 * Two entry points:
 *  - draw_canvas: render-only projection of a Scene (Phase 2 surface).
 *  - edit_canvas: the interactive canvas — pan/zoom, click/shift/marquee
 *    selection, node dragging, delete key. It NEVER touches the model: every
 *    model-changing gesture compiles to command lines returned in CanvasIO,
 *    and the caller dispatches them and re-projects (the one-sync rule).
 *    Camera changes flush to the config tier as commands too, on gesture end.
 *
 * This is a view-module header: it deliberately uses ImGui types and is only
 * available to targets linking voidmaiz_view. The UI-free library never
 * includes it.
 */
#pragma once

#include "voidmaiz/editor.hpp"
#include "voidmaiz/gesture.hpp" // BlockMetrics (the snap/adjacency conventions)
#include "voidmaiz/presence.hpp" // CanvasNet (networking, the half a canvas shows)
#include "voidmaiz/scene.hpp"
#include "voidmaiz/wires.hpp" // WireWriter

#include "imgui.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace maiz {

/* All canvas colors in one place (ImU32, IM_COL32 packing). Glyph hint colors
 * (headers) come from the host and work on both themes; header text picks
 * black/white automatically by luminance. */
struct CanvasTheme {
    unsigned canvas_bg, grid;
    unsigned node_bg, node_border, node_border_sel, header_default;
    unsigned text_dim, tag_text;
    unsigned principal;             // fettuccine + principal diamonds
    unsigned linguine, loose;
    unsigned marquee_fill, marquee_line;
    unsigned wire_ok, wire_bad, wire_neutral; // pending-wire verdict tints
    unsigned chrome;                // collapse dot / resize grip
    unsigned hover;                 // hover accent: node outline, port ring, wire

    static CanvasTheme dark();
    static CanvasTheme light();
};

/* ── the port style registry (VLS ask §1–§2, 2026-07-16) ─────────────────────
 * Host-declared port TYPE → color + marker shape. Shape is a second channel
 * on purpose: color must never be the only signal (color-vision deficiencies,
 * dim rooms). Applied wherever the type shows — port markers on every body
 * kind, drawn linguine (the from-port's type; the to-port's when the from end
 * is an untyped principal), the pending-wire drag (until a candidate port's
 * verdict takes over), and the port hover ring. Undeclared types keep the
 * hashed-palette circle; a declared entry with color 0 keeps the hashed color
 * but takes the shape. */
enum class PortShape { Circle, Square, Diamond, Triangle, Ring };

struct PortStyle {
    unsigned color = 0; // IM_COL32-packed; 0 = keep the hashed palette color
    PortShape shape = PortShape::Circle;
};

struct CanvasStyle {
    CanvasTheme theme = CanvasTheme::dark();
    std::map<std::string, PortStyle> port_types; // type → {color, shape}
    float node_w = 170.0f;    // default face width (hints.face.w overrides)
    float header_h = 26.0f;
    float port_row = 20.0f;
    float port_radius = 4.5f;
    float rounding = 6.0f;
    float grid_step = 32.0f;
    float min_zoom = 0.2f;
    float max_zoom = 3.0f;
    float click_slop = 4.0f;         // px before a press becomes a drag
    float camera_flush_idle = 0.6f;  // s of wheel silence before the camera logs
    float port_hit_radius = 9.0f;    // px, screen-space minimum for port grabs
    bool hover_tooltips = true;      // touch shells disable: the pointer never
                                     // leaves, so a tap's tooltip would linger
    /* How a wire gesture is WRITTEN. Empty (the default): plain `link`/`unlink`
     * edges, exactly as always. A host that stores connections as wire runes
     * (voidmaiz/wires.hpp: concurrent rewires then commute) passes
     * reified_writer(...) and every wire gesture compiles through it. */
    WireWriter wires;
    /* Scopes the names this canvas mints for new nodes (see unique_name). A host
     * that shares its document MUST set it: two devices minting one name is a
     * wire that vanishes on the other screen. */
    std::string device_tag;
    /* Set by apply_touch_canvas. On glass a long press IS a right-click
     * (touch.hpp), and on empty canvas that opens the add palette where the
     * finger is — "press and hold to create a node", which the author asked for
     * (2026-09-22) and which every node-graph host on glass wants. */
    bool touch = false;
    BlockMetrics block;              // block-shape geometry + snap thresholds
                                     // (shared with the UI-free snap compiler)
};

/* What the add box (Shift+A) and the right-click add menu offer: the host's
 * registered glyphs. `category` groups entries (Hormiga ask §3.5 / §3, the
 * palette taxonomy); the convention is a `"category"` key in the glyph
 * descriptor's hints, copied here by the host — empty = ungrouped, listed
 * first. Grouping only appears once any entry carries a category. */
struct AddPalette {
    struct Entry {
        std::string glyph;
        std::string label;
        std::string category;
    };
    std::vector<Entry> entries;
};

struct CanvasIO {
    /* Compiled gesture commands, in order. Dispatch each, then re-project.
     * Model commands (moves, deletes) and view commands (config camera) both
     * arrive here — the caller treats them identically: dispatch. */
    std::vector<std::string> commands;
};

/* ── the animation layer (pure view ephemera) ────────────────────────────────
 * The model changes instantly and atomically (a rewrite is ONE batch; undo
 * un-rewrites it); CanvasFx is how the VIEW plays that change as motion.
 * Nothing here is truth: a host builds a fresh CanvasFx every frame from its
 * own animation clock and hands it to edit_canvas, which draws (and hit-tests)
 * the overridden geometry. Drop the struct and the picture snaps to the model.
 */
struct NodeFx {
    float cx = 0, cy = 0;  // world CENTER to draw the node at
    float scale = 1.0f;    // body scale; ≤0.02 skips drawing entirely
};

struct GhostFx {
    SceneNode node;        // a visual copy of a node the model no longer has
    float cx = 0, cy = 0;  // world center
    float scale = 1.0f;    // ≤0.02 skips drawing
};

struct CanvasFx {
    std::map<std::string, NodeFx> nodes; // by node name: transform overrides
    std::vector<GhostFx> ghosts;         // drawn above wires, below live nodes
};

/* ── networking on the canvas (okf/concepts/networking.md) ────────────────────
 * The canvas joins presence exactly as any other view does — by declaring what
 * it shows — except that it already knows its nodes, so the declaration and the
 * marks are done here and no host writes them. Pass one to edit_canvas and:
 *   - the canvas declares itself as surface `surface_id`, and every node it
 *     draws ON SCREEN as shown on it (by the node's id, never its name);
 *   - it becomes the focused surface while its window has focus;
 *   - peers who selected a node mark it in their colour (`mark`, via the one
 *     renderer in netview.cpp), and nodes the share filter keeps local carry a
 *     padlock.
 * Leave it null and the canvas behaves exactly as before: an application that
 * never networks pays nothing and sees nothing. */
struct CanvasNet {
    Surfaces* surfaces = nullptr;   // required: this frame's declarations
    const Roster* roster = nullptr; // null = declare only, draw no marks
    PresenceDisplay display;        // the RECEIVER's switches
    ShareFilter shareable;          // empty = no padlocks
    std::string surface_id = "canvas";
    Mark mark = Mark::Outline;
};

/* Render-only (no interaction). Call between Begin()/End(). */
void draw_canvas(const char* str_id, const Scene& scene, const Camera& cam,
                 const CanvasStyle& style = {});

struct FaceRegistry; // voidmaiz/face.hpp

/* Host-supplied extra entries for the right-click context menu: called inside
 * the open popup with the target — a node, OR a wire (right-clicking a wire
 * targets the connection itself: an active fettuccine's host entry can fire
 * exactly that interaction), or neither (the canvas). Draw
 * ImGui::MenuItem(...)s and push compiled commands into io. Host entries render
 * ABOVE the built-ins (node: collapse/expand, delete; wire: unlink; canvas:
 * add-node). */
using ContextMenuFn = std::function<void(const SceneNode*, const SceneWire*, CanvasIO&)>;

/* The interactive canvas. Reads and writes `ed` (camera, selection, drag
 * ephemera); returns the commands this frame's gestures compiled to. Pass a
 * palette to enable the Shift+A add box (and the right-click add menu), a face
 * registry to draw per-glyph faces inside node bodies, a context_menu to
 * extend the right-click menu (right-drag still pans; the menu opens only on a
 * clean right-CLICK, within click-slop), and an fx to draw this frame's
 * animation overrides. */
CanvasIO edit_canvas(const char* str_id, const Scene& scene, EditorState& ed,
                     const CanvasStyle& style = {}, const AddPalette* palette = nullptr,
                     const FaceRegistry* faces = nullptr,
                     const ContextMenuFn& context_menu = {},
                     const CanvasFx* fx = nullptr, const CanvasNet* net = nullptr);

} // namespace maiz
