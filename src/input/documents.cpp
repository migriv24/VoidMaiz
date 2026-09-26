/* src/input/documents.cpp — the document holiday's Android crossing
 * (voidmaiz/documents.hpp). The other half is MaizActivity.java: it starts the
 * system picker or save dialog, copies the bytes on a background thread, and
 * queues a result that android_take_document drains. This file only asks and
 * collects; like textinput_android.cpp, it reaches the activity OBJECT rather
 * than FindClass, which cannot see an application's classes from a native
 * thread. Off Android every call returns false and a host uses its own dialog. */
#include "voidmaiz/documents.hpp"

#ifdef __ANDROID__

#include <android/native_activity.h>
#include <jni.h>

#include <cstdlib> // atoi

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

/* The method, or null (an APK whose activity predates it: say no, never crash). */
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

bool call_bool(ANativeActivity* a, const char* name, const char* sig, int request,
               const std::string& s1, const std::string& s2, const std::string& s3, int nstr) {
    if (!a || !a->clazz) return false;
    Env e(a);
    if (!e.env) return false;
    JNIEnv* env = e.env;
    jmethodID m = method(env, a->clazz, name, sig);
    if (!m) return false;
    jstring j1 = env->NewStringUTF(s1.c_str());
    jstring j2 = env->NewStringUTF(s2.c_str());
    jstring j3 = nstr > 2 ? env->NewStringUTF(s3.c_str()) : nullptr;
    const jboolean r = nstr > 2 ? env->CallBooleanMethod(a->clazz, m, (jint)request, j1, j2, j3)
                                : env->CallBooleanMethod(a->clazz, m, (jint)request, j1, j2);
    const bool threw = env->ExceptionCheck();
    if (threw) env->ExceptionClear();
    env->DeleteLocalRef(j1);
    env->DeleteLocalRef(j2);
    if (j3) env->DeleteLocalRef(j3);
    return !threw && r;
}

} // namespace

bool android_pick_document(ANativeActivity* a, int request, const std::string& mime,
                           const std::string& dest_dir) {
    return call_bool(a, "maizPickDocument", "(ILjava/lang/String;Ljava/lang/String;)Z", request, mime,
                     dest_dir, {}, 2);
}

bool android_save_document(ANativeActivity* a, int request, const std::string& src_path,
                           const std::string& name, const std::string& mime) {
    return call_bool(a, "maizSaveDocument",
                     "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z", request, src_path,
                     name, mime, 3);
}

bool android_take_document(ANativeActivity* a, DocumentResult& out) {
    if (!a || !a->clazz) return false;
    Env e(a);
    if (!e.env) return false;
    JNIEnv* env = e.env;
    jmethodID m = method(env, a->clazz, "maizTakeDocument", "()[Ljava/lang/String;");
    if (!m) return false;
    jobjectArray arr = (jobjectArray)env->CallObjectMethod(a->clazz, m);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    if (!arr) return false;
    bool got = false;
    if (env->GetArrayLength(arr) >= 4) {
        std::string f[4];
        for (int i = 0; i < 4; ++i) {
            jstring s = (jstring)env->GetObjectArrayElement(arr, i);
            f[i] = utf8(env, s);
            if (s) env->DeleteLocalRef(s);
        }
        out = DocumentResult{};
        out.request = std::atoi(f[0].c_str());
        out.status = f[1];
        out.path = f[2];
        out.name = f[3];
        got = true;
    }
    env->DeleteLocalRef(arr);
    return got;
}

void android_opened_document(ANativeActivity* a, const std::string& dest_dir) {
    if (!a || !a->clazz) return;
    Env e(a);
    if (!e.env) return;
    JNIEnv* env = e.env;
    jmethodID m = method(env, a->clazz, "maizTakeOpenedDocument", "(Ljava/lang/String;)V");
    if (!m) return;
    jstring d = env->NewStringUTF(dest_dir.c_str());
    env->CallVoidMethod(a->clazz, m, d);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(d);
}

} // namespace maiz

#else // not Android: a host uses its own dialog

namespace maiz {
bool android_pick_document(ANativeActivity*, int, const std::string&, const std::string&) { return false; }
bool android_save_document(ANativeActivity*, int, const std::string&, const std::string&,
                           const std::string&) {
    return false;
}
bool android_take_document(ANativeActivity*, DocumentResult&) { return false; }
void android_opened_document(ANativeActivity*, const std::string&) {}
} // namespace maiz

#endif
