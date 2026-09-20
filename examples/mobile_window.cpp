/* mobile_window.cpp — the touch shell, on a desktop.
 *
 * A phone-proportioned window driven entirely through maiz::TouchRecognizer:
 * the mouse becomes a finger, and holding Alt (or the right button) adds a
 * SECOND finger mirrored about the window centre, so pinch, two-finger pan and
 * rotate can be exercised without an APK.
 *
 * It exists for three reasons:
 *  1. It is the reference a mobile host copies — Void Hormiga's phone build
 *     starts here, not from InteractionCombinators' NativeActivity shell, which
 *     hand-rolled the recognizer this file's six-line hookup replaces.
 *  2. Every mobile widget is on screen at once, so a change to the kit is
 *     looked at rather than reasoned about.
 *  3. It proves the claim the whole design rests on: the canvas below is the
 *     UNMODIFIED edit_canvas. Long-press opens the desktop context menu, the
 *     pinch compiles the same `config set view.camera` the wheel does, and no
 *     line of voidmaiz_view knows a finger is involved.
 *
 * Read the frame loop's input block; the rest is an ordinary Void Maiz host.
 */
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/face.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/glhost.hpp"
#include "voidmaiz/inspector.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/usergraph.hpp"
#include "voidmaiz/widgets.hpp"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <cfloat>
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
    core.register_glyph(R"({"glyph":"out","label":"Output","fields":[],)"
                        R"("hints":{"color":"#8a3b3b","ports":[{"name":"prin","principal":true},)"
                        R"({"name":"in","dir":"in","type":"audio"}]}})");
    core.register_glyph(R"({"glyph":"note","label":"Note","fields":["text"],)"
                        R"("hints":{"color":"#4a4a52","face":{"w":140}}})");

    core.dispatch("mantle new phone-demo");
    core.dispatch("rune new osc lfo");
    core.dispatch("rune new osc carrier");
    core.dispatch("rune new filter lowpass");
    core.dispatch("rune new out speakers");
    core.dispatch("tag lfo +modulation");
    core.dispatch("tag carrier +voice");
    core.dispatch("tag lowpass +voice");
    core.dispatch("setjson lfo pos [40,40]");
    core.dispatch("setjson carrier pos [40,220]");
    core.dispatch("setjson lowpass pos [250,140]");
    core.dispatch("setjson speakers pos [250,330]");
    core.dispatch("link lfo lowpass --relation 2:2");
    core.dispatch("link carrier lowpass --relation 2:1");
    core.dispatch("link lowpass speakers --relation 3:1");
    core.dispatch("rune new note about");
    core.dispatch(R"(set about text "long-press anything")");
    core.dispatch("setjson about pos [40,420]");
    core.dispatch("use phone-demo");
}

int main() {
    glfwSetErrorCallback(
        [](int code, const char* desc) { std::fprintf(stderr, "glfw error %d: %s\n", code, desc); });
    if (!glfwInit()) return 1;
    const char* glsl = maiz::gl_context_hints();
    // a portrait phone, roughly a 6" device at 1x
    GLFWwindow* window = glfwCreateWindow(430, 900, "Void Maiz — touch shell", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    /* install_callbacks = FALSE is the whole trick: GLFW's own mouse events
     * never reach ImGui, so the recognizer is the only source of a pointer.
     * A real touch shell gets this for free — there is no mouse to suppress. */
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init(glsl);

    /* The two calls a touch shell makes at startup. `dp` is the platform's
     * density scale — AConfiguration_getDensity()/160 on Android; here the
     * window is 1x, so the demo picks a value that makes the chrome look like
     * a phone's on a desktop monitor. */
    maiz::TouchProfile profile;
    profile.dp = 1.6f;
    profile.edge_mm = 5.0f; // opt in: a left-edge swipe exits a subgraph
    maiz::apply_touch_metrics(profile.dp);

    ImGui::StyleColorsDark();
    maiz::CanvasStyle canvas_style;
    canvas_style.theme = maiz::CanvasTheme::dark();
    maiz::apply_touch_canvas(canvas_style, profile);

    maiz::TouchRecognizer touch(profile);

    maiz::Core core;
    std::vector<maiz::LogEntry> log;
    core.set_log_sink([&](std::string_view level, std::string_view op, std::string_view msg) {
        log.push_back({std::string(level), std::string(op), std::string(msg)});
    });
    core.dispatch("config set actor human:phone");
    build_demo_patch(core);

    maiz::Scene scene = maiz::project_scene(core);
    maiz::EditorState ed;
    ed.cam = {-20, -20, 1.0f};
    if (maiz::Camera saved; maiz::parse_camera(core.dispatch("config get view.camera").data, saved))
        ed.cam = saved;

    maiz::AddPalette palette;
    palette.entries = {{"osc", "Oscillator"}, {"filter", "Filter"}, {"out", "Output"},
                       {"note", "Note"}};
    maiz::FaceRegistry faces;
    faces.by_glyph["osc"] = [](maiz::FaceContext& ctx) {
        maiz::face_drag_number(ctx, "freq", 0.05f, 0.0f, 2000.0f);
        maiz::face_combo(ctx, "wave", {"sine", "saw", "square", "triangle"});
    };

    /* The attention graph, fed from the OBSERVED channel (okf/concepts/allomone/
     * user-graph.md). Before the recognizer existed the channel was whatever a
     * settings combo said, so Allomone's `device "touch"` matched a claim about
     * the input rather than the input. Here it is a fact: `tf.channel` is what
     * actually drew the contact.
     *
     * The framing policy — when one piece of work ends — is the HOST's and
     * needs a clock; the graph itself is timeless and never learns that time
     * passed. Materializing is a choice too: nothing is written to the model
     * unless someone asks, which is what keeps attention inside the ephemera
     * rule by default. */
    maiz::UserGraph ugraph;
    bool frame_open = false;
    double frame_idle = 0.0;
    const double frame_idle_limit = 2.5;
    std::string last_selection;

    // the mobile chrome's view state — all of it the host's, none of it truth
    maiz::BottomSheetState sheet;
    maiz::SnackbarState snack;
    maiz::SwipeListState swipe;
    int workflow = 0;
    bool dial_open = false;
    int next_id = 1;

    auto dispatch_and_reproject = [&](const std::string& cmd) {
        maiz::Result r = core.dispatch(cmd);
        scene = maiz::project_scene(core); // the one-sync rule
        /* The mobile confirmation idiom, and it costs one line because
         * commitment 2 already did the work: every gesture is a command, and
         * `undo` is a verb, so "what happened + UNDO" is just a widget. View
         * commands are not offered an undo — the config tier is undo-exempt. */
        if (r.ok && cmd.rfind("config", 0) != 0)
            maiz::show_snackbar(snack, cmd.substr(0, cmd.find(' ')) + " ok", "UNDO");
        return r;
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            glfwWaitEvents();
            continue;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        // ── the input block: everything a touch shell has to write ───────────
        int fbw = 0, fbh = 0;
        glfwGetWindowSize(window, &fbw, &fbh);
        touch.set_viewport((float)fbw, (float)fbh);

        std::vector<maiz::TouchPoint> contacts;
        {
            double mx = 0, my = 0;
            glfwGetCursorPos(window, &mx, &my);
            bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            bool second = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (down) {
                contacts.push_back({0, (float)mx, (float)my});
                // the emulator trick: a second finger mirrored about the centre,
                // so moving toward the middle pinches in
                if (second) contacts.push_back({1, (float)(fbw - mx), (float)(fbh - my)});
            }
        }

        maiz::TouchFrame tf = touch.update(contacts, ImGui::GetTime());
        ImGuiIO& io = ImGui::GetIO();
        if (tf.pointer.present) io.AddMousePosEvent(tf.pointer.x, tf.pointer.y);
        else io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        io.AddMouseButtonEvent(0, tf.pointer.left);
        io.AddMouseButtonEvent(1, tf.pointer.right);

        ImGui::NewFrame();

        /* Two-finger gestures never reach the pointer UI, so the host applies
         * them to the camera itself — and the flush is the SAME command the
         * wheel compiles on the desktop. The canvas origin is the work area's
         * top-left because the canvas below is full-bleed. */
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        bool cam_moved = false;
        for (const auto& e : tf.events) {
            if (maiz::camera_apply(e, ed.cam, vp->WorkPos.x, vp->WorkPos.y, canvas_style.min_zoom,
                                   canvas_style.max_zoom))
                cam_moved = true;
            // a left-edge swipe is "back": leave the subgraph, as a phone's
            // back gesture would
            if (e.kind == maiz::TouchGesture::EdgeSwipe && e.edge == maiz::TouchEdge::Left &&
                e.dx > 40.0f && !ed.mantle_stack.empty()) {
                std::string up = ed.mantle_stack.back();
                ed.mantle_stack.pop_back();
                dispatch_and_reproject("use " + up);
            }
        }
        if (cam_moved) ed.cam_dirty = true;
        if (ed.cam_dirty && !tf.gesture_owns_input && contacts.empty()) {
            core.dispatch(maiz::compile_camera(ed.cam)); // one command at gesture end
            ed.cam_dirty = false;
        }

        // ── attention: what was touched together, and through what ───────────
        {
            auto attend = [&](const std::string& id, const char* kind) {
                if (!frame_open) {
                    ugraph.open_frame();
                    frame_open = true;
                }
                frame_idle = 0.0;
                ugraph.touch(id, kind, tf.channel); // observed, not declared
            };
            if (frame_open) {
                frame_idle += io.DeltaTime;
                if (frame_idle > frame_idle_limit) { // a lull ends the work
                    ugraph.close_frame();
                    frame_open = false;
                }
            }
            std::string sel = ed.selection.empty() ? std::string() : ed.selection.front();
            if (!sel.empty() && sel != last_selection) attend("rune:" + sel, "rune");
            last_selection = sel;
        }

        // ── the shell ────────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##phone", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

        if (maiz::segmented("workflow", {"Graph", "List", "Help"}, workflow)) {
            if (!frame_open) {
                ugraph.open_frame();
                frame_open = true;
            }
            frame_idle = 0.0;
            ugraph.touch("workflow:" + std::to_string(workflow), "pane", tf.channel);
        }
        ImGui::Spacing();

        std::vector<std::string> pending;
        if (workflow == 0) {
            maiz::CanvasIO cio =
                maiz::edit_canvas("node-canvas", scene, ed, canvas_style, &palette, &faces);
            pending = cio.commands;
        } else if (workflow == 1) {
            /* The table view's mobile form: one swipe row per node. The row is
             * ordinary ImGui drawn over the widget; the action it returns is
             * compiled by the host, exactly as a context-menu entry would be. */
            ImGui::TextDisabled("swipe a row left");
            ImGui::Spacing();
            for (const auto& n : scene.nodes) {
                int hit = maiz::begin_swipe_row(swipe, n.name.c_str(), {"Delete"});
                ImGui::Text("%s  ", n.name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s", n.glyph.c_str());
                maiz::end_swipe_row(swipe);
                if (hit == 0) pending.push_back(maiz::compile_deletes({n.name}));
            }
        } else {
            ImGui::TextWrapped("This window is a phone. The mouse is a finger.");
            ImGui::Spacing();
            ImGui::BulletText("tap = tap, drag = drag");
            ImGui::BulletText("hold 0.4 s = long press,\n  which opens the DESKTOP context menu");
            ImGui::BulletText("Alt (or right button) while dragging\n  adds a mirrored second"
                              " finger:\n  pinch, two-finger pan, rotate");
            ImGui::BulletText("drag in from the left edge = back");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("recognizer");
            ImGui::Text("contacts: %d   events: %d", (int)contacts.size(), (int)tf.events.size());
            ImGui::Text("pointer: %s%s%s", tf.pointer.present ? "on" : "parked",
                        tf.pointer.left ? " L" : "", tf.pointer.right ? " R" : "");
            ImGui::Text("camera: %.0f %.0f  x%.2f", ed.cam.x, ed.cam.y, ed.cam.zoom);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("attention graph (Allomone reads this)");
            ImGui::Text("channel: %s", tf.channel.empty()
                                           ? "(nothing touched)"
                                           : std::string(tf.channel).c_str());
            ImGui::Text("affordances: %d   edges: %d   frame: %s",
                        (int)ugraph.affordances().size(), (int)ugraph.coincidences().size(),
                        frame_open ? "open" : "closed");
            if (ImGui::Button("materialize") && !ugraph.empty()) {
                // ephemera become model content only when asked — and then
                // Allomone can query them like any other data
                core.register_glyph(std::string(maiz::UserGraph::affordance_glyph()));
                core.dispatch("mantle new attention");
                core.dispatch("use attention");
                for (const auto& c : ugraph.compile("attention")) core.dispatch(c);
                core.dispatch("use phone-demo");
                maiz::show_snackbar(snack, "attention materialized");
            }
            ImGui::Spacing();
            ImGui::TextDisabled("steppers (touch-native number entry)");
            static float demo_v = 3.0f;
            maiz::stepper("demo", "iterations", demo_v, 1.0f, 0.0f, 20.0f);
        }
        ImGui::End();

        // ── the inspector, as a bottom sheet ─────────────────────────────────
        if (workflow == 0) {
            if (maiz::begin_bottom_sheet("##sheet", sheet)) {
                maiz::CanvasIO iio = maiz::draw_inspector(scene, ed);
                for (const auto& c : iio.commands) pending.push_back(c);
            }
            maiz::end_bottom_sheet(sheet);
            if (sheet.settled) // view state, config tier, one command
                core.dispatch("config set view.sheet " + std::to_string(sheet.detent));

            // the add palette, thumb-reachable
            int picked = maiz::speed_dial("##dial", "+", {"osc", "filter", "out", "note"},
                                          dial_open, ImVec2(0, sheet.height * vp->WorkSize.y));
            if (picked >= 0)
                pending.push_back("rune new " + palette.entries[picked].glyph + " " +
                                  palette.entries[picked].glyph + "-" + std::to_string(next_id++));
        }

        for (const auto& cmd : pending) dispatch_and_reproject(cmd);

        if (maiz::draw_snackbar(snack)) dispatch_and_reproject("undo");

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
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
