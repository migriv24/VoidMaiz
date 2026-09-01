/*
 * voidmaiz/inspector.hpp — the field inspector: the selection's editable
 * fields, compiled to `set`/`setjson` on commit.
 *
 * Staging discipline (VLS #14b, okf/concepts/views-as-projections.md): while
 * a field is being edited its buffer is local truth for that widget only —
 * re-projection never yanks it mid-edit — and the commit (Enter or defocus)
 * flushes exactly ONE command. Escape abandons the edit, nothing is sent.
 *
 * View-module header (ImGui types); draws into the current window.
 */
#pragma once

#include "voidmaiz/canvas.hpp"
#include "voidmaiz/widget.hpp"

namespace maiz {

/* Inspector for the current selection. Returns compiled commands (at most one
 * per commit); caller dispatches + re-projects, like edit_canvas.
 *
 * Fields render through the widget protocol (voidmaiz/widget.hpp): a glyph's
 * `hints.editors` picks each field's editor, resolved against `widgets` —
 * pass the host's registry so registered editors appear here too; nullptr
 * uses the library defaults (text/number/multiline/combo/date). */
CanvasIO draw_inspector(const Scene& scene, EditorState& ed,
                        const WidgetRegistry* widgets = nullptr);

} // namespace maiz
