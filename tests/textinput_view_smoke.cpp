/* textinput_view_smoke.cpp — the keyboard holiday driving a REAL ImGui field.
 *
 * textinput_smoke proves the mapping on a model of a field. This proves it on
 * the field itself: a headless ImGui context, one InputText declared as a phone
 * number, and a fake platform keyboard standing in for Android's. The fake sends
 * exactly what an input method sends (whole editing states, and the action
 * key), and the checks are about what the FIELD ends up holding, and about the
 * keyboard being told when the field changed without it.
 *
 * It is also the first test in this repository that runs ImGui at all, which is
 * why the setup below is spelled out: ImGui 1.92 manages its own font textures
 * when a "renderer" says it can, and a test renderer that never draws can say so. */
#include "voidmaiz/textinputview.hpp"

#include "imgui.h"

#include <cstring>
#include <iostream>
#include <string>

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

/* The platform keyboard, faked: it remembers what it was asked and plays back
 * what a person would have typed. */
struct FakeKeyboard : TextInputPlatform {
    int shows = 0, hides = 0, updates = 0;
    InputKind kind = InputKind::Text;
    EditingState seeded, last_update;
    std::vector<TextInputEvent> pending;

    void show(InputKind k, InputAction, const EditingState& st) override {
        ++shows;
        kind = k;
        seeded = st;
    }
    void update(const EditingState& st) override {
        ++updates;
        last_update = st;
    }
    void hide() override { ++hides; }
    void poll(std::vector<TextInputEvent>& out) override {
        for (auto& e : pending) out.push_back(e);
        pending.clear();
    }
    void type(const std::string& text, int cursor) {
        TextInputEvent e;
        e.type = TextInputEvent::Type::State;
        e.state.text = text;
        e.state.sel_start = e.state.sel_end = cursor;
        pending.push_back(e);
    }
    void action() {
        TextInputEvent e;
        e.type = TextInputEvent::Type::Action;
        pending.push_back(e);
    }
};

FakeKeyboard kb;
TextInputSession session;
char buf[256] = "";
bool committed = false;

void frame(bool focus = false) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    text_input_frame(session, &kb); // first, right after NewFrame, as documented
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::Begin("form");
    if (focus) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##phone", buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue)) committed = true;
    text_input_kind(InputKind::Phone);
    ImGui::End();
    ImGui::Render();
}

/* Frames until the field says `want` (ImGui trickles key presses, so a word
 * that needs three backspaces takes a few frames), or give up. */
bool settles(const char* want, int max_frames = 240) {
    for (int i = 0; i < max_frames; ++i) {
        frame();
        if (std::strcmp(buf, want) == 0) return true;
    }
    std::cerr << "  the field holds \"" << buf << "\", wanted \"" << want << "\"\n";
    return false;
}

} // namespace

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // a renderer that never draws
    io.Fonts->AddFontDefault();

    // nothing active: no keyboard
    frame();
    frame();
    CHECK(kb.shows == 0);

    // a field takes focus: the keyboard comes up for it, as a PHONE keyboard
    frame(true);
    for (int i = 0; i < 3; ++i) frame();
    CHECK(kb.shows == 1);
    CHECK(kb.kind == InputKind::Phone);
    CHECK(kb.seeded.text.empty());

    // typing
    kb.type("555", 3);
    CHECK(settles("555"));
    kb.type("555 12", 6);
    CHECK(settles("555 12"));

    // a keyboard rewriting what it had written (composing, autocorrect)
    kb.type("555 1234", 8);
    CHECK(settles("555 1234"));
    kb.type("Hola José", 10);
    CHECK(settles("Hola José"));
    kb.type("Hola José, ¿qué tal? 😀", 28);
    CHECK(settles("Hola José, ¿qué tal? 😀"));
    kb.type("Hola", 4);
    CHECK(settles("Hola"));

    // the field changed WITHOUT the keyboard (Home: a tap would do the same):
    // the keyboard is told where the cursor is now, once things are quiet
    const int updates_before = kb.updates;
    io.AddKeyEvent(ImGuiKey_Home, true);
    io.AddKeyEvent(ImGuiKey_Home, false);
    for (int i = 0; i < 10; ++i) frame();
    CHECK(kb.updates > updates_before);
    CHECK(kb.last_update.sel_end == 0 && kb.last_update.text == "Hola");

    // ...and typing continues from where the field says the cursor is
    kb.type("¡Hola", 2);
    CHECK(settles("¡Hola"));

    // the action key reaches the field as Enter: it commits, and the keyboard goes
    kb.action();
    for (int i = 0; i < 10; ++i) frame();
    CHECK(committed);
    CHECK(kb.hides == 1);

    ImGui::DestroyContext();
    std::cout << "textinput_view_smoke: " << (failures ? "FAILED" : "all ok") << "\n";
    return failures ? 1 : 0;
}
