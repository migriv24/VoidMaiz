/* src/input/textinput_android.cpp — the keyboard holiday's Android crossing.
 *
 * The other half is android/java/org/voidmaiz/MaizActivity.java: a
 * NativeActivity subclass with one invisible View that is a real text editor to
 * the input method (it returns an InputConnection). The system keyboard edits
 * that View's buffer; every change is sent here as the WHOLE editing state
 * (text, selection, composing region, UTF-8 byte offsets), queued, and drained
 * by text_input_frame on the render thread.
 *
 * Java calls in on the UI thread; we call out from the render thread. The queue
 * and the mutex are the whole of the threading. Everything the keyboard means
 * is decided in textinput.cpp (plan_edit), which has a test; this file only
 * moves bytes, which is what a crossing should be.
 *
 * Reached through `activity->clazz` (the activity OBJECT) rather than FindClass:
 * FindClass on a native thread sees only the system class loader and cannot find
 * an application's classes, which is the classic way this kind of shim fails. */
#ifdef __ANDROID__

#include "voidmaiz/textinput.hpp"

#include <android/log.h>
#include <android/native_activity.h>
#include <jni.h>

#include <mutex>

namespace maiz {

namespace {

struct Env {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;
    explicit Env(ANativeActivity* a) : vm(a ? a->vm : nullptr) {
        if (!vm) return;
        if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        }
    }
    ~Env() {
        if (attached) vm->DetachCurrentThread();
    }
};

// what the keyboard did, waiting for the render thread
std::mutex g_mu;
std::vector<TextInputEvent> g_queue;
int g_covered = 0;

std::string utf8_of(JNIEnv* env, jbyteArray a) {
    if (!a) return {};
    const jsize n = env->GetArrayLength(a);
    std::string s((size_t)n, '\0');
    if (n) env->GetByteArrayRegion(a, 0, n, (jbyte*)s.data());
    return s;
}

jbyteArray bytes_of(JNIEnv* env, const std::string& s) {
    jbyteArray a = env->NewByteArray((jsize)s.size());
    if (a && !s.empty()) env->SetByteArrayRegion(a, 0, (jsize)s.size(), (const jbyte*)s.data());
    return a;
}

// ── Java → native (MaizActivity's `static native` methods) ──────────────────
void JNICALL native_state(JNIEnv* env, jclass, jbyteArray text, jint ss, jint se, jint cs, jint ce) {
    TextInputEvent e;
    e.type = TextInputEvent::Type::State;
    e.state.text = utf8_of(env, text);
    e.state.sel_start = ss;
    e.state.sel_end = se;
    e.state.compose_start = cs;
    e.state.compose_end = ce;
    std::lock_guard<std::mutex> lk(g_mu);
    g_queue.push_back(std::move(e));
}

/* `key` is TextKey's ordinal (MaizActivity.KEY_*: 0 Left, 1 Right, 2 Backspace,
 * 3 Delete, 4 Enter, 5 Tab). */
void JNICALL native_key(JNIEnv*, jclass, jint key) {
    if (key < 0 || key > 5) return;
    TextInputEvent e;
    e.type = TextInputEvent::Type::Key;
    e.key = (TextKey)key;
    std::lock_guard<std::mutex> lk(g_mu);
    g_queue.push_back(e);
}

/* The action key (Done, Go, Search…): it reaches the field as Enter, which is
 * what every committing field listens for. */
void JNICALL native_action(JNIEnv*, jclass, jint) {
    TextInputEvent e;
    e.type = TextInputEvent::Type::Action;
    e.key = TextKey::Enter;
    std::lock_guard<std::mutex> lk(g_mu);
    g_queue.push_back(e);
}

void JNICALL native_covered(JNIEnv*, jclass, jint px) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_covered = px;
}

class AndroidTextInput final : public TextInputPlatform {
public:
    AndroidTextInput(ANativeActivity* a, jmethodID show, jmethodID update, jmethodID hide)
        : a_(a), show_(show), update_(update), hide_(hide) {}

    void show(InputKind kind, InputAction action, const EditingState& st) override {
        Env e(a_);
        if (!e.env) return;
        jbyteArray t = bytes_of(e.env, st.text);
        e.env->CallVoidMethod(a_->clazz, show_, (jint)android_input_type(kind),
                              (jint)android_ime_options(kind, action), t, (jint)st.sel_start,
                              (jint)st.sel_end);
        e.env->DeleteLocalRef(t);
        clear(e.env);
    }
    void update(const EditingState& st) override {
        Env e(a_);
        if (!e.env) return;
        jbyteArray t = bytes_of(e.env, st.text);
        e.env->CallVoidMethod(a_->clazz, update_, t, (jint)st.sel_start, (jint)st.sel_end);
        e.env->DeleteLocalRef(t);
        clear(e.env);
    }
    void hide() override {
        Env e(a_);
        if (!e.env) return;
        e.env->CallVoidMethod(a_->clazz, hide_);
        clear(e.env);
    }
    void poll(std::vector<TextInputEvent>& out) override {
        std::lock_guard<std::mutex> lk(g_mu);
        for (auto& ev : g_queue) out.push_back(std::move(ev));
        g_queue.clear();
    }
    float covered_px() const override {
        std::lock_guard<std::mutex> lk(g_mu);
        return (float)g_covered;
    }

private:
    static void clear(JNIEnv* env) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }
    ANativeActivity* a_;
    jmethodID show_, update_, hide_;
};

} // namespace

std::unique_ptr<TextInputPlatform> android_text_input(ANativeActivity* activity) {
    Env e(activity);
    if (!e.env || !activity->clazz) return nullptr;
    JNIEnv* env = e.env;
    jclass cls = env->GetObjectClass(activity->clazz);
    jmethodID show = env->GetMethodID(cls, "maizShowKeyboard", "(II[BII)V");
    jmethodID update = env->GetMethodID(cls, "maizUpdateKeyboard", "([BII)V");
    jmethodID hide = env->GetMethodID(cls, "maizHideKeyboard", "()V");
    if (!show || !update || !hide) {
        // a plain NativeActivity: no shim, so no system keyboard. Say so once.
        env->ExceptionClear();
        __android_log_print(ANDROID_LOG_WARN, "voidmaiz",
                            "text input: the activity is not org.voidmaiz.MaizActivity; "
                            "the system keyboard is unavailable (see okf/concepts/text-input.md)");
        return nullptr;
    }
    static const JNINativeMethod natives[] = {
        {"nativeState", "([BIIII)V", (void*)native_state},
        {"nativeKey", "(I)V", (void*)native_key},
        {"nativeAction", "(I)V", (void*)native_action},
        {"nativeCovered", "(I)V", (void*)native_covered},
    };
    // the natives are declared on MaizActivity; a host may subclass it, so
    // register on the first class up the chain that accepts them
    bool registered = false;
    for (jclass c = cls; c && !registered; c = env->GetSuperclass(c)) {
        if (env->RegisterNatives(c, natives, 4) == JNI_OK) registered = true;
        else env->ExceptionClear();
    }
    if (!registered) return nullptr;
    return std::make_unique<AndroidTextInput>(activity, show, update, hide);
}

} // namespace maiz

#endif // __ANDROID__
