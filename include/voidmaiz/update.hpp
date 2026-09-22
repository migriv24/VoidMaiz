/*
 * voidmaiz/update.hpp — an application updating itself (UI-free).
 *
 * okf/concepts/updates.md. The author, 2026-09-21: "updating the application,
 * updates in general, should be handled and exist in maiz. Just as networking
 * has been moved to maiz naturally … all applications should be able to update
 * themselves." Adapted from Void Hormiga's client (src/update/, 2026-09-04),
 * which proved the design on a shipping application; this is that design with
 * the application's name taken out.
 *
 * ── THE LINE ─────────────────────────────────────────────────────────────────
 *
 *   Void Mago     what a release IS: the files that ship, the installer, and
 *                 `void-updates.json` (`mago feed`), the document every client
 *                 reads. A BUILD-TIME tool; it never runs on a user's machine,
 *                 and nothing here needs it installed (an updater that needed
 *                 the thing it updates could not repair a broken install).
 *   Void Maiz     what updating LOOKS like and the in-app half every
 *                 application would otherwise write: asking permission to
 *                 check, fetching the feed, comparing, the prompt (with the
 *                 release's behavior changes), downloading, verifying, and
 *                 handing the verified file to the platform.
 *   the app       who it is (AppIdentity), where its feed lives, and anything
 *                 about its own data a new version must be told.
 *
 * ── THE TWO RULES ────────────────────────────────────────────────────────────
 *
 * 1. NEVER CHECK WITHOUT BEING ASKED TO. A check is a network request a person
 *    did not make. Preferences start `Unasked`; a first launch asks permission
 *    to check before it ever checks, and "don't ask again" is a real answer.
 * 2. NEVER INSTALL WITHOUT BEING TOLD TO. `download` fetches and verifies; it
 *    runs nothing. Applying is a separate call a front-end makes after a person
 *    chose it, and it never replaces the running copy: side-by-side installs are
 *    what make "try the update" reversible.
 *
 * ── PLATFORMS: THE SAME DECISION, THREE WAYS TO APPLY IT ────────────────────
 *
 * The feed names a file per platform, and what to do with it is read from the
 * file, not from #ifdef, so a new packaging on any platform needs no new code:
 *
 *   installer (.exe .msi)          run it; it installs beside the running copy
 *   archive (.zip .tar.gz .tgz)    unpack beside the running copy; start the new one
 *   package (.apk)                 hand the verified bytes to the platform's own
 *                                  installer (Android: PackageInstaller, which
 *                                  shows its own confirmation)
 *
 * The network is a seam (`Http`), because there is no `curl` on a phone:
 * `curl_http()` on the desktop, `android_http()` on Android.
 */
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ANativeActivity; // Android only

namespace maiz::update {

namespace fs = std::filesystem;

/* The key this build looks for in a feed's `artifacts`: "windows-x64",
 * "linux-x64", "macos-arm64", "android-arm64", decided at compile time. */
std::string platform_tag();

/* Who is asking. The application fills this once. */
struct AppIdentity {
    std::string app;        // its key in the feed ("interactioncombinators")
    std::string display;    // "Interaction Combinators"
    std::string version;    // this build's version ("0.2.0")
    std::string platform = platform_tag();
    std::string feed_url;   // where void-updates.json lives (https only)
    fs::path prefs_dir;     // PER MACHINE, surviving the update (not the document)
    fs::path install_dir;   // the running copy's folder (archive apply unpacks beside it)
    std::string executable; // the binary inside an unpacked archive ("interaction_combinators.exe")
};

// ── the feed (Mago's format: void-updates/0.1) ──────────────────────────────

struct BehaviorChange {
    std::string what;
    std::string who_is_affected;
};

struct Release {
    std::string version, date, change, summary;
    std::vector<std::string> adds;
    std::vector<BehaviorChange> behavior_changes;
    // the artifact for THIS platform, resolved while parsing
    bool has_artifact = false;
    std::string file, url, sha256, signature;
    long long bytes = 0;
};

struct Feed {
    bool ok = false;
    std::string error;
    std::string display_name;
    std::string latest;
    std::vector<Release> releases;
};

/* Dotted-numeric compare: -1 / 0 / 1. Missing components are zero ("0.1" <
 * "0.1.1"); non-numeric tails compare lexically after the numbers. It must never
 * call a newer version older, which is the only way to fail dangerously. */
int compare_versions(const std::string& a, const std::string& b);

/* Read a feed, keeping only `app`'s entry and only `platform`'s artifacts.
 * A document that is not void-updates/<n> is refused. An artifact whose sha256 is
 * missing or malformed is treated as ABSENT (an unverifiable download is not an
 * offer), as is one whose URL is not https or whose filename could escape. */
Feed parse_feed(const std::string& json, const std::string& app, const std::string& platform);

// ── preferences: per machine, never per document ────────────────────────────
// Not `config set`: Core's config rides the document, so "don't ask me about
// updates" would travel to another device on the next merge.

enum class Ask {
    Unasked, // never asked: ask before the first network request
    Never,   // "don't ask again", and it is real
    Startup, // check when the application starts
};

struct Prefs {
    Ask ask = Ask::Unasked;
    std::string feed_url;     // empty = the application's default
    std::string skip_version; // "not this one"; a newer release asks again
    std::string last_checked; // ISO-8601, informational
};

const char* ask_name(Ask);
Ask ask_from_name(const std::string&);
Prefs load_prefs(const fs::path& dir);
bool save_prefs(const fs::path& dir, const Prefs&);

/* Rule 1 as a function: true only when the person chose checks at startup. */
bool may_check_at_startup(const Prefs&);

// ── the decision ────────────────────────────────────────────────────────────

struct Offer {
    bool available = false; // strictly newer than `current`, has an artifact, not skipped
    bool skipped = false;   // newer, but the person said not this one
    Release release;
    std::string current;
};

Offer decide(const Feed& feed, const std::string& current, const std::string& skip_version);

/* What the prompt says: the summary, what it adds, and every behavior change
 * with who it affects. One function, so a GUI and a CLI say the same words. */
std::string describe(const Offer&);

// ── the network seam ────────────────────────────────────────────────────────

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string error;
};
/* GET `url`, following redirects, into the file `to`. */
using Http = std::function<HttpResult(const std::string& url, const fs::path& to)>;

/* The desktop default: the system `curl` (Windows 10 1803+, macOS, Linux),
 * -L for release redirects, a time limit so a hung network is not a hung app. */
Http curl_http();

/* Android: java.net.HttpURLConnection through JNI. Call from a thread the
 * JavaVM can attach (a worker thread is right: never block the render loop on
 * the network). Empty elsewhere. */
Http android_http(ANativeActivity* activity);

bool safe_url(const std::string& url); // https://, no quotes or whitespace

// ── check, download, apply ──────────────────────────────────────────────────

struct CheckResult {
    bool ok = false;
    std::string error;
    Feed feed;
    Offer offer;
};
/* Fetch + parse + decide. It stamps `prefs.last_checked` and does NOT save: a
 * check never quietly rewrites a preference. Calling it is the caller asserting
 * that a person asked (or chose Startup): see may_check_at_startup. */
CheckResult check(const Http&, const AppIdentity&, Prefs&, const fs::path& tmp);

struct Download {
    bool ok = false;
    std::string error;
    fs::path file;
};
/* Fetch the artifact into `dir` and CHECK ITS DIGEST BEFORE RETURNING OK. A
 * mismatch deletes the file: a bad download left on disk is how it gets run. */
Download download(const Http&, const Release&, const fs::path& dir);

std::string sha256_file(const fs::path&); // lowercase hex, "" if unreadable

enum class ApplyKind { Installer, Archive, Package, Unknown };
ApplyKind apply_kind(const fs::path& file); // by the name the feed gave it

/* Run a verified installer, detached. It installs beside the running copy;
 * nothing here deletes the version calling it. */
bool launch_installer(const fs::path& file);

struct Unpacked {
    bool ok = false;
    std::string error;
    fs::path folder; // the new version, BESIDE the running one
    fs::path binary; // folder / identity.executable
};
/* Unpack a verified archive into install_dir's PARENT, in a folder named after
 * the archive. An archive holding one top folder is used as-is; a flat archive
 * (IC's zip) gets its folder made for it. Refuses to unpack over the running
 * copy or over an existing folder. Uses the system `tar` (bsdtar on Windows
 * reads .zip). */
Unpacked unpack_beside(const fs::path& archive, const AppIdentity& self);

/* The system tar this module unpacks with (quoted, ready for a command line):
 * the System32 tar.exe on Windows, never whatever `tar` is first on
 * PATH. Exposed so tests and tools pack with the same one. */
std::string system_tar();

/* Start a binary detached (the unpacked new version). The caller then quits. */
bool launch_detached(const fs::path& binary);

/* Android: hand a verified .apk to PackageInstaller, which shows the system's
 * own confirmation. Needs REQUEST_INSTALL_PACKAGES in the manifest, and the
 * person allowing installs from this app the first time. The update must be
 * signed with the SAME key as the installed app, or Android refuses it. */
bool android_install_package(ANativeActivity* activity, const fs::path& apk, std::string* error);

/* Apply a verified download by what it is, and say whether the application
 * should quit now so the new version can take over:
 *   installer  launch it (side by side)                      -> quit
 *   archive    unpack beside, launch the new binary          -> quit
 *   package    hand to Android's installer (it asks the person) -> keep running
 * `activity` is needed only for a package on Android. */
struct ApplyResult {
    bool ok = false;
    bool quit_now = false;
    std::string message; // what to tell the person, success or not
};
ApplyResult apply(const Download& d, const AppIdentity& self, ANativeActivity* activity = nullptr);

/* ── the runner: the same steps, off the render thread ─────────────────────
 * A check or a download must never freeze a frame (a phone on a slow network
 * would look hung). Updater runs them on a worker thread and a host polls once a
 * frame. It holds no UI; voidmaiz/updateview.hpp draws it. Not thread-safe:
 * one thread calls every method. */
class Updater {
  public:
    enum class Stage { Idle, Checking, UpToDate, Offered, Downloading, Ready, Failed };

    Updater() = default;
    Updater(AppIdentity self, Http http);
    ~Updater();
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;

    /* Load preferences and, ONLY if the person chose it, start a check. */
    void start_up();
    void begin_check();    // the person asked ("Check now")
    void begin_download(); // the person chose the offered update
    /* Once a frame: collect finished work. True when the stage changed. */
    bool poll();

    Stage stage() const { return stage_; }
    bool busy() const { return stage_ == Stage::Checking || stage_ == Stage::Downloading; }
    const Offer& offer() const { return offer_; }
    const Download& downloaded() const { return download_; }
    const std::string& error() const { return error_; }
    const AppIdentity& self() const { return self_; }

    Prefs& prefs() { return prefs_; }
    void set_ask(Ask a);        // saves
    void skip_offered();        // "not this one": saves, and the offer goes away
    bool needs_consent() const { return prefs_.ask == Ask::Unasked; }

  private:
    struct Work;
    AppIdentity self_;
    Http http_;
    Prefs prefs_;
    Stage stage_ = Stage::Idle;
    Offer offer_;
    Download download_;
    std::string error_;
    Work* work_ = nullptr; // the in-flight job, if any
    void finish_work();
};

} // namespace maiz::update
