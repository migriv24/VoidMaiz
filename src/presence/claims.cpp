/* claims.cpp — first come first served (voidmaiz/claims.hpp). */
#include "voidmaiz/claims.hpp"

#include <algorithm>

namespace maiz {

std::string claim_port(int port) { return "port:" + std::to_string(port); }
std::string claim_field(std::string_view key) { return "field:" + std::string(key); }

bool claims_overlap(const Claim& a, const Claim& b) {
    if (a.rune != b.rune) return false;
    return a.part.empty() || b.part.empty() || a.part == b.part;
}

bool claim_wins(Participant a_kind, const Claim& a, std::string_view a_who, Participant b_kind,
                const Claim& b, std::string_view b_who) {
    if (a_kind != b_kind) return a_kind == Participant::Person; // a person outranks an agent
    if (a.stamp != b.stamp) return a.stamp < b.stamp;          // then: who claimed first
    return a_who < b_who;                                       // then: the same answer everywhere
}

void Claims::observe(const PresenceState& peer) {
    clock_ = std::max(clock_, peer.clock);
    for (const auto& c : peer.claims) clock_ = std::max(clock_, c.stamp);
}

std::optional<ClaimHolder> Claims::holder(const Roster& roster, std::string_view rune,
                                          std::string_view part) const {
    Claim probe{std::string(rune), std::string(part), 0};
    std::optional<ClaimHolder> best;
    auto consider = [&](ClaimHolder h) {
        if (!best || claim_wins(h.kind, h.claim, h.who, best->kind, best->claim, best->who))
            best = std::move(h);
    };
    for (const auto& c : mine_)
        if (claims_overlap(c, probe)) consider({self_, {}, kind_, c, true});
    for (const auto& p : roster.peers())
        for (const auto& c : p.state.claims)
            if (claims_overlap(c, probe))
                consider({p.state.who.id, p.state.who.name, p.state.kind, c, false});
    return best;
}

bool Claims::may_act(const Roster& roster, std::string_view rune, std::string_view part) const {
    auto h = holder(roster, rune, part);
    return !h || h->self;
}

std::string Claims::gate(const Roster& roster, std::string_view rune, std::string_view part) const {
    auto h = holder(roster, rune, part);
    if (!h || h->self) return {};
    std::string who = h->name.empty() ? h->who : h->name;
    std::string what = h->claim.part.empty() ? "it" : h->claim.part;
    return who + (h->kind == Participant::Agent ? " (agent)" : "") + " holds " + what +
           " on " + std::string(rune) + "; claim it first or wait";
}

ClaimHolder Claims::acquire(const Roster& roster, std::string_view rune, std::string_view part) {
    // re-selecting something already held keeps the original stamp
    for (const auto& c : mine_)
        if (c.rune == rune && c.part == part) {
            if (auto h = holder(roster, rune, part)) return *h;
        }
    Claim fresh{std::string(rune), std::string(part), ++clock_};
    // would it win against everything visible that overlaps it?
    ClaimHolder me{self_, {}, kind_, fresh, true};
    for (const auto& p : roster.peers())
        for (const auto& c : p.state.claims)
            if (claims_overlap(c, fresh) &&
                claim_wins(p.state.kind, c, p.state.who.id, kind_, fresh, self_))
                return {p.state.who.id, p.state.who.name, p.state.kind, c, false};
    mine_.push_back(fresh);
    return me;
}

void Claims::release(std::string_view rune, std::string_view part) {
    mine_.erase(std::remove_if(mine_.begin(), mine_.end(),
                               [&](const Claim& c) { return c.rune == rune && c.part == part; }),
                mine_.end());
}

std::vector<ClaimHolder> Claims::yield(const Roster& roster) {
    std::vector<ClaimHolder> lost;
    for (auto it = mine_.begin(); it != mine_.end();) {
        std::optional<ClaimHolder> winner;
        for (const auto& p : roster.peers())
            for (const auto& c : p.state.claims)
                if (claims_overlap(c, *it) &&
                    claim_wins(p.state.kind, c, p.state.who.id, kind_, *it, self_))
                    winner = ClaimHolder{p.state.who.id, p.state.who.name, p.state.kind, c, false};
        if (winner) {
            lost.push_back(*winner);
            it = mine_.erase(it);
        } else {
            ++it;
        }
    }
    return lost;
}

} // namespace maiz
