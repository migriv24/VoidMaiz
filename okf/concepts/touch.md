---
type: Concept
title: Touch
description: "What a finger changes and what it does not: the deferred press, the long-press that IS a right-click, physical units instead of pixels, the hit/draw asymmetry — plus the census of mobile-native chrome (and the handheld borrowings) with what is built, staged and declined."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-09-13T00:00:00Z
---

Touch is an **input modality, not a geometry** — [substrates](/concepts/substrates.md)
settled that on 2026-07-13, and it is still right. But "not a geometry" was read
for a year as "not our problem", and it is not that either. A finger is a
physical object about nine millimetres across that cannot hover, occludes what
it touches, and is attached to a hand that is also holding the device. None of
that changes the model; all of it changes the surface.

This page is what changed on 2026-09-13, when the mobile side got deliberate
attention ahead of **Void Hormiga going mobile**.

# Why now: the second host

The InteractionCombinators APK shipped 2026-07-14 with the whole touch layer
hand-rolled in its own NativeActivity shell — the two-finger camera, the pointer
release when a second finger lands, the degenerate-pinch guard, the
swallow-until-all-up rule, a `toast_until` float standing in for a snackbar, and
`touch_mode ? 14 : 6` for a splitter's thickness.

It works. It was also **never tested**, because a touch layer that lives in a
shell can only be tested by putting a phone in somebody's hand.

Void Hormiga is the second host, and it is the harder one: canvas, inspector,
table view, log strip, command bar and a Territory map, in four workflows, all
of which have to fit a screen 430 px wide. If the logic stayed in the shells, it
would be written twice, tested zero times, and would diverge — the same argument
the author accepted for docking (Q11, 2026-07-20) and for
[`glhost.hpp`](/../include/voidmaiz/glhost.hpp) (2026-09-08, where the reason was
sharper: *the examples are the first thing a new client copies, so a bug in them
propagates by design*).

# The commitment reading

Nothing here is new architecture. Commitment 2 says every gesture is a visible,
replayable command; a pinch is a gesture; therefore **a pinch compiles to the
same `config set view.camera` the mouse wheel does**, and a long press ends in
the same context menu a right-click opens. The recognizer sits on the UI-free
side beside [gesture.hpp](/../include/voidmaiz/gesture.hpp) for the same reason
the gesture compilers do: contacts in, commands out, no rendering types, and a
test that drives it with a literal array and no window.

`tests/touch_smoke.cpp` is 81 assertions of exactly that, and it is the part of
this work that the APK could not have.

# The deferred press (the mechanism everything else hangs on)

On glass a press is ambiguous for a few hundred milliseconds: it may become a
tap, a drag, or a long press. A shell that forwards the contact to the pointer UI
immediately **has already committed to "drag" before it knows** — so every long
press first starts a node move, and every tap is a zero-distance drag that the
canvas then has to un-interpret.

So the recognizer latches the press and delivers a *synthetic* pointer only once
the gesture is decided:

| the finger… | the pointer gets |
|---|---|
| moves past slop | left-down **at the original press point**, then follows |
| lifts early | left-down, then left-up, both at the press point (a tap) |
| is held past 400 ms | **right-down for one frame** at the press point, then parks |
| is joined by a second | left released immediately (an in-flight drag aborts), then parks until every finger is up |

Two details in that table are the difference between working and nearly working:

- **The press point, not the current point.** By the time slop is exceeded the
  finger has already slid; a down delivered where it now is hit-tests the wrong
  node. The library holds the pointer at the press point for exactly the frame
  the button goes down.
- **The release lands on screen.** A lift parks the pointer off-viewport, and
  the canvas reads the pointer position *on the release frame* — its click-slop
  test and its hit test both do. So a release is delivered at the last real
  position first, and the park happens after.

The state the APK's shell called *swallow until all up* is `Parked`, and it earns
its name: a two-finger pinch ends with the fingers leaving one at a time, so the
instant the first leaves there is exactly one contact on the glass — which,
without it, latches as a fresh press and finishes as a stray tap on whatever was
underneath.

# The long press IS a right-click, and that is the whole trick

The highest-leverage decision on this page is three lines long. A long press
synthesizes a clean right-CLICK at the press point, so **every context menu the
desktop canvas already has opens on glass with no canvas change whatsoever** —
node, wire, empty canvas, and every host entry hung off `ContextMenuFn`.

The alternative was a touch-specific menu path through `edit_canvas`, which would
have meant a second menu implementation, a second set of host hooks, and two
places for a host's entries to be forgotten. Instead the canvas never learns that
a finger exists. A host that wants something else — a radial menu, a preview —
takes the `LongPress` event and sets `synthesize_right_click = false`.

This is the same shape as the geographic view (a new metric, not a new
dimension): the substrate is absorbed at the boundary, and the thing above it
does not change.

# Millimetres, not pixels

Every threshold in `TouchProfile` is in **millimetres**. A 6 px slop is a
different gesture on a 160-dpi tablet and a 560-dpi phone, so a library that
ships pixel constants ships a different feel per device and calls it one
library. The shell supplies one number — `dp`, the platform's density scale —
and the budgets convert (160 dp = 1 inch = 25.4 mm).

# The hit/draw asymmetry

`port_hit_radius` was already the right idea and the wrong number: 9 px of
screen-space grab radius under a contact patch nine *millimetres* wide.
`apply_touch_canvas` sets it to about half a fingertip and leaves the drawn
marker alone. **The hit target grows and the picture does not** — nothing on
screen gets uglier and everything gets grabbable, which is the only way dense UI
survives a finger.

The deliberate omission is the node geometry (`node_w`, `header_h`, `port_row`).
Those are **world** units, and scaling them by display density would make a
phone's graph a different graph: the same document, opened on two devices, would
lay out differently. Screen size for world geometry is the camera's job, which is
why a touch host starts zoomed in rather than with fattened nodes.

And `click_slop` gets *smaller*, not bigger — counter-intuitively, and worth
writing down because the obvious edit is wrong. The recognizer has already
decided tap-vs-drag before the synthetic pointer goes down, so the canvas is
handed a decision, not an ambiguity; a second fat slop budget would only blur the
long-press right-click.

# The census: mobile-native chrome

The vocabulary a phone application is expected to have, with what Void Maiz does
about each. **Built** means it is in `voidmaiz/mobile.hpp` and compiles today.

| idiom | what it is for here | status |
|---|---|---|
| **bottom sheet with detents** | the inspector, the record detail, the filter rail — a phone's answer to a docked side panel; the detent is view state, flushed to the config tier on `settled` like the camera | **built** |
| **snackbar with an action** | the log strip has no room on a phone; "Deleted. UNDO" costs one widget because `undo` is already a verb and every gesture is already a command | **built** |
| **FAB + speed dial** | the add-palette, where a thumb can reach it | **built** |
| **segmented control** | Hormiga's four workflows; a tab bar a finger can hit | **built** |
| **stepper** | a drag-number needs sub-pixel aim and has no affordance under a finger; small and quantised fields get −/+ with hold-to-repeat | **built** (raw control; the registry-side field editor is staged) |
| **swipe-actionable row** | the gesture a phone uses where a desktop uses a right-click — Hormiga's table view | **built** |
| **long-press context menu** | see above: it is the desktop menu | **built** |
| **pinch / two-finger pan / rotate** | the camera, compiling the same command the wheel does; rotate is unclaimed and waiting for the backlog's shaped-node rotate gesture | **built** |
| **edge-swipe back** | leaving a subgraph — the mantle stack is exactly a navigation stack. Opt-in (`edge_mm`, default 0) so nobody discovers that the leftmost node cannot be dragged | **built** |
| **fling / momentum** | reported as a `Fling` event with velocity; the canvas does not yet coast, because inertia is an animation and `CanvasFx` is the place for it | **half**: recognized, not consumed |
| **soft keyboard / IME** | the command bar is unusable on glass without one, and this is the APK's oldest known gap | **not built — Q29** |
| **action sheet** | a modal list from the bottom edge | staged: a bottom sheet with a fixed detent |
| **pull-to-refresh** | "re-read the state document" — which in a log-first system is a real gesture, not a hack | staged |
| **wheel / drum pickers** | dates and enums | staged |
| **safe-area insets** | notches and gesture bars; ImGui's work area covers part of it | staged |
| **text-selection handles, magnifier** | ImGui's own text editing is desktop-shaped | not planned without a client |

# The census: borrowings from outside the phone

The author asked for this half explicitly, and it is where the interesting
answers are.

- **The 3DS/DS dual screen** — the view and the *control surface* are different
  screens. The mobile reading is the bottom sheet; the honest reading is that our
  architecture already does the extreme version, and it is worth saying out loud:
  **a phone can be the control surface for a desktop session**, because
  [headless](/concepts/headless.md) already proved two front-ends over one state
  document with no sync and no import. Nobody else's node library can offer that,
  and we would be claiming it rather than building it. Recorded as a horizon.
- **The DS's discipline** — single-touch, resistive, no hover, no multitouch.
  Every DS interaction was designed for one point and a drag. Our
  `hover_tooltips` flag already admits that touch loses the hover channel; the DS
  is the proof that losing it is survivable if the vocabulary is designed rather
  than ported.
- **Stylus stroke gestures** (*Phantom Hourglass*, *Kirby Canvas Curse*) — draw a
  stroke **across wires to cut them**, a circle to lasso-select, a stroke from
  node to node to link. On glass this is a better primary gesture than a precise
  port grab, and the backlog's T2 "wire cut gesture (Ctrl-drag)" is the same
  feature arriving from the desktop side. Staged, and the one on this list most
  worth building next.
- **Shake-to-undo, back-tap, the Vita's rear panel** — non-pointer input
  channels. They cost us nothing conceptually because they are just more command
  emitters; the recognizer's shape (contacts in, events out) extends to them
  without a new seam.
- **Pencil hover** (S-Pen, Apple Pencil 2) — gives the hover channel *back* on
  the devices that have it, which would let `hover_tooltips` become a capability
  question rather than a substrate question.
- **Reachability / the thumb arc** — why the FAB is in the corner and the
  workflow switcher is not at the top on a tall device. Partly built, partly a
  layout question (Q28).

# What touch gave the attention graph

Verifying the Allomone wiring after the touch work turned up a hole that neither
side could see alone, and it is the most useful thing this session found second.

`UserGraph::touch` takes a `channel`; Allomone's `device "pen"` reads it; and the
only host that fed it — `examples/allomone_playground.cpp` — got the value from a
**dropdown the person set by hand**. Every piece was individually correct, so
every test passed. But `device "touch"` was matching *a claim about the input*,
never the input, and no host could have done better because there was nothing
else to fill it with.

A recognizer is that something else. `TouchPoint::tool` (finger / stylus /
eraser / mouse — every touch platform reports it, and nothing had asked) and
`TouchFrame::channel` make the channel an observation:

    ugraph.touch(id, "widget", frame.channel);   // observed, not declared

The library still feeds no graph itself — attention is ephemera and
materializing stays the host's choice — and `channel` is empty while nothing is
being touched, so a host writes a channel only when there was one.

**What it opens** is the part worth building on. A stylus is not a finger, and on
a device with both, the difference is genuine intent: annotating versus
navigating. `when device "pen" -> annotate 1` is now a rule that can fire, and
the first responsive rule whose condition a person cannot accidentally
misreport. The same seam carries `xr` the day Void Maiz XR exists.

`tests/touch_usergraph_smoke.cpp` drives the whole chain — recognizer → channel →
`UserGraph` → host predicates → **the sibling repository's** evaluator — and
spells everything `maiz::`, so a broken re-export fails there rather than in a
host. See [user graph](/concepts/allomone/user-graph.md).

# The boundary

**The library owns**: recognition (contacts → gestures), the pointer policy, the
physical profile, the camera application, and the chrome widgets that have no
desktop ancestor.

**The host owns**: which panels exist and where, what a long press means beyond
the built-in menu, what an edge swipe navigates to, whether a fling coasts, and
every command — the library dispatches nothing, ever. `examples/mobile_window.cpp`
is the reference host, and it is a desktop binary on purpose: the mouse is a
finger and Alt adds a mirrored second one, so the kit can be looked at without an
APK.

**The library does not own** a pane layout engine. That is Q28, and the lean is
still the author's rule — climb one rung per real need.

# What this does not prove

The same limit as [platforms](/concepts/platforms.md), and it is the honest half
of this page: **nobody has touched any of it with a finger.** It compiles on
three desktops, 81 assertions pin the recognizer, and the reference host was
built and not run — the author was using the machine when the session finished,
so the interactive pass was skipped by the standing rule rather than forgotten.
`platforms` in `void.json` is unchanged.

A recognizer is exactly the kind of code that can be right in every test and
wrong in the hand. The next session's first job is a person, a phone or the
reference host, and ten minutes.
