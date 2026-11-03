/* src/view/nav.cpp — see voidmaiz/nav.hpp. ImGui only. Lifted from Void
 * Hormiga's src/phone/nav.cpp (2026-09-23); the drawing is theirs. */
#include "voidmaiz/nav.hpp"
#include "voidmaiz/mobile.hpp" // default_touch_gate: iOS's edge swipe

#include "imgui_internal.h" // BeginViewportSideBar: reserve an edge of the viewport

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace maiz {

namespace {

const ImGuiWindowFlags kBarFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

// the title is set larger than body text (a phone's top bar reads at arm's length)
constexpr float kTitleScale = 1.3f;
float app_bar_height() {
    const ImGuiStyle& s = ImGui::GetStyle();
    return std::max(ImGui::GetFrameHeight(), ImGui::GetFontSize() * kTitleScale + s.FramePadding.y * 2.0f) +
           s.WindowPadding.y * 2.0f;
}

/* One destination: an icon over its label, a pill behind the current one, a
 * badge if it has a count. Returns true when tapped. Drawn in the bar window. */
bool draw_item(const NavItem& it, int id, float x, float w, bool sel) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 at(x, ImGui::GetWindowPos().y);
    const float h = ImGui::GetWindowSize().y;
    const ImU32 on = ImGui::GetColorU32(ImGuiCol_CheckMark);
    const ImU32 off = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImFont* font = ImGui::GetFont();
    const float icon_px = ImGui::GetFontSize() * 1.35f;
    const float label_px = ImGui::GetFontSize() * 0.82f;
    ImGui::SetCursorScreenPos(at);
    ImGui::PushID(id);
    const bool tapped = ImGui::InvisibleButton("##dest", ImVec2(w, h));
    const bool hot = ImGui::IsItemHovered();
    ImGui::PopID();
    const ImU32 col = sel ? on : (hot ? ImGui::GetColorU32(ImGuiCol_Text) : off);
    const float cx = x + w * 0.5f;
    const ImVec2 isz = it.icon && !it.draw ? font->CalcTextSizeA(icon_px, FLT_MAX, 0, it.icon)
                                           : ImVec2(icon_px, icon_px);
    const float iy = at.y + h * 0.16f;
    if (sel) { // the selected destination gets a pill behind its icon (Material 3)
        const ImVec4 a = ImGui::ColorConvertU32ToFloat4(on);
        dl->AddRectFilled(ImVec2(cx - isz.x * 1.2f - 6, iy - 3), ImVec2(cx + isz.x * 1.2f + 6, iy + isz.y + 3),
                          ImGui::GetColorU32(ImVec4(a.x, a.y, a.z, 0.18f)), isz.y);
    }
    if (it.draw) it.draw(dl, ImVec2(cx, iy + isz.y * 0.5f), icon_px, col);
    else if (it.icon) dl->AddText(font, icon_px, ImVec2(cx - isz.x * 0.5f, iy), col, it.icon);
    const ImVec2 lsz = font->CalcTextSizeA(label_px, FLT_MAX, 0, it.label);
    dl->AddText(font, label_px, ImVec2(cx - lsz.x * 0.5f, iy + isz.y + 6), col, it.label);
    if (it.badge > 0) {
        char b[8];
        std::snprintf(b, sizeof b, "%d", std::min(it.badge, 99));
        const float r = label_px * 0.75f;
        const ImVec2 bc(cx + isz.x * 0.5f + r * 0.6f, iy + r * 0.3f);
        dl->AddCircleFilled(bc, r, IM_COL32(220, 70, 70, 255));
        const ImVec2 bs = font->CalcTextSizeA(label_px * 0.85f, FLT_MAX, 0, b);
        dl->AddText(font, label_px * 0.85f, ImVec2(bc.x - bs.x * 0.5f, bc.y - bs.y * 0.5f), IM_COL32_WHITE, b);
    }
    return tapped;
}

bool begin_nav_window() {
    // no window border: the bar draws its own hairline on the one edge that has one
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const bool open = ImGui::BeginViewportSideBar("##maiz-nav", ImGui::GetMainViewport(), ImGuiDir_Down,
                                                  nav_bar_height(), kBarFlags);
    ImGui::PopStyleVar(2);
    if (open) { // a hairline above the bar, so it reads as an edge and not a gap
        const ImVec2 at = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddLine(at, ImVec2(at.x + size.x, at.y), ImGui::GetColorU32(ImGuiCol_Border));
    }
    return open;
}

int g_back_frame = -1;    // the frame a Back arrived and was read
bool g_back_claimed = false;

} // namespace

float nav_bar_height() { return ImGui::GetFontSize() * 3.6f; }

bool nav_bar(const std::vector<NavItem>& items, int& current) {
    bool changed = false;
    if (begin_nav_window() && !items.empty()) {
        const ImVec2 at = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        const float w = size.x / (float)items.size();
        for (int i = 0; i < (int)items.size(); ++i)
            if (draw_item(items[i], i, at.x + w * (float)i, w, i == current)) {
                changed = true;
                current = i;
            }
    }
    ImGui::End();
    return changed;
}

int nav_bar_centred(const std::vector<NavItem>& items, int current, bool centre_on, const CentreFace& face) {
    int tapped = -1;
    if (begin_nav_window()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 at = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        // the centre takes a slot's width and a little more; the rest share what is left
        const int n = (int)items.size(), left = (n + 1) / 2;
        const float cw = size.x / (float)(n + 1) * 1.1f;
        const float w = n > 0 ? (size.x - cw) / (float)n : 0.0f;
        for (int i = 0; i < n; ++i) {
            const float x = at.x + w * (float)i + (i >= left ? cw : 0.0f);
            if (draw_item(items[i], i, x, w, i == current)) tapped = i;
        }
        const float cx = at.x + w * (float)left + cw * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cx - cw * 0.5f, at.y));
        if (ImGui::InvisibleButton("##centre", ImVec2(cw, size.y))) tapped = kCentreTapped;
        const bool held = ImGui::IsItemActive();
        const float r = std::min(cw * 0.36f, size.y * 0.36f) * (held ? 0.94f : 1.0f);
        const ImVec2 c(cx, at.y + size.y * 0.47f);
        dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.08f), r * 1.04f, IM_COL32(0, 0, 0, 40)); // shadow
        if (face) {
            face(dl, c, r, centre_on, held);
        } else {
            const ImVec4 acc = ImGui::GetStyle().Colors[ImGuiCol_CheckMark];
            const float k = centre_on ? 1.0f : 0.85f;
            dl->AddCircleFilled(c, r, ImGui::GetColorU32(ImVec4(acc.x * k, acc.y * k, acc.z * k, 1.0f)), 48);
        }
    }
    ImGui::End();
    return tapped;
}

bool begin_app_bar(const char* title, bool can_back) {
    bool back = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginViewportSideBar("##maiz-appbar", ImGui::GetMainViewport(), ImGuiDir_Up, app_bar_height(),
                                kBarFlags);
    ImGui::PopStyleVar();
    ImGui::AlignTextToFramePadding();
    if (can_back) {
        // a wide target: the chevron is small, the finger is not. Drawn as a
        // path, so it needs no icon font.
        const float fh = ImGui::GetFrameHeight();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        back = ImGui::InvisibleButton("##back", ImVec2(fh * 1.4f, fh));
        const ImU32 col = ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_CheckMark : ImGuiCol_Text);
        const float s = fh * 0.22f;
        const ImVec2 c(p.x + fh * 0.6f, p.y + fh * 0.5f);
        const ImVec2 pts[3] = {ImVec2(c.x + s * 0.5f, c.y - s), ImVec2(c.x - s * 0.5f, c.y),
                               ImVec2(c.x + s * 0.5f, c.y + s)};
        ImGui::GetWindowDrawList()->AddPolyline(pts, 3, col, 0, std::max(1.5f, fh * 0.08f));
        ImGui::SameLine();
    }
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * kTitleScale);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    return back;
}

void end_app_bar() {
    // the bar's lower edge, like the navigation bar's upper one
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    dl->AddLine(ImVec2(p.x, p.y + s.y - 1), ImVec2(p.x + s.x, p.y + s.y - 1), ImGui::GetColorU32(ImGuiCol_Border));
    ImGui::End();
}

bool back_pressed() {
    const ImGuiIO& io = ImGui::GetIO();
    const bool asked = ImGui::IsKeyPressed(ImGuiKey_AppBack, false) || ImGui::IsMouseClicked(3) ||
                       default_touch_gate().gestures.back ||
                       (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false));
    if (!asked) return false;
    const int frame = ImGui::GetFrameCount();
    if (g_back_frame == frame && g_back_claimed) return false; // one Back, one taker
    g_back_frame = frame;
    g_back_claimed = true;
    return true;
}

void unclaim_back() {
    if (g_back_frame == ImGui::GetFrameCount()) g_back_claimed = false;
}

bool back_claimed() { return g_back_frame == ImGui::GetFrameCount() && g_back_claimed; }

} // namespace maiz
