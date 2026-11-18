/* select_smoke.cpp — the list contract (voidmaiz/select.hpp, listselect.hpp,
 * okf/concepts/conventions.md): first the model rule, then a real ImGui list
 * driven by clicks and keys, headless. */
#include "voidmaiz/listselect.hpp"
#include "voidmaiz/select.hpp"

#include "imgui.h"

#include <iostream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(c)                                                                \
    do {                                                                        \
        if (!(c)) {                                                             \
            std::cerr << "FAIL line " << __LINE__ << ": " #c "\n";              \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

using namespace maiz;

namespace {

const std::vector<std::string> kItems = {"ana", "ben", "cai", "dee", "eli", "fay", "gus", "hal"};
const NameAt name_at = [](long i) { return kItems[(std::size_t)i]; };
const long kCount = (long)kItems.size();

std::string joined(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : ",") + x;
    return s;
}

// ── a real list, headless ────────────────────────────────────────────────────
std::vector<std::string> sel;
ListSelect ls;
ImVec2 row_pos[8];
bool changed_last = false;

void frame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(300, 400));
    ImGui::Begin("list", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ls.begin(sel, kCount, name_at);
    for (long i = 0; i < kCount; ++i) {
        const bool s = ls.item(i, kItems[(std::size_t)i]);
        ImGui::Selectable(kItems[(std::size_t)i].c_str(), s);
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        row_pos[i] = ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2);
    }
    changed_last = ls.end();
    ImGui::End();
    ImGui::Render();
}

void settle(int n = 3) {
    for (int i = 0; i < n; ++i) frame();
}

void click_at(ImVec2 p, bool ctrl = false, bool shift = false) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddKeyEvent(ImGuiMod_Shift, shift);
    io.AddMousePosEvent(p.x, p.y);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    settle();
}

void key(ImGuiKey k, bool ctrl = false) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddKeyEvent(k, true);
    frame();
    io.AddKeyEvent(k, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    settle();
}

} // namespace

int main() {
    // ── the model rule ───────────────────────────────────────────────────────
    {
        std::vector<std::string> s;
        long anchor = -1;
        CHECK(select_click(s, anchor, 2, {}, name_at, kCount) && joined(s) == "cai");
        CHECK(!select_click(s, anchor, 2, {}, name_at, kCount)); // the same click again: no change
        CHECK(select_click(s, anchor, 5, {true, false}, name_at, kCount) && joined(s) == "cai,dee,eli,fay");
        CHECK(select_click(s, anchor, 0, {false, true}, name_at, kCount) && joined(s) == "cai,dee,eli,fay,ana");
        CHECK(select_click(s, anchor, 3, {false, true}, name_at, kCount) && joined(s) == "cai,eli,fay,ana");
        // Ctrl+Shift adds the run from the last plain/Ctrl click (3) to 7, keeping the rest
        CHECK(select_click(s, anchor, 7, {true, true}, name_at, kCount) && joined(s) == "cai,eli,fay,ana,dee,gus,hal");
        CHECK(!select_click(s, anchor, 99, {}, name_at, kCount)); // off the list: nothing
        // shift upwards selects the same run
        anchor = 6;
        CHECK(select_click(s, anchor, 4, {true, false}, name_at, kCount) && joined(s) == "eli,fay,gus");
        CHECK(apply_select(s, {SelectRequest::Kind::All, 0, -1, true}, name_at, kCount) && s.size() == 8);
        CHECK(!apply_select(s, {SelectRequest::Kind::All, 0, -1, true}, name_at, kCount)); // already all
        CHECK(apply_select(s, {SelectRequest::Kind::Range, 6, 1, false}, name_at, kCount) && joined(s) == "ana,hal");
        CHECK(apply_select(s, {SelectRequest::Kind::All, 0, -1, false}, name_at, kCount) && s.empty());
    }

    // ── a real ImGui list ────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.Fonts->AddFontDefault();
    /* ImGui's macOS behaviour (on by default where __APPLE__ is defined) swaps Ctrl
     * and Cmd, so on a Mac the contract's Ctrl is Cmd: Cmd+click, Cmd+A. This test
     * presses Ctrl, so it asks for the same keys on every platform (found on the
     * macOS runner, 2026-11-18). */
    io.ConfigMacOSXBehaviors = false;
    settle();

    click_at(row_pos[1]);
    CHECK(joined(sel) == "ben");
    click_at(row_pos[4], false, true); // Shift: the run
    CHECK(joined(sel) == "ben,cai,dee,eli");
    click_at(row_pos[6], true); // Ctrl: one more
    CHECK(joined(sel) == "ben,cai,dee,eli,gus");
    click_at(row_pos[2], true); // Ctrl again: one fewer
    CHECK(joined(sel) == "ben,dee,eli,gus");
    click_at(row_pos[7]); // a plain click: just that one
    CHECK(joined(sel) == "hal");
    key(ImGuiKey_A, true); // Ctrl+A: all
    CHECK(sel.size() == 8);
    key(ImGuiKey_Escape); // Escape: none
    CHECK(sel.empty());
    click_at(row_pos[3]);
    CHECK(joined(sel) == "dee");
    click_at(ImVec2(150, 380)); // empty space under the rows: clicked away
    CHECK(sel.empty());
    // the host's selection, changed elsewhere (a canvas), is what the list shows
    // the same click from a touch screen (Void Maiz's touch gate reports a tap as one)
    {
        sel.clear();
        settle();
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        click_at(row_pos[2]);
        CHECK(joined(sel) == "cai");
        io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
    }
    sel = {"fay"};
    settle();
    click_at(row_pos[7], false, true); // Shift from... the list's own last click (dee) to hal
    CHECK(sel.size() >= 2 && sel.back() == "hal");

    ImGui::DestroyContext();
    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "OK - every list selects the same way\n";
    return failures ? 1 : 0;
}
