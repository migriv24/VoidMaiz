/*
 * voidmaiz/action.hpp — canvas actions as first-class, NAMED, introspectable
 * commands (DRAFT 2026-07-21, for Void Hormiga's Territory map — the forcing
 * client; okf/concepts/canvas-actions.md).
 *
 * The custom-view seam (views-as-projections.md) already makes a view's WRITES
 * first-class: project_scene in, compiled dispatcher commands out, every
 * gesture logged / undoable / replayable, and an agent reproduces them through
 * the same verbs. What it does NOT make legible is a view's INTERACTION
 * VOCABULARY: a map hand-rolls "place / move / draw-region / select-in-radius"
 * and emits raw commands anonymously, so nothing can enumerate "what does this
 * canvas afford?" — the actions exist in the transcript but not as named,
 * discoverable tools.
 *
 * An ActionDescriptor closes that gap without moving any truth into the
 * library. It NAMES an action, declares its parameter SCHEMA, and carries the
 * host's compile(scene, args) -> command line(s). That one compile is what a
 * gesture handler invokes today and what a CLI/voidscript verb would invoke
 * once Void Core grows host-registered verbs (MESSAGE_FOR_VOIDCORE.md) — so a
 * volunteer's click and an agent's `map place contact @here` become the SAME
 * transcript entry (the "one definition, two front-ends" property, Hormiga §3).
 *
 * The split, per the seam: the library owns naming, enumeration, and the run()
 * entry point; the HOST still owns the drawing, the gesture detection, and what
 * the action MEANS (it supplies compile — the library never invents commands,
 * total-observability's rule). The `gesture` field is a view-interpreted hint
 * ("click", "drag-region"), surfaced for discovery — the library never
 * dispatches a gesture (that stays the view's, like EditorState).
 *
 * UI-free on purpose: descriptors are data + a Scene→strings function, so an
 * agent/CLI host enumerates and invokes actions with no ImGui in sight. This
 * header is part of the base `voidmaiz` library, NOT voidmaiz_view.
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* One declared parameter of an action. `type` is a convention the host and its
 * agents share ("glyph", "geo", "number", "node", "text", "region"…) — the
 * library surfaces it for introspection but never validates or interprets it
 * (semantics are the host's, exactly like glyph hints).
 *
 * Keep this schema ONE type with the future Void Core verb arg-spec: Core's
 * 2026-07-21 ruling on host-verbs (verb macros compile-to-`batch`) recommends
 * that this param-schema also BE the CLI verb's arg-spec, so the gesture, the
 * manifest, and the eventual `map place …` verb all parse against one
 * declaration and cannot drift (okf/concepts/canvas-actions.md). */
struct ActionParam {
    std::string name;
    std::string type;
    bool required = true;
    std::string doc; // one-line human/agent hint
};

/* An action's arguments: param name → raw value (the string/JSON the host
 * encodes). String-keyed on purpose — it keeps the type UI-free and makes the
 * CLI-verb bridge trivial (a verb's tokens map straight in). */
using ActionArgs = std::map<std::string, std::string>;

/* A named, enumerable canvas action. The registrable unit of a view's
 * interaction vocabulary. */
struct ActionDescriptor {
    std::string name;                // "place", "move", "select-in-radius"
    std::string label;               // human label ("Place a contact")
    std::string doc;                 // what it does (agent-legible)
    std::vector<ActionParam> params; // the parameter schema
    std::string gesture;             // view-interpreted binding hint ("click",
                                     // "drag-region", ""); surfaced, not acted on

    /* The ONE definition: args → dispatcher command line(s), in order. Host-
     * supplied; return empty to decline (no command emitted). The library
     * never invents commands — the host owns what the action means. Both the
     * gesture front-end and a CLI verb call THIS, so they cannot drift. */
    std::function<std::vector<std::string>(const Scene&, const ActionArgs&)> compile;
};

/* A view's afforded actions, host-owned like AddPalette / WidgetRegistry (no
 * globals; re-established at boot). One per custom view, typically. */
struct ActionRegistry {
    std::vector<ActionDescriptor> actions;

    void add(ActionDescriptor a);
    const ActionDescriptor* find(std::string_view name) const;

    /* Run an action by name: look it up, compile against the scene, return the
     * command line(s) (empty if the name is unknown or compile declined). The
     * host dispatches + re-projects (the one-sync rule — the library never
     * touches the model). This is the single entry point a gesture handler AND
     * a CLI verb both call: the one-definition property, in one function. */
    std::vector<std::string> run(std::string_view name, const Scene& scene,
                                 const ActionArgs& args) const;

    /* Introspection: a stable JSON manifest of the afforded actions (name,
     * label, doc, gesture, params) — what a host, and through it an agent,
     * reads to DISCOVER "what can this canvas do?" and how to invoke each by
     * name. This is the machine-legible half of total observability extended to
     * a view's vocabulary. */
    std::string manifest() const;
};

} // namespace maiz
