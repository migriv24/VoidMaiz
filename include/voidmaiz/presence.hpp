/*
 * voidmaiz/presence.hpp — networking, the half an application SHOWS (UI-free).
 *
 * okf/concepts/networking.md, ruled by the author 2026-09-18 (Q30): Void Maiz
 * owns what networking LOOKS like and the rules that keep it consistent across
 * every surface; Void Palabra owns what networking IS (transport, keys,
 * signatures, sync, files by hash); the application owns what is shared. This
 * header is the first of those three and contains no socket, no key and no
 * Palabra type. An application that never networks can link it and never
 * notice; one that does carries its bytes over whatever transport it has.
 *
 * WHY THIS IS A UI LIBRARY'S JOB. Networking goes wrong in GUI applications
 * one view at a time: the list marks who is looking, the canvas outlines it,
 * the map shows nothing, because each view decided for itself. None of that is
 * a network bug. It is a view that was never told the rule. The canvas never
 * lets a view decide whether a move is a command; this header does the same for
 * presence. A view DECLARES what it shows (Surfaces), and every consumer —
 * presence, the surface census, the attention graph — reads that one
 * declaration. A new view gets presence by declaring, and cannot forget.
 *
 * FOUR RULES THIS HEADER ENFORCES RATHER THAN RECOMMENDS
 *
 *  1. Presence is keyed on the rune's IMMUTABLE id (SceneNode::id), never its
 *     name. A rename must not make someone vanish from what they are editing.
 *  2. The SENDER decides what is broadcast (SharePolicy, enforced in
 *     compose_presence). A receiver-side "hide avatars" is a clutter setting,
 *     not privacy; privacy has to happen before the bytes leave.
 *  3. A rune that may not leave this device is not mentioned in presence
 *     either. Broadcasting "I have `x` selected" leaks that `x` exists.
 *  4. Presence is EPHEMERAL. Nothing here writes to the model, and remote
 *     presence must never be fed into the local UserGraph — whose attention the
 *     attention graph records is Q20, and its lean (per-peer) is what keeps
 *     `with` meaning "coherent in MY work". Live presence shares attention now;
 *     it records nothing, so there is nothing to un-share later.
 *
 * WHAT THE APPLICATION ANSWERS: one question, "may this rune leave this
 * device?" (ShareFilter). The default is a tag convention; an Allomone rule can
 * answer it (voidmaiz/allomone.hpp, share_by_annotation); an application with
 * its own sharing model — a dataflow graph of backends, a permissions table —
 * compiles that model into the same function. The library never learns why.
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* ── the tag vocabulary networking uses ───────────────────────────────────────
 * Ordinary tags in the one tag grammar, so they filter, colour and appear in
 * the log like any other. Conventions, not types: an application may choose
 * its own and hand share_by_tag a different spelling. */
namespace net_tag {
/* A rune carrying this tag stays on this device: it is not synced, and it is
 * never named in presence. The default ShareFilter reads it. */
inline constexpr std::string_view private_ = "private";
} // namespace net_tag

/* How presence marks a surface. Chosen by the view that declares the surface —
 * a node reads well outlined, a dense list row reads better with a badge, a
 * calendar cell with a tint — and drawn by ONE renderer (voidmaiz/netview.hpp),
 * so the same peer looks the same everywhere. */
enum class Mark { Outline, Badge, Tint, None };

std::string_view mark_name(Mark m);                    // "outline" | "badge" | …
bool parse_mark(std::string_view text, Mark& out);     // the inverse; false if unknown

/* ── who: presentation only ──────────────────────────────────────────────────
 * Name, colour and picture. The KEYPAIR is deliberately absent: a key exists to
 * sign changes, the change format is Void Palabra's, and a key and its
 * signatures cannot sit on opposite sides of a trust boundary. A transport that
 * authenticates peers binds `id` to a key; this struct only draws them. */
struct Profile {
    std::string id;         // stable per device/person; the transport vouches for it
    std::string name;       // display name
    unsigned rgb = 0x4f86d9; // 0xRRGGBB — the peer's colour on every mark
    std::string avatar;     // an asset reference the application resolves; "" = initials
};

/* ── what is on screen: the surface registry ──────────────────────────────────
 * IMMEDIATE MODE, deliberately. Views declare their surfaces every frame
 * between begin_frame() and the next begin_frame(), exactly as ImGui draws
 * them. A registry with a lifecycle (register/unregister) goes stale the moment
 * a window closes on one device and not another; one rebuilt per frame cannot.
 *
 * A surface is an id, a kind, a mark style, the rune ids it shows, and whether
 * the person is working in it. It holds NO geometry: the view that declared it
 * draws its own marks, and a view whose layout is still changing (a builder, a
 * calendar, a map) does not have to change its declarations when it does. */
struct SurfaceDecl {
    std::string id;                 // application-chosen, stable across frames: "canvas",
                                    // "table:people", "map"
    std::string kind;               // host vocabulary: "canvas" | "list" | "map" | …
    Mark mark = Mark::Outline;
    std::vector<std::string> runes; // rune ids shown this frame, in declaration order
    bool focused = false;           // the one the person is working in
};

class Surfaces {
  public:
    /* Start a frame: forget last frame's declarations. Call once per frame
     * BEFORE any view draws. */
    void begin_frame();

    /* Declare a surface (idempotent within a frame: a second call updates
     * kind/mark and keeps the runes already shown). */
    void declare(std::string_view surface_id, std::string_view kind = {},
                 Mark mark = Mark::Outline);

    /* This surface shows this rune (by id). Declares the surface if needed. */
    void show(std::string_view surface_id, std::string_view rune_id);

    /* The surface the person is working in. At most one per frame; the last
     * call wins. */
    void focus(std::string_view surface_id);

    const std::vector<SurfaceDecl>& list() const { return decls_; }
    const SurfaceDecl* find(std::string_view surface_id) const;
    const SurfaceDecl* focused() const;
    std::vector<std::string> surfaces_showing(std::string_view rune_id) const;

  private:
    std::vector<SurfaceDecl> decls_;
    SurfaceDecl& decl_for(std::string_view id);
};

/* ── the one question the application answers ─────────────────────────────────
 * true = this rune may leave this device (sync, presence). */
using ShareFilter = std::function<bool(const SceneNode&)>;

/* The default: shareable unless tagged `tag` (net_tag::private_). */
ShareFilter share_by_tag(std::string tag = std::string(net_tag::private_));

/* Everything in `scene` that may leave — by id, in scene order. What an
 * application hands its sync layer as the export set, so the sync seam and the
 * presence seam cannot disagree about what is private. */
std::vector<std::string> shareable_runes(const Scene& scene, const ShareFilter& filter);

/* Resolve a rune by id (the key presence uses). nullptr if the scene has none. */
const SceneNode* find_by_id(const Scene& scene, std::string_view rune_id);

/* ── presence: what one peer says about itself ────────────────────────────────
 * The WIRE PAYLOAD. Its content is Void Maiz's vocabulary (surfaces, rune ids,
 * marks); carrying it is the transport's job, as opaque bytes, on a channel
 * that never enters history. */
struct PresenceState {
    Profile who;
    std::vector<std::string> selection; // rune ids the peer has selected
    std::vector<std::string> surfaces;  // surface ids visible to the peer ("" if withheld)
    std::string focus;                  // the surface they are working in ("" if withheld)
};

/* The SENDER's switches. Enforced where presence is composed, so no receiver
 * setting can undo a choice made here. */
struct SharePolicy {
    bool selection = true; // broadcast what I have selected
    bool surfaces = true;  // broadcast which surfaces I have open, and my focus
};

/* The RECEIVER's switches: what to draw of what others broadcast. Clutter
 * control, not privacy — see rule 2. */
struct PresenceDisplay {
    bool marks = true;          // outline/badge/tint on the runes peers selected
    bool surface_badges = true; // avatars on tabs/windows peers have open
    bool private_marks = true;  // show which of MY runes stay on this device
};

/* Everything a Networking settings section edits, in one struct the
 * application persists wherever it keeps device preferences (NOT the shared
 * document: these are about this device and this person). */
struct NetSettings {
    Profile self;
    SharePolicy send;
    PresenceDisplay show;
    /* Do not fetch files other members add; show a placeholder instead. The
     * application states the policy, the sync layer enforces it by not
     * fetching, and Void Maiz draws the placeholder. */
    bool cautious_files = false;
};

/* Compose what THIS device broadcasts. Applies the sender switches and drops
 * every selected rune the filter keeps local (rule 3). Selection ids that the
 * scene does not hold are dropped too: presence never names a rune the sender
 * cannot vouch for. */
PresenceState compose_presence(const Profile& self, const std::vector<std::string>& selection_ids,
                               const Surfaces& surfaces, const SharePolicy& policy,
                               const Scene& scene, const ShareFilter& filter);

/* Selection is held by NAME in EditorState (names are what wires and commands
 * use). Presence keys on ids; this is the one conversion, so no host writes it. */
std::vector<std::string> selection_ids(const Scene& scene, const std::vector<std::string>& names);

/* ── the codec: presence as bytes ─────────────────────────────────────────────
 * JSON, because it is readable in a log and every transport can carry text.
 * The PARSER IS DEFENSIVE: presence arrives from another machine, so a peer
 * decides how much it sends and the receiver must not hold whatever it gets.
 * Oversized input, too many ids and overlong strings are refused whole —
 * never truncated, because a truncated selection is a lie about what someone
 * is doing. */
struct PresenceLimits {
    std::size_t max_bytes = 16 * 1024;
    std::size_t max_ids = 512;  // per list
    std::size_t max_str = 256;  // per string
};

std::string presence_to_json(const PresenceState& state);
bool presence_from_json(std::string_view json, PresenceState& out, std::string* error = nullptr,
                        const PresenceLimits& limits = {});

/* ── what a merge could not decide ───────────────────────────────────────────
 * A sync layer answers "what is the value" with a lattice, and some questions a
 * lattice cannot answer: two devices wrote different values at the same time, or
 * one deleted what another was editing. Those are CONFLICTS, and they are not
 * errors — they are a question addressed to a person.
 *
 * These two structs are the plain-data projection of them: strings only, no
 * types from any sync library, so the VIEW that draws them links no such
 * library. `voidmaiz_net` fills them from Void Palabra; another sync layer
 * could fill them from something else; a host with neither can draw an empty
 * list. `id` is an opaque handle (Palabra's content address for the conflict —
 * two peers compute the same one) handed back to resolve it.
 *
 * An ANOMALY is the other half: a rule every device kept that the merge broke —
 * two runes with one name, a link whose endpoint a concurrent removal took away.
 * It asks for an edit rather than a choice, so it has no sides. */
struct ConflictRow {
    std::string id;
    std::string kind;   // "values" | "deleted" (a delete that raced an edit)
    std::string mantle;
    std::string rune;      // spirit.id; empty for a mantle-level field
    std::string rune_name; // the human handle, when the document still holds it
    std::string glyph;     // set only for two schemas given to one glyph name
    std::string field;     // "content.text", "domain", "present", "descriptor"
    std::vector<std::string> sides; // the values to choose between, in order
};

struct AnomalyRow {
    std::string kind;    // "duplicate_name" | "link_broken" | "type_removed"
    std::string mantle;
    std::string subject; // the name, the link endpoint, or the glyph
    std::string cause;   // link_broken: "removed" | "renamed"
    std::vector<std::string> runes;
};

/* ── the roster: everyone else who is here ────────────────────────────────────
 * The receiving side. Upserts by profile id, ignores this device's own echo
 * (LAN broadcasts come back), and forgets peers the application stops hearing
 * from. It takes the application's clock rather than keeping one, like the
 * attention graph: time is the host's policy. */
struct Peer {
    PresenceState state;
    double seen = 0.0; // application clock at the last update
};

class Roster {
  public:
    /* Updates carrying this id are ignored — a peer is never its own peer. */
    void set_self(std::string_view self_id) { self_ = std::string(self_id); }

    /* Returns false (and changes nothing) for this device's own echo or a
     * state with no profile id. */
    bool update(const PresenceState& state, double now);
    void leave(std::string_view peer_id);
    /* Forget peers not heard from within `ttl` seconds. Returns how many left. */
    int prune(double now, double ttl);
    void clear() { peers_.clear(); }

    const std::vector<Peer>& peers() const { return peers_; }
    const Peer* find(std::string_view peer_id) const;

    /* Peers whose selection holds this rune id. */
    std::vector<const Peer*> on_rune(std::string_view rune_id) const;
    /* Peers with this surface visible (or focused). */
    std::vector<const Peer*> on_surface(std::string_view surface_id) const;

  private:
    std::string self_;
    std::vector<Peer> peers_;
};

} // namespace maiz
