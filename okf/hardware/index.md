---
type: Index
title: Hardware
description: "The physical side of running a Void Maiz application off the grid: what to buy and how to set it up so many phones (Apple and Android) share one network with no internet. Sized from a pocket kit for a handful of devices to a multi-backpack kit for a hall. For anyone deploying a Void Maiz application, not only this project's agents."
tags: [status:draft, audience:all, confidence:asserted]
timestamp: 2026-10-17T00:00:00Z
---

# Hardware

The rest of this bundle is software. This section is for the person who has to
**buy things, plug them in and carry them**: a group running a Void Maiz
application (Void Hormiga, Interaction Combinators, anything built on the
library) where there is no internet, with more phones than Bluetooth can serve.

Why a kit at all, and how the software uses it, is
[the field network](/concepts/field-network.md). In one paragraph: Apple and
Android phones cannot form a fast network with each other on their own (their
peer-to-peer Wi-Fi does not cross, and Bluetooth is slow), so the group carries
a small **Wi-Fi network with no internet** and a small always-on computer, the
**brain**, that holds the room's copy of the work. Every phone joins the Wi-Fi
and talks to the brain.

## The pages

1. **[Choosing a kit](/hardware/choosing.md)**: how many devices, which tier,
   what to buy (by specification first, with dated examples), how long the
   batteries last.
2. **[Setting up a kit](/hardware/setup.md)**: configuration step by step, the
   test to run before taking it out, packing, and what to do when something
   does not connect.

## The assumptions every kit is built on

These are the author's (2026-10-17), and every recommendation is checked
against them:

1. **It fits in a backpack.** Per person carrying it; a larger kit is more
   backpacks, not a bigger box.
2. **It is mobile.** Battery-powered, set up in minutes, works on the move. No
   mains power, no internet, no fixed position.
3. **It scales by adding units.** A bigger room means another access point, not
   a different kit.
4. **Up to a few hundred devices is the horizon, not the target.** Kits here go
   to about 150; 300+ is noted where it changes the design, and needs gear that
   is no longer a backpack.
5. **It stays compatible with Void Buzz** (their ESP32-class devices,
   their Hive): keep a 2.4 GHz network on, and leave the radio bridge a
   USB port. See the field network's Void Buzz section.

## Status

**Nothing here has been bought or measured.** Specifications come from vendors
and reviews (cited on each page), planning figures from published Wi-Fi design
guides, battery runtimes from arithmetic. The first kit assembled is the test,
and these pages are to be updated with what it measured: real watts on a USB
power meter, real client counts, real runtimes. Until then they are
`confidence:asserted`.
