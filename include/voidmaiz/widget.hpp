/*
 * voidmaiz/widget.hpp — the host-widget protocol + registry (Phase 4,
 * okf/concepts/widget-registry.md — drafted 2026-07-16 for Void Hormiga's
 * Data section, the forcing client).
 *
 * The contract, made code: a widget is a function that receives a projection
 * and RETURNS commands — it never touches the model (CLI representation and
 * undo participation fall out of that shape, not out of ceremony), and it
 * matches the one tag grammar through the context's filter (tag awareness).
 * Responsiveness is the re-projection itself: dispatch → re-project → the
 * widget draws the new truth next frame.
 *
 * The registrable unit is the FIELD EDITOR. Glyphs declare WHAT can be edited
 * (their `fields` list) and — via the `hints.editors` map — WHICH editor kind
 * edits it ("date", "number:0.1,0,10", "combo:a,b,c", or a host-registered
 * kind like "color"). The registry maps kind → renderer; registration makes a
 * host's editor appear on EVERY surface that renders fields: the inspector,
 * node faces, forms/detail panes, and (when it lands) table cells. Widgets
 * that aren't field-shaped (buttons firing domain commands, drop zones,
 * whole panes) don't need the registry at all — implementing the context
 * shape by hand IS speaking the protocol.
 *
 * Staging discipline (VLS #14b, non-negotiable for registered editors): while
 * a widget is being manipulated its staged value is local truth for that
 * widget only — re-projection never yanks a live edit — and the gesture's end
 * flushes at most ONE command. Escape/abandon compiles nothing.
 *
 * This header is the ImGui-composed kit (the sanctioned path). A wrapped
 * foreign toolkit would have to fit through the same door — same context,
 * same one-command commits — but no adapter machinery ships until the author
 * sanctions a client that needs one (developer_questions.md Q12).
 *
 * View-module header (ImGui types). Not to be confused with widgets.hpp,
 * the library's own observability widgets (log strip, command bar, splitter).
 */
#pragma once

#include "voidmaiz/project.hpp" // node_matches (the one filter grammar)
#include "voidmaiz/scene.hpp"

#include "imgui.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace maiz {

/* What every protocol widget receives: the projection to read, the command
 * list to write. The host dispatches the commands and re-projects (the
 * one-sync rule) — exactly edit_canvas's seam, widget-sized. */
struct WidgetContext {
    const Scene& scene;                 // the truth picture (read-only)
    std::vector<std::string>& commands; // compiled interactions land here
    std::string filter;                 // active tag-filter expression; "" = all
    float width = 0.0f;                 // suggested item width px (0 = ImGui default)
};

/* Tag awareness: does this node pass the context's filter? (node_matches
 * over tags + name + "glyph:<g>" — the same bag every surface uses.) */
bool widget_visible(const WidgetContext& ctx, const SceneNode& node);

// ── the field editors (the library's kit; the registry's defaults) ──────────
// Each draws one field of one node, staged, and compiles at most ONE
// `set`/`setjson` on commit. A field whose like-named input port is wired
// renders disabled — the control graph owns that value. All return true the
// frame a command was emitted.
//
// `label` is the visible text (the glyph's hints.labels entry, surfaced as
// SceneField.label); nullptr/"" falls back to the raw field key. The control's
// ImGui ID always stays keyed on the field key, so relabeling never resets a
// live edit. Faces pass nullptr (a node-face label is inline chrome); the
// registry threads SceneField.label through.

/* Staged single-line text ("text"). String fields commit `set`; non-string
 * fields edit their compact JSON and commit `setjson`. */
bool widget_field_text(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const char* label = nullptr);

/* Staged drag-number ("number", args "speed,min,max"). */
bool widget_field_number(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                         float speed = 0.05f, float vmin = 0.0f, float vmax = 0.0f,
                         const char* label = nullptr);

/* Checkbox over a boolean field ("bool" / "checkbox"). A click is atomic —
 * no staging — and commits ONE `setjson <key> true|false`. A non-`true`/`false`
 * value_json reads as false; the commit normalizes it. */
bool widget_field_bool(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const char* label = nullptr);

/* Staged multiline text ("multiline", args "height"); commit on blur or
 * Ctrl+Enter, Escape reverts. height 0 = a 4-line default. */
bool widget_field_multiline(WidgetContext& ctx, const SceneNode& node,
                            const char* field_key, float height = 0.0f,
                            const char* label = nullptr);

/* Combo over fixed options ("combo:a,b,c"); picking commits one `set`. */
bool widget_field_combo(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                        const std::vector<std::string>& options, const char* label = nullptr);

/* Staged Y/M/D drag-triple over an ISO-8601 string field ("date"). */
bool widget_field_date(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const char* label = nullptr);

/* A file/path field ("path"): a staged text box (edit the path by hand, ONE
 * `set` on commit, exactly like widget_field_text) PLUS a "…" browse button.
 * The library owns the box and the commit; the HOST owns the OS file dialog —
 * the button calls `browse` and, on a non-empty return, commits ONE `set` of
 * the chosen path. Not in defaults() (the library ships no portable dialog):
 * register it with your dialog wired in, or use WidgetRegistry::add_path. */
using PathBrowseFn = std::function<std::string(const SceneNode& node, const char* field_key,
                                               std::string_view current)>;
bool widget_field_path(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const PathBrowseFn& browse, const char* label = nullptr);

/* Rotary knob over a numeric field ("knob:min,max,default,snap" — snap ≠ 0
 * rounds to integers). VLS-native's signature control, absorbed 2026-07-16:
 * 270° sweep with the gap at the bottom, vertical drag stages locally
 * (≈160 px = full sweep) and flushes ONE command on release, double-click
 * resets to vdefault, wired = disabled + tooltip. Colors derive from the
 * ImGui style (SliderGrab accent), so the knob wears the host's theme.
 * `scale` zooms the whole control (faces pass the camera zoom). Requires
 * vmax > vmin — a knob over an unbounded value would lie. */
bool widget_field_knob(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       float vmin, float vmax, bool integer = false, float vdefault = 0.0f,
                       float scale = 1.0f, const char* label = nullptr);

// ── the registry ─────────────────────────────────────────────────────────────

/* A registered field editor. `args` is the editor spec's tail: a glyph hint
 * "combo:red,green" reaches the "combo" entry with args = "red,green". */
using FieldWidget = std::function<bool(WidgetContext& ctx, const SceneNode& node,
                                       const SceneField& field, std::string_view args)>;

/* Kind → editor. Host-owned, like FaceRegistry (no globals, no per-Core
 * coupling — widgets are view config, re-established by the host at boot).
 * Start from defaults() and add your kinds; re-registering a kind overrides
 * it, so a host can also reskin the built-ins. */
struct WidgetRegistry {
    std::unordered_map<std::string, FieldWidget> editors;

    /* The kit above, registered under "text", "number", "bool" (also
     * "checkbox"), "multiline", "combo", "date", "knob". "path" is NOT here —
     * it needs the host's file dialog; wire it in with add_path. */
    static WidgetRegistry defaults();

    /* Register the "path" editor (widget_field_path) with the host's file
     * dialog wired in — the one line that makes file pickers appear on every
     * field surface. */
    void add_path(PathBrowseFn browse);
};

/* Split an editor spec into kind and args ("combo:a,b" → "combo", "a,b"). */
void split_editor_spec(std::string_view spec, std::string& kind, std::string& args);

/* Draw one field through its declared editor (field.editor, from the glyph's
 * hints.editors). No/unknown declaration falls back to widget_field_text —
 * every declared field is always editable, hints only upgrade the widget. */
bool widget_field(WidgetContext& ctx, const WidgetRegistry& reg, const SceneNode& node,
                  const SceneField& field);

/* A node's whole form — every declared field through widget_field, labeled.
 * The detail-pane/wizard-page building block: chrome around it (headers,
 * tabs, buttons) stays the host's. Returns true if any field committed. */
bool widget_form(WidgetContext& ctx, const WidgetRegistry& reg, const SceneNode& node);

} // namespace maiz
