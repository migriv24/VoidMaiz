/* voidmaiz/nav.hpp — a phone's navigation: the bottom navigation bar, the app
 * bar, Back from wherever the platform puts it, and a stack of screens per
 * destination.
 *
 * Lifted from Void Hormiga (src/phone/nav.hpp, 2026-09-23), which wrote it to
 * be lifted and asked for it: the author placed it here in so many words, *"a
 * navigation bar (or at least, the core concept of one) belongs to maiz"*.
 * Interaction Combinators' File/Edit/View phone layout is the case they named
 * as wrong for a real app, and the second client.
 *
 * What changed on the way in: no ant (a centred button takes a face callback),
 * no icon font required (an item may draw its own icon, and the back chevron
 * is a path, not a glyph), and Back is gathered from every platform's source,
 * including iOS's edge swipe, into one `back_pressed()`.
 *
 * THE ONE TRICK (Hormiga's): both bars RESERVE their edge of the main viewport,
 * the way ImGui's own main menu bar reserves the top (`BeginViewportSideBar`).
 * Every window placed after them reads a smaller `WorkSize`, including the FAB
 * and the snackbar (mobile.hpp), which anchor to the work area, so they sit
 * above the navigation bar without either knowing it exists. The work area
 * updates on the next frame, exactly as for the menu bar. Call the bars after
 * reserve_safe_area and before any other window.
 *
 * View-module header (ImGui types).
 */
#pragma once

#include "imgui.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace maiz {

/* ── the bottom navigation bar ───────────────────────────────────────────────
 * Three to five destinations, each an icon over a one-word label, the current
 * one in the accent colour. Material's navigation bar and iOS's tab bar are the
 * same idea at the same place: where a thumb reaches on a tall phone. */
struct NavItem {
    const char* icon = nullptr;  // text in the host's icon font, or null
    const char* label = "";      // one word
    int badge = 0;               // a count drawn on the icon (0 = none)
    /* Draws the icon instead of `icon`, centred on `c` and about `size` tall.
     * For a host with no icon font (the library's own examples). */
    std::function<void(ImDrawList* dl, ImVec2 c, float size, ImU32 col)> draw;
};

float nav_bar_height();

/* Returns true the frame the selection changes. A second tap on the current
 * item also returns true: the platform's "back to the top of this
 * destination", which a host does by resetting that destination's Stack. */
bool nav_bar(const std::vector<NavItem>& items, int& current);

/* The same bar with one fixed button in the middle (Hormiga's: the bar is the
 * person's to arrange, except one button that always opens everything).
 * `items` are the slots left to right; the centre sits after the first half.
 * `current` is a slot, or -1 when the screen on show is not on the bar. Returns
 * the slot tapped, kCentreTapped, or -1. `face` draws the centre button. */
inline constexpr int kCentreTapped = -2;
using CentreFace = std::function<void(ImDrawList* dl, ImVec2 c, float radius, bool on, bool held)>;
int nav_bar_centred(const std::vector<NavItem>& items, int current, bool centre_on, const CentreFace& face);

/* ── the app bar ─────────────────────────────────────────────────────────────
 * A back chevron when `can_back`, then the title. Returns true when the chevron
 * is tapped. Between begin and end a host may draw trailing items (SameLine
 * first). Always pair with end_app_bar.
 *
 * ON iOS THE CHEVRON IS NOT OPTIONAL. There is no system Back button, and the
 * edge swipe is undiscoverable, so a screen that can go back must show the
 * chevron. Pass can_back = stack.can_pop() and it is right everywhere. */
bool begin_app_bar(const char* title, bool can_back);
void end_app_bar();

/* ── Back, from wherever the platform puts it ────────────────────────────────
 * True once in the frame Back was asked for, by any of:
 *   - Android's Back key or back gesture (the shell turns it into
 *     ImGuiKey_AppBack; the stock ImGui android backend does NOT map it);
 *   - iOS's swipe in from the left edge (the touch gate's `gestures.back`);
 *   - a desktop's Escape (never while a field has the keyboard, where Escape
 *     means "stop typing") and the mouse's back button.
 * Calling it CLAIMS the Back: a host that pops a screen calls it, and a shell
 * that finds nobody claimed one does the platform's default (Android sends the
 * app to the background; iOS does nothing, because an iOS app never exits).
 * A host that calls it and then decides not to go back calls unclaim_back(). */
bool back_pressed();
void unclaim_back();
bool back_claimed(); // this frame; for the shell, after the app's frame

/* ── a stack of screens per destination ──────────────────────────────────────
 * Switching destinations and coming back keeps your place, as every phone
 * does. A route is a string the host parses ("list", "detail:maria-3fa9-1").
 * View state: it lives in the host, never in the document. */
struct Stack {
    std::vector<std::string> routes;
    explicit Stack(std::string root = "list") : routes{std::move(root)} {}
    const std::string& top() const { return routes.back(); }
    bool can_pop() const { return routes.size() > 1; }
    void push(std::string r) { routes.push_back(std::move(r)); }
    bool pop() {
        if (!can_pop()) return false;
        routes.pop_back();
        return true;
    }
    void reset() { routes.resize(1); }
};

} // namespace maiz
