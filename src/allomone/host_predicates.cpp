/* host_predicates.cpp — `with` and `device`, after they stopped being kernel.
 *
 * These two were Allomone kernel predicates until the language was extracted
 * (2026-08-29). They were the only two that read something outside a Subject —
 * this library's UserGraph — which made the language's kernel depend on one
 * host's data structure. That is the definition of a DSL rather than a
 * language, and it is the line Void Unity said only we could draw.
 *
 * Drawing it cost nothing, which is the interesting part: a host predicate
 * CLOSES OVER whatever host state it needs, so `with` and `device` are the same
 * two functions they always were, reached through the registry that already
 * existed for exactly this. Every script that used them still parses (parsing
 * was always registry-free), still colours, and — once a host calls the one
 * function below — still means precisely what it meant.
 *
 * The general shape, for any host reading this: if a predicate of yours needs
 * host state, capture it. The language will never hand you a pointer to it,
 * and that is the feature.
 */
#include "voidmaiz/allomone.hpp"

namespace maiz {

void register_user_graph_predicates(allomone::PredicateRegistry& preds,
                                    const UserGraph& graph) {
    /* The responsive operator: coherence with something the person works on,
     * not a property of the data. Symmetric and timeless by construction — see
     * usergraph.hpp on why there is no recency here. */
    preds.add("with", [&graph](const Subject& subject, std::string_view arg) {
        return graph.coherence(subject.id, arg) > 0;
    });

    /* How this subject was actually reached. With no record the honest answer
     * is "unknown", which is not a match — the same silence an unregistered
     * predicate gives, and for the same reason: not knowing must never be read
     * as knowing the negative. */
    preds.add("device", [&graph](const Subject& subject, std::string_view arg) {
        const Affordance* a = graph.find(subject.id);
        return a && a->channel == arg;
    });
}

/* The compatibility overloads. See voidmaiz/allomone.hpp for why they honour
 * the graph rather than ignoring it. */
namespace {
allomone::PredicateRegistry with_graph(const UserGraph* graph,
                                   const allomone::PredicateRegistry* preds) {
    allomone::PredicateRegistry combined = preds ? *preds : allomone::PredicateRegistry{};
    if (graph) register_user_graph_predicates(combined, *graph);
    return combined;
}
} // namespace

allomone::ConstraintMap allo_eval(const Script& script,
                              const std::vector<Subject>& subjects,
                              const UserGraph* graph,
                              const allomone::PredicateRegistry* preds) {
    /* A host predicate captures BY REFERENCE, so the registry the two closures
     * live in must outlive the call — it does: `combined` is alive for the
     * whole of eval(), and eval() keeps nothing. */
    const allomone::PredicateRegistry combined = with_graph(graph, preds);
    return allomone::eval(script, subjects, &combined);
}

bool allo_matches(const Rule& rule, const Subject& subject,
                  const UserGraph* graph, const allomone::PredicateRegistry* preds) {
    const allomone::PredicateRegistry combined = with_graph(graph, preds);
    return allomone::matches(rule, subject, &combined);
}

} // namespace maiz

// ── networking (okf/concepts/networking.md) ──────────────────────────────────

namespace maiz {

namespace {
/* The rune id a subject stands for. A host that keyed its Subjects on the rune
 * NAME gets resolved through the scene; otherwise the subject id is the rune id. */
std::string rune_id_of(const Subject& subject, const Scene* scene) {
    if (scene) {
        if (find_by_id(*scene, subject.id)) return subject.id;
        if (const SceneNode* n = scene->find(subject.id)) return n->id;
    }
    return subject.id;
}
} // namespace

void register_presence_predicates(allomone::PredicateRegistry& preds, const Roster& roster,
                                  const Scene* scene) {
    preds.add("present", [&roster, scene](const Subject& subject, std::string_view arg) {
        for (const Peer* p : roster.on_rune(rune_id_of(subject, scene)))
            if (arg.empty() || p->state.who.id == arg || p->state.who.name == arg) return true;
        return false;
    });
}

ShareFilter share_by_annotation(const allomone::Merged& merged, std::string property) {
    // copied: a filter the sync seam holds must not dangle into a dead Merged
    return [merged, property = std::move(property)](const SceneNode& n) {
        const allomone::MergedCell* c = merged.find(n.id, property);
        if (!c) c = merged.find(n.name, property);
        if (!c) return true;                 // no rule spoke
        if (c->conflicted) return false;     // disagreement never means "send it"
        return !(c->value == "false" || c->value == "0");
    };
}

} // namespace maiz
