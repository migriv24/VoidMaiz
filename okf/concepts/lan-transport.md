---
type: Concept
title: The LAN transport, and who owns it
description: "Where the seam is between Void Maiz, Void Palabra and the application, answered for the author's question 'i don't know the ownership of these things'. Maiz owns REACHING a device (sockets, discovery, multicast locks, the platform's radios); Palabra owns what crosses; the app owns who may join. And the answer for two Android phones with no router: a hotspot works today with no code at all, Wi-Fi Direct is a later transport behind the same seam, and Wi-Fi Aware is not worth its hardware gamble."
resource: include/voidmaiz/lan.hpp
tags: [status:current, audience:dev, audience:host, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

The author, 2026-09-22:

> it could also be usefull to look into wifi direct on android, to have 2 android
> devices connect to one another. i do apologize, i don't know the ownership of
> these things (whether this is a palabra thing, maiz thing, or an IC thing), but
> i feel like you can figure this out.

No apology needed: the question is the good one, and it has a clean answer that
has been implied by every piece built so far.

> **Direction, 2026-09-23 (the author, in a Void Hormiga session): all
> device-to-device networking moves onto Reticulum, with Void Palabra as its Void
> translation** (`../VoidPalabra/okf/concepts/reticulum.md`; a short-lived
> sibling, Void Snape, was archived the same day). Reticulum brings discovery,
> identity, encryption and every medium. Its C++ implementation
> (microReticulum) is host-driven, so **these sockets become Reticulum
> interfaces** rather than disappearing, and `LanSession`'s unencrypted peer
> traffic retires. What networking *looks like* (presence, the member list, the
> settings) stays Void Maiz's. Recorded here so the page below is read as
> history.
>
> **Built 2026-09-24: `RnsSession`** ([`include/voidmaiz/rnslink.hpp`](../../include/voidmaiz/rnslink.hpp),
> in `voidmaiz_net` when `MAIZ_RETICULUM` is on). It is
> `LanSession`'s job done over Reticulum, through Palabra's
> `voidpalabra_reticulum`, and it keeps `LanSession`'s shape (the same join flow,
> the same `LanEvent` and `LanFrame`, links named `rns:<peer id>`), so an
> application's frame loop does not change. `MAIZ_RETICULUM` is on by default,
> desktop and Android alike. Discovery is Reticulum announces
> carrying what a beacon carried. Joining is a link on which the joiner PROVES its
> identity, and a person still presses Allow. **What changed:** the bytes are
> sealed; "allowed" is kept by proven identity, not by a claimed id; a dead link
> is found by Reticulum's keepalives; and "beacon out, unicast back" moved into
> the UDP interface (`learn_peers`). **Interaction Combinators moved onto it
> the same day, Reticulum only** (the author's answer to Q37; IC 0.6.0), and
> two IC processes on one machine synced a net both ways through it. On
> Android it compiles and links into IC's APK, but it has never started on a
> device. `LanSession` stays in the library for any application that has not
> moved.

# The three owners

**Void Maiz owns reaching a device.** Sockets, broadcast and multicast,
addresses, interfaces, the Android multicast lock, join codes, and — when they
come — Wi-Fi Direct and any other radio. This is `voidmaiz_lan`
(`include/voidmaiz/lan.hpp`), and it is the right owner for one reason: none of
it knows what a document is. It moves bytes between two machines on one network.
Every GUI application in this family will want that, which is the same argument
that moved networking and then updates here.

**Void Palabra owns what crosses.** Frames, the merge, replica identity,
sessions, what a duplicate name means. Given a byte pipe, Palabra makes two
documents into one document. It never opens a socket, and it must not: a CRDT
that knows about Wi-Fi is a CRDT that cannot be tested without a network.

**The application owns who may join.** The Allow dialog, the net's name, the
profile, whether a joiner sees everything. `LanSession`
(`include/voidmaiz/lanlink.hpp`) is Maiz's default answer — a handshake, a
person's Allow, an idle timeout — and Interaction Combinators uses it as-is. An
application with different rules replaces that layer, not the two below it.

So: a new radio is a Void Maiz change. A new frame is a Palabra change. A new
question asked of a person is the app's.

# Two Android phones and no router

The practical problem: two phones on a table, no Wi-Fi network. Three ways.

**1. A hotspot. Works today, no code.** One phone turns on its hotspot and the
other joins it; the phones are then on an ordinary subnet (Android hands out
192.168.43.x), and everything already built — broadcast discovery, join codes,
TCP sessions — works unchanged, because nothing in it assumed a router rather
than a phone. This is the supported answer for the next test, and it should be
written in the app's LAN panel rather than left for someone to discover.

Its costs are real but small: the hotspot phone usually loses its own internet
(fine — the app needs none), and the person must leave the app to switch it on.

**2. Wi-Fi Direct (`WifiP2pManager`). A real transport, a real bill.** Two
devices negotiate a group with no access point; the group owner runs DHCP at
192.168.49.1 and, from that moment, our sockets work exactly as they do now.
The attraction is that it is automatic — discovery and connection inside the
app, no settings screen.

The bill is that it is a Java API with no C counterpart. Our APK is a
NativeActivity with no Java of its own, deliberately, and Wi-Fi Direct needs
more than a JNI call or two: it is a `BroadcastReceiver` (peers found, state
changed, connection info available) delivered to a Java class, plus
`NEARBY_WIFI_DEVICES` on Android 13+ and fine location below that — a
permission people rightly hesitate over, for a feature whose purpose is not
location. On top of that, service discovery over P2P is its own protocol
(`WifiP2pDnsSdServiceInfo`), so our beacon would need a second implementation
for this transport alone.

So: worth building, later, as a second `lan::` transport behind the same
interface that UDP discovery already hides — and only once a device is in hand
to test it, because every reported Wi-Fi Direct bug is a device-specific one.

**3. Wi-Fi Aware (NAN).** Cleaner than Direct, and the right shape for this app.
It is also hardware-dependent (`isWifiAwareAvailable()` is false on a great many
phones, including ones sold this year), which makes it a feature that works on
the author's phone or does not, with no way to tell before writing it. Not now.

# What this means for the next release

The LAN panel says which of these it is using, and how to get the other one: a
line under the join code when the device is on a hotspot's own subnet, and a
sentence in the panel when no interface is found at all. No code beyond that,
and no Wi-Fi Direct until there are two Android devices to test it on.

Related: [collaborative canvas](/concepts/collaborative-canvas.md) for what
crosses the link, [updates](/concepts/updates.md) for the other line that Mago
and Maiz share, and the author's
[testing guide](/testing/collaborative-canvas-user-tests.md) §L for the LAN
tests themselves.
