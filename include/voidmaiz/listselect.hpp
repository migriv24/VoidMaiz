/*
 * voidmaiz/listselect.hpp — every list a Void application draws selects the
 * same way (okf/concepts/conventions.md; the model half is voidmaiz/select.hpp).
 *
 * WRAP THE LIST, GET THE CONTRACT. A host that draws a list of things it can
 * act on puts `begin` before the items, `item` before each item's
 * Selectable (or TreeNode), and `end` after them. That list then has, with no
 * code of the host's: click selects one, Ctrl+click toggles one, Shift+click
 * selects a run, Ctrl+A selects all, Escape and a click on empty space clear,
 * and a drag across the rows from empty space selects them (box select). It
 * works under a clipper: `name_at` names any item, drawn or not.
 *
 * Built on ImGui's own multi-select (BeginMultiSelect, 1.91+), adapted to a
 * selection that is a list of NAMES (EditorState::selection) rather than
 * ImGui's index sets.
 *
 * View-module header (ImGui types).
 */
#pragma once

#include "voidmaiz/select.hpp"

#include "imgui.h"

#include <string>
#include <string_view>
#include <vector>

namespace maiz {

struct ListSelectOptions {
    bool box = true;          // drag from empty space across rows selects them
    bool grid = false;        // items laid out in 2D (cards): box select by area, not by rows
    bool single = false;      // a list of one choice (a radio): no Ctrl/Shift/Ctrl+A
    bool scope_rect = false;  // several lists in one window: clear/box only inside this one
};

class ListSelect {
  public:
    /* Before the items. `count` is the whole list (not only what is drawn),
     * `name_at(i)` its i-th item's name in drawing order. */
    void begin(std::vector<std::string>& selection, long count, NameAt name_at, ListSelectOptions o = {});
    /* Before each item's Selectable/TreeNode: marks it as item `index`, and
     * says whether it is selected (pass that to the Selectable). */
    bool item(long index, std::string_view name) const;
    /* After the items. True when the selection changed this frame. */
    bool end();

    /* After an item's Selectable: was it clicked with no Shift or Ctrl (what a
     * host "opens", focuses, or pans to)? */
    static bool plain_click();
    /* The list's window is focused, nothing is being typed, and Delete was
     * pressed: what a host deletes (through its dispatcher, so it undoes). */
    static bool delete_pressed();

  private:
    void apply(ImGuiMultiSelectIO* io);
    std::vector<std::string>* sel_ = nullptr;
    long count_ = 0;
    NameAt name_at_;
    bool changed_ = false;
};

} // namespace maiz
