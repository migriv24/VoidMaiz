---
type: Concept
title: The field network
description: "Many phones, Apple and Android, in one room with no internet, kept in sync: a carried kit (Wi-Fi access points with no uplink, a 'brain' that holds the room's always-on copy, batteries) that the people bring with them. Why a kit beats Bluetooth and phone-to-phone Wi-Fi for 40+ mixed devices, the star topology that makes 40 phones 40 links instead of 780, how every device finds the brain without the broadcast iOS forbids, scaling by adding units, Reticulum's place, and what it means for Void Buzz. The hardware to buy and how to set it up are in /hardware/."
tags: [status:draft, audience:all, confidence:asserted]
timestamp: 2026-10-17T00:00:00Z
---

# The field network

**The author, 2026-10-17**, on a Void Hormiga client: *"40+ devices in a single
room to be updated and communicating with each other. These are a mix of apple
and android devices. Using Bluetooth would be slow, and so having a mini router
would be great. They're usually off grid and on the move."* And on the device:
*"all the functions of a wifi router without internet"*, mobile, its own device,
able to scale by adding units, fitting in a backpack, and compatible with Void
Buzz. Not 300+ devices yet, but designed so that would not need a different
architecture.

This page is the design. **[Hardware](/hardware/index.md)** is the buying and
setup guide, by size, for anyone running a Void Maiz application this way.

> **Confidence.** Researched, not built or measured. No kit has been bought.
> Every number below is a vendor claim, a published planning figure or
> arithmetic, and says which. The first kit assembled is the test; this page
> and the hardware pages get its measurements.

## Why a kit, and not the phones alone

What exists between an iPhone and an Android phone with no infrastructure was
settled by the research, and it is short:

| link | iPhone ↔ Android | for 40 phones in a room |
|---|---|---|
| Bluetooth LE | **works** (both are GATT centrals and peripherals) | **too slow**: Ditto's own transport table plans about **20 KB per second**; an ESP32 holds at most 9 connections; and a backgrounded iPhone's advertisements are unreadable by Android (Apple moves them to an undocumented "overflow" area) |
| each platform's peer-to-peer Wi-Fi | **does not cross**: Apple's AWDL only finds Apple, Android's Wi-Fi Aware only Android (Ditto's documentation, stated plainly) | no |
| Wi-Fi Aware on iOS 26+ | the first standard on both; **unconfirmed** between them (developers report an iPhone that could not find an Android service; another needed iOS 27), and pairing goes through Apple's AccessorySetupKit or DeviceDiscoveryUI | not to build on yet |
| a phone's hotspot | works | **ruled out** by the author (2026-10-05: data plans, and not everyone can) |
| **a shared Wi-Fi network** | **works**, tens of Mbps per phone | **yes** |

So Ditto, a company whose product is offline sync between phones, says it
outright: **without a LAN, Bluetooth LE is the only common link, and it has
limited bandwidth.** The kit is the LAN, carried in.

It is not the phone hotspot the author ruled out. A hotspot spends a person's
data plan and needs someone able to run one; the kit has **no uplink at all**,
belongs to the group, and does nothing else.

## The kit, as roles

A kit is four roles. Small kits put several roles in one box; large kits give
each its own. The roles are the design; the boxes are the [hardware](/hardware/index.md).

1. **Access point (AP).** Wi-Fi with no internet: one network name, DHCP,
   local DNS, no client isolation (members must reach each other). Add more APs
   to serve more devices or more space; each unit joins the first by cable or by
   wireless mesh.
2. **Brain.** One always-on computer on the kit's network that holds **the
   room's copy of the document** (a headless Void Maiz application acting as the
   sync host: [headless](/concepts/headless.md)), runs Reticulum as a transport
   node, answers discovery, and keeps what a member missed while asleep. Phones
   come and go; the brain does not. **The brain is a role, not a box**: on a
   small kit it runs on the AP itself, on a large one on a small Linux computer
   wired to it.
3. **Power.** USB-C Power Delivery batteries, sized for a day, swappable
   without turning anything off.
4. **Radio bridge (optional).** A microcontroller on the brain's USB port that
   adds what Wi-Fi cannot: Bluetooth LE for a phone outside the room's Wi-Fi,
   LoRa between kits that have moved apart, and the low-power links Void Buzz's
   devices use.

## A star, not a mesh of phones

Forty phones each syncing with each other is 40 × 39 / 2 = **780 links**.
Forty phones each syncing with the brain is **40**. Every phone already speaks
to a host in today's sessions ([lan transport](/concepts/lan-transport.md):
`LanSession` and `RnsSession` are host and joiners), and Void Hormiga's phone is
already "a member, never the host". **The brain is the host they were missing**:
always on, never backgrounded, never out of battery before the room is.

What the star does to each kind of traffic:

- **Commits** (dispatched commands, through Palabra's sync) go phone → brain →
  everyone. A phone that slept catches up from the brain, not from whichever
  peer happens to be awake.
- **Presence** (cursors, selections, what someone is looking at;
  [collaborative canvas](/concepts/collaborative-canvas.md)) is the one that
  multiplies: 40 phones at one update a second, fanned out to 39 others, is
  about 1,560 small messages a second through the brain. Fine on Wi-Fi (on the
  order of 150 KB/s at ~100 bytes each, arithmetic), but **the brain should
  coalesce**: one presence batch per tick per recipient, not one message per
  sender. That is the first piece of software the kit asks of the library.
- **Files** (photos, by hash; Palabra's) go to the brain once and are fetched
  from it, instead of travelling phone to phone.

Phone-to-phone direct links stay possible on the same Wi-Fi; nothing forbids
them. They stop being necessary.

## Finding the brain, on iOS too

iOS refuses UDP broadcast and multicast to an app without an entitlement Apple
grants on request ([ios](/concepts/ios.md)). On the kit nothing needs it,
because the brain can be found three ways, tried in order:

1. **Bonjour** (`_voidmaiz._tcp`, advertised by the brain). iOS allows Bonjour
   for the service types an app declares in `NSBonjourServices`, behind the
   local-network permission prompt. Android has NSD.
2. **A name**: the AP's DNS answers `brain.kit.lan` (or whatever the kit is
   called).
3. **An address**: the brain has a fixed DHCP lease; the address is printed on
   the kit's card.

After that everything is unicast TCP or UDP to the brain, which is ordinary on
both platforms. This is **Q38, answered by the kit**: discovery by Bonjour and a
known address, the beacon kept for desktop and Android rooms without a kit.

## Two platform traps the research found

- **Android sends app traffic over mobile data when the Wi-Fi has no
  internet**, if mobile data is on. Google's own guidance: bind the process (or
  the socket) to the Wi-Fi network (`bindProcessToNetwork`,
  `Network.bindSocket`). Off-grid there is usually no mobile data, but a kit must
  not depend on that, so **the Android side of `voidmaiz_lan` must bind its
  sockets to the Wi-Fi network** when it is on a kit. Not built.
- **Both platforms probe for internet and mark a network without it.** iOS
  fetches `captive.apple.com/hotspot-detect.html` and expects "Success"; Android
  fetches `connectivitycheck.gstatic.com/generate_204` and expects a 204. A kit
  whose AP answers those two locally is treated as an ordinary network: no
  "no internet" nagging, no auto-leaving. The cost is that a phone then believes
  it has internet and an app that needs the real internet waits for a timeout
  instead of failing fast. **Lean (Q42): answer the probes on a kit with no
  uplink, never on a kit that has one.**

## Reticulum on the kit

How iOS Reticulum apps already work is the pattern to copy. The two iOS clients
found (ReticulumMessenger in Swift, thatSFguy's reticulum-mobile-app) connect by
**TCP to an `rnsd`** or by Bluetooth to an RNode; neither relies on the
multicast AutoInterface, and the official Sideband for iOS is still a TestFlight
beta, held back, in its author's words, because Apple allows no extended
background services. So on a kit:

- the brain runs `rnsd` (Reticulum's daemon; its manual has an OpenWrt install)
  with a **TCP server interface** and **transport enabled**;
- members connect to it by TCP at the brain's address: no multicast needed;
- two kits that have moved apart reach each other brain to brain over whatever
  links them: a cable, a wireless mesh, or LoRa through the radio bridge (an
  RNode-compatible board).

`RnsSession` already runs the family's sync over Reticulum, so this is
configuration, not a new protocol.

## Scaling by adding units

- **More devices in one room:** add an AP on the same network name, a different
  channel, joined to the first by cable (preferred: no airtime lost) or by
  wireless mesh. The published planning figure is **25 to 30 active clients per
  radio**, about 50 per dual-band AP (NetSpot's event guide; Meraki calls more
  than 30 clients on one AP "high density"). Use narrow channels (20 MHz) when
  dense. Every AP leads to the same brain.
- **More rooms, or a group that splits:** each group carries a kit with its own
  brain. Brains are Reticulum peers, and the documents merge when the brains can
  reach each other, the same lattice merge every device already does
  ([networking](/concepts/networking.md)). Nothing central.
- **300+ devices:** the architecture does not change (star per kit, kits linked
  brain to brain). The hardware does: ceiling-class access points on a power
  station, not backpacks. Out of scope for now; nothing here forecloses it.

## Void Buzz

The kit is **network substrate**, not one of Void Buzz's entities, and it was
designed against their bundle so that it stays that way:

- **It is not a Tree.** A Tree is stationary with a position everyone trusts;
  the kit moves with its people.
- **Flowers** are ESP32-class devices. ESP32 radios (except the newest ESP32-C5)
  are 2.4 GHz only, so **a kit keeps a 2.4 GHz network on** even when phones use
  5 GHz. Flowers too sleepy for Wi-Fi reach the brain through the radio bridge
  (ESP-NOW or Bluetooth LE).
- **A Hive** is a member like any other on the kit's network. Their Hive is
  mains-powered by definition; whether one can run on a kit's brain is Void
  Buzz's call, and nothing here assumes it.
- **Buzzes** (drones) use the kit's Wi-Fi in the room and LoRa beyond it, the
  same radio bridge two kits use.

## What the library must build for this

In order:

1. **A discovery seam** with Bonjour and a known address (Q38), so iOS can find
   a brain at all.
2. **Android socket binding** to the Wi-Fi network.
3. **The brain as a host**: a headless application in "room host" mode
   (always-on Session, the sync host of `LanSession` / `RnsSession`, presence
   coalescing, catching sleepers up). Lean (**Q41**): the library provides the
   room-host loop and the application provides its `HostApp`, since a brain must
   register the application's glyphs.
4. **A build of that host for the brain's hardware** (Linux, arm64; for a brain
   on the AP, OpenWrt's musl toolchain).
5. The radio bridge's firmware: later, and shared with Void Buzz.

## Sources

- Ditto, *Transports Overview* and network deployment pages: BLE about 20 KB/s,
  AWDL and Wi-Fi Aware each platform-only, LAN by mDNS/DNS-SD.
- Apple, CoreBluetooth programming guide (peripheral advertising, background
  execution, state restoration) and developer forum threads on the overflow area.
- Apple, Wi-Fi Aware and DeviceDiscoveryUI documentation; forum thread 837110.
- Espressif, ESP-IDF multi-connection guide (NimBLE: 9) and `ESP_WIFI_MAX_CONN_NUM`
  (soft-AP: 15).
- Android Developers blog, *Connecting your app to a Wi-Fi device* (2016).
- Purple, *Captive portal detection*: the iOS and Android probes.
- NetSpot, *Wi-Fi deployment for high-density events*; Cisco Meraki, *High
  Density Wi-Fi Deployments*.
- Reticulum manual (OpenWrt, `rnsd`, transport); GitHub: ReticulumMessenger,
  thatSFguy/reticulum-mobile-app, Reticulum discussion #307 (iOS);
  attermann/microReticulum (C++, ESP32-class, work in progress).
