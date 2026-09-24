/*
 * voidmaiz/textinputview.hpp — the keyboard holiday's ImGui half.
 *
 * voidmaiz/textinput.hpp is the pure mapping and the platform interface; this is
 * the glue that watches ImGui for "a text field wants typing", brings the
 * platform keyboard up for it, and turns what the keyboard does into the keys
 * and characters the field already accepts.
 *
 * ONE CALL A FRAME, immediately after ImGui::NewFrame():
 *
 *     ImGui::NewFrame();
 *     maiz::text_input_frame(session, platform.get());   // null platform = no-op
 *
 * A host with no platform keyboard (a desktop, or an APK still on a plain
 * NativeActivity) passes null, or keeps maiz::keyboard() (mobile.hpp) as the
 * drawn fallback. The two never run together: the drawn one is for when there
 * is nothing better.
 *
 * TELLING THE KEYBOARD WHAT A FIELD IS: right after a field, while it is the
 * last item,
 *
 *     ImGui::InputText("##phone", buf, sizeof buf);
 *     maiz::text_input_kind(maiz::InputKind::Phone);
 *
 * The widget registry does this for every field it draws (from the field's key
 * and editor), so a host writes this line only for its own hand-made fields.
 *
 * View-module header (ImGui types): only for targets linking voidmaiz_view.
 */
#pragma once

#include "voidmaiz/textinput.hpp"

#include <string>

struct AInputEvent; // <android/input.h>; Android only (global, like Android's own)

namespace maiz {

/* The glue's memory between frames. One per window. */
struct TextInputSession {
    unsigned active = 0;          // the ImGui id of the field the keyboard is for (0 = none)
    std::string synced;           // what we believe that field holds
    int synced_cursor = 0;        // …and where its cursor is (byte offset)
    bool shown = false;
    int quiet_frames = 0;         // frames since our last keystroke was consumed
    InputKind kind = InputKind::Text;
    InputAction action = InputAction::Done;
    float covered_px = 0.0f;      // what the keyboard covers now (0 = hidden)
};

/* The frame's work; see the header comment. Safe to call with null. */
void text_input_frame(TextInputSession& session, TextInputPlatform* platform);

/* Declare the LAST item's keyboard (call right after the InputText). The action
 * defaults to the kind's (a newline key for Multiline, Search for Search, Done
 * otherwise). Cheap; remembered for a few frames by item id. */
void text_input_kind(InputKind kind);
void text_input_kind(InputKind kind, InputAction action);

/* ── Android's system keys ───────────────────────────────────────────────────
 * ImGui's android backend has no mapping for AKEYCODE_BACK, so the Back button
 * a phone user presses reached no application at all (found 2026-09-23). This
 * turns it into ImGuiKey_AppBack, which a host's navigation reads. Call it
 * first in the shell's input handler; true = consumed (the activity will not
 * finish on Back, so the host decides what Back means). While the system
 * keyboard is up, Back never gets here: the keyboard takes it to close itself.
 * Always false off Android. */
bool android_system_key(const AInputEvent* event);

} // namespace maiz
