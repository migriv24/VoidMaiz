/*
 * voidmaiz/code.hpp — a from-scratch code editor whose tokens can be LIVE
 * WIDGETS (DRAFT 2026-08-06; okf/concepts/allomone/editor.md).
 *
 * WHY THIS EXISTS. ImGui's InputTextMultiline is a black box: you cannot place
 * an interactive item mid-text, so a colour literal can never become a colour
 * wheel where it sits. Owning the layout is the only way to get inline widgets,
 * and inline widgets are the whole point — the editor is a WIDGET CANVAS
 * disguised as a text editor. Void Hormiga proved the shape in their Allo Dev
 * tab; this is that build, generalized and moved to where every host can use it.
 *
 * WHAT MAKES IT A VOID MAIZ COMPONENT AND NOT JUST A TEXT BOX. Two things:
 *
 *  1. It is a PROJECTION with the standard seam. The editor holds a buffer and
 *     a caret — its own gesture state, exactly like EditorState — and hands back
 *     `commit` when the author is done. It never dispatches anything; the host
 *     compiles the text into ONE `set` on release (the VLS #14b staging
 *     discipline: stage locally, flush one command per gesture). Abandon the
 *     edit and nothing was ever logged.
 *  2. Its inline widgets ARE the widget registry. A token span is claimed by a
 *     SpanRenderer keyed on a `kind` string — the same `kind → renderer` binding
 *     widget.hpp already uses for field editors. So the colour wheel in a script
 *     is the colour wheel on a contact: one kit, many surfaces.
 *
 * WHAT IT IS NOT. Not a general-purpose text input (widget-registry.md's line
 * holds: general UI is the host's) and not a language-aware IDE — it knows
 * nothing about Allomone. Colouring and span detection arrive as callbacks, so
 * the editor serves any language, or none.
 *
 * View-module header (ImGui types).
 */
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* One coloured run of text, produced by the host's tokenizer. Offsets are byte
 * offsets into the buffer; overlapping spans are resolved first-wins. */
struct CodeSpan {
    size_t begin = 0, end = 0;
    unsigned rgb = 0xd0d0d0; // 0xrrggbb
};

/* A span the editor should render as a WIDGET instead of as text.
 *
 * `kind` is the widget-registry binding ("color", "date", …) — a host
 * convention the editor carries and never interprets. `value` is the current
 * decoded value; a renderer that changes it sets `changed` and the editor
 * splices the new text back over [begin, end) — which is what makes the wheel
 * edit the script rather than a copy of it. */
struct CodeWidgetSpan {
    size_t begin = 0, end = 0;
    std::string kind;
    std::string value;   // decoded (no quotes) — what the renderer edits
    bool quoted = false; // re-wrap in double quotes when splicing back
};

/* Drawn at the token's position, inside the editor's own layout. Return true
 * when `value` changed; the editor splices it in and reports a commit-worthy
 * edit. `active` is true while the popup owning this span is open, so a
 * renderer can keep itself alive across frames. */
using SpanRenderer =
    std::function<bool(const char* id, std::string& value, bool active)>;

/* kind → renderer, host-owned like WidgetRegistry (no globals). */
struct CodeWidgetRegistry {
    std::vector<std::pair<std::string, SpanRenderer>> renderers;

    void add(std::string kind, SpanRenderer r);
    const SpanRenderer* find(std::string_view kind) const;

    /* The library kit: a "color" renderer (a swatch that pops an ImGui colour
     * wheel over the token) and a "date" renderer (a compact month grid). Both
     * edit the token's text in place. A host adds its own kinds on top. */
    static CodeWidgetRegistry defaults();
};

// ── completion ──────────────────────────────────────────────────────────────

/* One offered completion. `text` is what gets spliced in; `label` is what the
 * list shows (defaults to `text` when empty); `detail` is a right-aligned hint
 * — "tag", "4 subjects", "host predicate" — which is what turns a list of words
 * into something you can choose from without already knowing the answer. */
struct Completion {
    std::string text;
    std::string label;
    std::string detail;
};

/* What to offer, and what it replaces.
 *
 * The HOST supplies the range rather than the editor guessing it, because only
 * the host knows whether the caret sits in a bare word, inside a quoted string,
 * or somewhere that should replace nothing at all. Guessing here would be wrong
 * for exactly the cases that matter — a tag name inside quotes is not a word
 * boundary problem the editor can solve generically. */
struct CompletionSet {
    size_t replace_begin = 0, replace_end = 0;
    std::vector<Completion> items;
};

/* The editor's own gesture state — the caret, the selection, the scroll. NOT
 * part of any seam: like EditorState, it is this view's business, and a host
 * that renders two editors keeps two of these.
 *
 * `text` is the staged buffer. The host seeds it when the target changes and
 * reads it back on commit; between those, edits stay local and nothing is
 * logged (the staging discipline). */
struct CodeEditorState {
    std::string text;

    size_t caret = 0;      // byte offset
    size_t anchor = 0;     // selection anchor; == caret means no selection
    int caret_col_hint = -1; // desired column across up/down moves; -1 = derive

    bool focused = false;
    float scroll_y = 0;
    double blink = 0;

    /* Ctrl+Wheel zoom. Below `lod_threshold` rendered pixels per line the
     * editor stops drawing glyphs and draws each coloured token as a filled
     * bar instead — a level-of-detail switch, so zooming out turns the script
     * into a readable SHAPE (where the colours are, how the rules are massed)
     * rather than illegible grey mush. View state, not model state: it belongs
     * to this editor instance and is never dispatched. */
    float zoom = 1.0f;

    // The span whose popup is open, as a byte offset; npos = none.
    size_t open_span = std::string::npos;

    /* Completion session. `completion_pick` is the highlighted row; dismissing
     * with Escape sets `completion_off` until the buffer or caret next moves,
     * so the popup stays out of the way once you have said no to it. */
    int completion_pick = 0;
    bool completion_off = false;
    size_t completion_at = std::string::npos; // caret the suppression applies to
    /* Ctrl+Space asked for it. Without this the popup only appears once you are
     * TYPING a word — an editor that suggests something on every blank line is
     * noise you have to keep dismissing. */
    bool completion_forced = false;

    /* True when the buffer changed since the last clear_dirty() — the host's cue
     * that there is something worth committing. */
    bool dirty = false;
    void clear_dirty() { dirty = false; }

    bool has_selection() const { return caret != anchor; }
    std::string selected_text() const;
    void select_all();
    void set_text(std::string t); // reseat the buffer and clamp the caret
};

/* What one frame of the editor produced. */
struct CodeEditorIO {
    bool edited = false; // the buffer changed this frame
    bool commit = false; // the author signalled "done" (Ctrl+Enter, or focus
                         // left after an edit) — the host's cue to compile ONE
                         // command and dispatch it
};

struct CodeEditorOptions {
    /* Colour the text. Called each frame with the whole buffer; return spans in
     * any order. Absent = uniform default colour. */
    std::function<std::vector<CodeSpan>(std::string_view)> highlight;
    /* Claim token spans as widgets. Called each frame with the whole buffer. */
    std::function<std::vector<CodeWidgetSpan>(std::string_view)> widgets;
    /* A tooltip for the token under the mouse, given its byte offset. Return ""
     * for none — this is where a host puts "which subjects does this line
     * refer to?" without deriving anything. */
    std::function<std::string(std::string_view, size_t)> hover;

    /* What to offer at the caret. Called each frame while the editor has focus;
     * return an empty item list for "nothing to suggest here".
     *
     * This is where a domain vocabulary becomes usable: a host that registers
     * predicates and properties nobody can be expected to memorize should offer
     * them here (okf/concepts/allomone/host-protocol.md). */
    std::function<CompletionSet(std::string_view, size_t)> complete;

    /* Explain the word at this offset, for the right-click menu. Multi-line is
     * fine — this is a panel, not a tooltip. Return "" for "nothing to say".
     *
     * Distinct from `hover` on purpose: hover fires constantly and must stay
     * terse, while this is asked for deliberately and can afford to teach. It
     * is also how a domain vocabulary becomes *discoverable* rather than merely
     * completable — completion tells you a word exists, this tells you what it
     * does. */
    std::function<std::string(std::string_view, size_t)> explain;

    const CodeWidgetRegistry* registry = nullptr;
    bool read_only = false;
    float line_spacing = 1.15f;

    /* Ctrl+Wheel zoom bounds, and the line height (in pixels) below which the
     * editor switches to level-of-detail bars. */
    bool allow_zoom = true;
    float zoom_min = 0.25f, zoom_max = 3.0f;
    float lod_threshold = 7.0f;
};

/* Draw the editor at the current cursor, filling `size` (0 = the available
 * region). `id` scopes the ImGui state. Handles text input, caret movement
 * (arrows / home / end / word-wise with Ctrl), click and drag selection,
 * shift-select, double-click word select, copy / cut / paste / select-all,
 * undo/redo of the local buffer, scrolling, and the inline widget spans. */
CodeEditorIO code_editor(const char* id, CodeEditorState& st,
                         const CodeEditorOptions& opts = {},
                         float width = 0, float height = 0);

} // namespace maiz
