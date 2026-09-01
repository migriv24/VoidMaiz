/*
 * voidmaiz/allomone.hpp — Allomone, as Void Maiz spells it.
 *
 * THE LANGUAGE AND THE MERGE LIVE IN VOID ALLOMONE NOW (`../VoidAllomone`,
 * 2026-08-29). This header holds no implementation. It includes the real thing
 * and re-exports it into `maiz::`, so that:
 *
 *   - a Void Maiz host writes `maiz::Script`, `maiz::merge`, `maiz::Lattice`
 *     exactly as before, and
 *   - **Void Hormiga compiles unchanged.** They use sixteen of these types by
 *     their `maiz::` names across a 12k-line application, and an extraction
 *     that hands its first adopter a day of find-and-replace is an extraction
 *     that gets reverted. The names are theirs; the code moving out from under
 *     them is our business, not theirs.
 *
 * WHAT DID CHANGE, AND CANNOT BE PAPERED OVER: `device` and `with` are no
 * longer kernel predicates. They were the only two that read something outside
 * a Subject — Void Maiz's UserGraph — and a language whose kernel needs one
 * host's data structure is that host's DSL wearing a language's clothes. They
 * are HOST predicates now, and Void Maiz registers them:
 *
 *     maiz::PredicateRegistry preds;
 *     maiz::register_user_graph_predicates(preds, graph);   // `with`, `device`
 *
 * Every script that used them still parses, still colours, and still means the
 * same thing — parsing was always registry-free — and now behaves identically
 * once that one line is called.
 *
 * The `allo_` prefix on the free functions is kept here and dropped upstream:
 * `allomone::parse` is the real name, `maiz::allo_parse` is what two hosts already
 * type. The prefix existed because `maiz::` was the only namespace; upstream it
 * would stutter.
 */
#pragma once

#include "allomone/allomone.hpp"
#include "voidmaiz/usergraph.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace maiz {

// ── the language, under its Void Maiz spelling ──────────────────────────────
using allomone::Subject;
using allomone::TokenKind;
using allomone::Token;
using allomone::Term;
using allomone::Effect;
using allomone::Rule;
using allomone::Diagnostic;
using allomone::DefItem;
using allomone::Definition;
using allomone::Script;
using allomone::PredicateFn;

/* The registry, plus ONE compatibility overload.
 *
 * Until the extraction a predicate was handed a `const UserGraph*` by the
 * language itself. It no longer is: a host predicate closes over whatever host
 * state it needs, which is what let `with` and `device` leave the kernel
 * without losing anything. Hormiga's twenty-two predicates all take that third
 * parameter and (by their own account) none of them read it, so the overload
 * below adapts the old shape rather than making them delete an argument
 * twenty-two times.
 *
 * DEPRECATED, and deliberately not sugar: it passes `nullptr`, because there is
 * no graph to pass. A predicate that actually wants the user graph must capture
 * one — see register_user_graph_predicates(). */
struct PredicateRegistry : allomone::PredicateRegistry {
    using allomone::PredicateRegistry::add;

    using LegacyFn = std::function<bool(const Subject&, std::string_view,
                                        const UserGraph*)>;
    void add(std::string name, LegacyFn fn) {
        if (!fn) { allomone::PredicateRegistry::add(std::move(name), nullptr); return; }
        allomone::PredicateRegistry::add(
            std::move(name),
            [fn = std::move(fn)](const Subject& s, std::string_view arg) {
                return fn(s, arg, nullptr);
            });
    }
};

/* Free functions keep the `allo_` prefix two hosts already type. Inline
 * forwarders rather than `using`, because upstream renamed them. */
inline std::vector<Token> allo_tokens(std::string_view source) {
    return allomone::tokens(source);
}
inline Script allo_parse(std::string_view id, std::string_view source) {
    return allomone::parse(id, source);
}
inline std::vector<Diagnostic> allo_check(const Script& script,
                                          const allomone::PredicateRegistry* preds) {
    return allomone::check(script, preds);
}
inline allomone::ConstraintMap allo_eval(const Script& script,
                                     const std::vector<Subject>& subjects,
                                     const allomone::PredicateRegistry* preds = nullptr) {
    return allomone::eval(script, subjects, preds);
}
inline bool allo_matches(const Rule& rule, const Subject& subject,
                         const allomone::PredicateRegistry* preds = nullptr) {
    return allomone::matches(rule, subject, preds);
}

/* COMPATIBILITY: the pre-extraction call shape, `(script, subjects, graph,
 * preds)`.
 *
 * NOT A NO-OP ADAPTER. A non-null `graph` still makes `with` and `device` work,
 * exactly as it did when they were kernel predicates — this builds a registry
 * that is `*preds` plus those two, and evaluates against it. Dropping the graph
 * on the floor would have been three lines shorter and would have silently
 * turned every responsive rule into a rule that never fires, which is precisely
 * the class of change this project keeps writing messages to other agents
 * about. A compatibility shim that changes behaviour is not compatibility.
 *
 * `add` replaces by name, so a host that registered its own `with` keeps it. */
allomone::ConstraintMap allo_eval(const Script& script,
                              const std::vector<Subject>& subjects,
                              const UserGraph* graph,
                              const allomone::PredicateRegistry* preds);
bool allo_matches(const Rule& rule, const Subject& subject,
                  const UserGraph* graph, const allomone::PredicateRegistry* preds);

/* THE TWO PREDICATES THAT USED TO BE KERNEL, registered as what they always
 * were: Void Maiz domain terms that happen to be older than the boundary.
 *
 *   with "<id>"      this subject shares a working frame with <id>
 *   device "<name>"  this subject was last reached through <name>
 *
 * `graph` must outlive the registry — the predicates capture it by reference,
 * which is the ordinary contract for a projection that is rebuilt per frame.
 * With no registration at all a script asking about the user gets silence
 * rather than an error, exactly as before. */
void register_user_graph_predicates(allomone::PredicateRegistry& preds,
                                    const UserGraph& graph);

} // namespace maiz
