/*
 * voidmaiz/widgets.hpp — the observability widgets: the log strip and the
 * command bar (the CLI inside the UI).
 *
 * Total observability's showpiece: everything the mouse does appears in the
 * log strip, and anything the log strip shows can be typed back into the
 * command bar. Humans, scripts, and agents share the one surface — this is
 * where the UI proves it.
 *
 * View-module header (ImGui types); both draw into the current window.
 */
#pragma once

#include "voidmaiz/canvas.hpp"

#include <string>
#include <vector>

namespace maiz {

/* One log line, as the host's log sink received it. Use level ">" for
 * command echoes (results the host wants shown alongside the core's lines). */
struct LogEntry {
    std::string level; // "info" | "warn" | "error" | ">" | …
    std::string op;
    std::string msg;
};

/* The strip as clipboard text, one "[level] op: msg" line each.
 * condensed=true keeps only the structural story: errors/warnings plus
 * mutation-spine lines (rune/rm/link/unlink/tag/mantle/glyph/undo/redo/set/
 * setjson/use, and batches containing any of those), dropping view-tier noise
 * (config lines, pos/size/collapsed writes, move-only batches) and query
 * chatter. Exposed so hosts can put copy buttons anywhere. */
std::string log_to_text(const std::vector<LogEntry>& entries, bool condensed);

/* Level-colored, pinned to the tail. Right-click the strip for copy options
 * ("copy condensed" / "copy all" → the OS clipboard via log_to_text). */
void draw_log_strip(const std::vector<LogEntry>& entries);

struct CommandBarState {
    char buf[512] = {};
    std::vector<std::string> history;
    int hist_pos = -1; // -1 = composing fresh
};

/* Full-width input; Enter (or the run button) emits the line as a command in
 * CanvasIO and keeps focus for the next one; Up/Down recalls history. */
CanvasIO draw_command_bar(CommandBarState& st);

/* ── panel splitter ──────────────────────────────────────────────────────────
 * A draggable divider between two regions laid out by the caller. `frac` is
 * the first region's share of `span` pixels; the drag updates it live and
 * `released` fires once at drag end — the host's cue to flush the layout to
 * the config tier (view state: logged and persisted, never popped by undo,
 * exactly like the camera). */
struct SplitterResult {
    bool dragging = false; // frac changed this frame
    bool released = false; // the drag just ended (flush now)
};

/* vertical=true draws a vertical bar (splits left|right, drags along x);
 * false draws a horizontal bar (splits top/bottom, drags along y). Call at
 * the cursor position between the two regions (use SameLine for vertical).
 * length is the bar's extent along its own axis (0 = the available region). */
SplitterResult splitter(const char* str_id, bool vertical, float& frac, float span,
                        float min_frac = 0.1f, float max_frac = 0.9f,
                        float thickness = 6.0f, float length = 0.0f);

/* ── touch chrome (the input-modality axis, substrates.md) ───────────────────
 * The library's widgets are substrate-free by keeping their metrics in the
 * ImGuiStyle; a touch shell calls apply_touch_metrics ONCE after creating the
 * context (before any theming) and every widget — menus, buttons, splitters,
 * modals — becomes finger-sized. `scale` is the shell's density scale
 * (dpi/160, typically ×1.3 on top: physical parity still reads small on
 * glass). Canvas text is NOT affected: node names scale with the camera, not
 * the chrome (canvas_font in canvas.cpp). */
void apply_touch_metrics(float scale);

/* A toolbar button that is a real target on glass and stays compact on the
 * desktop: touch=false → SmallButton, touch=true → Button (which picks up
 * apply_touch_metrics' padding). Hosts build their toolbars from this so the
 * same frame() code serves both shells. */
bool tool_button(const char* label, bool touch);

/* ── docking workspace (Q11's pane rung, ruled 2026-07-20) ───────────────────
 * The author's ruling: enable Dear ImGui's DockSpace so a host's panels are
 * movable / floating / re-dockable (FL-Studio-style, "everything a window"),
 * rather than the library hand-rolling a pane manager. **DockSpace only —
 * multi-viewport stays OFF**: it cannot work on a single-surface mobile/NDK
 * target, and the node canvas assumes one OS window. The host still owns WHICH
 * panels exist and their default arrangement (via ImGui's DockBuilder against
 * the returned id); the library owns only the boilerplate every host repeats —
 * the fullscreen host window that carries the DockSpace. Layout persists in
 * ImGui's `.ini` by default (a host-side preference file, like the recents
 * list); a host that would rather ride it in the saved project can serialize
 * ImGui's settings into the config tier itself. This is the "one rung past
 * splitter" Q11 gated on a second real host — Void Hormiga's four workflows. */

/* Turn docking on (or off). Call ONCE after ImGui::CreateContext(), before the
 * first frame. Enables DockingEnable only — never multi-viewport. No-op-safe
 * if the vendored ImGui build lacks docking. */
void enable_docking(bool on = true);

/* Open a fullscreen host window over the main viewport carrying a DockSpace
 * that panels dock into; returns the DockSpace's ImGuiID (an `unsigned`) — seed
 * a default arrangement ONCE with ImGui::DockBuilder* against it. Pair with
 * end_dockspace(). If docking is disabled it still opens the host window (the
 * panels simply float free), so the host's per-frame code is identical either
 * way. Draw your panels as ordinary ImGui::Begin windows between the two. */
unsigned begin_dockspace(const char* str_id = "voidmaiz-workspace");
void end_dockspace();

/* Should the host seed its default arrangement THIS frame?
 *
 * The obvious guard — `DockBuilderGetNode(dock) == nullptr` — is a trap, and
 * every host writes it: begin_dockspace() has already called ImGui::DockSpace(),
 * which CREATES the node, so the test is false on the very first frame and the
 * default layout never gets built. The panels then float as fresh windows,
 * stacked in the corner. It only looked right because a checked-in `.ini`
 * happened to carry a layout (found 2026-08-06 — both example hosts had it).
 *
 * The honest question is "does this dockspace hold a layout yet?", which is:
 * no node, or an empty un-split central node. A layout restored from the user's
 * `.ini` is a split node (or has windows docked), so it answers false and the
 * user's saved arrangement still wins — the property the naive guard intended.
 *
 * Call it right after begin_dockspace(); build with ImGui::DockBuilder* against
 * the returned id when it answers true. */
bool dockspace_needs_seed(unsigned dock_id);

} // namespace maiz
