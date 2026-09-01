/* canvas_window.cpp — Phase 2: the node canvas over a real state document.
 *
 * Builds a small patch through the dispatcher (never by hand), then renders
 * it: an audio-ish dataflow chain (linguine), a γ/δ interaction pair joined
 * principal-to-principal (fettuccine), and a loose semantic link. The buttons
 * dispatch commands and the canvas re-projects — the one-sync rule on screen:
 * dispatch → project, nothing else ever touches the picture. */
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/face.hpp"
#include "voidmaiz/inspector.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/widgets.hpp"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h" // DockBuilder* — the one-time default dock layout
#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>
#include <vector>

static void build_demo_patch(maiz::Core& core) {
    core.register_glyph(
        R"({"glyph":"osc","label":"Oscillator","fields":["freq","wave"],)"
        R"("hints":{"color":"#5a4fcf","face":{"w":170,"h":124},)"
        R"("ports":[{"name":"prin","principal":true},)"
        R"({"name":"freq","dir":"in","type":"float"},{"name":"out","dir":"out","type":"audio"}]}})");
    core.register_glyph(
        R"({"glyph":"filter","label":"Filter","fields":["cutoff"],)"
        R"("hints":{"color":"#2f7d5a","ports":[{"name":"prin","principal":true},)"
        R"({"name":"in","dir":"in","type":"audio"},{"name":"cutoff","dir":"in","type":"float"},)"
        R"({"name":"out","dir":"out","type":"audio"}]}})");
    core.register_glyph(
        R"({"glyph":"out","label":"Output","fields":[],)"
        R"("hints":{"color":"#8a3b3b","ports":[{"name":"prin","principal":true},)"
        R"({"name":"in","dir":"in","type":"audio"}]}})");
    core.register_glyph(
        R"({"glyph":"combinator","label":"Combinator","fields":["kind","target"],)"
        R"("hints":{"color":"#b8862d","face":{"w":120,"h":64},"enter":"target"}})");
    core.register_glyph(R"({"glyph":"note","label":"Note","fields":["text"],)"
                        R"("hints":{"color":"#4a4a52","face":{"w":140}}})");

    core.dispatch("mantle new canvas-demo");
    // The dataflow chain — linguine wires between auxiliary ports ("i:j").
    core.dispatch("rune new osc lfo");
    core.dispatch("rune new osc carrier");
    core.dispatch("rune new filter lowpass");
    core.dispatch("rune new out speakers");
    core.dispatch("tag lfo +modulation");
    core.dispatch("tag carrier +voice");
    core.dispatch("tag lowpass +voice");
    core.dispatch("link lfo lowpass --relation 2:2");     // lfo.out -> lowpass.cutoff
    core.dispatch("link carrier lowpass --relation 2:1"); // carrier.out -> lowpass.in
    core.dispatch("link lfo carrier --relation 2:1");     // audio-rate FM: lfo owns carrier.freq
    core.dispatch("link lowpass speakers --relation 3:1");
    // The interaction pair — one fettuccine between principals ("0:0").
    core.dispatch("rune new combinator gamma");
    core.dispatch("rune new combinator delta");
    core.dispatch("set gamma kind gamma");
    core.dispatch("set delta kind delta");
    core.dispatch("tag gamma +net");
    core.dispatch("tag delta +net");
    core.dispatch("setjson gamma pos [120,330]");
    core.dispatch("setjson delta pos [360,330]");
    core.dispatch("link gamma delta --relation 0:0");
    // A loose semantic link and a placed note.
    core.dispatch("rune new note about");
    core.dispatch(R"(set about text "gamma~delta is an active pair")");
    core.dispatch("setjson about pos [120,450]");
    core.dispatch("link about gamma --relation annotates");
    // A subgraph: gamma's guts live in their own mantle; double-click enters.
    core.dispatch("mantle new inside-gamma");
    core.dispatch("rune new osc inner-osc");
    core.dispatch("rune new out inner-out");
    core.dispatch("link inner-osc inner-out --relation 2:1");
    core.dispatch("use canvas-demo");
    core.dispatch("set gamma target inside-gamma");
}

int main() {
    glfwSetErrorCallback([](int code, const char* desc) {
        std::fprintf(stderr, "glfw error %d: %s\n", code, desc);
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1280, 760, "Void Maiz — canvas", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    maiz::enable_docking(); // panels are movable/floating/re-dockable (Q11 ruling)

    // Light by default (the author's preference); toggle lives in the header.
    bool light_mode = true;
    maiz::CanvasStyle canvas_style;
    auto apply_theme = [&] {
        if (light_mode) {
            ImGui::StyleColorsLight();
            canvas_style.theme = maiz::CanvasTheme::light();
        } else {
            ImGui::StyleColorsDark();
            canvas_style.theme = maiz::CanvasTheme::dark();
        }
    };
    apply_theme();

    maiz::Core core;
    std::vector<maiz::LogEntry> log;
    core.set_log_sink([&](std::string_view level, std::string_view op, std::string_view msg) {
        log.push_back({std::string(level), std::string(op), std::string(msg)});
    });
    core.dispatch("config set actor human:canvas");
    build_demo_patch(core);

    maiz::Scene scene = maiz::project_scene(core);
    maiz::EditorState ed;
    ed.cam.x = -20;
    ed.cam.y = -20;
    // Restore the persisted camera, if this session's config carries one.
    if (maiz::Camera saved; maiz::parse_camera(core.dispatch("config get view.camera").data, saved))
        ed.cam = saved;
    int next_id = 1;
    maiz::AddPalette palette;
    palette.entries = {{"osc", "Oscillator"},
                       {"filter", "Filter"},
                       {"out", "Output"},
                       {"combinator", "Combinator"},
                       {"note", "Note"}};

    maiz::CommandBarState cmdbar;
    int undo_depth = 0;
    auto refresh_undo_depth = [&] {
        maiz::Result h = core.dispatch("history");
        undo_depth = (h.lines.size() == 1 && h.lines[0] == "(no history)")
                         ? 0
                         : (int)h.lines.size();
    };
    refresh_undo_depth();

    // Faces: the oscillator gets live widgets on its body — a staged
    // drag-number for freq (ONE `set` on release) and a wave combo.
    maiz::FaceRegistry faces;
    faces.by_glyph["osc"] = [](maiz::FaceContext& ctx) {
        maiz::face_drag_number(ctx, "freq", 0.05f, 0.0f, 2000.0f);
        maiz::face_combo(ctx, "wave", {"sine", "saw", "square", "triangle"});
    };

    auto dispatch_and_reproject = [&](const std::string& cmd) {
        maiz::Result r = core.dispatch(cmd);
        scene = maiz::project_scene(core); // the one-sync rule
        refresh_undo_depth();
        return r;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) { glfwWaitEvents(); continue; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        // Movable/floating/re-dockable panels via the library's DockSpace host.
        unsigned dock = maiz::begin_dockspace();
        // Seed a default arrangement ONCE (only when no saved .ini layout
        // exists — a user's saved dock layout wins): Canvas center, Inspector
        // right, Log bottom. Host owns the arrangement; the library owns the
        // DockSpace primitive.
        if (maiz::dockspace_needs_seed(dock)) {
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
            ImGuiID main_id = dock;
            ImGuiID right = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Right, 0.22f, nullptr,
                                                        &main_id);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down, 0.24f, nullptr,
                                                         &main_id);
            ImGui::DockBuilderDockWindow("Canvas", main_id);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Log", bottom);
            ImGui::DockBuilderFinish(dock);
        }

        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::Text("mantle: %s   nodes: %d   wires: %d   selected: %d   undo: %d   zoom: %.2f",
                    scene.mantle.c_str(), (int)scene.nodes.size(), (int)scene.wires.size(),
                    (int)ed.selection.size(), undo_depth, ed.cam.zoom);
        ImGui::SameLine(0, 24);
        if (ImGui::SmallButton("add osc")) {
            dispatch_and_reproject("rune new osc osc-" + std::to_string(next_id++));
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("undo")) dispatch_and_reproject("undo");
        ImGui::SameLine();
        if (ImGui::SmallButton("redo")) dispatch_and_reproject("redo");
        ImGui::SameLine();
        if (ImGui::SmallButton(light_mode ? "dark" : "light")) {
            light_mode = !light_mode;
            apply_theme();
        }
        ImGui::SameLine(0, 24);
        ImGui::TextDisabled("drag node: move | drag port: wire | shift+A: add | wheel: zoom | mid/right: pan | del: rm");
        maiz::CanvasIO cio = maiz::edit_canvas("node-canvas", scene, ed, canvas_style, &palette, &faces);
        for (const auto& cmd : cio.commands) dispatch_and_reproject(cmd);
        ImGui::End();

        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
        maiz::CanvasIO iio = maiz::draw_inspector(scene, ed);
        for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
        ImGui::End();

        ImGui::Begin("Log", nullptr, ImGuiWindowFlags_NoCollapse);
        float footer = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##lines", ImVec2(0, -footer));
        maiz::draw_log_strip(log);
        ImGui::EndChild();
        maiz::CanvasIO bio = maiz::draw_command_bar(cmdbar);
        for (const auto& cmd : bio.commands) {
            maiz::Result r = dispatch_and_reproject(cmd);
            log.push_back({">", cmd, r.text().empty() ? (r.ok ? "ok" : "failed") : r.text()});
        }
        ImGui::End();

        maiz::end_dockspace();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        if (light_mode) glClearColor(0.93f, 0.93f, 0.94f, 1.0f);
        else glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
