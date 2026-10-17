/* voidmaiz/device.hpp — what the device is, and what it can do for an app.
 *
 * The other half of voidmaiz/app.hpp. A portable application (one `maiz::App`
 * that runs on a desktop, on Android and on iOS) never asks "which platform am
 * I on?" to find a service. It asks its Device, and a service that is not on
 * this device answers null or false. The shell (the library's, one per
 * platform) makes the Device; the app only ever reads it.
 *
 * ── WHY THE DEVICE SAYS CONVENTIONS, NOT JUST ITS NAME ──────────────────────
 * The things that actually differ between phones are mostly not features but
 * RULES: an iOS app never quits itself (Apple reads a self-exit as a crash),
 * never updates itself (the App Store does), and has no Back button (a swipe
 * from the left edge, and a chevron that must always be shown). A host that
 * branches on `platform() == Platform::iOS` writes each rule again; a host that
 * reads `can_quit()` and `self_updates()` writes none, and is right on the next
 * platform too. So the Device answers the questions, and `platform()` is there
 * for the rare case nothing else covers (and for the user graph's `platform`
 * predicate). The principle under it: BEHAVIOUR FOLLOWS THE PLATFORM,
 * APPEARANCE FOLLOWS THE APP. Void Maiz does not draw an iOS look on iOS; it
 * makes the app behave the way a hand on that phone expects.
 *
 * ── UNITS ───────────────────────────────────────────────────────────────────
 * Every length crossing this header is in FRAMEBUFFER PIXELS, as on Android
 * already, and `density()` is pixels per dp (Android's dp, iOS's point: the
 * same idea, one finger-sized unit). So a safe area, a touch and an ImGui
 * coordinate are one unit everywhere, and a layout written in `n * density()`
 * is the same size on every device.
 *
 * ImGui-free: no frame loop here, so a test can install a fake Device and the
 * base library (which does not see ImGui) can read it.
 */
#pragma once

#include "voidmaiz/http.hpp"
#include "voidmaiz/safearea.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace maiz {

struct TextInputPlatform; // textinput.hpp

enum class Platform { Desktop, Android, iOS };
const char* platform_name(Platform p); // "desktop", "android", "ios"
Platform build_platform();             // the one this binary was compiled for

/* What happened to the application, from outside it. */
enum class AppEvent {
    /* Back in front of the person. */
    Foreground,
    /* No longer in front of the person: SAVE NOW. On iOS and Android this may be
     * the last code that runs; the system ends a background process without
     * another word, and a phone never "closes" an app the way a desktop does.
     * Networking will be suspended on iOS within seconds (a socket held open
     * across this is a socket that silently dies, which the LAN sessions'
     * keepalive and reconnect already survive). */
    Background,
    /* The system is short of memory: drop what can be rebuilt (caches, tiles). */
    LowMemory,
    /* The drawing surface went away (Android destroys it on every background;
     * a returning photo picker does it too) and every texture with it. Reload
     * pictures on SurfaceRestored. Metal on iOS keeps its textures, so this is
     * not sent there: a host must not assume it pairs with Background. */
    SurfaceLost,
    SurfaceRestored,
    /* The process is ending, if the platform says so (desktop close, Android
     * destroy). iOS usually does NOT: it suspends and later kills. Never put
     * saving here and only here. */
    Terminating,
};
const char* app_event_name(AppEvent e);

/* What `share` hands the system's share sheet. Any one field is enough. */
struct ShareItem {
    std::string text;
    std::string url;
    std::string file; // a local path; the sheet sends a copy
    std::string mime; // for `file`; "" = guess from the extension
    std::string subject;
};

enum class Haptic { Tap, Select, Success, Warning, Error };

/* A picture uploaded to the GPU. `id` is what ImGui::Image takes (an
 * ImTextureID: a GL texture name or a retained MTLTexture). */
struct Texture {
    std::uint64_t id = 0;
    int w = 0, h = 0;
    explicit operator bool() const { return id != 0; }
};

class Device {
  public:
    virtual ~Device() = default;

    // ── facts ──
    virtual Platform platform() const = 0;
    /* A desktop playing a phone (`--device iphone-16`): platform() is the
     * phone's, every rule follows it, and the services are stand-ins. */
    virtual bool preview() const { return false; }
    virtual std::string model() const { return {}; } // "iPhone 16 (preview)", "Pixel 8"
    virtual float density() const = 0;               // framebuffer px per dp / point
    virtual SafeArea safe_area() const { return {}; }
    /* Private, persistent, included in the device's backup. An app's state
     * document lives here (never beside the binary: a phone has no "beside"). */
    virtual std::filesystem::path data_dir() const = 0;
    /* May be emptied by the system when space is short (map tiles go here). */
    virtual std::filesystem::path cache_dir() const { return data_dir() / "cache"; }
    /* A file shipped INSIDE the app (fonts, a starter document): an APK's
     * assets, an iOS bundle's resources, a desktop's folder beside the binary. */
    virtual bool read_asset(const std::string& name, std::string& bytes) const;

    // ── conventions (read these rather than platform()) ──
    virtual bool can_quit() const { return platform() != Platform::iOS; }
    virtual bool self_updates() const { return platform() != Platform::iOS; }
    /* iOS: the edge swipe goes Back and a back chevron is mandatory. */
    virtual bool edge_back() const { return platform() == Platform::iOS; }

    // ── services: null / false = not on this device ──
    virtual TextInputPlatform* text_input() { return nullptr; }
    virtual Http http(const std::string& user_agent) { (void)user_agent; return {}; }
    /* https:, tel:, mailto:, sms:, and a maps URL. From a person's tap. */
    virtual bool open_url(const std::string& url) { (void)url; return false; }
    virtual bool share(const ShareItem& item) { (void)item; return false; }
    virtual void haptic(Haptic h) { (void)h; }
    /* The system's picker / save dialog (documents.hpp's request-and-result
     * idea, behind the Device). The answer arrives later, through
     * App::document, never on this line. */
    virtual bool pick_document(int request, const std::string& mime) { (void)request; (void)mime; return false; }
    virtual bool save_document(int request, const std::string& src_path, const std::string& name,
                               const std::string& mime) {
        (void)request; (void)src_path; (void)name; (void)mime;
        return false;
    }
    /* RGBA8, row-major, `w * h * 4` bytes. Call on the frame thread. */
    virtual Texture create_texture(const unsigned char* rgba, int w, int h) {
        (void)rgba; (void)w; (void)h;
        return {};
    }
    virtual void destroy_texture(Texture& t) { t = {}; }
    /* Leave. Does nothing where can_quit() is false. */
    virtual void quit() {}
};

/* The device this process is running on, installed by the shell before the
 * app starts; null in a test or a headless binary that installed none. For code
 * deep in a view that wants a haptic or a URL without being handed the Device. */
void install_device(Device* d);
Device* device();

/* ── devices to preview on a desktop ─────────────────────────────────────────
 * Nominal numbers (points / dp, portrait) from the manufacturers' published
 * screen sizes and the safe areas UIKit and Android report. A preview is for
 * LAYOUT: does the bar clear the home indicator, does the title clear the
 * Dynamic Island, does a 375-point screen fit. It is not an emulator. */
enum class Cutout { None, Notch, Island, Punch, HomeButton };

struct DeviceProfile {
    const char* name;  // "iphone-16"
    const char* label; // "iPhone 16"
    Platform platform;
    float w, h;        // portrait, dp / pt
    float scale;       // the real device's px per dp (3 for an iPhone 16)
    SafeArea safe;     // portrait, dp / pt
    SafeArea safe_landscape;
    float corner;      // screen corner radius, dp / pt
    Cutout cutout;
};

const std::vector<DeviceProfile>& device_profiles();
const DeviceProfile* find_device_profile(const std::string& name);

} // namespace maiz
