/* net.cpp — a Void Maiz application on Void Palabra's sync (voidmaiz/net.hpp). */
#include "voidmaiz/net.hpp"

#include "voidpalabra/canonical.hpp"

#include "cJSON.h"

#include <algorithm>
#include <utility>

namespace maiz {

namespace vps = voidpalabra::sync;

namespace {

struct Json {
    cJSON* p = nullptr;
    explicit Json(cJSON* q) : p(q) {}
    ~Json() { cJSON_Delete(p); }
    Json(const Json&) = delete;
    Json& operator=(const Json&) = delete;
};

std::string print(const cJSON* j) {
    char* t = cJSON_PrintUnformatted(j);
    std::string s = t ? t : "";
    cJSON_free(t);
    return s;
}

int rank(vps::ContentState s) {
    switch (s) {
    case vps::ContentState::held: return 5;
    case vps::ContentState::requested: return 4;
    case vps::ContentState::wanted: return 3;
    case vps::ContentState::deferred: return 2;
    case vps::ContentState::unavailable: return 1;
    case vps::ContentState::unknown: return 0;
    }
    return 0;
}

} // namespace

voidpalabra::JoinPolicy presentational_joins() {
    voidpalabra::JoinPolicy p = voidpalabra::JoinPolicy::core_defaults();
    // Latest (Palabra SPEC 5.10): the Lamport-latest write, the same on every
    // peer, with no wall clock. The author's "last one wins", honestly. View
    // state only: a peer minting huge counters would win every Latest field.
    using FJ = voidpalabra::FieldJoin;
    p.fields["placement"] = FJ::Latest;
    p.fields["content.pos"] = FJ::Latest;
    p.fields["content.size"] = FJ::Latest;
    p.fields["content.collapsed"] = FJ::Latest;
    p.fields["content.route.*"] = FJ::Latest;
    return p;
}

Network::Network(Core& core, voidpalabra::Replica replica, NetOptions options)
    : core_(core), replica_(std::move(replica)), opt_(std::move(options)) {
    replica_.set_policy(opt_.joins);
    if (settings_.self.id.empty()) settings_.self.id = replica_.id();
    roster_.set_self(replica_.id());
    observe_local(true); // the document as it stands IS this device's starting state
}

void Network::note(const char* level, const std::string& link, std::string text) {
    notes_.push_back({level, link, std::move(text)});
}

// ── the one question, as Palabra asks it ─────────────────────────────────────

void Network::rebuild_index(const std::string& state_json) {
    index_.clear();
    mantles_.clear();
    Json root(cJSON_Parse(state_json.c_str()));
    const cJSON* mantles = cJSON_GetObjectItemCaseSensitive(root.p, "mantles");
    const cJSON* m = nullptr;
    cJSON_ArrayForEach(m, mantles) {
        if (const cJSON* nm = cJSON_GetObjectItemCaseSensitive(m, "name"); cJSON_IsString(nm))
            mantles_.insert(nm->valuestring);
        const cJSON* runes = cJSON_GetObjectItemCaseSensitive(m, "runes");
        const cJSON* r = nullptr;
        cJSON_ArrayForEach(r, runes) {
            const cJSON* spirit = cJSON_GetObjectItemCaseSensitive(r, "spirit");
            const cJSON* id = cJSON_GetObjectItemCaseSensitive(spirit, "id");
            if (!cJSON_IsString(id)) continue;
            SceneNode n;
            n.id = id->valuestring;
            if (const cJSON* nm = cJSON_GetObjectItemCaseSensitive(spirit, "name");
                cJSON_IsString(nm))
                n.name = nm->valuestring;
            if (const cJSON* g = cJSON_GetObjectItemCaseSensitive(r, "glyph"); cJSON_IsString(g))
                n.glyph = g->valuestring;
            const cJSON* t = nullptr;
            cJSON_ArrayForEach(t, cJSON_GetObjectItemCaseSensitive(r, "tags")) {
                if (cJSON_IsString(t)) n.tags.emplace_back(t->valuestring);
            }
            index_[n.id] = std::move(n);
        }
    }
}

vps::Host Network::make_host() {
    vps::Host h;
    h.share = [this](const std::string& mantle, const std::string& rune_id) {
        if (rune_id.empty()) return !opt_.share_mantle || opt_.share_mantle(mantle);
        auto it = index_.find(rune_id);
        /* A rune the Core does not hold (removed here, say) is not vouched for.
         * Its REMOVAL still travels — Palabra sends removals regardless — and
         * nothing else about it should. */
        if (it == index_.end()) return false;
        return !opt_.share || opt_.share(it->second);
    };
    h.references = opt_.files.references;
    h.have = opt_.files.have;
    h.read = opt_.files.read;
    h.fetch = settings_.cautious_files ? vps::FetchPolicy::cautious : vps::FetchPolicy::automatic;
    h.sign = opt_.sign;
    h.verify = opt_.verify;
    return h;
}

// ── the replica loop ─────────────────────────────────────────────────────────

void Network::observe_local(bool force) {
    std::string state = core_.export_state();
    if (!force && state == last_state_) return;
    Json root(cJSON_Parse(state.c_str()));
    if (!root.p) {
        note("error", "", "the Core exported a state that does not parse; nothing observed");
        return;
    }
    voidpalabra::Replica::Observed o = replica_.observe(root.p);
    if (!o.ok) {
        note("error", "", "observe refused the local state: " + o.error);
        return;
    }
    last_state_ = std::move(state);
    rebuild_index(last_state_);
    if (o.changes > 0) {
        stats_.observed_changes += o.changes;
        // PERSIST before anything carrying these tags can leave (Palabra's step 2)
        if (opt_.persist) opt_.persist(replica_.to_bytes());
    }
}

void Network::splice_merged() {
    // an edit made since the last tick is RECORDED first, never overwritten
    observe_local(false);

    voidpalabra::Doc flat = replica_.flatten();
    Json state(cJSON_Parse(core_.export_state().c_str()));
    if (!flat.root || !state.p) return;

    bool differs = false;
    for (const char* key : {"mantles", "glyphs"}) {
        const cJSON* want = cJSON_GetObjectItemCaseSensitive(flat.root, key);
        const cJSON* have = cJSON_GetObjectItemCaseSensitive(state.p, key);
        if (!want) continue;
        if (!have || !cJSON_Compare(want, have, 1)) differs = true;
    }
    /* Only a merge that CHANGED the document costs a splice — and a splice costs
     * the undo history. An idle peer re-sending what we already show must not. */
    if (!differs) return;

    for (const char* key : {"mantles", "glyphs"}) {
        const cJSON* want = cJSON_GetObjectItemCaseSensitive(flat.root, key);
        if (!want) continue;
        cJSON* copy = cJSON_Duplicate(want, 1);
        if (cJSON_GetObjectItemCaseSensitive(state.p, key))
            cJSON_ReplaceItemInObjectCaseSensitive(state.p, key, copy);
        else
            cJSON_AddItemToObject(state.p, key, copy);
    }
    if (!core_.replace_state(print(state.p))) {
        note("error", "", "a merge could not be spliced into the document");
        return;
    }
    spliced_ = true;
    ++stats_.splices;

    /* The splice must be a fixed point: observing what we just showed records
     * nothing. If it records something, the Core's re-export and Palabra's
     * flatten disagree about representation, and every tick would mint phantom
     * changes and trade them with every peer forever. Surface it loudly. */
    std::size_t before = stats_.observed_changes;
    observe_local(true);
    if (stats_.observed_changes != before)
        note("warn", "",
             "a splice did not round-trip: observing it recorded " +
                 std::to_string(stats_.observed_changes - before) + " change(s)");
}

void Network::handle(const std::string& link, vps::Step step, NetMillis now) {
    bool merged = false;
    for (auto& e : step.events) {
        using T = vps::Event::Type;
        switch (e.type) {
        case T::peer_identified:
            note("info", link, "peer " + e.peer + " said hello");
            break;
        case T::merged:
            ++stats_.merges;
            merged = true;
            splice_merged();
            if (e.conflicts || e.anomalies)
                note("warn", link,
                     "merged from " + e.peer + ": " + std::to_string(e.conflicts) +
                         " conflict(s), " + std::to_string(e.anomalies) + " anomaly(ies)");
            else
                note("info", link, "merged from " + e.peer);
            break;
        case T::merge_refused:
            note("error", link, "refused a state from " + e.peer + ": " + e.detail);
            break;
        case T::peer_refused_state:
            note("error", link, e.peer + " refused our state: " + e.detail);
            break;
        case T::content_arrived:
            if (opt_.files.store) opt_.files.store(e.address, e.bytes);
            note("info", link, "file arrived: " + e.address);
            break;
        case T::content_refused:
            note("warn", link, "file refused (bytes did not match): " + e.address);
            break;
        case T::content_unavailable:
            note("warn", link, "file unavailable from " + e.peer + ": " + e.address);
            break;
        case T::presence: {
            PresenceState st;
            std::string why;
            PresenceLimits lim;
            lim.max_bytes = opt_.limits.max_presence;
            if (!presence_from_json(e.bytes, st, &why, lim)) {
                ++stats_.presence_refused;
                note("warn", link, "presence from " + e.peer + " refused: " + why);
                break;
            }
            /* Keyed on the identity the SESSION established, not the one the
             * payload claims: a peer cannot speak as another by naming it. */
            st.who.id = e.peer;
            roster_.update(st, (double)now / 1000.0);
            ++stats_.presence_in;
            break;
        }
        case T::presence_expired:
            roster_.leave(e.peer);
            break;
        case T::message_refused:
            note("warn", link, "a frame was refused: " + e.detail);
            break;
        case T::closed:
            roster_.leave(e.peer);
            note("info", link, "session closed: " + e.detail);
            break;
        }
    }
    if (merged && opt_.persist) opt_.persist(replica_.to_bytes());
    for (auto& f : step.send) out_.push_back({link, std::move(f)});
}

// ── links ────────────────────────────────────────────────────────────────────

void Network::connect(const std::string& link, NetMillis now) {
    if (links_.count(link)) return;
    observe_local(false); // never open a session on a stale export
    Link& l = links_[link];
    l.session = std::make_unique<vps::Session>(replica_, make_host(), opt_.timing, opt_.limits);
    handle(link, l.session->start(now), now);
}

void Network::receive(const std::string& link, const std::string& frame, NetMillis now) {
    if (!links_.count(link)) connect(link, now); // an inbound peer
    handle(link, links_[link].session->receive(frame, now), now);
}

void Network::disconnect(const std::string& link, NetMillis now) {
    auto it = links_.find(link);
    if (it == links_.end()) return;
    std::string peer = it->second.session->peer();
    handle(link, it->second.session->close("disconnected", now), now);
    if (!peer.empty()) roster_.leave(peer);
    links_.erase(it);
}

void Network::tick(NetMillis now, const std::vector<std::string>& selection_ids,
                   const Surfaces& surfaces) {
    tick(now, selection_ids, surfaces, CollabOut{});
}

void Network::tick(NetMillis now, const std::vector<std::string>& selection_ids,
                   const Surfaces& surfaces, const CollabOut& collab) {
    observe_local(false);

    // presence: composed with the SENDER's switches, from this device's own view.
    // Everything in flight is vouched for against the whole index, not only the
    // selection: a wire gesture or a claim may name a rune nobody has selected.
    Scene known;
    known.nodes.reserve(index_.size());
    for (const auto& [id, n] : index_) known.nodes.push_back(n);
    PresenceState mine = compose_presence(settings_.self, selection_ids, surfaces, settings_.send,
                                          known, opt_.share, collab);
    // mantle-wide claims (IC's crank): the mantle must exist here and may leave
    for (const Claim& cl : collab.claims)
        if (mantles_.count(cl.rune) && !index_.count(cl.rune) &&
            (!opt_.share_mantle || opt_.share_mantle(cl.rune)))
            mine.claims.push_back(cl);
    std::string payload = presence_to_json(mine);
    bool stale = presence_sent_at_ < 0 || now - presence_sent_at_ >= opt_.timing.presence_ttl / 3;
    if (payload != last_presence_ || stale) {
        for (auto& [link, l] : links_)
            if (!l.session->is_closed()) handle(link, l.session->publish_presence(payload, now), now);
        last_presence_ = payload;
        presence_sent_at_ = now;
    }

    for (auto& [link, l] : links_) handle(link, l.session->tick(now), now);

    for (auto it = links_.begin(); it != links_.end();) {
        if (it->second.session->is_closed()) {
            if (!it->second.session->peer().empty()) roster_.leave(it->second.session->peer());
            it = links_.erase(it);
        } else {
            ++it;
        }
    }
}

void Network::resync(NetMillis now) {
    observe_local(true); // whatever the Core holds now IS what we have
    std::vector<std::string> names;
    for (const auto& [link, l] : links_) names.push_back(link);
    links_.clear(); // drop the sessions; their peers see a restart
    for (const auto& link : names) connect(link, now);
    note("info", "", "resynchronised: " + std::to_string(names.size()) +
                         " link(s) started a new session and the whole document again");
}

void Network::fetch(const std::string& address, NetMillis now) {
    for (auto& [link, l] : links_) handle(link, l.session->fetch(address, now), now);
}

// ── out ──────────────────────────────────────────────────────────────────────

std::vector<Outgoing> Network::take_outgoing() { return std::exchange(out_, {}); }
std::vector<NetNote> Network::take_notes() { return std::exchange(notes_, {}); }
bool Network::take_spliced() { return std::exchange(spliced_, false); }

std::vector<LinkStatus> Network::links() const {
    std::vector<LinkStatus> out;
    for (const auto& [link, l] : links_)
        out.push_back({link, l.session->peer(), l.session->is_open(), l.session->is_closed(),
                       l.session->in_sync()});
    return out;
}

std::vector<ConflictRow> Network::conflicts() const {
    std::vector<ConflictRow> out;
    for (const auto& c : replica_.conflicts()) {
        ConflictRow r;
        r.id = voidpalabra::to_hex(c.hash());
        r.kind = c.kind == voidpalabra::ConflictKind::deleted_while_edited ? "deleted" : "values";
        r.mantle = c.mantle;
        r.rune = c.rune;
        if (auto it = index_.find(c.rune); it != index_.end()) r.rune_name = it->second.name;
        r.glyph = c.glyph;
        r.field = c.field;
        r.sides = c.sides;
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<AnomalyRow> Network::anomalies() const {
    std::vector<AnomalyRow> out;
    for (const auto& a : replica_.anomalies()) {
        AnomalyRow r;
        switch (a.kind) {
        case voidpalabra::AnomalyKind::duplicate_name: r.kind = "duplicate_name"; break;
        case voidpalabra::AnomalyKind::link_broken: r.kind = "link_broken"; break;
        case voidpalabra::AnomalyKind::type_removed: r.kind = "type_removed"; break;
        }
        r.mantle = a.mantle;
        r.subject = a.subject;
        r.cause = a.cause;
        r.runes = a.runes;
        out.push_back(std::move(r));
    }
    return out;
}

bool Network::resolve(const std::string& conflict_id, std::size_t side, NetMillis now) {
    (void)now;
    for (const auto& c : replica_.conflicts()) {
        if (voidpalabra::to_hex(c.hash()) != conflict_id) continue;
        if (side >= c.sides.size()) return false;
        if (!replica_.resolve(c, side)) return false; // stale: read the rows again
        /* A resolution is a change to the document like any other: show it here,
         * and let the next tick carry it to every peer. */
        voidpalabra::Doc flat = replica_.flatten();
        Json state(cJSON_Parse(core_.export_state().c_str()));
        if (flat.root && state.p) {
            for (const char* key : {"mantles", "glyphs"}) {
                const cJSON* want = cJSON_GetObjectItemCaseSensitive(flat.root, key);
                if (!want) continue;
                cJSON* copy = cJSON_Duplicate(want, 1);
                if (cJSON_GetObjectItemCaseSensitive(state.p, key))
                    cJSON_ReplaceItemInObjectCaseSensitive(state.p, key, copy);
                else
                    cJSON_AddItemToObject(state.p, key, copy);
            }
            if (core_.replace_state(print(state.p))) {
                spliced_ = true;
                ++stats_.splices;
                observe_local(true);
            }
        }
        if (opt_.persist) opt_.persist(replica_.to_bytes());
        note("info", "", "a conflict was resolved on this device");
        return true;
    }
    return false;
}

vps::ContentState Network::content_state(const std::string& address) const {
    vps::ContentState best = vps::ContentState::unknown;
    if (opt_.files.have && opt_.files.have(address)) return vps::ContentState::held;
    for (const auto& [link, l] : links_) {
        vps::ContentState s = l.session->content_state(address);
        if (rank(s) > rank(best)) best = s;
    }
    return best;
}

} // namespace maiz
