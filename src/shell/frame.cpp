/* frame.cpp — the frame order every shell shares (frame.hpp). */
#include "frame.hpp"

#include "voidmaiz/mobile.hpp" // reserve_safe_area, the touch gate
#include "voidmaiz/nav.hpp"    // back_claimed

#include "imgui.h"

namespace maiz::shell {

Runner::Runner(Device& device) : device_(device) {}

void Runner::create() {
    install_device(&device_);
    TouchGate& gate = default_touch_gate();
    gate.edge_back_dp = device_.edge_back() ? 20.0f : 0.0f;
    app_ = create_app();
    if (!app_) return;
    ImGuiIO& io = ImGui::GetIO();
    app_->fonts(device_);
    if (io.Fonts->Fonts.empty()) {
        ImFontConfig cfg;
        cfg.SizePixels = 15.0f * device_.density();
        io.Fonts->AddFontDefault(&cfg);
    }
}

void Runner::start() {
    if (started_ || !app_) return;
    started_ = true;
    app_->start(device_);
}

bool Runner::frame() {
    if (!app_) return false;
    if (!started_) start();
    std::vector<DocumentResult> docs;
    docs.swap(pending_);
    for (const auto& d : docs) app_->document(device_, d);

    text_input_frame(text_, device_.text_input()); // first: its own header says so
    reserve_safe_area(device_.safe_area());         // before any bar, so bars land inside it
    TouchGate& gate = default_touch_gate();
    touch_gate_frame(gate, device_.density());
    app_->frame(device_);

    const bool asked = ImGui::IsKeyPressed(ImGuiKey_AppBack, false) || gate.gestures.back;
    return asked && !back_claimed();
}

void Runner::event(AppEvent e) {
    if (app_ && started_) app_->event(device_, e);
}

void Runner::document(DocumentResult r) { pending_.push_back(std::move(r)); }

} // namespace maiz::shell
