/* src/input/location.cpp — the location holiday (voidmaiz/location.hpp).
 *
 * The process-wide platform, the names, and the Android crossing. The other
 * half of the crossing is MaizActivity.java (maizLocation*): it asks for the
 * permission, runs LocationManager on the UI thread, and keeps the newest fix
 * in fields this file reads. Like documents.cpp it reaches the activity OBJECT
 * rather than FindClass, which cannot see an application's classes from a
 * native thread, and a method that is missing (an APK whose activity predates
 * it) reads as Unavailable, never as a crash. */
#include "voidmaiz/location.hpp"

namespace maiz {

namespace {
std::unique_ptr<LocationPlatform>& installed() {
    static std::unique_ptr<LocationPlatform> p;
    return p;
}
} // namespace

void install_location(std::unique_ptr<LocationPlatform> platform) { installed() = std::move(platform); }
LocationPlatform* location() { return installed().get(); }

const char* access_name(LocationAccess a) {
    switch (a) {
    case LocationAccess::Unasked: return "unasked";
    case LocationAccess::Asking: return "asking";
    case LocationAccess::Precise: return "precise";
    case LocationAccess::Approximate: return "approximate";
    case LocationAccess::Denied: return "denied";
    case LocationAccess::Unavailable: return "unavailable";
    }
    return "unavailable";
}

} // namespace maiz

#ifdef __ANDROID__

#include <android/native_activity.h>
#include <jni.h>

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

jmethodID method(JNIEnv* env, jobject obj, const char* name, const char* sig) {
    jclass cls = env->GetObjectClass(obj);
    jmethodID m = cls ? env->GetMethodID(cls, name, sig) : nullptr;
    if (!m) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    return m;
}

const char* kProviders[] = {"gps", "network", "fused", "passive"};

class AndroidLocation : public LocationPlatform {
  public:
    explicit AndroidLocation(ANativeActivity* a) : a_(a) {}

    LocationAccess access() override {
        Env e(a_);
        if (!e.env || !a_->clazz) return LocationAccess::Unavailable;
        jmethodID m = method(e.env, a_->clazz, "maizLocationAccess", "()I");
        if (!m) return LocationAccess::Unavailable;
        const jint v = e.env->CallIntMethod(a_->clazz, m);
        if (e.env->ExceptionCheck()) {
            e.env->ExceptionClear();
            return LocationAccess::Unavailable;
        }
        return v >= 0 && v <= 5 ? (LocationAccess)v : LocationAccess::Unavailable;
    }

    bool request(bool precise) override {
        Env e(a_);
        if (!e.env || !a_->clazz) return false;
        jmethodID m = method(e.env, a_->clazz, "maizLocationRequest", "(Z)Z");
        if (!m) return false;
        const jboolean r = e.env->CallBooleanMethod(a_->clazz, m, (jboolean)precise);
        if (e.env->ExceptionCheck()) {
            e.env->ExceptionClear();
            return false;
        }
        on_ = r;
        return r;
    }

    void stop() override {
        on_ = false;
        Env e(a_);
        if (!e.env || !a_->clazz) return;
        if (jmethodID m = method(e.env, a_->clazz, "maizLocationStop", "()V")) {
            e.env->CallVoidMethod(a_->clazz, m);
            if (e.env->ExceptionCheck()) e.env->ExceptionClear();
        }
    }

    bool latest(LocationFix& out) override {
        Env e(a_);
        if (!e.env || !a_->clazz) return false;
        jmethodID m = method(e.env, a_->clazz, "maizLocationLatest", "()[D");
        if (!m) return false;
        jdoubleArray arr = (jdoubleArray)e.env->CallObjectMethod(a_->clazz, m);
        if (e.env->ExceptionCheck()) {
            e.env->ExceptionClear();
            return false;
        }
        if (!arr) return false;
        bool got = false;
        if (e.env->GetArrayLength(arr) >= 5) {
            jdouble v[5];
            e.env->GetDoubleArrayRegion(arr, 0, 5, v);
            out.lat = v[0];
            out.lon = v[1];
            out.accuracy_m = (float)v[2];
            out.age_s = v[3];
            const int p = (int)v[4];
            out.provider = p >= 0 && p < 4 ? kProviders[p] : "unknown";
            got = true;
        }
        e.env->DeleteLocalRef(arr);
        return got;
    }

    bool following() const override { return on_; }

  private:
    ANativeActivity* a_;
    bool on_ = false;
};

} // namespace

std::unique_ptr<LocationPlatform> android_location(ANativeActivity* activity) {
    if (!activity) return nullptr;
    return std::make_unique<AndroidLocation>(activity);
}

} // namespace maiz

#else

namespace maiz {
std::unique_ptr<LocationPlatform> android_location(ANativeActivity*) { return nullptr; }
} // namespace maiz

#endif
