/* update_smoke.cpp — an application updating itself (voidmaiz/update.hpp).
 *
 * No network: the Http seam is a fake that "serves" files from a folder, which
 * is exactly why the network is a seam. Everything the two rules depend on is
 * pinned here: nothing offered without a verifiable artifact, a bad download
 * deleted, never unpacking over the running copy. */
#include "voidmaiz/update.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz::update;

static void write(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << s;
}

/* std::system through cmd.exe needs the same outer quotes run_capture adds. */
static int sh(const std::string& cmd) {
#if defined(_WIN32)
    return std::system(("\"" + cmd + "\"").c_str());
#else
    return std::system(cmd.c_str());
#endif
}

static std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/* A fake network: URL -> file on disk. */
static Http fake(std::map<std::string, fs::path> routes) {
    return [routes](const std::string& url, const fs::path& to) {
        HttpResult r;
        auto it = routes.find(url);
        if (it == routes.end()) {
            r.status = 404;
            r.error = url + " answered HTTP 404";
            return r;
        }
        fs::create_directories(to.parent_path());
        fs::copy_file(it->second, to, fs::copy_options::overwrite_existing);
        r.ok = true;
        r.status = 200;
        return r;
    };
}

static std::string feed_json(const std::string& latest, const std::string& sha, const std::string& file,
                             const std::string& url) {
    return std::string(R"({"feed":"void-updates/0.1","applications":{"ic":{"display_name":"IC","latest":")") +
           latest + R"(","releases":[{"version":")" + latest +
           R"(","change":"compatible","summary":"rotation works","adds":["a phone layout"],)"
           R"("behavior_changes":[{"what":"the camera fits the net on a phone","who_is_affected":"phone users"}],)"
           R"("artifacts":{")" + platform_tag() + R"(":{"file":")" + file + R"(","url":")" + url +
           R"(","bytes":5,"sha256":")" + sha + R"(","signature":null}}}]},"other":{"latest":"9.9.9"}}})";
}

/* `maiz_update_smoke --probe <feed.json> <app> <platform> <current>`: read a
 * REAL feed (a release script's output) through the client, and say what it
 * would offer. The release script's feed is only worth uploading if this reads it. */
static int probe(int argc, char** argv) {
    if (argc < 6) return 2;
    std::string text = read(argv[2]);
    Feed f = parse_feed(text, argv[3], argv[4]);
    if (!f.ok) {
        std::cout << "probe: the client refuses this feed: " << f.error << '\n';
        return 1;
    }
    Offer o = decide(f, argv[5], "");
    std::cout << "probe: latest " << f.latest << ", offer for " << argv[5] << " on " << argv[4] << ": "
              << (o.available ? "YES, " + o.release.file : std::string("no")) << '\n';
    if (o.available) std::cout << describe(o);
    return o.available ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--probe") return probe(argc, argv);
    fs::path root = fs::temp_directory_path() / "maiz_update_smoke";
    fs::remove_all(root);
    fs::create_directories(root);

    // ── versions: never call a newer version older ──────────────────────────
    CHECK(compare_versions("0.2.0", "0.3.0") < 0);
    CHECK(compare_versions("0.10.0", "0.9.9") > 0); // numeric, not lexical
    CHECK(compare_versions("0.1", "0.1.0") == 0);
    CHECK(compare_versions("0.1", "0.1.1") < 0);
    CHECK(compare_versions("1.0.0", "1.0.0-rc1") > 0);
    CHECK(compare_versions("1.0.0-rc1", "1.0.0-rc2") < 0);
    CHECK(compare_versions("2.0.0", "10.0.0") < 0);

    // ── the feed: only our entry, only our platform, only what is verifiable ─
    std::string sha_of_hello = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"; // "hello"
    {
        Feed f = parse_feed(feed_json("0.3.0", sha_of_hello, "ic.zip", "https://x.test/ic.zip"), "ic",
                            platform_tag());
        CHECK(f.ok && f.latest == "0.3.0" && f.releases.size() == 1);
        CHECK(f.releases[0].has_artifact && f.releases[0].file == "ic.zip");
        CHECK(f.releases[0].behavior_changes.size() == 1 && f.releases[0].adds.size() == 1);

        CHECK(!parse_feed("{}", "ic", platform_tag()).ok);
        CHECK(!parse_feed(R"({"feed":"something-else/1"})", "ic", platform_tag()).ok);
        CHECK(!parse_feed(feed_json("0.3.0", sha_of_hello, "ic.zip", "https://x.test/ic.zip"), "nobody",
                          platform_tag()).ok);
        // unverifiable, unsafe, or escaping: present in the feed, absent as an artifact
        CHECK(!parse_feed(feed_json("0.3.0", "", "ic.zip", "https://x.test/ic.zip"), "ic", platform_tag())
                   .releases[0].has_artifact); // no digest (Mago's "sha256": null)
        CHECK(!parse_feed(feed_json("0.3.0", sha_of_hello, "ic.zip", "http://x.test/ic.zip"), "ic",
                          platform_tag()).releases[0].has_artifact); // not https
        CHECK(!parse_feed(feed_json("0.3.0", sha_of_hello, "../../evil.exe", "https://x.test/e"), "ic",
                          platform_tag()).releases[0].has_artifact); // escapes the folder
        // another platform's artifact is not ours
        CHECK(!parse_feed(feed_json("0.3.0", sha_of_hello, "ic.zip", "https://x.test/ic.zip"), "ic",
                          "plan9-mips").releases[0].has_artifact);
    }

    // ── the decision and the prompt ─────────────────────────────────────────
    {
        Feed f = parse_feed(feed_json("0.3.0", sha_of_hello, "ic.zip", "https://x.test/ic.zip"), "ic",
                            platform_tag());
        Offer o = decide(f, "0.2.0", "");
        CHECK(o.available && o.release.version == "0.3.0");
        std::string text = describe(o);
        CHECK(text.find("0.3.0 is available") != std::string::npos);
        CHECK(text.find("the camera fits the net on a phone") != std::string::npos); // behavior change shown
        CHECK(text.find("phone users") != std::string::npos);                          // and who it affects

        CHECK(!decide(f, "0.3.0", "").available); // up to date
        CHECK(!decide(f, "0.4.0", "").available); // newer than the feed
        Offer skipped = decide(f, "0.2.0", "0.3.0");
        CHECK(!skipped.available && skipped.skipped);
        CHECK(decide(f, "0.2.0", "0.2.5").available); // a newer release asks again
        Feed no_artifact = parse_feed(feed_json("0.3.0", "", "ic.zip", "https://x.test/ic.zip"), "ic",
                                      platform_tag());
        CHECK(!decide(no_artifact, "0.2.0", "").available); // nothing verifiable = no offer
    }

    // ── preferences: rule 1 starts Unasked, and the answer persists ─────────
    {
        fs::path dir = root / "prefs";
        Prefs p = load_prefs(dir);
        CHECK(p.ask == Ask::Unasked);
        CHECK(!may_check_at_startup(p));
        p.ask = Ask::Startup;
        p.skip_version = "0.3.0";
        CHECK(save_prefs(dir, p));
        Prefs back = load_prefs(dir);
        CHECK(back.ask == Ask::Startup && back.skip_version == "0.3.0");
        CHECK(may_check_at_startup(back));
        back.ask = Ask::Never;
        save_prefs(dir, back);
        CHECK(!may_check_at_startup(load_prefs(dir)));
    }

    // ── check + download: the digest is checked before "ok" ─────────────────
    {
        fs::path served = root / "served";
        write(served / "ic.zip", "hello");  // matches sha_of_hello
        write(served / "bad.zip", "HELLO"); // does not
        write(served / "feed.json", feed_json("0.3.0", sha_of_hello, "ic.zip", "https://x.test/ic.zip"));
        Http net = fake({{"https://x.test/feed.json", served / "feed.json"},
                         {"https://x.test/ic.zip", served / "ic.zip"},
                         {"https://x.test/bad.zip", served / "bad.zip"}});

        AppIdentity self;
        self.app = "ic";
        self.version = "0.2.0";
        self.feed_url = "https://x.test/feed.json";
        Prefs prefs;
        CheckResult c = check(net, self, prefs, root / "tmp");
        CHECK(c.ok && c.offer.available);
        CHECK(!prefs.last_checked.empty()); // stamped, not saved

        Download d = download(net, c.offer.release, root / "dl");
        CHECK(d.ok && fs::exists(d.file) && sha256_file(d.file) == sha_of_hello);

        Release bad = c.offer.release;
        bad.url = "https://x.test/bad.zip";
        bad.file = "bad.zip";
        Download db = download(net, bad, root / "dl");
        CHECK(!db.ok && db.error.find("sha256") != std::string::npos);
        CHECK(!fs::exists(root / "dl" / "bad.zip")); // deleted, never left to be run

        AppIdentity lost = self;
        lost.feed_url = "https://x.test/missing.json";
        CheckResult miss = check(net, lost, prefs, root / "tmp");
        CHECK(!miss.ok && miss.error.find("404") != std::string::npos);
        CHECK(!check(Http{}, self, prefs, root / "tmp").ok); // no network: an error, not a crash
    }

    // ── applying: by the file's name, and side by side ──────────────────────
    CHECK(apply_kind("IC-0.3.0-setup.exe") == ApplyKind::Installer);
    CHECK(apply_kind("IC-0.3.0-windows-x64.zip") == ApplyKind::Archive);
    CHECK(apply_kind("IC-0.3.0-linux-x64.tar.gz") == ApplyKind::Archive);
    CHECK(apply_kind("IC-0.3.0.APK") == ApplyKind::Package);
    CHECK(apply_kind("notes.txt") == ApplyKind::Unknown);
    CHECK(safe_url("https://github.com/a/b/releases/latest/download/void-updates.json"));
    CHECK(!safe_url("https://x.test/a b") && !safe_url("https://x.test/\"&calc") && !safe_url("ftp://x"));
    {
        // a FLAT archive (like IC's zip): the folder is made for it, beside the install
        fs::path stage = root / "stage";
        write(stage / "app.bin", "new version");
        write(stage / "lib.dll", "a library");
        fs::path archive = root / "downloads" / "App-0.3.0-linux-x64.tar.gz";
        fs::create_directories(archive.parent_path());
        std::string cmd = system_tar() + " -czf \"" + archive.string() + "\" -C \"" + stage.string() + "\" app.bin lib.dll";
        CHECK(sh(cmd) == 0);

        AppIdentity self;
        self.install_dir = root / "installs" / "App-0.2.0";
        self.executable = "app.bin";
        fs::create_directories(self.install_dir);
        Unpacked u = unpack_beside(archive, self);
        CHECK(u.ok);
        CHECK(u.folder == root / "installs" / "App-0.3.0-linux-x64");
        CHECK(read(u.binary) == "new version");
        CHECK(fs::exists(self.install_dir)); // the running copy is untouched
        CHECK(!fs::exists(root / "installs" / "App-0.3.0-linux-x64.partial"));

        // again: the folder exists now, so it refuses rather than overwrite
        Unpacked again = unpack_beside(archive, self);
        CHECK(!again.ok && again.error.find("already exists") != std::string::npos);

        // an archive holding ONE top folder is used as-is
        fs::path nested = root / "nested";
        write(nested / "App-0.4.0" / "app.bin", "v4");
        fs::path a4 = root / "downloads" / "App-0.4.0.tar.gz";
        cmd = system_tar() + " -czf \"" + a4.string() + "\" -C \"" + nested.string() + "\" App-0.4.0";
        CHECK(sh(cmd) == 0);
        Unpacked u4 = unpack_beside(a4, self);
        CHECK(u4.ok && read(u4.binary) == "v4");
        CHECK(u4.folder == root / "installs" / "App-0.4.0");

#if defined(_WIN32)
        // IC's real packaging on Windows: a FLAT .zip, read by the system bsdtar
        fs::path zstage = root / "zstage";
        write(zstage / "interaction_combinators.exe", "zip version");
        write(zstage / "libvoidcore.dll", "core");
        fs::path zip = root / "downloads" / "InteractionCombinators-0.3.0-windows-x64.zip";
        cmd = system_tar() + " -a -cf \"" + zip.string() + "\" -C \"" + zstage.string() +
              "\" interaction_combinators.exe libvoidcore.dll";
        CHECK(sh(cmd) == 0);
        AppIdentity ic;
        ic.install_dir = root / "installs" / "InteractionCombinators-0.2.0-windows-x64";
        ic.executable = "interaction_combinators.exe";
        fs::create_directories(ic.install_dir);
        Unpacked uz = unpack_beside(zip, ic);
        CHECK(uz.ok);
        CHECK(uz.folder == root / "installs" / "InteractionCombinators-0.3.0-windows-x64");
        CHECK(read(uz.binary) == "zip version");
        CHECK(fs::exists(uz.folder / "libvoidcore.dll"));
#endif

        // an archive missing the executable says so
        AppIdentity wrong = self;
        wrong.executable = "not-there.bin";
        fs::path a5 = root / "downloads" / "App-0.5.0.tar.gz";
        fs::copy_file(archive, a5);
        Unpacked u5 = unpack_beside(a5, wrong);
        CHECK(!u5.ok && u5.error.find("not-there.bin") != std::string::npos);
    }

    // ── the runner: off the render thread, polled once a frame ───────────────
    {
        fs::path served = root / "served2";
        write(served / "ic.zip", "hello");
        write(served / "feed.json", feed_json("0.3.0", sha_of_hello, "ic.zip", "https://x.test/ic.zip"));
        Http net = fake({{"https://x.test/feed.json", served / "feed.json"},
                         {"https://x.test/ic.zip", served / "ic.zip"}});
        AppIdentity self;
        self.app = "ic";
        self.version = "0.2.0";
        self.feed_url = "https://x.test/feed.json";
        self.prefs_dir = root / "runner";

        auto settle = [](Updater& u) {
            for (int k = 0; k < 2000 && u.busy(); ++k) {
                u.poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };

        // rule 1: a fresh install does not check at start-up; it needs consent
        Updater u(self, net);
        u.start_up();
        CHECK(u.stage() == Updater::Stage::Idle && u.needs_consent());

        u.begin_check(); // the person asked
        CHECK(u.stage() == Updater::Stage::Checking);
        settle(u);
        CHECK(u.stage() == Updater::Stage::Offered && u.offer().release.version == "0.3.0");

        u.begin_download(); // the person chose it
        settle(u);
        CHECK(u.stage() == Updater::Stage::Ready && u.downloaded().ok);
        CHECK(sha256_file(u.downloaded().file) == sha_of_hello);

        // consent given: the next start-up checks by itself
        u.set_ask(Ask::Startup);
        Updater again(self, net);
        again.start_up();
        CHECK(again.stage() == Updater::Stage::Checking);
        settle(again);
        CHECK(again.stage() == Updater::Stage::Offered);

        // "not this one" persists, and the offer goes away until something newer
        again.skip_offered();
        CHECK(again.stage() == Updater::Stage::UpToDate);
        Updater third(self, net);
        third.start_up();
        settle(third);
        CHECK(third.stage() == Updater::Stage::UpToDate && third.offer().skipped);

        // an unreachable feed is a stage, not a crash
        AppIdentity lost = self;
        lost.feed_url = "https://x.test/gone.json";
        lost.prefs_dir = root / "runner-lost";
        Updater f(lost, net);
        f.begin_check();
        settle(f);
        CHECK(f.stage() == Updater::Stage::Failed && !f.error().empty());

        // apply() refuses what was never verified
        Download nothing;
        CHECK(!apply(nothing, self).ok);
    }

    fs::remove_all(root);
    if (failures) {
        std::cerr << "update_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "update_smoke: all ok\n";
    return 0;
}
