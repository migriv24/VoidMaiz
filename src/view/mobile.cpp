/* mobile.cpp — the mobile chrome kit (voidmaiz/mobile.hpp). */
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/widgets.hpp" // tool_button

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <cstdio>

namespace maiz {
namespace {

/* Exponential approach, frame-rate independent. Used by the sheet and the
 * swipe drawer so both settle with the same weight. */
float approach(float from, float to, float per_second, float dt) {
    if (dt <= 0.0f) return to;
    float k = 1.0f - std::exp(-per_second * dt);
    float v = from + (to - from) * k;
    return std::fabs(v - to) < 0.001f ? to : v;
}

ImU32 alpha(ImU32 col, float a) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    c.w *= a;
    return ImGui::ColorConvertFloat4ToU32(c);
}

} // namespace

// ── the canvas, read by a finger ─────────────────────────────────────────────

void apply_touch_canvas(CanvasStyle& style, const TouchProfile& profile) {
    // ~half a fingertip: the HIT target grows, the DRAWN marker does not
    style.port_hit_radius = profile.px(4.5f);
    // small on purpose — the recognizer already decided tap vs drag
    style.click_slop = profile.px(1.0f);
    style.hover_tooltips = false;
    style.touch = true; // long press on empty canvas adds a node
}

// ── bottom sheet ─────────────────────────────────────────────────────────────

void bottom_sheet_snap(BottomSheetState& st, int detent) {
    if (st.detents.empty()) return;
    st.detent = std::clamp(detent, 0, (int)st.detents.size() - 1);
}

bool begin_bottom_sheet(const char* str_id, BottomSheetState& st) {
    st.settled = false;
    if (st.detents.empty()) st.detents = {0.12f, 0.5f, 0.92f};
    st.detent = std::clamp(st.detent, 0, (int)st.detents.size() - 1);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& gs = ImGui::GetStyle();

    if (!st.initialized) {
        st.height = st.detents[st.detent];
        st.initialized = true;
    }
    if (!st.dragging) st.height = approach(st.height, st.detents[st.detent], 16.0f, io.DeltaTime);

    const float grip = std::max(26.0f, gs.FramePadding.y * 2.0f + 14.0f);
    float h = std::max(st.height * vp->WorkSize.y, grip);

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(gs.WindowPadding.x, 0));
    ImGui::Begin(str_id, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar(2);

    // ── the grip: the only part that drags ───────────────────────────────────
    ImGui::InvisibleButton("##grip", ImVec2(-1, grip));
    bool grip_active = ImGui::IsItemActive();
    {
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        float w = std::max(36.0f, (mx.x - mn.x) * 0.08f);
        float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;
        float t = std::max(3.0f, grip * 0.14f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(cx - w * 0.5f, cy - t * 0.5f), ImVec2(cx + w * 0.5f, cy + t * 0.5f),
            alpha(ImGui::GetColorU32(ImGuiCol_Text), ImGui::IsItemHovered() ? 0.8f : 0.45f),
            t * 0.5f);
    }

    if (grip_active && !st.dragging) {
        st.dragging = true;
        st.drag_from = st.height;
    }
    if (st.dragging) {
        if (grip_active) {
            // dragging UP grows the sheet, so the delta is negated
            float delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y / vp->WorkSize.y;
            st.height = std::clamp(st.drag_from - delta, st.detents.front(), st.detents.back());
        } else {
            /* Release: settle on the NEAREST detent, unless the release was a
             * flick — a fast drag means the next detent in that direction, the
             * way a phone's sheet answers a flung thumb rather than snapping
             * back to wherever the finger happened to stop. */
            st.dragging = false;
            float v = io.MouseDelta.y / std::max(io.DeltaTime, 1e-4f); // px/s, screen down+
            int best = 0;
            float bd = 1e9f;
            for (int i = 0; i < (int)st.detents.size(); ++i) {
                float d = std::fabs(st.detents[i] - st.height);
                if (d < bd) {
                    bd = d;
                    best = i;
                }
            }
            if (v < -900.0f) best = std::min(best + 1, (int)st.detents.size() - 1);
            else if (v > 900.0f) best = std::max(best - 1, 0);
            st.detent = best;
            st.settled = true; // the host's cue to flush ONE config command
        }
    }

    ImGui::Separator();

    /* Anything above the first detent is a peeking header with no room for
     * content; report that the way ImGui::Begin does, so the caller skips its
     * content block and still calls end_bottom_sheet. */
    bool room = ImGui::GetContentRegionAvail().y > ImGui::GetTextLineHeight();
    st.body_child = room && st.scroll_body;
    if (st.body_child)
        ImGui::BeginChild("##sheet-body", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoSavedSettings);
    return room;
}

void end_bottom_sheet(BottomSheetState& st) {
    if (st.body_child) ImGui::EndChild();
    st.body_child = false;
    ImGui::End();
}

// ── snackbar ─────────────────────────────────────────────────────────────────

void show_snackbar(SnackbarState& st, const std::string& message, const std::string& action,
                   double seconds) {
    st.message = message;
    st.action = action;
    if (seconds <= 0.0) seconds = action.empty() ? 2.5 : 4.0;
    st.until = ImGui::GetTime() + seconds;
}

bool draw_snackbar(SnackbarState& st) {
    if (st.until < 0.0) return false;
    double now = ImGui::GetTime();
    if (now >= st.until) {
        st.until = -1.0;
        return false;
    }
    // fade the last 300 ms rather than vanishing mid-sentence
    float a = (float)std::min(1.0, (st.until - now) / 0.3);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiStyle& gs = ImGui::GetStyle();
    float pad = gs.WindowPadding.x;
    ImGui::SetNextWindowPos(
        ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - pad),
        ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(vp->WorkSize.x * 0.5f, 0),
                                        ImVec2(vp->WorkSize.x - pad * 2.0f, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.94f * a);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, gs.Alpha * a);
    ImGui::Begin("##vm-snackbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

    bool fired = false;
    ImGui::TextUnformatted(st.message.c_str());
    if (!st.action.empty()) {
        ImGui::SameLine(0, gs.ItemSpacing.x * 2.0f);
        if (ImGui::SmallButton(st.action.c_str())) {
            fired = true;
            st.until = -1.0; // acting on it dismisses it
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    return fired;
}

// ── the action button and its speed dial ─────────────────────────────────────

namespace {

/* Place a window in the bottom-right of the work area, inset by `offset` plus
 * the usual margin. Both the fab and the dial ride this. */
void place_corner(const ImVec2& offset, float pivot_y = 1.0f) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float pad = ImGui::GetStyle().WindowPadding.x + 8.0f;
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - pad - offset.x,
                                   vp->WorkPos.y + vp->WorkSize.y - pad - offset.y),
                            ImGuiCond_Always, ImVec2(1.0f, pivot_y));
}

const ImGuiWindowFlags kFloatFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoBackground;

} // namespace

bool fab(const char* label, ImVec2 offset) {
    place_corner(offset);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(label, nullptr, kFloatFlags);
    ImGui::PopStyleVar();
    float d = ImGui::GetFrameHeight() * 1.7f;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, d * 0.5f);
    bool hit = ImGui::Button(label, ImVec2(d, d));
    ImGui::PopStyleVar();
    ImGui::End();
    return hit;
}

int speed_dial(const char* str_id, const char* label, const std::vector<std::string>& entries,
               bool& open, ImVec2 offset) {
    int picked = -1;
    if (open && !entries.empty()) {
        // the column sits above the button, right-aligned with it
        place_corner(ImVec2(offset.x, offset.y + ImGui::GetFrameHeight() * 1.7f + 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin(str_id, nullptr, kFloatFlags);
        ImGui::PopStyleVar();
        for (int i = 0; i < (int)entries.size(); ++i) {
            float w = ImGui::CalcTextSize(entries[i].c_str()).x +
                      ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w); // right-align the stack
            if (ImGui::Button(entries[i].c_str(), ImVec2(w, 0))) {
                picked = i;
                open = false;
            }
        }
        ImGui::End();
    }
    if (fab(label, offset)) open = !open;
    return picked;
}

// ── segmented control ────────────────────────────────────────────────────────

bool segmented(const char* str_id, const std::vector<std::string>& labels, int& current,
               float width) {
    if (labels.empty()) return false;
    ImGuiStyle& gs = ImGui::GetStyle();
    float total = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    float seg = (total - gs.ItemSpacing.x * (labels.size() - 1)) / (float)labels.size();
    bool changed = false;

    ImGui::PushID(str_id);
    for (int i = 0; i < (int)labels.size(); ++i) {
        if (i) ImGui::SameLine();
        bool on = (i == current);
        /* The selected segment is a filled button and the rest are transparent:
         * one selection channel that is not colour alone, because a segmented
         * control on a phone in daylight is the worst case for contrast. */
        ImGui::PushStyleColor(ImGuiCol_Button,
                              gs.Colors[on ? ImGuiCol_ButtonActive : ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              gs.Colors[on ? ImGuiCol_Text : ImGuiCol_TextDisabled]);
        ImGui::PushID(i);
        if (ImGui::Button(labels[i].c_str(), ImVec2(seg, 0)) && !on) {
            current = i;
            changed = true;
        }
        ImGui::PopID();
        ImGui::PopStyleColor(2);
    }
    ImGui::PopID();
    return changed;
}

// ── stepper ──────────────────────────────────────────────────────────────────

bool stepper(const char* str_id, const char* label, float& value, float step, float vmin,
             float vmax, const char* fmt) {
    ImGuiStyle& gs = ImGui::GetStyle();
    float h = ImGui::GetFrameHeight();
    bool changed = false;

    ImGui::PushID(str_id);
    auto bump = [&](float by) {
        float v = value + by;
        if (vmin < vmax) v = std::clamp(v, vmin, vmax);
        if (v != value) {
            value = v;
            changed = true;
        }
    };
    // hold-to-repeat: a stepper that needs twelve taps is a broken stepper
    ImGui::PushButtonRepeat(true);
    if (ImGui::Button("-", ImVec2(h * 1.4f, h))) bump(-step);
    ImGui::SameLine(0, gs.ItemSpacing.x);
    char buf[64];
    std::snprintf(buf, sizeof buf, fmt, value);
    float w = std::max(ImGui::CalcTextSize(buf).x, ImGui::CalcTextSize("0000").x);
    ImGui::AlignTextToFramePadding();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize(buf).x) * 0.5f);
    ImGui::TextUnformatted(buf);
    ImGui::SameLine(0, gs.ItemSpacing.x + (w - ImGui::CalcTextSize(buf).x) * 0.5f);
    if (ImGui::Button("+", ImVec2(h * 1.4f, h))) bump(step);
    ImGui::PopButtonRepeat();
    if (label && *label) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
    }
    ImGui::PopID();
    return changed;
}

// ── swipe-actionable row ─────────────────────────────────────────────────────

int begin_swipe_row(SwipeListState& st, const char* row_id, const std::vector<std::string>& actions,
                    float height) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& gs = ImGui::GetStyle();
    bool is_open = (st.open_id == row_id);
    float h = height > 0.0f ? height : ImGui::GetFrameHeight() * 1.4f;

    // the drawer's width: every action at its natural size
    float drawer = 0.0f;
    for (const auto& a : actions)
        drawer += ImGui::CalcTextSize(a.c_str()).x + gs.FramePadding.x * 2.0f + gs.ItemSpacing.x;

    float target = is_open ? -drawer : 0.0f;
    if (!(st.dragging && st.drag_id == row_id))
        st.offset = is_open || st.offset != 0.0f
                        ? approach(is_open ? st.offset : 0.0f, target, 18.0f, io.DeltaTime)
                        : 0.0f;

    ImGui::PushID(row_id);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float full = ImGui::GetContentRegionAvail().x;
    int picked = -1;

    // ── the drawer, drawn UNDER the row and revealed by the displacement ─────
    float shown = is_open || (st.dragging && st.drag_id == row_id) ? -st.offset : 0.0f;
    if (shown > 1.0f && !actions.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + full - drawer, origin.y));
        ImGui::PushClipRect(ImVec2(origin.x + full - shown, origin.y),
                            ImVec2(origin.x + full, origin.y + h), true);
        for (int i = 0; i < (int)actions.size(); ++i) {
            if (i) ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::Button(actions[i].c_str(),
                              ImVec2(ImGui::CalcTextSize(actions[i].c_str()).x +
                                         gs.FramePadding.x * 2.0f,
                                     h))) {
                picked = i;
                st.open_id.clear(); // acting closes the drawer
            }
            ImGui::PopID();
        }
        ImGui::PopClipRect();
    }

    // ── the row itself, displaced ────────────────────────────────────────────
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (st.offset < 0 ? st.offset : 0), origin.y));
    ImGui::InvisibleButton("##swipe", ImVec2(full, h));
    bool active = ImGui::IsItemActive();
    if (active && !st.dragging && std::fabs(ImGui::GetMouseDragDelta().x) > 4.0f) {
        st.dragging = true;
        st.drag_id = row_id;
        st.drag_from = is_open ? -drawer : 0.0f;
    }
    if (st.dragging && st.drag_id == row_id) {
        if (active) {
            st.offset = std::clamp(st.drag_from + ImGui::GetMouseDragDelta().x, -drawer, 0.0f);
        } else {
            /* Past halfway it opens, before halfway it springs back — the
             * standard threshold, and the one a thumb expects without being
             * told. One row at a time: opening this one closes the last. */
            st.dragging = false;
            st.open_id = (-st.offset > drawer * 0.5f) ? row_id : std::string();
        }
    } else if (ImGui::IsItemDeactivated() && is_open && !st.dragging) {
        st.open_id.clear(); // a plain tap on an open row closes it
    }

    // the caller draws the row's content over the invisible button
    ImGui::SetCursorScreenPos(
        ImVec2(origin.x + (st.offset < 0 ? st.offset : 0) + gs.FramePadding.x,
               origin.y + (h - ImGui::GetTextLineHeight()) * 0.5f));
    return picked;
}

void end_swipe_row(SwipeListState& st) {
    (void)st;
    ImGui::PopID();
    ImGui::Spacing();
}

/* Dim AND wrapped (voidmaiz/mobile.hpp). */
void dim_wrapped(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// ── a drawn keyboard ─────────────────────────────────────────────────────────

bool keyboard(KeyboardState& st, bool touch) {
    ImGuiIO& io = ImGui::GetIO();
    st.visible = touch && io.WantTextInput;
    st.height = 0.0f;
    if (!st.visible) {
        st.shift = false;
        st.symbols = false;
        return false;
    }
    static const char* letters[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    static const char* symbols[3] = {"1234567890", "-_/:.,@", "!?'\"()+="};
    const char** rows = st.symbols ? symbols : letters;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float gap = std::round(style.ItemSpacing.x * 0.5f) + 1.0f;
    const float key_h = std::round(ImGui::GetFontSize() * 2.1f);
    const float total = key_h * 4 + gap * 5;
    st.height = total;

    const ImVec2 pmin(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - total);
    const ImVec2 pmax(vp->WorkPos.x + vp->WorkSize.x, vp->WorkPos.y + vp->WorkSize.y);
    dl->AddRectFilled(pmin, pmax, ImGui::GetColorU32(ImGuiCol_WindowBg, 0.98f));
    dl->AddLine(pmin, ImVec2(pmax.x, pmin.y), ImGui::GetColorU32(ImGuiCol_Border));

    const bool over = io.MousePos.x >= pmin.x && io.MousePos.x <= pmax.x &&
                      io.MousePos.y >= pmin.y && io.MousePos.y <= pmax.y;
    const float unit = (vp->WorkSize.x - gap * 11) / 10.0f;

    float x = 0, y = pmin.y + gap;
    auto key = [&](const char* label, float units, bool accent) {
        ImVec2 a(pmin.x + x, y);
        ImVec2 b(a.x + unit * units + gap * (units - 1.0f), a.y + key_h);
        x += (b.x - a.x) + gap;
        const bool hot = over && io.MousePos.x >= a.x && io.MousePos.x <= b.x &&
                         io.MousePos.y >= a.y && io.MousePos.y <= b.y;
        ImGuiCol c = ImGuiCol_Button;
        if (hot && io.MouseDown[0]) c = ImGuiCol_ButtonActive;
        else if (accent) c = ImGuiCol_ButtonHovered;
        dl->AddRectFilled(a, b, ImGui::GetColorU32(c), style.FrameRounding);
        ImVec2 t = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(std::round((a.x + b.x - t.x) * 0.5f),
                           std::round((a.y + b.y - t.y) * 0.5f)),
                    ImGui::GetColorU32(ImGuiCol_Text), label);
        return hot && io.MouseClicked[0];
    };

    for (int r = 0; r < 3; ++r) {
        const std::string row = rows[r];
        // the shorter rows sit centred, as on every phone
        x = std::max(0.0f, (vp->WorkSize.x - (unit * row.size() + gap * (row.size() - 1))) * 0.5f);
        for (char ch : row) {
            if (!st.symbols && st.shift) ch = (char)std::toupper((unsigned char)ch);
            const char label[2] = {ch, 0};
            if (key(label, 1.0f, false)) {
                io.AddInputCharacter((unsigned)(unsigned char)ch);
                st.shift = false;
            }
        }
        y += key_h + gap;
    }
    x = gap;
    if (key(st.symbols ? "abc" : "123", 1.4f, false)) st.symbols = !st.symbols;
    if (key("shift", 1.4f, st.shift)) st.shift = !st.shift;
    if (key("space", 3.4f, false)) io.AddInputCharacter(' ');
    if (key("back", 1.4f, false)) {
        io.AddKeyEvent(ImGuiKey_Backspace, true); // the field's own handler does the rest
        io.AddKeyEvent(ImGuiKey_Backspace, false);
    }
    if (key("done", 1.4f, true)) {
        io.AddKeyEvent(ImGuiKey_Enter, true);
        io.AddKeyEvent(ImGuiKey_Enter, false);
    }

    if (over) {
        /* The keyboard swallows the touch. Nothing under it hovers, nothing under
         * it is clicked, and — the point — the text field being typed into never
         * sees a click land outside itself, so it stays focused. */
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i) {
            io.MouseDown[i] = false;
            io.MouseClicked[i] = false;
        }
    }
    return true;
}

// ── the action bar ──────────────────────────────────────────────────────────

float dots_button_width(bool touch) {
    return touch ? ImGui::GetFrameHeight() : ImGui::GetTextLineHeight() + 6.0f;
}

bool dots_button(const char* str_id, bool touch) {
    float w = dots_button_width(touch);
    float h = touch ? ImGui::GetFrameHeight() : ImGui::GetTextLineHeight();
    ImVec2 at = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(str_id, ImVec2(w, h));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = ImGui::GetColorU32(ImGui::IsItemActive()    ? ImGuiCol_ButtonActive
                                  : ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered
                                                           : ImGuiCol_Button);
    dl->AddRectFilled(at, ImVec2(at.x + w, at.y + h), bg, ImGui::GetStyle().FrameRounding);
    // three dots, stacked (the platform's "more" affordance)
    float r = std::max(1.5f, h * 0.075f);
    ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text);
    for (int k = -1; k <= 1; ++k)
        dl->AddCircleFilled(ImVec2(at.x + w * 0.5f, at.y + h * 0.5f + k * h * 0.24f), r, fg);
    return pressed;
}

bool begin_overflow_menu(const char* str_id, bool touch) {
    ImGui::PushID(str_id);
    if (dots_button("##more", touch)) ImGui::OpenPopup("##menu");
    bool open = ImGui::BeginPopup("##menu");
    if (!open) ImGui::PopID();
    return open;
}

void end_overflow_menu() {
    ImGui::EndPopup();
    ImGui::PopID();
}

int action_bar(const char* str_id, const std::vector<BarAction>& actions, bool touch,
               float width) {
    ImGui::PushID(str_id);
    const ImGuiStyle& style = ImGui::GetStyle();
    float avail = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    std::vector<ActionSpec> specs;
    specs.reserve(actions.size());
    for (const auto& a : actions) {
        ActionSpec s;
        // Button and SmallButton share the horizontal padding
        s.width = ImGui::CalcTextSize(a.label.c_str(), nullptr, true).x + style.FramePadding.x * 2;
        s.priority = a.priority;
        s.pinned = a.pinned;
        specs.push_back(s);
    }
    ActionPlan plan = plan_action_bar(specs, avail, dots_button_width(touch), style.ItemSpacing.x);

    int pressed = -1;
    bool first = true;
    for (int i : plan.visible) {
        const BarAction& a = actions[i];
        if (!first) ImGui::SameLine();
        first = false;
        ImGui::PushID(i);
        if (!a.enabled) ImGui::BeginDisabled();
        if (a.primary) {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_ButtonActive]);
        }
        bool hit = tool_button(a.label.c_str(), touch);
        if (a.primary) ImGui::PopStyleColor(2);
        if (!a.enabled) ImGui::EndDisabled();
        ImGui::PopID();
        if (hit) pressed = i;
    }
    if (!plan.overflow.empty()) {
        if (!first) ImGui::SameLine();
        if (dots_button("##more", touch)) ImGui::OpenPopup("##overflow");
        if (ImGui::BeginPopup("##overflow")) {
            for (int i : plan.overflow) {
                const BarAction& a = actions[i];
                ImGui::PushID(i);
                if (ImGui::MenuItem(a.label.c_str(), nullptr, false, a.enabled)) pressed = i;
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::PopID();
    return pressed;
}

} // namespace maiz
