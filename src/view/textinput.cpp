/* src/view/textinput.cpp — the keyboard holiday's ImGui half
 * (voidmaiz/textinputview.hpp). Watches ImGui's active text field, drives a
 * TextInputPlatform, and turns its editing states into keys and characters. */
#include "voidmaiz/textinputview.hpp"

#include "imgui.h"
#include "imgui_internal.h" // GetInputTextState, ActiveId, InputEventsQueue

#include <unordered_map>

#ifdef __ANDROID__
#include <android/input.h>
#endif

namespace maiz {

namespace {

/* What each field declared, by item id, with the frame it was last declared
 * on — a field that stops being drawn stops being remembered. */
struct Declared {
    InputKind kind;
    InputAction action;
    int frame;
};
std::unordered_map<ImGuiID, Declared>& declared() {
    static std::unordered_map<ImGuiID, Declared> m;
    return m;
}

void press(ImGuiIO& io, ImGuiKey k, int times = 1) {
    for (int i = 0; i < times; ++i) {
        io.AddKeyEvent(k, true);
        io.AddKeyEvent(k, false);
    }
}

void emit(ImGuiIO& io, const EditPlan& p) {
    if (p.move < 0) press(io, ImGuiKey_LeftArrow, -p.move);
    if (p.move > 0) press(io, ImGuiKey_RightArrow, p.move);
    press(io, ImGuiKey_Backspace, p.backspaces);
    if (!p.insert.empty()) io.AddInputCharactersUTF8(p.insert.c_str());
    if (p.move_after < 0) press(io, ImGuiKey_LeftArrow, -p.move_after);
    if (p.move_after > 0) press(io, ImGuiKey_RightArrow, p.move_after);
}

ImGuiKey key_of(TextKey k) {
    switch (k) {
    case TextKey::Left: return ImGuiKey_LeftArrow;
    case TextKey::Right: return ImGuiKey_RightArrow;
    case TextKey::Backspace: return ImGuiKey_Backspace;
    case TextKey::Delete: return ImGuiKey_Delete;
    case TextKey::Tab: return ImGuiKey_Tab;
    case TextKey::Enter: return ImGuiKey_Enter;
    }
    return ImGuiKey_Enter;
}

} // namespace

void text_input_kind(InputKind kind) { text_input_kind(kind, default_action(kind)); }

void text_input_kind(InputKind kind, InputAction action) {
    const ImGuiID id = ImGui::GetItemID();
    if (id) declared()[id] = {kind, action, ImGui::GetFrameCount()};
}

void text_input_frame(TextInputSession& s, TextInputPlatform* platform) {
    if (!platform) return;
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiIO& io = ImGui::GetIO();
    s.covered_px = platform->covered_px();

    // forget declarations nobody has made for a while (fields not drawn)
    for (auto it = declared().begin(); it != declared().end();)
        it = (g.FrameCount - it->second.frame > 120) ? declared().erase(it) : std::next(it);

    /* THE FIELD. ImGui answers "does something want text?" from last frame, and
     * the active text state is readable until the field lets go. Both are what
     * a person would see: a caret in a box. */
    ImGuiInputTextState* st = ImGui::GetInputTextState(g.ActiveId);
    std::vector<TextInputEvent> events;
    if (!st || !io.WantTextInput) {
        platform->poll(events); // nothing to type into: what arrives is dropped
        if (s.shown) platform->hide();
        s = TextInputSession{};
        return;
    }
    const std::string now(st->TextA.Data ? st->TextA.Data : "", (size_t)st->TextLen);
    const int cursor = st->GetCursorPos();

    if (st->ID != s.active || !s.shown) {
        // a new field (or the keyboard went away): bring it up, seeded
        s.active = st->ID;
        s.synced = now;
        s.synced_cursor = cursor;
        auto d = declared().find(st->ID);
        const bool multi = (st->Flags & ImGuiInputTextFlags_Multiline) != 0;
        s.kind = d != declared().end() ? d->second.kind : (multi ? InputKind::Multiline : InputKind::Text);
        s.action = d != declared().end() ? d->second.action : default_action(s.kind);
        EditingState seed;
        seed.text = now;
        seed.sel_start = seed.sel_end = cursor;
        platform->show(s.kind, s.action, seed);
        s.shown = true;
        s.quiet_frames = 0;
        platform->poll(events); // anything from before this field is not for it
        return;
    }

    // a tap on the field while the keyboard was dismissed by the platform's
    // own Back: ask again (show is idempotent on the platform side)
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && platform->covered_px() <= 0.0f) {
        EditingState seed;
        seed.text = now;
        seed.sel_start = seed.sel_end = cursor;
        platform->show(s.kind, s.action, seed);
    }

    /* WHAT THE KEYBOARD DID, in order: the mapping, then keys. */
    platform->poll(events);
    for (const auto& e : events) {
        if (e.type == TextInputEvent::Type::State) {
            emit(io, plan_edit(s.synced, s.synced_cursor, e.state));
            s.synced = e.state.text;
            s.synced_cursor = e.state.sel_end;
        } else {
            press(io, key_of(e.key)); // a raw key, or the action key (Enter)
        }
    }
    if (!events.empty()) {
        s.quiet_frames = 0;
        return;
    }

    /* THE FIELD MOVED WITHOUT THE KEYBOARD — a tap placed the cursor, a
     * hardware key typed. Only once our own keystrokes have landed: ImGui
     * trickles key presses across frames, and a field read mid-trickle is not a
     * divergence, it is the plan half-applied. Two quiet frames = settled. */
    if (!g.InputEventsQueue.empty()) {
        s.quiet_frames = 0;
        return;
    }
    if (++s.quiet_frames < 2) return;
    if (now != s.synced || cursor != s.synced_cursor) {
        s.synced = now;
        s.synced_cursor = cursor;
        EditingState st2;
        st2.text = now;
        st2.sel_start = st2.sel_end = cursor;
        platform->update(st2);
    }
}

#ifdef __ANDROID__
bool android_system_key(const AInputEvent* ev) {
    if (!ev || AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_KEY) return false;
    if (AKeyEvent_getKeyCode(ev) != AKEYCODE_BACK) return false;
    const int32_t a = AKeyEvent_getAction(ev);
    if (a == AKEY_EVENT_ACTION_DOWN || a == AKEY_EVENT_ACTION_UP)
        ImGui::GetIO().AddKeyEvent(ImGuiKey_AppBack, a == AKEY_EVENT_ACTION_DOWN);
    return true;
}
#else
bool android_system_key(const AInputEvent*) { return false; }
#endif

} // namespace maiz
