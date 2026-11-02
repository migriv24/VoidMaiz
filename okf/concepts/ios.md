---
type: Concept
title: iOS
description: "Void Maiz on iPhone and iPad, begun on a Windows machine: how it is developed with no Mac (a desktop preview daily, a GitHub macOS runner as the Mac), what iOS changes about each holiday and rule (Metal, no Back button, no quitting, no self-update, no processes, a networking wall around broadcast), and the App Store path, as the work list it is."
tags: [status:draft, audience:all, confidence:asserted]
timestamp: 2026-10-17T00:00:00Z
---

# iOS

**The author, 2026-10-17:** *"a new thing to begin creating: void maiz for iOS
devices. We're going to have to start with abstract (since we're on a windows
machine), but if possible to actually begin development on this device … the
Play store is a bit easier to get into, but the app store is going to take some
work, and i believe that work starts here."*

It starts with a decision that is not about iOS at all: **the library owns the
platform shell now, and a host writes one portable `maiz::App`**
([portable apps](/concepts/portable-apps.md)). Without that, "Void Maiz for iOS"
would mean every host writing a second shell in a second language. With it, iOS
is one more shell in this repository: [`src/shell/shell_ios.mm`](/../src/shell/shell_ios.mm).

> **Confidence.** Everything iOS-specific on this page was written on Windows.
> The shell is compiled and launched only by CI (below). Until that job has run
> green, `shell_ios.mm` is reasoned, not witnessed; this page's platform facts
> are asserted from Apple's documentation and the sources listed at the end.
>
> **First run, 2026-10-30: red at the device build.** clang compiled
> `shell_ios.mm` with manual reference counting. It rejected the `__weak`
> references, and the weak ivar `_exportPicker` became "unavailable": six errors,
> all one cause. ARC had been asked for with
> `$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>`, which the Xcode generator did not
> pass to the static libraries. Since 2026-11-02 it is set on each `.mm` file
> (`COMPILE_OPTIONS -fobjc-arc`) and as `XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC`
> on `voidmaiz_app` and `voidmaiz_imgui`. The Simulator steps have not run yet.

## Developing iOS with no Mac

Three loops, from cheapest to most real.

1. **Every day, on Windows: the preview.** `my_app --device iphone-16` runs the
   portable app as that phone: its screen in points, its safe area, the Dynamic
   Island and home indicator drawn over it, **its rules** (no Quit, the edge
   swipe goes Back), the mouse as a finger. `--screenshot` and `--input` make it
   scriptable for an agent. This is where layout and behaviour get right, and it
   costs nothing. It is not an emulator: the GPU and the keyboard are this
   machine's.
2. **Every push: a Mac in CI.** The `ios` job in
   [`.github/workflows/ci.yml`](/../.github/workflows/ci.yml) runs on a GitHub
   macOS runner (free for a public repository). It builds the library, the shell
   and the example for an iPhone and for the iOS Simulator; boots the newest
   iPhone simulator the image has; installs and launches the app; checks it is
   still running 20 s later; sends it to the background and back and checks that
   its state document was written; and uploads screenshots. **That is the
   answer to "begin development on this device through some sort of virtual
   creation"**: the Mac is virtual, rented by the minute, and legitimate.
3. **A real iPhone.** Needs the Apple Developer Program (a yearly fee), a
   signing certificate and provisioning profile, and a way to sign: a Mac, a
   cloud Mac, or CI holding the certificate and an App Store Connect API key,
   which can also upload to TestFlight. TestFlight is how the author would hold
   the first build in a hand.

**Considered and not recommended:** running macOS in a virtual machine on this
PC (Apple's license permits macOS only on Apple hardware), and cross-compiling
iOS from Windows or Linux with unofficial toolchains (they exist, still need
Xcode's SDK, and would make the family's iOS build depend on a project
outside it).

## What iOS changes

Ordered by how much a host has to care.

### Rules (the Device states them; see portable apps)

- **No Back button.** Back is a swipe in from the left edge, and a back chevron
  in the app bar is mandatory, because the swipe is invisible. The touch gate
  now does the swipe (`TouchGate::edge_back_dp`, on when `Device::edge_back()`),
  and `back_pressed()` hears it like Android's key. A chevron at the very edge
  still taps: a finger in the edge strip is an ordinary touch until it moves
  right.
- **An app never quits itself.** `can_quit()` is false; the example draws no
  Quit button there.
- **An app never updates itself.** `self_updates()` is false and
  `update::platform_tag()` says `ios-arm64` (it said `macos-arm64`, since both
  are Apple on arm64; caught before any build existed, and an iPhone would have
  been offered a Mac release). The [update client](/concepts/updates.md) must
  show "a new version is in the App Store" or nothing. A related store rule
  (App Review Guideline 2.5.2): an app may not download code that changes its
  features. **An Allomone rule a person writes in their own document is their
  content**, like a spreadsheet formula; a vendor shipping new behaviour as
  downloaded scripts would not be. Keep that line where it is.
- **Background may be the last word, and the GPU is off-limits there.** The
  shell pauses drawing on `sceneDidEnterBackground` (iOS ends apps that use the
  GPU in the background) and sends `AppEvent::Background`, where the app saves.
  `Terminating` is rarely sent. **Sockets are suspended** within seconds of
  backgrounding; the LAN and Reticulum sessions' keepalive and reconnect
  ([lan transport](/concepts/lan-transport.md)), built for Android's sleeping
  phones, are what survive this.
- **No processes.** `system()` and `popen()` do not exist in the iOS SDK. The
  update client's shell-outs are compiled out there, and **any host code that
  runs `curl` must move to `Device::http`**, which on iOS is NSURLSession.

### Holidays (each one's iOS half)

| holiday | Android | iOS (in `shell_ios.mm`) | status |
|---|---|---|---|
| drawing | EGL + GLES 3 | MTKView + ImGui's Metal backend | written |
| keyboard ([text input](/concepts/text-input.md)) | InputConnection | **stage 1**: a UIKeyInput view under the same pivot; accents (long-press) and emoji work because they arrive as text | written |
| — stage 2 | | UITextInput: marked text (Chinese, Japanese, Korean composition), autocorrect with context, the system's cursor and selection handles, which is also Void Hormiga's "copy a phone number out of a note" | **not built** |
| safe area | MaizActivity insets | `safeAreaInsets`, every frame | written |
| photos | photo picker (no permission) | PHPicker (no permission) | written |
| files, save a copy, "Open in" | Storage Access Framework | UIDocumentPicker; scene URL contexts | written |
| share | text only (a file needs a FileProvider) | the share sheet, files included (anchored as a popover on iPad, where an unanchored one crashes) | written |
| open a URL, `tel:`, `mailto:` | an intent | `openURL` | written |
| haptics | `performHapticFeedback` | UIFeedbackGenerator | written |
| http | HttpURLConnection | NSURLSession (HTTPS only, which App Transport Security wants anyway) | written |
| location | LocationManager | CoreLocation; "approximate" is iOS's reduced accuracy | written |
| radios ([radios](/concepts/radios.md)) | LE + Wi-Fi Direct | **not built** (see networking) | — |
| LAN discovery | UDP beacon, multicast lock | **blocked as built** (see networking) | — |

### Networking: the one wall

This is the largest real difference, and it touches the family's whole
local-first story, so it is set out plainly.

- **A permission, asked once.** Any app that talks on the local network gets the
  system's "find devices on your local network" prompt and needs a purpose
  string (`NSLocalNetworkUsageDescription`).
- **Broadcast and multicast need an entitlement Apple must grant**
  (`com.apple.developer.networking.multicast`, requested from Apple with a
  justification). Unicast does not. `LanSession` discovers hosts by **UDP
  broadcast beacon**, and Reticulum's automatic interface uses **IPv6
  multicast**. As built, neither discovers anything on an iPhone.
- **Bonjour (DNS-SD) is the way around it**, declared in `NSBonjourServices`,
  and needs no special entitlement for the service types an app declares. The
  lean (**Q38**) is a discovery seam with two implementations: the beacon on
  desktop and Android, Bonjour on iOS (and possibly everywhere, since Android has
  NSD and Windows 10+ has DNS-SD). Everything after discovery is unicast and
  works unchanged. Requesting the entitlement is the fallback, not the plan.
- **Radios between an iPhone and an Android phone.** Bluetooth LE works on both
  (CoreBluetooth is a GATT central and peripheral), so `radio.hpp`'s LE half has
  an iOS port to write. **Wi-Fi Direct does not exist on iOS.** Apple's own
  peer-to-peer (MultipeerConnectivity) does not talk to Android. iOS 26 added
  **Wi-Fi Aware**, the standard Android has had for years, which is the first
  cross-platform peer-to-peer Wi-Fi; developers had not yet confirmed it
  interoperating with Android when this was written. For Void Hormiga's phones:
  **LE is the radio that reaches both today; Wi-Fi Aware is the one to watch.**
- **Encryption and the store.** Reticulum and libsodium are cryptography. An app
  that carries them answers Apple's export-compliance questions differently from
  one that only uses HTTPS (`ITSAppUsesNonExemptEncryption` is `false` in the
  template today because nothing on iOS links them yet). Revisit when they do.

So the order for networking on iOS: Bonjour discovery behind a seam, then the
existing sessions over unicast, then CoreBluetooth for LE, then Wi-Fi Aware when
someone can test it against an Android phone.

**For many phones at once, the answer is a carried network** (2026-10-17):
[the field network](/concepts/field-network.md). Research confirmed what the
table above implies: between iPhone and Android the only fast link is a shared
Wi-Fi, so a group off the grid carries one (travel routers with no uplink, an
always-on brain, batteries), and every iPhone finds the brain by Bonjour or a
known address, with no broadcast needed. The buying and setup guide is
[hardware](/hardware/index.md) (now in Void Hammer). The existing iOS Reticulum apps connect the same
way (TCP to an `rnsd`); the official Sideband for iOS is still a TestFlight
beta, held back by iOS's limits on background work.

## The App Store: the work list

What the library already does, and what remains, for the first submission of a
Void Maiz app (Void Hormiga, presumably):

**Done here, for every app:**
- the UIScene life cycle (Apple's TN3187: required when building with the SDK
  after iOS 26; an app that does not adopt it will not launch);
- an `Info.plist` with the name, version, orientations, launch screen, an honest
  default location purpose string (the shell links CoreLocation, and an upload
  without the string is refused even if the app never asks), and the
  encryption answer; a host adds its own keys with `IOS_PLIST_EXTRA`;
- a **privacy manifest** (`PrivacyInfo.xcprivacy`, required since 2024 for apps
  using "required reason" APIs): no tracking, no collection, file timestamps
  only inside the container, elapsed-time measurement;
- no shelling out, no self-update, no quitting, no GPU in the background.

**Left for the host and the author:**
- the **Apple Developer Program** account, a bundle id registered to it, the
  signing certificate and profiles;
- an **app icon** set and screenshots (the template has none);
- **App Store Connect privacy answers.** Local-first helps: data that never
  leaves the device is not "collected" in Apple's definition. Data that syncs to
  other members' devices over the LAN needs an honest reading of that
  definition before the form is filled in;
- **document types** for the app's own files (Hormiga's `.miga`): declare the
  type and "Open in" works through the shell's scene handler;
- the **networking** above, if the app syncs;
- **review notes**: an app whose features need a second device on the local
  network should tell the reviewer how to see them.

## Sources

- Apple, *TN3187: Migrating to the UIKit scene-based life cycle*
  (developer.apple.com/documentation/technotes/tn3187-migrating-to-the-uikit-scene-based-life-cycle).
- Apple, `com.apple.developer.networking.multicast` entitlement reference, and
  the Local Network Privacy FAQ on the developer forums (thread 663875).
- MacRumors (2025-06-21) and heise (2025) on Wi-Fi Aware in iOS 26; Apple
  developer forum threads 790195 and 845528 on Android interoperability not yet
  confirmed.
- Apple, App Review Guidelines 2.5.2; privacy manifest and required-reason API
  documentation.
- `confidence:asserted` throughout: read, not yet exercised by this project.
