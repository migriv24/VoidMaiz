/* device.cpp — the process's Device, and the preview profiles (voidmaiz/device.hpp). */
#include "voidmaiz/device.hpp"

struct ANativeActivity;
namespace maiz {
ANativeActivity* android_activity();
}

#include <fstream>
#include <iterator>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace maiz {

const char* platform_name(Platform p) {
    switch (p) {
    case Platform::Android: return "android";
    case Platform::iOS: return "ios";
    case Platform::Desktop: break;
    }
    return "desktop";
}

Platform build_platform() {
#if defined(__ANDROID__)
    return Platform::Android;
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return Platform::iOS;
#else
    return Platform::Desktop;
#endif
}

const char* app_event_name(AppEvent e) {
    switch (e) {
    case AppEvent::Foreground: return "foreground";
    case AppEvent::Background: return "background";
    case AppEvent::LowMemory: return "low-memory";
    case AppEvent::SurfaceLost: return "surface-lost";
    case AppEvent::SurfaceRestored: return "surface-restored";
    case AppEvent::Terminating: return "terminating";
    }
    return "?";
}

/* The desktop's reading: a file under `assets/` beside the working directory.
 * The phone shells override it (APK assets, the iOS bundle). */
bool Device::read_asset(const std::string& name, std::string& bytes) const {
    std::ifstream in(std::filesystem::path("assets") / name, std::ios::binary);
    if (!in) return false;
    bytes.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

namespace {
Device* g_device = nullptr;
}

void install_device(Device* d) { g_device = d; }

#if !defined(__ANDROID__)
ANativeActivity* android_activity() { return nullptr; } // the Android shell defines the real one
#endif
Device* device() { return g_device; }

const std::vector<DeviceProfile>& device_profiles() {
    /* Safe areas are UIKit's safeAreaInsets for each model (portrait: status
     * bar or sensor housing on top, home indicator below; landscape: the
     * housing's side on both sides, the indicator below), and Android's
     * typical status bar and gesture strip. */
    static const std::vector<DeviceProfile> all = {
        {"iphone-se", "iPhone SE (3rd gen)", Platform::iOS, 375, 667, 2, {0, 20, 0, 0}, {0, 0, 0, 0}, 0,
         Cutout::HomeButton},
        {"iphone-13-mini", "iPhone 13 mini", Platform::iOS, 375, 812, 3, {0, 50, 0, 34}, {50, 0, 50, 21}, 44,
         Cutout::Notch},
        {"iphone-16", "iPhone 16", Platform::iOS, 393, 852, 3, {0, 59, 0, 34}, {59, 0, 59, 21}, 55,
         Cutout::Island},
        {"iphone-16-pro-max", "iPhone 16 Pro Max", Platform::iOS, 440, 956, 3, {0, 62, 0, 34}, {62, 0, 62, 21},
         62, Cutout::Island},
        {"ipad-11", "iPad Air 11-inch", Platform::iOS, 820, 1180, 2, {0, 24, 0, 20}, {0, 24, 0, 20}, 18,
         Cutout::None},
        {"pixel-8", "Pixel 8", Platform::Android, 412, 915, 2.625f, {0, 24, 0, 24}, {0, 24, 0, 24}, 28,
         Cutout::Punch},
        {"android-small", "Small Android", Platform::Android, 360, 740, 3, {0, 24, 0, 24}, {0, 24, 0, 24}, 16,
         Cutout::Punch},
    };
    return all;
}

const DeviceProfile* find_device_profile(const std::string& name) {
    for (const auto& p : device_profiles())
        if (name == p.name) return &p;
    return nullptr;
}

} // namespace maiz
