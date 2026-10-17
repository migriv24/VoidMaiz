/* voidmaiz/http.hpp — fetching a URL into a file, on any platform: the network
 * seam, lifted out of the update client (2026-10-17).
 *
 * Void Hormiga asked for it (2026-09-22): it shelled out to `curl` for
 * publishing, map tiles and calendar feeds, and a phone has no `curl`. The seam
 * already existed inside update.hpp, because the updater met the same problem
 * first; this header gives it a name that is not "update". `maiz::update::Http`
 * and friends still resolve, so nothing that compiled stops compiling.
 *
 * One function type, one implementation per platform:
 *   - desktop: the system `curl` (Windows 10 1803+, macOS, Linux);
 *   - Android: java.net.HttpURLConnection through JNI;
 *   - iOS:     NSURLSession, in the iOS shell (there is no process to spawn on
 *              iOS: `system()` and `popen()` do not exist there).
 * A portable application never picks one: it asks its Device (app.hpp) for
 * `http(user_agent)` and gets the right one.
 *
 * Call it from a worker thread, never the frame loop: every implementation
 * blocks until the file is written or the request fails.
 *
 * Implemented in voidmaiz_update for now (link that target), because the
 * Android half shares the updater's JNI plumbing.
 */
#pragma once

#include <filesystem>
#include <functional>
#include <string>

struct ANativeActivity; // Android only

namespace maiz {

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string error;
};

/* GET `url`, following redirects, into the file `to`. */
using Http = std::function<HttpResult(const std::string& url, const std::filesystem::path& to)>;

/* The desktop default: the system `curl`, -L for redirects, a time limit so a
 * hung network is not a hung app. Empty on iOS, where nothing can be spawned. */
Http curl_http();

/* Android: java.net.HttpURLConnection through JNI. Call from a thread the
 * JavaVM can attach (a worker thread is right). Empty elsewhere.
 *
 * `user_agent`, when given, is sent as the User-Agent (2026-10-04). Some
 * servers refuse the platform's generic "Dalvik/…" one: OpenStreetMap's tile
 * policy asks every application to identify itself. */
Http android_http(ANativeActivity* activity, const std::string& user_agent = {});

/* iOS: NSURLSession (src/shell/shell_ios.mm). Empty off iOS. */
Http ios_http(const std::string& user_agent = {});

} // namespace maiz
