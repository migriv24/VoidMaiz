/* widget.cpp — the host-widget protocol: the field-editor kit + registry.
 * The staging discipline lives in ImGui itself: drags stage in the window's
 * state storage keyed by widget ID, text edits stage in the active InputText's
 * internal buffer (re-projection never yanks either), and every commit is ONE
 * compiled `set`/`setjson`. face.cpp's node-face widgets delegate here — the
 * kit is one implementation with two frames around it. */
#include "voidmaiz/widget.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/textinputview.hpp" // text_input_kind: every field tells the keyboard what it is

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace maiz {

namespace {

const SceneField* find_field(const SceneNode& n, const char* key) {
    for (const auto& f : n.fields)
        if (f.key == key) return &f;
    return nullptr;
}

float field_as_float(const SceneField& f) {
    const char* s = f.value_json.c_str();
    if (f.is_string && f.value_json.size() >= 2) s += 1; // skip the opening quote
    return (float)std::strtod(s, nullptr);
}

std::string format_number(float v) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

std::string commit_value(const SceneNode& node, const SceneField& f, const std::string& text) {
    return f.is_string ? compile_set(node.name, f.key, text)
                       : compile_setjson(node.name, f.key, text);
}

/* A field is "owned by the control graph" when an input port named like it
 * has a linguine feeding it — editors show it disabled (VLS #14b). Both
 * spellings match: `<key>` and `p:<key>` (the params-as-ports convention,
 * proven in the Python VLS and blessed 2026-07-16 — VLS-native ask §3). */
bool field_is_wired(const Scene& scene, const SceneNode& node, const char* key) {
    const std::string param = std::string("p:") + key;
    int index = -1;
    for (const auto& p : node.inputs)
        if (p.name == key || p.name == param) { index = p.index; break; }
    if (index < 0) return false;
    for (const auto& w : scene.wires)
        if (w.kind == SceneWire::Kind::Linguine && w.to == node.name && w.to_port == index)
            return true;
    return false;
}

/* Compact-JSON string → the actual text (the common escapes; \uXXXX passes
 * through verbatim — the commit round-trips it). */
std::string json_unquote(const std::string& value_json) {
    if (value_json.size() < 2 || value_json.front() != '"') return value_json;
    std::string out;
    out.reserve(value_json.size());
    for (size_t i = 1; i + 1 < value_json.size(); ++i) {
        char c = value_json[i];
        if (c != '\\') {
            out += c;
            continue;
        }
        if (++i + 1 > value_json.size()) break;
        switch (value_json[i]) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'u': out += "\\u"; break;
        default: out += value_json[i]; break;
        }
    }
    return out;
}

void apply_width(const WidgetContext& ctx) {
    if (ctx.width > 0) ImGui::SetNextItemWidth(ctx.width);
}

/* The visible ImGui label for a field control: show `label` (the glyph's
 * hints.labels entry) when given, else the raw key; the control's ID always
 * stays keyed on `key` via `##key`, so relabeling never resets a live edit. */
std::string field_label(const char* key, const char* label) {
    std::string s = (label && *label) ? label : key;
    s += "##";
    s += key;
    return s;
}

/* SceneField.label as a widget-kit `label` argument (empty → nullptr → key). */
const char* label_of(const SceneField& f) { return f.label.empty() ? nullptr : f.label.c_str(); }

/* "0.1,0,10" → floats (missing/blank entries keep the caller's defaults). */
void csv_floats(std::string_view args, float* out, int n) {
    size_t pos = 0;
    for (int i = 0; i < n && pos <= args.size(); ++i) {
        size_t comma = args.find(',', pos);
        std::string part(args.substr(pos, comma == std::string_view::npos ? args.size() - pos
                                                                          : comma - pos));
        if (!part.empty()) out[i] = (float)std::strtod(part.c_str(), nullptr);
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
}

std::vector<std::string> csv_strings(std::string_view args) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= args.size()) {
        size_t comma = args.find(',', pos);
        if (comma == std::string_view::npos) {
            if (pos < args.size()) out.emplace_back(args.substr(pos));
            break;
        }
        out.emplace_back(args.substr(pos, comma - pos));
        pos = comma + 1;
    }
    return out;
}

} // namespace

bool widget_visible(const WidgetContext& ctx, const SceneNode& node) {
    return node_matches(ctx.filter, node);
}

bool widget_field_text(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;

    /* String fields edit their text and commit `set`; anything else edits its
     * compact JSON and commits `setjson` (an invalid payload is rejected by
     * the core and visibly logged — the picture just doesn't change). */
    std::string projected = f->is_string ? json_unquote(f->value_json) : f->value_json;
    char buf[512];
    std::snprintf(buf, sizeof buf, "%s", projected.c_str());
    std::string lbl = field_label(field_key, label);

    bool committed = false;
    ImGui::PushID(node.name.c_str());
    apply_width(ctx);
    if (field_is_wired(ctx.scene, node, field_key)) {
        ImGui::BeginDisabled();
        ImGui::InputText(lbl.c_str(), buf, sizeof buf, ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
    } else {
        /* While active, ImGui's internal buffer owns the text — re-projection
         * never yanks a live edit; Escape reverts natively (the unchanged
         * value then compiles nothing). */
        bool entered = ImGui::InputText(lbl.c_str(), buf, sizeof buf,
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        // a `phone` field gets a dial pad and an `email` field an @, on any
        // platform keyboard (okf/concepts/text-input.md): registration picks it
        text_input_kind(input_kind_for(field_key));
        if ((entered || ImGui::IsItemDeactivatedAfterEdit()) && projected != buf) {
            ctx.commands.push_back(commit_value(node, *f, buf));
            committed = true;
        }
    }
    ImGui::PopID();
    return committed;
}

bool widget_field_number(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                         float speed, float vmin, float vmax, const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;
    std::string lbl = field_label(field_key, label);

    ImGui::PushID(node.name.c_str());
    if (field_is_wired(ctx.scene, node, field_key)) { // the wire owns it; show, don't edit
        float v = field_as_float(*f);
        apply_width(ctx);
        ImGui::BeginDisabled();
        ImGui::DragFloat(lbl.c_str(), &v, 0, 0, 0, "%.3g");
        ImGui::EndDisabled();
        ImGui::PopID();
        return false;
    }

    ImGuiID id = ImGui::GetID(field_key);
    ImGuiStorage* store = ImGui::GetStateStorage();
    bool staging = store->GetBool(id + 1, false);
    float v = staging ? store->GetFloat(id, 0.0f) : field_as_float(*f);

    apply_width(ctx);
    ImGui::DragFloat(lbl.c_str(), &v, speed, vmin, vmax, "%.3g");
    if (ImGui::IsItemActivated()) store->SetBool(id + 1, true);
    if (store->GetBool(id + 1, false)) store->SetFloat(id, v);

    bool committed = false;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        store->SetBool(id + 1, false);
        ctx.commands.push_back(commit_value(node, *f, format_number(v)));
        committed = true;
    } else if (ImGui::IsItemDeactivated()) {
        store->SetBool(id + 1, false); // abandoned
    }
    ImGui::PopID();
    return committed;
}

bool widget_field_bool(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;
    // A boolean field is non-string content; anything but the literal "true"
    // reads as false. A click is atomic — no staging — so it commits ONE
    // `setjson <key> true|false` immediately (nothing to abandon).
    bool v = (f->value_json == "true");
    std::string lbl = field_label(field_key, label);

    bool committed = false;
    ImGui::PushID(node.name.c_str());
    if (field_is_wired(ctx.scene, node, field_key)) {
        ImGui::BeginDisabled();
        ImGui::Checkbox(lbl.c_str(), &v);
        ImGui::EndDisabled();
    } else if (ImGui::Checkbox(lbl.c_str(), &v)) {
        ctx.commands.push_back(compile_setjson(node.name, field_key, v ? "true" : "false"));
        committed = true;
    }
    ImGui::PopID();
    return committed;
}

bool widget_field_multiline(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                            float height, const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;

    std::string projected = json_unquote(f->value_json);
    char buf[4096];
    std::snprintf(buf, sizeof buf, "%s", projected.c_str());
    std::string lbl = field_label(field_key, label);

    float w = ctx.width > 0 ? ctx.width : ImGui::CalcItemWidth();
    float h = height > 0 ? height
                         : ImGui::GetTextLineHeight() * 4.0f +
                               ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 area(w, h);

    bool committed = false;
    ImGui::PushID(node.name.c_str());
    if (field_is_wired(ctx.scene, node, field_key)) {
        ImGui::BeginDisabled();
        ImGui::InputTextMultiline(lbl.c_str(), buf, sizeof buf, area,
                                  ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
    } else {
        bool chorded = ImGui::InputTextMultiline(
            lbl.c_str(), buf, sizeof buf, area,
            ImGuiInputTextFlags_EnterReturnsTrue); // multiline: Ctrl+Enter commits,
                                                   // plain Enter stays a newline
        text_input_kind(InputKind::Multiline);     // a phone keyboard's newline key
        if ((chorded || ImGui::IsItemDeactivatedAfterEdit()) && projected != buf) {
            ctx.commands.push_back(compile_set(node.name, field_key, buf));
            committed = true;
        }
    }
    ImGui::PopID();
    return committed;
}

bool widget_field_combo(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                        const std::vector<std::string>& options, const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;

    std::string current = f->value_json;
    if (f->is_string && current.size() >= 2) current = current.substr(1, current.size() - 2);
    std::string lbl = field_label(field_key, label);

    bool committed = false;
    ImGui::PushID(node.name.c_str());
    apply_width(ctx);
    if (field_is_wired(ctx.scene, node, field_key)) {
        ImGui::BeginDisabled();
        if (ImGui::BeginCombo(lbl.c_str(), current.c_str())) ImGui::EndCombo();
        ImGui::EndDisabled();
    } else if (ImGui::BeginCombo(lbl.c_str(), current.c_str())) {
        for (const auto& opt : options) {
            if (ImGui::Selectable(opt.c_str(), opt == current) && opt != current) {
                ctx.commands.push_back(compile_set(node.name, field_key, opt));
                committed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return committed;
}

bool widget_field_date(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;

    int y = 2026, mo = 1, d = 1;
    {
        std::string cur = json_unquote(f->value_json);
        int py, pm, pd;
        if (std::sscanf(cur.c_str(), "%d-%d-%d", &py, &pm, &pd) == 3) {
            y = py;
            mo = pm;
            d = pd;
        }
    }

    ImGui::PushID(node.name.c_str());
    ImGuiID id = ImGui::GetID(field_key);
    ImGuiStorage* store = ImGui::GetStateStorage();
    bool staging = store->GetBool(id + 3, false);
    if (staging) { // a drag in flight: the staged date wins over re-projection
        y = store->GetInt(id, y);
        mo = store->GetInt(id + 1, mo);
        d = store->GetInt(id + 2, d);
    }

    bool committed = false;
    float total = ctx.width > 0 ? ctx.width : ImGui::CalcItemWidth();
    float w = (total - 2.0f * ImGui::GetStyle().ItemInnerSpacing.x) / 3.0f;
    int parts[3] = {y, mo, d};
    const char* fmts[3] = {"%04d", "%02d", "%02d"};
    int minv[3] = {1, 1, 1}, maxv[3] = {9999, 12, 31};
    const char* labels[3] = {"##y", "##m", "##d"};
    bool any_release = false;
    for (int i = 0; i < 3; ++i) {
        if (i) ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::SetNextItemWidth(w);
        ImGui::PushID(field_key);
        ImGui::DragInt(labels[i], &parts[i], 0.15f, minv[i], maxv[i], fmts[i]);
        if (ImGui::IsItemActivated()) store->SetBool(id + 3, true);
        if (ImGui::IsItemDeactivatedAfterEdit()) any_release = true;
        if (ImGui::IsItemDeactivated()) store->SetBool(id + 3, false);
        ImGui::PopID();
    }
    if (label && *label) { // the three boxes carry no inline label; add it here
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(label);
    }
    if (store->GetBool(id + 3, false)) { // stage the triple while any drags
        store->SetInt(id, parts[0]);
        store->SetInt(id + 1, parts[1]);
        store->SetInt(id + 2, parts[2]);
    }
    if (any_release) {
        // clamp the day to the month (leap-aware) and commit ONE ISO string
        static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int yy = parts[0], mm = std::clamp(parts[1], 1, 12);
        int cap = days[mm - 1];
        if (mm == 2 && ((yy % 4 == 0 && yy % 100 != 0) || yy % 400 == 0)) cap = 29;
        int dd = std::clamp(parts[2], 1, cap);
        char iso[16];
        std::snprintf(iso, sizeof iso, "%04d-%02d-%02d", yy, mm, dd);
        std::string prev = json_unquote(f->value_json);
        if (prev != iso) {
            ctx.commands.push_back(compile_set(node.name, field_key, iso));
            committed = true;
        }
    }
    ImGui::PopID();
    return committed;
}

bool widget_field_knob(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       float vmin, float vmax, bool integer, float vdefault, float scale,
                       const char* label) {
    // Lifted from VLS-native (native/src/faces.cpp::face_knob, offered
    // 2026-07-16); kit adaptations: commit through commit_value (a
    // string-typed field stays a string), colors from the ImGui style.
    const SceneField* f = find_field(node, field_key);
    if (!f || vmax <= vmin) return false;
    const char* disp = (label && *label) ? label : field_key; // shown; ID stays on the key
    const bool wired = field_is_wired(ctx.scene, node, field_key);
    const float z = std::max(0.35f, scale);
    constexpr float kPi = 3.14159265358979f;

    ImGui::PushID(node.name.c_str());
    ImGui::PushID(field_key);
    const ImVec2 item(58.0f * z, 60.0f * z);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    if (wired) ImGui::BeginDisabled();
    ImGui::InvisibleButton("##knob", item);
    const bool active = ImGui::IsItemActive();
    const bool activated = ImGui::IsItemActivated();
    const bool released = ImGui::IsItemDeactivated();
    const bool hovered = ImGui::IsItemHovered();
    if (wired) ImGui::EndDisabled();

    // ── staging: the drag owns a local value; the model hears ONE command ──
    ImGuiStorage* store = ImGui::GetStateStorage();
    const ImGuiID base_id = ImGui::GetID("##base");
    const bool unset =
        f->value_json.empty() || f->value_json == "null" || f->value_json == "\"\"";
    const float model_v = std::clamp(unset ? vdefault : field_as_float(*f), vmin, vmax);
    if (activated) store->SetFloat(base_id, model_v);

    float shown = model_v;
    bool emitted = false;
    if (active) {
        const float base = store->GetFloat(base_id, model_v);
        const float per_px = (vmax - vmin) / 160.0f; // full sweep ≈ 160 px
        shown = std::clamp(base - ImGui::GetMouseDragDelta(0, 0.0f).y * per_px, vmin, vmax);
        if (integer) shown = std::roundf(shown);
    }
    if (released && shown != model_v) {
        ctx.commands.push_back(commit_value(node, *f, format_number(shown)));
        emitted = true;
    }
    if (!wired && hovered && ImGui::IsMouseDoubleClicked(0) && model_v != vdefault) {
        ctx.commands.push_back(
            commit_value(node, *f, format_number(std::clamp(vdefault, vmin, vmax))));
        emitted = true;
    }

    // ── drawing: 270° sweep, gap at the bottom ──────────────────────────────
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = 15.0f * z;
    const ImVec2 c(p0.x + item.x * 0.5f, p0.y + r + 3.0f * z);
    const float a0 = 0.75f * kPi; // 135°
    const float a1 = 2.25f * kPi; // 405°
    const float frac = (shown - vmin) / (vmax - vmin);
    const float av = a0 + (a1 - a0) * frac;

    const ImU32 accent = wired ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                               : ImGui::GetColorU32(ImGuiCol_SliderGrab);
    const ImU32 body = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 track = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dim = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    dl->AddCircleFilled(c, r, body);
    dl->PathArcTo(c, r - 2.5f * z, a0, a1, 32);
    dl->PathStroke(track, 0, 2.5f * z);
    if (frac > 0.001f) {
        dl->PathArcTo(c, r - 2.5f * z, a0, av, 32);
        dl->PathStroke(accent, 0, 2.5f * z);
    }
    dl->AddLine(ImVec2(c.x + std::cos(av) * r * 0.30f, c.y + std::sin(av) * r * 0.30f),
                ImVec2(c.x + std::cos(av) * r * 0.82f, c.y + std::sin(av) * r * 0.82f),
                (active || hovered) && !wired ? accent : text, 2.0f * z);

    // label + live value beneath the dial
    char val[48];
    if (integer)
        std::snprintf(val, sizeof val, "%d", (int)shown);
    else
        std::snprintf(val, sizeof val, "%.4g", shown);
    const float fs = ImGui::GetFontSize() * 0.78f;
    ImVec2 ts = ImGui::CalcTextSize(disp);
    dl->AddText(nullptr, fs, ImVec2(c.x - ts.x * 0.39f, p0.y + 2.0f * r + 6.0f * z), dim,
                disp);
    ts = ImGui::CalcTextSize(val);
    dl->AddText(nullptr, fs, ImVec2(c.x - ts.x * 0.39f, p0.y + 2.0f * r + 6.0f * z + fs),
                active ? accent : text, val);

    if (hovered && !active)
        ImGui::SetTooltip(wired ? "%s — wired (the control graph owns it)" : "%s", disp);
    ImGui::PopID();
    ImGui::PopID();
    return emitted;
}

bool widget_field_path(WidgetContext& ctx, const SceneNode& node, const char* field_key,
                       const PathBrowseFn& browse, const char* label) {
    const SceneField* f = find_field(node, field_key);
    if (!f) return false;
    // A path is a string field: edit by hand (ONE `set` on commit, like text)
    // or via the host's dialog. The library never touches the filesystem.
    std::string projected = f->is_string ? json_unquote(f->value_json) : f->value_json;
    char buf[1024];
    std::snprintf(buf, sizeof buf, "%s", projected.c_str());
    const bool wired = field_is_wired(ctx.scene, node, field_key);

    bool committed = false;
    ImGui::PushID(node.name.c_str());
    ImGui::PushID(field_key);

    // the browse button sits at the right; the text box takes what's left
    const char* browse_txt = "…";
    const ImGuiStyle& st = ImGui::GetStyle();
    float btn_w = ImGui::CalcTextSize(browse_txt).x + st.FramePadding.x * 2.0f;
    float total = ctx.width > 0 ? ctx.width : ImGui::CalcItemWidth();
    float box_w = total - btn_w - st.ItemInnerSpacing.x;
    if (box_w < 40.0f) box_w = 40.0f;

    ImGui::SetNextItemWidth(box_w);
    if (wired) {
        ImGui::BeginDisabled();
        ImGui::InputText("##path", buf, sizeof buf, ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
    } else {
        bool entered = ImGui::InputText("##path", buf, sizeof buf,
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        if ((entered || ImGui::IsItemDeactivatedAfterEdit()) && projected != buf) {
            ctx.commands.push_back(compile_set(node.name, field_key, buf));
            committed = true;
        }
    }

    ImGui::SameLine(0, st.ItemInnerSpacing.x);
    if (wired) ImGui::BeginDisabled();
    if (ImGui::Button(browse_txt) && browse) {
        std::string chosen = browse(node, field_key, projected); // host owns the dialog
        if (!chosen.empty() && chosen != projected) {
            ctx.commands.push_back(compile_set(node.name, field_key, chosen));
            committed = true;
        }
    }
    if (wired) ImGui::EndDisabled();

    ImGui::SameLine(0, st.ItemInnerSpacing.x);
    ImGui::TextUnformatted((label && *label) ? label : field_key);

    ImGui::PopID();
    ImGui::PopID();
    return committed;
}

// ── the registry ─────────────────────────────────────────────────────────────

void split_editor_spec(std::string_view spec, std::string& kind, std::string& args) {
    size_t colon = spec.find(':');
    if (colon == std::string_view::npos) {
        kind = std::string(spec);
        args.clear();
    } else {
        kind = std::string(spec.substr(0, colon));
        args = std::string(spec.substr(colon + 1));
    }
}

WidgetRegistry WidgetRegistry::defaults() {
    WidgetRegistry reg;
    reg.editors["text"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                             std::string_view) {
        return widget_field_text(ctx, n, f.key.c_str(), label_of(f));
    };
    reg.editors["number"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                               std::string_view args) {
        float p[3] = {0.05f, 0.0f, 0.0f}; // speed, min, max
        csv_floats(args, p, 3);
        return widget_field_number(ctx, n, f.key.c_str(), p[0], p[1], p[2], label_of(f));
    };
    reg.editors["bool"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                             std::string_view) {
        return widget_field_bool(ctx, n, f.key.c_str(), label_of(f));
    };
    reg.editors["checkbox"] = reg.editors["bool"]; // a friendlier spelling of the same kind
    reg.editors["multiline"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                                  std::string_view args) {
        float h = 0.0f;
        csv_floats(args, &h, 1);
        return widget_field_multiline(ctx, n, f.key.c_str(), h, label_of(f));
    };
    reg.editors["combo"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                              std::string_view args) {
        return widget_field_combo(ctx, n, f.key.c_str(), csv_strings(args), label_of(f));
    };
    reg.editors["date"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                             std::string_view) {
        return widget_field_date(ctx, n, f.key.c_str(), label_of(f));
    };
    reg.editors["knob"] = [](WidgetContext& ctx, const SceneNode& n, const SceneField& f,
                             std::string_view args) {
        float p[4] = {0.0f, 1.0f, 0.0f, 0.0f}; // min, max, default, snap
        csv_floats(args, p, 4);
        return widget_field_knob(ctx, n, f.key.c_str(), p[0], p[1], p[3] != 0.0f, p[2], 1.0f,
                                 label_of(f));
    };
    return reg;
}

void WidgetRegistry::add_path(PathBrowseFn browse) {
    editors["path"] = [browse = std::move(browse)](WidgetContext& ctx, const SceneNode& n,
                                                   const SceneField& f, std::string_view) {
        return widget_field_path(ctx, n, f.key.c_str(), browse, label_of(f));
    };
}

bool widget_field(WidgetContext& ctx, const WidgetRegistry& reg, const SceneNode& node,
                  const SceneField& field) {
    if (!field.editor.empty()) {
        std::string kind, args;
        split_editor_spec(field.editor, kind, args);
        auto it = reg.editors.find(kind);
        if (it != reg.editors.end() && it->second) return it->second(ctx, node, field, args);
        // unknown kind: fall through — a missing registration must never make
        // a declared field uneditable
    }
    /* No DECLARED editor, but the glyph may have said what the number IS
     * (`kinds`, SPEC §3.3.2, Void Core 0.2.14). A quantity is schema, so it
     * may pick a DEFAULT — the presentation still outranks it, which is why
     * this sits after the `field.editor` branch and never before it.
     *
     * The inference is deliberately narrow, and stops exactly where the
     * measurement level stops licensing it: a bounded RATIO quantity is a
     * magnitude with a true zero and a full sweep, which is what a knob draws
     * honestly; an INTERVAL one (a date, a temperature) has no true zero, so a
     * sweep from `min` would draw a proportion that does not exist. Nominal and
     * ordinal are not numbers a drag control should touch at all. Everything
     * not covered falls through to text, which is where it was already. */
    if (field.quantity.present && field.quantity.level == "ratio") {
        if (field.quantity.bounded())
            return widget_field_knob(ctx, node, field.key.c_str(), (float)field.quantity.min,
                                     (float)field.quantity.max, /*integer=*/false,
                                     (float)field.quantity.min, /*scale=*/1.0f, label_of(field));
        return widget_field_number(ctx, node, field.key.c_str(), 0.05f,
                                   field.quantity.has_min ? (float)field.quantity.min : 0.0f,
                                   field.quantity.has_max ? (float)field.quantity.max : 0.0f,
                                   label_of(field));
    }
    return widget_field_text(ctx, node, field.key.c_str(), label_of(field));
}

bool widget_form(WidgetContext& ctx, const WidgetRegistry& reg, const SceneNode& node) {
    bool any = false;
    ImGui::PushID(node.name.c_str());
    ImGui::Text("%s", node.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", node.label.c_str());
    if (node.fields.empty()) ImGui::TextDisabled("no editable fields");
    for (const auto& f : node.fields)
        if (widget_field(ctx, reg, node, f)) any = true;
    ImGui::PopID();
    return any;
}

} // namespace maiz
