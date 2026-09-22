/*
 * voidmaiz/updateview.hpp — what updating looks like, the same in every app.
 *
 * okf/concepts/updates.md. The view half of voidmaiz/update.hpp, and the reason
 * updating belongs in a UI library: the two rules (never check unasked, never
 * install untold) are only as good as the screens that ask, and an application
 * that draws its own consent question is an application that can forget to.
 *
 * Three pieces a host places, and one it calls:
 *   - update_badge: a small button in the toolbar or app bar, shown only while
 *     an update is offered. The update never interrupts; it waits to be noticed.
 *   - draw_update_settings: a section for the host's settings window.
 *   - open_update_prompt: a menu item's "Check for updates…".
 *   - draw_update_modals, once a frame: the one-time consent question and the
 *     prompt. It returns Apply when a VERIFIED download is ready and the person
 *     pressed Install; the host then calls update::apply, because only the host
 *     knows its platform handle and when it is safe to quit.
 *
 * View-module header (ImGui types): only for targets linking voidmaiz_view.
 */
#pragma once

#include "voidmaiz/update.hpp"

#include <string>

namespace maiz {

struct UpdateViewState {
    bool prompt_open = false;
    bool consent_dismissed = false; // "Not now": ask again next launch, not this session
    std::string message;            // the last apply result, shown in the prompt
};

enum class UpdateChoice { None, Apply };

UpdateChoice draw_update_modals(update::Updater& u, UpdateViewState& v, bool touch);

/* Returns true when it drew (an update is offered or ready). */
bool update_badge(update::Updater& u, UpdateViewState& v, bool touch);

void draw_update_settings(update::Updater& u, UpdateViewState& v);

void open_update_prompt(update::Updater& u, UpdateViewState& v, bool check_now);

} // namespace maiz
