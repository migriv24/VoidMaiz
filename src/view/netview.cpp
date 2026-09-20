/* netview.cpp — how networking looks (voidmaiz/netview.hpp). */
#include "voidmaiz/netview.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>

namespace maiz {

namespace {

/* Up to two initials: "Maria Lopez" → "ML", "ana" → "A". */
std::string initials(const Profile& who) {
    std::string out;
    bool start = true;
    const std::string& src = who.name.empty() ? who.id : who.name;
    for (char c : src) {
        if (std::isspace((unsigned char)c)) {
            start = true;
            continue;
        }
        if (start && out.size() < 2) out += (char)std::toupper((unsigned char)c);
        start = false;
    }
    return out.empty() ? "?" : out;
}

ImU32 contrast(unsigned rgb) {
    float l = 0.299f * ((rgb >> 16) & 0xFF) + 0.587f * ((rgb >> 8) & 0xFF) + 0.114f * (rgb & 0xFF);
    return l > 150.0f ? IM_COL32(20, 20, 24, 255) : IM_COL32(250, 250, 252, 255);
}

} // namespace

ImU32 peer_color(const Profile& who, float alpha) {
    unsigned c = who.rgb;
    return IM_COL32((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF,
                    (int)(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
}

void draw_avatar(ImDrawList* dl, ImVec2 center, float radius, const Profile& who) {
    dl->AddCircleFilled(center, radius, peer_color(who));
    dl->AddCircle(center, radius, IM_COL32(0, 0, 0, 90), 0, 1.0f);
    std::string ini = initials(who);
    float size = radius * 1.1f;
    ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, ini.c_str());
    dl->AddText(nullptr, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f),
                contrast(who.rgb), ini.c_str());
}

void draw_presence_mark(ImDrawList* dl, ImVec2 min, ImVec2 max, Mark mark,
                        const std::vector<const Peer*>& peers, float rounding, float scale) {
    if (peers.empty() || mark == Mark::None) return;
    switch (mark) {
    case Mark::Outline: {
        // concentric rings, outward: two people on one thing read as two
        float thick = 2.0f * scale, gap = 3.0f * scale;
        for (std::size_t i = 0; i < peers.size() && i < 4; ++i) {
            float pad = gap + (float)i * (thick + 1.0f * scale);
            dl->AddRect(ImVec2(min.x - pad, min.y - pad), ImVec2(max.x + pad, max.y + pad),
                        peer_color(peers[i]->state.who), rounding + pad, 0, thick);
        }
        break;
    }
    case Mark::Tint: {
        // one wash per peer, faint enough that three still leave text readable
        for (std::size_t i = 0; i < peers.size() && i < 3; ++i)
            dl->AddRectFilled(min, max, peer_color(peers[i]->state.who, 0.14f), rounding);
        dl->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + 3.0f * scale, max.y),
                          peer_color(peers[0]->state.who), rounding);
        break;
    }
    case Mark::Badge:
    default: {
        // avatars along the right edge, overlapping like a stack of cards
        float r = std::min(9.0f * scale, (max.y - min.y) * 0.42f);
        if (r < 3.0f) r = 3.0f;
        float x = max.x - r - 2.0f * scale, y = (min.y + max.y) * 0.5f;
        for (std::size_t i = 0; i < peers.size() && i < 3; ++i) {
            draw_avatar(dl, ImVec2(x, y), r, peers[i]->state.who);
            x -= r * 1.4f;
        }
        if (peers.size() > 3) {
            char more[8];
            std::snprintf(more, sizeof more, "+%d", (int)peers.size() - 3);
            ImVec2 ts = ImGui::CalcTextSize(more);
            dl->AddText(ImVec2(x - ts.x * 0.5f + r, y - ts.y * 0.5f),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled), more);
        }
        break;
    }
    }
}

void draw_private_mark(ImDrawList* dl, ImVec2 min, ImVec2 max, float scale) {
    /* TOP-RIGHT, deliberately. Top-left is where node chrome lives (the canvas
     * draws a port marker there), and the corner can never collide with a
     * presence badge: a private rune is never named in anyone's presence, so no
     * peer can have it selected and nothing else is ever drawn on it. */
    ImU32 col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    float w = 8.0f * scale, h = 6.0f * scale;
    ImVec2 body(max.x - w - 6.0f * scale, min.y + 4.0f * scale + h * 0.8f);
    dl->AddRectFilled(body, ImVec2(body.x + w, body.y + h), col, 1.5f * scale);
    // the shackle: an arc over the body
    dl->PathArcTo(ImVec2(body.x + w * 0.5f, body.y), w * 0.32f, 3.14159f, 6.28318f, 10);
    dl->PathStroke(col, 0, 1.6f * scale);
}

void presence_rect(Surfaces& surfaces, const Roster& roster, const PresenceDisplay& display,
                   std::string_view surface_id, std::string_view rune_id, ImVec2 min, ImVec2 max,
                   Mark mark, bool local_only) {
    surfaces.declare(surface_id, {}, mark);
    surfaces.show(surface_id, rune_id);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (display.marks) draw_presence_mark(dl, min, max, mark, roster.on_rune(rune_id));
    if (local_only && display.private_marks) draw_private_mark(dl, min, max);
}

void presence_item(Surfaces& surfaces, const Roster& roster, const PresenceDisplay& display,
                   std::string_view surface_id, std::string_view rune_id, Mark mark,
                   bool local_only) {
    presence_rect(surfaces, roster, display, surface_id, rune_id, ImGui::GetItemRectMin(),
                  ImGui::GetItemRectMax(), mark, local_only);
}

void presence_focus_if_active(Surfaces& surfaces, std::string_view surface_id) {
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) surfaces.focus(surface_id);
}

void presence_surface_badges(const Roster& roster, const PresenceDisplay& display,
                             std::string_view surface_id, int max_shown) {
    if (!display.surface_badges) return;
    std::vector<const Peer*> here = roster.on_surface(surface_id);
    if (here.empty()) return;
    float r = ImGui::GetTextLineHeight() * 0.42f;
    ImVec2 at = ImGui::GetCursorScreenPos();
    float y = at.y + ImGui::GetTextLineHeight() * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int shown = std::min((int)here.size(), std::max(max_shown, 1));
    float x = at.x + r;
    for (int i = 0; i < shown; ++i) {
        draw_avatar(dl, ImVec2(x, y), r, here[i]->state.who);
        x += r * 1.5f;
    }
    float width = x - at.x + r * 0.5f;
    if ((int)here.size() > shown) {
        char more[8];
        std::snprintf(more, sizeof more, "+%d", (int)here.size() - shown);
        dl->AddText(ImVec2(x, at.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), more);
        width += ImGui::CalcTextSize(more).x;
    }
    ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight()));
}

void draw_member_list(const Roster& roster, const Profile& self) {
    auto row = [](const Profile& who, const char* where, bool me) {
        float r = ImGui::GetTextLineHeight() * 0.5f;
        ImVec2 at = ImGui::GetCursorScreenPos();
        draw_avatar(ImGui::GetWindowDrawList(), ImVec2(at.x + r, at.y + r), r, who);
        ImGui::Dummy(ImVec2(r * 2.0f, r * 2.0f));
        ImGui::SameLine();
        ImGui::TextUnformatted(who.name.empty() ? who.id.c_str() : who.name.c_str());
        if (me) {
            ImGui::SameLine();
            ImGui::TextDisabled("(you)");
        } else if (where && *where) {
            ImGui::SameLine();
            ImGui::TextDisabled("· %s", where);
        }
    };
    row(self, nullptr, true);
    for (const auto& p : roster.peers()) row(p.state.who, p.state.focus.c_str(), false);
    if (roster.peers().empty()) ImGui::TextDisabled("nobody else is here");
}

bool draw_profile_editor(Profile& self) {
    bool changed = false;
    char buf[128];
    std::snprintf(buf, sizeof buf, "%s", self.name.c_str());
    if (ImGui::InputText("name", buf, sizeof buf)) {
        self.name = buf;
        changed = true;
    }
    float col[3] = {((self.rgb >> 16) & 0xFF) / 255.0f, ((self.rgb >> 8) & 0xFF) / 255.0f,
                    (self.rgb & 0xFF) / 255.0f};
    if (ImGui::ColorEdit3("colour", col, ImGuiColorEditFlags_NoInputs)) {
        auto b = [](float v) { return (unsigned)(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
        self.rgb = (b(col[0]) << 16) | (b(col[1]) << 8) | b(col[2]);
        changed = true;
    }
    ImGui::SameLine();
    float r = ImGui::GetFrameHeight() * 0.5f;
    ImVec2 at = ImGui::GetCursorScreenPos();
    draw_avatar(ImGui::GetWindowDrawList(), ImVec2(at.x + r, at.y + r), r, self);
    ImGui::Dummy(ImVec2(r * 2.0f, r * 2.0f));
    return changed;
}

bool draw_network_settings(NetSettings& s) {
    bool changed = false;
    ImGui::PushID("vm-net-settings");

    ImGui::SeparatorText("You");
    changed |= draw_profile_editor(s.self);

    /* Two groups, deliberately apart. The first decides what LEAVES this
     * device — the only switches that are privacy. The second decides what you
     * see of others — clutter control. Mixing them would invite someone to hide
     * avatars and believe they had hidden themselves. */
    ImGui::SeparatorText("What others can see of you");
    changed |= ImGui::Checkbox("share what I have selected", &s.send.selection);
    changed |= ImGui::Checkbox("share which views I have open", &s.send.surfaces);

    ImGui::SeparatorText("What you see of others");
    changed |= ImGui::Checkbox("mark what others have selected", &s.show.marks);
    changed |= ImGui::Checkbox("show who is in each view", &s.show.surface_badges);
    changed |= ImGui::Checkbox("show which of my items stay on this device", &s.show.private_marks);

    ImGui::SeparatorText("Files");
    changed |= ImGui::Checkbox("cautious file transfer", &s.cautious_files);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Files other members add are not downloaded until you ask;\n"
                          "a placeholder is shown instead.");

    ImGui::PopID();
    return changed;
}

// ── what a merge could not decide ────────────────────────────────────────────

namespace {

/* A canonical value as a person should read it: JSON strings lose their quotes,
 * and anything long is cut for the button face (the full text is a tooltip). */
std::string readable(const std::string& canonical) {
    std::string v = canonical;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    if (v.empty()) return "(empty)";
    return v;
}

std::string elide(const std::string& v, std::size_t n = 48) {
    return v.size() <= n ? v : v.substr(0, n) + "…";
}

/* "content.text on `budget`" — the thing a person recognizes, not an id. */
std::string subject_of(const ConflictRow& r) {
    if (!r.glyph.empty()) return "the `" + r.glyph + "` declaration";
    std::string who = !r.rune_name.empty() ? "`" + r.rune_name + "`"
                      : !r.rune.empty()    ? r.rune
                                           : "the mantle `" + r.mantle + "`";
    return r.field + " on " + who;
}

} // namespace

ConflictChoice draw_conflicts(const std::vector<ConflictRow>& rows) {
    ConflictChoice chose;
    if (rows.empty()) {
        ImGui::TextDisabled("nothing to decide");
        return chose;
    }
    ImGui::PushID("vm-conflicts");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const ConflictRow& r = rows[i];
        ImGui::PushID((int)i);
        ImGui::Separator();

        if (r.kind == "deleted") {
            /* The one a silent merge would lose: someone's edit vanishing
             * because someone else deleted the thing. Say it in those words. */
            ImGui::TextWrapped("%s was deleted on one device and edited on another.",
                               subject_of(r).c_str());
        } else {
            ImGui::TextWrapped("%s was changed on two devices at once.", subject_of(r).c_str());
        }
        ImGui::Spacing();

        for (std::size_t k = 0; k < r.sides.size(); ++k) {
            std::string full = readable(r.sides[k]);
            std::string face = elide(full);
            if (ImGui::Button((face + "##" + std::to_string(k)).c_str())) {
                chose.row = r.id;
                chose.side = (int)k;
            }
            if (full != face && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", full.c_str());
            if (k + 1 < r.sides.size()) ImGui::SameLine();
        }
        ImGui::PopID();
    }
    ImGui::PopID();
    return chose;
}

void draw_anomalies(const std::vector<AnomalyRow>& rows) {
    if (rows.empty()) {
        ImGui::TextDisabled("nothing broken");
        return;
    }
    for (const auto& a : rows) {
        if (a.kind == "duplicate_name")
            ImGui::TextWrapped("Two things are now called `%s` in `%s`. Rename one.",
                               a.subject.c_str(), a.mantle.c_str());
        else if (a.kind == "link_broken")
            ImGui::TextWrapped("A link to `%s` broke: it was %s on another device.",
                               a.subject.c_str(),
                               a.cause.empty() ? "changed" : a.cause.c_str());
        else if (a.kind == "type_removed")
            ImGui::TextWrapped("`%s` no longer has a declared type here.", a.subject.c_str());
        else
            ImGui::TextWrapped("%s: %s", a.kind.c_str(), a.subject.c_str());
    }
}

} // namespace maiz
