/* presence.cpp — networking, the half an application shows (voidmaiz/presence.hpp). */
#include "voidmaiz/presence.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace maiz {

// ── marks ────────────────────────────────────────────────────────────────────

std::string_view mark_name(Mark m) {
    switch (m) {
    case Mark::Outline: return "outline";
    case Mark::Badge: return "badge";
    case Mark::Tint: return "tint";
    case Mark::None: return "none";
    }
    return "outline";
}

bool parse_mark(std::string_view text, Mark& out) {
    for (Mark m : {Mark::Outline, Mark::Badge, Mark::Tint, Mark::None})
        if (text == mark_name(m)) {
            out = m;
            return true;
        }
    return false;
}

// ── surfaces ─────────────────────────────────────────────────────────────────

void Surfaces::begin_frame() { decls_.clear(); }

SurfaceDecl& Surfaces::decl_for(std::string_view id) {
    for (auto& d : decls_)
        if (d.id == id) return d;
    decls_.push_back({});
    decls_.back().id = std::string(id);
    return decls_.back();
}

void Surfaces::declare(std::string_view surface_id, std::string_view kind, Mark mark) {
    SurfaceDecl& d = decl_for(surface_id);
    if (!kind.empty()) d.kind = std::string(kind);
    d.mark = mark;
}

void Surfaces::show(std::string_view surface_id, std::string_view rune_id) {
    if (rune_id.empty()) return;
    SurfaceDecl& d = decl_for(surface_id);
    // a list that draws the same rune twice still shows it once
    if (std::find(d.runes.begin(), d.runes.end(), rune_id) == d.runes.end())
        d.runes.emplace_back(rune_id);
}

void Surfaces::focus(std::string_view surface_id) {
    for (auto& d : decls_) d.focused = false;
    decl_for(surface_id).focused = true;
}

const SurfaceDecl* Surfaces::find(std::string_view surface_id) const {
    for (const auto& d : decls_)
        if (d.id == surface_id) return &d;
    return nullptr;
}

const SurfaceDecl* Surfaces::focused() const {
    for (const auto& d : decls_)
        if (d.focused) return &d;
    return nullptr;
}

std::vector<std::string> Surfaces::surfaces_showing(std::string_view rune_id) const {
    std::vector<std::string> out;
    for (const auto& d : decls_)
        if (std::find(d.runes.begin(), d.runes.end(), rune_id) != d.runes.end())
            out.push_back(d.id);
    return out;
}

// ── sharing ──────────────────────────────────────────────────────────────────

ShareFilter share_by_tag(std::string tag) {
    return [tag = std::move(tag)](const SceneNode& n) {
        return std::find(n.tags.begin(), n.tags.end(), tag) == n.tags.end();
    };
}

const SceneNode* find_by_id(const Scene& scene, std::string_view rune_id) {
    for (const auto& n : scene.nodes)
        if (n.id == rune_id) return &n;
    return nullptr;
}

std::vector<std::string> shareable_runes(const Scene& scene, const ShareFilter& filter) {
    std::vector<std::string> out;
    for (const auto& n : scene.nodes)
        if (!filter || filter(n)) out.push_back(n.id);
    return out;
}

std::vector<std::string> selection_ids(const Scene& scene, const std::vector<std::string>& names) {
    std::vector<std::string> out;
    for (const auto& name : names)
        if (const SceneNode* n = scene.find(name)) out.push_back(n->id);
    return out;
}

PresenceState compose_presence(const Profile& self, const std::vector<std::string>& selection,
                               const Surfaces& surfaces, const SharePolicy& policy,
                               const Scene& scene, const ShareFilter& filter) {
    PresenceState s;
    s.who = self;
    if (policy.selection) {
        for (const auto& id : selection) {
            const SceneNode* n = find_by_id(scene, id);
            // rule 3: a rune that stays here is not even named; and a rune the
            // sender cannot find is not vouched for
            if (!n || (filter && !filter(*n))) continue;
            s.selection.push_back(id);
        }
    }
    if (policy.surfaces) {
        for (const auto& d : surfaces.list()) s.surfaces.push_back(d.id);
        if (const SurfaceDecl* f = surfaces.focused()) s.focus = f->id;
    }
    return s;
}

// ── codec ────────────────────────────────────────────────────────────────────

namespace {

void add_list(cJSON* obj, const char* key, const std::vector<std::string>& items) {
    cJSON* arr = cJSON_AddArrayToObject(obj, key);
    for (const auto& s : items) cJSON_AddItemToArray(arr, cJSON_CreateString(s.c_str()));
}

bool fail(std::string* error, const char* why) {
    if (error) *error = why;
    return false;
}

/* Read an optional string member. Absent is fine; present-but-wrong is not. */
bool read_str(const cJSON* obj, const char* key, std::string& out, const PresenceLimits& lim,
              std::string* error) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) return true;
    if (!cJSON_IsString(v) || !v->valuestring) return fail(error, "a string field is not a string");
    std::size_t n = std::string_view(v->valuestring).size();
    if (n > lim.max_str) return fail(error, "a string exceeds max_str");
    out = v->valuestring;
    return true;
}

bool read_list(const cJSON* obj, const char* key, std::vector<std::string>& out,
               const PresenceLimits& lim, std::string* error) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) return true;
    if (!cJSON_IsArray(v)) return fail(error, "a list field is not an array");
    if ((std::size_t)cJSON_GetArraySize(v) > lim.max_ids)
        return fail(error, "a list exceeds max_ids");
    const cJSON* it = nullptr;
    cJSON_ArrayForEach(it, v) {
        if (!cJSON_IsString(it) || !it->valuestring) return fail(error, "a list holds a non-string");
        if (std::string_view(it->valuestring).size() > lim.max_str)
            return fail(error, "a string exceeds max_str");
        out.emplace_back(it->valuestring);
    }
    return true;
}

} // namespace

std::string presence_to_json(const PresenceState& st) {
    cJSON* root = cJSON_CreateObject();
    cJSON* who = cJSON_AddObjectToObject(root, "who");
    cJSON_AddStringToObject(who, "id", st.who.id.c_str());
    cJSON_AddStringToObject(who, "name", st.who.name.c_str());
    char rgb[8];
    std::snprintf(rgb, sizeof rgb, "#%06x", st.who.rgb & 0xFFFFFFu);
    cJSON_AddStringToObject(who, "rgb", rgb);
    if (!st.who.avatar.empty()) cJSON_AddStringToObject(who, "avatar", st.who.avatar.c_str());
    add_list(root, "selection", st.selection);
    add_list(root, "surfaces", st.surfaces);
    if (!st.focus.empty()) cJSON_AddStringToObject(root, "focus", st.focus.c_str());
    char* text = cJSON_PrintUnformatted(root);
    std::string out = text ? text : "{}";
    cJSON_free(text);
    cJSON_Delete(root);
    return out;
}

bool presence_from_json(std::string_view json, PresenceState& out, std::string* error,
                        const PresenceLimits& lim) {
    if (json.size() > lim.max_bytes) return fail(error, "payload exceeds max_bytes");
    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    if (!root) return fail(error, "not JSON");
    PresenceState st;
    bool ok = false;
    do {
        if (!cJSON_IsObject(root)) { fail(error, "not an object"); break; }
        const cJSON* who = cJSON_GetObjectItemCaseSensitive(root, "who");
        if (!cJSON_IsObject(who)) { fail(error, "missing `who`"); break; }
        if (!read_str(who, "id", st.who.id, lim, error)) break;
        if (st.who.id.empty()) { fail(error, "missing `who.id`"); break; }
        if (!read_str(who, "name", st.who.name, lim, error)) break;
        if (!read_str(who, "avatar", st.who.avatar, lim, error)) break;
        std::string rgb;
        if (!read_str(who, "rgb", rgb, lim, error)) break;
        if (rgb.size() == 7 && rgb[0] == '#') {
            char* end = nullptr;
            unsigned long v = std::strtoul(rgb.c_str() + 1, &end, 16);
            if (end && *end == '\0') st.who.rgb = (unsigned)v;
        }
        if (!read_list(root, "selection", st.selection, lim, error)) break;
        if (!read_list(root, "surfaces", st.surfaces, lim, error)) break;
        if (!read_str(root, "focus", st.focus, lim, error)) break;
        ok = true;
    } while (false);
    cJSON_Delete(root);
    if (ok) out = std::move(st); // refused whole, or accepted whole
    return ok;
}

// ── roster ───────────────────────────────────────────────────────────────────

bool Roster::update(const PresenceState& st, double now) {
    if (st.who.id.empty() || (!self_.empty() && st.who.id == self_)) return false;
    for (auto& p : peers_)
        if (p.state.who.id == st.who.id) {
            p.state = st;
            p.seen = now;
            return true;
        }
    peers_.push_back({st, now});
    return true;
}

void Roster::leave(std::string_view peer_id) {
    peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                [&](const Peer& p) { return p.state.who.id == peer_id; }),
                 peers_.end());
}

int Roster::prune(double now, double ttl) {
    std::size_t before = peers_.size();
    peers_.erase(std::remove_if(peers_.begin(), peers_.end(),
                                [&](const Peer& p) { return now - p.seen > ttl; }),
                 peers_.end());
    return (int)(before - peers_.size());
}

const Peer* Roster::find(std::string_view peer_id) const {
    for (const auto& p : peers_)
        if (p.state.who.id == peer_id) return &p;
    return nullptr;
}

std::vector<const Peer*> Roster::on_rune(std::string_view rune_id) const {
    std::vector<const Peer*> out;
    for (const auto& p : peers_)
        if (std::find(p.state.selection.begin(), p.state.selection.end(), rune_id) !=
            p.state.selection.end())
            out.push_back(&p);
    return out;
}

std::vector<const Peer*> Roster::on_surface(std::string_view surface_id) const {
    std::vector<const Peer*> out;
    for (const auto& p : peers_)
        if (p.state.focus == surface_id ||
            std::find(p.state.surfaces.begin(), p.state.surfaces.end(), surface_id) !=
                p.state.surfaces.end())
            out.push_back(&p);
    return out;
}

} // namespace maiz
