/* src/input/radio.cpp — the radio holiday (voidmaiz/radio.hpp): the
 * process-wide platform, the names, and the Android crossing. The other half
 * of the crossing is MaizActivity.java (maizRadio*), which owns Bluetooth LE and
 * Wi-Fi Direct on Android's threads and queues what happened for poll(). Like
 * documents.cpp and location.cpp it reaches the activity OBJECT, and a method
 * an older APK's activity lacks reads as Unavailable, never as a crash.
 *
 * The desktop stand-in (loopback_radio) needs sockets, so it lives in the LAN
 * target: src/lan/radio_loopback.cpp. */
#include "voidmaiz/radio.hpp"

namespace maiz {

namespace {
std::unique_ptr<RadioPlatform>& installed() {
    static std::unique_ptr<RadioPlatform> p;
    return p;
}
} // namespace

void install_radio(std::unique_ptr<RadioPlatform> platform) { installed() = std::move(platform); }
RadioPlatform* radio() { return installed().get(); }

const char* radio_access_name(RadioAccess a) {
    switch (a) {
    case RadioAccess::Unasked: return "unasked";
    case RadioAccess::Asking: return "asking";
    case RadioAccess::Ready: return "ready";
    case RadioAccess::Off: return "off";
    case RadioAccess::Denied: return "denied";
    case RadioAccess::Unavailable: return "unavailable";
    }
    return "unavailable";
}

} // namespace maiz

#ifdef __ANDROID__

#include <android/native_activity.h>
#include <jni.h>

#include <cstdlib>

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
    bool ok(ANativeActivity* a) const { return env && a && a->clazz; }
    bool threw() {
        if (!env->ExceptionCheck()) return false;
        env->ExceptionClear();
        return true;
    }
};

jmethodID method(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    jclass cls = env->GetObjectClass(obj);
    jmethodID m = cls ? env->GetMethodID(cls, name, sig) : nullptr;
    if (!m) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    return m;
}

std::string utf8(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return out;
}

class AndroidRadio : public RadioPlatform {
  public:
    explicit AndroidRadio(ANativeActivity* a) : a_(a) {}

    RadioAccess access(RadioKind kind) override {
        Env e(a_);
        if (!e.ok(a_)) return RadioAccess::Unavailable;
        jmethodID m = method(e.env, a_->clazz, "maizRadioAccess", "(I)I");
        if (!m) return RadioAccess::Unavailable;
        const jint v = e.env->CallIntMethod(a_->clazz, m, (jint)kind);
        if (e.threw()) return RadioAccess::Unavailable;
        return v >= 0 && v <= 5 ? (RadioAccess)v : RadioAccess::Unavailable;
    }
    bool request(RadioKind kind) override { return call_bool("maizRadioRequest", "(I)Z", kind); }
    bool start(RadioKind kind, const std::string& tag) override {
        Env e(a_);
        if (!e.ok(a_)) return false;
        jmethodID m = method(e.env, a_->clazz, "maizRadioStart", "(ILjava/lang/String;)Z");
        if (!m) return false;
        jstring t = e.env->NewStringUTF(tag.c_str());
        const jboolean r = e.env->CallBooleanMethod(a_->clazz, m, (jint)kind, t);
        e.env->DeleteLocalRef(t);
        const bool ok = !e.threw() && r;
        if (ok) running_[(int)kind] = true;
        return ok;
    }
    void stop(RadioKind kind) override {
        running_[(int)kind] = false;
        Env e(a_);
        if (!e.ok(a_)) return;
        if (jmethodID m = method(e.env, a_->clazz, "maizRadioStop", "(I)V")) {
            e.env->CallVoidMethod(a_->clazz, m, (jint)kind);
            e.threw();
        }
    }
    bool running(RadioKind kind) const override { return running_[(int)kind]; }
    bool connect(RadioKind kind, const std::string& peer) override {
        return call_peer("maizRadioConnect", kind, peer);
    }
    void disconnect(RadioKind kind, const std::string& peer) override {
        call_peer("maizRadioDisconnect", kind, peer);
    }
    bool send(const std::string& peer, const std::string& bytes) override {
        Env e(a_);
        if (!e.ok(a_)) return false;
        jmethodID m = method(e.env, a_->clazz, "maizRadioSend", "(Ljava/lang/String;[B)Z");
        if (!m) return false;
        jstring p = e.env->NewStringUTF(peer.c_str());
        jbyteArray b = e.env->NewByteArray((jsize)bytes.size());
        e.env->SetByteArrayRegion(b, 0, (jsize)bytes.size(), (const jbyte*)bytes.data());
        const jboolean r = e.env->CallBooleanMethod(a_->clazz, m, p, b);
        e.env->DeleteLocalRef(p);
        e.env->DeleteLocalRef(b);
        return !e.threw() && r;
    }
    std::size_t backlog(const std::string& peer) const override {
        Env e(a_);
        if (!e.ok(a_)) return 0;
        jmethodID m = method(e.env, a_->clazz, "maizRadioBacklog", "(Ljava/lang/String;)I");
        if (!m) return 0;
        jstring p = e.env->NewStringUTF(peer.c_str());
        const jint n = e.env->CallIntMethod(a_->clazz, m, p);
        e.env->DeleteLocalRef(p);
        return e.threw() || n < 0 ? 0 : (std::size_t)n;
    }
    void poll(std::vector<RadioEvent>& out) override {
        Env e(a_);
        if (!e.ok(a_)) return;
        jmethodID take = method(e.env, a_->clazz, "maizRadioTake", "()[Ljava/lang/String;");
        jmethodID bytes = method(e.env, a_->clazz, "maizRadioTakeBytes", "()[B");
        if (!take || !bytes) return;
        for (int budget = 0; budget < 512; ++budget) {
            jobjectArray arr = (jobjectArray)e.env->CallObjectMethod(a_->clazz, take);
            if (e.threw() || !arr) break;
            std::string f[7];
            const jsize n = e.env->GetArrayLength(arr);
            for (jsize i = 0; i < n && i < 7; ++i) {
                jstring s = (jstring)e.env->GetObjectArrayElement(arr, i);
                f[i] = utf8(e.env, s);
                if (s) e.env->DeleteLocalRef(s);
            }
            e.env->DeleteLocalRef(arr);
            RadioEvent ev;
            ev.type = (RadioEvent::Type)std::atoi(f[0].c_str());
            ev.kind = (RadioKind)std::atoi(f[1].c_str());
            ev.peer = f[2];
            ev.name = f[3];
            ev.tag = f[4];
            ev.detail = f[5];
            ev.rssi = std::atoi(f[6].c_str());
            if (ev.type == RadioEvent::Type::data) {
                jbyteArray b = (jbyteArray)e.env->CallObjectMethod(a_->clazz, bytes);
                if (!e.threw() && b) {
                    const jsize len = e.env->GetArrayLength(b);
                    ev.bytes.resize((std::size_t)len);
                    e.env->GetByteArrayRegion(b, 0, len, (jbyte*)ev.bytes.data());
                    e.env->DeleteLocalRef(b);
                }
            }
            out.push_back(std::move(ev));
        }
    }

  private:
    bool call_bool(const char* name, const char* sig, RadioKind kind) {
        Env e(a_);
        if (!e.ok(a_)) return false;
        jmethodID m = method(e.env, a_->clazz, name, sig);
        if (!m) return false;
        const jboolean r = e.env->CallBooleanMethod(a_->clazz, m, (jint)kind);
        return !e.threw() && r;
    }
    bool call_peer(const char* name, RadioKind kind, const std::string& peer) {
        Env e(a_);
        if (!e.ok(a_)) return false;
        jmethodID m = method(e.env, a_->clazz, name, "(ILjava/lang/String;)Z");
        if (!m) return false;
        jstring p = e.env->NewStringUTF(peer.c_str());
        const jboolean r = e.env->CallBooleanMethod(a_->clazz, m, (jint)kind, p);
        e.env->DeleteLocalRef(p);
        return !e.threw() && r;
    }
    ANativeActivity* a_;
    bool running_[2] = {false, false};
};

} // namespace

std::unique_ptr<RadioPlatform> android_radio(ANativeActivity* activity) {
    if (!activity) return nullptr;
    return std::make_unique<AndroidRadio>(activity);
}

} // namespace maiz

#else

namespace maiz {
std::unique_ptr<RadioPlatform> android_radio(ANativeActivity*) { return nullptr; }
} // namespace maiz

#endif
