# Void Maiz — agent instructions

You are working on **Void Maiz**, a C++20 node-graph UI library for applications
built on **Void Core** (`../VoidCore` — a C library with a pure C ABI,
`core/include/voidcore.h`). This project will eventually have its own dedicated
agent; until then, sessions may be shared with Void Loops Studio work.

> **Naming note.** The project was renamed *Void Node* → **Void Maiz** on
> 2026-07-13 (namespace `maiz::`, targets `voidmaiz*`, headers `voidmaiz/`).
> The physical repo folder was renamed `VoidNode/` → `VoidMaiz/` on 2026-07-13;
> the sibling demo's `VOIDMAIZ_ROOT` default was retargeted to `../VoidMaiz` to
> match. Historical docs (`okf/log.md`, `okf/references/`, `MESSAGE_*`,
> `promptToStart.md`) keep the old name on purpose.

## Ground rules

1. **OKF first.** `okf/` is the source of truth for design; code follows concepts,
   never the other way. Every session that changes design or code captures it in
   `okf/log.md` and trues up the affected concept docs. Read `okf/index.md` before
   doing anything.
2. **Void Maiz owns no model state.** The graph lives in Void Core's state
   document; Void Maiz renders projections of it and emits dispatcher commands.
   If you find yourself adding a source-of-truth data structure to the library,
   stop and re-read [okf/concepts/log-first-inheritance.md](okf/concepts/log-first-inheritance.md).
3. **Every gesture is a command.** Anything a user does that changes anything goes
   through the dispatcher and is visible in the log/CLI. This is the founding
   commitment (see [okf/concepts/total-observability.md](okf/concepts/total-observability.md)).
4. **Upstream messages, not upstream edits.** Void Maiz *uses* Void Core; it never
   redesigns it. Needs/gaps for Void Core (or any sibling agent — Void Hormiga,
   VLS-native) get drafted as a message file in the recipient's repo root for the
   author to relay. **Uniquely title every message** (2026-07-21 convention):
   filename `MESSAGE_FOR_<RECIPIENT>_<sender>-<topic>-<YYYY-MM-DD>.md` and a
   matching self-identifying H1 — **never** a bare `MESSAGE_FOR_<RECIPIENT>.md`,
   because a generic name lets an agent mistake a stale message for the current
   one (several agents relay at once). Unique titles let dated messages coexist
   as history safely; the OKF index tracks which are open vs consumed (retire by
   the index, not by deletion). Full convention: [okf/index.md](okf/index.md)
   §"Inter-agent messages."
5. **Allomone is a sibling repository, not ours.** `../VoidAllomone` holds the
   derivation language and the lattice merge, extracted 2026-08-29. We compile
   it from source (`add_subdirectory`) and re-export it into `maiz::` so hosts
   see no change. Design questions about the LANGUAGE go to its OKF; questions
   about how Void Maiz uses it stay here. We kept the two things that are ours
   rather than the language's: the ImGui script **editor** and the **attention
   graph** whose data the `with`/`device` host predicates read. If you find
   yourself teaching the language what a Scene, a widget or a canvas is, stop —
   that is the boundary, and it is the one Void Unity found by trying to use it.
6. **Vendor, don't depend.** Third-party pieces (Dear ImGui, miniaudio-class
   single-file libs) are vendored with licenses, LiteGraph-style. No package
   managers, no CDNs, no frameworks.
7. **The author decides.** Open design decisions go to `okf/developer_questions.md`
   with a lean; answers arrive inline, in chat, or via FaultSack notes.

## The reference implementation

Void Loops Studio's Python editor (`../VoidLoopsStudio/voidloops/ui.py` +
`editor.html`) is the *proven prototype* of everything Void Maiz generalizes:
the `/state`+dispatcher seam, state-rebuild auto-repair, node faces as windows,
face widgets, positions-as-model-content. When in doubt about a behavior, that
code and its OKF (`../VoidLoopsStudio/okf/`) are the precedent.
