/* hello_window.cpp — Phase 2's first window.
 *
 * Not a canvas yet: this proves the substrate (GLFW + OpenGL 3 + Dear ImGui)
 * and shows total observability in miniature — a live maiz::Core whose every
 * button press is a dispatched command, with the log strip right below it.
 * The projection engine and the real node canvas come next. */
#include "voidmaiz/embed.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>
#include <vector>

int main() {
    glfwSetErrorCallback([](int code, const char* desc) {
        std::fprintf(stderr, "glfw error %d: %s\n", code, desc);
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1024, 640, "Void Maiz — first window", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // The model — a live Void Core manager; the window owns no truth.
    maiz::Core core;
    std::vector<std::string> log;
    core.set_log_sink([&](std::string_view level, std::string_view op, std::string_view msg) {
        log.push_back("[" + std::string(level) + "] " + std::string(op) + ": " + std::string(msg));
    });
    core.dispatch("config set actor human:window");
    core.register_glyph(R"({"glyph":"note","label":"Note","editor":"form","fields":["text"],)"
                        R"("hints":{"color":"#7a5cff","face":{"w":160,"h":80}}})");
    core.dispatch("mantle new hello");

    int next_id = 1;
    std::string last_reply;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) { glfwWaitEvents(); continue; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Void Maiz");
        ImGui::Text("voidmaiz %.*s on Void Core (version verb: %s)",
                    (int)maiz::kVoidmaizVersion.size(), maiz::kVoidmaizVersion.data(),
                    core.dispatch("version").data.c_str());
        ImGui::Separator();

        // Every gesture is a command — these buttons ARE dispatches.
        if (ImGui::Button("Add note rune")) {
            last_reply = core.dispatch("rune new note note-" + std::to_string(next_id++)).text();
        }
        ImGui::SameLine();
        if (ImGui::Button("Undo")) last_reply = core.dispatch("undo").text();
        ImGui::SameLine();
        if (ImGui::Button("Redo")) last_reply = core.dispatch("redo").text();
        ImGui::SameLine();
        if (ImGui::Button("ls")) last_reply = core.dispatch("ls").text();
        if (!last_reply.empty()) ImGui::TextWrapped("%s", last_reply.c_str());
        ImGui::End();

        // The log strip: the CLI transcript, live. This is the founding
        // commitment on screen — nothing above happened without a line here.
        ImGui::Begin("Log");
        for (const auto& line : log) ImGui::TextUnformatted(line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::End();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
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
