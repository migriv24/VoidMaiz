/* shell_desktop.cpp — a portable App (voidmaiz/app.hpp) in a desktop window,
 * and the same App previewed as a phone.
 *
 *     my_app                          a desktop window
 *     my_app --device iphone-16       that phone: its screen in points, its safe
 *                                     area, its sensor housing and home
 *                                     indicator drawn over the app, its RULES
 *                                     (no quit, edge-swipe back), and the mouse
 *                                     as a finger (hold Alt for a second one)
 *     my_app --device iphone-16 --landscape
 *     my_app --list-devices
 *
 * For an automated look (an agent's, CI's, a harness's):
 *     --screenshot out.png [--frames N]   draw N frames (default 30), save, exit
 *     --input "10:down 20 400;12:move 120 400;14:up 220 400"
 *                                         touches at frame numbers, in dp; also
 *                                         "N:back" (Back) and "N:background" /
 *                                         "N:foreground" (the lifecycle)
 *     --data DIR                          where data_dir() points
 *
 * F9 in a preview sends the app to the background and back, which is how to
 * check that it saves when a phone would take it away.
 *
 * WHAT A PREVIEW IS NOT: an emulator. The layout, the safe area and the rules
 * are the phone's; the keyboard is this one, the GPU is GL, and nothing here
 * proves the iOS shell runs. CI's iOS Simulator job is what does that. */
#include "frame.hpp"

#include "voidmaiz/glhost.hpp"
#include "voidmaiz/location.hpp"
#include "voidmaiz/mobile.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace fs = std::filesystem;

// cmake/VoidMaizApp.cmake writes these per app (maiz_add_app's NAME and BUNDLE_ID)
extern "C" const char* const maiz_app_name;
extern "C" const char* const maiz_app_id;

namespace {

// ── a PNG, for --screenshot (stored deflate: no compressor to vendor) ─────────

unsigned crc32(const unsigned char* p, size_t n, unsigned c = 0xffffffffu) {
    static unsigned table[256];
    static bool made = false;
    if (!made) {
        for (unsigned i = 0; i < 256; ++i) {
            unsigned k = i;
            for (int j = 0; j < 8; ++j) k = k & 1 ? 0xedb88320u ^ (k >> 1) : k >> 1;
            table[i] = k;
        }
        made = true;
    }
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c;
}

bool write_png(const std::string& path, int w, int h, const std::vector<unsigned char>& rgba) {
    std::vector<unsigned char> raw; // filter byte + RGBA per row
    raw.reserve((size_t)h * (w * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + (size_t)y * w * 4, rgba.begin() + (size_t)(y + 1) * w * 4);
    }
    std::vector<unsigned char> z{0x78, 0x01};
    unsigned a = 1, b = 0;
    for (size_t off = 0; off < raw.size() || off == 0;) {
        const size_t n = std::min<size_t>(65535, raw.size() - off);
        const bool last = off + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xff);
        z.push_back(n >> 8);
        z.push_back(~n & 0xff);
        z.push_back((~n >> 8) & 0xff);
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
        for (size_t i = off; i < off + n; ++i) a = (a + raw[i]) % 65521, b = (b + a) % 65521;
        off += n;
        if (last) break;
    }
    const unsigned adler = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) z.push_back((adler >> s) & 0xff);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    auto be32 = [&](unsigned v) {
        const unsigned char c[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                                    (unsigned char)v};
        out.write((const char*)c, 4);
    };
    auto chunk = [&](const char* type, const std::vector<unsigned char>& data) {
        be32((unsigned)data.size());
        std::vector<unsigned char> td(type, type + 4);
        td.insert(td.end(), data.begin(), data.end());
        out.write((const char*)td.data(), (std::streamsize)td.size());
        be32(crc32(td.data(), td.size()) ^ 0xffffffffu);
    };
    out.write("\x89PNG\r\n\x1a\n", 8);
    std::vector<unsigned char> ihdr = {(unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8),
                                       (unsigned char)w, (unsigned char)(h >> 24), (unsigned char)(h >> 16),
                                       (unsigned char)(h >> 8), (unsigned char)h, 8, 6, 0, 0, 0};
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    return (bool)out;
}

// ── where a desktop keeps an app's files ──────────────────────────────────────

fs::path user_data_root() {
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA")) return a;
#elif defined(__APPLE__)
    if (const char* h = std::getenv("HOME")) return fs::path(h) / "Library" / "Application Support";
#else
    if (const char* x = std::getenv("XDG_DATA_HOME")) return x;
    if (const char* h = std::getenv("HOME")) return fs::path(h) / ".local" / "share";
#endif
    return fs::current_path();
}

bool safe_scheme(const std::string& url) {
    for (const char* s : {"https://", "http://", "tel:", "mailto:", "sms:", "geo:"})
        if (url.rfind(s, 0) == 0) return url.find_first_of("\"'`\r\n") == std::string::npos;
    return false;
}

// ── the desktop's Device ──────────────────────────────────────────────────────

struct DesktopDevice : maiz::Device {
    const maiz::DeviceProfile* profile = nullptr; // set: a preview of that phone
    bool landscape = false;
    float zoom = 1.0f;     // px per dp in the window
    fs::path data;
    float haptic_t = -1.0f; // when the last haptic fired, for the preview's flash
    maiz::Haptic haptic_kind = maiz::Haptic::Tap;

    maiz::Platform platform() const override {
        return profile ? profile->platform : maiz::Platform::Desktop;
    }
    bool preview() const override { return profile != nullptr; }
    std::string model() const override {
        return profile ? std::string(profile->label) + " (preview)" : std::string("desktop");
    }
    float density() const override { return zoom; }
    maiz::SafeArea safe_area() const override {
        if (!profile) return {};
        const maiz::SafeArea& s = landscape ? profile->safe_landscape : profile->safe;
        return {s.left * zoom, s.top * zoom, s.right * zoom, s.bottom * zoom};
    }
    fs::path data_dir() const override { return data; }
    maiz::Http http(const std::string&) override { return maiz::curl_http(); }
    bool open_url(const std::string& url) override {
        if (!safe_scheme(url)) return false;
        std::printf("[device] open %s\n", url.c_str());
#ifdef _WIN32
        return (INT_PTR)ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
#elif defined(__APPLE__)
        return std::system(("open '" + url + "'").c_str()) == 0;
#else
        return std::system(("xdg-open '" + url + "' >/dev/null 2>&1 &").c_str()) == 0;
#endif
    }
    bool share(const maiz::ShareItem& item) override {
        // a desktop has no share sheet; a preview says what a phone would share
        std::printf("[device] share text=\"%s\" url=\"%s\" file=\"%s\"\n", item.text.c_str(), item.url.c_str(),
                    item.file.c_str());
        return profile != nullptr;
    }
    void haptic(maiz::Haptic h) override {
        haptic_t = (float)ImGui::GetTime();
        haptic_kind = h;
    }
    maiz::Texture create_texture(const unsigned char* rgba, int w, int h) override {
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        return {(std::uint64_t)id, w, h};
    }
    void destroy_texture(maiz::Texture& t) override {
        GLuint id = (GLuint)t.id;
        if (id) glDeleteTextures(1, &id);
        t = {};
    }
    GLFWwindow* window = nullptr;
    void quit() override {
        if (can_quit() && window) glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
};

// ── the preview's drawing: what the phone's hardware covers ───────────────────

void draw_preview_chrome(const DesktopDevice& d, bool backgrounded) {
    if (!d.profile) return;
    const maiz::DeviceProfile& p = *d.profile;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float k = d.zoom;
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    // the screen's rounded corners: what lies outside them is not screen
    if (p.corner > 0) {
        const float r = p.corner * k;
        const ImVec2 corners[4] = {{0, 0}, {size.x, 0}, {size.x, size.y}, {0, size.y}};
        const ImVec2 centres[4] = {{r, r}, {size.x - r, r}, {size.x - r, size.y - r}, {r, size.y - r}};
        const float pi = 3.14159265f;
        for (int i = 0; i < 4; ++i) { // a fan of triangles from the corner to the arc
            const float a0 = pi + i * pi * 0.5f;
            const int n = 16;
            for (int j = 0; j < n; ++j) {
                const float t0 = a0 + pi * 0.5f * j / n, t1 = a0 + pi * 0.5f * (j + 1) / n;
                dl->AddTriangleFilled(corners[i],
                                      ImVec2(centres[i].x + r * std::cos(t0), centres[i].y + r * std::sin(t0)),
                                      ImVec2(centres[i].x + r * std::cos(t1), centres[i].y + r * std::sin(t1)),
                                      black);
            }
        }
    }
    const bool ios = p.platform == maiz::Platform::iOS;
    // the sensor housing
    if (!d.landscape) {
        const float cx = size.x * 0.5f;
        switch (p.cutout) {
        case maiz::Cutout::Island:
            dl->AddRectFilled(ImVec2(cx - 63 * k, 11 * k), ImVec2(cx + 63 * k, 48 * k), black, 18.5f * k);
            break;
        case maiz::Cutout::Notch:
            dl->AddRectFilled(ImVec2(cx - 81 * k, 0), ImVec2(cx + 81 * k, 32 * k), black, 20 * k,
                              ImDrawFlags_RoundCornersBottom);
            break;
        case maiz::Cutout::Punch: dl->AddCircleFilled(ImVec2(cx, 12 * k), 5.5f * k, black); break;
        default: break;
        }
    } else if (p.cutout == maiz::Cutout::Island) {
        const float cy = size.y * 0.5f;
        dl->AddRectFilled(ImVec2(11 * k, cy - 63 * k), ImVec2(48 * k, cy + 63 * k), black, 18.5f * k);
    } else if (p.cutout == maiz::Cutout::Notch) {
        const float cy = size.y * 0.5f;
        dl->AddRectFilled(ImVec2(0, cy - 81 * k), ImVec2(32 * k, cy + 81 * k), black, 20 * k,
                          ImDrawFlags_RoundCornersRight);
    }
    // the status bar's clock, where the system draws it
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text, 0.85f);
    if (!d.landscape || !ios) {
        const float top = (d.landscape ? p.safe_landscape.top : p.safe.top) * k;
        if (top > 0) {
            const float fs = std::max(10.0f, 14.0f * k);
            const float x = p.cutout == maiz::Cutout::Island || p.cutout == maiz::Cutout::Notch ? size.x * 0.17f
                                                                                                 : 16 * k;
            dl->AddText(nullptr, fs, ImVec2(x, top * 0.5f - fs * 0.5f), ink, "9:41");
        }
    }
    // the home indicator (iOS) or the gesture pill (Android): never tappable
    if (p.cutout != maiz::Cutout::HomeButton) {
        const float w = (ios ? 134 : 108) * k, h = (ios ? 5 : 4) * k;
        const float y = size.y - (ios ? 8 : 9) * k - h;
        dl->AddRectFilled(ImVec2(size.x * 0.5f - w * 0.5f, y), ImVec2(size.x * 0.5f + w * 0.5f, y + h),
                          ImGui::GetColorU32(ImGuiCol_Text, 0.6f), h * 0.5f);
    }
    // a haptic, made visible: a ring where the finger is, for a moment
    const float since = (float)ImGui::GetTime() - d.haptic_t;
    if (d.haptic_t >= 0 && since < 0.25f) {
        const ImVec2 m = ImGui::GetIO().MousePos.x > -1e30f ? ImGui::GetIO().MousePos : ImVec2(size.x * 0.5f, size.y * 0.5f);
        const ImU32 c = d.haptic_kind == maiz::Haptic::Error     ? IM_COL32(230, 80, 80, 200)
                        : d.haptic_kind == maiz::Haptic::Warning ? IM_COL32(230, 180, 60, 200)
                                                                 : IM_COL32(120, 170, 255, 200);
        dl->AddCircle(m, (18 + since * 80) * k, c, 32, 2.5f * k);
    }
    if (backgrounded) {
        dl->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 0, 0, 170));
        const char* msg = "in the background (F9 to return)";
        const ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(size.x * 0.5f - ts.x * 0.5f, size.y * 0.5f), IM_COL32_WHITE, msg);
    }
}

// ── the mouse as a finger ──────────────────────────────────────────────────────

struct Finger {
    bool down = false;
    bool second = false; // Alt was held when it went down: a mirrored second finger
    float w = 0, h = 0;
};
Finger g_finger;

void finger_button(GLFWwindow* win, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    double x, y;
    glfwGetCursorPos(win, &x, &y);
    int w, h;
    glfwGetWindowSize(win, &w, &h);
    g_finger.w = (float)w;
    g_finger.h = (float)h;
    maiz::TouchGate& g = maiz::default_touch_gate();
    const double t = glfwGetTime();
    if (action == GLFW_PRESS && !g_finger.down) {
        g_finger.down = true;
        g_finger.second = (mods & GLFW_MOD_ALT) != 0;
        maiz::touch_gate_down(g, 0, (float)x, (float)y, t);
        if (g_finger.second) maiz::touch_gate_down(g, 1, (float)(w - x), (float)(h - y), t);
    } else if (action == GLFW_RELEASE && g_finger.down) {
        g_finger.down = false;
        if (g_finger.second) maiz::touch_gate_up(g, 1, (float)(w - x), (float)(h - y), t);
        maiz::touch_gate_up(g, 0, (float)x, (float)y, t);
    }
}

void finger_move(GLFWwindow*, double x, double y) {
    if (!g_finger.down) return;
    maiz::TouchGate& g = maiz::default_touch_gate();
    const double t = glfwGetTime();
    maiz::touch_gate_move(g, 0, (float)x, (float)y, t);
    if (g_finger.second) maiz::touch_gate_move(g, 1, g_finger.w - (float)x, g_finger.h - (float)y, t);
}

// ── scripted input (--input) ───────────────────────────────────────────────────

struct Scripted {
    int frame;
    std::string verb;
    float x = 0, y = 0;
};

std::vector<Scripted> parse_script(const std::string& s) {
    std::vector<Scripted> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ';')) {
        const auto colon = item.find(':');
        if (colon == std::string::npos) continue;
        Scripted e;
        e.frame = std::atoi(item.substr(0, colon).c_str());
        std::stringstream rest(item.substr(colon + 1));
        rest >> e.verb >> e.x >> e.y;
        out.push_back(e);
    }
    return out;
}

[[noreturn]] void usage_devices() {
    std::printf("devices for --device:\n");
    for (const auto& p : maiz::device_profiles())
        std::printf("  %-18s %-22s %4.0fx%-4.0f %s\n", p.name, p.label, p.w, p.h, maiz::platform_name(p.platform));
    std::exit(0);
}

} // namespace

int main(int argc, char** argv) {
    DesktopDevice dev;
    std::string screenshot, script;
    int frames = 30;
    float zoom = 0.0f;
    std::string data_arg;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--device") {
            const std::string n = next();
            dev.profile = maiz::find_device_profile(n);
            if (!dev.profile) {
                std::fprintf(stderr, "no device \"%s\"\n", n.c_str());
                usage_devices();
            }
        } else if (a == "--landscape") dev.landscape = true;
        else if (a == "--zoom") zoom = (float)std::atof(next().c_str());
        else if (a == "--screenshot") screenshot = next();
        else if (a == "--frames") frames = std::max(1, std::atoi(next().c_str()));
        else if (a == "--input") script = next();
        else if (a == "--data") data_arg = next();
        else if (a == "--list-devices") usage_devices();
    }
    dev.data = data_arg.empty() ? user_data_root() / maiz_app_id : fs::path(data_arg);
    if (dev.profile && data_arg.empty()) dev.data /= "preview"; // a preview never touches the real data
    std::error_code ec;
    fs::create_directories(dev.data, ec);

    glfwSetErrorCallback([](int code, const char* desc) { std::fprintf(stderr, "glfw error %d: %s\n", code, desc); });
    if (!glfwInit()) return 1;
    const char* glsl = maiz::gl_context_hints();

    int ww = 1100, wh = 720;
    if (dev.profile) {
        float w = dev.profile->w, h = dev.profile->h;
        if (dev.landscape) std::swap(w, h);
        if (zoom <= 0.0f) { // fit the screen: 90% of the monitor's height, and never above 2x
            zoom = 1.0f;
            if (const GLFWvidmode* m = glfwGetVideoMode(glfwGetPrimaryMonitor()))
                zoom = std::clamp(0.88f * (float)m->height / h, 0.5f, 2.0f);
        }
        ww = (int)std::lround(w * zoom);
        wh = (int)std::lround(h * zoom);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    } else if (zoom <= 0.0f) {
        zoom = 1.0f;
    }
    dev.zoom = zoom;
    if (!screenshot.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    std::string title = maiz_app_name;
    if (dev.profile) title += std::string(" - ") + dev.profile->label + (dev.landscape ? " (landscape)" : "");
    GLFWwindow* window = glfwCreateWindow(ww, wh, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    dev.window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // an app's layout is its own, not imgui.ini's
    if (dev.profile) {
        // a phone: touches go through the gate, never straight to ImGui; the
        // keyboard and the wheel still go to ImGui (this machine's keyboard
        // stands in for the phone's)
        ImGui_ImplGlfw_InitForOpenGL(window, false);
        glfwSetKeyCallback(window, ImGui_ImplGlfw_KeyCallback);
        glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
        glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
        glfwSetWindowFocusCallback(window, ImGui_ImplGlfw_WindowFocusCallback);
        glfwSetMouseButtonCallback(window, finger_button);
        glfwSetCursorPosCallback(window, finger_move);
        // "the pointer is over the window": stops the backend polling the real
        // cursor every frame, which would hover things a finger never hovers
        ImGui_ImplGlfw_CursorEnterCallback(window, 1);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    } else {
        ImGui_ImplGlfw_InitForOpenGL(window, true);
    }
    ImGui_ImplOpenGL3_Init(glsl);

    // a preview's stand-in location: a person can be asked, and say yes
    if (dev.profile) maiz::install_location(std::make_unique<maiz::FixedLocation>());

    maiz::shell::Runner runner(dev);
    runner.create();
    if (!runner.app()) {
        std::fprintf(stderr, "maiz::create_app() returned no app\n");
        return 1;
    }
    runner.start();

    const std::vector<Scripted> steps = parse_script(script);
    bool backgrounded = false, f9_was = false, iconified = false;
    int frame = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const bool icon_now = glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
        if (icon_now != iconified) {
            iconified = icon_now;
            runner.event(iconified ? maiz::AppEvent::Background : maiz::AppEvent::Foreground);
        }
        if (iconified) {
            glfwWaitEvents();
            continue;
        }
        const bool f9 = glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS;
        if (dev.profile && f9 && !f9_was) {
            backgrounded = !backgrounded;
            runner.event(backgrounded ? maiz::AppEvent::Background : maiz::AppEvent::Foreground);
        }
        f9_was = f9;

        // the script's touches for this frame, as a shell would deliver them
        for (const auto& s : steps) {
            if (s.frame != frame) continue;
            maiz::TouchGate& g = maiz::default_touch_gate();
            const float x = s.x * dev.zoom, y = s.y * dev.zoom;
            const double t = glfwGetTime();
            if (s.verb == "down") maiz::touch_gate_down(g, 0, x, y, t);
            else if (s.verb == "move") maiz::touch_gate_move(g, 0, x, y, t);
            else if (s.verb == "up") maiz::touch_gate_up(g, 0, x, y, t);
            else if (s.verb == "back") io.AddKeyEvent(ImGuiKey_AppBack, true);
            else if (s.verb == "background") runner.event(maiz::AppEvent::Background);
            else if (s.verb == "foreground") runner.event(maiz::AppEvent::Foreground);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        runner.frame(); // an unclaimed Back does nothing on a desktop, or on iOS
        draw_preview_chrome(dev, backgrounded);
        ImGui::Render();
        io.AddKeyEvent(ImGuiKey_AppBack, false);

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        glClearColor(bg.x, bg.y, bg.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!screenshot.empty() && frame + 1 >= frames) {
            std::vector<unsigned char> px((size_t)fbw * fbh * 4), flipped(px.size());
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            for (int y = 0; y < fbh; ++y)
                std::memcpy(&flipped[(size_t)y * fbw * 4], &px[(size_t)(fbh - 1 - y) * fbw * 4], (size_t)fbw * 4);
            for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
            const bool ok = write_png(screenshot, fbw, fbh, flipped);
            std::printf("%s %s (%dx%d)\n", ok ? "wrote" : "could not write", screenshot.c_str(), fbw, fbh);
            break;
        }
        glfwSwapBuffers(window);
        ++frame;
    }
    runner.event(maiz::AppEvent::Terminating);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
