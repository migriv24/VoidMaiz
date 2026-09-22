/* presence.cpp — networking, the half an application shows (voidmaiz/presence.hpp). */
#include "voidmaiz/presence.hpp"

#include "voidmaiz/embed.hpp" // split_argv: reading a recent command line before it leaves

#include "cJSON.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <initializer_list>

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

std::string_view gesture_name(CanvasGesture g) {
    switch (g) {
    case CanvasGesture::None: return "none";
    case CanvasGesture::Move: return "move";
    case CanvasGesture::Wire: return "wire";
    case CanvasGesture::Marquee: return "marquee";
    case CanvasGesture::Resize: return "resize";
    case CanvasGesture::Typing: return "typing";
    }
    return "none";
}

bool parse_gesture(std::string_view text, CanvasGesture& out) {
    for (CanvasGesture g : {CanvasGesture::None, CanvasGesture::Move, CanvasGesture::Wire,
                            CanvasGesture::Marquee, CanvasGesture::Resize, CanvasGesture::Typing})
        if (text == gesture_name(g)) {
            out = g;
            return true;
        }
    return false;
}

// ── the profile, per machine ─────────────────────────────────────────────────

namespace {
std::filesystem::path profile_file(const std::filesystem::path& dir) { return dir / "profile.json"; }
} // namespace

Profile load_profile(const std::filesystem::path& dir) {
    Profile p;
    std::ifstream in(profile_file(dir), std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    cJSON* root = cJSON_ParseWithLength(text.data(), text.size());
    if (cJSON_IsObject(root)) {
        if (const cJSON* n = cJSON_GetObjectItemCaseSensitive(root, "name"); cJSON_IsString(n))
            p.name = n->valuestring;
        if (const cJSON* c = cJSON_GetObjectItemCaseSensitive(root, "rgb"); cJSON_IsString(c) &&
            std::string_view(c->valuestring).size() == 7)
            p.rgb = (unsigned)std::strtoul(c->valuestring + 1, nullptr, 16);
        if (const cJSON* i = cJSON_GetObjectItemCaseSensitive(root, "id"); cJSON_IsString(i))
            p.id = i->valuestring;
    }
    cJSON_Delete(root);
    if (p.name.empty()) { // first run: the machine's name, a colour of its own
        p.name = default_device_name();
        p.rgb = suggested_colour(p.name);
    }
    return p;
}

bool save_profile(const std::filesystem::path& dir, const Profile& p) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", p.name.c_str());
    char rgb[8];
    std::snprintf(rgb, sizeof rgb, "#%06x", p.rgb & 0xFFFFFFu);
    cJSON_AddStringToObject(root, "rgb", rgb);
    if (!p.id.empty()) cJSON_AddStringToObject(root, "id", p.id.c_str());
    char* text = cJSON_Print(root);
    std::filesystem::path tmp = profile_file(dir);
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << (text ? text : "{}");
    }
    cJSON_free(text);
    cJSON_Delete(root);
    std::filesystem::rename(tmp, profile_file(dir), ec); // atomic
    return !ec;
}

std::string default_device_name() {
    for (const char* var : {"COMPUTERNAME", "HOSTNAME", "HOST"})
        if (const char* v = std::getenv(var); v && *v) return v;
    return "This device";
}

unsigned suggested_colour(std::string_view seed) {
    // no red, yellow or blue: a peer's mark must not read as a node's pigment
    static const unsigned palette[] = {0x2f9e8f, 0xc2548a, 0x6f5bd6, 0x2b8fd9, 0x8a9b2e, 0xd9822b};
    std::size_t h = 1469598103934665603ull;
    for (char ch : seed) h = (h ^ (unsigned char)ch) * 1099511628211ull;
    return palette[h % 6];
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

PresenceState compose_presence(const Profile& self, const std::vector<std::string>& selection,
                               const Surfaces& surfaces, const SharePolicy& policy,
                               const Scene& scene, const ShareFilter& filter,
                               const CollabOut& collab) {
    PresenceState s = compose_presence(self, selection, surfaces, policy, scene, filter);
    s.kind = collab.kind;
    s.device = collab.device;
    s.compat = collab.compat;
    s.clock = collab.clock;

    // rule 3, applied to everything in flight: name nothing the sender cannot
    // vouch for, and nothing that stays on this device
    auto vouched = [&](const std::string& id) {
        const SceneNode* n = find_by_id(scene, id);
        return n && (!filter || filter(*n));
    };

    for (CanvasPresence c : collab.canvas) {
        if (!policy.cursor) c.has_cursor = c.has_view = false;
        switch (c.gesture) {
        case CanvasGesture::Move: // the ghost IS the selection, offset
            if (!policy.gestures || !policy.selection || s.selection.empty())
                c.gesture = CanvasGesture::None;
            break;
        case CanvasGesture::Wire:
            if (!policy.gestures || !vouched(c.wire_rune)) c.gesture = CanvasGesture::None;
            break;
        case CanvasGesture::Marquee:
            if (!policy.gestures) c.gesture = CanvasGesture::None;
            break;
        case CanvasGesture::Resize:
            if (!policy.gestures || !vouched(c.field_rune)) c.gesture = CanvasGesture::None;
            break;
        case CanvasGesture::Typing:
            if (!policy.typing || !vouched(c.field_rune)) c.gesture = CanvasGesture::None;
            else if (!policy.preview) c.preview.clear();
            break;
        case CanvasGesture::None: break;
        }
        if (c.gesture != CanvasGesture::Wire) {
            c.wire_rune.clear();
            c.wire_port = -1;
        }
        if (c.gesture != CanvasGesture::Typing && c.gesture != CanvasGesture::Resize) {
            c.field_rune.clear();
            c.field_key.clear();
        }
        if (c.gesture != CanvasGesture::Typing) c.preview.clear();
        s.canvas.push_back(std::move(c));
    }

    for (const Claim& cl : collab.claims)
        if (vouched(cl.rune) || (!cl.rune.empty() && cl.rune == scene.mantle))
            s.claims.push_back(cl);

    if (policy.recent) {
        // a command line naming a private rune (by name or id) is dropped whole:
        // a redacted line would still say "something here was touched"
        std::vector<const SceneNode*> hidden;
        for (const auto& n : scene.nodes)
            if (filter && !filter(n)) hidden.push_back(&n);
        for (const auto& line : collab.recent) {
            Argv a = split_argv(line);
            bool leaks = !a; // a line we cannot read is a line we cannot vouch for
            for (const auto& tok : a.argv)
                for (const SceneNode* n : hidden)
                    if (tok == n->name || tok == n->id) leaks = true;
            if (!leaks) s.recent.push_back(line);
        }
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

void add_nums(cJSON* obj, const char* key, std::initializer_list<float> v) {
    cJSON* arr = cJSON_AddArrayToObject(obj, key);
    for (float f : v) cJSON_AddItemToArray(arr, cJSON_CreateNumber(f));
}

/* An optional array of exactly `n` finite numbers within ±max_coord. Refused
 * rather than clamped: a clamped cursor is a lie about where someone is. */
bool read_nums(const cJSON* obj, const char* key, float* out, int n, bool& present,
               const PresenceLimits& lim, std::string* error) {
    present = false;
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) return true;
    if (!cJSON_IsArray(v) || cJSON_GetArraySize(v) != n)
        return fail(error, "a coordinate field has the wrong shape");
    int i = 0;
    const cJSON* it = nullptr;
    cJSON_ArrayForEach(it, v) {
        if (!cJSON_IsNumber(it) || !std::isfinite(it->valuedouble) ||
            std::fabs(it->valuedouble) > lim.max_coord)
            return fail(error, "a coordinate is not a finite number in range");
        out[i++] = (float)it->valuedouble;
    }
    present = true;
    return true;
}

/* An optional non-negative integer no larger than `max`. */
bool read_uint(const cJSON* obj, const char* key, double max, std::uint64_t& out,
               std::string* error) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v) return true;
    if (!cJSON_IsNumber(v) || !std::isfinite(v->valuedouble) || v->valuedouble < 0 ||
        v->valuedouble > max || v->valuedouble != std::floor(v->valuedouble))
        return fail(error, "an integer field is out of range");
    out = (std::uint64_t)v->valuedouble;
    return true;
}

bool read_str(const cJSON* obj, const char* key, std::string& out, const PresenceLimits& lim,
              std::string* error);

bool read_canvas(const cJSON* o, CanvasPresence& c, const PresenceLimits& lim,
                 std::string* error) {
    if (!cJSON_IsObject(o)) return fail(error, "a canvas entry is not an object");
    if (!read_str(o, "surface", c.surface, lim, error)) return false;
    float xy[4];
    bool present = false;
    if (!read_nums(o, "cursor", xy, 2, present, lim, error)) return false;
    if ((c.has_cursor = present)) {
        c.cursor_x = xy[0];
        c.cursor_y = xy[1];
    }
    if (!read_nums(o, "view", xy, 4, present, lim, error)) return false;
    if ((c.has_view = present)) {
        c.view_x0 = xy[0];
        c.view_y0 = xy[1];
        c.view_x1 = xy[2];
        c.view_y1 = xy[3];
    }
    std::string g;
    if (!read_str(o, "gesture", g, lim, error)) return false;
    if (!g.empty() && !parse_gesture(g, c.gesture)) return fail(error, "unknown gesture");
    if (!read_nums(o, "offset", xy, 2, present, lim, error)) return false;
    if (present) {
        c.dx = xy[0];
        c.dy = xy[1];
    }
    if (!read_str(o, "wire_rune", c.wire_rune, lim, error)) return false;
    if (cJSON_GetObjectItemCaseSensitive(o, "wire_port")) {
        std::uint64_t port = 0;
        if (!read_uint(o, "wire_port", 1e6, port, error)) return false;
        c.wire_port = (int)port;
    }
    if (!read_nums(o, "marquee", xy, 4, present, lim, error)) return false;
    if (present) {
        c.mq_x0 = xy[0];
        c.mq_y0 = xy[1];
        c.mq_x1 = xy[2];
        c.mq_y1 = xy[3];
    }
    if (!read_str(o, "field_rune", c.field_rune, lim, error)) return false;
    if (!read_str(o, "field_key", c.field_key, lim, error)) return false;
    PresenceLimits wide = lim;
    wide.max_str = lim.max_preview;
    if (!read_str(o, "preview", c.preview, wide, error)) return false;
    std::uint64_t seq = 0;
    if (!read_uint(o, "ping_seq", 4294967295.0, seq, error)) return false;
    c.ping_seq = (std::uint32_t)seq;
    if (!read_nums(o, "ping", xy, 2, present, lim, error)) return false;
    if (present) {
        c.ping_x = xy[0];
        c.ping_y = xy[1];
    }
    // a gesture must carry what it names
    if (c.gesture == CanvasGesture::Wire && c.wire_rune.empty())
        return fail(error, "a wire gesture names no rune");
    if (c.gesture == CanvasGesture::Typing && (c.field_rune.empty() || c.field_key.empty()))
        return fail(error, "a typing gesture names no field");
    return true;
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
    if (st.kind == Participant::Agent) cJSON_AddStringToObject(root, "kind", "agent");
    if (!st.device.empty()) cJSON_AddStringToObject(root, "device", st.device.c_str());
    if (!st.compat.empty()) cJSON_AddStringToObject(root, "compat", st.compat.c_str());
    if (st.clock) cJSON_AddNumberToObject(root, "clock", (double)st.clock);
    if (!st.canvas.empty()) {
        cJSON* arr = cJSON_AddArrayToObject(root, "canvas");
        for (const auto& c : st.canvas) {
            cJSON* o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "surface", c.surface.c_str());
            if (c.has_cursor) add_nums(o, "cursor", {c.cursor_x, c.cursor_y});
            if (c.has_view) add_nums(o, "view", {c.view_x0, c.view_y0, c.view_x1, c.view_y1});
            if (c.gesture != CanvasGesture::None) {
                cJSON_AddStringToObject(o, "gesture", std::string(gesture_name(c.gesture)).c_str());
                if (c.gesture == CanvasGesture::Move) add_nums(o, "offset", {c.dx, c.dy});
                if (c.gesture == CanvasGesture::Wire) {
                    cJSON_AddStringToObject(o, "wire_rune", c.wire_rune.c_str());
                    if (c.wire_port >= 0) cJSON_AddNumberToObject(o, "wire_port", c.wire_port);
                }
                if (c.gesture == CanvasGesture::Marquee)
                    add_nums(o, "marquee", {c.mq_x0, c.mq_y0, c.mq_x1, c.mq_y1});
                if (!c.field_rune.empty())
                    cJSON_AddStringToObject(o, "field_rune", c.field_rune.c_str());
                if (!c.field_key.empty())
                    cJSON_AddStringToObject(o, "field_key", c.field_key.c_str());
                if (!c.preview.empty()) cJSON_AddStringToObject(o, "preview", c.preview.c_str());
            }
            if (c.ping_seq) {
                cJSON_AddNumberToObject(o, "ping_seq", c.ping_seq);
                add_nums(o, "ping", {c.ping_x, c.ping_y});
            }
            cJSON_AddItemToArray(arr, o);
        }
    }
    if (!st.claims.empty()) {
        cJSON* arr = cJSON_AddArrayToObject(root, "claims");
        for (const auto& cl : st.claims) {
            cJSON* o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "rune", cl.rune.c_str());
            if (!cl.part.empty()) cJSON_AddStringToObject(o, "part", cl.part.c_str());
            cJSON_AddNumberToObject(o, "stamp", (double)cl.stamp);
            cJSON_AddItemToArray(arr, o);
        }
    }
    if (!st.recent.empty()) add_list(root, "recent", st.recent);
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

        std::string kind;
        if (!read_str(root, "kind", kind, lim, error)) break;
        if (kind == "agent") {
            st.kind = Participant::Agent;
        } else if (!kind.empty() && kind != "person") {
            fail(error, "unknown participant kind");
            break;
        }
        if (!read_str(root, "device", st.device, lim, error)) break;
        if (!read_str(root, "compat", st.compat, lim, error)) break;
        // 2^53: the largest integer a JSON number carries exactly
        if (!read_uint(root, "clock", 9007199254740992.0, st.clock, error)) break;

        bool bad = false;
        if (const cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "canvas")) {
            if (!cJSON_IsArray(arr)) { fail(error, "`canvas` is not an array"); break; }
            if ((std::size_t)cJSON_GetArraySize(arr) > lim.max_canvas) {
                fail(error, "`canvas` exceeds max_canvas");
                break;
            }
            const cJSON* it = nullptr;
            cJSON_ArrayForEach(it, arr) {
                CanvasPresence c;
                if (!read_canvas(it, c, lim, error)) { bad = true; break; }
                st.canvas.push_back(std::move(c));
            }
            if (bad) break;
        }
        if (const cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "claims")) {
            if (!cJSON_IsArray(arr)) { fail(error, "`claims` is not an array"); break; }
            if ((std::size_t)cJSON_GetArraySize(arr) > lim.max_claims) {
                fail(error, "`claims` exceeds max_claims");
                break;
            }
            const cJSON* it = nullptr;
            cJSON_ArrayForEach(it, arr) {
                Claim cl;
                if (!cJSON_IsObject(it)) { fail(error, "a claim is not an object"); bad = true; break; }
                if (!read_str(it, "rune", cl.rune, lim, error) ||
                    !read_str(it, "part", cl.part, lim, error) ||
                    !read_uint(it, "stamp", 9007199254740992.0, cl.stamp, error)) {
                    bad = true;
                    break;
                }
                if (cl.rune.empty()) { fail(error, "a claim names no rune"); bad = true; break; }
                st.claims.push_back(std::move(cl));
            }
            if (bad) break;
        }
        PresenceLimits recent_lim = lim;
        recent_lim.max_ids = lim.max_recent;
        recent_lim.max_str = lim.max_preview; // a command line may be longer than a name
        if (!read_list(root, "recent", st.recent, recent_lim, error)) break;
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
