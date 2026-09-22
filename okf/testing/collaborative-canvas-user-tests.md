---
type: Guide
title: Collaborative canvas — user testing guide
description: "The author's script for judging the collaborative canvas as a person: scenarios to run with two or more devices (desktop and Android), what to watch for, and the specific decision each one tests, so a verdict lands on a number. Tests are marked by the build stage that makes them runnable; most are waiting on the drawing and the IC integration."
tags: [status:current, audience:author, confidence:asserted]
timestamp: 2026-09-20T00:00:00Z
---

**Who this is for:** the author, testing by hand.
**What it is for:** Claude decides whether collaboration *works* (the tests in
`tests/` prove that). You decide whether it is *the right call*: whether it feels
good, whether it gets in your way, whether the defaults are right. Every
scenario names the decision it is testing, from
[collaborative canvas §0.1](/concepts/collaborative-canvas.md), so "too slow" or
"too loud" maps straight to a number that can change.

# How to report back

For each test you run, reply with its ID and one verdict:

- **keep**: right as it is.
- **tweak**: right idea, wrong amount (say which way: faster, quieter, bigger…).
- **wrong**: the wrong idea. Say what you expected instead.
- **broken**: it did not do what the test says it should. That one is a bug report, not a
  design verdict.

A sentence of how it *felt* is worth more than a score. "I kept losing track of
where Bo was" is exactly the kind of note that changes a design.

**You do not need to run everything.** The ★ tests are the ones whose answers
change the most.

# When each test becomes runnable

| stage | what it adds | status |
|---|---|---|
| **now** | the headless pieces only: the test suite, and what your machine reports about its network | ready |
| **N0** | two IC windows syncing on one machine; remote changes animate | **ready (2026-09-21)**: the duo bench, below |
| **0.3.0** | the phone layout (fits, rotates) and updating itself | **ready (2026-09-22)**: M1–M4, U-1–U-3 below |
| **N1** | cursors, drag ghosts, pending wires, held rings, edge chips | not built |
| **N2** | reduction claims and the crank, your-own-undo | not built |
| **N3** | a real LAN between devices, including an Android phone | **ready (0.4.0, 2026-09-22)**: the LAN button; tests L1–L4 below, then E1–E6 |
| **N4** | follow, pings, typing preview, "while you were away" | not built |

Each test below says which stage it needs. When a stage lands, Claude will tell
you which IDs just became runnable.

# The duo bench (stage N0, ready now)

One person, one desk, two windows: **Ana** (the host) on the left and **Bo**
(who joined) on the right. Each is a complete Interaction Combinators with its
own document, and they sync through the real Palabra session. Only the network
is simulated. Ana's window has a **Duo link** panel with the network's latency,
its loss, and a **cut the link** switch (a partition).

**Launch:** from `InteractionCombinators/`, build with `cmake -S . -B build` and
`cmake --build build`, then run `build/bin/interaction_combinators_duo.exe`.
(`interaction_combinators_duo.exe --selftest` runs the bench's own checks in hidden
windows and prints them. Claude runs that; you don't need to.)

**Runnable on the bench today:** C1, C3 (without claims yet: both of you *can*
step neighbours, and the wires still come out right, which is the research result),
C5, B4 (without the grab friction yet: you can both drag one node, and "latest wins"
decides), and **P1** below. The cursors, ghosts, claims and on-node conflict
marks (everything tagged N1–N4) are not drawn yet.

**P1 ★ Work apart, then meet** *(N0)*
Tick **cut the link**. In Ana, fire a pair. In Bo, fire a *different* pair and
move a node. Untick it.
*Watch:* both windows settle into the same net within a second or two, and the
changes you made in the other window **animate in** (moves glide, a step plays
its rewrite) instead of jumping. Does it feel like the other person's work
*arrived*, or like the screen glitched?
*Tests:* remote playback (250 ms tweens, the rewrite animation for remote steps).

# The phone layout (0.3.0, ready now)

Install `InteractionCombinators-0.3.0.apk` over 0.2.0 (it updates in place: same
signing key). Nothing here needs a second device.

**M1 ★ Does everything fit?** Hold the phone upright.
*Watch:* a slim bar at the top (the name, the pair count, and **⋮**), the canvas,
a row of actions at the bottom (step, reduce all, undo, redo, add, and a **⋮**
for the rest), and a sheet peeking up from the very bottom. Can you reach
every action? Is anything cut off?
*Tests:* the action bar's overflow; the ⋮ menu holding File / Edit / Selection /
Add / View / Net.

**M2 ★ Turn it.** Rotate to landscape, then back.
*Watch:* sideways, the inspector moves to the right of the canvas; upright, it
goes back into the sheet. Each time, the whole net is framed on screen. Does
reframing on every rotation help, or would you rather it kept your view?
*Tests:* orientation layouts; the fit-on-rotate camera (a behavior change listed
in the release).

**M3 The inspector sheet.** Upright, select an agent and drag the sheet up.
*Watch:* it snaps to half and to nearly full; drag it back down to peek. Is the
peek tall enough to see what you selected without opening it?
*Tests:* the bottom sheet's detents (12 %, 50 %, 92 % of the screen).

**M4 The menus behind ⋮.** Open ⋮ at the top, then File, then Settings.
*Watch:* do submenus open with a tap? Is the Settings window usable at phone
width?

# Updates (0.3.0, ready now)

**U-1 ★ The one question.** First launch of 0.3.0 (phone or desktop).
*Watch:* before anything else, it asks *"Check for updates when Interaction
Combinators starts?"* with **Yes, check / Not now / Never**. Is the wording right?
Does the second line make it clear that nothing downloads unless you choose it?
*Tests:* rule 1, never check unasked.

**U-2 After you answer.** Choose **Yes, check**.
*Watch:* nothing interrupts you. There is no update newer than 0.3.0 yet, so
nothing is offered. **View > Check for updates** should say you have the newest
version. Settings has an **Updates** section with your answer, which you can
change.

**U-3 ★ A real update** *(when 0.3.1 exists)*.
*Watch:* an **update 0.3.1** button appears in the toolbar (desktop) or the top
bar (phone). It never pops up by itself. Tap it: the prompt lists what 0.3.1 adds
and what behaves differently. **Download**, then **Install**. On Windows the new
version opens from a folder beside the old one, and the old one is untouched. On
Android the system asks you to confirm (the first time, it also asks you to allow
installs from this app).
*Tests:* the whole update path on a real release. **The Android half has never
run on a device before this test.**

# Two devices on one Wi-Fi (0.4.0, ready now)

Phone and PC on the **same Wi-Fi**. On the PC, the first time you share, Windows
asks about network access: choose **Allow** on private networks. (Guest and
campus Wi-Fi often block devices from reaching each other; home Wi-Fi works.)

**L1 ★ Share and join.** On the PC press **LAN** (bottom bar) → **Share this
net**. On the phone press **LAN** → **Join a net**.
*Watch:* within a few seconds the phone lists the PC by its computer name. Tap
**Join**. The PC's LAN panel pops up: *"Phone-xxx wants to join"* → **Allow**. The
phone now shows the PC's net.
*Tests:* discovery ("beacon out, unicast back": the phone hears the PC's answer
even though Android drops broadcasts), the Allow gate.

**L2 Join by code.** If the phone's list stays empty: the PC's panel shows a short
code (about three digits). On the phone, type it on the keypad → **Join by code**.

**L3 ★ Work together.** Move a node on one device; fire a pair on the other; tag an
agent `+red` on one and `+blue` on the other at the same time.
*Watch:* each change appears on the other device within a second; a step fired on
one plays its rewrite animation on the other; the two tags make purple on both.
*Tests:* sync over the real LAN, remote playback, pigments merging.

**L5 ★ The phone sleeps.** While joined, turn the phone's screen off for a
minute, then turn it back on.
*Watch:* the PC says the phone "went quiet" within about 12 seconds (it no
longer claims to be connected). When the phone wakes it rejoins **by itself**,
and the PC does **not** ask you to allow it again. Anything made while it slept
arrives.
*Tests:* the idle timeout, the retry loop, and letting a known device back in.

**L6 ★ Do the two screens really agree?** Wire some ports on one device
(including a constructor wired to itself, the case that looked wrong in 0.4.0).
Open **LAN** on both and compare the line "this device: N agents, N wires, N
pairs".
*Watch:* the numbers should match. If they do not, press **Resync now** on
either device and watch them again; then tell Claude both sets of numbers and
whether Resync fixed it. That is the measurement that was missing when this went
wrong the first time.

**L7 Names.** On the host, open **LAN** and type a net name (desktop). On the
joiner's list the net appears under that name. In **Settings > Profile**, change
your name and colour: the other device shows the new name when you reconnect.

**L4 Deny, and leaving.** A third device (or the phone again) asks to join and the
host presses **Deny**: the joiner is told "the host said no" and receives nothing.
Press **Leave** on the phone: the PC's count drops, and the phone keeps its copy.

# Setup, once per session

- **Two people is best**; one person with two devices works for most tests.
  Where a test needs "Bo", that is the second device or the second person.
- **Desktop + desktop** first (N0–N2 run as two windows on one machine), then
  **desktop + phone** (N3).
- Give the two profiles **different colours** that are *not* red, yellow or blue
  for the first run, then repeat one test (U3) with a red profile on purpose.

---

# A. Seeing each other

**A1 ★ Cursors: can you follow where Bo is?** *(N1)*
Both open the same net. Bo moves around, pauses, moves again.
*Watch:* does Bo's arrow move smoothly, or jump? Does the name tag fading after
2 s feel right, or does it vanish while you are still looking? Is it distracting
when Bo is idle?
*Tests:* 20 Hz send rate, 100 ms smoothing delay, 2 s tag fade.

**A2 Phone cursor** *(N3)*
Bo is on the phone. Bo touches the canvas, drags, lifts.
*Watch:* the fingertip dot appears only while touching. Does it disappear too
abruptly when the finger lifts? Would you rather it linger?
*Tests:* the fingertip dot, shown only while the finger is down.

**A3 ★ Where did Bo go?** *(N1)*
Bo pans far away from you.
*Watch:* an avatar chip appears on the edge of your canvas pointing toward Bo.
Click it and your view glides to Bo. Is the chip noticeable without being noisy?
Is the 300 ms glide disorienting, or too slow?
*Tests:* edge chips, click-to-jump, 300 ms glide.

**A4 Follow** *(N4)*
Click Bo in the member strip, then let Bo lead you around the net.
*Watch:* do you stay oriented? When you nudge the camera yourself, following
stops. Is that what you wanted, or annoying?
*Tests:* follow, "any camera gesture of your own stops it".

**A5 ★ Look here** *(N4)*
Alt+click (desktop) or two-finger tap (phone) on an active pair while Bo is
looking elsewhere.
*Watch:* does Bo notice the ripple? From off screen, does the flashing edge chip
get Bo's attention? Is 1.5 s long enough?
*Tests:* ping, 1.5 s ripple.

# B. Working on the same thing

**B1 ★ Grabbing what Bo is holding** *(N1)*
Bo starts dragging a node and keeps holding it. You try to drag the same node.
*Watch:* your drag does not start, and a toast says "Bo is moving this". Now
press and **hold 600 ms**: you take it. Is refusing your drag the right default,
or would you rather both drags just happen and the later one win? Is 600 ms the
right hold?
*Tests:* grab friction, 600 ms override. **This is the decision most likely to be
wrong**, because it trades your speed for avoiding collisions.

**B2 ★ Two ports, one node** *(N1)*
Bo drags a wire out of port **a** of a γ agent. At the same moment you drag a
wire out of port **b** of the same agent.
*Watch:* both of you should be allowed: different ports do not contend. Then try
port **a** while Bo holds it, and you should be refused. Does per-port feel right,
or should one person hold the whole node?
*Tests:* your ruling "whoever selected this specific port first".

**B3 Who got there first** *(N1)*
You and Bo count down and click the same port at the same instant.
*Watch:* one of you gets it, the other's gesture cancels with a toast naming the
winner, and the loser's staged wire snaps back over 150 ms. Did the losing side
feel robbed, or was it clear what happened?
*Tests:* concurrent claims, the 150 ms snap-back.

**B4 ★ Two drags of one node** *(N1, with the override)*
Use the B1 override so that you are both genuinely dragging one node, and release at
nearly the same time.
*Watch:* both screens end with the node in **the same place**, with no dialog.
Could you tell whose position won? Did it matter to you?
*Tests:* view state never asks (`Pick`). Also tell us whether "last one wins"
matters to you here or whether "the same on both screens" is enough.

**B5 Typing in the same field** *(N4)*
Bo starts renaming an agent. You open the same field.
*Watch:* you see Bo's caret and Bo's half-typed text in italics. You get
"Bo is editing. Edit anyway?". Is seeing the half-typed text useful, or creepy?
*Tests:* typing indicator, live preview, field claims.

**B6 ★ A real text conflict** *(N3, needs a disconnect)*
Disconnect the phone (airplane mode). Both edit the same label to different
words. Reconnect.
*Watch:* the field shows both values side by side in your two colours, and one
click keeps one. It never pops up a dialog. Was it obvious something needed
you? Was it obvious which was yours?
*Tests:* text conflicts shown in place, never modal.

# C. Reducing together (interaction combinators)

**C1 ★ Watching Bo reduce** *(N0)*
Bo clicks **step** on a pair you can see.
*Watch:* on your screen the rewrite animates exactly as if you had done it, in
Bo's colour. Did you understand what happened without being told?
*Tests:* remote rewrite playback.

**C2 ★ The crank** *(N2)*
Bo clicks **reduce all**. You try to click **step**.
*Watch:* your buttons read "Bo is reducing". Click **Request**, and Bo gets a
ping. Is locking you out of stepping during Bo's run right, or should you be
able to step a region far from Bo's?
*Tests:* the crank. Your answer decides whether we go straight to region claims.

**C3 Stepping in different corners** *(N2)*
With no crank held, you step a pair on the left of the net while Bo steps one on
the right.
*Watch:* both steps happen, and both screens agree afterwards.
*Tests:* redex claims allow disjoint steps.

**C4 ★ Stepping neighbours** *(N2)*
Find two active pairs joined by a wire (Claude can build this net for you). You
step one while Bo steps the other, at the same moment.
*Watch:* one of you is refused ("Bo is reducing next to this") and the net stays
correct. This is the exact case the research is about. When the wire-as-variable
encoding lands, **both** steps will be allowed and still come out right, and
this test will be re-run to see if that feels better.
*Tests:* boundary claims, and later the HVM2-style encoding.

**C5 Pigments at once** *(N0)*
You tag an agent `+red` while Bo tags the same agent `+blue`.
*Watch:* both screens turn it **purple**. The pigment net resolved your
collaboration. Is this as delightful as we think?
*Tests:* nothing to tune. It tells us whether this is the demo's headline moment.

# D. Undo with company

**D1 ★ Undo my own thing** *(N2)*
You move a node, Bo moves a different one, and you press Ctrl+Z.
*Watch:* only **your** move is undone, and Bo's stays.
*Tests:* your-own-undo.

**D2 Undo something Bo has since changed** *(N2)*
You move a node, Bo then moves the same node, and you press Ctrl+Z.
*Watch:* nothing happens, and a toast says "Bo changed γ-3 since". Would you rather
it undo anyway (moving Bo's node back)?
*Tests:* undo refuses to overwrite a collaborator's later work.

# E. Joining, leaving, and the network

**E1 ★ Join from the phone** *(N3)*
Desktop: **Host**. Phone: **Join**. The desktop should appear in the list.
*Watch:* how long until it appears? Did the pairing code step make sense?
*Tests:* discovery by "beacon out, unicast back" (no multicast lock needed
between phone and desktop).

**E2 Join by code** *(N3)*
Phone: **Join by code**, and type the digits the desktop shows (on home Wi-Fi,
about three digits plus maybe a port).
*Watch:* was the keypad comfortable? Was the code short enough?
*Tests:* the join code, and the digits keypad instead of the system keyboard.

**E3 Phone to phone** *(N3, needs two phones)*
Two phones, no desktop.
*Watch:* do they find each other? (This is the one case that needs the
multicast lock. If it fails, join by code must still work.)

**E4 The phone goes to sleep** *(N3)*
Mid-session, lock the phone for 30 s, keep editing on the desktop, then unlock.
*Watch:* the phone reconnects on its own, and the things that changed while it
slept are highlighted. Is the highlight helpful or noise?
*Tests:* reconnect after background, "while you were away".

**E5 A network that isolates devices** *(N3, optional)*
Try on a guest or campus Wi-Fi.
*Watch:* does the status pill say something useful ("can't see anyone" versus
"can see them, can't connect"), or does it just never connect?

**E6 First launch on Windows** *(N3)*
The first time you host, Windows may ask about the firewall. Try **Cancel** once
on purpose.
*Watch:* does the app eventually tell you why nobody can find you?

# U. Clutter and legibility

**U1 Three or more people** *(N1)*
Three devices, everyone moving.
*Watch:* at what point do cursors and name tags become noise?

**U2 Presence against the hot halo** *(N1)*
Bo selects an agent that is part of an active pair (the orange halo).
*Watch:* can you still see both the halo and Bo's ring?

**U3 ★ Red on red** *(N1)*
Set Bo's colour to red, and have Bo select a red-pigmented agent.
*Watch:* is Bo's selection still visible? (That is why the dashed ring exists.)
*Tests:* never colour alone.

# Available now

These need nothing new, just a terminal in the repo.

**N-1 What your machine reports about its network**
Build, then run `ctest -R lan_smoke -V` from `build/`. It prints the interface it
would use for the LAN. On this machine it printed `Wi-Fi 10.0.0.169/24`. Run it
with a VPN connected, or on a laptop with WSL or Docker, and tell us whether it
still picks the right one.

---

*Maintained by the Void Maiz agent: tests are added when features land, and your
verdicts are folded into [collaborative canvas](/concepts/collaborative-canvas.md)
§0.1 with the date.*
