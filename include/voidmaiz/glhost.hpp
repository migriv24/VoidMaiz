/*
 * voidmaiz/glhost.hpp — the GL context a Void Maiz view needs, per platform.
 *
 * WHY THIS EXISTS. Every host that opens a window repeats the same four lines
 * before `glfwCreateWindow`, and then repeats a matching GLSL version string
 * at `ImGui_ImplOpenGL3_Init`. The two must agree, and on macOS the pair every
 * host had copied does not:
 *
 *     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
 *     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
 *     ...
 *     ImGui_ImplOpenGL3_Init("#version 130");
 *
 * macOS ships no OpenGL 3.0. It offers a legacy 2.1 context, or 3.2+ **core
 * profile with forward compatibility** and nothing between, so a plain 3.0
 * request silently hands back 2.1 — `glfwCreateWindow` SUCCEEDS — and then the
 * `#version 130` shader fails to compile against it. The window opens and
 * stays blank, which is the worst shape a portability bug can take: it looks
 * like a rendering bug in the host's own code.
 *
 * So the hints and the version string are ONE call that returns the other
 * half. They cannot drift apart, because there is no longer a second place to
 * write either of them:
 *
 *     if (!glfwInit()) return 1;
 *     const char* glsl = maiz::gl_context_hints();
 *     GLFWwindow* w = glfwCreateWindow(1280, 760, "…", nullptr, nullptr);
 *     ...
 *     ImGui_ImplOpenGL3_Init(glsl);
 *
 * Header-only on purpose: `voidmaiz_imgui` is the vendored substrate and
 * carries none of our translation units, and a host that links only
 * `voidmaiz_imgui` (the `maiz_hello` shape) must still be able to call this.
 *
 * NOT a window/loop abstraction. The host owns its window, its event pump and
 * its frame; this is the one piece where the answer is the library's, because
 * it is fixed by which ImGui backend we vendor and which GL that backend needs
 * (okf/concepts/views-as-projections.md — the library owns the substrate, the
 * host owns the surface).
 */
#pragma once

#include <GLFW/glfw3.h>

namespace maiz {

/* Set the GLFW window hints this library's ImGui backend needs, and return the
 * GLSL version string that matches them — hand it straight to
 * `ImGui_ImplOpenGL3_Init`. Call after `glfwInit()`, before
 * `glfwCreateWindow`.
 *
 * Desktop GL 3.0 / `#version 130` everywhere except macOS, which gets 3.2 core
 * + forward-compat / `#version 150`. Those are the SAME capability: 3.2 core is
 * the lowest context macOS offers that runs a shader-based backend at all, and
 * `#version 150` is its GLSL equivalent of 130. The returned pointer is a
 * string literal and outlives any caller. */
inline const char* gl_context_hints() {
#if defined(__APPLE__)
    /* The three hints are a set, not a preference: on macOS a core profile
     * without FORWARD_COMPAT is rejected outright, and 3.2 is the floor. */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    return "#version 150";
#else
    /* Windows, Linux, and anything else with a compatibility profile: 3.0 is
     * deliberately modest, so an old Intel GPU or a software rasterizer (a CI
     * runner, a VM, a remote desktop) still gets a context. */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    return "#version 130";
#endif
}

} // namespace maiz
