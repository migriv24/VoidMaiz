/* face.cpp — the node-face widgets, as thin frames over the widget protocol's
 * field-editor kit (widget.cpp — one implementation of each staging
 * discipline, two frames around it). Only face_image lives here in full: it
 * is face-only (an image click is domain semantics, not a field commit). */
#include "voidmaiz/face.hpp"
#include "voidmaiz/widget.hpp"

#include <algorithm>

namespace maiz {

namespace {

WidgetContext widget_ctx(FaceContext& ctx) {
    return WidgetContext{ctx.scene, ctx.commands, {}, ctx.size.x};
}

} // namespace

bool face_drag_number(FaceContext& ctx, const char* field_key, float speed, float vmin,
                      float vmax) {
    WidgetContext w = widget_ctx(ctx);
    return widget_field_number(w, ctx.node, field_key, speed, vmin, vmax);
}

bool face_text_multiline(FaceContext& ctx, const char* field_key, float height) {
    WidgetContext w = widget_ctx(ctx);
    return widget_field_multiline(w, ctx.node, field_key, height > 0 ? height : ctx.size.y);
}

bool face_combo(FaceContext& ctx, const char* field_key,
                const std::vector<std::string>& options) {
    WidgetContext w = widget_ctx(ctx);
    return widget_field_combo(w, ctx.node, field_key, options);
}

bool face_date(FaceContext& ctx, const char* field_key) {
    WidgetContext w = widget_ctx(ctx);
    return widget_field_date(w, ctx.node, field_key);
}

bool face_knob(FaceContext& ctx, const char* field_key, float vmin, float vmax,
               bool integer, float vdefault) {
    WidgetContext w = widget_ctx(ctx);
    return widget_field_knob(w, ctx.node, field_key, vmin, vmax, integer, vdefault,
                             ctx.zoom);
}

bool face_image(FaceContext& ctx, ImTextureID tex, float tex_w, float tex_h,
                const char* placeholder) {
    ImVec2 area = ctx.size;
    if (area.x < 4 || area.y < 4) return false;
    ImVec2 p0 = ImGui::GetCursorScreenPos(); // where the region actually lands
    ImGui::InvisibleButton("##face-image", area);
    bool clicked = ImGui::IsItemClicked();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1(p0.x + area.x, p0.y + area.y);

    if (tex && tex_w > 0 && tex_h > 0) {
        // aspect-fit: the largest centered rect that preserves the ratio
        float s = std::min(area.x / tex_w, area.y / tex_h);
        ImVec2 sz(tex_w * s, tex_h * s);
        ImVec2 a(p0.x + (area.x - sz.x) * 0.5f, p0.y + (area.y - sz.y) * 0.5f);
        dl->AddImage(tex, a, ImVec2(a.x + sz.x, a.y + sz.y));
    } else {
        // placeholder: the host is still loading/decoding, or there is none
        dl->AddRectFilled(p0, p1, IM_COL32(127, 127, 132, 40), 3.0f);
        dl->AddRect(p0, p1, IM_COL32(127, 127, 132, 110), 3.0f);
        ImVec2 ts = ImGui::CalcTextSize(placeholder);
        if (ts.x < area.x && ts.y < area.y)
            dl->AddText(ImVec2(p0.x + (area.x - ts.x) * 0.5f, p0.y + (area.y - ts.y) * 0.5f),
                        IM_COL32(127, 127, 132, 200), placeholder);
    }
    return clicked;
}

} // namespace maiz
