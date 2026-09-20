/* network_window.cpp — networking, the half an application shows.
 *
 * One state document, three views of it that have nothing in common as
 * drawings: the node canvas, a plain list, and a custom-drawn "board" of cards
 * (standing in for a map, a calendar, a builder — anything a host draws
 * itself). Two simulated peers wander between runes.
 *
 * The claim this window exists to show: NONE of the three views contains
 * networking code. The canvas takes a CanvasNet; the list calls presence_item
 * after each row; the board calls presence_rect with each card's rect. That is
 * one declaration per thing shown, and every mark — colour, outline, badge,
 * tint, padlock — comes from one renderer, so a peer looks the same everywhere.
 * "The map had no presence" is what the alternative looks like.
 *
 * There is no network. The simulated peers go through the REAL path —
 * compose_presence → presence_to_json → presence_from_json → Roster — so the
 * sender's switches and the codec are exercised exactly as a transport would
 * exercise them. A real application replaces `loopback` with its transport and
 * changes nothing else.
 */
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/glhost.hpp"
#include "voidmaiz/netview.hpp"
#include "voidmaiz/presence.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/widgets.hpp"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h" // DockBuilder* — the one-time default layout
#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>
#include <vector>

static void build_patch(maiz::Core& core) {
    core.register_glyph(R"({"glyph":"card","label":"Card","fields":["text"],)"
                        R"("hints":{"color":"#4f86d9","face":{"w":150}}})");
    core.dispatch("mantle new team");
    const char* names[] = {"budget", "roadmap", "hiring", "launch", "diary", "retro"};
    for (int i = 0; i < 6; ++i) {
        core.dispatch(std::string("rune new card ") + names[i]);
        core.dispatch(std::string("setjson ") + names[i] + " pos [" +
                      std::to_string(40 + (i % 3) * 200) + "," + std::to_string(40 + (i / 3) * 140) +
                      "]");
    }
    core.dispatch("tag diary +private"); // stays on this device: never synced, never named
    core.dispatch("link budget launch --relation depends");
    core.dispatch("link roadmap launch --relation depends");
    core.dispatch("use team");
}

/* A simulated peer: picks a rune, sits on it, moves on. Its presence is
 * composed with its OWN sender switches, the same way a real device's is. */
struct SimPeer {
    maiz::Profile who;
    maiz::SharePolicy policy;
    int at = 0;
    double next = 0.0;
    std::vector<std::string> surfaces;
};

int main() {
    glfwSetErrorCallback(
        [](int code, const char* desc) { std::fprintf(stderr, "glfw error %d: %s\n", code, desc); });
    if (!glfwInit()) return 1;
    const char* glsl = maiz::gl_context_hints();
    GLFWwindow* window = glfwCreateWindow(1320, 800, "Void Maiz — networking", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl);
    maiz::enable_docking();
    ImGui::StyleColorsDark();

    maiz::Core core;
    core.dispatch("config set actor human:network-demo");
    build_patch(core);
    maiz::Scene scene = maiz::project_scene(core);
    maiz::EditorState ed;
    maiz::CanvasStyle style;

    // ── this device ──────────────────────────────────────────────────────────
    maiz::NetSettings net;
    net.self = {"dev-self", "You", 0x3fbf7f, ""};
    maiz::ShareFilter shareable = maiz::share_by_tag(); // `private` stays home
    maiz::Surfaces surfaces;
    maiz::Roster roster;
    roster.set_self(net.self.id);

    std::vector<SimPeer> sims = {
        {{"dev-bo", "Bo Builder", 0xe8a33d, ""}, {}, 0, 0.0, {"canvas", "list"}},
        {{"dev-cy", "Cy Planner", 0xc0508a, ""}, {}, 3, 1.3, {"board"}},
    };

    /* The transport, replaced by a function call. A real application sends
     * `bytes` on its ephemeral channel and calls receive() when bytes arrive. */
    std::vector<std::string> loopback;
    auto receive = [&](const std::string& bytes, double now) {
        maiz::PresenceState st;
        std::string err;
        if (maiz::presence_from_json(bytes, st, &err)) roster.update(st, now);
    };

    std::string last_sent;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            glfwWaitEvents();
            continue;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        double now = ImGui::GetTime();

        // ── the simulated peers speak (through the real codec) ───────────────
        for (auto& p : sims) {
            if (now >= p.next) {
                p.at = (p.at + 1 + (int)(now * 7) % 3) % (int)scene.nodes.size();
                p.next = now + 2.2;
            }
            // a peer's view of the SAME document: it selects by id, like everyone
            maiz::Surfaces theirs;
            theirs.begin_frame();
            for (const auto& s : p.surfaces) theirs.declare(s);
            if (!p.surfaces.empty()) theirs.focus(p.surfaces.front());
            maiz::PresenceState st = maiz::compose_presence(
                p.who, {scene.nodes[p.at].id}, theirs, p.policy, scene, shareable);
            loopback.push_back(maiz::presence_to_json(st));
        }
        for (const auto& bytes : loopback) receive(bytes, now);
        loopback.clear();
        roster.prune(now, 5.0);

        surfaces.begin_frame(); // every view below re-declares what it shows

        unsigned dock = maiz::begin_dockspace();
        if (maiz::dockspace_needs_seed(dock)) {
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
            ImGuiID main_id = dock;
            ImGuiID right = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Right, 0.28f, nullptr,
                                                        &main_id);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down, 0.36f, nullptr,
                                                         &main_id);
            ImGuiID right_bottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.55f,
                                                               nullptr, &right);
            ImGui::DockBuilderDockWindow("Canvas", main_id);
            ImGui::DockBuilderDockWindow("Board", bottom);
            ImGui::DockBuilderDockWindow("List", right);
            ImGui::DockBuilderDockWindow("Networking", right_bottom);
            ImGui::DockBuilderFinish(dock);
        }

        std::vector<std::string> pending;

        // ── view 1: the node canvas — one struct, no networking code ─────────
        ImGui::Begin("Canvas");
        maiz::presence_surface_badges(roster, net.show, "canvas");
        maiz::CanvasNet cnet;
        cnet.surfaces = &surfaces;
        cnet.roster = &roster;
        cnet.display = net.show;
        cnet.shareable = shareable;
        maiz::CanvasIO cio =
            maiz::edit_canvas("net-canvas", scene, ed, style, nullptr, nullptr, {}, nullptr, &cnet);
        pending = cio.commands;
        ImGui::End();

        // ── view 2: a plain list — one call per row ──────────────────────────
        ImGui::Begin("List");
        maiz::presence_surface_badges(roster, net.show, "list");
        maiz::presence_focus_if_active(surfaces, "list");
        for (const auto& n : scene.nodes) {
            bool sel = ed.selected(n.name);
            if (ImGui::Selectable(n.name.c_str(), sel, 0, ImVec2(0, ImGui::GetFrameHeight()))) {
                ed.selection.clear();
                ed.selection.push_back(n.name);
            }
            maiz::presence_item(surfaces, roster, net.show, "list", n.id, maiz::Mark::Badge,
                                !shareable(n));
        }
        ImGui::End();

        // ── view 3: a custom-drawn board — one call per card rect ────────────
        ImGui::Begin("Board");
        maiz::presence_surface_badges(roster, net.show, "board");
        maiz::presence_focus_if_active(surfaces, "board");
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 at = ImGui::GetCursorScreenPos();
            float cw = 150, ch = 70, gap = 18;
            for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
                const auto& n = scene.nodes[i];
                ImVec2 mn(at.x + (float)i * (cw + gap) + 8, at.y + 16);
                ImVec2 mx(mn.x + cw, mn.y + ch);
                dl->AddRectFilled(mn, mx, IM_COL32(44, 48, 58, 255), 6.0f);
                dl->AddText(ImVec2(mn.x + 12, mn.y + ch * 0.5f - 7), IM_COL32(230, 230, 235, 255),
                            n.name.c_str());
                maiz::presence_rect(surfaces, roster, net.show, "board", n.id, mn, mx,
                                    maiz::Mark::Tint, !shareable(n));
            }
            ImGui::Dummy(ImVec2((cw + gap) * (float)scene.nodes.size(), ch + 32));
        }
        ImGui::End();

        // ── the Networking section, and what this device would broadcast ─────
        ImGui::Begin("Networking");
        maiz::draw_member_list(roster, net.self);
        ImGui::Spacing();
        maiz::draw_network_settings(net);
        /* The conflict panel, on SAMPLE rows: this window has no sync layer (it
         * simulates peers), so there is nothing here that could really
         * disagree. The widget is the same one voidmaiz_net feeds from a real
         * merge — plain rows in, a choice out. */
        ImGui::SeparatorText("if a merge could not decide");
        static std::vector<maiz::ConflictRow> sample = {
            {"c1", "values", "team", "r-002", "roadmap", "", "content.text",
             {"\"ship in March\"", "\"ship in April\""}},
            {"c2", "deleted", "team", "r-004", "launch", "", "present",
             {"deleted", "kept"}}};
        static std::string answered; // shown below, so a click has a visible effect
        maiz::ConflictChoice pick = maiz::draw_conflicts(sample);
        if (pick.side >= 0) {
            for (auto it = sample.begin(); it != sample.end(); ++it)
                if (it->id == pick.row) {
                    answered = it->sides[(std::size_t)pick.side];
                    sample.erase(it);
                    break;
                }
        }

        if (!answered.empty()) ImGui::TextDisabled("you chose: %s", answered.c_str());

        maiz::PresenceState mine = maiz::compose_presence(
            net.self, maiz::selection_ids(scene, ed.selection), surfaces, net.send, scene,
            shareable);
        last_sent = maiz::presence_to_json(mine);
        ImGui::SeparatorText("this device would send");
        ImGui::TextWrapped("%s", last_sent.c_str());
        ImGui::TextDisabled("select `diary`: it is private, so it is never named here");
        ImGui::End();

        maiz::end_dockspace();

        for (const auto& c : pending) {
            core.dispatch(c);
            scene = maiz::project_scene(core);
        }

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
