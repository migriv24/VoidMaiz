---
type: Concept
title: Allomone — the editor surface
description: "The code editor as a widget canvas disguised as a text editor: why ImGui's InputTextMultiline forced a from-scratch build, how token spans become live widgets through the same kind→renderer protocol as field editors, the staging discipline that keeps one command per gesture, and the honest list of what it is not yet."
resource: src/view/code.cpp
tags: [status:current, audience:kernel, audience:host, confidence:asserted]
timestamp: 2026-08-06T00:00:00Z
---

Part of [Allomone](/../../VoidAllomone/okf/index.md). Audience: **both**, though a
host mostly consumes this rather than extends it.

Implemented in `include/voidmaiz/code.hpp` + `src/view/code.cpp`
(`voidmaiz_view` — it needs ImGui). Language-agnostic: it knows nothing about
Allomone. Colouring and span detection arrive as callbacks, so it serves any
language, or none.

# Why it exists

ImGui's `InputTextMultiline` is a black box: you cannot place an interactive item
mid-text. So a colour literal can never become a colour wheel *where it sits* —
the best you get is a palette bolted alongside.

**Owning the layout is the only way to get inline widgets, and inline widgets are
the whole point.** Void Hormiga proved the shape in their "Allo Dev" tab after
hitting the same wall; this is that build, generalized and moved to where every
host can use it.

The framing that makes it worth the cost: **the editor is a widget canvas
disguised as a text editor.** Text is the default rendering of a token; where a
token has a richer type, it draws an interactive control instead.

# What makes it a Void Maiz component

Two things, and without them this would just be a text box we should not own:

**1. It is a projection with the standard seam.** The editor holds a buffer and a
caret — its own gesture state, exactly like `EditorState` — and reports `commit`
when the author is done. It never dispatches. The host compiles the text into
**one `set` on commit**, per the VLS #14b staging discipline: stage locally,
flush one command per gesture. Abandon the edit and nothing was ever logged.

Committing happens on `Ctrl+Enter`, when focus leaves after an edit, **or when an
inline widget's popup closes** — because an edit that never flushes is an edit
the transcript never saw.

> **The bug that taught this (2026-08-06).** Editing a colour appeared to do
> *nothing*: the wheel spliced the new hex into the buffer correctly, but commit
> required focus to leave the whole editor, which clicking "done" did not do. So
> the model was never written and the playground never restyled. **Closing the
> picker IS the release** — that is the VLS #14b staging discipline applied at
> the right granularity (stage during the gesture, flush ONE command when it
> ends), and getting the granularity wrong is indistinguishable from the feature
> being broken. A host should also offer an explicit **Apply**, because
> "it committed when focus left" is invisible; the demo shows a dirty marker
> next to the script name.

**2. Its inline widgets ARE the widget registry.** A span is claimed by a
`SpanRenderer` keyed on a `kind` string — the same `kind → renderer` binding
[widget registry](/concepts/widget-registry.md) uses for field editors. So the
colour wheel in a script is *the same wheel* as on a contact's colour field. One
kit, many surfaces, one registration.

# The seam

    CodeEditorOptions {
        highlight(source) -> [CodeSpan]        // byte ranges + rgb
        widgets(source)   -> [CodeWidgetSpan]  // byte ranges + kind + value
        hover(source, offset)    -> string     // tooltip, or ""
        complete(source, caret)  -> CompletionSet
        explain(source, offset)  -> string     // right-click panel, or ""
        registry, read_only, line_spacing
        allow_zoom, zoom_min, zoom_max, lod_threshold
    }

**`hover` and `explain` are separate on purpose.** Hover fires constantly and
must stay terse; `explain` is asked for deliberately and can afford to teach.
Completion tells you a word exists — `explain` is what tells you what it does,
which is the difference between a vocabulary being *completable* and being
*discoverable*.

All the callbacks take the whole buffer and are called per frame. That is
deliberately naive and fine at script scale; it is also why the
[tokenizer is part of the language contract](/../../VoidAllomone/okf/concepts/language.md) — the
editor and the parser address the same byte offsets, so one implementation is how
they cannot drift.

When a span's renderer changes the value, the editor **splices it back over
`[begin, end)`** in the buffer. The widget edits the *script*, not a copy of it —
which is what makes it feel like direct manipulation rather than a form.

# What ships

- **Text model** — UTF-8 buffer; caret motion steps over continuation bytes so
  multi-byte text is never split mid-codepoint.
- **Selection** — click, drag, shift-select, double-click word select.
- **Clipboard** — copy / cut / paste / select-all through ImGui's platform seam.
- **Undo/redo** of the local buffer (bounded stack), independent of Core's undo —
  correctly, because staged text is not model state until commit.
- **Colouring** — per-run batched draw calls off the host's span list.
- **Auto-indent** — Enter carries the current line's leading whitespace.
- **Inline widgets** — hover outline + hand cursor; click opens the renderer
  positioned at the token; a colour token additionally draws a swatch of itself.
  **Dismissed by clicking away or Escape** — there is no confirm button, because
  needing to confirm the end of a gesture you already finished is friction.
- **Zoom + level of detail** — Ctrl+Wheel zooms (plain wheel still scrolls, so
  the common gesture is undisturbed). Below `lod_threshold` pixels per line the
  editor **stops drawing glyphs and draws each coloured token as a filled bar**,
  whitespace skipped so indentation survives as gaps. Zooming out turns a script
  into a readable *shape* — where the colours are, how the rules mass — instead
  of illegible grey. View state: it belongs to the editor instance and is never
  dispatched.
- **Hover tooltips** — the demo previews *which subjects a rule refers to*,
  computed with `allo_matches` straight off the AST, deriving nothing.
- **Completion** (2026-08-06) — a `CompletionSet` the HOST supplies, with the
  byte range to replace. The range is the host's because only it knows whether
  the caret sits in a bare word or inside a quoted string, and guessing would be
  wrong for exactly the cases that matter.

  **`Tab` accepts; `Enter` is always a newline** (author's call — *"'enter' and
  'tab' are different in function, please make sure those are correctly
  distinguished"*). Enter had been an accept key too, which is the common
  default and is wrong: the one key you press without looking, on every line,
  must never mean two things depending on whether a popup happens to be open.
  Up/Down choose, Escape dismisses until the caret moves, and everything else
  still edits, so typing never stalls waiting on it.

  **It only appears while you are typing a word**, or on **Ctrl+Space** when you
  ask. An editor that suggests something on every blank line and after every
  space is noise you spend the day dismissing, so the trigger is a non-empty
  replace range rather than "has focus".

  **This is where a domain vocabulary becomes usable.** Between Core's aliases,
  host predicates and whatever tags the data happens to carry, nobody can hold
  the vocabulary in their head; the host knows all of it, so the host offers it.
  The demo completes tags, glyphs, rune names, mantles, devices, registered
  predicates and properties — each with a `detail` hint ("4 subjects", "host
  predicate") that turns a list of words into something choosable by someone who
  does not already know the answer.

  **The host is expected to complete by POSITION, not by one flat list.** After
  `when`, offer conditions; after a condition keyword, offer that condition's
  real **values**, already quoted. Offering `tag` again straight after `tag` is
  the failure mode, and it is the host's to avoid — the editor supplies the seam
  and never guesses the grammar.
- **Right-click** — the `explain` panel for the word under the cursor, plus
  clipboard actions. It **scales with the editor's zoom**: a menu that is *about*
  the text should not be read at a different size than the text, which at 300%
  is a jarring change of scale mid-thought.
- **Scrolling** that keeps the caret in view.

The kit ships `color` (a hue wheel that live-edits the hex as you drag) and
`date` (a compact month grid).

# The monospace assumption

Layout assumes a **monospace font** — ImGui's built-in ProggyClean is one, and a
host loading JetBrains Mono keeps the property. That is what makes caret
hit-testing an integer division instead of a per-glyph walk.

Stated plainly because it is the one real constraint: **column arithmetic counts
bytes**, so a line mixing wide characters measures slightly off. A proportional
font would replace the whole measure path, not patch it. Worth knowing before
someone loads a display font and files a bug about caret drift.

# What it is not, yet

Kept honest rather than aspirational — see the
[roadmap](/../../VoidAllomone/okf/concepts/roadmap.md):

- **No IME**, so non-Latin input is not usable for composed scripts.
- **No word-wrap** — long lines scroll horizontally.
- **No minimap.** The level-of-detail view is close to one already; a
  side-by-side overview pane would be a small step from it.
- **No find/replace**, no multi-cursor, no bracket matching.
- **No friendly/raw toggle.** Hormiga's design has references stored as ids and
  displayed as names, so a rename does not break a script. That is a genuinely
  good idea we have not built, and it needs a name↔id resolution seam the kernel
  does not currently have.

None of these block the demo. All of them will be felt by a real domain language
— **find/replace first**, now that completion has shipped and taken the top of
that list with it.
