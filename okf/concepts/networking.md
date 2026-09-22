---
type: Concept
title: Networking
description: "Networking for ANY Void Maiz application, ruled by the author 2026-09-18 (Q30): Void Maiz owns what networking LOOKS like — surfaces, presence, marks, the privacy padlock, profile presentation, the Networking settings — and the rules that make it the same on every view; Void Palabra owns what networking IS (transport, keys, sync, files by hash); the application answers one question, 'may this rune leave this device?'. Stages A, B and C built; only a real network transport waits, on the trust model."
resource: include/voidmaiz/presence.hpp
tags: [status:current, audience:dev, audience:host, confidence:measured]
timestamp: 2026-09-18T00:00:00Z
---

Raised by Void Hormiga after its two-machine LAN test. The author's direction:
*"a general universal networking ability should exist for any void maiz based
application."* The author then clarified what to build: **not "Hormiga's
networking component" but "any GUI application's networking component"**, even
if an application that already networks has to change significantly to adopt it.

**Built:** stages A and B are `voidmaiz/presence.hpp` (UI-free),
`voidmaiz/netview.hpp` (view), the canvas's `CanvasNet`, and the Allomone bridge in
`voidmaiz/allomone.hpp`, pinned by `tests/presence_smoke.cpp` and seen working in
`examples/network_window.cpp`. Stage C is `voidmaiz/net.hpp` (target
`voidmaiz_net`, 2026-09-19), which runs real Cores through Void Palabra's sync
session, pinned by `tests/net_smoke.cpp` over an in-memory lossy mesh.
**Not built:** a transport that moves bytes across a real network. That is gated
on the trust model (signatures, whose keys, capabilities), as the family's rule
requires.

# Why a UI library owns this

Networking goes wrong in GUI applications **one view at a time**. The list marks
who is looking, the canvas outlines it, the map shows nothing, because each view
decided for itself. None of that is a network bug. Each one is a view that was
never told the rule.

Void Maiz exists to make rules like that universal. A view does not decide
whether a move on the canvas is a command; it always is, because the library is
the only path. Presence works the same way: **a view declares what it shows, and
one renderer marks it.** A new view gets presence by declaring and cannot forget
half of it, because declaring and drawing are the same call.

# The line (Q30, ruled 2026-09-18)

**Void Maiz owns what networking looks like. Void Palabra owns what networking
is. The application answers what may be shared.**

| | owner |
|---|---|
| surfaces (what is on screen), presence (who is on what), marks, the privacy padlock, avatars, the member list, the Networking settings, the presence payload's *content* | **Void Maiz** |
| the keypair and signatures, transport, the ephemeral channel presence travels on, the sync loop and deletions, files fetched by hash, not fetching under cautious transfer | **Void Palabra** |
| the answer to "may this rune leave this device?", which surfaces it has, which settings it exposes, its own domain | **the application** |

# The model, in five pieces

**1. Surfaces: what is on screen, in immediate mode.** Views declare their
surfaces every frame, as ImGui draws them: `begin_frame()`, then `declare` /
`show(surface, rune_id)` / `focus`. A registry with a lifecycle
(register/unregister) goes stale the moment a window closes on one device and
not on another. A registry rebuilt every frame cannot go stale. A surface holds
**no geometry**, so a view whose layout is still changing (a builder, a calendar,
a map) never has to change its declarations when its layout changes. The canvas
declares only nodes that are **on screen**, because "who is looking at this" is
meaningless if it counts things nobody can see.

**2. Presence: what one peer says about itself.** Profile (id, name, colour,
avatar), selection **by rune id**, visible surfaces, focus. This is the wire
payload. Its content is Void Maiz vocabulary, and the transport carries it as
opaque bytes (`presence_to_json` / `presence_from_json`).

**3. The roster: everyone else.** Upserts by profile id, ignores this device's
own echo (LAN broadcasts come back), and forgets peers not heard from within a
TTL on the application's clock.

**4. The one question.** `ShareFilter`: may this rune leave this device? The
default is a tag convention (`net_tag::private_`). An Allomone rule can answer it
instead (`share_by_annotation`: `when tag "draft" -> share 0`). An application
with its own sharing model compiles that model into the same function.
`shareable_runes(scene, filter)` is the export set the sync layer is handed, so
**the sync seam and the presence seam cannot disagree about what is private**,
because both ask the same function.

**5. One renderer.** `draw_presence_mark` handles outline, badge and tint, and
`draw_private_mark` draws the padlock. Views join by shape:

| a view that draws… | joins with |
|---|---|
| ImGui items (rows, buttons, cards) | `presence_item` right after each one |
| custom geometry (markers, cells, blocks) | `presence_rect` with each rect |
| the node canvas | a `CanvasNet` passed to `edit_canvas` |
| a whole tab or window | `presence_surface_badges`, `presence_focus_if_active` |

# The rules the code enforces rather than recommends

1. **Presence is keyed on the immutable rune id, never the name.** A rename must
   not make someone vanish from what they are editing. `selection_ids` is the
   one conversion from the editor's name-keyed selection.
2. **The SENDER decides what is broadcast.** `SharePolicy` is applied in
   `compose_presence`, before the bytes exist. The receiver's `PresenceDisplay`
   only controls clutter; a filter on someone else's screen is not privacy. The
   settings UI draws the two as visibly separate groups, so nobody can hide
   avatars and believe they have hidden themselves.
3. **A rune that may not leave is never mentioned either.** Broadcasting "I have
   `x` selected" leaks that `x` exists. A selection the sender's scene does not
   hold is dropped too: presence never names a rune the sender cannot vouch for.
4. **Presence is ephemeral.** Nothing here writes the model, and **remote
   presence is never fed into the local UserGraph**. Q20's lean (attention is
   per-peer) is what keeps `with` meaning "coherent in MY work". Live presence
   shares attention now and records nothing, so there is nothing to un-share
   later.
5. **The codec refuses rather than truncates.** Presence arrives from another
   machine, so the receiver must not hold whatever it is sent. Oversized input,
   too many ids and overlong strings are rejected whole (`PresenceLimits`),
   because a truncated selection is a lie about what someone is doing.
6. **A conflict keeps a rune home.** When the share answer comes from Allomone
   and two sources disagree, the rune stays local. Un-sending is impossible, and
   the conservative answer only costs a delay until someone resolves the
   conflict.

# Networking and Allomone

Collaboration becomes a condition like any other:

    when present -> ring 1          # someone else has this selected
    when present "Bo" -> bo 1       # …that person specifically

`register_presence_predicates(preds, roster, &scene)`. **The id trap** is pinned
by a test. Presence is id-keyed, but many hosts key their Subjects on the rune
**name** (both of this library's own Allomone examples do). Compared directly,
the two silently never match. That would be the name-versus-id bug presence
exists to avoid, reintroduced one layer up. Passing the Scene resolves a name to
an id first.

And the one question, answered by the language:

    when tag "draft" -> share 0

**There are no bare booleans in Allomone.** `share false` does not parse, and a
rule that does not parse keeps nothing private. This session's own first draft
of the header example made exactly that mistake, and the test caught it only
because it checked that the scripts parse. That check is now pinned.

# Migrating an application that already networks

An application that built networking before this module existed has probably
built something good: Hormiga's LAN sharing works across two real operating
systems. The adoption is not about replacing a working mechanism for its own
sake. It changes **where decisions live**:

- **Per-view marking code is deleted.** Whatever outlined nodes in one view and
  badged rows in another becomes `CanvasNet`, `presence_item` and
  `presence_rect`. Every view then looks the same.
- **The presence payload becomes `PresenceState`.** "Which section am I in"
  becomes surfaces plus focus. Any sealing or encryption stays in the
  application's transport, since this module never sees bytes it did not produce.
- **Private tags become a `ShareFilter`.** If the application's sync seam
  and presence seam each check privacy themselves, they can disagree; with one
  filter they cannot.
- **A sharing-configuration model stops being where networking behaviour is
  decided, and becomes one way of answering the one question.** This is the
  largest change and the most important one. A model where sharing is *visible
  and wirable* is a genuinely good design: you can see what flows where, and
  every change to it is a command. It should be kept. What changes is that it
  compiles into a `ShareFilter` and `NetSettings` rather than deciding
  per-view behaviour. It can also compile into Allomone annotations, which makes
  it a peer of every other source rather than a special case.
- **The profile's presentation moves to `Profile`**, and its keypair stays with
  whatever transport the application has until Palabra's arrives.

# Staging

| stage | what | status |
|---|---|---|
| **A** | surfaces, presence, roster, codec, one renderer, canvas integration, Allomone bridge | **built 2026-09-18** |
| **B** | profile presentation, member list, avatars, the Networking settings section (sender and receiver groups, cautious files) | **built 2026-09-18** |
| **C** | `voidmaiz_net`: one replica, one Palabra session per link, the observe → persist → share → merge → splice loop, presence through Palabra's ephemeral channel into the Roster | **built 2026-09-19**; a real network transport waits on the trust model |

**Next, researched and not built:** the canvas in depth, meaning gestures in
flight over presence, reduction under concurrency, undo in a session and
routes as content. See [collaborative canvas](/concepts/collaborative-canvas.md)
(2026-09-20).

Stages A and B live in `voidmaiz` and `voidmaiz_view` and link nothing new. **An
application that never networks pays nothing**: every entry point is optional,
and `edit_canvas` behaves exactly as before when no `CanvasNet` is passed. Only
stage C creates the optional target, and it is the only thing that will require
Palabra.

# Stage C: what `voidmaiz_net` does that an application would otherwise get wrong

Void Palabra's `Session` is a pure state machine: it opens no socket and reads no
clock. `maiz::Network` is the same, one layer up. The application moves frames
between **links** (its word for "a connection to one peer") and passes the time
in. What `Network` owns:

- **The loop, in Palabra's required order**: observe, **persist**, share, merge,
  splice, persist. `persist` is a required callback. It runs before any frame
  carrying new tags can be taken, because a delta sent before the replica is
  saved re-mints those tags after a crash.
- **The splice.** Only `mantles` and `glyphs` are replaced, and **only when the
  merged slice actually differs** (an order-insensitive compare). The local
  state is observed again first, so an edit made since the last tick is
  recorded rather than overwritten. It uses `Core::replace_state`, which keeps
  the log sink, the effect handler and every registered glyph. Registered
  glyphs do not travel in an exported state (measured: `"glyphs":{}`), so
  without the replay the canvas would lose every node style on the first merge.
- **The undo cost, stated.** A splice starts the undo history over. Void Core's
  undo is memento-based, so a snapshot taken before a peer's changes would
  revert them on undo, and the next sync would send that revert to everyone.
  This is why the splice happens only on a real difference: **an idle peer
  never costs the user their undo.** Pinned both ways.
- **No phantom loop.** Observing a splice records nothing. If it recorded
  anything, every tick would mint changes and trade them with every peer
  forever. `net_smoke` runs ten simulated idle seconds after convergence and
  requires zero observed changes, zero splices and bounded traffic. A splice
  that fails to round-trip raises a `warn` note.
- **One answer to "may this leave?"**: the `ShareFilter` is Palabra's
  `ExportSet`, so sync and presence cannot disagree. A rune the Core does not
  hold is not vouched for; its *removal* still travels, as Palabra requires.
- **Presence keyed on the session's identity.** The Roster files a peer under
  the replica id its session established, never the id its payload claims, so
  a peer cannot speak as another by naming it (pinned).

## Guidance every networked application needs

**A device joining a shared document must receive its mantles, never create one
with the same name.** Palabra keys mantles by name, so two devices that each run
`mantle new team` mint two ids for one name and conflict on the mantle's `id`
on every merge. The first version of `net_smoke` did exactly that: three
devices, three `mantle new team`, and "1 conflict" reported on every merge until
the test was made realistic (one founder, two joiners who adopt what arrives).
An application's "join" flow must not seed the document.

**Compare documents canonically.** Two converged devices can hold one document
with different JSON key orders. `voidpalabra::slice_hash` is the right equality;
printed JSON is not. The test's first comparison reported a divergence that was
only `{"image","text"}` against `{"text","image"}`.

# What this does not prove

- No bytes have crossed a real network through this module. The demo's peers
  use loopback, and `net_smoke` uses an in-memory wire that drops 30% of frames
  (Palabra's own test covers reordering, duplication and partitions under the
  session).
- The avatar is always the initials disc. `Profile::avatar` is an asset
  reference the application resolves, and there is no texture path yet.
- Nobody has tested it on a phone. Presence on glass is a declaration on a
  bottom sheet (see [touch](/concepts/touch.md)), and that has not been drawn.
