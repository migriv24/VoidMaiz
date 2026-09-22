/* inspector.cpp — the selection's fields, rendered through the widget
 * protocol (widget.cpp's field-editor kit): the glyph's hints.editors picks
 * each field's widget, and the inspector is just another registry client —
 * a host-registered editor shows up here without inspector changes. */
#include "voidmaiz/inspector.hpp"

#include "voidmaiz/tags.hpp" // suggest_tags: the chips under the add box
#include "voidmaiz/gesture.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace maiz {

CanvasIO draw_inspector(const Scene& scene, EditorState& ed, const WidgetRegistry* widgets) {
    CanvasIO out;
    if (ed.selection.empty()) {
        ImGui::TextDisabled("nothing selected");
        return out;
    }
    if (ed.selection.size() > 1) {
        ImGui::Text("%d nodes selected", (int)ed.selection.size());
        for (const auto& name : ed.selection) ImGui::BulletText("%s", name.c_str());
        return out;
    }
    const SceneNode* node = scene.find(ed.selection.front());
    if (!node) {
        ImGui::TextDisabled("selection not in scene");
        return out;
    }

    ImGui::Text("%s", node->name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", node->label.c_str());

    // tags as chips: click a chip's × to remove it; the input adds one.
    // Any tag — color pigments or otherwise — edits the same way (tags are
    // model state: adds/removes are logged, undoable `tag` commands).
    float avail_w = ImGui::GetContentRegionAvail().x;
    float used_w = 0.0f;
    for (const auto& t : node->tags) {
        std::string chip = "@" + t + "  ×";
        float w = ImGui::CalcTextSize(chip.c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
        if (used_w > 0 && used_w + w > avail_w) used_w = 0; // wrap
        else if (used_w > 0) ImGui::SameLine();
        ImGui::PushID(t.c_str());
        if (ImGui::SmallButton(chip.c_str()))
            out.commands.push_back(compile_tag(node->name, t, false));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("remove tag @%s", t.c_str());
        ImGui::PopID();
        used_w += w + ImGui::GetStyle().ItemSpacing.x;
    }
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
    if (ImGui::InputTextWithHint("##vm-tag-add", "+ tag…", ed.tag_add_buf,
                                 sizeof ed.tag_add_buf,
                                 ImGuiInputTextFlags_EnterReturnsTrue) &&
        ed.tag_add_buf[0]) {
        out.commands.push_back(compile_tag(node->name, ed.tag_add_buf, true));
        ed.tag_add_buf[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1); // stay for the next tag
    }

    /* Suggestions (voidmaiz/tags.hpp): what this node's same-glyph peers are
     * tagged, that it is not. One click adds one — which matters most on a
     * phone, where the alternative is typing it. Outlined rather than filled:
     * a suggestion is not a tag until it is chosen. Recomputed only when the
     * node, its tags or the mode change. */
    {
        std::string key = node->name + "|" + std::to_string((int)ed.tag_suggest_mode);
        for (const auto& t : node->tags) key += "|" + t;
        if (key != ed.tag_suggest_key) {
            ed.tag_suggest_key = key;
            Suggest opt;
            opt.mode = ed.tag_suggest_mode;
            opt.limit = 5;
            ed.tag_suggest = suggest_tags(scene, *node, opt);
        }
        if (!ed.tag_suggest.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.50f, 0.26f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            float sw = 0.0f;
            for (const auto& t : ed.tag_suggest) {
                std::string chip = "+ " + t;
                float w = ImGui::CalcTextSize(chip.c_str()).x +
                          ImGui::GetStyle().FramePadding.x * 2;
                if (sw > 0 && sw + w > avail_w) sw = 0;
                else if (sw > 0) ImGui::SameLine();
                ImGui::PushID(("sg-" + t).c_str());
                if (ImGui::SmallButton(chip.c_str()))
                    out.commands.push_back(compile_tag(node->name, t, true));
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("other %s runes carry @%s", node->glyph.c_str(),
                                      t.c_str());
                ImGui::PopID();
                sw += w + ImGui::GetStyle().ItemSpacing.x;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::Separator();

    // size — every node has one (content.size, the same view-state-as-content
    // tier as pos). Staged like fields; commit compiles ONE `setjson size`.
    {
        char shown[64];
        std::snprintf(shown, sizeof shown, "%g %g", node->w, node->h);
        bool active_this = ed.edit_node == node->name && ed.edit_field == "__size";
        char tmp[sizeof ed.edit_buf];
        std::snprintf(tmp, sizeof tmp, "%s", shown);
        char* buf = active_this ? ed.edit_buf : tmp;
        ImGui::PushID("__size");
        bool entered = ImGui::InputText("size (w h)", buf, sizeof ed.edit_buf,
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemActivated()) {
            ed.edit_node = node->name;
            ed.edit_field = "__size";
            std::snprintf(ed.edit_buf, sizeof ed.edit_buf, "%s", tmp);
        }
        if (active_this) {
            if (entered || ImGui::IsItemDeactivatedAfterEdit()) {
                float w = 0, h = 0;
                for (char* c = ed.edit_buf; *c; ++c)
                    if (*c == ',' || *c == 'x') *c = ' ';
                if (std::strcmp(shown, ed.edit_buf) != 0 &&
                    std::sscanf(ed.edit_buf, "%f %f", &w, &h) == 2 && w > 0 && h > 0)
                    out.commands.push_back(compile_resize(node->name, w, h));
                ed.edit_node.clear();
                ed.edit_field.clear();
            } else if (ImGui::IsItemDeactivated()) {
                ed.edit_node.clear();
                ed.edit_field.clear();
            }
        }
        ImGui::PopID();
    }

    if (node->fields.empty()) ImGui::TextDisabled("no editable fields");

    static const WidgetRegistry k_defaults = WidgetRegistry::defaults();
    const WidgetRegistry& reg = widgets ? *widgets : k_defaults;
    WidgetContext wctx{scene, out.commands, {}, 0.0f};
    for (const auto& f : node->fields) widget_field(wctx, reg, *node, f);
    return out;
}

} // namespace maiz
