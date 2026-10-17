---
type: Guide
title: Setting up a kit
description: "Step by step: the first access point, a second one by cable, the brain, the phones; the test to run at home before going out; packing a kit that runs in a bag without cooking; and what to check when a phone does not connect. Written for the Room tier (the largest common case), with notes for smaller and larger kits."
tags: [status:draft, audience:all, confidence:asserted]
timestamp: 2026-10-17T00:00:00Z
---

# Setting up a kit

Written for the **Room** tier ([choosing](/hardware/choosing.md)): two travel
routers, a Raspberry Pi 5 as the brain, three power banks. A Pocket or Small
kit skips the steps for units it does not have. The menu names are GL.iNet's
(their admin page sits on OpenWrt; "Advanced" opens OpenWrt's own LuCI); any
OpenWrt router has the same settings under other names.

**Do all of this at home, on mains power, the day before.** Nothing below needs
internet once it is done, but firmware updates and packages do.

## 0. Plan the names and numbers (five minutes, on paper)

Write these on a card that travels with the kit, and print the Wi-Fi details
as a QR code on it (phones join by scanning):

| what | example |
|---|---|
| network name (SSID) | `fieldkit-hormiga` |
| password | long, random, written on the card |
| the kit's subnet | `10.77.0.0/24` (anything private that will not clash with a phone's home network; avoid `192.168.0.x` and `192.168.1.x`) |
| AP 1 (main) | `10.77.0.1` |
| AP 2 | `10.77.0.3` |
| brain | `10.77.0.2`, name `brain.kit.lan` |
| channels | AP 1: 5 GHz ch 36, 2.4 GHz ch 1. AP 2: 5 GHz ch 149 (or 44), 2.4 GHz ch 11 |

## 1. The main access point

1. Power it from the wall, join its default Wi-Fi, open its admin page, set an
   admin password. **Update the firmware now** (it will never see the internet
   again).
2. **LAN address** → `10.77.0.1`, DHCP pool `10.77.0.100`–`10.77.0.250` (room for
   150 devices).
3. **Wi-Fi:** one SSID and password on both bands (WPA2/WPA3 mixed mode, so
   older phones join). 5 GHz at **20 or 40 MHz width** in a dense room (wider
   channels hurt when many devices share the air), on the planned channel.
   2.4 GHz stays **on** at 20 MHz (ESP32-class devices only speak 2.4 GHz).
4. **Turn client isolation OFF** ("AP isolation", "guest mode"). With it on,
   members cannot reach each other or the brain, and nothing works.
5. **The unused WAN port becomes a LAN port.** There is no internet to plug
   into, so in OpenWrt add the WAN port to the LAN bridge. Now the router has two
   LAN ports: one for the brain, one for AP 2. (Otherwise use a small switch.)
6. **Fixed lease** for the brain (its MAC address → `10.77.0.2`) and a **local
   DNS name**: `brain.kit.lan` → `10.77.0.2` (DNS → hostnames / custom records).
7. **Answer the phones' internet probes** (an off-grid kit only; see the field
   network's Q42). Make the router's DNS answer `captive.apple.com` and
   `connectivitycheck.gstatic.com` with `10.77.0.1`, and serve
   `/hotspot-detect.html` with a page containing `Success` and `/generate_204`
   with an empty 204. Phones then stop calling the network "No Internet" and
   stop trying to leave it. **Never do this on a kit that has an uplink.**
8. Turn off what has nothing to do: the router's VPN, cloud, and remote
   management features; internet "connection checks" that reboot or blink.

## 2. The second access point (Room tier and up)

1. Same firmware update and admin password.
2. Set it to **Access Point mode** (GL.iNet: Network → Network Mode → Access
   Point). Its DHCP turns off; the main router hands out addresses.
3. Address `10.77.0.3`, gateway and DNS `10.77.0.1`.
4. **Same SSID and password**, the **other** channels from step 0, client
   isolation off.
5. Cable it to the main router's second LAN port. Phones now pick whichever
   unit is closer; put the two units at opposite sides of the room.

Larger kits: each further unit is the same, on its own channels, cabled through
a switch. GL.iNet's own mesh (firmware 4.10+, Slate 7 among others) can replace
the cable, at the cost of some airtime; cable whenever the room allows it.

## 3. The brain

1. Install **Raspberry Pi OS Lite (64-bit)** with SSH enabled. Plug it into the
   main router's LAN port. Check it received `10.77.0.2`.
2. Advertise the kit by Bonjour so iPhones find it without broadcast: install
   `avahi-daemon` and publish a `_voidmaiz._tcp` service on the application's
   port.
3. **Reticulum**: install it (`pip install rns`), and in its config enable
   transport and add a **TCP server interface** on port 4242. Members connect by
   TCP to `brain.kit.lan`.
4. **The room's copy of the work.** The application's room-host mode does not
   exist yet ([field network](/concepts/field-network.md), Q41). **Until it
   does,** the host is a laptop on the kit's Wi-Fi running the desktop
   application as the session host (Void Hormiga's desktop already hosts; its
   phones join as members). Give that laptop the fixed address instead.

## 4. The phones

- **Join the Wi-Fi** by scanning the card's QR code.
- **iPhone:** the first time the application opens on the kit it asks for
  **Local Network** access: allow it, or the app cannot see anything on the kit
  (Settings → Privacy & Security → Local Network fixes a "No").
- **Android:** if the phone has mobile data, it may still route the app over
  mobile data because the Wi-Fi has no internet (the probe answers in step 1.7
  prevent this; turning mobile data off also does).
- **Keep the app in front** for the first sync. iPhones suspend backgrounded
  apps within seconds; the app catches up from the brain when it comes back.

## 5. The test, at home, before the first outing

Use at least **two iPhones and two Android phones**; borrow them if necessary.
The first kit is a measurement as much as a test: write the numbers into
[choosing](/hardware/choosing.md).

1. **Power, on batteries only.** Unplug the wall. Put the USB power meter on
   each unit's cable; write down idle watts, then watts with all phones syncing.
2. **Joining:** every phone joins from the QR code, and every iPhone is asked
   for Local Network access once.
3. **Finding the brain:** from each phone the app finds the host (by Bonjour,
   then by name, then by address).
4. **Syncing:** a change on an iPhone appears on an Android phone and the other
   way round.
5. **A sleeper:** lock one iPhone for five minutes while others make changes.
   Unlock it: it catches up without anyone doing anything.
6. **Roaming:** walk a phone from one access point's side of the room to the
   other while it syncs.
7. **The edge:** where in the space does the signal fail? Mark it.
8. **Runtime:** leave the kit running on batteries with one phone syncing now
   and then until a bank runs out. That is the real runtime.

Anything that fails here would fail in the field without anyone able to fix it.

## 6. Packing

- **Air, not padding.** Travel routers run hot (owners of the Beryl AX report
  60–66 °C on the case at idle). Carry them in a mesh or outer pocket, or a
  small vented pouch; never sealed inside a padded bag while running.
- **One bank per unit**, labelled, so a flat bank is swapped without touching
  the rest. Spares charged, in a separate pocket.
- **Cables pre-plugged** into the units where possible; the long cable for AP 2
  coiled with it.
- **The card** (SSID, password, QR, the brain's address, these steps in short)
  in a sleeve on the outside.
- **Set-up in the field** is then: power on AP 1, power on the brain, power on
  AP 2 at the far side, scan the QR. Two minutes.

## 7. When something does not connect

| symptom | likely cause | check |
|---|---|---|
| phones join the Wi-Fi but see nobody | client isolation on | step 1.4 on every access point |
| iPhones see nobody, Androids do | Local Network permission refused | iPhone Settings → Privacy & Security → Local Network |
| an Android works only with mobile data off | traffic routed over mobile data | step 1.7, or mobile data off |
| phones keep leaving the Wi-Fi | "no internet" | step 1.7 |
| the brain is not found by name | lease or DNS record missing | step 1.6; try the address from the card |
| works near AP 1, fails near AP 2 | AP 2 not cabled, or different SSID/password | step 2 |
| slow with many phones | channels too wide, or both units on one channel | step 0's channel plan, 20 MHz on 5 GHz |
| a unit reboots under load | the bank cannot hold its peak draw | swap to a bank with a higher PD rating; check with the meter |
| ESP32 devices cannot join | 2.4 GHz off, or WPA3-only | step 1.3: 2.4 GHz on, mixed WPA2/WPA3 |
