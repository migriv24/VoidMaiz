/*
 * voidmaiz/netview.hpp — how networking LOOKS, the same on every surface.
 *
 * The view half of okf/concepts/networking.md. voidmaiz/presence.hpp decides
 * who is where and what may leave; this header draws it, and it is the ONLY
 * place presence is drawn — so a peer's colour, a selection outline and a
 * "stays on this device" lock look identical in the canvas, a list, a map
 * marker and a calendar cell, because none of those views draws them itself.
 *
 * How a view joins, by shape:
 *
 *   an ImGui item (a row, a button, a card)   presence_item(...) right after it
 *   a custom-drawn thing (a marker, a cell)   presence_rect(...) with its rect
 *   the node canvas                           pass a CanvasNet to edit_canvas
 *   a tab or window as a whole                presence_surface_badges(...)
 *
 * Every one of them DECLARES into the frame's Surfaces as a side effect, so
 * drawing presence and being visible to presence are one call, not two a view
 * could forget half of.
 *
 * Nothing here dispatches a command or writes the model: presence is ephemera.
 * The settings and profile editors return true when edited; the application
 * persists NetSettings wherever it keeps device preferences.
 *
 * View-module header (ImGui types).
 */
#pragma once

#include "voidmaiz/presence.hpp"

#include "imgui.h"

#include <string_view>
#include <vector>

namespace maiz {

/* A peer's colour as ImGui packs it. */
ImU32 peer_color(const Profile& who, float alpha = 1.0f);

/* A disc in the peer's colour with their initials (the avatar reference is the
 * application's to resolve into a texture; a disc is the universal fallback and
 * what small sizes use regardless). */
void draw_avatar(ImDrawList* dl, ImVec2 center, float radius, const Profile& who);

/* The one renderer. `peers` are the people on this thing; nothing is drawn for
 * an empty list or Mark::None. Several peers on one outline nest as concentric
 * rings rather than overdrawing, so two people on one node read as two. */
void draw_presence_mark(ImDrawList* dl, ImVec2 min, ImVec2 max, Mark mark,
                        const std::vector<const Peer*>& peers, float rounding = 4.0f,
                        float scale = 1.0f);

/* "This stays on this device": a small padlock in the top-right corner, drawn
 * with primitives so it needs no icon font. Shown only on things the share
 * filter keeps local, so a person can SEE what is private rather than having
 * to remember which tag means it. */
void draw_private_mark(ImDrawList* dl, ImVec2 min, ImVec2 max, float scale = 1.0f);

/* An ImGui item just drew a thing that shows `rune_id`: declare it, and mark it.
 * Call immediately after the item. `local_only` draws the padlock. */
void presence_item(Surfaces& surfaces, const Roster& roster, const PresenceDisplay& display,
                   std::string_view surface_id, std::string_view rune_id,
                   Mark mark = Mark::Badge, bool local_only = false);

/* The same for something the view drew itself, at a screen rect. */
void presence_rect(Surfaces& surfaces, const Roster& roster, const PresenceDisplay& display,
                   std::string_view surface_id, std::string_view rune_id, ImVec2 min, ImVec2 max,
                   Mark mark = Mark::Outline, bool local_only = false);

/* Mark the surface this window belongs to as the one the person is working in,
 * if the window has focus. Call inside the window. */
void presence_focus_if_active(Surfaces& surfaces, std::string_view surface_id);

/* Avatars of the peers who have `surface_id` open, drawn inline at the cursor
 * (SameLine-friendly — beside a tab label, a window title, a section header).
 * Collapses to "+N" past `max_shown`: the author's warning was tabs "filled
 * with a ton of like, circles and profile icons". Honours
 * display.surface_badges. */
void presence_surface_badges(const Roster& roster, const PresenceDisplay& display,
                             std::string_view surface_id, int max_shown = 3);

/* Who is here: self first, then peers, each with what they are focused on. */
void draw_member_list(const Roster& roster, const Profile& self);

/* ── what a merge could not decide ───────────────────────────────────────────
 * A conflict is a QUESTION, not an error, and the only surface that can answer
 * it is one a person is looking at. These draw the rows voidmaiz_net projects
 * (plain strings — this module links no sync library).
 *
 * `draw_conflicts` returns the choice made this frame: `row` is the conflict's
 * id and `side` the index chosen; both empty/-1 when nothing was chosen. Hand
 * them to Network::resolve, which records the choice as this device's own act
 * so it syncs like any other change.
 *
 * Deliberately NOT a modal. A merge can arrive while someone is mid-sentence,
 * and a dialog that steals the frame would make an automatic sync feel like an
 * interruption every time two people work at once. It is a panel; the document
 * keeps the conflict until somebody answers. */
struct ConflictChoice {
    std::string row;
    int side = -1;
};

ConflictChoice draw_conflicts(const std::vector<ConflictRow>& rows);

/* Anomalies ask for an edit rather than a choice (two runes with one name, a
 * link a concurrent removal broke), so this only explains them. */
void draw_anomalies(const std::vector<AnomalyRow>& rows);

/* Name and colour. Returns true the frame either changed. */
bool draw_profile_editor(Profile& self);

/* The Networking section of an application's Settings: the profile, the
 * SENDER switches and the RECEIVER switches as two visibly separate groups
 * (rule 2 of presence.hpp — one hides things from you, the other hides you from
 * others), and cautious file transfer. Returns true the frame anything
 * changed. */
bool draw_network_settings(NetSettings& settings);

} // namespace maiz
