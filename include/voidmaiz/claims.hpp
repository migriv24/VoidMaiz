/*
 * voidmaiz/claims.hpp — who is doing what, first come first served (UI-free).
 *
 * okf/concepts/collaborative-canvas.md §3.3. The author's rule (2026-09-20):
 * "whoever selected this specific port on the node first, is the one who is
 * doing stuff with it … it's not JUST assigning ports and stuff, but now the
 * selection step also matters." A person drags a wire; an agent simply assigns
 * one. Both must CLAIM before they act, so a human mid-drag and an agent issuing
 * `link` meet the same rule, and the loser learns before committing rather than
 * after a merge.
 *
 * WHAT "FIRST" MEANS WITHOUT A SHARED CLOCK. Each participant keeps a Lamport
 * clock: it ticks on every local claim and jumps past any stamp seen in a peer's
 * presence. So a claim made AFTER seeing another claim always carries a larger
 * stamp and loses — the one ordering a person can actually perceive ("they had
 * it; I grabbed it anyway") is honoured exactly. Two claims made concurrently
 * (neither saw the other, one LAN round-trip) are ordered by a rule every device
 * computes identically. The order, most important first:
 *
 *   1. a person outranks an agent (okf/concepts/headless.md precedence);
 *   2. the smaller Lamport stamp;
 *   3. the smaller participant id — arbitrary, but the same everywhere.
 *
 * Claims are ADVISORY. They ride in presence, never in the document; nothing in
 * the merge enforces them. They make conflicts rare, and whatever races past
 * them lands in the merge's ordinary last-writer rule (collaborative-canvas §6).
 *
 * OVERLAP. Two claims contend when they name the same rune and either is
 * whole-rune ("") or both name the same part. A node claim therefore covers its
 * ports; two different ports of one node do not contend.
 */
#pragma once

#include "voidmaiz/presence.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* The canonical part spellings, so hosts and agents spell them alike. */
std::string claim_port(int port);                  // "port:<n>"
std::string claim_field(std::string_view key);     // "field:<key>"
inline constexpr std::string_view claim_crank = "crank"; // IC: "I am driving reduction"

/* true when a and b contend (see OVERLAP). */
bool claims_overlap(const Claim& a, const Claim& b);

/* Who holds something, as the rule above decides. */
struct ClaimHolder {
    std::string who;    // participant id (this device's own id when `self`)
    std::string name;   // display name, for "Gary is moving this"
    Participant kind = Participant::Person;
    Claim claim;
    bool self = false;
};

class Claims {
  public:
    Claims(std::string self_id, Participant self_kind = Participant::Person)
        : self_(std::move(self_id)), kind_(self_kind) {}

    // ── the Lamport clock ─────────────────────────────────────────────────────
    std::uint64_t clock() const { return clock_; }
    /* Call for every peer presence received (PresenceState::clock and every
     * claim stamp in it): the clock moves past what this device has seen. */
    void observe(const PresenceState& peer);

    // ── this device's claims ──────────────────────────────────────────────────
    /* Try to claim. Returns the holder that wins afterwards: `self` if this
     * device now holds it, otherwise whoever already did (and nothing is
     * added — a claim that would lose is not broadcast, so it cannot pester the
     * winner). Re-claiming something already held keeps the ORIGINAL stamp:
     * holding a port does not get weaker by selecting it again. */
    ClaimHolder acquire(const Roster& roster, std::string_view rune, std::string_view part = {});
    void release(std::string_view rune, std::string_view part = {});
    void release_all() { mine_.clear(); }
    /* Drop this device's claims that a peer now wins (a person arrived on
     * something an agent held, or a concurrent claim resolved the other way).
     * Returns what was lost, so the host can cancel the gesture and say why. */
    std::vector<ClaimHolder> yield(const Roster& roster);

    const std::vector<Claim>& mine() const { return mine_; }

    // ── queries ───────────────────────────────────────────────────────────────
    /* The winner among every claim overlapping (rune, part) — this device's and
     * every peer's in `roster` — or nothing if nobody claims it. */
    std::optional<ClaimHolder> holder(const Roster& roster, std::string_view rune,
                                      std::string_view part = {}) const;
    /* May this device act on (rune, part)? True when unclaimed or it wins. */
    bool may_act(const Roster& roster, std::string_view rune, std::string_view part = {}) const;
    /* For a front-end that assigns without gesturing (an agent's `link`): empty
     * if the act may proceed, else the reason to refuse it, naming the holder. */
    std::string gate(const Roster& roster, std::string_view rune, std::string_view part = {}) const;

  private:
    std::string self_;
    Participant kind_;
    std::uint64_t clock_ = 0;
    std::vector<Claim> mine_;
};

/* The ordering itself, exposed for tests and for hosts that hold claims their
 * own way: true when `a` beats `b`. */
bool claim_wins(Participant a_kind, const Claim& a, std::string_view a_who, Participant b_kind,
                const Claim& b, std::string_view b_who);

} // namespace maiz
