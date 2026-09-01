/*
 * voidmaiz/usergraph.hpp — the attention graph: a symmetric, timeless map of
 * what was actually TOUCHED TOGETHER, so a script can be responsive to the work
 * being done and not only to the data (DRAFT 2026-08-06, generalized
 * 2026-08-29; okf/concepts/user-graph.md).
 *
 * "USER" IS THE ORIGINAL CASE, NOT THE ABSTRACTION. This began as a GUI
 * structure — widgets a person clicks, reached through a pointer or a touch —
 * and the shape it found is more general than the case that produced it: a set
 * of addresses attended to inside one working frame, folded into a symmetric
 * hyperedge. Nothing in that sentence needs a person, a screen, or a hand. An
 * agent working through the dispatcher has working frames. A harvest has
 * stages. A game has turns. The closed `Device` enum was the last thing
 * insisting otherwise, and it is now an open `channel` string.
 *
 * WHAT IS STILL OPEN: which mantle a compiled attention graph belongs in. It is
 * `compile(mantle)`'s argument and therefore the host's call today, but "the
 * mantle of attention" is not obviously one thing when the attender is not one
 * person at one screen (developer_questions.md Q25).
 *
 * NOT A LOG. The author's framing is explicit: "not a list of previous actions,
 * not trying to predict user actions, but a big graph that represents user
 * actions" — and, overall, avoid linear thinking. So this deliberately refuses
 * the shape everything else in this space takes:
 *
 *   - Nodes are AFFORDANCES, not events — the things a person can touch (a
 *     widget, an action, a node, a pane), each carrying a touch weight.
 *   - Edges are SYMMETRIC and TIMELESS. No timestamps, no ordering, no
 *     direction: the only claim an edge makes is "these were touched in the
 *     same working frame." Reversing history changes nothing here, which is the
 *     point — coherence is a topological fact about the person's attention, not
 *     a sequence.
 *   - A frame closes into a HYPEREDGE (the whole coherent set) as well as its
 *     pairwise projections, so "these three go together" survives instead of
 *     being lost to three unrelated pairs.
 *
 * THE CHANNEL IS FIRST-CLASS. A tap is not a click and a gaze is neither, and
 * an agent's dispatch is none of the three; `channel` records how a touch
 * actually arrived, so a mobile substrate is honest rather than pretending to
 * be a mouse — and a non-GUI host is honest rather than pretending to be a
 * substrate at all. Per substrates.md's quarantine, the library records the
 * channel and never interprets it, which is now true of the TYPE and not only
 * of the prose (see `channel` below).
 *
 * WHY THIS IS ALLOWED TO EXIST. total-observability.md rules hover and
 * selection EPHEMERA — "Never state" — so a persisted attention graph would be
 * state accumulated from things declared never-state. The resolution is the
 * census.hpp pattern verbatim: accumulate in-session, then compile() to
 * dispatcher commands. The graph becomes runes in a mantle — logged,
 * replayable, undoable, and queryable BY Allomone itself — instead of a private
 * library structure nobody can see. Drop the object and nothing is lost that
 * the model didn't already have.
 *
 * PRECEDENT, AND THE DEPARTURE. Eclipse Mylyn shipped this idea (a
 * degree-of-interest model over program elements, used to filter the IDE) and
 * weights by frequency AND recency. We drop recency on purpose — it is what
 * makes the structure symmetric and order-free — and accept the known cost:
 * without decay, stale coherence accumulates. The mitigation is that the graph
 * is model content, so pruning is an ordinary command rather than hidden policy.
 *
 * UI-free: part of the base `voidmaiz` library, no ImGui. A host feeds it from
 * the input it already handles.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* THE CHANNEL an attention arrived through — an OPEN string, not an enum.
 *
 * This was `enum class Device { Pointer, Touch, Pen, Gamepad, Voice, XR }`
 * until 2026-08-29, and the doc above it said, in these words, that the library
 * "records the modality and never interprets it". A closed enumeration of six
 * HID modalities is an interpretation: it decides in advance that attention
 * arrives through a hand on a device, which is true of a GUI and of nothing
 * else. An agent's attention arrives through the dispatcher. A game's arrives
 * through play. A harvest's arrives through a pipeline stage. None of those is
 * a Pointer, and forcing them to pick one is the library lying on the host's
 * behalf.
 *
 * So `channel` is a host string under the same quarantine `kind`, `property`
 * and glyph hints already live under: transported, compared, never understood.
 * The six spellings below are the ones Void Maiz's own GUI uses, kept as
 * constants so a host that IS a GUI does not invent six new ones — they are a
 * vocabulary, not a type.
 *
 * (Author's direction, 2026-08-29: *"the user graph needs to be more abstract
 * on what it is being applied to."*) */
namespace channel {
inline constexpr std::string_view pointer = "pointer";
inline constexpr std::string_view touch   = "touch";
inline constexpr std::string_view pen     = "pen";
inline constexpr std::string_view gamepad = "gamepad";
inline constexpr std::string_view voice   = "voice";
inline constexpr std::string_view xr      = "xr";
} // namespace channel

/* One node of the graph: something that can be ATTENDED TO. `id` is the host's
 * address for it — the same address space annotations use, so a rule that
 * matched via the user graph can annotate what it found.
 *
 * "Affordance" is still the right word and is deliberately kept: it names a
 * thing that CAN be attended to, which is the abstraction — a widget, a pane, a
 * rune, a dataset column, an inventory slot, a step in a harvest. What made the
 * old shape GUI-only was never this struct's name; it was the closed `Device`
 * enum and the `"widget"` default, both of which are now open. */
struct Affordance {
    std::string id;      // "widget:volume", "rune:maria-lopez", "slot:ring-a"
    std::string kind;    // host vocabulary — never interpreted here
    std::string channel; // how it was last reached; "" = the host did not say
    int touches = 0;     // total weight
};

/* One symmetric edge: `a` and `b` were touched inside the same frame, `weight`
 * times. Canonically ordered (a < b) so the edge has ONE representation and the
 * structure cannot smuggle in a direction. */
struct Coincidence {
    std::string a, b;
    int weight = 0;
};

/* One closed frame: the whole set of affordances touched together. Kept
 * alongside the pairwise projection because a triple is not three pairs — this
 * is the hypergraph half, and it is what "the runes coherent with this one"
 * should really consult. */
struct Frame {
    std::vector<std::string> members; // sorted, deduped
    int weight = 0;                   // how many times this exact set recurred
};

/* The accumulator. Holds ephemera for the session and compiles them into
 * commands; owns no truth and takes no clock. */
class UserGraph {
public:
    /* Open a coherence window. Touches recorded until close_frame() are
     * considered part of one piece of work. Nesting is flattened — a frame is a
     * set, not a stack. */
    void open_frame();
    /* Close the window: fold its members into the hyperedge table and the
     * pairwise coincidences. A frame of 0 or 1 members contributes no edge
     * (nothing co-occurred) but its touches still count. */
    void close_frame();

    /* Record one touch. Outside a frame this counts the touch and nothing else;
     * inside one it also joins the coherent set.
     *
     * `kind` and `channel` default to EMPTY rather than to "widget"/"pointer".
     * A default that names a GUI is a claim about the host, and the honest
     * answer for a host that did not say is "not stated" — which `device ""`
     * can then match on purpose, instead of matching a pointer nobody used. */
    void touch(std::string_view id, std::string_view kind = {},
               std::string_view channel = {});

    // ── reading (what a script consults) ────────────────────────────────────
    const Affordance* find(std::string_view id) const;
    const std::vector<Affordance>& affordances() const { return nodes_; }
    const std::vector<Coincidence>& coincidences() const { return edges_; }
    const std::vector<Frame>& frames() const { return frames_; }

    /* The coherence weight between two affordances — symmetric by construction,
     * 0 if they never shared a frame. */
    int coherence(std::string_view a, std::string_view b) const;

    /* The affordances coherent with `id`, strongest first (ties by id, so the
     * answer is determinate). `min_weight` filters noise; 0 returns everything
     * that ever co-occurred. This is what Allomone's `with` operator reads. */
    std::vector<std::string> coherent_with(std::string_view id,
                                           int min_weight = 1) const;

    void clear();
    bool empty() const { return nodes_.empty(); }

    // ── writing (the census pattern: ephemera -> commands) ──────────────────
    /* The graph as dispatcher command lines that build it as runes in `mantle`:
     * an `allomone-affordance` rune per node (kind, channel, touches) and an
     * undirected `link --relation coherence:<w>` per edge. The caller
     * dispatches and re-projects; the library performs no I/O and invents no
     * commands beyond these.
     *
     * Materializing is a CHOICE the host makes — a session that never calls
     * this leaves no trace, which keeps the ephemera rule intact by default. */
    std::vector<std::string> compile(std::string_view mantle) const;

    /* The glyph descriptor the compiled commands need registered, mirroring
     * Census::okf_concept_glyph(): the library ships the shape it emits so a
     * host doesn't have to guess it. */
    static std::string_view affordance_glyph();

private:
    std::vector<Affordance> nodes_;
    std::vector<Coincidence> edges_;
    std::vector<Frame> frames_;
    std::vector<std::string> open_;   // members of the frame being built
    bool framing_ = false;

    Affordance& node_for(std::string_view id, std::string_view kind,
                         std::string_view channel);
};

} // namespace maiz
