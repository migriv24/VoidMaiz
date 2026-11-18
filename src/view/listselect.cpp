/* listselect.cpp — the list contract over ImGui's multi-select. See
 * voidmaiz/listselect.hpp and okf/concepts/conventions.md. */
#include "voidmaiz/listselect.hpp"

#include <algorithm>

namespace maiz {

void ListSelect::begin(std::vector<std::string>& selection, long count, NameAt name_at, ListSelectOptions o) {
    sel_ = &selection;
    count_ = count;
    name_at_ = std::move(name_at);
    changed_ = false;
    ImGuiMultiSelectFlags f = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid;
    if (o.single) f |= ImGuiMultiSelectFlags_SingleSelect;
    else if (o.box) f |= o.grid ? ImGuiMultiSelectFlags_BoxSelect2d : ImGuiMultiSelectFlags_BoxSelect1d;
    f |= o.scope_rect ? ImGuiMultiSelectFlags_ScopeRect : ImGuiMultiSelectFlags_ScopeWindow;
    apply(ImGui::BeginMultiSelect(f, (int)selection.size(), (int)count));
}

bool ListSelect::item(long index, std::string_view name) const {
    ImGui::SetNextItemSelectionUserData((ImGuiSelectionUserData)index);
    if (!sel_) return false;
    return std::find(sel_->begin(), sel_->end(), name) != sel_->end();
}

bool ListSelect::end() {
    apply(ImGui::EndMultiSelect());
    sel_ = nullptr;
    return changed_;
}

void ListSelect::apply(ImGuiMultiSelectIO* io) {
    if (!io || !sel_) return;
    for (const ImGuiSelectionRequest& r : io->Requests) {
        SelectRequest q;
        if (r.Type == ImGuiSelectionRequestType_SetAll) {
            q.kind = SelectRequest::Kind::All;
        } else if (r.Type == ImGuiSelectionRequestType_SetRange) {
            q.kind = SelectRequest::Kind::Range;
            q.first = (long)r.RangeFirstItem;
            q.last = (long)r.RangeLastItem;
        } else {
            continue;
        }
        q.selected = r.Selected;
        changed_ = apply_select(*sel_, q, name_at_, count_) || changed_;
    }
}

bool ListSelect::plain_click() {
    const ImGuiIO& io = ImGui::GetIO();
    return ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeyShift;
}

bool ListSelect::delete_pressed() {
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
           ImGui::IsKeyPressed(ImGuiKey_Delete, false);
}

} // namespace maiz
