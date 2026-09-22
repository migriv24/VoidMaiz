/* updateview.cpp — what updating looks like (voidmaiz/updateview.hpp). */
#include "voidmaiz/updateview.hpp"

#include "voidmaiz/mobile.hpp"  // dim_wrapped
#include "voidmaiz/widgets.hpp" // tool_button

#include "imgui.h"

#include <algorithm>

namespace maiz {

using update::Ask;
using update::Updater;

namespace {

/* Modals sized to the screen they are on: a phone gets nearly all of it. */
void size_modal(bool touch, float desktop_w) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float w = touch ? vp->WorkSize.x * 0.92f : std::min(desktop_w, vp->WorkSize.x * 0.9f);
    ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

bool button(const char* label, bool touch) { return tool_button(label, touch); }

} // namespace

void open_update_prompt(Updater& u, UpdateViewState& v, bool check_now) {
    v.prompt_open = true;
    v.message.clear();
    if (check_now && !u.busy() && u.stage() != Updater::Stage::Ready) u.begin_check();
}

bool update_badge(Updater& u, UpdateViewState& v, bool touch) {
    if (u.stage() != Updater::Stage::Offered && u.stage() != Updater::Stage::Ready) return false;
    std::string label = "update " + u.offer().release.version;
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (tool_button(label.c_str(), touch)) open_update_prompt(u, v, false);
    ImGui::PopStyleColor();
    return true;
}

void draw_update_settings(Updater& u, UpdateViewState& v) {
    ImGui::TextUnformatted("Check for updates");
    int ask = (int)u.prefs().ask;
    // the three answers, in the words the consent question used
    if (ImGui::RadioButton("when the app starts", ask == (int)Ask::Startup)) u.set_ask(Ask::Startup);
    if (ImGui::RadioButton("never", ask == (int)Ask::Never)) u.set_ask(Ask::Never);
    if (ask == (int)Ask::Unasked) ImGui::TextDisabled("(not asked yet: you will be asked once)");
    ImGui::BeginDisabled(u.busy());
    if (ImGui::Button("Check now")) open_update_prompt(u, v, true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("you have %s", u.self().version.c_str());
    if (!u.prefs().last_checked.empty()) ImGui::TextDisabled("last checked %s", u.prefs().last_checked.c_str());
}

UpdateChoice draw_update_modals(Updater& u, UpdateViewState& v, bool touch) {
    UpdateChoice choice = UpdateChoice::None;
    u.poll();

    // ── the one-time question: rule 1, asked before the first request ─────────
    if (u.needs_consent() && !v.consent_dismissed && !ImGui::IsPopupOpen("##update-consent"))
        ImGui::OpenPopup("##update-consent");
    size_modal(touch, 460.0f);
    if (ImGui::BeginPopupModal("##update-consent", nullptr,
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Check for updates when %s starts?", u.self().display.c_str());
        dim_wrapped("It asks one file on the internet which version is newest. Nothing is downloaded or"
                    " installed unless you choose it.");
        ImGui::Spacing();
        if (button("Yes, check", touch)) {
            u.set_ask(Ask::Startup);
            u.begin_check();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (button("Not now", touch)) {
            v.consent_dismissed = true; // ask again next launch
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (button("Never", touch)) {
            u.set_ask(Ask::Never);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── the prompt ────────────────────────────────────────────────────────────
    if (v.prompt_open && !ImGui::IsPopupOpen("Updates")) ImGui::OpenPopup("Updates");
    size_modal(touch, 560.0f);
    if (ImGui::BeginPopupModal("Updates", &v.prompt_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        const update::Offer& o = u.offer();
        switch (u.stage()) {
        case Updater::Stage::Idle:
            ImGui::TextDisabled("Not checked yet.");
            if (button("Check now", touch)) u.begin_check();
            break;
        case Updater::Stage::Checking:
            ImGui::TextUnformatted("Checking for updates...");
            break;
        case Updater::Stage::UpToDate:
            if (o.skipped)
                ImGui::TextWrapped("%s is available, and you chose not to install it. A newer release will ask again.",
                                   o.release.version.c_str());
            else
                ImGui::TextWrapped("You have the newest version (%s).", u.self().version.c_str());
            break;
        case Updater::Stage::Offered: {
            // the release's own words, including what behaves differently:
            // "never silently" means the person reads what changes
            std::string text = update::describe(o);
            float h = std::min(ImGui::GetTextLineHeightWithSpacing() * 14.0f,
                               ImGui::GetMainViewport()->WorkSize.y * 0.5f);
            ImGui::BeginChild("##notes", ImVec2(0, h), ImGuiChildFlags_Borders);
            ImGui::TextWrapped("%s", text.c_str());
            ImGui::EndChild();
            if (o.release.signature.empty())
                dim_wrapped("Checked by its sha256 digest. Not yet signed.");
            if (button("Download", touch)) u.begin_download();
            ImGui::SameLine();
            if (button("Not this one", touch)) u.skip_offered();
            ImGui::SameLine();
            if (button("Later", touch)) {
                v.prompt_open = false;
                ImGui::CloseCurrentPopup();
            }
            break;
        }
        case Updater::Stage::Downloading:
            ImGui::TextWrapped("Downloading %s...", o.release.file.c_str());
            break;
        case Updater::Stage::Ready:
            ImGui::TextWrapped("%s is downloaded and its digest matches.", o.release.version.c_str());
            if (!v.message.empty()) ImGui::TextWrapped("%s", v.message.c_str());
            if (button("Install", touch)) choice = UpdateChoice::Apply;
            ImGui::SameLine();
            if (button("Later", touch)) {
                v.prompt_open = false;
                ImGui::CloseCurrentPopup();
            }
            break;
        case Updater::Stage::Failed:
            ImGui::TextWrapped("That did not work: %s", u.error().c_str());
            if (button("Try again", touch)) u.begin_check();
            break;
        }
        if (u.stage() != Updater::Stage::Offered && u.stage() != Updater::Stage::Ready) {
            if (button("Close", touch)) {
                v.prompt_open = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    return choice;
}

} // namespace maiz
