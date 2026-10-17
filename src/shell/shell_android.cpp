/* shell_android.cpp — a portable App (voidmaiz/app.hpp) on Android: the
 * NativeActivity shell, owned by the library (2026-10-17).
 *
 * Generalised from Void Hormiga's android/src/main_android.cpp, which was
 * itself Interaction Combinators' shell grown up on real phones. Everything in
 * it that knew Hormiga is now a Device service or an App hook; everything that
 * did not is here, once:
 *   - EGL + GLES3, torn down and rebuilt with the window (Android destroys the
 *     surface on every trip to the background, and the photo picker is one);
 *   - fingers through the touch gate, every historical sample with its own time;
 *   - Back (key and gesture) as ImGuiKey_AppBack, and the platform default when
 *     no screen claimed it: to the background, as Android 12+ does;
 *   - the safe area, re-read twice a second (rotation, navigation mode);
 *   - documents picked, saved and opened-with, delivered between frames;
 *   - the platform keyboard and location, installed once (the radios are
 *     Void Hammer's since 2026-10-17: an app that wants them installs
 *     maiz::android_radio(maiz::android_activity()) itself);
 *   - Background on pause, which is where an app saves.
 *
 * The manifest names org.voidmaiz.MaizActivity (android/java), hasCode="true",
 * windowSoftInputMode="adjustNothing"; android/build_java.ps1 builds its dex.
 * Interaction Combinators' and Void Hormiga's build_apk.ps1 package the .so. */
#include "frame.hpp"

#include "voidmaiz/location.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/textinputview.hpp"

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "voidmaiz", __VA_ARGS__)

extern "C" const char* const maiz_app_name;
extern "C" const char* const maiz_app_version;

namespace {

/* One call into MaizActivity with string arguments. A missing method (a plain
 * NativeActivity, an older MaizActivity) is false, never a crash. */
struct Jni {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;
    explicit Jni(ANativeActivity* a) : vm(a ? a->vm : nullptr) {
        if (!vm) return;
        if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        }
    }
    ~Jni() {
        if (attached) vm->DetachCurrentThread();
    }
};

jmethodID find(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    jclass cls = env->GetObjectClass(obj);
    jmethodID m = cls ? env->GetMethodID(cls, name, sig) : nullptr;
    if (!m) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    return m;
}

bool call_strings(ANativeActivity* a, const char* name, const char* sig, std::initializer_list<std::string> args) {
    if (!a || !a->clazz) return false;
    Jni j(a);
    if (!j.env) return false;
    jmethodID m = find(j.env, a->clazz, name, sig);
    if (!m) return false;
    jvalue v[4] = {};
    std::vector<jstring> made;
    int i = 0;
    for (const auto& s : args) {
        made.push_back(j.env->NewStringUTF(s.c_str()));
        v[i++].l = made.back();
    }
    const bool ok = j.env->CallBooleanMethodA(a->clazz, m, v);
    for (jstring s : made) j.env->DeleteLocalRef(s);
    if (j.env->ExceptionCheck()) {
        j.env->ExceptionClear();
        return false;
    }
    return ok;
}

void call_void(ANativeActivity* a, const char* name, const char* sig, int arg, bool has_arg) {
    if (!a || !a->clazz) return;
    Jni j(a);
    if (!j.env) return;
    jmethodID m = find(j.env, a->clazz, name, sig);
    if (!m) return;
    if (has_arg) j.env->CallVoidMethod(a->clazz, m, (jint)arg);
    else j.env->CallVoidMethod(a->clazz, m);
    if (j.env->ExceptionCheck()) j.env->ExceptionClear();
}

bool safe_scheme(const std::string& url) {
    for (const char* s : {"https://", "http://", "tel:", "mailto:", "sms:", "geo:"})
        if (url.rfind(s, 0) == 0) return url.find_first_of("\"'`\r\n") == std::string::npos;
    return false;
}

struct AndroidDevice : maiz::Device {
    android_app* aapp = nullptr;
    float dens = 2.0f;
    maiz::SafeArea safe;
    std::unique_ptr<maiz::TextInputPlatform> keyboard;

    maiz::Platform platform() const override { return maiz::Platform::Android; }
    std::string model() const override { return "Android"; }
    float density() const override { return dens; }
    maiz::SafeArea safe_area() const override { return safe; }
    std::filesystem::path data_dir() const override {
        const char* p = aapp->activity->internalDataPath;
        return p ? std::filesystem::path(p) : std::filesystem::path("/data/local/tmp");
    }
    bool read_asset(const std::string& name, std::string& bytes) const override {
        AAsset* a = AAssetManager_open(aapp->activity->assetManager, name.c_str(), AASSET_MODE_BUFFER);
        if (!a) return false;
        const off_t n = AAsset_getLength(a);
        bytes.resize((size_t)n);
        const int got = AAsset_read(a, bytes.data(), (size_t)n);
        AAsset_close(a);
        return got == n;
    }
    maiz::TextInputPlatform* text_input() override { return keyboard.get(); }
    maiz::Http http(const std::string& ua) override {
        return maiz::android_http(aapp->activity,
                                  ua.empty() ? std::string(maiz_app_name) + "/" + maiz_app_version + " (Android)" : ua);
    }
    bool open_url(const std::string& url) override {
        return safe_scheme(url) && call_strings(aapp->activity, "maizOpenUrl", "(Ljava/lang/String;)Z", {url});
    }
    bool share(const maiz::ShareItem& item) override {
        if (!item.file.empty() && item.text.empty() && item.url.empty()) return false; // see MaizActivity.maizShare
        std::string text = item.text;
        if (!item.url.empty()) text += (text.empty() ? "" : "\n") + item.url;
        return call_strings(aapp->activity, "maizShare", "(Ljava/lang/String;Ljava/lang/String;)Z",
                            {text, item.subject});
    }
    void haptic(maiz::Haptic h) override { call_void(aapp->activity, "maizHaptic", "(I)V", (int)h, true); }
    bool pick_document(int request, const std::string& mime) override {
        return maiz::android_pick_document(aapp->activity, request, mime, (data_dir() / "incoming").string());
    }
    bool save_document(int request, const std::string& src, const std::string& name, const std::string& mime) override {
        return maiz::android_save_document(aapp->activity, request, src, name,
                                           mime.empty() ? "application/octet-stream" : mime);
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
    void quit() override { ANativeActivity_finish(aapp->activity); }
};

struct Shell {
    android_app* aapp = nullptr;
    AndroidDevice dev;
    std::unique_ptr<maiz::shell::Runner> runner;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool backends_ready = false;
    bool surface_was_lost = false;
    int safe_age = 1 << 20;
};

bool egl_init(Shell& s) {
    s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s.display == EGL_NO_DISPLAY || !eglInitialize(s.display, nullptr, nullptr)) return false;
    const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint num = 0;
    if (!eglChooseConfig(s.display, attribs, &config, 1, &num) || num < 1) return false;
    EGLint format;
    eglGetConfigAttrib(s.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(s.aapp->window, 0, 0, format);
    s.surface = eglCreateWindowSurface(s.display, config, s.aapp->window, nullptr);
    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.context = eglCreateContext(s.display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (s.surface == EGL_NO_SURFACE || s.context == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(s.display, s.surface, s.surface, s.context) == EGL_TRUE;
}

void egl_term(Shell& s) {
    if (s.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display, s.context);
        if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display, s.surface);
        eglTerminate(s.display);
    }
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
}

void backends_up(Shell& s) {
    if (s.backends_ready || !s.aapp->window) return;
    if (!egl_init(s)) {
        LOGE("EGL init failed");
        return;
    }
    ImGui_ImplAndroid_Init(s.aapp->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    if (!s.runner) { // once per process
        const float dpi = (float)AConfiguration_getDensity(s.aapp->config);
        s.dev.dens = std::clamp(dpi > 0 ? dpi / 160.0f : 2.0f, 1.0f, 5.0f);
        // Android has no /tmp, and std::filesystem::temp_directory_path falls
        // back to it; TMPDIR is what it reads first (Void Hormiga, 2026-09-25)
        std::error_code ec;
        const auto tmp = s.dev.data_dir() / "tmp";
        std::filesystem::create_directories(tmp, ec);
        setenv("TMPDIR", tmp.c_str(), 1);
        s.dev.keyboard = maiz::android_text_input(s.aapp->activity);
        maiz::install_location(maiz::android_location(s.aapp->activity));
        s.runner = std::make_unique<maiz::shell::Runner>(s.dev);
        s.runner->create();
        s.runner->start();
        // started by "Open with" on a file: it arrives like a pick, request -1
        maiz::android_opened_document(s.aapp->activity, (s.dev.data_dir() / "incoming").string());
    } else if (s.surface_was_lost) {
        s.runner->event(maiz::AppEvent::SurfaceRestored);
    }
    s.surface_was_lost = false;
    s.backends_ready = true;
}

void backends_down(Shell& s) {
    if (!s.backends_ready) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    egl_term(s);
    s.backends_ready = false; // the app and the ImGui context survive backgrounding
    if (s.runner) {
        s.surface_was_lost = true;
        s.runner->event(maiz::AppEvent::SurfaceLost); // every texture went with the context
    }
}

void on_cmd(android_app* a, int32_t cmd) {
    Shell& s = *(Shell*)a->userData;
    switch (cmd) {
    case APP_CMD_INIT_WINDOW: backends_up(s); break;
    case APP_CMD_TERM_WINDOW: backends_down(s); break;
    case APP_CMD_RESUME:
        if (s.runner) {
            s.runner->event(maiz::AppEvent::Foreground);
            maiz::android_opened_document(a->activity, (s.dev.data_dir() / "incoming").string());
        }
        break;
    case APP_CMD_PAUSE:
        if (s.runner) s.runner->event(maiz::AppEvent::Background); // save now: there may be no later
        break;
    case APP_CMD_LOW_MEMORY:
        if (s.runner) s.runner->event(maiz::AppEvent::LowMemory);
        break;
    default: break;
    }
}

/* Fingers through the touch gate; a mouse or a stylus straight to ImGui. */
bool gate_motion(AInputEvent* ev) {
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return false;
    if (AMotionEvent_getToolType(ev, 0) != AMOTION_EVENT_TOOL_TYPE_FINGER) return false;
    maiz::TouchGate& g = maiz::default_touch_gate();
    const int32_t action = AMotionEvent_getAction(ev);
    const int32_t kind = action & AMOTION_EVENT_ACTION_MASK;
    const size_t index =
        (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    const double t = (double)AMotionEvent_getEventTime(ev) * 1e-9;
    auto id = [&](size_t i) { return (int)AMotionEvent_getPointerId(ev, i); };
    switch (kind) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        maiz::touch_gate_down(g, id(index), AMotionEvent_getX(ev, index), AMotionEvent_getY(ev, index), t);
        break;
    case AMOTION_EVENT_ACTION_MOVE: {
        const size_t n = AMotionEvent_getPointerCount(ev);
        const size_t hist = AMotionEvent_getHistorySize(ev);
        for (size_t h = 0; h < hist; ++h)
            for (size_t i = 0; i < n; ++i)
                maiz::touch_gate_move(g, id(i), AMotionEvent_getHistoricalX(ev, i, h),
                                      AMotionEvent_getHistoricalY(ev, i, h),
                                      (double)AMotionEvent_getHistoricalEventTime(ev, h) * 1e-9);
        for (size_t i = 0; i < n; ++i)
            maiz::touch_gate_move(g, id(i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i), t);
        break;
    }
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
        maiz::touch_gate_up(g, id(index), AMotionEvent_getX(ev, index), AMotionEvent_getY(ev, index), t);
        break;
    case AMOTION_EVENT_ACTION_CANCEL: maiz::touch_gate_cancel(g); break;
    default: break;
    }
    return true;
}

int32_t on_input(android_app* a, AInputEvent* ev) {
    Shell& s = *(Shell*)a->userData;
    if (!s.backends_ready || !s.runner) return 0;
    if (maiz::android_system_key(ev)) return 1; // Back -> ImGuiKey_AppBack; the frame decides
    if (gate_motion(ev)) return 1;
    return ImGui_ImplAndroid_HandleInputEvent(ev);
}

void render_frame(Shell& s) {
    for (maiz::DocumentResult r; maiz::android_take_document(s.aapp->activity, r);) s.runner->document(r);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    if (++s.safe_age > 30) {
        s.dev.safe = maiz::android_safe_area(s.aapp->activity);
        s.safe_age = 0;
    }
    if (s.runner->frame()) call_void(s.aapp->activity, "maizMoveToBack", "()V", 0, false); // Back at the root
    ImGui::Render();
    EGLint w = 0, h = 0;
    eglQuerySurface(s.display, s.surface, EGL_WIDTH, &w);
    eglQuerySurface(s.display, s.surface, EGL_HEIGHT, &h);
    glViewport(0, 0, w, h);
    const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    glClearColor(bg.x, bg.y, bg.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(s.display, s.surface);
}

} // namespace

static ANativeActivity* g_activity = nullptr;
ANativeActivity* maiz::android_activity() { return g_activity; }

void android_main(android_app* a) {
    Shell s;
    s.aapp = a;
    s.dev.aapp = a;
    g_activity = a->activity;
    a->userData = &s;
    a->onAppCmd = on_cmd;
    a->onInputEvent = on_input;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // a phone's layout is its own, not imgui.ini's

    while (true) {
        int events;
        android_poll_source* source;
        while (ALooper_pollOnce(s.backends_ready ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(a, source);
            if (a->destroyRequested) {
                if (s.runner) s.runner->event(maiz::AppEvent::Terminating);
                backends_down(s);
                s.runner.reset();
                ImGui::DestroyContext();
                g_activity = nullptr;
                return;
            }
            if (s.backends_ready) break;
        }
        if (s.backends_ready) render_frame(s);
    }
}
