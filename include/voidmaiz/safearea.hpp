/* voidmaiz/safearea.hpp — the parts of a phone's screen that are the SYSTEM's.
 *
 * No ImGui here on purpose: the Android half (src/input/textinput_android.cpp)
 * lives in the base library, which does not see ImGui. Reserving the area in a
 * frame is voidmaiz/mobile.hpp's reserve_safe_area.
 *
 * A phone's app surface runs under the status bar, the display cutout and the
 * navigation area. On a gesture-navigation phone (no buttons, swipe up for
 * home) the bottom strip is not tappable at all: the system takes every touch
 * there as the start of a gesture. The author's phone is exactly that, and the
 * first Hormiga APK put its navigation bar there (2026-09-25: "i can barely tap
 * on the navigation bar at all"). Pixels, in the framebuffer's units. */
#pragma once

struct ANativeActivity; // Android's; only a pointer passes through here

namespace maiz {

struct SafeArea {
    float left = 0, top = 0, right = 0, bottom = 0;
    bool empty() const { return left <= 0 && top <= 0 && right <= 0 && bottom <= 0; }
};

/* Android: the system bars and the display cutout, and at the bottom at least
 * the mandatory gesture strip, read from org.voidmaiz.MaizActivity. Cheap
 * enough to call every half second; they change on rotation and when the
 * navigation mode does. All zero on every other platform, or when the activity
 * is a plain NativeActivity. */
SafeArea android_safe_area(ANativeActivity* activity);

} // namespace maiz
