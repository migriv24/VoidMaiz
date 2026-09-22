/*
 * voidmaiz/net.hpp — stage C: a Void Maiz application on Void Palabra's sync.
 *
 * okf/concepts/networking.md. The line (Q30): Void Maiz owns what networking
 * LOOKS like, Void Palabra owns what networking IS, the application owns what is
 * SHARED. voidmaiz/presence.hpp and netview.hpp are the first; Palabra's
 * `sync.hpp` is the second; this header is the seam between them, and it is the
 * code every networked Void Maiz application would otherwise write for itself —
 * and, on the evidence of the one that did, write subtly wrong.
 *
 * OPTIONAL, by construction. It is its own target, `voidmaiz_net`, the only one
 * that links Void Palabra, and CMake builds it only when `../VoidPalabra` is
 * present. An application that never networks links none of this and never
 * needs Palabra checked out.
 *
 * SANS-IO, like the Palabra session under it. A `Network` opens no socket and
 * reads no clock. The application moves frames between LINKS (its word for "a
 * connection to one peer" — a LAN socket, a BLE characteristic, a test queue)
 * and passes the time in; this hands back frames to send and says what changed.
 * That is why it can be built and tested today, while real transport is still
 * gated on the trust model (Palabra's open-questions §6.1, and the family rule:
 * a missing security railguard blocks the code that would need it).
 *
 * WHAT IT DOES, which is what an application would otherwise get wrong:
 *
 *   1. ONE replica, one Palabra session per link, all driven from one thread.
 *   2. The replica loop in the order Palabra requires — observe, PERSIST, share,
 *      merge, splice, persist — with the persist step a callback the application
 *      cannot skip (a delta sent before the replica is saved re-mints tags after
 *      a crash).
 *   3. The splice: `mantles` and `glyphs` into the Core's document, never the
 *      whole document, and ONLY when the merged slice actually differs — so an
 *      idle peer never costs the user their undo history (see
 *      Core::replace_state for why a splice must clear it). The local state is
 *      observed again immediately before splicing, so an edit made since the
 *      last tick is recorded rather than overwritten.
 *   4. One answer to "may this leave?": the application's ShareFilter is the
 *      ExportSet Palabra asks, so the sync seam and the presence seam cannot
 *      disagree about what is private.
 *   5. Presence: composed with the SENDER's switches, carried as Palabra's
 *      opaque ephemeral message, parsed by our bounded codec on arrival, and
 *      held in a Roster keyed on the PEER'S REPLICA ID — the identity the session
 *      established — never on whatever id the payload claims. A peer cannot put
 *      words in another peer's mouth by naming it.
 *   6. Everything that happened, as notes for the log strip (commitment 2: a
 *      merge that changed the document is not a gesture, but it must not be
 *      invisible either).
 *
 * NOT THREAD-SAFE: one Network, one thread — the replica under it is not.
 */
#pragma once

#include "voidmaiz/embed.hpp"
#include "voidmaiz/presence.hpp"

#include "voidpalabra/replica.hpp"
#include "voidpalabra/sync.hpp"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace maiz {

using NetMillis = voidpalabra::sync::Millis;

/* A file named by content (an image in a rune, say). All optional: leave
 * `references` empty and no file ever syncs. */
struct NetFiles {
    voidpalabra::ReferencePolicy references; // which content fields name files
    std::function<bool(const std::string& address)> have;
    std::function<bool(const std::string& address, std::string& bytes)> read;
    /* A verified file arrived (Palabra has already checked the bytes against the
     * address). Store it; the placeholder redraws from content_state(). */
    std::function<void(const std::string& address, const std::string& bytes)> store;
};

/* How each field resolves when two devices write it at once — Palabra's
 * JoinPolicy, which is a READ-TIME projection: both values stay in the document,
 * every peer shows the same one, and convergence never depends on it.
 *
 * The canvas's view state gets `Latest` (the Lamport-latest write; Palabra
 * SPEC 5.10, built 2026-09-20 at our request): `placement`, and the content keys the
 * gesture compiler writes for a node's position, size, collapse and hand-shaped
 * route (`content.pos`, `content.size`, `content.collapsed`, `content.route.*`).
 * Two people dragging one node then CONVERGE instead of raising a question
 * nobody can meaningfully answer ("which coordinates?") — the author's call,
 * 2026-09-20: hard conflicts slow people down. Everything else keeps Palabra's
 * default, Conflict: a label or a value somebody typed is never silently lost.
 * collaborative-canvas.md §6. */
voidpalabra::JoinPolicy presentational_joins();

struct NetOptions {
    /* REQUIRED. Called with the replica's bytes whenever they must be saved —
     * after every local observation that recorded something, BEFORE any frame
     * carrying it is released, and after every merge. Store atomically. */
    std::function<void(const std::string& replica_bytes)> persist;

    /* The one question. Default: shareable unless tagged `private`. */
    ShareFilter share = share_by_tag();
    /* Whole mantles: may this one leave at all? Default: yes. */
    std::function<bool(const std::string& mantle)> share_mantle;

    NetFiles files;
    /* Per-field resolution. Default: view state picks, everything else conflicts. */
    voidpalabra::JoinPolicy joins = presentational_joins();
    voidpalabra::sync::Timing timing;
    voidpalabra::sync::Limits limits;

    /* The trust seams, passed straight to Palabra. Empty until a scheme is
     * chosen; a real transport should not ship until they are not. */
    std::function<std::string(const std::string&)> sign;
    std::function<bool(const std::string&, const std::string&)> verify;
};

/* A frame the application must deliver to the peer at the other end of `link`. */
struct Outgoing {
    std::string link;
    std::string frame;
};

/* Something that happened, for the log strip. `level` uses LogEntry's
 * vocabulary: "info" | "warn" | "error". */
struct NetNote {
    std::string level;
    std::string link;
    std::string text;
};

struct LinkStatus {
    std::string link;
    std::string peer; // the peer's replica id, once it has said hello
    bool open = false;
    bool closed = false;
    bool in_sync = false; // the peer acknowledged our current shareable state
};

/* Counters a test (or a status bar) can read. `splices` is the one that shows
 * the phantom-change loop if it ever appears: after convergence it must stop. */
struct NetStats {
    std::size_t observed_changes = 0; // registers/sets recorded from local edits
    std::size_t merges = 0;
    std::size_t splices = 0;
    std::size_t presence_in = 0;
    std::size_t presence_refused = 0; // payloads our codec would not accept
};

class Network {
  public:
    /* `core` must outlive the Network. `replica` is created once per device
     * (voidpalabra::Replica::create, with a RANDOM id of 16+ characters — this
     * library has no CSPRNG either) or restored from the bytes `persist` saved. */
    Network(Core& core, voidpalabra::Replica replica, NetOptions options);

    /* Pinned in place: every session holds the replica by reference and asks
     * the export set through `this`. */
    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;
    Network(Network&&) = delete;
    Network& operator=(Network&&) = delete;

    /* `cautious_files` is read when a link CONNECTS; changing it affects links
     * opened afterwards (reconnect to apply it to one already open). */
    NetSettings& settings() { return settings_; }
    const NetSettings& settings() const { return settings_; }

    // ── links ─────────────────────────────────────────────────────────────────
    void connect(const std::string& link, NetMillis now);
    void receive(const std::string& link, const std::string& frame, NetMillis now);
    void disconnect(const std::string& link, NetMillis now);

    /* Once per frame (or on a timer). Records local edits, publishes presence
     * when it changed (and often enough that it never expires on a peer), and
     * runs every session's timers. `selection_ids` are rune ids
     * (selection_ids(scene, ed.selection)); `surfaces` is this frame's registry. */
    void tick(NetMillis now, const std::vector<std::string>& selection_ids,
              const Surfaces& surfaces);
    /* The same, plus the collaborative canvas's in-flight half: cursors, gesture
     * ghosts, claims, recent commands (okf/concepts/collaborative-canvas.md). It
     * is filtered like the selection — nothing in it can name a rune the share
     * filter keeps local. Feed every peer's PresenceState from roster() into your
     * Claims::observe so first-to-select stays honest. */
    void tick(NetMillis now, const std::vector<std::string>& selection_ids,
              const Surfaces& surfaces, const CollabOut& collab);

    /* Start every link's session again, from this device's whole state. The
     * recovery a person reaches for when two screens disagree and nobody knows
     * why: a new session nonce makes each peer forget what it believed was
     * acknowledged, so the whole document is exchanged again. Cheap (one
     * document), and it cannot lose anything: the merge is the same merge. */
    void resync(NetMillis now);

    /* Cautious file transfer: the user chose to download this one. */
    void fetch(const std::string& address, NetMillis now);

    // ── what came out ─────────────────────────────────────────────────────────
    std::vector<Outgoing> take_outgoing();
    std::vector<NetNote> take_notes();
    /* True once after the Core's document was replaced by a merge: re-project. */
    bool take_spliced();

    const Roster& roster() const { return roster_; }
    const voidpalabra::Replica& replica() const { return replica_; }
    std::vector<LinkStatus> links() const;
    const NetStats& stats() const { return stats_; }

    /* Across every link: held beats requested beats wanted beats deferred beats
     * unavailable. What the placeholder draws from. */
    voidpalabra::sync::ContentState content_state(const std::string& address) const;

    /* ── what a merge could not decide ────────────────────────────────────────
     * The questions to put to a person, as plain rows (voidmaiz/presence.hpp),
     * so the view that draws them links no sync library. Read after any note
     * that reported conflicts; they persist in the document until resolved. */
    std::vector<ConflictRow> conflicts() const;
    std::vector<AnomalyRow> anomalies() const;

    /* Settle one conflict by choosing one of its `sides` — recorded as THIS
     * device's act, so it is an ordinary change that syncs like any other, and
     * the other device sees the question answered rather than re-asked.
     *
     * Returns false, changing nothing, if that conflict no longer exists as
     * given: a merge since the rows were read may have changed it, and
     * resolving a stale conflict would overwrite a value nobody saw. Read
     * `conflicts()` again. */
    bool resolve(const std::string& conflict_id, std::size_t side, NetMillis now);

  private:
    struct Link {
        std::unique_ptr<voidpalabra::sync::Session> session;
    };

    void observe_local(bool force);
    void rebuild_index(const std::string& state_json);
    void handle(const std::string& link, voidpalabra::sync::Step step, NetMillis now);
    void splice_merged();
    void note(const char* level, const std::string& link, std::string text);
    voidpalabra::sync::Host make_host();

    Core& core_;
    voidpalabra::Replica replica_;
    NetOptions opt_;
    NetSettings settings_;
    Roster roster_;

    std::map<std::string, Link> links_;
    std::map<std::string, SceneNode> index_; // rune id → what the ShareFilter reads
    std::set<std::string> mantles_;          // mantle names the Core holds
    std::string last_state_;                 // the export last observed
    std::string last_presence_;
    NetMillis presence_sent_at_ = -1;
    bool spliced_ = false;

    std::vector<Outgoing> out_;
    std::vector<NetNote> notes_;
    NetStats stats_;
};

} // namespace maiz
