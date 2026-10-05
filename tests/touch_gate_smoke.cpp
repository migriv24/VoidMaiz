/* touch_gate_smoke.cpp — the touch gate's map gestures, on a real ImGui.
 *
 * The gate decides what a finger was before ImGui hears of it (mobile.hpp), so
 * it can only be tested with ImGui running: which window is under the finger
 * decides whether a drag scrolls, and what ImGui was told decides whether a
 * button clicked. Two windows stand in for a phone: a CANVAS (a map: nothing
 * scrolls, one button fills it) and a LIST that scrolls.
 *
 * Added 2026-10-04 with the gestures Void Hormiga's map needed: long press,
 * double tap, two-finger tap, quick zoom (tap, then drag) and a canvas fling.
 * The checks that matter most are the negative ones: a quick zoom never
 * presses the canvas, a two-finger tap never clicks, and a list still scrolls
 * where a canvas would zoom. */
#include "voidmaiz/location.hpp"
#include "voidmaiz/mobile.hpp"

#include "imgui.h"

#include <cmath>
#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz;

namespace {

TouchGate gate;
int clicks = 0;     // the canvas button's clicks
bool active = false; // the canvas button is held this frame
float list_scroll = 0;

// the canvas: x 0..400, the list: x 400..800 (both full height)
void frame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    touch_gate_frame(gate, 1.0f);
    const ImGuiWindowFlags fixed = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 600));
    ImGui::Begin("canvas", nullptr, fixed | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (ImGui::InvisibleButton("surface", ImGui::GetContentRegionAvail())) ++clicks;
    active = ImGui::IsItemActive();
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(400, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 600));
    ImGui::Begin("list", nullptr, fixed);
    for (int i = 0; i < 80; ++i) ImGui::Text("row %d", i);
    list_scroll = ImGui::GetScrollY();
    ImGui::End();
    ImGui::Render();
}

double now() { return ImGui::GetTime(); }

void frames(int n) {
    for (int i = 0; i < n; ++i) frame();
}

/* A tap: down, a frame, up, a frame. Returns the gestures of the frame after. */
TouchGate::Gestures tap(float x, float y) {
    touch_gate_down(gate, 0, x, y, now());
    frame();
    touch_gate_up(gate, 0, x, y, now());
    frame();
    return gate.gestures;
}

} // namespace

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // a renderer that never draws
    io.Fonts->AddFontDefault();
    frames(3);

    // ── a tap clicks, and says it was one tap ──
    {
        clicks = 0;
        auto g = tap(200, 300);
        frames(2);
        CHECK(g.taps == 1);
        CHECK(clicks == 1);
        frames(30); // past the double-tap window
    }
    // ── a double tap: both reach ImGui, the second says 2 ──
    {
        clicks = 0;
        auto g1 = tap(200, 300);
        auto g2 = tap(204, 302);
        frames(4); // ImGui trickles a press and its release over two frames
        CHECK(g1.taps == 1);
        CHECK(g2.taps == 2);
        CHECK(clicks == 2);
        frames(30);
    }
    // ── two taps far apart are two single taps ──
    {
        tap(100, 100);
        auto g = tap(300, 500);
        CHECK(g.taps == 1);
        frames(30);
    }
    // ── a long press: once, while the canvas holds a press; its release is not a tap ──
    {
        clicks = 0;
        int fired = 0;
        touch_gate_down(gate, 0, 200, 300, now());
        for (int i = 0; i < 45; ++i) {
            frame();
            if (gate.gestures.long_press) ++fired;
        }
        CHECK(fired == 1);
        CHECK(active); // the press is ImGui's: a drag from here moves what it holds
        touch_gate_up(gate, 0, 200, 300, now());
        frame();
        CHECK(gate.gestures.taps == 0);
        frames(30);
    }
    // ── a two-finger tap: reported, and nothing clicks ──
    {
        clicks = 0;
        touch_gate_down(gate, 0, 150, 300, now());
        touch_gate_down(gate, 1, 250, 300, now());
        frame();
        touch_gate_up(gate, 1, 250, 300, now());
        touch_gate_up(gate, 0, 150, 300, now());
        frame();
        CHECK(gate.gestures.two_finger_tap);
        CHECK(std::fabs(gate.gestures.x - 200) < 1);
        frames(3);
        CHECK(clicks == 0);
        frames(30);
    }
    // ── a pinch that moves is not a two-finger tap ──
    {
        touch_gate_down(gate, 0, 150, 300, now());
        touch_gate_down(gate, 1, 250, 300, now());
        frame();
        bool pinched = false;
        for (int k = 1; k <= 6; ++k) {
            touch_gate_move(gate, 1, 250 + k * 10.0f, 300, now());
            frame();
            pinched = pinched || (gate.pinch.active && gate.pinch.scale > 1.0f);
        }
        touch_gate_up(gate, 1, 310, 300, now());
        touch_gate_up(gate, 0, 150, 300, now());
        frame();
        CHECK(pinched);
        CHECK(!gate.gestures.two_finger_tap);
        frames(30);
    }
    // ── quick zoom: tap, then down and drag DOWN 160 px — the zoom doubles, and
    // the canvas never holds a press meanwhile ──
    {
        clicks = 0;
        tap(200, 200);
        touch_gate_down(gate, 0, 200, 200, now());
        frame();
        float scale = 1.0f;
        bool pressed = false;
        for (int k = 1; k <= 16; ++k) {
            touch_gate_move(gate, 0, 200, 200 + k * 10.0f, now());
            frame();
            if (gate.gestures.quick_zoom) scale *= gate.gestures.zoom_scale;
            pressed = pressed || active;
        }
        touch_gate_up(gate, 0, 200, 360, now());
        frame();
        CHECK(std::fabs(scale - 2.0f) < 0.05f);
        CHECK(!pressed);
        CHECK(clicks == 1); // the first tap only
        frames(30);
    }
    // ── on a list, tap-then-drag scrolls: a list never zooms ──
    {
        tap(600, 400);
        const float before = list_scroll;
        touch_gate_down(gate, 0, 600, 400, now());
        frame();
        bool zoomed = false;
        for (int k = 1; k <= 10; ++k) {
            touch_gate_move(gate, 0, 600, 400 - k * 15.0f, now());
            frame();
            zoomed = zoomed || gate.gestures.quick_zoom;
        }
        touch_gate_up(gate, 0, 600, 250, now());
        frames(2);
        CHECK(!zoomed);
        CHECK(list_scroll > before + 50);
        frames(120);
    }
    // ── a canvas drag released fast flings; a slow one that stops does not ──
    {
        touch_gate_down(gate, 0, 100, 300, now());
        frame();
        bool flung = false;
        float vx = 0;
        for (int k = 1; k <= 6; ++k) {
            touch_gate_move(gate, 0, 100 + k * 30.0f, 300, now());
            frame();
        }
        touch_gate_up(gate, 0, 280, 300, now());
        frame();
        flung = gate.gestures.fling;
        vx = gate.gestures.vx;
        CHECK(flung);
        CHECK(vx > 500);
        frames(30);

        touch_gate_down(gate, 0, 100, 300, now());
        frame();
        for (int k = 1; k <= 4; ++k) {
            touch_gate_move(gate, 0, 100 + k * 20.0f, 300, now());
            frame();
        }
        frames(10); // stopped before lifting: "stay here"
        touch_gate_up(gate, 0, 180, 300, now());
        frame();
        CHECK(!gate.gestures.fling);
        frames(30);
    }

    // ── the location stand-in: unasked until a request, and a refusal is kept ──
    {
        CHECK(location() == nullptr);
        auto fixed = std::make_unique<FixedLocation>();
        fixed->set(44.05, -123.09, 12.0f);
        install_location(std::move(fixed));
        LocationPlatform* loc = location();
        CHECK(loc != nullptr);
        LocationFix f;
        CHECK(loc->access() == LocationAccess::Unasked);
        CHECK(!loc->latest(f)); // nothing before anyone asked
        loc->request(true);
        CHECK(loc->access() == LocationAccess::Precise);
        CHECK(loc->latest(f) && std::fabs(f.lat - 44.05) < 1e-9 && f.accuracy_m == 12.0f);
        loc->stop();
        CHECK(!loc->latest(f));

        FixedLocation no;
        no.grant = LocationAccess::Denied;
        no.request(true);
        CHECK(no.access() == LocationAccess::Denied);
        CHECK(!no.following());
        CHECK(std::string(access_name(LocationAccess::Approximate)) == "approximate");
        install_location(nullptr);
    }

    ImGui::DestroyContext();
    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "touch_gate_smoke: all passed\n";
    return 0;
}
