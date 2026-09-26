/*
 * voidmaiz/documents.hpp — the system's own document picker and save dialog:
 * how a file gets onto a phone, and how it gets off one.
 *
 * A desktop application opens a file dialog and gets a path back on the same
 * line. A phone has no file dialog an application may draw, and no path the
 * application may read: the person picks in the SYSTEM's picker (Android's photo
 * picker, the Storage Access Framework), and the application is handed a
 * content URI, later, after its activity was paused and resumed. So this is a
 * request and a result, never a call that returns a path:
 *
 *     maiz::android_pick_document(activity, 7, "image/*", photos_dir);
 *     ...every frame...
 *     maiz::DocumentResult r;
 *     while (maiz::android_take_document(activity, r)) { if (r.request == 7 && r.ok()) use(r.path); }
 *
 * WHAT ARRIVES IS A COPY. The Java side (MaizActivity) copies the picked bytes
 * into `dest_dir` on a background thread, under the file's own display name
 * (made safe, and made unique), and only then reports it. Native code only ever
 * sees an ordinary file it owns, which is why nothing else in the library has
 * to know what a content URI is.
 *
 * WHY THE PICKER AND NOT A PERMISSION. Android 13+ has a photo picker that
 * needs no permission at all: the person chooses which photos, and the choice
 * IS the consent. The older "allow access to all your photos" permission
 * (READ_MEDIA_IMAGES) is what the platform now tells applications not to ask
 * for when they only need what a person picks. Older Android gets the Storage
 * Access Framework's picker, which is the same idea.
 *
 * Saving is the mirror: `android_save_document` opens the system's "save to"
 * dialog (Downloads, a drive, another app's storage) and copies `src_path` to
 * where the person chose. That is how a phone gets a `.miga` off itself.
 *
 * ImGui-free, like safearea.hpp, because its Android half lives in the base
 * library next to the keyboard's. On every other platform these return false
 * and a host uses its own dialog.
 */
#pragma once

#include <string>

struct ANativeActivity;

namespace maiz {

struct DocumentResult {
    int request = 0;
    std::string status; // "ok" | "cancelled" | "error"
    std::string path;   // ok + pick: the local copy; ok + save: where it went (a URI)
    std::string name;   // the name the person saw (a pick's display name), or the error
    bool ok() const { return status == "ok"; }
};

/* Open the system picker for `mime` ("image/*", "*\/*"). The chosen file is
 * copied into `dest_dir` (created if missing). `request` is the host's own tag,
 * echoed in the result. Returns false when there is no activity to ask. */
bool android_pick_document(ANativeActivity* activity, int request, const std::string& mime,
                           const std::string& dest_dir);

/* Open the system "save to" dialog suggesting `name`, and copy `src_path` there. */
bool android_save_document(ANativeActivity* activity, int request, const std::string& src_path,
                           const std::string& name, const std::string& mime);

/* One finished request, oldest first; false when none is waiting. Call once a
 * frame (it is a cheap JNI call) until it returns false. */
bool android_take_document(ANativeActivity* activity, DocumentResult& out);

/* The file this activity was opened WITH ("Open with Void Hormiga" on a .miga
 * in a file manager or a chat), copied into `dest_dir` like a pick. Arrives as a
 * result with request -1. Call once at start and on every resume. */
void android_opened_document(ANativeActivity* activity, const std::string& dest_dir);

} // namespace maiz
