/*
 * voidmaiz/location.hpp — where this device is: the location holiday.
 *
 * Void Hormiga's author, 2026-10-04, on the map coming to the phone: "a
 * functionality for gps or something, where we know where the phone is ... it
 * could be a thing the app asks for permission to know the location of the
 * device." Every phone application that marks places will want the same three
 * things, so it is here and not in a host: ask, follow, stop.
 *
 * ── A PERMISSION, ASKED WHEN IT MEANS SOMETHING ─────────────────────────────
 * Unlike the photo picker (documents.hpp), there is no permission-free way to
 * know where a phone is: Android's ACCESS_FINE_LOCATION / ACCESS_COARSE_LOCATION
 * are runtime permissions and the SYSTEM draws the prompt. So `request()` is
 * the only thing that may ever cause a prompt, and a host calls it from a
 * person's tap on a "where am I" control, never at start-up. The field
 * evidence is that the moment of asking matters more than the words around it
 * (a 2,579-person experiment found a stated reason made no measurable
 * difference to the answer; what the application was, and when it asked, did),
 * so the library gives a host no way to ask early by accident.
 *
 * Android since 12 lets the person answer "approximate" instead of "precise".
 * That is a real answer and is reported as `Approximate`, not as a failure: a
 * fix arrives with an accuracy of a kilometre or so, and the host draws it as
 * that (a big circle), not as a pin it does not have.
 *
 * ── A FIX IS A CLAIM WITH A RADIUS ──────────────────────────────────────────
 * `LocationFix::accuracy_m` is the platform's 68% radius. A host that places a
 * thing "where I am" should carry the accuracy with it, or at least show it,
 * because a phone indoors can be forty metres out and say so honestly.
 *
 * ── THE SHAPE: one platform per device ──────────────────────────────────────
 * Like the keyboard (textinput.hpp), the platform is an object a shell makes
 * and hands over; unlike the keyboard there is one per device, not one per
 * window, so it is installed process-wide, the way default_touch_gate() is:
 *
 *     maiz::install_location(maiz::android_location(app->activity));  // the shell, once
 *     if (auto* loc = maiz::location()) loc->request(true);           // a person's tap
 *     maiz::LocationFix fix; if (loc && loc->latest(fix)) draw(fix);   // every frame
 *
 * A desktop has none (location() returns null and the control is not drawn).
 * `FixedLocation` stands in for a test or a phone harness: a fix set by hand,
 * and a permission that can be set to say no.
 *
 * ImGui-free, like documents.hpp; the Android half is src/input/location.cpp
 * and MaizActivity.java (maizLocation*). The manifest needs
 * android.permission.ACCESS_FINE_LOCATION and ACCESS_COARSE_LOCATION.
 */
#pragma once

#include <memory>
#include <string>

struct ANativeActivity;

namespace maiz {

enum class LocationAccess {
    Unasked,     // nobody has asked; a request() will show the system's prompt
    Asking,      // the system's prompt is on screen
    Precise,     // granted, precise
    Approximate, // granted, approximate only (Android 12+: the person chose it)
    Denied,      // refused; the system will not show its prompt again for this app
    Unavailable, // no location hardware, or location is switched off in settings
};

const char* access_name(LocationAccess a); // "unasked", "precise", … for logs and tests

struct LocationFix {
    double lat = 0, lon = 0;
    float accuracy_m = 0;    // 68% radius, metres (0 = unknown)
    double age_s = 0;        // how old the fix is, seconds
    std::string provider;    // "gps", "network", "fused", "fixed"
};

class LocationPlatform {
  public:
    virtual ~LocationPlatform() = default;
    virtual LocationAccess access() = 0;
    /* Ask (the system prompt, if it has not been answered) and start following.
     * Call from a person's tap. Returns false when nothing could even be asked. */
    virtual bool request(bool precise) = 0;
    /* Stop following: the GPS costs battery, so a host stops when its map is not
     * on screen. Access is kept; request() again resumes without a prompt. */
    virtual void stop() = 0;
    /* The newest fix, if there is one. Cheap: call it every frame. */
    virtual bool latest(LocationFix& out) = 0;
    virtual bool following() const = 0;
};

/* The process-wide platform (null until a shell installs one). */
void install_location(std::unique_ptr<LocationPlatform> platform);
LocationPlatform* location();

/* Android: LocationManager through MaizActivity. Null off Android. */
std::unique_ptr<LocationPlatform> android_location(ANativeActivity* activity);

/* A stand-in: a fix you set, a permission you set. For tests and harnesses. */
class FixedLocation : public LocationPlatform {
  public:
    LocationAccess grant = LocationAccess::Precise; // what request() turns Unasked into
    LocationAccess state = LocationAccess::Unasked;
    bool has_fix = false;
    LocationFix fix;
    bool on = false;

    LocationAccess access() override { return state; }
    bool request(bool) override {
        if (state == LocationAccess::Unasked || state == LocationAccess::Asking) state = grant;
        on = state == LocationAccess::Precise || state == LocationAccess::Approximate;
        return true;
    }
    void stop() override { on = false; }
    bool latest(LocationFix& out) override {
        if (!on || !has_fix) return false;
        out = fix;
        return true;
    }
    bool following() const override { return on; }
    void set(double lat, double lon, float accuracy_m) {
        fix.lat = lat;
        fix.lon = lon;
        fix.accuracy_m = accuracy_m;
        fix.provider = "fixed";
        has_fix = true;
    }
};

} // namespace maiz
