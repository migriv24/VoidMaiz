/*
 * voidmaiz/select.hpp — a selection of names, changed the way every desktop
 * list and canvas changes one (okf/concepts/conventions.md).
 *
 * THE CONTRACT A VOID APPLICATION KEEPS, whatever its screens are made of (a
 * host's own tabs, or tabs a database describes): in any list of things, a
 * click selects one, Ctrl+click adds or removes one, Shift+click selects the
 * run from the last click, Ctrl+A selects all, Escape or a click on nothing
 * clears. A host does not opt into this list by list; it uses `ListSelect`
 * (voidmaiz/listselect.hpp) and gets all of it, or `select_click` for a
 * surface ImGui's multi-select cannot drive (a canvas, a phone list).
 *
 * Model tier: no ImGui. A selection is the names of what is selected (the
 * `EditorState::selection` shape), in the order they were added; a list is
 * `count` items, named in its own order by `name_at`.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace maiz {

using NameAt = std::function<std::string(long)>;

/* A change to a selection, in a list's terms: every item, or the run
 * [first, last] of it, selected or not (ImGui's SetAll / SetRange). */
struct SelectRequest {
    enum class Kind { All, Range } kind = Kind::All;
    long first = 0, last = -1;
    bool selected = true;
};

/* Applies `r`; true when the selection changed. Selecting adds names at the
 * end, in list order; unselecting removes them wherever they are. */
inline bool apply_select(std::vector<std::string>& selection, const SelectRequest& r, const NameAt& name_at,
                         long count) {
    long a = r.kind == SelectRequest::Kind::All ? 0 : r.first;
    long b = r.kind == SelectRequest::Kind::All ? count - 1 : r.last;
    if (a > b) std::swap(a, b);
    a = a < 0 ? 0 : a;
    b = b >= count ? count - 1 : b;
    if (r.kind == SelectRequest::Kind::All && !r.selected) {
        const bool had = !selection.empty();
        selection.clear();
        return had;
    }
    std::unordered_set<std::string> in(selection.begin(), selection.end());
    bool changed = false;
    if (r.selected) {
        for (long i = a; i <= b; ++i) {
            std::string n = name_at(i);
            if (!n.empty() && in.insert(n).second) selection.push_back(std::move(n)), changed = true;
        }
    } else {
        std::unordered_set<std::string> drop;
        for (long i = a; i <= b; ++i) drop.insert(name_at(i));
        const std::size_t before = selection.size();
        std::erase_if(selection, [&](const std::string& s) { return drop.count(s) > 0; });
        changed = selection.size() != before;
    }
    return changed;
}

/* The modifier keys of a click. */
struct SelectMods {
    bool shift = false, ctrl = false;
};

/* A click on item `index` of a list, by the contract above, for a surface that
 * is not an ImGui list (a canvas's things in drawing order, a phone list in
 * selection mode). `anchor` is the last item clicked without Shift; it is
 * kept here and reset to the click. Returns whether the selection changed. */
inline bool select_click(std::vector<std::string>& selection, long& anchor, long index, SelectMods m,
                         const NameAt& name_at, long count) {
    if (index < 0 || index >= count) return false;
    const std::string name = name_at(index);
    if (m.shift && anchor >= 0 && anchor < count) {
        std::vector<std::string> before = selection;
        if (!m.ctrl) selection.clear(); // Shift: the run replaces; Ctrl+Shift: the run is added
        apply_select(selection, {SelectRequest::Kind::Range, anchor, index, true}, name_at, count);
        return selection != before;
    }
    anchor = index;
    if (m.ctrl) {
        for (auto it = selection.begin(); it != selection.end(); ++it)
            if (*it == name) {
                selection.erase(it);
                return true;
            }
        selection.push_back(name);
        return true;
    }
    if (selection.size() == 1 && selection.front() == name) return false;
    selection = {name};
    return true;
}

} // namespace maiz
