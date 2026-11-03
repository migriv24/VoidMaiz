/* voidmaiz/app.hpp — a portable application: write it once, and the library
 * runs it on a desktop, on Android and on iOS.
 *
 * (2026-10-17, opened for iOS. okf/concepts/portable-apps.md is the design.)
 *
 * ── WHY THE LIBRARY OWNS THE SHELL NOW ──────────────────────────────────────
 * Until today every host wrote its own platform shell: Interaction Combinators
 * 234 lines of Android NativeActivity, Void Hormiga 388, and most of those
 * lines the same ones (EGL, the touch gate fed sample by sample, the safe area
 * re-read twice a second, fonts out of the APK, textures, save on pause). iOS
 * would have meant every host writing the same thing again in Objective-C++,
 * against UIKit and Metal, on a machine none of them have. That is the cost
 * this header removes. A host writes:
 *
 *     class MyApp : public maiz::App {
 *         void start(maiz::Device& d) override { ... open the document in d.data_dir() ... }
 *         void frame(maiz::Device& d) override { ... ImGui, the nav bar, screens ... }
 *         void event(maiz::Device& d, maiz::AppEvent e) override {
 *             if (e == maiz::AppEvent::Background) save();
 *         }
 *     };
 *     std::unique_ptr<maiz::App> maiz::create_app() { return std::make_unique<MyApp>(); }
 *
 * links `voidmaiz_app`, and calls `maiz_add_app()` in CMake
 * (cmake/VoidMaizApp.cmake). The SAME file then builds as:
 *   - a desktop window, and with `--device iphone-16` a preview of that phone
 *     (its screen, its safe area, its rules, the mouse as a finger);
 *   - an Android .so whose NativeActivity shell is the library's;
 *   - an iOS app bundle whose UIKit + Metal shell is the library's.
 *
 * ── THE FRAME ───────────────────────────────────────────────────────────────
 * Before `frame` runs, the shell has already, in this order: started the ImGui
 * frame; run the platform keyboard (text_input_frame: the app does NOT call it);
 * reserved the safe area (so the app's bars land inside it); and run the touch
 * gate (taps, scrolls, pinches, and on iOS the edge swipe). After `frame`, if
 * the person asked for Back and nothing claimed it (nav.hpp's back_pressed),
 * the shell does the platform's default: Android goes to the background, iOS
 * does nothing, a desktop does nothing.
 *
 * Nothing here owns model state. An App is a host like any other: it holds a
 * Core or a Session, dispatches every change as a command, and draws
 * projections. The shell is plumbing between the device and that host.
 *
 * View-module header (ImGui runs inside `frame`).
 */
#pragma once

#include "voidmaiz/device.hpp"
#include "voidmaiz/documents.hpp" // DocumentResult

#include <memory>

namespace maiz {

class App {
  public:
    virtual ~App() = default;

    /* Fonts and style, once, before the first frame. The ImGui context exists
     * and the device's density is known. If the app adds no font, the shell
     * adds ImGui's default at 15 dp. */
    virtual void fonts(Device& device) { (void)device; }

    /* Once per process, before the first frame (after fonts). Open the
     * document here, under device.data_dir(). */
    virtual void start(Device& device) = 0;

    /* Every frame, inside the ImGui frame. */
    virtual void frame(Device& device) = 0;

    /* What happened from outside: see AppEvent. Background is the one that
     * must save. */
    virtual void event(Device& device, AppEvent e) {
        (void)device;
        (void)e;
    }

    /* A picked or saved document (Device::pick_document / save_document), or
     * a file the app was opened WITH (request -1: "Open with", a share into
     * the app). Delivered between frames, never inside one. */
    virtual void document(Device& device, const DocumentResult& r) {
        (void)device;
        (void)r;
    }
};

/* THE ONE FUNCTION A HOST DEFINES. The shell calls it once. */
std::unique_ptr<App> create_app();

/* Escape hatch: the Android activity, for a holiday the Device does not cover
 * yet (lan.hpp's MulticastLock takes one). Null off Android, and before the
 * shell started. Prefer a Device service: every use of this is a port to iOS
 * that someone will have to write. */
ANativeActivity* android_activity();

} // namespace maiz
