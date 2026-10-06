---
type: Concept
title: Radios — Bluetooth LE and Wi-Fi Direct, and Reticulum over them
description: "The radio holiday (voidmaiz/radio.hpp) and the bridge to Reticulum (voidmaiz/rnsradio.hpp), built 2026-10-05 for Void Hormiga's phones on its author's ask: share with no Wi-Fi in common, never a hotspot. Bytes between nearby devices, nothing more; a connected LE peer becomes a Palabra pipe, a Wi-Fi Direct group a UDP interface; the application chooses the tag and whom to connect. The Android half, the desktop stand-in, what was measured and what needs two phones."
resource: include/voidmaiz/radio.hpp
tags: [status:current, audience:dev, audience:host, confidence:measured]
timestamp: 2026-10-05T00:00:00Z
---

Built 2026-10-05, by a Void Hormiga session under the author's grant. It
supersedes [the LAN transport](/concepts/lan-transport.md)'s lean on two phones
with no router ("a hotspot works today"): Void Hormiga's author ruled the hotspot
out ("a big waste of data plans and money ... not everyone can easily do
hotspot") and asked for **Wi-Fi Direct** and **Bluetooth** ("substantially
slower ... however, it would be compatible with an iOS device and android device
in the future"), with Reticulum on top.

The ownership line of [lan-transport](/concepts/lan-transport.md) holds exactly:
**Void Maiz reaches a device**, Palabra owns what crosses, the application owns
who may join. So this is two headers and no policy.

# The radio holiday (`voidmaiz/radio.hpp`)

Bytes between nearby devices. It knows no Reticulum and no application.

- **Bluetooth LE, not classic.** An iPhone may speak LE GATT to anything; a
  classic RFCOMM socket needs Apple's accessory programme. So LE, slower as it
  is, is the one radio a mixed group of phones can share later.
- **Each device both advertises and scans.** It advertises our service with a
  short **tag** the application chooses (in the scan response's service data,
  at most 10 bytes), and reports every device advertising the service, with its
  tag and signal. The application filters; the holiday never decides who is
  "ours".
- **A connection is one byte stream each way**: a GATT server on every device
  (RX written by the other side, TX notified to it), the central negotiating MTU
  517, chunks to the MTU, **one write in flight** per direction so nothing is
  dropped. Either side may have dialled; both see `connected`.
- **Wi-Fi Direct**: the same service and tag by DNS-SD; `connect` forms a group
  (the system may ask the other person); the holiday reports the group
  (`owner <ip>` or `client <owner ip>`) and carries no bytes for it: a group is
  an IP network.
- **Permissions are the system's.** Android 12+ asks once for "Nearby devices"
  (`BLUETOOTH_SCAN` / `CONNECT` / `ADVERTISE`; `NEARBY_WIFI_DEVICES` from 13),
  older Android for location. `request()` is the only call that can show a
  prompt, and like the location holiday a host calls it from a tap.
- **One per device**, installed by the shell, polled once a frame. The Android
  half is `MaizRadio.java`, reached through `MaizActivity.maizRadio*`; a method
  an older APK's activity lacks reads as Unavailable, never a crash.
- **`loopback_radio`**, the desktop stand-in: "LE" as a local TCP stream,
  throttled to a chosen speed, with the same hello (name and tag). Two processes
  on one computer exercise everything above the radio this way.

# The bridge (`voidmaiz/rnsradio.hpp`, in `voidmaiz_net` with Reticulum)

Written once so no application learns what HDLC is:

- each connected LE peer becomes a Palabra **pipe** named `ble:<peer>`: the
  node's packets HDLC-framed onto the stream, arriving bytes unframed into
  packets. The pipe declares LE's real speed (48 kbit/s by default) so
  Reticulum's timeouts wait as long as LE needs;
- a Wi-Fi Direct group becomes a **UDP interface** `p2p` (port 4243): a client
  sends to the owner, the owner to the group's broadcast, and both learn whoever
  speaks (`learn_peers`). It goes when the group dissolves;
- `waiting(peer)`: bytes framed and not yet on the air, for a progress bar.

Sans-thread: `handle()` the radio's events and `pump()` after every
`Node::loop()`, from the application's one thread.

# Measured

- `maiz_radio_smoke`: HDLC survives every split of the stream and the two
  special bytes; two processes, one Reticulum each, **no UDP at all**, over the
  loopback radio at 8 KB/s: the tag arrives, an announce is heard over the pipe,
  a link opens, a 30 KB message goes as a Resource (its transfer seen) and is
  echoed back.
- Void Hormiga, on top of this: two members synced over the loopback radio at
  6 KB/s with no LAN at all, and its phone harness showed the transfer bar and
  the member's signal. Its record is its own `okf/concepts/platform/radios.md`.
- `MaizRadio.java` compiles to dex and is in Hormiga's APK. **No device has run
  it.** Wi-Fi Direct has no stand-in; it waits for two Android phones.

# Boundaries

- **The holiday decides nothing about who.** Tags in, tags out.
- **Bytes, not messages.** LE delivers any split of the stream; framing is the
  bridge's (HDLC), meaning is the application's.
- **No hotspot path** is built or recommended for this purpose.
- **Nothing is asked until a person taps.**
