---
type: Concept
title: Updates
description: "An application updating itself, owned by Void Maiz since 2026-09-21 on the author's call: 'all applications should be able to update themselves.' The line: Void Mago owns what a release IS (void-updates.json, installers; build-time only, never on a user's machine), Void Maiz owns what updating LOOKS like and the in-app client (consent, check, prompt with behavior changes, download, verify, apply), the application owns who it is. Two rules, enforced: never check unasked, never install untold. Three ways to apply, read from the file: installer, archive beside the running copy, Android package handed to PackageInstaller. Built and tested on the desktop; the Android half compiled, not yet run on a device."
resource: include/voidmaiz/update.hpp
tags: [status:current, audience:dev, audience:host, confidence:measured]
timestamp: 2026-09-21T00:00:00Z
---

The author, 2026-09-21, after using the 0.2.0 APK:

> i also feel like updating the application, updates in general, should be
> handled and exist in maiz. Just as networking has been moved to maiz naturally,
> i also think updates (therefore mago) should interact with void maiz directly as
> well … this changes a lot, depending on what kind of application one is trying
> to make, and especially on mobile (no real 1 size fits all thing on mobile).
> however, there should still be abstract mechanisms for doing so … all
> applications should be able to update themselves.

# The line

It is the same line [networking](/concepts/networking.md) drew, for the same
reason: the part that goes wrong is the part every application writes for itself.

| | owner |
|---|---|
| what a release IS: the files that ship, the installer, the `release` block's notes and behavior changes, and **`void-updates.json`** (`mago feed`) | **Void Mago** |
| what updating LOOKS like and the in-app client: the consent question, the check, the prompt, the download, the digest check, handing the file to the platform | **Void Maiz** (`voidmaiz/update.hpp`, `voidmaiz/updateview.hpp`) |
| who the application is (`AppIdentity`: its feed key, version, feed URL, install folder), and anything about its own data a new version must be told | **the application** |

**Why Mago does not move into Maiz.** Mago is a build-time tool and says so in
its first paragraph: *"an updater that needs the libraries it is updating cannot
repair a broken install."* Nothing here needs Mago installed. What "Mago
interacts with Maiz directly" means is that the two meet at one document, and
Void Maiz is now the only reader of it that any Void application needs. Mago's
own shipping notes had already listed the reader's duties under "what the
application implements"; that column is what moved.

**Adapted from Void Hormiga's client** (`VoidHormiga/src/update/`, 2026-09-04),
which proved the design on a shipping application. What changed in the move: the
application's name came out (`AppIdentity`), the network became a seam (there is
no `curl` on a phone), archives became `.zip` as well as `.tar.gz` (the system
`tar` reads both), and Android got its own apply path.

# The two rules

1. **Never check without being asked to.** A check is a network request a person
   did not make. Preferences start `Unasked`, and the first launch asks *"Check
   for updates when … starts?"* before any request. *Never* is a real answer, and
   *Not now* asks again next launch.
2. **Never install without being told to.** `download` fetches and verifies and
   runs nothing. Installing is a separate button, and nothing replaces the running
   copy: side by side is what makes "try the update" reversible.

Preferences are **per machine, never per document**: not `config set`, because
Core's config rides the document, and "don't ask me" would travel to another
device on the next merge.

# Three ways to apply, read from the file

| the feed names | what happens | then |
|---|---|---|
| `.exe`, `.msi` | the installer runs, beside the running copy | the app quits |
| `.zip`, `.tar.gz` | unpacked **beside** the running copy into a folder named after the archive, and the new binary starts | the app quits; the old folder is untouched |
| `.apk` | the verified bytes go to Android's `PackageInstaller`, which shows the system's own confirmation | the app keeps running until Android replaces it |

It is decided by the file name, not `#ifdef`, so a new packaging on any platform
needs no new code. **Nothing unverifiable is ever offered**: an artifact with no
sha256 (Mago writes `null` when it had nothing to hash), a non-https URL, or a
filename that could escape its folder is treated as absent.

# What the prompt says

The release's own words: the summary, what it adds, and **what behaves
differently, and who that affects**. That is Mago's `behavior_changes`, the
category a version number cannot carry, and it is the difference between a
prompt people read and one they dismiss. `describe()` is one function, so a GUI
and a CLI say the same words.

# Found while building it

- **`tar` on Windows is not one program.** A machine with Git installed can have
  GNU tar earlier on `PATH`, and GNU tar reads `C:\...` as host `C`. The client
  names `%SystemRoot%\System32\tar.exe` (bsdtar, shipped since Windows 10 1803,
  which also reads `.zip`). Found by `update_smoke` on the machine it was written
  on, which is exactly such a machine.
- **`cmd.exe` strips the outer quotes** of a line that starts with a quote and
  holds more. Every shell command goes through one function that adds the
  documented extra pair.
- **The first consent dialog cut off the sentence that makes it fair** (*nothing
  is downloaded unless you choose it*): `TextDisabled` does not wrap. Seen in a
  screenshot, fixed.
- **Android requires the SAME signing key** for an in-place update. The APK is
  signed with a key minted once and kept out of the repository
  (`android/debug.keystore`, ignored); losing it means every phone must uninstall
  to update. It must be backed up.

# Android, stated plainly

The Android half is **compiled** (NDK r28, arm64, no warnings) and **not yet run
on a device**. Two JNI paths: `android_http` (`java.net.HttpURLConnection`,
called from the updater's worker thread) and `android_install_package`
(`PackageInstaller` sessions, which need `REQUEST_INSTALL_PACKAGES` and the
person allowing installs from the app once). There is no Java source, but these
are calls into the Java runtime, the same honest caveat Q29 records for the IME.

# For Void Mago

`mago feed` names Windows artifacts `-setup.exe` and everything else `.tar.gz`.
Interaction Combinators ships a portable `.zip` and an `.apk`, so its release
script runs `mago feed` and then fills in the artifact names and digests. Void
Mago was asked (2026-09-21) to let a manifest declare its artifact names, at which
point that step becomes one call.

# Built, and pinned

| piece | where | pinned by |
|---|---|---|
| feed parsing (void-updates/0.1, only this app, only this platform, only verifiable artifacts), version comparison, the decision, `describe`, per-machine preferences | `voidmaiz/update.hpp` (target `voidmaiz_update`) | `update_smoke` |
| `curl_http`, `download` with the digest checked before OK (a mismatch deletes the file), `unpack_beside` (flat or one-folder archives, `.zip` via the system tar, never over the running copy), `launch_detached`, `apply` | same | `update_smoke`, with a fake network and real archives |
| the `Updater` runner: check and download on a worker thread, polled once a frame | same | `update_smoke` |
| the consent question, the badge, the prompt, the settings section | `voidmaiz/updateview.hpp` (in `voidmaiz_view`) | screenshots, phone and desktop |
| Android HTTP and package install | `update.cpp` | NDK compile only |
| the probe: `maiz_update_smoke --probe <feed> <app> <platform> <version>` reads a real release's feed through the client | `tests/update_smoke.cpp` | run on Interaction Combinators 0.3.0's feed: offers the zip on Windows and the APK on Android to 0.2.0, nothing to 0.3.0 |

**First application: Interaction Combinators 0.3.0.** Updates only begin *from*
0.3.0: 0.2.0 has no client, so its users install 0.3.0 by hand once.
