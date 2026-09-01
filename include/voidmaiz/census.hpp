/*
 * voidmaiz/census.hpp — the surface census: a running GUI harvested into OKF
 * concept runes (DRAFT 2026-07-24, author's direction;
 * okf/concepts/surface-census.md). NOT YET IMPLEMENTED — contract first.
 *
 * The author asked for "an engine that registers UI — its shape, its flow —
 * into the OKF", without rewriting the apps already built on Void Maiz. The
 * answer is NOT a markdown writer: Void Core's OKF engine
 * (VoidCore/holidays/okf/) already produces conformant bundles from a mantle,
 * losslessly. What is missing is the HARVEST — so this header emits dispatcher
 * commands that build a doc mantle of `okf-concept` runes, and `produce` writes
 * the markdown afterwards:
 *
 *     running app -> Census -> command lines -> a doc mantle -> okf/surfaces/*.md
 *
 * Which keeps every ground rule intact: the census owns no truth (it emits
 * commands, exactly like a gesture — total-observability's rule), the markdown
 * is a projection of a mantle rather than a maintained file, and Void Maiz
 * ships no documentation format of its own.
 *
 * THE NO-REWRITE PROPERTY. Harvest is a ladder and tier 0 costs an existing app
 * nothing: observe(Core) reads `glyphs` / `mantles` / `help` — the entire node
 * vocabulary, its ports, its editable fields and which widget edits each — off
 * a live core that the app already has. harvest_panes() reads the live ImGui
 * window/dock tree. Tier 1 is one line per registry the host ALREADY built
 * (ActionRegistry::manifest() has been introspectable since 2026-07-21; the
 * census is simply its first writer). Tier 2 (notes) and tier 3 (observed
 * command flow) are opt-in. Nothing above tier 0 is required for a document.
 *
 * WHAT THE CENSUS NEVER DOES: invent prose. It records what is there, never
 * what it means — intent arrives only through SurfaceNote, from a human. A
 * generated doc that claimed to know why a pane exists is precisely the drift
 * the OKF honesty convention exists to prevent.
 *
 * UI-free on purpose (the action.hpp split): this header is part of the base
 * `voidmaiz` library and includes no ImGui. The view-module types
 * (WidgetRegistry, FaceRegistry) are taken as incomplete types by the observe
 * overloads below, whose definitions live in voidmaiz_view — an agent/CLI host
 * censuses a headless app with no ImGui present.
 */
#pragma once

#include "voidmaiz/action.hpp"
#include "voidmaiz/embed.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

struct AddPalette;     // voidmaiz/canvas.hpp  (view module)
struct WidgetRegistry; // voidmaiz/widget.hpp  (view module)
struct FaceRegistry;   // voidmaiz/face.hpp    (view module)

/* One pane as the window manager sees it — the "shape" half, harvested rather
 * than declared. Plain data so the base library stays ImGui-free; the view
 * module's harvest_panes() fills it from the live ImGui context. */
struct PaneInfo {
    std::string name;    // the ImGui window name ("Canvas", "Inspector", "Log")
    bool open = false;   // visible this frame
    bool docked = false; // sitting in a dock node
    std::string dock;    // dock node label, when the host named one
    float x = 0, y = 0, w = 0, h = 0; // screen rect at harvest time
};

/* The one hand-authored input (tier 2): intent, which no amount of reflection
 * can recover. `surface` addresses a harvested concept by its census id
 * ("surfaces/canvas", "vocabulary/glyph/osc", "actions/place").
 *
 * Prose is written to `content.notes`, NEVER to `content.body`, so a re-census
 * overwrites the harvest and structurally cannot touch it. Void Core shipped
 * the engine half on 2026-07-24: `produce` separates the two with a lone
 * `<!-- okf:notes -->` line, `consume` splits them back (field-level lossless),
 * links inside notes still join the graph, and `notes` is a declared field on
 * the `okf-concept` glyph — so authoring is the ordinary `set <c> notes "…"`.
 *
 * `resource` overrides CensusOptions::resource_for for this one concept. */
struct SurfaceNote {
    std::string surface;
    std::string title;    // override the harvested title
    std::string doc;      // the prose -> content.notes
    std::string resource; // e.g. "src/app/canvas_pane.cpp"
};

/* One observed command (tier 3): flow as transcript, not as diagram. `surface`
 * is whatever the host calls the emitter ("canvas", "inspector",
 * "action:place", "command-bar"). */
struct ObservedCommand {
    std::string surface;
    std::string command;
};

/* Concept id -> the source path that backs it, repo-root-relative
 * ("src/app/panes.cpp"). The HOST supplies this: the library can see that a
 * glyph named "osc" exists, but not which file registered it.
 *
 * Not optional decoration — FaultSack's 2026-07-24 reply made it a requirement
 * twice over. (1) `resource:` is what binds the harvested UI half to the code
 * half in their unified graph; without it "a census is a well-formed subgraph
 * attached to nothing", and ranking UI surfaces by responsibility means
 * nothing. (2) The OKF honesty rule fires on exactly `status:current` with no
 * `resource:` — and every census concept is `status:current` by nature — so an
 * unattributed census buries a study session under warnings.
 *
 * Return "" for concepts you can't attribute; those simply carry no resource. */
using ResourceFn = std::function<std::string(std::string_view concept_id)>;

struct CensusOptions {
    std::string mantle = "okf-surfaces"; // the doc mantle the commands build
    std::string app;                     // app id, for the bundle's identity
    std::string timestamp;               // ISO-8601, stamped on EVERY concept —
                                         // host-supplied (the library takes no
                                         // clock, as it takes no I/O). Required
                                         // in practice: drift detection is
                                         // `resource` mtime vs this stamp, so
                                         // an undated concept is invisible to
                                         // freshness checks forever — a waste,
                                         // since a census is the one kind of doc
                                         // that always knows when it was true.
    ResourceFn resource_for;             // concept id -> backing source path
    std::vector<std::string> tags;       // stamped on every concept, on top of
                                         // the census's own (`generated`,
                                         // `type:<T>`, `surface:<kind>`)
    bool include_flows = true;           // emit the tier-3 flow concepts
};

/* The harvester. Additive: call the observe overloads you have, in any order,
 * then compile(). Holds only the accumulated harvest — no model state, no
 * clock, no filesystem. */
class Census {
public:
    explicit Census(CensusOptions opts = {});
    ~Census();

    Census(Census&&) noexcept;
    Census& operator=(Census&&) noexcept;
    Census(const Census&) = delete;
    Census& operator=(const Census&) = delete;

    // ── tier 0: free, no app changes ────────────────────────────────────────
    /* The node vocabulary and document structure, read off a live core with
     * read-only verbs (`glyphs`, `mantles`, `describe`, `links`, `help`) —
     * every glyph becomes a `vocabulary/glyph/<name>` concept carrying its
     * ports, its editable fields, and the editor kind declared for each. */
    void observe(Core& core);
    /* The pane layout (from harvest_panes(), or from a host that tracks its own
     * panes on a substrate where ImGui isn't the window manager). */
    void observe(const std::vector<PaneInfo>& panes);

    // ── tier 1: one line each, over registries the host already built ───────
    void observe(const ActionRegistry& actions); // uses manifest() as-is
    void observe(const AddPalette& palette);     // defined in voidmaiz_view
    void observe(const WidgetRegistry& widgets); // defined in voidmaiz_view
    void observe(const FaceRegistry& faces);     // defined in voidmaiz_view

    // ── tier 2: intent, from a human ────────────────────────────────────────
    void note(SurfaceNote n);

    // ── tier 3: flow, observed ──────────────────────────────────────────────
    /* Record one command and who emitted it. A host calls this from the loop it
     * already has (the log sink, or the `for (cmd : io.commands)` it already
     * writes) — the flow concepts are built from what the UI ACTUALLY did. */
    void observe_command(std::string_view surface, std::string_view command);

    // ── the output ──────────────────────────────────────────────────────────
    /* The harvest as dispatcher command lines, in order: `mantle new`,
     * `rune new okf-concept <id>`, `set`/`setjson` of body/title/description,
     * `tag`, and `link … --relation` for the surface graph. The caller
     * dispatches them into a doc Core and exports it — the library performs no
     * I/O, invents no commands beyond these, and touches no app state. */
    std::vector<std::string> compile() const;

    /* The glyph descriptor the compiled commands need registered on the doc
     * core (Void Core's own `okf-concept`, verbatim — we add no glyph of our
     * own; the concept TYPE rides as a `type:<T>` tag, which is what keeps the
     * existing producer and validator working untouched). */
    static std::string_view okf_concept_glyph();

    /* Convenience: compile() into a fresh Core and hand back its exported state
     * document — the JSON that `python holidays/okf produce` consumes. The one
     * function a `--census` flag needs. */
    std::string state_document() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

/* Harvest the live ImGui window/dock tree (voidmaiz_view; call inside a frame,
 * after the panes have been submitted). The zero-annotation half of "shape":
 * every ImGui::Begin the host already writes is a pane the census can see. */
std::vector<PaneInfo> harvest_panes();

} // namespace maiz
