cmake_minimum_required(VERSION 3.20)
project(voidmaiz CXX C) # C for the vendored cJSON

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Void Core (the model) ────────────────────────────────────────────────────
# Desktop links the sibling repo's prebuilt shared library. Android (and any
# cross-compile) builds the pure-C core from source into a static lib instead —
# there are no prebuilts for that substrate, and the C11 sources need nothing.
set(VOIDCORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core" CACHE PATH
    "Path to Void Core's core/ directory (include/voidcore.h + build/)")

if(NOT EXISTS "${VOIDCORE_ROOT}/include/voidcore.h")
  message(FATAL_ERROR "voidcore.h not found under VOIDCORE_ROOT=${VOIDCORE_ROOT}")
endif()

if(ANDROID)
  add_library(voidcore_static STATIC
    "${VOIDCORE_ROOT}/vendor/cJSON.c"
    "${VOIDCORE_ROOT}/src/util/ids.c"
    "${VOIDCORE_ROOT}/src/util/log.c"
    "${VOIDCORE_ROOT}/src/glyph/glyph.c"
    "${VOIDCORE_ROOT}/src/tags/tag.c"
    "${VOIDCORE_ROOT}/src/model/spirit.c"
    "${VOIDCORE_ROOT}/src/model/rune.c"
    "${VOIDCORE_ROOT}/src/model/mantle.c"
    "${VOIDCORE_ROOT}/src/model/binding.c"
    "${VOIDCORE_ROOT}/src/dispatch/args.c"
    "${VOIDCORE_ROOT}/src/dispatch/codec.c"
    "${VOIDCORE_ROOT}/src/dispatch/dispatch.c"
    "${VOIDCORE_ROOT}/src/dispatch/dispatch_util.c"
    "${VOIDCORE_ROOT}/src/dispatch/verbs_query.c"
    "${VOIDCORE_ROOT}/src/dispatch/verbs_edit.c"
    "${VOIDCORE_ROOT}/src/dispatch/verbs_graph.c"
    "${VOIDCORE_ROOT}/src/dispatch/verbs_lifecycle.c"
    "${VOIDCORE_ROOT}/src/dispatch/verbs_script.c"
    "${VOIDCORE_ROOT}/src/dispatch/undo.c"
    # journal.c arrived in Void Core 0.2.8-0.2.12 (the command journal) and was
    # missed here, because the desktop build links the prebuilt DLL and never
    # walks this list — so only an Android/cross build could have found it, and
    # nobody had run one since. Kept in sync deliberately: this list is the one
    # place Void Maiz has to know the core's file layout.
    "${VOIDCORE_ROOT}/src/dispatch/journal.c"
    "${VOIDCORE_ROOT}/src/dispatch/lifecycle.c"
    "${VOIDCORE_ROOT}/src/scripts/voidscript.c"
    "${VOIDCORE_ROOT}/src/scripts/vs_interp.c"
    "${VOIDCORE_ROOT}/src/scripts/vs_expr.c"
    "${VOIDCORE_ROOT}/src/vc_manager.c"
  )
  target_include_directories(voidcore_static
    PUBLIC  "${VOIDCORE_ROOT}/include"
    PRIVATE "${VOIDCORE_ROOT}/src" "${VOIDCORE_ROOT}/vendor")
  target_compile_definitions(voidcore_static PRIVATE CJSON_HIDE_SYMBOLS)
  set(VOIDCORE_LIBRARY voidcore_static)
else()
  # A MULTI-CONFIG GENERATOR PUTS IT ONE FOLDER DEEPER (2026-09-20). Visual
  # Studio builds into build/Release and build/bin/Release, so a host configured
  # with the default Windows generator was told to "build the core first" when it
  # already had. Void Hormiga's Windows CI had been red on this since 0.1.2, and
  # it looked like a missing step rather than a search path.
  find_library(VOIDCORE_LIBRARY NAMES voidcore libvoidcore
    HINTS "${VOIDCORE_ROOT}/build" "${VOIDCORE_ROOT}/build/bin"
          "${VOIDCORE_ROOT}/build/Release" "${VOIDCORE_ROOT}/build/bin/Release"
          "${VOIDCORE_ROOT}/build/Debug" "${VOIDCORE_ROOT}/build/bin/Debug"
    NO_DEFAULT_PATH)
  if(NOT VOIDCORE_LIBRARY)
    message(FATAL_ERROR "No built voidcore library under ${VOIDCORE_ROOT}/build — build the core first")
  endif()

  find_file(VOIDCORE_DLL NAMES libvoidcore.dll voidcore.dll
    HINTS "${VOIDCORE_ROOT}/build/bin" "${VOIDCORE_ROOT}/build"
          "${VOIDCORE_ROOT}/build/bin/Release" "${VOIDCORE_ROOT}/build/Release"
          "${VOIDCORE_ROOT}/build/bin/Debug" "${VOIDCORE_ROOT}/build/Debug"
    NO_DEFAULT_PATH)
endif()

# ── Void Allomone (the language and the merge) ───────────────────────────────
#
# A SIBLING REPO SINCE 2026-08-29, and compiled FROM SOURCE rather than linked
# as a prebuilt — the opposite of how we consume Void Core, deliberately.
#
# Void Core is C with a C ABI, so a prebuilt shared library is linkable by any
# toolchain. Void Allomone is C++20, and a prebuilt C++ library is only linkable
# by the compiler that built it. More importantly, the C++ API is the one worth
# having here: `std::function` host predicates and `Custom` join laws do not
# cross a C boundary, and routing ourselves through the C ABI to save a
# compilation step would cost us both.
#
# So C++ hosts (Void Maiz, Void Hormiga) take the source; Void Unity — C#, which
# cannot link C++ at all — takes the `voidallomone_c` shared library, which is
# why that ABI exists. `voidmaiz_merge` was the target that lived here between
# 2026-08-29 morning and evening; the sibling repo replaces it, and this is the
# last trace of it.
set(VOIDALLOMONE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidAllomone" CACHE PATH
    "Path to the Void Allomone repository (include/allomone/ + src/)")
if(NOT EXISTS "${VOIDALLOMONE_ROOT}/include/allomone/allomone.hpp")
  message(FATAL_ERROR
    "Void Allomone not found under VOIDALLOMONE_ROOT=${VOIDALLOMONE_ROOT}. "
    "It is a sibling repository: clone it beside this one, or set the cache "
    "variable.")
endif()
# The C ABI is Void Unity's, not ours — we never link it, so do not build it.
set(VOIDALLOMONE_BUILD_C_ABI OFF CACHE BOOL "" FORCE)
add_subdirectory("${VOIDALLOMONE_ROOT}" voidallomone)

# ── The library (static, per developer question #5) ──────────────────────────
add_library(voidmaiz STATIC
  src/embed/embed.cpp
  src/project/project.cpp
  src/gesture/gesture.cpp
  src/gesture/wires.cpp
  src/touch/touch.cpp
  src/presence/presence.cpp
  src/presence/claims.cpp
  src/action/action.cpp
  src/usergraph/usergraph.cpp
  src/allomone/host_predicates.cpp
  src/allomone/sentinel_scene.cpp
  vendor/cJSON.c
)
target_include_directories(voidmaiz
  PUBLIC  include "${VOIDCORE_ROOT}/include"
  PRIVATE vendor
)
target_link_libraries(voidmaiz PUBLIC voidallomone)
# VC_DLL: consuming the core as a shared library on Windows (dllimport) —
# not on Android, where the core is compiled in statically.
# CJSON_HIDE_SYMBOLS: mirror the core — cJSON stays internal.
target_compile_definitions(voidmaiz PRIVATE CJSON_HIDE_SYMBOLS)
if(NOT ANDROID)
  target_compile_definitions(voidmaiz PUBLIC VC_DLL)
endif()
target_link_libraries(voidmaiz PUBLIC "${VOIDCORE_LIBRARY}")
if(NOT MSVC)
  target_compile_options(voidmaiz PRIVATE -Wall -Wextra)
endif()

# Platform facts for LAN discovery (okf/concepts/collaborative-canvas.md, E24-E30):
# addresses, the join code, and Android's Wi-Fi multicast lock. Opens no socket
# (the transport is Void Palabra's). Its own target so an application that never
# networks links no socket library.
add_library(voidmaiz_lan STATIC src/lan/lan.cpp)
target_include_directories(voidmaiz_lan PUBLIC include)
target_compile_features(voidmaiz_lan PUBLIC cxx_std_20)
if(WIN32)
  target_link_libraries(voidmaiz_lan PUBLIC iphlpapi ws2_32)
endif()
if(NOT MSVC)
  target_compile_options(voidmaiz_lan PRIVATE -Wall -Wextra)
endif()

# An application updating itself (okf/concepts/updates.md): asking permission,
# fetching Void Mago's void-updates.json, deciding, downloading, verifying, and
# handing the verified file to the platform. UI-free; the prompt is in
# voidmaiz_view. Its own target so an application that never updates links no
# network code.
add_library(voidmaiz_update STATIC src/update/update.cpp)
target_include_directories(voidmaiz_update PUBLIC include PRIVATE vendor)
target_link_libraries(voidmaiz_update PUBLIC voidmaiz) # shares voidmaiz's cJSON objects
if(NOT MSVC)
  target_compile_options(voidmaiz_update PRIVATE -Wall -Wextra)
endif()

# ── The reduce executor (application-side component; the canvas never calls it)
add_library(voidmaiz_reduce STATIC src/reduce/reduce.cpp)
target_include_directories(voidmaiz_reduce PUBLIC include PRIVATE vendor)
target_link_libraries(voidmaiz_reduce PUBLIC voidmaiz) # shares voidmaiz's cJSON objects
if(NOT MSVC)
  target_compile_options(voidmaiz_reduce PRIVATE -Wall -Wextra)
endif()

# ── Allomone ────────────────────────────────────────────────────────────────
#
# `voidmaiz_allomone` is now an ALIAS, kept because Void Hormiga's CMake links
# it by that name and an extraction whose first act is breaking its only
# adopter's build has failed at the thing it was for. The language itself is
# `voidallomone` above; everything Void Maiz adds on top (the `with`/`device`
# host predicates, `effect_scene`) lives in `voidmaiz`, so the alias points
# there and a host that links it gets exactly what it got before.
add_library(voidmaiz_allomone INTERFACE)
target_link_libraries(voidmaiz_allomone INTERFACE voidmaiz)

# ── Headless: the same application with no front-end ────────────────────────
# okf/concepts/headless.md. Its own target for the same reason Allomone has
# one: a GUI app that will never run headless links only `voidmaiz`, and this
# target links no ImGui, so a headless binary carries no window system at all.
add_library(voidmaiz_headless STATIC src/headless/headless.cpp)
target_include_directories(voidmaiz_headless PUBLIC include PRIVATE vendor)
target_link_libraries(voidmaiz_headless PUBLIC voidmaiz)
target_compile_definitions(voidmaiz_headless PRIVATE CJSON_HIDE_SYMBOLS)
if(NOT MSVC)
  target_compile_options(voidmaiz_headless PRIVATE -Wall -Wextra)
endif()

# ── Networking, stage C: Void Palabra's sync (optional) ──────────────────────
# okf/concepts/networking.md. The ONLY target that links Void Palabra, and it is
# built only when the sibling repo is present: an application that never
# networks never needs Palabra checked out, and the build proves it.
#
# Palabra is embedded the way it asks to be (its CMakeLists §"Consumed two ways")
# — as a target, never a copied source list; the one list we did copy (Void
# Core's, for Android) went months without a file. VENDOR_CJSON=OFF because we
# already compile cJSON: one copy per process.
set(VOIDPALABRA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidPalabra" CACHE PATH
    "Path to the Void Palabra repository (optional: enables voidmaiz_net)")
if(EXISTS "${VOIDPALABRA_ROOT}/include/voidpalabra/sync.hpp")
  set(VOIDPALABRA_VENDOR_CJSON OFF CACHE BOOL "" FORCE)
  add_subdirectory("${VOIDPALABRA_ROOT}" voidpalabra)
  add_library(voidmaiz_net STATIC src/net/net.cpp)
  target_include_directories(voidmaiz_net PUBLIC include PRIVATE vendor)
  target_link_libraries(voidmaiz_net PUBLIC voidmaiz voidpalabra::voidpalabra)
  target_compile_definitions(voidmaiz_net PRIVATE CJSON_HIDE_SYMBOLS)
  if(NOT MSVC)
    target_compile_options(voidmaiz_net PRIVATE -Wall -Wextra)
  endif()
  set(VOIDMAIZ_HAVE_NET ON)
else()
  message(STATUS "Void Palabra not found at ${VOIDPALABRA_ROOT}: voidmaiz_net is not built")
  set(VOIDMAIZ_HAVE_NET OFF)
endif()

# ── Vendored UI substrate (Phase 2): Dear ImGui over the platform's GL ──────
# Desktop: GLFW + OpenGL 3. Android: NativeActivity glue + GLES3 — GLFW has no
# Android port; the platform shim in the app repo owns the EGL context.
set(VOIDMAIZ_IMGUI_SOURCES
  vendor/imgui/imgui.cpp
  vendor/imgui/imgui_draw.cpp
  vendor/imgui/imgui_tables.cpp
  vendor/imgui/imgui_widgets.cpp
  vendor/imgui/imgui_demo.cpp
  vendor/imgui/backends/imgui_impl_opengl3.cpp
)
if(ANDROID)
  add_library(voidmaiz_imgui STATIC
    ${VOIDMAIZ_IMGUI_SOURCES}
    vendor/imgui/backends/imgui_impl_android.cpp
  )
  target_include_directories(voidmaiz_imgui PUBLIC vendor/imgui vendor/imgui/backends)
  target_compile_definitions(voidmaiz_imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
  target_link_libraries(voidmaiz_imgui PUBLIC GLESv3 EGL android log)
else()
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
  add_subdirectory(vendor/glfw)

  find_package(OpenGL REQUIRED)

  add_library(voidmaiz_imgui STATIC
    ${VOIDMAIZ_IMGUI_SOURCES}
    vendor/imgui/backends/imgui_impl_glfw.cpp
  )
  target_include_directories(voidmaiz_imgui PUBLIC vendor/imgui vendor/imgui/backends)
  target_link_libraries(voidmaiz_imgui PUBLIC glfw OpenGL::GL)
endif()

# ── The view module: canvas rendering (our code that needs ImGui types) ──────
add_library(voidmaiz_view STATIC
  src/view/canvas.cpp
  src/view/code.cpp
  src/view/face.cpp
  src/view/inspector.cpp
  src/view/mobile.cpp
  src/view/netview.cpp
  src/view/updateview.cpp
  src/view/widget.cpp
  src/view/widgets.cpp
)
target_link_libraries(voidmaiz_view PUBLIC voidmaiz voidmaiz_imgui voidmaiz_update)
if(NOT MSVC)
  target_compile_options(voidmaiz_view PRIVATE -Wall -Wextra)
endif()

# ── Examples & tests (desktop only — Android hosts them through the app) ────
if(ANDROID)
  return()
endif()

add_executable(maiz_hello examples/hello_window.cpp)
target_link_libraries(maiz_hello PRIVATE voidmaiz voidmaiz_imgui)

add_executable(maiz_canvas examples/canvas_window.cpp)
target_link_libraries(maiz_canvas PRIVATE voidmaiz_view)

# The touch shell, on a desktop: a phone-proportioned window driven entirely
# through maiz::TouchRecognizer, with the mouse as a finger (Alt adds a second).
# The reference a mobile host copies — and the only way to LOOK at the mobile
# kit without building an APK.
add_executable(maiz_mobile examples/mobile_window.cpp)
target_link_libraries(maiz_mobile PRIVATE voidmaiz_view)

# Networking, the half an application shows: three unrelated views, one
# declaration each, identical marks; simulated peers through the real codec.
add_executable(maiz_network examples/network_window.cpp)
target_link_libraries(maiz_network PRIVATE voidmaiz_view)

# The Allomone test vehicle: script workbench + tagged playground.
add_executable(maiz_allomone examples/allomone_playground.cpp)
target_link_libraries(maiz_allomone PRIVATE voidmaiz_view voidmaiz_allomone)

# ── Smoke test (Phase 1 exit: replay a state document and walk its nodes) ────
enable_testing()
add_executable(maiz_smoke tests/embed_smoke.cpp)
target_link_libraries(maiz_smoke PRIVATE voidmaiz)
add_test(NAME embed_smoke COMMAND maiz_smoke)

add_executable(maiz_project_smoke tests/project_smoke.cpp)
target_link_libraries(maiz_project_smoke PRIVATE voidmaiz)
add_test(NAME project_smoke COMMAND maiz_project_smoke)

add_executable(maiz_gesture_smoke tests/gesture_smoke.cpp)
target_link_libraries(maiz_gesture_smoke PRIVATE voidmaiz)
add_test(NAME gesture_smoke COMMAND maiz_gesture_smoke)

# The touch recognizer, driven with literal contact sets — no window, no
# device. The point of keeping it on the UI-free side is that this test exists.
add_executable(maiz_touch_smoke tests/touch_smoke.cpp)
target_link_libraries(maiz_touch_smoke PRIVATE voidmaiz)
add_test(NAME touch_smoke COMMAND maiz_touch_smoke)

add_executable(maiz_replay_smoke tests/replay_smoke.cpp)
target_link_libraries(maiz_replay_smoke PRIVATE voidmaiz)
add_test(NAME replay_smoke COMMAND maiz_replay_smoke)

add_executable(maiz_action_smoke tests/action_smoke.cpp)
target_link_libraries(maiz_action_smoke PRIVATE voidmaiz)
add_test(NAME action_smoke COMMAND maiz_action_smoke)

# The two pieces that did NOT move to Void Allomone, tested where they live.
add_executable(maiz_effect_scene_smoke tests/effect_scene_smoke.cpp)
target_link_libraries(maiz_effect_scene_smoke PRIVATE voidmaiz)
add_test(NAME effect_scene_smoke COMMAND maiz_effect_scene_smoke)

# `with` and `device` after they stopped being kernel predicates: the proof that
# the extraction cost no behaviour, only a registration.
add_executable(maiz_host_predicates_smoke tests/host_predicates_smoke.cpp)
target_link_libraries(maiz_host_predicates_smoke PRIVATE voidmaiz)
add_test(NAME host_predicates_smoke COMMAND maiz_host_predicates_smoke)

# The whole chain in one test: recognizer -> observed channel -> UserGraph ->
# host predicates -> the SIBLING REPO's evaluator. Links voidmaiz_allomone so a
# broken re-export fails here rather than in a host.
add_executable(maiz_touch_usergraph_smoke tests/touch_usergraph_smoke.cpp)
target_link_libraries(maiz_touch_usergraph_smoke PRIVATE voidmaiz_allomone)
add_test(NAME touch_usergraph_smoke COMMAND maiz_touch_usergraph_smoke)

# Networking, the half an application shows (okf/concepts/networking.md): no
# socket, no Palabra. Links voidmaiz_allomone for the presence predicates and
# the Allomone-answered share filter.
add_executable(maiz_presence_smoke tests/presence_smoke.cpp)
target_link_libraries(maiz_presence_smoke PRIVATE voidmaiz_allomone)
add_test(NAME presence_smoke COMMAND maiz_presence_smoke)

# The collaborative canvas, UI-free half: in-flight gestures in presence, and
# first-to-select claims for people and agents alike. No socket, no Palabra.
add_executable(maiz_collab_smoke tests/collab_smoke.cpp)
target_link_libraries(maiz_collab_smoke PRIVATE voidmaiz)
add_test(NAME collab_smoke COMMAND maiz_collab_smoke)

# Connections as wire runes: concurrent rewires commute (collaborative-canvas §4.2).
add_executable(maiz_wires_smoke tests/wires_smoke.cpp)
target_link_libraries(maiz_wires_smoke PRIVATE voidmaiz_reduce)
add_test(NAME wires_smoke COMMAND maiz_wires_smoke)

add_executable(maiz_update_smoke tests/update_smoke.cpp)
target_link_libraries(maiz_update_smoke PRIVATE voidmaiz_update)
add_test(NAME update_smoke COMMAND maiz_update_smoke)

add_executable(maiz_lan_smoke tests/lan_smoke.cpp)
target_link_libraries(maiz_lan_smoke PRIVATE voidmaiz_lan)
add_test(NAME lan_smoke COMMAND maiz_lan_smoke)

# Stage C: real Cores syncing through Void Palabra over an in-memory lossy wire.
# Only when Palabra is present — the same condition as the target itself.
if(VOIDMAIZ_HAVE_NET)
  add_executable(maiz_net_smoke tests/net_smoke.cpp)
  target_link_libraries(maiz_net_smoke PRIVATE voidmaiz_net)
  target_include_directories(maiz_net_smoke PRIVATE vendor)
  add_test(NAME net_smoke COMMAND maiz_net_smoke)
endif()

add_executable(maiz_usergraph_smoke tests/usergraph_smoke.cpp)
target_link_libraries(maiz_usergraph_smoke PRIVATE voidmaiz)
add_test(NAME usergraph_smoke COMMAND maiz_usergraph_smoke)


add_executable(maiz_command_smoke tests/command_smoke.cpp)
target_link_libraries(maiz_command_smoke PRIVATE voidmaiz_allomone)
add_test(NAME command_smoke COMMAND maiz_command_smoke)


# The minimal host integration, registered as a TEST as well as an example:
# it is the first thing a new client copies, so it must not be allowed to rot.
# Deliberately links no view module — proof that Allomone works headless.
add_executable(maiz_allomone_minimal examples/allomone_minimal.cpp)
target_link_libraries(maiz_allomone_minimal PRIVATE voidmaiz_allomone)
add_test(NAME allomone_minimal COMMAND maiz_allomone_minimal)

# A whole application with no front-end (okf/concepts/headless.md). Doubles as
# the headless integration test: --selftest checks the claims that are most
# expensive to be wrong about (changes persist, a second session sees them, the
# lock refuses a concurrent writer, --atomic rolls back).
add_executable(maiz_headless examples/headless_app.cpp)
target_link_libraries(maiz_headless PRIVATE voidmaiz_headless)
add_test(NAME headless_smoke COMMAND maiz_headless --selftest)



add_executable(maiz_reduce_conformance tests/reduce_conformance.cpp)
target_link_libraries(maiz_reduce_conformance PRIVATE voidmaiz_reduce)
target_include_directories(maiz_reduce_conformance PRIVATE vendor)
add_test(NAME reduce_conformance
         COMMAND maiz_reduce_conformance "${VOIDCORE_ROOT}/../conformance/reduce/cases")

if(WIN32 AND VOIDCORE_DLL)
  add_custom_command(TARGET maiz_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VOIDCORE_DLL}" "$<TARGET_FILE_DIR:maiz_smoke>")
endif()

# ── the toolchain runtime, next to the binaries (Windows/MinGW) ──────────────
#
# WHY THIS EXISTS. Windows searches the EXE'S OWN DIRECTORY before PATH, and a
# shell whose PATH carries a *different* MinGW is the normal case here, not an
# exotic one: MSYS2's Git Bash puts Git-for-Windows' `/mingw64/bin` ahead of the
# UCRT64 toolchain that actually compiled these binaries. The foreign
# `libstdc++-6.dll` then loads and the process dies at 0xc0000139
# (STATUS_ENTRYPOINT_NOT_FOUND) before `main`.
#
# It looked environmental and random for a month — reported by Void Hormiga
# 2026-08-17, and recorded on our side as "environmental, not real" — because it
# hit EXACTLY ONE target. `reduce_conformance` is the only binary that uses
# <filesystem>, and `std::filesystem`'s symbols are precisely what an older
# libstdc++ lacks; every other test links fine against the wrong DLL because it
# never asks for anything the wrong DLL is missing. A partial ABI match is worse
# than no match, because it fails one target at a time and reads as a fluke.
#
# Copying the runtime next to the executables makes the directory search win, so
# the binaries load the runtime they were BUILT against no matter which shell
# launches them — ctest, Git Bash, PowerShell or a double-click.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  get_filename_component(_maiz_toolchain_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(_maiz_runtime_dlls "")
  foreach(_dll libstdc++-6.dll libgcc_s_seh-1.dll libgcc_s_dw2-1.dll libwinpthread-1.dll)
    if(EXISTS "${_maiz_toolchain_bin}/${_dll}")
      list(APPEND _maiz_runtime_dlls "${_maiz_toolchain_bin}/${_dll}")
    endif()
  endforeach()
  if(_maiz_runtime_dlls)
    add_custom_command(TARGET maiz_smoke POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${_maiz_runtime_dlls} "$<TARGET_FILE_DIR:maiz_smoke>"
      COMMENT "Staging the MinGW runtime next to the binaries (loader-order fix)")
  endif()
endif()
