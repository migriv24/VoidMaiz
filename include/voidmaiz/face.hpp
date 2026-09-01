/*
 * voidmaiz/face.hpp — node faces: per-glyph body renderers (Phase 4).
 *
 * A face is domain code drawing INSIDE a node's body — VLS's piano roll, a
 * waveform, a color swatch — registered by the host per glyph. Faces are
 * views like any other: they read the projected node and emit dispatcher
 * commands; they never touch model state directly. The registry here was the
 * first concrete surface of the widget-registry contract
 * (okf/concepts/widget-registry.md): CLI representation (commands out),
 * tag awareness (the node's tags ride in), undo participation (commands are
 * ordinary model commands), responsiveness (re-projection feeds back in).
 * The general protocol now lives in voidmaiz/widget.hpp — the face widgets
 * below are thin frames over its field-editor kit.
 *
 * The face area sits below the port rows and above the tag badges; hosts
 * size it via the glyph's hints.face. Face widgets follow the VLS #14b
 * staging discipline: drags stage locally and flush ONE command on release.
 *
 * View-module header (ImGui types).
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include "imgui.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace maiz {

struct FaceContext {
    const SceneNode& node;  // the projection (fields, tags, size — all of it)
    const Scene& scene;     // the whole scene (wire lookups, neighbors)
    ImVec2 pos;             // screen top-left of the face area
    ImVec2 size;            // screen size of the face area
    float zoom = 1.0f;
    std::vector<std::string>& commands; // compiled gestures land here
};

using FaceRenderer = std::function<void(FaceContext&)>;

struct FaceRegistry {
    std::unordered_map<std::string, FaceRenderer> by_glyph;
};

// ── default face widgets (the library's node-specific widgets) ──────────────

/* A drag-number bound to a field: shows the projected value, stages while
 * dragging (re-projection never yanks it), and compiles ONE `set`/`setjson`
 * on release. If an input port named like the field is wired, the widget
 * renders disabled — the control graph owns that value (VLS #14b).
 * Returns true the frame a command was emitted. */
bool face_drag_number(FaceContext& ctx, const char* field_key, float speed = 0.05f,
                      float vmin = 0.0f, float vmax = 0.0f);

/* A combo bound to a field: picking an option compiles one `set`. */
bool face_combo(FaceContext& ctx, const char* field_key,
                const std::vector<std::string>& options);

/* A staged multiline text area bound to a string field (narratives, bios,
 * descriptions — Void Hormiga ask §3.1, 2026-07-15). Edit freely — while the
 * widget is active ImGui owns the buffer, so re-projection never yanks the
 * edit; commit ONE `set` on blur or Ctrl+Enter; Escape reverts (ImGui's
 * native revert — the unchanged value compiles nothing). Height 0 fills the
 * face area. Values up to ~4KB stage; longer text needs a host widget. */
bool face_text_multiline(FaceContext& ctx, const char* field_key, float height = 0.0f);

/* An image region (Void Hormiga ask §3.2): the HOST owns file I/O, decoding,
 * and the texture lifetime; the library owns layout (aspect-fit within the
 * face area), the placeholder while tex is 0 (still loading / none), and the
 * hit-test. Returns true on click — the host compiles whatever command the
 * click means (there is no default: an image click is domain semantics). */
bool face_image(FaceContext& ctx, ImTextureID tex, float tex_w, float tex_h,
                const char* placeholder = "image");

/* A staged date field bound to a string field holding ISO-8601 "YYYY-MM-DD"
 * (Void Hormiga ask §3.3): three drag-fields (Y/M/D), each completed drag
 * commits ONE `set` with the clamped, zero-padded ISO string — free-text
 * date drift never reaches the model. An unparsable value starts from
 * 2026-01-01. */
bool face_date(FaceContext& ctx, const char* field_key);

/* A rotary knob bound to a numeric field (VLS-native's signature control,
 * absorbed into the kit 2026-07-16): 270° sweep, vertical drag stages and
 * flushes ONE command on release, double-click resets to vdefault, integer
 * snap, disabled + tooltip when wired. Scales with the camera zoom. Requires
 * vmax > vmin — a knob over an unbounded value would lie (use
 * face_drag_number for those). */
bool face_knob(FaceContext& ctx, const char* field_key, float vmin, float vmax,
               bool integer = false, float vdefault = 0.0f);

} // namespace maiz
