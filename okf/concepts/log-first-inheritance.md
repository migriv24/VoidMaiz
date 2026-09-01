---
type: Concept
title: Log-first inheritance
description: The original draft's "log-first architecture" is not something Void Maiz builds — it's what Void Core already is. The honest mapping, and what's genuinely left for Void Maiz.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-09T00:00:00Z
---

The pre-Void-Core draft ([archived](/references/voidnode-draft-v1.md)) proposed a
"radical" architecture: an append-only transaction log as source of truth, a CLI
as the primary interface, replayable state, human/agent attribution. The draft was
right about the *value* of all of this — and wrong about who builds it. **That
architecture is Void Core.** Void Maiz's founding act is refusing to reinvent it.

# The mapping (draft concept → what actually provides it)

| Draft concept | Reality |
|---|---|
| Transaction log as source of truth | The core's **state document** + dispatcher command history + undo mementos + the **log ring** (`vc_set_log_sink` streams every line live). The state document IS the live model (cJSON in-memory), so "replay" is just `vc_create(state_json)`. |
| CLI as primary interface | The **dispatcher** (`vc_dispatch(cmd) → {ok,lines,data}`) is already the one command surface — CLI, UI, scripts, agents all issue the same verbs. SPEC §6/§7. |
| Transaction structure (id/user/op/params/metadata) | Dispatcher commands + the log spine (SPEC §9). Attribution: **implemented upstream 0.2.3** — `config set actor` stamps `who` on every log record and undo frame; the mutation spine logs every successful command. |
| Undo/redo, snapshots, revert | Core verbs: `undo`, `redo`, `history`, `status`, `diff`, `revert`, `save` (baseline dirty-tracking), atomic `batch` = one undo frame. |
| Typed ports / node types | **Glyphs** (registered per-host via `vc_register_glyph`); port signatures ride as glyph metadata (the VLS pattern: `NODE_KIND` → glyph defs). Signal types are host conventions the UI enforces. |
| Subgraphs / hierarchical composition | **Mantles**, and the Loop pattern (a whole mantle as one node) — designed, built, and audibly proven in VLS (`okf/concepts/loop.md` there). |
| Selections, grouping, routing | The **tag system** + filter grammar (`vc_tag_match` is exported *stateless* — the UI can filter at render time). `@<filter>` group-targeting already lets one command hit many runes. |
| Scripting / automation | **Voidscript** (SPEC §8), run via the `script` verb. |
| External systems / devices | The **holiday** pattern: all real I/O crosses `vc_set_effect_handler`. A MIDI controller is an input holiday *emitting dispatcher commands*; a VST is a compute holiday (VLS proved it). |
| Git-like branches/merges, cloud BaaS | **Not built, and not Void Maiz's job.** The state document is diffable JSON (versionable with actual git today). Branch/merge semantics for logs = an upstream research question; BaaS/sync = a host holiday. Cut from scope. |
| Execution engine (topological eval, parallel dataflow) | **Deliberately NOT Void Maiz.** The compute boundary (VLS's drift test) holds: the core structures, the *application* computes. Graph→execution compilation is the Reduce/Scry pipeline — since 2026-07-09 a **portable, language-neutral contract** at `VoidCore/conformance/reduce/` (semantics README + pinned JSON cases — 10 when this was written, 25 as of 2026-09-01 — plus a ~100-line reference runner to port; the Python implementation stays the oracle until a C-ABI reduce lands). A UI library that starts executing graphs has eaten its host. |
| Compilation targets (C++/GLSL/CUDA/SQL) | Application-side, far-horizon. Out of Void Maiz. |

# What is genuinely left — Void Maiz's actual substance

Strip away everything the core provides and the real library emerges, still
substantial:

1. **The projection engine** — state document → drawable graph scene, incremental,
   fast, with the auto-repair property (re-project after every dispatch; rejected
   edits simply never appear). See [views as projections](/concepts/views-as-projections.md).
2. **The interaction grammar** — hit-testing, wire dragging with type checking,
   marquee/multi-select, keyboard gestures, the add-search box… each gesture
   *compiling to dispatcher commands*. This is the part no one else has: the
   gesture→command compiler. See [total observability](/concepts/total-observability.md).
3. **Node faces & widgets** — nodes as windows (collapse/maximize), custom face
   renderers per glyph (piano roll, waveform, live video), face widgets
   (sliders/combos on nodes — VLS #14b carried over as a founding feature, per
   the author 2026-07-09).
4. **The view-state discipline** — positions, collapsed flags, window sizes as
   model content (or a designated session tier — open question #3), never as
   library-private state.
5. **A C++20 embedding of the core** — a thin RAII wrapper over the C ABI
   (manager lifetime, string ownership, JSON round-trips) that every C++ Void
   Core app can reuse even without the UI.

# The one-sync rule (inherited, hardened)

The VLS editor's deepest lesson: **the page never mutates its own picture** — it
posts a command, then re-projects from state. Server-rejected edits vanish;
there is no divergence to reconcile because there is only one truth. Void Maiz
keeps this exactly, in-process: dispatch → re-project. The projection must
therefore be cheap (incremental, dirty-tracked) — that's an engineering
requirement of THIS library, not a reason to cache truth.
