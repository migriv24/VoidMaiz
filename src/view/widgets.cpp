/* widgets.cpp — log strip + command bar + the docking workspace helpers. */
#include "voidmaiz/widgets.hpp"

#include "imgui.h"
#include "imgui_internal.h" // ImGuiDockNode — dockspace_needs_seed inspects it

#include <string_view>

namespace maiz {

namespace {

std::string format_line(const LogEntry& e) {
    return "[" + e.level + "] " + (e.op.empty() ? "" : e.op + ": ") + e.msg;
}

/* Is this line part of the structural story? Heuristic over the line's text
 * (echo entries carry the command in op; core lines name the verb there). */
bool log_line_major(const LogEntry& e) {
    if (e.level == "error" || e.level == "warn") return true;
    std::string text = e.op + " " + e.msg;
    std::string_view s = text;
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    std::string_view verb = s.substr(0, s.find(' '));
    auto is_one_of = [&](std::initializer_list<std::string_view> vs) {
        for (auto v : vs)
            if (verb == v) return true;
        return false;
    };
    if (!is_one_of({"rune", "rm", "link", "unlink", "batch", "tag", "mantle", "glyph",
                    "undo", "redo", "set", "setjson", "use"}))
        return false;
    if (verb == "batch") // move-only batches are view noise; structural ones stay
        return text.find("link") != std::string::npos ||
               text.find("rune") != std::string::npos ||
               text.find("rm ") != std::string::npos ||
               text.find("\"set ") != std::string::npos;
    if (verb == "set" || verb == "setjson") {
        // "set(json) <node> <field> …" — pure view-state fields don't condense
        size_t a = text.find(' ');
        size_t b = a == std::string::npos ? a : text.find(' ', a + 1);
        size_t c = b == std::string::npos ? b : text.find(' ', b + 1);
        if (b != std::string::npos) {
            std::string_view field(text.c_str() + b + 1,
                                   (c == std::string::npos ? text.size() : c) - b - 1);
            if (field == "pos" || field == "size" || field == "collapsed") return false;
        }
    }
    return true;
}

} // namespace

std::string log_to_text(const std::vector<LogEntry>& entries, bool condensed) {
    std::string out;
    for (const auto& e : entries) {
        if (condensed && !log_line_major(e)) continue;
        out += format_line(e);
        out += '\n';
    }
    return out;
}

void draw_log_strip(const std::vector<LogEntry>& entries) {
    for (const auto& e : entries) {
        // level colors chosen to read on both light and dark ImGui styles;
        // plain info lines use the style's own text color
        bool colored = true;
        ImVec4 col;
        if (e.level == "error") col = ImVec4(0.85f, 0.28f, 0.22f, 1.0f);
        else if (e.level == "warn") col = ImVec4(0.72f, 0.53f, 0.08f, 1.0f);
        else if (e.level == ">") col = ImVec4(0.25f, 0.45f, 0.85f, 1.0f);
        else colored = false;
        if (colored) ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(format_line(e).c_str());
        if (colored) ImGui::PopStyleColor();
    }
    // follow the tail ONLY while the user is at it — scrolling up to read
    // history must stick (an unconditional pin fights the wheel and wins)
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);
    // right-click anywhere on the strip → copy options (the transcript is the
    // session; it should leave the app as easily as it reads)
    if (ImGui::BeginPopupContextWindow("vm-log-copy")) {
        if (ImGui::MenuItem("copy condensed"))
            ImGui::SetClipboardText(log_to_text(entries, true).c_str());
        if (ImGui::MenuItem("copy all"))
            ImGui::SetClipboardText(log_to_text(entries, false).c_str());
        ImGui::EndPopup();
    }
}

namespace {
int history_cb(ImGuiInputTextCallbackData* data) {
    auto* st = static_cast<CommandBarState*>(data->UserData);
    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory) return 0;
    int prev = st->hist_pos;
    if (data->EventKey == ImGuiKey_UpArrow) {
        if (st->hist_pos == -1) st->hist_pos = (int)st->history.size() - 1;
        else if (st->hist_pos > 0) --st->hist_pos;
    } else if (data->EventKey == ImGuiKey_DownArrow) {
        if (st->hist_pos != -1 && ++st->hist_pos >= (int)st->history.size()) st->hist_pos = -1;
    }
    if (prev != st->hist_pos) {
        const char* s = st->hist_pos >= 0 ? st->history[st->hist_pos].c_str() : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, s);
    }
    return 0;
}
} // namespace

SplitterResult splitter(const char* str_id, bool vertical, float& frac, float span,
                        float min_frac, float max_frac, float thickness, float length) {
    SplitterResult r;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float len = length > 0.0f ? length : (vertical ? avail.y : avail.x);
    ImVec2 size = vertical ? ImVec2(thickness, len > 0 ? len : 1.0f)
                           : ImVec2(len > 0 ? len : 1.0f, thickness);
    ImGui::InvisibleButton(str_id, size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();
    if (hovered || active)
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW
                                       : ImGuiMouseCursor_ResizeNS);
    if (active && span > 1.0f) {
        float d = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
        if (d != 0.0f) {
            float next = frac + d / span;
            next = next < min_frac ? min_frac : (next > max_frac ? max_frac : next);
            if (next != frac) {
                frac = next;
                r.dragging = true;
            }
        }
    }
    if (ImGui::IsItemDeactivated()) r.released = true;
    // a subtle grip line, brighter under the cursor
    ImVec2 p0 = ImGui::GetItemRectMin(), p1 = ImGui::GetItemRectMax();
    ImU32 col = ImGui::GetColorU32(active    ? ImGuiCol_SeparatorActive
                                   : hovered ? ImGuiCol_SeparatorHovered
                                             : ImGuiCol_Separator);
    float mid = vertical ? (p0.x + p1.x) * 0.5f : (p0.y + p1.y) * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (vertical)
        dl->AddLine(ImVec2(mid, p0.y), ImVec2(mid, p1.y), col, 2.0f);
    else
        dl->AddLine(ImVec2(p0.x, mid), ImVec2(p1.x, mid), col, 2.0f);
    return r;
}

void apply_touch_metrics(float scale) {
    if (scale < 1.0f) scale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    // finger-sized metrics BEFORE the scale so they scale with everything else
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(10, 8);
    style.ScrollbarSize = 18.0f;
    style.GrabMinSize = 18.0f;
    style.TouchExtraPadding = ImVec2(4, 4); // forgiveness around every widget
    style.ScaleAllSizes(scale);
    ImGui::GetIO().FontGlobalScale = scale;
    // taps are sloppier than clicks: a double-tap's two touches land apart
    ImGui::GetIO().MouseDoubleClickMaxDist = 48.0f;
    ImGui::GetIO().MouseDoubleClickTime = 0.35f;
}

bool tool_button(const char* label, bool touch) {
    return touch ? ImGui::Button(label) : ImGui::SmallButton(label);
}

CanvasIO draw_command_bar(CommandBarState& st) {
    CanvasIO out;
    ImGui::SetNextItemWidth(-52.0f);
    bool entered = ImGui::InputTextWithHint(
        "##vn-cmd", "command…  (Enter: dispatch, Up/Down: history)", st.buf, sizeof st.buf,
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
        history_cb, &st);
    ImGui::SameLine();
    bool run = ImGui::Button("run");
    if ((entered || run) && st.buf[0]) {
        out.commands.emplace_back(st.buf);
        st.history.emplace_back(st.buf);
        st.hist_pos = -1;
        st.buf[0] = '\0';
        if (entered) ImGui::SetKeyboardFocusHere(-1); // stay in the bar
    }
    return out;
}

// ── docking workspace ────────────────────────────────────────────────────────

void enable_docking(bool on) {
    ImGuiIO& io = ImGui::GetIO();
#ifdef IMGUI_HAS_DOCK
    if (on)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    else
        io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
    // Multi-viewport is deliberately NOT touched: DockSpace works on one OS
    // surface (desktop and NDK alike); viewports would break the mobile path.
#else
    (void)on; // vendored ImGui without docking: a no-op, host code unchanged
#endif
}

unsigned begin_dockspace(const char* str_id) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    // A borderless, input-passthrough host window that is never itself a dock
    // target and never eats gestures — panels dock INTO its DockSpace.
    ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::Begin(str_id, nullptr, host_flags);
    ImGui::PopStyleVar(3);

    unsigned dock_id = 0;
#ifdef IMGUI_HAS_DOCK
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        dock_id = ImGui::GetID(str_id);
        ImGui::DockSpace(dock_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    }
#endif
    return dock_id;
}

void end_dockspace() { ImGui::End(); }

bool dockspace_needs_seed(unsigned dock_id) {
    if (!dock_id) return false;
#ifdef IMGUI_HAS_DOCK
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id);
    if (!node) return true; // docking on, nothing built yet
    // begin_dockspace() just created this node, so "exists" proves nothing. A
    // real layout is split, or has windows docked into it; a bare central node
    // is the empty one DockSpace() mints every first run.
    return !node->IsSplitNode() && node->Windows.Size == 0;
#else
    return false; // no docking in this ImGui build: nothing to seed
#endif
}

} // namespace maiz
