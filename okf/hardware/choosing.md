---
type: Guide
title: Choosing a kit
description: "Which kit for how many devices (pocket, small, room, hall, venue), what each role needs by specification, dated example products, and battery arithmetic. Count every Wi-Fi device, not every person."
tags: [status:draft, audience:all, confidence:asserted]
timestamp: 2026-10-17T00:00:00Z
---

# Choosing a kit

## 1. Count devices, not people

Count **everything that will join the Wi-Fi**: phones, tablets, laptops, and any
ESP32-class devices (Void Buzz's). Add a margin of a quarter for people who
bring two devices. That number picks the tier.

## 2. The tiers

| tier | devices | access points | brain | power for ~8 h | carried as |
|---|---|---|---|---|---|
| **Pocket** | 1–6 | 1 small travel router | on a member's laptop, or the router | 1 bank | a jacket pocket |
| **Small** | 7–25 | 1 dual-band travel router | on the router (≥1 GB RAM) or a single-board computer | 2 banks | half a backpack |
| **Room** | 26–60 | 2 travel routers, cabled | a single-board computer | 3 banks | one backpack |
| **Hall** | 61–150 | 3–4 travel routers, cabled through a small switch | a single-board computer or mini PC | 5–7 banks | two backpacks (two Room kits joined) |
| **Venue** | 150–300 | ceiling-class access points on a PoE switch | a mini PC | a portable power station | a case, not a backpack: **design only, not covered here** |

**The arithmetic behind "access points":** published high-density design
guidance plans **25–30 active clients per radio**, about **50 per dual-band
access point** (NetSpot's event guide; Cisco Meraki treats more than 30 on one
AP as high density). Vendors quote much higher (GL.iNet: 70+ for the Beryl AX,
120+ for the Slate 7), but those are how many can *associate*, not how many
work well at once. Plan on 30 per unit, and the Room tier's two units give
40–60 devices headroom.

**The Void Hormiga client** (40+ mixed Apple and Android devices in one room) is
the **Room** tier.

**Under 6 devices, Bluetooth is an option with no hardware**
([radios](/concepts/radios.md)): it works between iPhone and Android, at about
20 KB/s. Fine for text and records, painful for photos. Any group that shares
pictures wants the Pocket kit.

## 3. What to buy, by role

Buy by **specification**. The examples are what fit in October 2026; check
current models and prices, and prefer whatever meets the spec.

### Access point (travel router)

**Must:**
- **OpenWrt-based** (or flashable to OpenWrt). The kit needs settings consumer
  firmware hides: client isolation **off**, fixed DHCP leases, local DNS names,
  answering the phones' internet probes, and installing packages.
- **Wi-Fi 6 or newer, dual-band** (2.4 GHz kept on for ESP32-class devices,
  5 GHz for phones).
- **Access-point mode with a wired uplink**, so a second unit can extend the
  first by cable.
- **USB-C power** (Power Delivery), and a known maximum draw.
- **Two or more Ethernet ports.**

**If the brain runs on it** (Pocket and Small tiers): 1 GB RAM or more and a
64-bit (arm64) processor.

**Avoid:** 2.4 GHz-only units; "mobile hotspot" units that need a SIM; an ESP32
as the access point for phones (its soft AP takes 15 stations at most, and
phones need more airtime than it has).

**Examples** (vendor figures, 2026-10):
- **GL.iNet Beryl AX (GL-MT3000)**: Wi-Fi 6, OpenWrt-based, 256 MB RAM, USB-C
  5 V/3 A, under 8 W (vendor), 1 WAN + 1 LAN port, about 200 g. A good second
  or Pocket unit; too little RAM to be the brain comfortably.
- **GL.iNet Slate 7 (GL-BE3600)**: Wi-Fi 7, 1 GB RAM, 512 MB storage, USB-C PD
  (5/9/12 V), up to 18 W (CNX Software), two 2.5 GbE ports, 295 g; GL.iNet's own
  mesh (firmware 4.10, up to eight nodes) lists it as supported. A good first
  unit, and able to host a small brain.

A note from owners: the Beryl AX's case runs at 60–66 °C idle (GL.iNet forum).
Every router here needs air around it, which matters in a bag (see setup).

### Brain (Room tier and up; optional below)

**Must:** 64-bit Linux, 4 GB RAM or more, wired Ethernet, USB-C powered,
storage you trust (an SSD, or an A2-rated microSD from a known brand: the
brain writes the room's document and its journal).

**Example:** **Raspberry Pi 5** (4 or 8 GB). About 3 W idle headless, up to
about 9 W under full load (raspberry.tips measurements). Its official supply is
5 V/5 A (27 W) PD. **On a power bank that cannot offer 5 V/5 A it still runs,
but limits its USB ports to 600 mA in total**, which is enough for the radio
bridge and not for a USB hard drive. A small x86 mini PC (Intel N100 class) is
the Hall-tier alternative when the brain has more to do.

### Power

**Must:**
- **USB-C Power Delivery power banks of 99 Wh or less each.** 100 Wh is the
  usual airline carry-on limit without airline approval (the reason the kit is
  many small batteries, not one big one).
- **Enough PD ports** for the units on each bank, at the wattage each needs.
- **Pass-through charging** if the kit must run while being recharged (not all
  banks switch over without a dropout: test).
- **A USB-C power meter.** It is how a kit's real draw is learned, and how these
  pages get real numbers.

**Runtime arithmetic** (to be replaced with measurements): usable energy is
about 85% of the bank's rating after conversion.

| kit | planned draw | one 99 Wh bank lasts | for 8 h |
|---|---|---|---|
| Pocket: one Beryl AX | ~6 W | ~14 h | 1 bank |
| Small: one Slate 7 hosting the brain | ~12 W | ~7 h | 2 banks |
| Room: Slate 7 + Beryl AX + Pi 5 | ~6 + 12 + 5 = ~23 W | ~3.7 h | 3 banks |
| Hall: two Room kits | ~46 W | — | 5–7 banks |

The planned draws are vendor ceilings and published measurements, so real
runtimes should be longer. Measure before the first outing.

### Cables and small parts

- Short Cat6 patch cables (flat cables pack well), one per wired unit, plus a
  long one (10 m) to put the second access point across the room.
- USB-C cables rated for each load (an e-marked 5 A cable for the Pi 5).
- **Hall tier:** a small gigabit switch that runs on USB power.
- A label maker or tape: every unit labelled with its role and address.

### Radio bridge (optional)

- **For Void Buzz devices and Bluetooth reach:** an ESP32-S3 development board
  on the brain's USB port (Wi-Fi, Bluetooth LE, ESP-NOW).
- **For kit-to-kit over distance:** a LoRa board that runs RNode firmware
  (Heltec WiFi LoRa 32 V3 or LilyGO T3S3 class), so Reticulum can link two
  brains kilometres apart at low bandwidth. Buy the **frequency band legal where
  you are** (915 MHz in the Americas, 868 MHz in Europe, and so on).

## 4. Growing a kit

Every tier is the one before it plus units, which is assumption 3:

- **Pocket → Small:** the router becomes the second unit of a Small kit, or
  stays as a spare.
- **Small → Room:** add a Pi 5 brain and a second router on a cable.
- **Room → Hall:** two Room kits, cabled together, on different channels, one
  brain chosen as the room's host (the other stands by), or each kit keeping its
  own brain and the two syncing brain to brain over Reticulum.
- **Two groups that split up** each carry a kit; their brains merge the work
  when they can reach each other again, by cable, mesh or LoRa.
