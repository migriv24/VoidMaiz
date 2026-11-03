/* portable_app.cpp — one Void Maiz application, written once, for every device.
 *
 * This file builds, unchanged, as:
 *   - a desktop window:              maiz_portable
 *   - a phone preview on a desktop:  maiz_portable --device iphone-16
 *   - an iOS app (CI builds it for the Simulator; cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS)
 *   - an Android .so for the library's NativeActivity shell
 * It is the reference a host copies when it ports (okf/concepts/portable-apps.md):
 * three screens under a navigation bar, a stack per screen with Back from
 * wherever the platform puts it, a document saved when the phone takes the app
 * away, and every service asked of the Device rather than of a platform.
 *
 * It is an ordinary Void Maiz host: the model is a Void Core Session, every
 * change is a dispatched command, and the screens draw projections. */
#include "voidmaiz/app.hpp"
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/headless.hpp"
#include "voidmaiz/location.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/nav.hpp"
#include "voidmaiz/project.hpp"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

maiz::HostApp host_app() {
    maiz::HostApp a;
    a.id = "maiz-portable";
    a.label = "Void Maiz";
    a.version = "0.1.0";
    a.glyphs = {
        R"({"glyph":"note","label":"Note","fields":["text"],"hints":{"color":"#5a4fcf","face":{"w":150}}})",
        R"({"glyph":"place","label":"Place","fields":["where"],"hints":{"color":"#2f7d5a",)"
        R"("ports":[{"name":"prin","principal":true},{"name":"near","dir":"in"}]}})",
    };
    return a;
}

// three icons the library's default font does not have, drawn instead
void icon_graph(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    const float r = s * 0.14f;
    const ImVec2 a(c.x - s * 0.3f, c.y + s * 0.2f), b(c.x + s * 0.3f, c.y + s * 0.2f), t(c.x, c.y - s * 0.28f);
    dl->AddLine(a, t, col, 1.5f);
    dl->AddLine(b, t, col, 1.5f);
    for (ImVec2 p : {a, b, t}) dl->AddCircleFilled(p, r, col);
}
void icon_list(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    for (int i = -1; i <= 1; ++i) {
        const float y = c.y + i * s * 0.26f;
        dl->AddCircleFilled(ImVec2(c.x - s * 0.32f, y), s * 0.06f, col);
        dl->AddLine(ImVec2(c.x - s * 0.18f, y), ImVec2(c.x + s * 0.36f, y), col, 2.0f);
    }
}
void icon_device(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    dl->AddRect(ImVec2(c.x - s * 0.22f, c.y - s * 0.38f), ImVec2(c.x + s * 0.22f, c.y + s * 0.38f), col, s * 0.08f,
                0, 1.6f);
    dl->AddLine(ImVec2(c.x - s * 0.07f, c.y + s * 0.28f), ImVec2(c.x + s * 0.07f, c.y + s * 0.28f), col, 1.6f);
}

class PortableApp : public maiz::App {
  public:
    void fonts(maiz::Device& d) override {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        if (d.platform() != maiz::Platform::Desktop) { // finger-sized, before the scale
            style.FramePadding = ImVec2(10, 8);
            style.ItemSpacing = ImVec2(10, 10);
            style.ScrollbarSize = 6.0f; // a phone's scroll bar is an indicator, not a handle
            style.TouchExtraPadding = ImVec2(4, 4);
        }
        style.ScaleAllSizes(d.density());
    }

    void start(maiz::Device& d) override {
        maiz::SessionOptions o;
        o.state_path = (d.data_dir() / "portable.state.json").string();
        o.actor = "human:" + std::string(maiz::platform_name(d.platform()));
        o.kind = maiz::SessionKind::Human;
        /* A phone runs one copy of an app, and kills it without a word, so a
         * lock there only ever guards against the app's own last life. The
         * lock is for a desktop's two windows and its agents. */
        o.lock = d.platform() == maiz::Platform::Desktop && !d.preview();
        session_ = std::make_unique<maiz::Session>(app_, o);
        if (!session_->start()) {
            status_ = "could not open the document: " + session_->error();
            return;
        }
        if (maiz::project_scene(session_->core()).nodes.empty()) seed();
        reproject();
        style_.theme = maiz::CanvasTheme::dark();
        ed_.cam = {-30, -30, 1.0f};
        log_event("start");
    }

    void frame(maiz::Device& d) override {
        const std::vector<maiz::NavItem> items = {
            {nullptr, "Graph", 0, icon_graph},
            {nullptr, "List", (int)scene_.nodes.size(), icon_list},
            {nullptr, "Device", 0, icon_device},
        };
        if (maiz::nav_bar(items, tab_) && tab_ == last_tab_) stacks_[tab_].reset(); // second tap: to the top
        last_tab_ = tab_;
        maiz::Stack& stack = stacks_[tab_];

        // Back, from Android's key, iOS's edge swipe or a desktop's Escape
        if (stack.can_pop() && maiz::back_pressed()) stack.pop();

        const std::string route = stack.top();
        const bool detail = route.rfind("detail:", 0) == 0;
        const char* title = detail ? "Note" : items[tab_].label;
        if (maiz::begin_app_bar(title, stack.can_pop())) stack.pop();
        maiz::end_app_bar();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##screen", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();
        // an edge swipe in flight slides the screen, as iOS's does
        if (const float p = maiz::default_touch_gate().edge_progress; p > 0.0f)
            ImGui::GetWindowDrawList()->AddRectFilled(vp->WorkPos, ImVec2(vp->WorkPos.x + vp->WorkSize.x * p,
                                                                          vp->WorkPos.y + vp->WorkSize.y),
                                                      IM_COL32(0, 0, 0, 60));
        if (!session_) {
            ImGui::TextWrapped("%s", status_.c_str());
        } else if (tab_ == 0) {
            graph_screen();
        } else if (tab_ == 1) {
            if (detail) note_screen(route.substr(7), stack);
            else list_screen(stack, d);
        } else {
            device_screen(d);
        }
        ImGui::End();
        if (maiz::draw_snackbar(snack_) && snack_.action == "UNDO") dispatch("undo"); // the only action we offer
    }

    void event(maiz::Device&, maiz::AppEvent e) override {
        log_event(maiz::app_event_name(e));
        // THE ONE THAT MATTERS: a phone may end the process after this without
        // another word, so the document is written now
        if ((e == maiz::AppEvent::Background || e == maiz::AppEvent::Terminating) && session_) {
            if (session_->save()) log_event("saved");
        }
    }

    void document(maiz::Device&, const maiz::DocumentResult& r) override {
        log_event("document " + r.status + " " + r.name);
    }

  private:
    void seed() {
        for (const char* c : {"mantle new portable", "use portable", "rune new note welcome", R"(set welcome text "one app, every device")",
                              "setjson welcome pos [20,20]", "rune new place market",
                              R"(set market where "the corner by the bakery")", "setjson market pos [40,180]",
                              "link welcome market --relation 0:1"})
            session_->dispatch(c);
    }
    void reproject() { scene_ = maiz::project_scene(session_->core()); }
    void dispatch(const std::string& cmd) {
        maiz::Result r = session_->dispatch(cmd);
        reproject();
        if (!r.ok) maiz::show_snackbar(snack_, "failed: " + r.text());
    }
    void log_event(const std::string& what) {
        events_.push_back(what);
        if (events_.size() > 12) events_.erase(events_.begin());
    }

    void graph_screen() {
        // a pinch zooms the graph; the release writes ONE camera command
        const maiz::TouchGate& g = maiz::default_touch_gate();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        if (g.pinch.active) {
            maiz::camera_pan(ed_.cam, g.pinch.dx, g.pinch.dy);
            maiz::camera_pinch(ed_.cam, g.pinch.scale, g.pinch.cx, g.pinch.cy, origin.x, origin.y);
            pinching_ = true;
        } else if (pinching_) {
            pinching_ = false;
            dispatch(maiz::compile_camera(ed_.cam));
        }
        maiz::CanvasIO io = maiz::edit_canvas("graph", scene_, ed_, style_);
        for (const auto& c : io.commands) dispatch(c);
    }

    void list_screen(maiz::Stack& stack, maiz::Device& d) {
        for (const auto& n : scene_.nodes) {
            const std::string text = maiz::field_of(n, n.glyph == "place" ? "where" : "text");
            ImGui::PushID(n.id.c_str());
            if (ImGui::Selectable("##row", false, 0, ImVec2(0, ImGui::GetFrameHeight() * 1.6f)))
                stack.push("detail:" + n.name);
            ImGui::SameLine(ImGui::GetStyle().ItemSpacing.x);
            ImGui::BeginGroup();
            ImGui::TextUnformatted(n.name.c_str());
            ImGui::TextDisabled("%s", text.empty() ? n.glyph.c_str() : text.c_str());
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::Spacing();
        if (ImGui::Button("+ note", ImVec2(-1, 0))) {
            char name[32];
            std::snprintf(name, sizeof name, "note-%d", ++made_);
            dispatch(std::string("rune new note ") + name);
            d.haptic(maiz::Haptic::Success);
        }
    }

    void note_screen(const std::string& name, maiz::Stack& stack) {
        const maiz::SceneNode* node = nullptr;
        for (const auto& n : scene_.nodes)
            if (n.name == name) node = &n;
        if (!node) {
            stack.pop();
            return;
        }
        const char* key = node->glyph == "place" ? "where" : "text";
        if (editing_ != name) {
            editing_ = name;
            std::snprintf(buf_, sizeof buf_, "%s", maiz::field_of(*node, key).c_str());
        }
        ImGui::TextDisabled("%s, a %s", name.c_str(), node->glyph.c_str());
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextMultiline("##text", buf_, sizeof buf_, ImVec2(-1, ImGui::GetFrameHeight() * 4));
        if (ImGui::IsItemDeactivatedAfterEdit()) // one command per finished edit
            dispatch("set " + name + " " + key + " " + maiz::arg(buf_));
        if (ImGui::Button("Delete", ImVec2(-1, 0))) {
            dispatch("rm " + name);
            maiz::show_snackbar(snack_, name + " deleted", "UNDO");
            stack.pop();
        }
    }

    void device_screen(maiz::Device& d) {
        const maiz::SafeArea s = d.safe_area();
        ImGui::Text("%s", d.model().c_str());
        ImGui::TextDisabled("platform %s%s, %.2f px per dp", maiz::platform_name(d.platform()),
                            d.preview() ? " (preview)" : "", d.density());
        ImGui::TextDisabled("safe area  L%.0f T%.0f R%.0f B%.0f px", s.left, s.top, s.right, s.bottom);
        ImGui::TextDisabled("can quit: %s   updates itself: %s   edge back: %s", d.can_quit() ? "yes" : "no",
                            d.self_updates() ? "yes" : "no", d.edge_back() ? "yes" : "no");
        ImGui::TextWrapped("data: %s", d.data_dir().string().c_str());
        ImGui::Separator();
        const float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Open a web page", ImVec2(w, 0)) && !d.open_url("https://github.com/migriv24/VoidMaiz"))
            maiz::show_snackbar(snack_, "no browser on this device");
        if (ImGui::Button("Share", ImVec2(w, 0)) && !d.share({"Made with Void Maiz", "", "", "", "Void Maiz"}))
            maiz::show_snackbar(snack_, "nothing to share with here");
        if (ImGui::Button("Haptic", ImVec2(w, 0))) d.haptic(maiz::Haptic::Tap);
        if (maiz::LocationPlatform* loc = maiz::location()) {
            if (ImGui::Button("Where am I?", ImVec2(w, 0))) loc->request(true);
            maiz::LocationFix fix;
            if (loc->latest(fix)) ImGui::TextDisabled("%.5f, %.5f (+/- %.0f m)", fix.lat, fix.lon, fix.accuracy_m);
            else ImGui::TextDisabled("location: %s", maiz::access_name(loc->access()));
        }
        if (d.can_quit() && ImGui::Button("Quit", ImVec2(w, 0))) d.quit(); // never drawn on iOS
        ImGui::Separator();
        ImGui::TextDisabled("what happened to the app:");
        for (const auto& e : events_) ImGui::BulletText("%s", e.c_str());
    }

    maiz::HostApp app_ = host_app();
    std::unique_ptr<maiz::Session> session_;
    maiz::Scene scene_;
    maiz::EditorState ed_;
    maiz::CanvasStyle style_;
    maiz::SnackbarState snack_;
    maiz::Stack stacks_[3] = {maiz::Stack("graph"), maiz::Stack("list"), maiz::Stack("device")};
    int tab_ = 1, last_tab_ = 1;
    bool pinching_ = false;
    int made_ = 0;
    std::string editing_, status_;
    char buf_[1024] = {};
    std::vector<std::string> events_;
};

} // namespace

std::unique_ptr<maiz::App> maiz::create_app() { return std::make_unique<PortableApp>(); }
