---
type: Concept
title: Conventions — what every list and canvas in a Void application does
description: "The interaction contract a Void application keeps on every list and canvas, whoever built the screen: click, Ctrl+click, Shift+click, Ctrl+A, Escape, click away, box select, Delete; the wheel, the middle button, Ctrl and Shift on a canvas; and the phone's translation. Built for lists as ListSelect (voidmaiz/listselect.hpp) over ImGui's multi-select, with the model rule in voidmaiz/select.hpp."
tags: [status:current, audience:host, confidence:asserted]
timestamp: 2026-11-17T00:00:00Z
---

# Conventions (2026-11-17)

Void Hormiga's author, asking for Shift and Ctrl selection in one of its map
panels: *"UI/UX features like that should just come with any sort of list view
that exists ... build systems such that they are in every list thing, not just
specifically the map ... for an application where you essentially construct the
app itself as well, we should definitely have some rules and lines to hold."*

So this is not a feature of one list. It is a **contract**: what a person can
rely on in any list or canvas of any Void application, including screens a
database describes rather than code (Hormiga's "screens"). A convention that one
list keeps and the next forgets is worse than none, because it teaches the hand
something that then fails. The way to keep a contract is to make it the only
way to draw the thing: a host builds its lists on `ListSelect` and gets all of
it, and a screen built from data is drawn by components built on `ListSelect`,
so it cannot opt out.

**On a Mac, Ctrl is Cmd** (ImGui's macOS behaviour swaps them): Cmd+click,
Cmd+A, and Cmd on a canvas, as every Mac application does.

## Lists: anything shown as rows or cards that a person can act on

| input | does | built |
|---|---|---|
| click | selects that one | ✅ `ListSelect` |
| Ctrl+click | adds or removes that one | ✅ |
| Shift+click | selects the run from the last click | ✅ (across groups, if the host gives one order) |
| Ctrl+Shift+click | adds the run | ✅ |
| Ctrl+A | selects all, including rows not drawn (a clipper) | ✅ |
| Escape | clears | ✅ |
| a click on empty space in the list | clears | ✅ |
| a drag from empty space across rows | selects them (box select; cards: by area) | ✅ |
| Delete | deletes what is selected, through the dispatcher (so it undoes) | ✅ `ListSelect::delete_pressed()`; the host decides what "delete" means |
| right-click on a selected item | acts on the whole selection | host (Hormiga's Data list and map) |
| several selected | the detail pane says how many and offers what applies to all | host |
| ↑/↓ moves, Shift+↑/↓ extends | — | not yet: needs ImGui keyboard navigation turned on, which changes other keys |
| type the start of a name to jump to it | — | not yet |

**A list of choices is not a selection list.** A list where exactly one value
is chosen (the active layer, a view mode) is a radio: `ListSelectOptions{.single
= true}` keeps the clicks and drops Ctrl/Shift/Ctrl+A.

**The selection is names.** `EditorState::selection` is the names of what is
selected; a list says how it is ordered (`name_at`). So a canvas and a list over
the same things share one selection, and a selection made on the map shows in
its overview and the other way round.

## Canvases: anything with a camera

| input | does |
|---|---|
| wheel | zooms about the pointer |
| middle-button drag | pans, whatever tool is in hand |
| left drag on empty space | the canvas's own: pans (a map) or box-selects (a node canvas) |
| click a thing | selects that one |
| Ctrl+click or Shift+click a thing | adds or removes it |
| Ctrl- or Shift-drag on empty space | box-selects, adding |
| click on empty space | clears |
| Escape | clears (or ends the tool in hand first) |

A canvas that is not an ImGui list uses `select_click` (voidmaiz/select.hpp)
for the same rule over its things in drawing order.

## The phone

A finger has no Ctrl or Shift, so the same contract is translated, not dropped:

- **a tap opens** the thing (the phone's "select" is "go to it");
- **a long press starts selecting**: the thing is selected, and further taps
  add or remove (Ctrl+click); a bar says how many and offers what applies to
  all; Back or the bar's ✕ clears;
- **two fingers pan, a pinch zooms**; one finger pans a map.

Built in Void Maiz's touch gate for the gestures; the selection mode is the
host's today (not yet in Hormiga's phone lists).

## Where it lives

- `voidmaiz/select.hpp`: the model rule (`apply_select`, `select_click`), no
  ImGui.
- `voidmaiz/listselect.hpp`: `ListSelect`, the list contract over ImGui's
  `BeginMultiSelect` (1.91+).
- `tests/select_smoke.cpp`: the rule, then a real ImGui list driven headless by
  clicks and keys (click, Shift, Ctrl, Ctrl+A, Escape, click away).
