---
type: Concept
title: Collaborative canvas
description: "Several people (and agents) editing one node graph over LAN, desktop and Android, demonstrated with Interaction Combinators. Three channels: COMMITTED (commands, synced by Palabra), IN FLIGHT (presence carries the staged half of every gesture), LOCAL (camera). The author ruled on 2026-09-20: no hard conflicts for view state (Palabra's read-time Latest), first-to-select claims bind people and agents alike (Lamport order, a person outranks an agent), and interaction nets are the strongest case rather than a trap (every concurrent-rewrite scenario has an answer; the shared-wire case is HVM2's wires-as-variables, sent to Palabra as research). Concrete UI/UX defaults for the author to test. Built: the in-flight payload, claims, the collaboration-aware network tick, presentational joins, and LAN platform facts including Android's multicast lock. Built 2026-09-21: wires as runes (Palabra SPEC 5.11) end to end, and stage N0: IC collaborates (Host/Join, remote playback) with a two-window duo bench. Not built: the N1 drawing (cursors, ghosts, claims on the canvas) and any real transport (Q36)."
resource: include/voidmaiz/presence.hpp
tags: [status:current, audience:dev, audience:host, confidence:measured]
timestamp: 2026-09-20T00:00:00Z
---

Opened 2026-09-20 on the author's brief:

> i want to specifically focus on the node graph for a bit … how colaboration in
> a node graph could look like with multiple people. because node placements
> matter, the way wires are routed matter. the entry fields and stuff, all of
> that matter and should be shared in real time … our demonstration will be with
> interaction combinators … it should be able to interact with both android
> devices and desktop devices over LAN.

[Networking](/concepts/networking.md) made networking uniform across **views**:
every view declares, one renderer marks. This page covers one view in depth,
the canvas, where the questions are harder because a node graph is spatial and
its gestures take time. A drag lasts a second. A wire is dragged across the
screen before it lands. A text field is edited for a while before it commits.

**Status (second pass, 2026-09-20): decided, and the foundations are built.**
The author answered the first pass the same day ("i trust your leans") with three
rulings that change it (§0). What is built is listed in §0.2. The canvas drawing
and the IC integration are not. Claims about what syncs were checked against code
and Palabra's `SPEC.md` (§5, §11). Claims about other products (Figma, tldraw,
Unreal Multi-User, Blender's multi-user add-on) are **recalled, not verified**,
and are used only as prior art for UX patterns.

# 0.0 What the second and third device tests changed (2026-09-22)

Three sessions of real use, and every one of them found something a test had
not. In order of how much they cost:

**A node made on two devices at once was one node.** `unique_name` minted
`gamma-1` on every device, and because a wire rune names its ends, one name for
two runes made every wire touching it ambiguous — so wires vanished on the peer
while the nodes themselves looked fine. That is why "make two nodes, then wire
them" worked and "drag a port out and make a node there" did not: the first
mints on one device only. Names are now scoped by a per-device tag
(`unique_name(scene, glyph, device_tag)`, `CanvasStyle::device_tag`), and
net_smoke asserts both halves: without a tag the duplicate name and the missing
wire appear; with one, both screens agree.

**Live physics is a rule of the mantle, not a switch on a device.** The author:
*"if live physics is turned on, it should be turned on for all synced devices.
rather than a constant update of position information."* Both halves are built.
A mantle already has `rules` in the Core, so the rule crosses as one ordinary
command — `{"rule":"physics","driver":"<device>"}` via
[voidmaiz/rules.hpp](../../include/voidmaiz/rules.hpp) — and **one** device
drives the simulation while the others receive the settled positions as ordinary
moves. Streaming positions was never needed; agreeing on a rule was.

**A long press on empty glass makes a node.** On touch the canvas opens the add
palette where the finger is rather than a context menu whose only useful entry
is that palette (`CanvasStyle::touch`, `EditorState::add_request` — which also
lets a host's own button open it).

**The phone had no keyboard**, so a tag could not be typed: `maiz::keyboard`
draws one, in the foreground draw list so it survives a modal, hit-testing its
own keys and swallowing the touch so the field it types into stays focused.

**Tag suggestions are Maiz's now** ([tags.hpp](../../include/voidmaiz/tags.hpp)),
ported from Void Hormiga on the author's call that every Void application should
have them.

# 0. The author's rulings, and what they changed

1. **"i dont really like hard conflicts, because they slow productivity … i don't
   see an issue with last one wins."** View state now converges silently. It uses
   Palabra's `JoinPolicy::Latest` (built at our request 2026-09-20: a read-time
   projection that shows the Lamport-latest write, the same on every peer, with
   no wall clock; it was `Pick` for one day before that existed), declared on `placement`,
   `content.pos`, `content.size`, `content.collapsed` and `content.route.*` by
   `presentational_joins()`, the default of `NetOptions::joins`. §6 is rewritten.
   **"Last" in the honest sense:** a write made after *seeing* another already
   replaces it (Palabra's register retires the tag it observed), which is
   last-writer-wins for everything a person can perceive as sequential. Only
   truly simultaneous writes need an arbitrary but shared pick. Palabra refuses
   wall-clock LWW on principle, and a Lamport-ordered `Latest` has been proposed to
   them as the honest way to make that pick "latest" (message of 2026-09-20, §5).
2. **"whoever selected this specific port on the node first, is the one who is
   doing stuff with it … agents don't drag wires, they simply assign them."**
   Claims are now the one mechanism for people **and** agents. Selecting (or
   starting a gesture) claims; an agent's assign must claim first and is refused
   by `Claims::gate` if someone holds it. "First" is a **Lamport order**, so a claim
   made after seeing another always loses. Ties are broken the same way on every
   device, and a person outranks an agent. §3.3 is updated.
3. **"i would not like to look at interaction nets as a trap, but rather as our
   strongest area."** Correct, and the research backs it: every concurrent-rewrite
   scenario the author listed has an answer (§4.2 is rewritten). The shared-wire
   case, which the first pass called a trap, is solved in HVM2 §5 by treating wires
   as variables, and that translates directly onto Palabra's registers. That
   research is Palabra's to lead, and the message is sent.

Also ruled: **Android LAN must exist** ("if it doesn't exist, it SHOULD"). The
platform facts are ours (`voidmaiz/lan.hpp`, built). **Palabra declined the socket
layer on 2026-09-20** (the author leans toward Palabra not owning Android
networking) and built everything a transport must agree on instead (the stream
envelope, `StreamReader`, coalescing). Where the sockets live is **Q36**, and it
gates stage N3 only. The
author also accepted every Q31–Q35 lean, so those questions are closed.

## 0.1 UI/UX decisions, made so the author can test them

The author: *"you can just make some decisions for the UI/UX part of things. then
in testing i'll let you know weather or not those were good decisions."* These are
the defaults. Each one is a row in the
[user testing guide](/testing/collaborative-canvas-user-tests.md), so a verdict
lands on a specific number.

| element | decision |
|---|---|
| remote cursor | arrow in the peer's colour, name tag beside it. The tag fades after **2 s** still and returns on movement. **Sent at 20 Hz** (10 Hz from a phone), drawn **100 ms behind** and interpolated, never extrapolated |
| phone peer | a **fingertip dot** instead of an arrow, and only while a finger is down |
| selection mark | the existing outline, **plus a dashed ring**, so colour is never the only signal (IC's red, yellow and blue pigments would swallow a same-coloured outline) |
| drag ghost | peers' in-flight moves drawn as translucent copies (**35 % opacity**, peer colour) at the offset. The real node does not move until they release |
| pending wire | the peer's wire from port to cursor, **dashed**, peer colour |
| held node | thicker ring, peer colour, small hand glyph |
| grabbing a held node | the drag does not start. A toast reads "**Gary is moving this**". **Hold 600 ms** to take it anyway (on glass, a long press opens the menu with **Move anyway**). Taking it is logged |
| a claim lost to a concurrent claim | your gesture cancels, and a toast names who got there first. Your staged drag snaps back over **150 ms** |
| off-screen peers | an avatar chip on the canvas edge pointing toward them. **Click to jump** (a one-shot glide, 300 ms) |
| follow | click a name in the member strip. Any camera gesture of your own stops it, and "following Bo ×" shows while active |
| remote changes | moved nodes **tween 250 ms**. Removed nodes shrink and fade, added nodes grow in. A remote reduction plays IC's own rewrite animation in the author's colour. Steps arriving faster than they can play: **skip to the latest** |
| who changed it | a remotely changed node glows in the changer's colour for **1.2 s** (needs Palabra's writer read, message §6; until then, the colour of whoever the diff's session delivered) |
| typing | a caret glyph in the typist's colour on the field, with the staged text shown **in italics** beneath it (sender can turn preview off) |
| text conflict | the field shows both values side by side with the authors' colours, and a click keeps one. Never a modal |
| ping | Alt+click (desktop) or two-finger tap (glass): a ripple in your colour, **1.5 s**, flashing the edge chip for off-screen viewers |
| status pill | *offline* / *looking for others* / *syncing* / *in sync with N* / *N to resolve* |
| join | Host / Join, two buttons. Join lists what the network announces and offers **Join by code** with a digits keypad (the code is `maiz::lan::encode_join_code`) |
| reduction | a step claims its redex plus boundary. **Reduce all** takes the **crank**, and while Bo holds it everyone else's step buttons read "**Bo is reducing**" with a *Request* button that pings Bo |
| undo in a session | Ctrl+Z undoes **your own** last gesture with a compensating command. If someone has since changed it: a toast, "**Bo changed γ-3 since**", and nothing happens |

## 0.2 What is built (2026-09-20, second pass)

| piece | where | pinned by |
|---|---|---|
| gestures in flight: `CanvasPresence` (cursor, view rect, move offset, pending wire, marquee, resize, typing with preview, ping), `Participant`, `device`, `compat`, Lamport `clock`, `recent` commands. Codec refuses hostile input whole (bounds, shapes, counts, NaN) | `presence.hpp` | `collab_smoke` |
| rule 3 applied to everything in flight: a gesture, typing, claim or recent line naming a private (or unknown) rune never leaves | `compose_presence(…, CollabOut)` | `collab_smoke`, `net_smoke` |
| claims: first to select wins (person over agent, then Lamport stamp, then id), port/field/whole-rune overlap, `yield`, and `gate` for agents | `claims.hpp` | `collab_smoke` |
| the network carries all of it through a real Palabra session, including mantle-wide claims (the crank) | `Network::tick(…, CollabOut)` | `net_smoke` |
| view state converges instead of conflicting, while content still conflicts | `NetOptions::joins` = `presentational_joins()` | `net_smoke` |
| LAN platform facts: interfaces ranked for LAN, subnet maths, the join code, Android's multicast lock through JNI (opt-in), the manifest permission list | `lan.hpp`, target `voidmaiz_lan` | `lan_smoke`. The Android half is compiled with the NDK (arm64), not yet run on a device |
| **wires as runes** (Palabra's normative encoding): classes read from a projected scene, collapsed into node-to-node wires for drawing (`via`, `contested`), and the attach, detach, fuse and segment commands | `wires.hpp` (2026-09-21) | `wires_smoke`; `net_smoke` runs the §4.2 case end to end: two partitioned rewrites sharing a wire, 30 % loss, converge on `a2–c2` with Palabra's `check_links` clean, **and** the plain-edge contrast loses the wire |
| view state resolves by **Lamport-latest** (Palabra's `FieldJoin::Latest`, built at our request) | `presentational_joins()` (2026-09-21) | `net_smoke` |
| **IC on wire runes**: the canvas writes wire gestures through `CanvasStyle::wires` (`reified_writer`); IC projects then collapses; reduction reads the drawn net (`reduce::to_net(Scene)`); a step inherits every boundary by a new segment **fused** onto the old wire; old projects and the starter are upgraded once (`compile_upgrade`) | `wires.hpp`, `canvas.hpp`, `reduce.hpp`; IC `app.cpp` (2026-09-21) | `wires_smoke` (the upgrade leaves the net identical, checked against the mantle reader) |
| **stage N0: IC collaborates.** Solo / Host / Join roles (a joiner starts EMPTY and adopts, E12); the actor comes from the profile (E19); device-scoped names for segments and minted agents (E13); the status pill; remote changes play (a remote step runs the rewrite animation, anything else tweens 250 ms). **The duo bench**: two windows, one process, the real Palabra session, a simulated link with latency, loss and a partition switch | IC `app.cpp`, `main_duo.cpp` (2026-09-21) | `interaction_combinators_duo --selftest`: join converges, a remote step follows, **two partitioned steps heal into one valid net**, normal form identical, zero anomalies, conflicts or broken wires. Seen on screen: both windows render the same net, "in sync with 1" |
| the IC APK declares the four LAN permissions | `InteractionCombinators/android/AndroidManifest.xml` | the APK builds (2026-09-21) and `aapt dump permissions` lists all four; not yet installed on a phone |

**Not built:** everything drawn (cursors, ghosts, chips, rings, playback,
conflict-on-node), IC's claims, crank and integrity check, the compensating undo,
and any real transport.

# 1. What already syncs, and how it behaves when two people touch it

Every canvas gesture already compiles to a command (commitment 2), and
`voidmaiz_net` already syncs the resulting document. So the first question is
not what to build but **what the existing commands do when they race**. The
answer comes from Palabra's enriched document (SPEC §5.2): a rune's fields are
**multi-value registers** (a concurrent write is a *conflict*, never
last-writer-wins), its tags and a mantle's edges are **observed-remove sets**
(add-wins), and runes are keyed by the immutable `spirit.id`.

| what the user does | the command today | Palabra structure | two people at once |
|---|---|---|---|
| drag a node | `setjson <name> pos [x,y]` (`compile_move`) | `content.pos` register | **conflict**: two positions, a question to a person |
| drag two different nodes | same | two registers | clean |
| resize a face | `setjson … size` | register | conflict |
| collapse | `set … collapsed` | register | conflict (same value on both sides = no conflict) |
| edit a face field | `set`/`setjson <key>` on commit | **one register per key** | different keys: clean. Same key: conflict |
| add a tag / pigment (IC) | `tag <name> +red` | tag OR-set | **clean, and meaningful**: `+red` and `+blue` concurrently give purple |
| wire two ports | `link a b --relation i:j` | edge OR-set (value = whole edge) | both wires survive (add-wins) |
| rewire (drag a wire end) | `batch` unlink + link | remove + add in the edge set | both new wires survive, the old one is gone |
| delete a node | `rm` | `present` OR-set | delete vs a concurrent edit gives `deleted_while_edited` |
| add a node | `rune new <glyph> <name>` | new rune, random `spirit.id` | if both pick the same **name**: `duplicate_name` anomaly |
| rename | `rename` (repoints edges) | `spirit.name` register | concurrent link to the old name gives `link_broken (renamed)` |
| IC reduction step | ONE `batch`: rm, rm, `rune new …`, move, link | all of the above | see §4. This is where the demo lives or dies |
| camera, panel sizes, anim speed | `config set view.*` | **not synced** (config is peer-local) | already correct: everyone keeps their own camera |

Three things follow before any new UI:

- **Moves are the most frequent write and the one most likely to conflict.**
  Putting a "which position?" dialog in front of a person is absurd, so
  presentational fields need a resolution policy (§6).
- **Names are the weak point.** Edges address runes by *name* (Void Core
  SPEC §3.7). IC picks names by first-free local search (`γ-1`, `γ-2`, …), so
  two peers adding or reducing at the same moment choose the same name for two
  different runes. That is the most likely anomaly in the demo (§4.3).
- **Edge values are whole edges.** Anything stored *on* an edge (a route, a
  label) turns into remove plus add, and two concurrent edits leave two wires.
  Nothing that changes often may live on the edge value (§7).

# 2. The organising idea: three channels

Everything below sorts into exactly one of three channels. Keeping them apart is
what preserves the founding commitments.

| channel | carries | transport | persisted | in the log |
|---|---|---|---|---|
| **committed** | commands and the model they change | Palabra `doc` frames (the state document) | yes | yes, as commands (local) or merge notes (remote) |
| **in flight** | gestures that have not committed: cursor, drag ghost, pending wire, marquee, typing, claims, pings | Palabra `presence` (opaque, 16 KB, newest-wins, TTL) | **never** | no. A gesture that never commits changed nothing |
| **local** | camera, panels, theme, display toggles | none (`config`, device settings) | on this device | yes, as `config set` |

**Presence is where gestures live before they are commands.** Today presence
carries only who you are, what you have selected and which surfaces you have
open. The largest single addition this page proposes is to let it carry the
**in-flight half of every canvas gesture**, because the canvas already stages
each gesture locally (`EditorState::staged`, the wire drag, the marquee, face
widget staging) and commits one command on release. The staged state is already
there. Collaboration only asks us to *show* it to the others.

This keeps all three commitments intact:

- **Commitment 1 (no truth in Void Maiz).** Presence is ephemeral and never
  merged (Palabra §11.6). A drag ghost on a peer's screen is as much a
  projection as the local drag preview.
- **Commitment 2 (every gesture is a command).** The rule was always "every
  gesture that *changes anything*". A drag in progress changes nothing, and on
  release it becomes exactly one command, visible in the log on the device that
  made it, then synced.
- **Presence rule 4 (never fed into the local UserGraph).** Unchanged. Remote
  gestures are drawn and then forgotten.

**Presence gains geometry, but only in the canvas's own section.** Networking
deliberately keeps `SurfaceDecl` free of geometry, so a map or calendar never has
to declare coordinates. That rule stands. The canvas adds an *optional,
surface-scoped* block, the only place world coordinates appear:

```cpp
struct CanvasPresence {             // one per canvas surface, optional
    std::string surface;            // which canvas (and which mantle: boxes nest)
    float cursor_x, cursor_y;       // WORLD coords; absent while a phone has no finger down
    float view[4];                  // the peer's visible world rect (minimap, follow)
    enum class Gesture { None, Move, Wire, Marquee, Resize, Typing } gesture;
    float dx, dy;                   // Move: ONE offset for the whole selection (ids already in presence)
    std::string wire_from; int wire_port; // Wire: the fixed end; the free end is the cursor
    float marquee[4];
    std::string field_rune, field_key;    // Typing: which field is being edited
    std::string preview;                  // Typing: staged text, capped, share-filtered
    std::string claim;                    // IC: "crank", or a redex claim (§4.4)
    Ping ping;                            // "look here": world x,y and a sequence number
};
```

A Move is sent as **one offset**, not a position per node. The staged drag is a
uniform translation of the selection, and the selection ids are already in the
payload. A 300-node drag therefore costs about 20 bytes. Snapping, the one case
where the translation is not uniform, sends the snap target and lets the
receiver draw the snap preview it already knows how to draw.

**Rate.** Cursors want roughly 20–30 Hz on the sender, interpolated on the
receiver (render about 100 ms behind and lerp; draw nothing rather than
extrapolate). Phones send at half the rate and stop entirely when no finger is
down. Palabra already keeps only the newest presence by `seq`, so a lost cursor
update costs nothing.

# 3. The UI/UX catalogue

Tiers match the [backlog](/backlog.md): **T1** is needed for the IC demo to feel
collaborative, **T2** makes it good, **T3** is polish. Each entry names its
channel and **where it lives**. "Library" means `voidmaiz_view` or `voidmaiz`
and serves every canvas host. "IC" means the demo application.

## 3.1 Awareness: who is here and where they are

| | element | channel | where | tier |
|---|---|---|---|---|
| A1 | **Remote cursors**: an arrow in the peer's colour with a name tag. The tag fades after about 2 s still and returns on movement | in flight | library (`CanvasNet`) | T1 |
| A2 | **Selection outlines**: already built (`Mark::Outline`) | in flight | library, done | — |
| A3 | **Member strip with device badge**: avatars top-right, a small glyph for desktop, phone or headless agent. Tap to jump or follow | in flight | library (`draw_member_list` grows a compact form) | T1 |
| A4 | **Off-screen indicators**: an avatar chip pinned to the canvas edge, pointing toward a peer whose cursor or view is off screen. Answers "where is everyone" without a minimap. It is also the canvas answer to Hormiga's "somebody is somewhere in these 400 rows" ask | in flight | library | T1 |
| A5 | **Follow**: click an avatar and your camera tracks their view rect (fit to your aspect, smoothed). Any camera gesture of your own stops it. The follow state is shown ("following Bo ×") | in flight + local | library | T2 |
| A6 | **Jump to**: a one-shot follow | same | library | T1 (almost free once A5's math exists) |
| A7 | **Minimap with view rects**: each peer's visible rect drawn in their colour | in flight | library | T2 |
| A8 | **Ping ("look here")**: Alt+click, or a two-finger tap on glass, drops a ripple at a world point in your colour for about 1.5 s on everyone's canvas. Off screen, it flashes the A4 edge chip. For teaching IC ("watch this pair") it is worth more than voice | in flight | library | T2 |
| A9 | **Summon**: ask everyone to come to your view. A toast with Go / Dismiss, never a forced camera move (consent: nobody's camera is moved by someone else) | in flight | library | T3 |
| A10 | **Focus scope**: "Bo is inside box `adder`". IC can double-click into box mantles, and the surface is per mantle, so the focus tells you which | in flight | library | T2 |

## 3.2 Gesture ghosts: seeing a gesture before it lands

| | element | where | tier |
|---|---|---|---|
| G1 | **Drag ghost**: peers' in-flight moves drawn as translucent copies at the offset, in their colour, with the real node staying put until commit. Showing the *real* node moving would claim a model change that has not happened (and may be cancelled) | library | T1 |
| G2 | **Pending wire**: the peer's rubber-band wire from their fixed port to their cursor, dashed, in their colour | library | T1 |
| G3 | **Marquee**: the peer's selection rectangle, outline only | library | T3 |
| G4 | **Typing indicator**: a caret glyph and their colour on the field being edited. Optionally the **staged preview text** in italics, so a co-editor sees words appear before commit. It is capped, and the SENDER's share filter applies (rule 3: a private rune's text never leaves) | library (face widgets and inspector) | T2 |
| G5 | **Add-box / context menu open**: "Bo is adding a node here", a small palette icon at their cursor | library | T3 |
| G6 | **Resize ghost** | library | T3 |

## 3.3 Soft claims: avoiding conflicts instead of resolving them

> **Built 2026-09-20 (`voidmaiz/claims.hpp`), and extended by the author's
> ruling: claims bind agents too.** "whoever selected this specific port on the
> node first, is the one who is doing stuff with it." Selecting a port or node
> claims it. An agent that assigns without gesturing must `acquire` first, and
> `Claims::gate` refuses its command, naming the holder, if someone else holds
> it. "First" is a Lamport order carried in presence: a claim made after *seeing*
> another always loses, a simultaneous pair is ordered identically on every
> device, and a person outranks an agent. The loser learns through `yield`, and
> its gesture cancels with a toast naming the winner.

Palabra's merge can report a conflict but cannot prevent one. Most conflicts in
a live session are avoidable, because a person can *see* that someone else has
hold of a thing. A **claim** is presence saying "I am doing something to X right
now". It is advisory and never enforced by the merge.

- **C1, the held ring** (T1): a node inside a peer's in-flight gesture (G1, G2,
  G4) draws a thicker ring in their colour with a small hand glyph.
- **C2, grab friction** (T1): pressing on a held node does not start a drag.
  Instead a tooltip or toast says "Bo is moving this". Press and hold for 600 ms
  to **take it anyway**, which is a local decision, logged as a note. On glass
  the long press is already a right-click ([touch](/concepts/touch.md)), so the
  override is a menu item: "Move anyway".
- **C3, field claims** (T2): a field someone is typing in shows their caret, and
  focusing it yourself shows "Bo is editing. Edit anyway?". Most concurrent text
  conflicts disappear once people can see each other typing.
- **Race window.** Two people can press the same node within one presence
  round-trip (about 30–100 ms on LAN). Claims make conflicts rare, not
  impossible, so §6's policy still has to exist.

## 3.4 Remote changes: playback, not teleportation

When a merge splices, the canvas today re-projects and the picture **jumps**.
Nodes teleport, agents vanish, new ones pop in. For one person that is fine,
because they caused it. For a collaborator it is disorienting, and in IC it
throws away the rewrite animation, which is the demo's best moment.

- **R1, diff playback** (T1): on `take_spliced()`, diff the old and new Scene
  (by id): moved nodes tween over about 250 ms, removed nodes fade and shrink,
  added nodes fade in, wires cross-fade. It is built on the existing `CanvasFx`,
  which already does exactly this kind of view-only override. A `SceneDiff`
  (UI-free) plus `fx_from_diff` (view) would be about 150 lines in the library.
- **R2, remote rewrite animation** (T1, IC): a diff that removes an **active
  pair** and mints agents is a remote reduction step. IC recognises that shape
  and plays its own `StepAnim` exactly as for a local step, tinted in the
  author's colour.
- **R3, attribution flash** (T2): a remotely changed node briefly glows in the
  colour of the peer who changed it. **The attribution is already in the
  document.** Palabra tags are `<replica id>_<n>` (SPEC §5.7), so the tag behind
  a register's live value names the replica that wrote it, and the session knows
  which replica is which peer. This needs a small read API from Palabra (an
  upstream ask, §9) and no protocol change.
- **R4, "while you were away"** (T2): on reconnect (a phone coming back from the
  background), keep the pre-splice scene and tint everything that changed,
  fading on first view or after 10 s. A late joiner gets "N changes since you
  last saw this net".
- **R5, queue collapse** (T1): if remote steps arrive faster than animations
  play (a peer running "reduce all"), skip to the latest diff rather than
  queueing a minute of animation.

## 3.5 Conflicts and anomalies, drawn on the canvas

`draw_conflicts` and `draw_anomalies` exist as **lists**. On a canvas they
belong on the node:

- **K1** (T1): a node with an open conflict draws a **split ring** in both
  authors' colours (R3's attribution) with a small "?" badge. Clicking it opens
  the chooser *in place*.
- **K2, position conflicts are shown, not asked** (T1, if Q33 goes the other
  way): the node draws at *both* positions, one solid and one ghosted, joined
  by a dotted line. Clicking a ghost picks it. Nobody reads coordinates.
- **K3, deleted-while-edited** (T1): the node is drawn as a translucent
  tombstone with **Restore** and **Let it go**.
- **K4, anomalies** (T1): `link_broken` draws the surviving half of the wire as a
  stub ending in a red ×. `duplicate_name` puts a "2" badge on both nodes and
  offers **Rename one**, which becomes a `rename` command.

## 3.6 Activity and the log

- **L1** (T1): merge notes already reach the log strip. Make them readable:
  "Bo · step γ-2 ⋈ δ-1 (−2 +4)", "Bo · moved 3 agents". This comes from R1's
  diff summary, with the author from R3.
- **L2** (T2): **the peer's own command lines**, sent in presence as a short
  ring buffer of their last few commands, composed on the sender and
  **share-filtered** (a command naming a private rune is dropped, rule 3). This
  is commitment 2 carried across the wire: you see what the other person
  *typed*, not only what changed. It fits Void Hormiga's 2026-09-20 console
  proposal (a per-source tag and colour), with each peer as a source.

## 3.7 Session chrome

- **S1** (T1): **Host / Join**, two different flows, because a joiner must never
  seed (§8, E12). Host shares the open net. Join lists discovered sessions and
  opens an **empty** document that fills from the first merge.
- **S2** (T1): a **status pill**: *offline* / *discovering* / *syncing* /
  *in sync with 2* / *1 conflict*. `LinkStatus::in_sync` already exists.
- **S3** (T1): **pairing**: the six-character short authentication string on
  both screens, and Allow / Deny on the host (Hormiga's flow, which the author
  already approved for LAN).
- **S4** (T2): **version mismatch banner**: peers exchange app version and a
  **reduce-spec hash** in presence. A mismatch warns, and disables reduction
  with that peer (E17).

# 4. Interaction combinators under collaboration

IC is a better demo than it first looks, and more dangerous than it first looks,
for the same reason: it has a **reduction engine**, which a plain node editor
does not.

## 4.1 The good news: the maths is on our side

- **Locality.** A step touches only the redex and its wires. Two people working
  in different regions of a net never interfere.
- **Strong confluence.** Reduction order does not matter: same normal form, same
  step count. "Who reduced first" is a question with no consequence, *as long as
  the merge composes the steps correctly* (§4.2 is where it does not).
- **Pigments are tags, and tags are an add-wins set.** Two people tinting one
  agent at once (`+red`, `+blue`) merge to *both tags*, and the invisible
  pigment net mixes them into purple. A concurrent edit resolved by the
  application's own interaction net is the demo's best single moment:
  collaboration composing through the thing being demonstrated.

## 4.2 Two redexes that share a wire, and why it is solvable

> **Reframed 2026-09-20 on the author's direction:** this is not a trap. It is
> the one case where a naive CRDT loses what the mathematics guarantees, and the
> fix is known. **Every scenario the author listed has an answer:**
>
> | scenario | answer |
> |---|---|
> | **overlap**: Gary fires A–B, Mike B–C, John C–A | impossible in an interaction net: one principal port per agent makes it ONE redex and two non-interactions. In a general rule system it is a critical pair: first claim wins, and the loser's rewrite is void, never half-applied |
> | **duplicated**: the same redex fired twice | idempotent once minted agents take redex-derived ids (identical bytes merge to one value) |
> | **gone**: my redex's agent was consumed by someone else | my step's precondition failed, so it is void |
> | **split / transformed**: disjoint redexes sharing a wire (below) | commute once **a wire is a variable** with two end-slots, each written only by the rewrite that consumes the agent at that end. One principal port means one writer per slot, so there is no conflict, and the joined wire is read by chasing (HVM2 §5, *Substitution Map & Atomic Linker*) |
> | **missing**: an edit arrives before the agent it names | already handled by Palabra (§5.3: an edit may land before its creation) |
>
> The full argument, the literature (Lafont; DPO parallel independence and
> Critical Pair Analysis; HVM2; IPA and Explicit Consistency; Kleppmann's move
> operation) and the open design choices went to Palabra:
> `../VoidPalabra/MESSAGE_FOR_VOIDPALABRA_maiz-concurrent-rewrites-are-our-strongest-case-2026-09-20.md`.
> IC will prototype the wire-as-variable encoding (as runes, needing no format
> change) so Palabra has evidence.
>
> **Answered 2026-09-20 by Palabra, and built here 2026-09-21.** Palabra made
> *wires as runes plus `"="` fusion links* (the partition-lattice form, not the
> chase) the **normative** encoding, with a generic `check_links` (SPEC §5.11).
> The fusion form needs no one-writer argument, so it works for any rule system.
> Void Maiz's half is `voidmaiz/wires.hpp`, and `net_smoke` measures the case
> below end to end. **The known cost:** segments are never removed (removing one
> could split a class another peer is fusing onto), so long reductions accumulate
> wire runes. Safe compaction needs a point every peer has passed, the same open
> problem as Palabra's tag growth.
>
> What follows is the original first-pass description of the failure, kept
> because it is the precise statement of what the encoding must fix.

Take two disjoint active pairs, `(a,b)` and `(c,d)`, where an **auxiliary wire
joins `a` to `c`**. The redexes are disjoint, so mathematically they commute and
the combined result wires `a'` (from step 1) to `c'` (from step 2).

As **edits**, they do not commute:

- Peer 1 steps `(a,b)`: removes the edge `a.1–c.2`, adds `a'.x–c.2`.
- Peer 2 steps `(c,d)`: removes the same edge, adds `a.1–c'.y`.
- The merge keeps both additions. `a'.x–c.2` points at a removed `c`, and
  `a.1–c'.y` points at a removed `a`. That is two `link_broken` anomalies and a
  net missing the wire `a'.x–c'.y`, which no peer ever wrote.

**Strong confluence is a property of the net, not of the edit operations.**
Palabra's merge is correct: it composed two edits that each knew half the story.
The broken net is semantically wrong, reported loudly as anomalies, and fixable,
but a demo that routinely produces it is a demo of the wrong thing.

## 4.3 The other traps

- **Name minting.** `compile_step` names minted agents `glyph-k` by first-free
  local search, and `rune new` mints a **random** `spirit.id`. Two peers stepping
  anything at the same moment, or adding any node, very likely pick the same
  name for different runes, which gives `duplicate_name` and, because edges
  address names, **ambiguous wires**.
- **The same redex, fired twice.** Two people click the same active pair. Each
  removes `a` and `b` (fine, since removal is idempotent) and mints four agents
  with **different random ids**. The merge holds eight new agents, two of every
  wire, and a net that means nothing.
- **Auto-reduce on two devices.** "Reduce all" on two peers at once is the
  worst case of both traps above.
- **Live physics.** A settle is one batch of moves over many nodes. On two
  peers it conflicts on every node both touched, and on one peer it moves nodes
  out from under another person's drag.
- **Per-agent privacy breaks a net.** A private agent's edges are withheld too
  (Palabra §11.5), so peers see its ports as **free** and may wire them, which
  after the merge puts two wires on one port. An interaction net with holes in
  it is not a net.
- **Port single-occupancy is IC's rule, not Palabra's.** Any concurrent rewire
  of one port (two people each wiring a principal to something) leaves two wires
  on the port after the merge. No Palabra anomaly exists for it. It is a domain
  invariant a merge can break (SPEC §5.8's category), so **the application has
  to check it**.

## 4.4 What to do about reduction

Four layers, cheapest first. The demo needs the first two, and the third makes
it convergent by construction.

1. **Integrity check after every merge (IC, T1).** After `take_spliced()`,
   validate the net: every port holds at most one wire, and no wire has a
   missing endpoint. Violations draw as K4 anomalies with repair offers. This
   costs little and catches every trap above, including ones this page has not
   thought of.

2. **Claims on reduction (IC, T1).** Before a step, claim the redex **plus its
   boundary** (the agents at the other end of the redex's aux wires) in
   presence. A step is allowed only if no peer's claim intersects. Two steps
   conflict exactly when one's redex touches the other's boundary, which is the
   §4.2 case. "Reduce all" takes **the crank**: a single token meaning "I am
   driving reduction". While Bo holds it, everyone else's step buttons read
   "Bo is reducing" (with a *Request* that pings Bo). Simultaneous claims are
   broken deterministically (lowest replica id wins), and the loser's canvas
   releases within one round-trip. A person's claim outranks an agent's, as in
   [headless](/concepts/headless.md).

3. **Derived ids for minted agents (upstream, T2): makes the same-redex case
   converge.** Void Core's SPEC §3.1 already carves out an exception: *agents
   minted inside a reduction are derived from the redex so that reduction is
   reproducible across peers*. IC does not use it, because its step is a `batch`
   of plain `rune new`, which always mints randomly. If the step minted with
   derived ids **and** derived names (and deterministic placement from the
   redex's positions), two peers firing the same redex would write
   **byte-identical** runes and edges. Palabra would see the same value added
   under two tags and report no conflict: **concurrent identical reductions
   become idempotent under merge.** This needs either Core's `reduce` to
   single-step and commit into the live mantle, or `rune new` to accept a
   derived id inside a reduction. That is an ask to Void Core, not an edit.

4. **Deterministic re-knotting (research, T3): would make §4.2 converge too.**
   With derived ids, every peer can recompute, for a consumed agent's port, the
   port it became. A post-merge pass that finds the two dangling halves of
   §4.2's wire and joins them, `a'.x–c'.y`, computes the *same* repair on every
   peer, so both write it and the writes converge. Then concurrent reduction
   anywhere in the net would be merge-safe, which **no general-purpose node
   editor can claim**, because none has a confluent rewrite system underneath.
   It is worth one careful attempt after layers 1–3 exist, and it is the most
   original thing on this page.

**For privacy, IC should offer mantle-level privacy only** (a private scratch
net, `share_mantle` returns false) and hide the per-agent padlock. A net with
invisible agents is not a net.

**For physics:** only the crank holder may run live physics, and physics never
moves a node another peer has selected or claimed.

# 5. Undo in a shared session

Every splice that changes the document **clears the undo history**
(`Core::replace_state`). That is correct: Void Core's undo is memento-based, so
an old snapshot would revert a peer's work, and the next sync would send the
revert to everyone. But in a live session with one other active person, merges
arrive every few seconds, so **undo effectively stops working**. That is the
largest UX regression collaboration introduces, and it is invisible in a
two-device test where only one person edits at a time.

**Proposal: a local history of compensating commands (library, T1).** The
gesture compiler already knows each gesture's inverse at the moment it compiles
it: a move's old position, a link's unlink, an add's `rm`, a delete's re-add
(content, tags and wires captured at delete time), and a step's inverse batch.
Keep a per-device stack of `{forward, inverse, precondition}`. Undo **dispatches
the inverse as a new forward command**. It is logged, syncs to peers like any
edit, and never touches anyone else's work.

- **Precondition:** undo applies only if the things it touches still hold the
  values the forward command wrote. If Bo has since moved the node, undoing my
  move is refused with "Bo moved γ-3 since". An undo that silently overwrote a
  collaborator's later work would be worse than no undo.
- A step's inverse is refused once any minted agent has been touched or reduced
  further. Undo is local to *my* acts, which is what people expect in every
  multiplayer editor.
- This does not conflict with Void Core's undo. Solo, Core's undo keeps working.
  In a session, Ctrl+Z routes to the local history. It interacts with **Q15**:
  if moves leave undo (`place`), they also leave this history, and that
  decision should be made once for both.

# 6. Presentational conflicts: a declared policy, not a dialog

Palabra's rule is that a conflict surfaces and never resolves itself, and for
*content* that is right. For **presentation** (position, size, collapse, route)
a person cannot meaningfully answer "which of these two coordinates?".

**Decided and built (2026-09-20, second pass).** The first pass proposed that
`voidmaiz_net` *write* a deterministic resolution for each presentational
conflict. That turned out to be unnecessary, because Palabra had already built
the better version: **`JoinPolicy`, a read-time projection.** A field declared
`Pick` keeps both values in the document, every peer *shows* the same one, and no
write happens. So there is no resolution traffic, no ping-pong, and changing the
policy later re-resolves old data correctly. `NetOptions::joins` defaults to
`presentational_joins()`: Palabra's `core_defaults()` plus `Pick` on
`content.pos`, `content.size`, `content.collapsed` and `content.route.*`. Pinned
by `net_smoke` (a partitioned drag converges with no question, and a partitioned
*text* edit still asks on both devices).

The honest limit: `Pick` is deterministic, not "latest". A move made after seeing
the other already wins outright (no conflict exists). Only two moves made within
one round-trip of each other fall back to the arbitrary pick, and claims (§3.3)
make that rare: you cannot grab a node someone else is dragging without the
deliberate override. A Lamport-ordered `Latest` is proposed to Palabra.

# 7. Wire routes as shared content

The author named routing explicitly: *"the way wires are routed matter."* Today
routes are **computed** (tangents, [Q27](/developer_questions.md)), so they are
identical on every screen automatically and nothing needs syncing. Syncing
becomes necessary the moment a person can *shape* a wire, and three storage
options exist:

| option | concurrency | fit |
|---|---|---|
| waypoints on the **edge value** | **wrong**: edge values are whole OR-set elements, so an edit is remove plus add, and two concurrent edits leave **two wires** | rejected |
| **reroute runes** (Blender-style pass-through nodes) | each is an ordinary rune, so moves conflict per waypoint and sync for free | right for dataflow hosts (backlog T2). **Wrong for IC**: a reroute would be an agent in `to_net`, unless the net reader contracts it away |
| `content.route.<port>` on the endpoint rune: a waypoint list keyed by the port, **stamped with the partner** (`{to: "δ-1:0", pts: [[x,y],…]}`) | one register per port, so concurrent edits to one wire give a presentational conflict (§6) | **right for IC**: port single-occupancy means a port names its wire. The partner stamp makes a stale route (the wire was rewired concurrently) **ignore itself** instead of drawing a wrong path |

**Lean (Q35):** per-port routes for IC and any host with single-occupancy
ports. Reroute runes stay the general answer for fan-out graphs. Every gesture
that removes a wire clears its route in the same batch, and a rewrite removes
routes with the agents that held them. Minted wires start straight, which is
honest, because nobody drew them.

# 8. Edge cases, specific to this stack

Numbered so the build can pin them as tests. **(T)** marks the ones
`net_smoke`'s in-memory mesh can reproduce today.

**Sync semantics**

- **E1 (T)** Concurrent drag of one node: `content.pos` conflict. Handled by
  §6 and C2.
- **E2 (T)** Drag vs delete: `deleted_while_edited`. For IC, a reduction's
  delete should beat a pure position edit automatically (presentation lost to a
  model change). Anything else surfaces as K3.
- **E3 (T)** Rename vs link to the old name: `link_broken (renamed)`. IC
  renames rarely, but the inspector allows it. Claim the rune while renaming.
- **E4 (T)** A text field edited on both sides: conflict on one key. C3 makes
  it rare, and it surfaces with both texts. There is no character-level merge:
  Palabra has registers, not sequence CRDTs, and IC's fields are short, so this
  is acceptable. A host with long prose fields would need more (out of scope).
- **E5** Every merge clears undo, so undo dies in active sessions (§5).
- **E6** A splice while *I* am mid-drag: the node I am dragging moved or was
  deleted under me. The drag must survive a re-projection (the staged offset is
  relative, so it does). If the node is gone, cancel the drag with a toast, and
  do not commit a move to a rune that no longer exists.
- **E7** A splice while I am typing in a field whose value changed remotely:
  widget staging already refuses to yank a live edit. On commit, the conflict
  surfaces if the other value was not seen. Show "changed by Bo while you were
  editing" beside the field *before* commit.
- **E8** Whole-state exchange (Palabra sends O(document) per change) during
  "reduce all": a steady stream of full documents, plus a replica **persist to
  flash** per observation. Fine on LAN for IC-sized nets, but coalesce
  observations to about 10 Hz during auto-reduce, and measure on a phone.
- **E9 (T)** No phantom loop, and an idle peer never costs undo: pinned already.
  Remote-change *playback* must not re-observe anything.

**Interaction-net domain (IC)**

- **E10 (T)** Two redexes sharing a boundary wire, stepped concurrently: two
  dangling halves (§4.2).
- **E11 (T)** One redex stepped twice: duplicated minted agents (§4.3).
- **E12 (T)** **The joiner seeds the net.** IC's `init` runs `mantle new lafont`
  and `build_starter`. A joiner doing that mints a second mantle id under the
  same name (a conflict on every merge, the exact bug `net_smoke` first had) and
  a duplicate starter net. Join must start empty.
- **E13 (T)** Minted-name collisions from concurrent adds and steps (§4.3).
- **E14** Concurrent rewires of one port: two wires on a principal (§4.3).
  Caught by the integrity check.
- **E15** A vicious circle created *by a merge* (each peer's edit legal, the
  union deadlocked). Legal by the author's ruling (*detectable, not prevented*),
  but worth a note: "this merge closed a vicious circle".
- **E16** Private agents make holes in the shared net (§4.3). Mantle-level
  privacy only.
- **E17** **Version skew**: a desktop build and an older APK with different
  pigment rules or glyphs reduce the same redex differently, and derived ids
  would then collide with *different* content. Presence carries the app version
  and the reduce-spec hash, and a mismatch disables reduction between those peers.
- **E18** Remote steps arriving while my own step animation plays. The animation
  is ghosts over the model, so the model can change under it. R5: snap and
  replay the latest.
- **E19** `config set actor human:lafont` is hard-coded in IC, so every peer's
  log attributes every act to "lafont". The actor must come from the profile.

**UX**

- **E20** Peer colour against IC's pigments: a red peer outline around a
  red-pigmented agent disappears, and the hot halo of an active pair competes
  with presence rings. Presence marks need a **shape channel** (dashed ring,
  avatar badge), not colour alone, which is the port-style rule applied to people.
- **E21** Phones have no hover. A phone peer's cursor exists only while a finger
  is down, so draw a fingertip dot, not an arrow, and don't send a stale
  cursor. A phone's view rect is small and portrait, so follow must fit it.
- **E22** Follow while the followed peer is inside a box mantle I am not in:
  either enter it too (a `use`, which is local view state) or stop following
  with a note. Never enter it silently.
- **E23** Many peers: at 6 or more, cursors and name tags become noise. Collapse
  idle cursors to dots, and let PresenceDisplay hide cursors separately from
  selections.

**Platform, LAN, Android**

- **E24** **Android multicast.** Many Android devices drop broadcast and
  multicast UDP unless the app holds a `WifiManager.MulticastLock`. That is a
  Java API reached through JNI, the same zero-Java tension as **Q29**. The
  fallback, entering an address and code, needs a keyboard, which is Q29 again.
  A **digits-only ImGui keypad** answers the fallback without deciding Q29.
- **E25** **The APK declares no permissions at all** (measured 2026-09-20,
  `InteractionCombinators/android/AndroidManifest.xml`), not even `INTERNET`,
  so today it cannot open a socket. It needs `INTERNET`, `ACCESS_NETWORK_STATE`,
  `ACCESS_WIFI_STATE` and `CHANGE_WIFI_MULTICAST_STATE`. All four are
  normal-level permissions with no runtime prompt, so `hasCode=false` survives
  the change. The manifest also locks the activity to **portrait**, which E21's
  follow math has to assume.
- **E26** **Android lifecycle**: backgrounding pauses the NativeActivity, sockets
  die and peers expire the phone. Resume must reconnect and merge (R4 shows what
  changed). The replica must be persisted *before* pause (the `persist` callback
  runs per observation, so this is already true, but test it).
- **E27** **Windows firewall** prompts on first listen. If the user declines,
  discovery silently finds nobody. Detect "no beacons heard in 10 s while
  sharing" and say where to look.
- **E28** **Client isolation** on guest and campus Wi-Fi: beacons may or may not
  arrive, and connections fail. The status pill should distinguish "can't see
  anyone" from "can see them, can't connect".
- **E29** Multiple network interfaces (VPN, virtual adapters, Docker) send the
  beacon out the wrong one. Bind per interface. Hormiga's two-copies-on-one-
  machine bug (one well-known port) also applies: announce the sync port in
  presence.
- **E30** Hormiga's first real link found **a quiet socket read as a broken
  one**. IC's transport must not tear down a link on 700 ms of silence. This is
  the first place to look if a peer "never said hello".

**Identity and agents**

- **E31** Two instances on one machine for testing need separate profiles and
  replicas (Hormiga's `HORMIGA_PROFILE_DIR` pattern). Otherwise the handshake
  refuses: *two devices hold one identity*, Palabra §11.2.
- **E32** A restored backup or a copied project folder reuses a replica id.
  Palabra refuses the merge, correctly. Opening a project copied from another
  device must **fork** the replica.
- **E33** A headless agent as a collaborator: it has no view, so no cursor or
  surfaces, but it can select and hold claims. Its avatar says "agent", and a
  person's claim outranks it (headless precedence).

# 9. What belongs where

| piece | owner | why |
|---|---|---|
| `CanvasPresence` (cursor, view rect, gesture ghosts, typing, pings, claims), its codec and limits | **Void Maiz** (`presence.hpp`) | what networking looks like (Q30) |
| drawing cursors, ghosts, edge chips, follow, minimap rects, split rings, tombstones | **Void Maiz** (`CanvasNet`, `netview`) | one renderer |
| `SceneDiff`, diff playback, attribution flash | **Void Maiz** | view over any host's scene |
| local compensating-command history | **Void Maiz** (gesture layer) | the gesture compiler is where inverses are known |
| presentational auto-resolve policy | **Void Maiz** (`voidmaiz_net`), the field list declared by the host | a rule every canvas host needs |
| net integrity check, redex claims, the crank, remote-step animation, re-knotting | **IC** | interaction-net meaning. The library must not learn what a redex is (the Allomone boundary lesson, CLAUDE.md rule 5) |
| LAN discovery, pairing, sealed transport | **Void Palabra** (transport is theirs under Q30), today living in Void Hormiga | see Q34 |
| reading who wrote a register's live value (R3) | **Void Palabra** (small read API) | the tag is theirs |
| derived ids for reduction-minted runes in the live mantle | **Void Core** (the SPEC §3.1 carve-out exists; the verb does not) | ask, don't edit (rule 4) |

**Upstream asks this page implies, not yet sent** (pending the author's answers
to Q31–Q35):

- **Void Palabra**: (1) a LAN transport companion target, lifted from Hormiga's
  beacon, SAS and sealed session, so IC does not copy Hormiga's code; (2) a read
  of the replica id behind a register's live value (attribution); (3) optional
  coalescing of `doc` sends.
- **Void Core**: minting reduction agents with derived ids into the *live*
  mantle, either a single-step `reduce --step --commit` or a derived-id form of
  `rune new` allowed only inside a reduction.

# 10. A staged path for the demo

| stage | what | proves |
|---|---|---|
| **N0** | IC on `voidmaiz_net` with the in-memory mesh: Host/Join (E12), actor from profile (E19), integrity check, status pill, remote diff playback (R1, R2) | two IC windows in one process converge, and a remote step animates |
| **N1** | `CanvasPresence`: cursors, drag ghosts, pending wires, held rings with grab friction, edge chips, jump-to | "I can see what you are about to do" |
| **N2** | reduction claims and the crank. Presentational auto-resolve. Local compensating undo | the demo stays correct when two people reduce |
| **N3** | a real LAN transport (Q34), desktop ↔ desktop, then desktop ↔ Android (E24–E30) | the author's target: phone and PC on one net |
| **N4** | follow, pings, minimap rects, typing preview, "while you were away", per-port routes | the experience is good, not only correct |
| **N5** | derived-id reduction (upstream), then the re-knotting experiment | concurrent reduction is merge-safe by construction |

N0–N2 need no network and no one's permission, and can be pinned by
`net_smoke`-style tests: every **(T)** edge case above becomes a test. N3 is the
first stage that needs the author's explicit yes for IC over LAN, which so far
was given to Hormiga only.

# What this does not know

- How often concurrent moves actually collide on LAN latency. The claim that
  claims make conflicts rare is reasoning, not measurement. N1 should count
  conflicts per minute in a two-person session.
- Whether Android devices in hand drop broadcast without the multicast lock.
  Reported widely, and not tested here.
- Whether O(document) exchange stays comfortable during "reduce all" on a phone
  (E8). Palabra measured convergence, not throughput.
- The prior art in §3 is recalled (Figma's multiplayer cursors, follow and
  spotlight, tldraw's follow, Unreal Multi-User Editing's per-object locks,
  Blender's multi-user add-on with per-object ownership). None of it was checked
  for this page, and none of it bears on correctness, only on which UX patterns
  people already know.
