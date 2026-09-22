/* update.cpp — an application updating itself (voidmaiz/update.hpp). */
#include "voidmaiz/update.hpp"

#include "../reduce/sha256.hpp" // FIPS 180-4, already pinned by Void Core's vectors

#include "cJSON.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <future>
#include <sstream>

#if defined(__ANDROID__)
#include <android/native_activity.h>
#include <jni.h>
#endif

namespace maiz::update {

namespace {

struct Json {
    cJSON* p = nullptr;
    explicit Json(cJSON* x) : p(x) {}
    ~Json() { cJSON_Delete(p); }
    Json(const Json&) = delete;
    Json& operator=(const Json&) = delete;
};

std::string str_of(const cJSON* o, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && v->valuestring ? std::string(v->valuestring) : std::string();
}

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string now_iso8601() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

/* A name the feed gave us becomes a path on this disk: nothing that can climb
 * out of the download folder, and nothing a shell would read. */
bool safe_filename(const std::string& f) {
    if (f.empty() || f.size() > 200 || f == "." || f == "..") return false;
    for (char c : f)
        if (!(std::isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_' || c == '+'))
            return false;
    return f.find("..") == std::string::npos;
}

bool valid_sha256(const std::string& h) {
    if (h.size() != 64) return false;
    for (char c : h)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

/* Quote a path for the platform shell. Paths come from wherever a person put
 * the install; a `$` or a quote in one must not be read by the shell. */
std::string shell_quote(const std::string& s) {
#if defined(_WIN32)
    // cmd.exe: a double-quoted token; a literal " cannot occur in a Windows path
    return "\"" + s + "\"";
#else
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
#endif
}

std::string run_capture(const std::string& cmd) {
#if defined(_WIN32)
    // cmd.exe strips the first and last quote of a line that starts with one and
    // holds more (`"C:\...\tar.exe" -xf "a" -C "b"` loses both ends). One more
    // pair around the whole line is the documented way to keep it intact.
    FILE* f = _popen(("\"" + cmd + "\"").c_str(), "r");
#else
    FILE* f = popen(cmd.c_str(), "r");
#endif
    if (!f) return {};
    std::string out;
    char buf[512];
    while (std::fgets(buf, sizeof buf, f)) out += buf;
#if defined(_WIN32)
    _pclose(f);
#else
    pclose(f);
#endif
    return out;
}

} // namespace

/* The system's own tar. On Windows that is %SystemRoot%\System32\tar.exe
 * (bsdtar, shipped since 1803, which also reads .zip), named EXPLICITLY: a
 * machine with Git installed can have GNU tar earlier on PATH, and GNU tar reads
 * "C:\..." as host "C", path "\..." and fails (found by update_smoke on the
 * machine this was written on). */
std::string system_tar() {
#if defined(_WIN32)
    const char* root = std::getenv("SystemRoot");
    return shell_quote(std::string(root ? root : "C:\\Windows") + "\\System32\\tar.exe");
#else
    return "tar";
#endif
}

std::string platform_tag() {
#if defined(__ANDROID__)
#if defined(__aarch64__)
    return "android-arm64";
#else
    return "android-other";
#endif
#elif defined(_WIN32)
    return "windows-x64";
#elif defined(__APPLE__)
#if defined(__aarch64__)
    return "macos-arm64";
#else
    return "macos-x64";
#endif
#else
    return "linux-x64";
#endif
}

// ── versions and the feed ─────────────────────────────────────────────────────

int compare_versions(const std::string& a, const std::string& b) {
    std::size_t i = 0, j = 0;
    auto num = [](const std::string& s, std::size_t& k, bool& is_num) {
        long long v = 0;
        is_num = k < s.size() && std::isdigit((unsigned char)s[k]);
        while (k < s.size() && std::isdigit((unsigned char)s[k])) v = v * 10 + (s[k++] - '0');
        return v;
    };
    while (i < a.size() || j < b.size()) {
        bool na = false, nb = false;
        long long va = num(a, i, na), vb = num(b, j, nb);
        if (na || nb) {
            if (va != vb) return va < vb ? -1 : 1;
        }
        // a non-numeric tail ("-rc1") compares lexically, and ranks BELOW none
        std::string ta, tb;
        while (i < a.size() && a[i] != '.') ta += a[i++];
        while (j < b.size() && b[j] != '.') tb += b[j++];
        if (ta != tb) {
            if (ta.empty()) return 1; // "1.0" > "1.0-rc1"
            if (tb.empty()) return -1;
            return ta < tb ? -1 : 1;
        }
        if (i < a.size() && a[i] == '.') ++i;
        if (j < b.size() && b[j] == '.') ++j;
    }
    return 0;
}

Feed parse_feed(const std::string& text, const std::string& app, const std::string& platform) {
    Feed f;
    Json root(cJSON_ParseWithLength(text.data(), text.size()));
    if (!root.p || !cJSON_IsObject(root.p)) {
        f.error = "the update feed is not a JSON object";
        return f;
    }
    std::string kind = str_of(root.p, "feed");
    if (kind.rfind("void-updates/", 0) != 0) {
        f.error = "not a void-updates document (found \"" + kind + "\")";
        return f;
    }
    const cJSON* apps = cJSON_GetObjectItemCaseSensitive(root.p, "applications");
    const cJSON* mine = cJSON_GetObjectItemCaseSensitive(apps, app.c_str());
    if (!cJSON_IsObject(mine)) {
        f.error = "the update feed does not list \"" + app + "\"";
        return f;
    }
    f.display_name = str_of(mine, "display_name");
    f.latest = str_of(mine, "latest");
    const cJSON* rels = cJSON_GetObjectItemCaseSensitive(mine, "releases");
    const cJSON* r = nullptr;
    cJSON_ArrayForEach(r, rels) {
        if (!cJSON_IsObject(r)) continue;
        Release rel;
        rel.version = str_of(r, "version");
        rel.date = str_of(r, "date");
        rel.change = str_of(r, "change");
        rel.summary = str_of(r, "summary");
        const cJSON* a = nullptr;
        cJSON_ArrayForEach(a, cJSON_GetObjectItemCaseSensitive(r, "adds")) {
            if (cJSON_IsString(a)) rel.adds.emplace_back(a->valuestring);
        }
        const cJSON* b = nullptr;
        cJSON_ArrayForEach(b, cJSON_GetObjectItemCaseSensitive(r, "behavior_changes")) {
            if (cJSON_IsObject(b)) rel.behavior_changes.push_back({str_of(b, "what"), str_of(b, "who_is_affected")});
        }
        const cJSON* art = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(r, "artifacts"), platform.c_str());
        if (cJSON_IsObject(art)) {
            rel.file = str_of(art, "file");
            rel.url = str_of(art, "url");
            rel.sha256 = str_of(art, "sha256");
            rel.signature = str_of(art, "signature");
            if (const cJSON* bytes = cJSON_GetObjectItemCaseSensitive(art, "bytes"); cJSON_IsNumber(bytes))
                rel.bytes = (long long)bytes->valuedouble;
            // an artifact we cannot verify, fetch safely or store safely is no artifact
            rel.has_artifact = valid_sha256(rel.sha256) && safe_url(rel.url) && safe_filename(rel.file);
        }
        if (!rel.version.empty()) f.releases.push_back(std::move(rel));
    }
    if (f.latest.empty()) {
        f.error = "the update feed gives no latest version for \"" + app + "\"";
        return f;
    }
    f.ok = true;
    return f;
}

// ── preferences ───────────────────────────────────────────────────────────────

const char* ask_name(Ask a) {
    switch (a) {
    case Ask::Never: return "never";
    case Ask::Startup: return "startup";
    case Ask::Unasked: break;
    }
    return "unasked";
}

Ask ask_from_name(const std::string& s) {
    if (s == "never") return Ask::Never;
    if (s == "startup") return Ask::Startup;
    return Ask::Unasked;
}

static fs::path prefs_file(const fs::path& dir) { return dir / "updates.json"; }

Prefs load_prefs(const fs::path& dir) {
    Prefs p;
    std::string text = read_file(prefs_file(dir));
    Json root(cJSON_Parse(text.c_str()));
    if (!cJSON_IsObject(root.p)) return p;
    p.ask = ask_from_name(str_of(root.p, "ask"));
    p.feed_url = str_of(root.p, "feed_url");
    p.skip_version = str_of(root.p, "skip_version");
    p.last_checked = str_of(root.p, "last_checked");
    return p;
}

bool save_prefs(const fs::path& dir, const Prefs& p) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    Json root(cJSON_CreateObject());
    cJSON_AddStringToObject(root.p, "ask", ask_name(p.ask));
    if (!p.feed_url.empty()) cJSON_AddStringToObject(root.p, "feed_url", p.feed_url.c_str());
    if (!p.skip_version.empty()) cJSON_AddStringToObject(root.p, "skip_version", p.skip_version.c_str());
    if (!p.last_checked.empty()) cJSON_AddStringToObject(root.p, "last_checked", p.last_checked.c_str());
    char* text = cJSON_Print(root.p);
    fs::path tmp = prefs_file(dir);
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << (text ? text : "{}");
    }
    cJSON_free(text);
    fs::rename(tmp, prefs_file(dir), ec); // atomic: the answer survives a crash
    return !ec;
}

bool may_check_at_startup(const Prefs& p) { return p.ask == Ask::Startup; }

// ── the decision ──────────────────────────────────────────────────────────────

Offer decide(const Feed& feed, const std::string& current, const std::string& skip_version) {
    Offer o;
    o.current = current;
    if (!feed.ok || compare_versions(feed.latest, current) <= 0) return o;
    for (const auto& r : feed.releases)
        if (r.version == feed.latest) {
            o.release = r;
            break;
        }
    if (o.release.version.empty() || !o.release.has_artifact) return o; // nothing to fetch here
    if (!skip_version.empty() && compare_versions(feed.latest, skip_version) <= 0) {
        o.skipped = true;
        return o;
    }
    o.available = true;
    return o;
}

std::string describe(const Offer& o) {
    if (!o.available && !o.skipped) return {};
    const Release& r = o.release;
    std::string s = r.version + (r.change == "breaking" ? " (breaking)" : "") + " is available";
    s += " (you have " + o.current + ").\n";
    if (!r.summary.empty()) s += r.summary + "\n";
    if (!r.adds.empty()) {
        s += "\nNew:\n";
        for (const auto& a : r.adds) s += "  - " + a + "\n";
    }
    if (!r.behavior_changes.empty()) {
        s += "\nWhat behaves differently:\n";
        for (const auto& b : r.behavior_changes) {
            s += "  - " + b.what + "\n";
            if (!b.who_is_affected.empty()) s += "    affects: " + b.who_is_affected + "\n";
        }
    }
    return s;
}

// ── the network ───────────────────────────────────────────────────────────────

bool safe_url(const std::string& u) {
    if (u.rfind("https://", 0) != 0 || u.size() > 2048) return false;
    for (char c : u)
        if (c == '"' || c == '\'' || c == '`' || c == '\\' || std::isspace((unsigned char)c) ||
            (unsigned char)c < 0x20)
            return false;
    return true;
}

Http curl_http() {
    return [](const std::string& url, const fs::path& to) {
        HttpResult r;
        if (!safe_url(url)) {
            r.error = "refusing \"" + url + "\": not a plain https URL";
            return r;
        }
        std::error_code ec;
        fs::create_directories(to.parent_path(), ec);
        fs::remove(to, ec);
        // -L: release downloads redirect to a storage host. --max-time: a hung
        // network must not look like a hung application. -w: the status, alone
        // on the last line (anything before it is curl's own diagnostic).
        std::string cmd = "curl -sSL --max-time 120 -o " + shell_quote(to.string()) +
                          " -w \"\\n%{http_code}\" " + shell_quote(url) + " 2>&1";
        std::string tail = run_capture(cmd);
        std::string code = tail, note;
        if (auto nl = tail.find_last_of('\n'); nl != std::string::npos) {
            note = tail.substr(0, nl);
            code = tail.substr(nl + 1);
        }
        while (!code.empty() && (code.back() == '\r' || code.back() == ' ')) code.pop_back();
        bool numeric = !code.empty() && code.size() <= 4 &&
                       std::all_of(code.begin(), code.end(), [](char c) { return c >= '0' && c <= '9'; });
        r.status = numeric ? std::stoi(code) : 0;
        if (r.status == 0) {
            r.error = "could not reach " + url + (note.empty() ? std::string() : " (" + note + ")");
        } else if (r.status < 200 || r.status >= 300) {
            r.error = url + " answered HTTP " + std::to_string(r.status);
        } else {
            r.ok = true;
        }
        if (!r.ok) fs::remove(to, ec);
        return r;
    };
}

CheckResult check(const Http& http, const AppIdentity& self, Prefs& prefs, const fs::path& tmp) {
    CheckResult c;
    if (!http) {
        c.error = "no network on this platform";
        return c;
    }
    const std::string url = prefs.feed_url.empty() ? self.feed_url : prefs.feed_url;
    const fs::path body = tmp / ("." + self.app + "-feed.json");
    HttpResult h = http(url, body);
    prefs.last_checked = now_iso8601();
    if (!h.ok) {
        c.error = h.error;
        return c;
    }
    std::string text = read_file(body);
    std::error_code ec;
    fs::remove(body, ec);
    c.feed = parse_feed(text, self.app, self.platform);
    if (!c.feed.ok) {
        c.error = c.feed.error;
        return c;
    }
    c.offer = decide(c.feed, self.version, prefs.skip_version);
    c.ok = true;
    return c;
}

std::string sha256_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::string bytes = read_file(p);
    return maiz::reduce::detail::sha256_hex(bytes, 32);
}

Download download(const Http& http, const Release& r, const fs::path& dir) {
    Download d;
    if (!http) {
        d.error = "no network on this platform";
        return d;
    }
    if (!r.has_artifact) {
        d.error = "this release has nothing verifiable for this platform";
        return d;
    }
    d.file = dir / r.file;
    HttpResult h = http(r.url, d.file);
    if (!h.ok) {
        d.error = h.error;
        return d;
    }
    std::string got = sha256_file(d.file);
    if (got != r.sha256) {
        std::error_code ec;
        fs::remove(d.file, ec); // never leave a bad download where it could be run
        d.error = "the download does not match its sha256 (expected " + r.sha256 + ", got " +
                  (got.empty() ? "nothing" : got) + "); it was deleted";
        return d;
    }
    d.ok = true;
    return d;
}

// ── applying ──────────────────────────────────────────────────────────────────

ApplyKind apply_kind(const fs::path& file) {
    std::string n = file.filename().string();
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    auto ends = [&](const char* s) {
        std::string x = s;
        return n.size() >= x.size() && n.compare(n.size() - x.size(), x.size(), x) == 0;
    };
    if (ends(".exe") || ends(".msi")) return ApplyKind::Installer;
    if (ends(".zip") || ends(".tar.gz") || ends(".tgz")) return ApplyKind::Archive;
    if (ends(".apk")) return ApplyKind::Package;
    return ApplyKind::Unknown;
}

bool launch_detached(const fs::path& binary) {
    std::error_code ec;
    if (!fs::exists(binary, ec)) return false;
#if defined(_WIN32)
    // `start ""` detaches; the empty title keeps a quoted path from being read as one
    std::string cmd = "start \"\" /D " + shell_quote(binary.parent_path().string()) + " " +
                      shell_quote(binary.string());
#else
    std::string cmd = "cd " + shell_quote(binary.parent_path().string()) + " && nohup " +
                      shell_quote(binary.string()) + " >/dev/null 2>&1 &";
#endif
    return std::system(cmd.c_str()) == 0;
}

bool launch_installer(const fs::path& file) { return launch_detached(file); }

Unpacked unpack_beside(const fs::path& archive, const AppIdentity& self) {
    Unpacked u;
    std::error_code ec;
    if (!fs::exists(archive, ec)) {
        u.error = "no archive at " + archive.string();
        return u;
    }
    fs::path parent = self.install_dir.empty() ? archive.parent_path() : self.install_dir.parent_path();
    std::string stem = archive.filename().string();
    for (const char* ext : {".tar.gz", ".tgz", ".zip"}) {
        std::string e = ext;
        if (stem.size() > e.size() && stem.compare(stem.size() - e.size(), e.size(), e) == 0) {
            stem.resize(stem.size() - e.size());
            break;
        }
    }
    u.folder = parent / stem;
    if (!self.install_dir.empty() && fs::equivalent(u.folder, self.install_dir, ec)) {
        u.error = "refusing to unpack over the running copy (" + u.folder.string() + ")";
        return u;
    }
    if (fs::exists(u.folder, ec)) {
        u.error = u.folder.string() + " already exists; the new version may already be installed there";
        return u;
    }
    // unpack into a scratch folder first: nothing half-written ever carries the real name
    fs::path scratch = parent / (stem + ".partial");
    fs::remove_all(scratch, ec);
    fs::create_directories(scratch, ec);
    std::string cmd = system_tar() + " -xf " + shell_quote(archive.string()) + " -C " +
                      shell_quote(scratch.string()) + " 2>&1";
    std::string out = run_capture(cmd);
    // one top folder: that IS the release; otherwise the scratch folder is
    std::vector<fs::path> entries;
    for (const auto& e : fs::directory_iterator(scratch, ec)) entries.push_back(e.path());
    if (entries.empty()) {
        fs::remove_all(scratch, ec);
        u.error = "the archive unpacked to nothing" + (out.empty() ? std::string() : ": " + out);
        return u;
    }
    if (entries.size() == 1 && fs::is_directory(entries[0], ec)) {
        fs::rename(entries[0], u.folder, ec);
        fs::remove_all(scratch, ec);
    } else {
        fs::rename(scratch, u.folder, ec);
    }
    if (ec || !fs::exists(u.folder)) {
        u.error = "could not put the new version at " + u.folder.string();
        return u;
    }
    u.binary = u.folder / self.executable;
    if (self.executable.empty() || !fs::exists(u.binary, ec)) {
        u.error = "unpacked, but " + self.executable + " is not in " + u.folder.string();
        return u;
    }
    u.ok = true;
    return u;
}

// ── Android ───────────────────────────────────────────────────────────────────

#if defined(__ANDROID__)

namespace {

struct Env {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;
    explicit Env(ANativeActivity* a) : vm(a ? a->vm : nullptr) {
        if (!vm) return;
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        }
    }
    ~Env() {
        if (attached) vm->DetachCurrentThread();
    }
    /* A Java exception becomes a string and is cleared: it must never unwind into C++. */
    bool failed(std::string& error, const char* what) {
        if (!env->ExceptionCheck()) return false;
        jthrowable ex = env->ExceptionOccurred();
        env->ExceptionClear();
        error = what;
        if (ex) {
            jclass cls = env->GetObjectClass(ex);
            jmethodID msg = env->GetMethodID(cls, "toString", "()Ljava/lang/String;");
            if (auto js = (jstring)env->CallObjectMethod(ex, msg)) {
                const char* c = env->GetStringUTFChars(js, nullptr);
                error += std::string(": ") + c;
                env->ReleaseStringUTFChars(js, c);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        return true;
    }
};

jobject app_context(JNIEnv* env, ANativeActivity* a) {
    jclass cls = env->GetObjectClass(a->clazz);
    jmethodID m = env->GetMethodID(cls, "getApplicationContext", "()Landroid/content/Context;");
    return env->CallObjectMethod(a->clazz, m);
}

} // namespace

Http android_http(ANativeActivity* activity) {
    return [activity](const std::string& url, const fs::path& to) {
        HttpResult r;
        if (!safe_url(url)) {
            r.error = "refusing \"" + url + "\": not a plain https URL";
            return r;
        }
        Env e(activity);
        if (!e.env) {
            r.error = "no JNI environment for this thread";
            return r;
        }
        JNIEnv* env = e.env;
        jclass url_cls = env->FindClass("java/net/URL");
        jmethodID url_ctor = env->GetMethodID(url_cls, "<init>", "(Ljava/lang/String;)V");
        jstring jurl = env->NewStringUTF(url.c_str());
        jobject u = env->NewObject(url_cls, url_ctor, jurl);
        if (e.failed(r.error, "URL")) return r;
        jmethodID open = env->GetMethodID(url_cls, "openConnection", "()Ljava/net/URLConnection;");
        jobject conn = env->CallObjectMethod(u, open);
        if (e.failed(r.error, "openConnection")) return r;
        jclass conn_cls = env->FindClass("java/net/HttpURLConnection");
        env->CallVoidMethod(conn, env->GetMethodID(conn_cls, "setInstanceFollowRedirects", "(Z)V"), JNI_TRUE);
        env->CallVoidMethod(conn, env->GetMethodID(conn_cls, "setConnectTimeout", "(I)V"), 15000);
        env->CallVoidMethod(conn, env->GetMethodID(conn_cls, "setReadTimeout", "(I)V"), 60000);
        jint status = env->CallIntMethod(conn, env->GetMethodID(conn_cls, "getResponseCode", "()I"));
        if (e.failed(r.error, "HTTP request")) return r;
        r.status = status;
        if (status < 200 || status >= 300) {
            r.error = url + " answered HTTP " + std::to_string(status);
            return r;
        }
        jobject in = env->CallObjectMethod(conn, env->GetMethodID(conn_cls, "getInputStream", "()Ljava/io/InputStream;"));
        if (e.failed(r.error, "getInputStream")) return r;
        jclass in_cls = env->FindClass("java/io/InputStream");
        jmethodID read = env->GetMethodID(in_cls, "read", "([B)I");
        jbyteArray buf = env->NewByteArray(64 * 1024);
        std::error_code ec;
        fs::create_directories(to.parent_path(), ec);
        std::ofstream out(to, std::ios::binary | std::ios::trunc);
        std::vector<jbyte> chunk(64 * 1024);
        for (;;) {
            jint n = env->CallIntMethod(in, read, buf);
            if (e.failed(r.error, "read")) break;
            if (n < 0) {
                r.ok = true;
                break;
            }
            env->GetByteArrayRegion(buf, 0, n, chunk.data());
            out.write(reinterpret_cast<const char*>(chunk.data()), n);
        }
        env->CallVoidMethod(in, env->GetMethodID(in_cls, "close", "()V"));
        if (env->ExceptionCheck()) env->ExceptionClear();
        out.close();
        if (!r.ok) fs::remove(to, ec);
        return r;
    };
}

bool android_install_package(ANativeActivity* activity, const fs::path& apk, std::string* error) {
    std::string err;
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    Env e(activity);
    if (!e.env) return fail("no JNI environment for this thread");
    JNIEnv* env = e.env;
    std::string bytes = read_file(apk);
    if (bytes.empty()) return fail("could not read " + apk.string());

    jobject ctx = app_context(env, activity);
    jclass ctx_cls = env->GetObjectClass(ctx);
    jobject pm = env->CallObjectMethod(ctx, env->GetMethodID(ctx_cls, "getPackageManager", "()Landroid/content/pm/PackageManager;"));
    jclass pm_cls = env->GetObjectClass(pm);
    jobject pi = env->CallObjectMethod(pm, env->GetMethodID(pm_cls, "getPackageInstaller", "()Landroid/content/pm/PackageInstaller;"));
    if (e.failed(err, "getPackageInstaller")) return fail(err);

    // a session: write the verified bytes in, commit, and Android asks the person
    jclass params_cls = env->FindClass("android/content/pm/PackageInstaller$SessionParams");
    jobject params = env->NewObject(params_cls, env->GetMethodID(params_cls, "<init>", "(I)V"), 1 /* MODE_FULL_INSTALL */);
    jclass pi_cls = env->GetObjectClass(pi);
    jint id = env->CallIntMethod(pi, env->GetMethodID(pi_cls, "createSession", "(Landroid/content/pm/PackageInstaller$SessionParams;)I"), params);
    if (e.failed(err, "createSession")) return fail(err);
    jobject session = env->CallObjectMethod(pi, env->GetMethodID(pi_cls, "openSession", "(I)Landroid/content/pm/PackageInstaller$Session;"), id);
    if (e.failed(err, "openSession")) return fail(err);
    jclass s_cls = env->GetObjectClass(session);
    jstring name = env->NewStringUTF("update.apk");
    jobject out = env->CallObjectMethod(session, env->GetMethodID(s_cls, "openWrite", "(Ljava/lang/String;JJ)Ljava/io/OutputStream;"),
                                        name, (jlong)0, (jlong)bytes.size());
    if (e.failed(err, "openWrite")) return fail(err);
    jclass os_cls = env->FindClass("java/io/OutputStream");
    jmethodID write = env->GetMethodID(os_cls, "write", "([BII)V");
    const std::size_t step = 256 * 1024;
    jbyteArray buf = env->NewByteArray((jsize)step);
    for (std::size_t off = 0; off < bytes.size(); off += step) {
        jsize n = (jsize)std::min(step, bytes.size() - off);
        env->SetByteArrayRegion(buf, 0, n, reinterpret_cast<const jbyte*>(bytes.data() + off));
        env->CallVoidMethod(out, write, buf, 0, n);
        if (e.failed(err, "write")) return fail(err);
    }
    env->CallVoidMethod(session, env->GetMethodID(s_cls, "fsync", "(Ljava/io/OutputStream;)V"), out);
    env->CallVoidMethod(out, env->GetMethodID(os_cls, "close", "()V"));
    if (e.failed(err, "fsync/close")) return fail(err);

    // the status comes back as an Intent to this activity; the system shows its
    // own confirmation dialog either way
    jclass intent_cls = env->FindClass("android/content/Intent");
    jobject intent = env->NewObject(intent_cls, env->GetMethodID(intent_cls, "<init>", "(Landroid/content/Context;Ljava/lang/Class;)V"),
                                    ctx, env->GetObjectClass(activity->clazz));
    jclass pend_cls = env->FindClass("android/app/PendingIntent");
    const jint FLAG_MUTABLE = 0x02000000; // the installer fills in the status extras
    jobject pend = env->CallStaticObjectMethod(pend_cls,
        env->GetStaticMethodID(pend_cls, "getActivity", "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;"),
        ctx, id, intent, FLAG_MUTABLE);
    if (e.failed(err, "PendingIntent")) return fail(err);
    jobject sender = env->CallObjectMethod(pend, env->GetMethodID(pend_cls, "getIntentSender", "()Landroid/content/IntentSender;"));
    env->CallVoidMethod(session, env->GetMethodID(s_cls, "commit", "(Landroid/content/IntentSender;)V"), sender);
    if (e.failed(err, "commit")) return fail(err);
    return true;
}

#else

Http android_http(ANativeActivity*) { return {}; }

bool android_install_package(ANativeActivity*, const fs::path&, std::string* error) {
    if (error) *error = "not Android";
    return false;
}

#endif

// ── apply ─────────────────────────────────────────────────────────────────────

ApplyResult apply(const Download& d, const AppIdentity& self, ANativeActivity* activity) {
    ApplyResult r;
    if (!d.ok) {
        r.message = "nothing verified to apply";
        return r;
    }
    switch (apply_kind(d.file)) {
    case ApplyKind::Installer:
        r.ok = launch_installer(d.file);
        r.quit_now = r.ok;
        r.message = r.ok ? "The installer is running. This copy stays where it is until the new one works."
                         : "The installer could not be started. It is verified and saved at " + d.file.string();
        return r;
    case ApplyKind::Archive: {
        Unpacked u = unpack_beside(d.file, self);
        if (!u.ok) {
            r.message = u.error;
            return r;
        }
        r.ok = launch_detached(u.binary);
        r.quit_now = r.ok;
        r.message = r.ok ? "The new version is at " + u.folder.string() + " and is starting. This copy is untouched."
                         : "Unpacked to " + u.folder.string() + ", but it could not be started. Start it from there.";
        return r;
    }
    case ApplyKind::Package: {
        std::string why;
        r.ok = android_install_package(activity, d.file, &why);
        r.message = r.ok ? "Android will ask you to confirm the update." : "Android's installer refused: " + why;
        return r;
    }
    case ApplyKind::Unknown: break;
    }
    r.message = "this platform does not know how to apply " + d.file.filename().string();
    return r;
}

// ── the runner ────────────────────────────────────────────────────────────────

struct Updater::Work {
    enum class Kind { Check, Download } kind;
    std::future<CheckResult> check;
    std::future<Download> download;
    Prefs prefs; // the copy the worker stamped
};

Updater::Updater(AppIdentity self, Http http) : self_(std::move(self)), http_(std::move(http)) {}

Updater::~Updater() { finish_work(); }

void Updater::finish_work() {
    if (!work_) return;
    // a destructor or a new request waits for the worker rather than abandoning
    // a thread that still references this object's copies
    if (work_->check.valid()) work_->check.wait();
    if (work_->download.valid()) work_->download.wait();
    delete work_;
    work_ = nullptr;
}

void Updater::start_up() {
    prefs_ = load_prefs(self_.prefs_dir);
    if (may_check_at_startup(prefs_)) begin_check(); // rule 1: only when chosen
}

void Updater::begin_check() {
    if (busy()) return;
    finish_work();
    work_ = new Work{Work::Kind::Check, {}, {}, prefs_};
    fs::path tmp = self_.prefs_dir / "tmp";
    Work* w = work_;
    AppIdentity self = self_;
    Http http = http_;
    work_->check = std::async(std::launch::async, [w, self, http, tmp]() mutable {
        return check(http, self, w->prefs, tmp);
    });
    stage_ = Stage::Checking;
    error_.clear();
}

void Updater::begin_download() {
    if (busy() || !offer_.available) return;
    finish_work();
    work_ = new Work{Work::Kind::Download, {}, {}, prefs_};
    fs::path dir = self_.prefs_dir / "downloads";
    Release rel = offer_.release;
    Http http = http_;
    work_->download = std::async(std::launch::async, [http, rel, dir]() { return download(http, rel, dir); });
    stage_ = Stage::Downloading;
    error_.clear();
}

bool Updater::poll() {
    if (!work_) return false;
    using namespace std::chrono_literals;
    if (work_->kind == Work::Kind::Check && work_->check.valid() &&
        work_->check.wait_for(0ms) == std::future_status::ready) {
        CheckResult c = work_->check.get();
        prefs_.last_checked = work_->prefs.last_checked;
        save_prefs(self_.prefs_dir, prefs_); // the stamp, now that it is ours
        offer_ = c.offer;
        if (!c.ok) {
            error_ = c.error;
            stage_ = Stage::Failed;
        } else {
            stage_ = offer_.available ? Stage::Offered : Stage::UpToDate;
        }
        finish_work();
        return true;
    }
    if (work_->kind == Work::Kind::Download && work_->download.valid() &&
        work_->download.wait_for(0ms) == std::future_status::ready) {
        download_ = work_->download.get();
        if (!download_.ok) {
            error_ = download_.error;
            stage_ = Stage::Failed;
        } else {
            stage_ = Stage::Ready;
        }
        finish_work();
        return true;
    }
    return false;
}

void Updater::set_ask(Ask a) {
    prefs_.ask = a;
    save_prefs(self_.prefs_dir, prefs_);
}

void Updater::skip_offered() {
    if (offer_.release.version.empty()) return;
    prefs_.skip_version = offer_.release.version;
    save_prefs(self_.prefs_dir, prefs_);
    offer_.available = false;
    offer_.skipped = true;
    stage_ = Stage::UpToDate;
}

} // namespace maiz::update
