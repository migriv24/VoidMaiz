---
type: Concept
title: Portable apps
description: "One maiz::App, written once, run by the library's shell on a desktop, an Android phone and an iPhone. The Device answers what a platform can do and what its rules are; the shell owns the window, the GPU, the events and the frame order. Why the library took the shell from the hosts, how a host ports, and what has been verified where."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-10-17T00:00:00Z
---

# Portable apps

**The author, 2026-10-17:** *"we are migrating to iOS soon. I want to soon start
creating a iOS and Android app for applications like Hormiga … I want it to be
as easy as possible for other void maiz based applications to easily port their
code onto iOS."*

The easy part of that sentence is the one that sounds hard. Drawing on an
iPhone is a solved problem: ImGui has a Metal backend, and nothing in
`voidmaiz_view` calls GL. **The hard part is that every host wrote its own
shell.** Interaction Combinators' Android shell is 234 lines and Void Hormiga's
is 388, and most of those lines are the same lines: EGL up and down with the
window, the touch gate fed sample by sample, the safe area re-read twice a
second, fonts out of the APK, textures, saving on pause. iOS, done the same
way, means every host writing all of that again, in Objective-C++, against
UIKit and Metal, on a machine none of them owns. Porting would cost each host a
second shell per platform, forever.

So the shell is the library's now. A host writes **one `maiz::App`**:

```cpp
class MyApp : public maiz::App {
    void start(maiz::Device& d) override { /* open the document under d.data_dir() */ }
    void frame(maiz::Device& d) override { /* ImGui: bars, screens, the canvas */ }
    void event(maiz::Device& d, maiz::AppEvent e) override {
        if (e == maiz::AppEvent::Background) save(); // a phone may never speak again
    }
};
std::unique_ptr<maiz::App> maiz::create_app() { return std::make_unique<MyApp>(); }
```

and one CMake call, `maiz_add_app(my_app SOURCES … NAME … BUNDLE_ID …)`
([cmake/VoidMaizApp.cmake](/../cmake/VoidMaizApp.cmake)). The same source then
builds as a desktop executable, an Android `.so` and an iOS app bundle.

Nothing here changes commitment 1. An App is a host like any other: it holds a
Core or a Session, every change is a dispatched command, every screen is a
projection. The shell is plumbing between a device and a host.

## The two halves

**[`device.hpp`](/../include/voidmaiz/device.hpp)** (ImGui-free) says what the
device is and what it can do. **[`app.hpp`](/../include/voidmaiz/app.hpp)** is the
class a host writes.

### The Device states rules, not just a name

The things that actually differ between phones are mostly not features. They
are **rules**:

| rule | Android | iOS | desktop |
|---|---|---|---|
| may the app quit itself? (`can_quit`) | yes | **no**: Apple reads a self-exit as a crash | yes |
| does it update itself? (`self_updates`) | yes ([updates](/concepts/updates.md)) | **no**: the App Store does | yes |
| what is Back? (`edge_back`) | a key / the system's gesture | **a swipe from the left edge, and a chevron that must be shown** | Escape, the mouse's back button |
| what happens after Background? | maybe nothing ever again | maybe nothing ever again; and **no GPU work** | usually you come back |

A host that branches on `platform() == Platform::iOS` writes each rule again,
and is wrong on the next platform. A host that reads `can_quit()` and
`self_updates()` writes none of them. So the Device answers the questions, and
`platform()` exists for what nothing else covers (and for the `platform`
predicate below).

**The principle under it: behaviour follows the platform, appearance follows
the app.** Void Maiz does not draw an iOS look on iOS or Material on Android. It
makes the app *behave* the way a hand on that phone expects (the edge swipe, no
Quit, saving when backgrounded) and leaves the look to the application, which
is the family's look everywhere.

### Services are asked of the Device, and may be absent

`text_input()`, `http(user_agent)`, `open_url`, `share`, `haptic`,
`pick_document` / `save_document`, `create_texture` / `destroy_texture`,
`read_asset`, `data_dir` / `cache_dir`. Null or false means "not on this
device", and a host draws no control for it, which is the rule Void Hormiga's
shell already followed with its optional hooks ("the app treats an absent hook
as not on this device"). Location and the radios stay process-wide
(`maiz::location()`, `maiz::radio()`), installed by the shell, because they
already were.

`maiz::device()` returns the installed Device for code deep in a view that wants
a haptic without being handed one.

### Units: pixels, and density = px per dp

Every length crossing these headers is in **framebuffer pixels**, as on Android
already, and `density()` is pixels per dp. iOS's point is the same idea as
Android's dp (one finger-sized unit), so on an iPhone 16 the density is 3 and
the safe area arrives as 177 px at the top. A layout written in
`n * density()` is the same physical size everywhere, and the touch gate's
thresholds (in dp) need no per-platform tuning.

## The frame, the same on every platform

[`src/shell/frame.cpp`](/../src/shell/frame.cpp) is the one order every shell
runs:

1. documents that arrived since the last frame (a pick, an "Open with"),
   delivered between frames, never inside one;
2. `text_input_frame`: the platform keyboard (**the app no longer calls it**;
   Void Hormiga's phone front-end does today and must stop when it ports);
3. `reserve_safe_area`: so every bar the app draws lands inside it;
4. `touch_gate_frame`: taps, scrolls, pinches, and on iOS the edge swipe;
5. `app.frame()`;
6. if Back was asked for and nothing claimed it (`back_pressed`,
   [nav.hpp](/../include/voidmaiz/nav.hpp)), the platform's default: Android
   goes to the background, as Android 12+ does for every app; iOS and a desktop
   do nothing.

## The three shells

| | desktop | Android | iOS |
|---|---|---|---|
| file | `shell_desktop.cpp` | `shell_android.cpp` | `shell_ios.mm` |
| window / GPU | GLFW + GL 3 | NativeActivity + EGL / GLES 3 | UIKit scene + MTKView / Metal |
| fingers | the mouse, through the gate (in a preview) | the gate, with historical samples | the gate, with coalesced samples |
| keyboard | the physical one | MaizActivity's InputConnection | a UIKeyInput view (stage 1) |
| lifecycle | iconify, F9 in a preview, close | pause / resume / low memory / window lost | scene background / foreground / memory warning |
| verified | **built, run, screenshotted** | **compiled** (NDK r28, arm64; the dex's signatures checked) | **written; compiled only by CI** ([ios](/concepts/ios.md)) |

## The preview: an iPhone on a Windows machine

```
maiz_portable --device iphone-16            # or iphone-se, iphone-16-pro-max, ipad-11, pixel-8, android-small
maiz_portable --device iphone-16 --landscape
maiz_portable --list-devices
```

The window is the phone's screen in points, scaled to fit the monitor. Drawn
over the app: the rounded corners, the Dynamic Island or notch or punch hole,
the status bar's clock, the home indicator or gesture pill. Those are the
places a phone's own hardware covers, so a bar that sits under one is visible as
a mistake. `platform()` is the phone's, so **the rules are the phone's**: no
Quit on an iPhone, the edge swipe goes Back, and the same swipe on a Pixel does
nothing (Android's system owns that gesture). The mouse is a finger, Alt adds a
mirrored second one for pinching, the keyboard is this machine's. F9 sends the
app to the background and back, which is how to watch it save.

For an agent, a harness or CI, there is no window to look at:

```
maiz_portable --device iphone-16 --screenshot out.png --frames 40 \
  --input "10:down 120 135;12:up 120 135;20:down 4 400;22:move 120 401;26:up 220 402"
```

touches at frame numbers in dp (also `N:back`, `N:background`, `N:foreground`).
That line is how this page's claims about the edge swipe were checked: tap a
row, the note opens with a chevron; swipe in from the edge, the screen shades
as the finger moves and nothing under it is pressed; release past a third of
the width, and the list is back. On the Android profile the same input leaves
the note open, as it should.

**What a preview is not: an emulator.** Layout, safe area and rules are the
phone's. The GPU, the keyboard and the event plumbing are this machine's.

## Porting an existing host

The reference is [`examples/portable_app.cpp`](/../examples/portable_app.cpp):
three screens under a navigation bar, a stack per screen, Back from wherever the
platform puts it, a Session saved on Background, every service asked of the
Device. For a host with its own Android shell today (Void Hormiga, Interaction
Combinators), the move is mostly deletion:

1. **Make the app class an `App`.** Hormiga's `HormigaApp` already has the
   shape: `init` is `start`, `frame` is `frame`, `shutdown`-on-pause becomes
   `event(Background)`, `document_arrived` becomes `document`.
2. **Replace each host hook with the Device.** `on_pick_document` becomes
   `device.pick_document`, `on_http_get` becomes `device.http(ua)`,
   `on_load_texture` becomes `device.create_texture` after decoding (a host's
   `glTexImage2D` will not compile on iOS), `on_quit` becomes `device.quit()`
   behind `can_quit()`, `base_dir` becomes `data_dir()`.
3. **Fonts move to `App::fonts`,** read with `device.read_asset` and sized by
   `density()`. List them in `maiz_add_app(RESOURCES …)`.
4. **Delete the shell** (`android/src/main_android.cpp`) and the call to
   `text_input_frame`. Build with `maiz_add_app`; the APK script packages
   `lib<app>.so` as before.
5. **Things only the host can decide stay the host's**: which screens exist,
   what Back means on each, what is saved, the multicast lock (reach it with
   `maiz::android_activity()` until it is a Device service; every use of that
   escape hatch is an iOS port someone has to write).

What does **not** port by itself: anything that shells out (`curl`, `tar`, a
browser via `system()`). iOS has no processes. That is the one class of host
code the move to iOS forces out, and `Device::http` is where it goes.

## Allomone can read the device

[`register_device_predicates`](/../include/voidmaiz/allomone.hpp) (asked by Void
Hormiga, 2026-09-23):

```
when tag "overdue" -> tint "red"                                fires everywhere
when form "phone" -> chips "collapsed"                          a stated difference
when platform "ios" and surface "table:data" -> swipe "delete"
when shown_as "sheet" -> compact 1
```

`platform` and `form` are device facts (the same for every subject); `surface`
and `shown_as` read the frame's surface declarations, which now carry a
**form** (`window`, `screen`, `sheet`, `panel`, `bar`) beside the id. The rule
for ids, Hormiga's and now the library's: **a surface's id is the concept,
never the form.** A phone's Data screen and a desktop's Data window both
declare `table:data`, so a peer reads "in Data" whatever device someone is on.
Pinned by `host_predicates_smoke` (one script, a phone and a desktop, different
answers only where the rule says so).

## What this does not do yet

- **No compact landscape bar.** In landscape the navigation bar is as tall as
  in portrait; iOS uses a compact tab bar there. A `nav_bar` detail, not a
  contract change.
- **The keyboard does not lift the work area.** `covered_px` is reported on
  both platforms; nothing yet keeps a focused field above the keyboard.
- **No interactive slide.** The edge swipe shades the screen as it moves
  (`TouchGate::edge_progress`); it does not slide the previous screen in.
- **Android shares text, not files** (a file needs a FileProvider, and the APK
  has no AndroidX). iOS shares both.
- **Neither phone shell has run on a device.** See [ios](/concepts/ios.md) for
  what CI will and will not prove.
