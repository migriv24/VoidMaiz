/*
 * voidmaiz/reduce.hpp — the interaction-net executor, ported from Void Core's
 * portable contract (VoidCore/conformance/reduce/README.md).
 *
 * This is APPLICATION-side machinery (the compute boundary: the canvas never
 * calls it) shipped as a reusable component — the InteractionCombinators demo
 * and VLS-native's compile pipeline both consume it. The Python reference
 * (`reduce/net.py`, `reduce/reduce.py`) is the oracle; this port passes 17 of
 * the 25 pinned conformance cases on canonical forms (checked 2026-09-01). The
 * eight red ones are the contract's 2026-09-01 growth — boxes (17-20, 23, 24),
 * the reserved separator (22) and the `patch` rule (25) — none of which this
 * port implements yet; nothing that once passed regressed. Where the README is
 * ambiguous the reference decides, and ambiguities we hit go upstream as
 * minimal case requests — the 0.2.4 contract adopted two of ours verbatim
 * (`swap` annihilation, cases 11; internal-redex-wire resolution as the
 * DEFAULT, cases 12–14).
 *
 * The model (contract §1–§3): agents with one principal port (index 0) and
 * `arity` auxiliary ports; symmetric wires with linearity; active pairs =
 * principal-principal wires whose glyph pair has a rule; `annihilate` and
 * `commute` on the restricted confluent subset; purity, locality, opacity,
 * and a termination guard.
 *
 * UI-free, core-free: pure data in, pure data out (JSON strings at the seam).
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace maiz::reduce {

/* A malformed net: unknown agent, port out of range, linearity violation,
 * or a mantle edge without the strict "i:j" relation (contract §4). */
struct NetError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ReduceError : std::runtime_error {
    enum class Kind { Termination, Locality, Other };
    Kind kind;
    ReduceError(Kind k, const std::string& msg) : std::runtime_error(msg), kind(k) {}
};

using Port = std::pair<std::string, int>; // (agent id, index); 0 = principal

struct Agent {
    std::string id;
    std::string glyph;
    int arity = 0;
    std::string content = "{}"; // canonical JSON text (sorted keys, compact)
    std::vector<std::string> tags;
};

struct Net {
    std::map<std::string, Agent> agents;
    std::map<Port, Port> link; // symmetric; free ports simply absent

    void add(Agent a);
    void connect(const Port& p, const Port& q); // both must be free
    const Port* partner(const Port& p) const;
    void remove_agent(const std::string& id); // disconnects all its ports
    std::vector<Port> free_ports() const;
    void check() const; // well-formedness (symmetry, valid ports)
};

/* The data-authored rule form (`config.transform.reduce`, contract §2).
 * `fuse` is OUR extension beyond the upstream contract (0.2.4: noted, not
 * adopted — becomes a candidate when a second consumer wants it): the pair
 * rewrites to ONE agent of glyph `into`, whose
 * aux ports adopt a's boundaries 1..m then b's 1..n in order (so
 * arity(into) must equal m+n). Arity-0 fusions — e.g. tag-pigment color
 * mixing, where red~yellow → orange — leave a single free-floating agent. */
struct Spec {
    enum class RuleKind { Annihilate, Commute, Fuse };
    struct Rule {
        std::string a, b; // registered order (rules are oriented a-then-b)
        RuleKind kind;
        std::string into;  // Fuse only: the result glyph
        bool swap = false; // Annihilate only: connect x_i to y_{n+1-i} (Lafont's
                           // γγ — index-swapped, draws PARALLEL between mirrored
                           // bodies; without it x_i–y_i, δδ's crossing look).
                           // Our spelling, adopted verbatim by the 0.2.4
                           // contract (case 11); reversal only, by design — a
                           // general permutation would force an agent order
                           // onto the unordered rule form.
    };
    std::map<std::string, int> signatures; // glyph -> aux-port count
    std::vector<Rule> rules;               // ≤1 per unordered glyph pair
};

/* Parse {"signatures":…,"rules":[…]}. Throws std::invalid_argument on an
 * unknown rule kind, malformed pair, or duplicate pair (the confluence
 * guard). */
Spec spec_from_json(std::string_view spec_json);

/* The strict mantle adapter (contract §4): runes + layout.edges JSON → Net.
 * Every edge relation MUST be "i:j"; anything else throws NetError. */
Net to_net(std::string_view mantle_json, const std::map<std::string, int>& signatures);

/* The same net, read from a projected Scene instead of mantle JSON — so a host
 * whose connections are WIRE RUNES (voidmaiz/wires.hpp) reduces what it draws:
 * pass collapse_wires(project_scene(core)). Agents are the scene's nodes, by
 * name, arity from `signatures` (0 if absent), tags carried. Every wire with
 * two ports connects; a wire with no ports ("Loose") or one marked `contested`
 * (a merge that broke "a wire has two ends") throws NetError — the strict adapter
 * never guesses. Content is not in a Scene, so agents carry "{}". */
Net to_net(const Scene& scene, const std::map<std::string, int>& signatures);

/* Reduce to normal form — pure (the input is copied, never mutated).
 * `opaque` freezes agents by id or glyph. `pick` chooses the index of the
 * next redex among the sorted active pairs (default 0 = canonical-first;
 * confluence makes the choice immaterial on this subset). Throws ReduceError
 * (Termination past max_steps).
 *
 * Internal redex wires (a redex whose aux ports are wired to each other — a
 * legal Lafont net) RESOLVE correctly by default — the contract's default
 * too since 0.2.4 (cases 12–14): annihilation chases the wire equations
 * through the redex (two external ends bridge, one stays free, a closed loop
 * VANISHES — normative ruling: the net model can't represent an agentless
 * wire; hosts wanting Lafont loops-as-values count them host-side),
 * commutation wires the corresponding copies' principals together — a fresh
 * active pair. Pass strict_locality=true for the restricted subset, which
 * rejects them with a Locality error; it is a per-case opt-in key in the
 * contract (case 09 carries it, the runner forwards it) — applications
 * shouldn't set it. */
Net reduce(const Spec& spec, const Net& net, int max_steps = 100000,
           const std::set<std::string>& opaque = {},
           const std::function<size_t(size_t)>& pick = {}, bool strict_locality = false);

/* The sorted active pairs (redexes) of `net` under `spec`: principal-principal
 * wires whose glyph pair has a rule, neither agent opaque. Exposed so hosts
 * can highlight redexes and drive single steps. */
std::vector<std::pair<std::string, std::string>> active_pairs(
    const Spec& spec, const Net& net, const std::set<std::string>& opaque = {});

/* Fire exactly one redex — pure (returns a new net; the input is untouched).
 * Fresh agent ids continue past any existing `_rN` so repeated stepping never
 * collides. Internal redex wires resolve as reduce() describes (strict mode
 * rejects them with Locality). */
Net step(const Spec& spec, const Net& net,
         const std::pair<std::string, std::string>& redex, bool strict_locality = false);

/* The portable canonical form (contract §4) as canonical JSON text:
 * {"agents":[[glyph,content,sorted-tags]…],"free":[…],"wires":[…]} with every
 * list sorted by its elements' canonical text. Id-independent. */
std::string canonical(const Net& net);

/* Canonical JSON text of arbitrary JSON: object keys sorted recursively,
 * compact separators — the contract's one sort/compare key. Throws
 * std::invalid_argument on unparseable input. */
std::string canon_text(std::string_view json);

/* THE FRESH-ID MINTER, exposed (conformance README §2, normative down to the
 * bytes since Void Core 0.2.10).
 *
 *     key = US.join( sorted(glyph_a, glyph_b) + sorted(parent_a, parent_b)
 *                    + [str(ordinal)] )                     US = U+001F
 *     id  = "_r" + hex( SHA-256(key)[0:6] )
 *
 * The two components are sorted SEPARATELY and are not interchangeable — a flat
 * sort of all four strings is a different key and a wrong implementation. Both
 * are unordered, so the id does not depend on which side the executor called A.
 * `ordinal` is 1-based within one rewrite.
 *
 * WHY IT IS PUBLIC RATHER THAN A LAMBDA INSIDE THE REWRITER, WHICH IS WHAT IT
 * WAS. Void Core shipped `16-minter.json` — nine key->id vectors with no
 * reduction around them — precisely because case 15 pins the minter through six
 * other layers, so a wrong digest, a wrong ordinal order and a wrong pair
 * ordering all look identical there. A test can only use those vectors by
 * calling the minter directly, and the alternative — rebuilding the key inside
 * the test — is a second implementation of a normative rule, which is the exact
 * mistake §6.1 taught us not to make twice. */
std::string derived_id(std::string_view glyph_a, std::string_view glyph_b,
                       std::string_view parent_a, std::string_view parent_b,
                       long ordinal);

} // namespace maiz::reduce
