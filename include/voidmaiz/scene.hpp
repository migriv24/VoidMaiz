/*
 * voidmaiz/scene.hpp — the scene graph: what a view draws.
 *
 * A Scene is a *projection* of one mantle in the core's state document — pure
 * data, no rendering types, rebuilt (never mutated in place) after each
 * dispatch. It holds no truth: node positions, wires, tags all come from the
 * state document and glyph descriptors; if it isn't in the model, it isn't
 * here (okf/concepts/views-as-projections.md).
 *
 * Port indexing follows the reduce contract (VoidCore/conformance/reduce):
 * port 0 is the principal, ports 1..n are auxiliary in declaration order.
 * A wire whose edge relation parses as "i:j" joins from-port i to to-port j;
 * "0:0" is a fettuccine (principal–principal interaction wire), any other
 * "i:j" is a linguine (auxiliary dataflow), and a non-numeric relation is a
 * loose semantic link (okf/concepts/interaction-connections.md).
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace maiz {

/* What a number IS (Void Core 0.2.14, SPEC §3.3.2) — the annotation the core
 * stores and never enforces: `{level, unit, min, max}`. It reaches a Scene from
 * two places, deliberately the same shape in both, because an application that
 * keeps a value in a FIELD and one that puts it on an EDGE are describing the
 * same quantity: a glyph's `kinds` map annotates a field (SceneField::quantity),
 * and a measure rune's `quantity` annotates the dimension itself
 * (SceneNode::quantity).
 *
 * `level` is the measurement level — nominal (=), ordinal (<), interval
 * (differences, no true zero: a date), ratio (ratios, true zero: a speed) —
 * and it decides which operations mean anything. Void Maiz does not enforce it
 * either; it READS it, which is the point: a bounded ratio field can be drawn
 * as a knob and an interval one cannot, and until 0.2.14 there was nowhere to
 * say so except a host-private hint. */
struct Quantity {
    bool present = false;   // false = the model said nothing; every other member is then unset
    std::string level;      // "nominal" | "ordinal" | "interval" | "ratio"; "" = unstated
    std::string unit;       // free text ("m/s", "grid-columns"); "" = dimensionless/unstated
    bool has_min = false, has_max = false;
    double min = 0.0, max = 0.0;

    bool bounded() const { return has_min && has_max && max > min; }
};

/* The rune's KIND (SPEC §3.3.1), from its glyph descriptor. Structural, not
 * domain-specific, and the reason it matters to a node-graph library is arity:
 * an edge label can only ever express a BINARY relation, so a ternary fact
 * ("Superman flies across the sky") has to reify its verb as a node with typed
 * ports for the roles — which is an interaction-net agent, which is the thing
 * this library already draws. An `act` rune is that node.
 *
 * Default Entity, and an unknown/absent kind reads as Entity: every rune that
 * existed before 0.2.14 is one, and a projection degrades rather than blanks.
 *
 * DELIBERATELY NOT γ/δ/ε. Void Core proposed those names and withdrew them
 * because this library already spends them in Lafont's sense, about glyphs
 * (voidmaiz/reduce.hpp: `swap` as "Lafont's γγ", and ε as the ERASER — an
 * arity-zero agent that terminates a wire, close to the opposite of "a concept
 * that carries a value"). The reducer contract is untouched by any of this:
 * `signatures` maps glyph → aux-port count and lives in the reduce spec, while
 * `kind` is a different key on the glyph DESCRIPTOR. Neither reads the other. */
enum class RuneKind { Entity, Act, Measure };

struct ScenePort {
    int index = 0;         // net port index (1..n for auxiliary; 0 only for the principal)
    std::string name;      // from glyph hints, or synthesized "p<i>"
    std::string type;      // host type convention ("float", "audio", …); empty = untyped
    bool adjacency = false; // hint "render":"adjacency" — wires on this port are
                            // shown by the bodies touching, not a drawn bezier
                            // (okf/concepts/node-blocks.md)
};

/* One editable field: a key the glyph DECLARES (the editability registry,
 * SPEC §3.3) paired with the rune's current value. Content keys the glyph
 * doesn't declare (like our `pos` convention) are deliberately not here. */
struct SceneField {
    std::string key;
    std::string value_json; // compact JSON ("\"susie\"", "2.5", "[1,2]"…)
    bool is_string = true;  // true → edit as text + `set`; false → `setjson`
    std::string editor;     // hints.editors[key] — "kind" or "kind:args", the
                            // widget-registry binding (voidmaiz/widget.hpp);
                            // empty = the text/JSON fallback
    std::string label;      // hints.labels[key] — the human-facing field label
                            // ("Text (English)" for the key `text_en`); empty =
                            // fall back to the key. Lights up every field
                            // surface at once, exactly like `editor`.
    Quantity quantity;      // the glyph's `kinds[key]` annotation (SPEC §3.3.2).
                            // SCHEMA, not presentation — so it may PICK a
                            // default editor when the glyph declared none (a
                            // bounded ratio field is a knob), and a declared
                            // `editor` always wins over that inference.
};

/* The node's body geometry (okf/concepts/node-geometry.md): shape is
 * notation. Window = the chrome rect (default); Polygon/Circle = compact
 * notation bodies with perimeter port anchors and optional auto-orientation
 * (the principal points at its wire partner); Block = a Blockly/Scratch-style
 * statement block (okf/concepts/node-blocks.md) whose connectors are drawn as
 * a notch (top, the aux input named "prev") and a tab (bottom, the aux output
 * named "next") — connector shape IS the port type, rendered geometrically. */
enum class NodeShape { Window, Polygon, Circle, Block };

struct SceneNode {
    std::string id;        // spirit.id (immutable)
    std::string name;      // spirit.name (the human handle; wire endpoints use it)
    std::string glyph;
    std::string label;     // glyph descriptor label, fallback = glyph name
    std::vector<std::string> tags;

    /* The glyph descriptor's `kind` (SPEC §3.3.1). Entity unless the
     * descriptor says otherwise — including when no descriptor was supplied. */
    RuneKind kind = RuneKind::Entity;

    /* The rune's own `quantity` (SPEC §3.2) — present only on a MEASURE rune,
     * and read from the RUNE rather than the glyph on purpose: `health`,
     * `speed` and `strength` can share one schema and differ only in what they
     * measure. `present` is false for every entity and act rune. */
    Quantity quantity;

    NodeShape shape = NodeShape::Window;
    int shape_sides = 3;      // Polygon only
    bool rot_auto = true;     // shaped bodies: face the principal partner
    float rot = -90.0f;       // degrees; used when not auto or principal free

    float x = 0, y = 0;    // world position
    bool placed = false;   // true if the position came from the model
                           // (rune.placement or content.pos); false = auto-laid-out
    float w = 0, h = 0;    // face size: content.size > hints.face > 0 (renderer default)
    bool collapsed = false;      // content.collapsed — header-only chrome
    std::string enter_mantle;    // non-empty: double-click enters this mantle
                                 // (hints.enter names the content field holding it)

    unsigned rgb = 0;      // header color from hints ("#rrggbb"); 0 = default
    bool has_color = false;

    std::string principal_name;          // hints name for port 0 (may be empty)
    std::vector<ScenePort> inputs;       // auxiliary, dir=in  (drawn left)
    std::vector<ScenePort> outputs;      // auxiliary, dir=out (drawn right)
    std::vector<SceneField> fields;      // glyph-declared, in declaration order
};

struct SceneWire {
    enum class Kind {
        Linguine,   // auxiliary dataflow: directed, typed, fan-out allowed
        Fettuccine, // principal–principal interaction wire: symmetric, 1:1
        Loose,      // named semantic relation (not "i:j") — rendered dimmer
    };
    /* How the wire is DRAWN — a render style, not a fourth wire kind
     * (semantics unchanged; okf/concepts/node-blocks.md). Adjacency: the
     * bodies touching IS the visible connection; the canvas draws nothing
     * while the endpoints sit flush, and a dim honest line when they don't. */
    enum class Style { Drawn, Adjacency };
    std::string from, to;  // node names
    int from_port = -1;    // net indices; -1 when Kind::Loose
    int to_port = -1;
    Kind kind = Kind::Loose;
    Style style = Style::Drawn; // set by projection from the port hints
    std::string relation;  // the raw edge relation
    bool directed = true;
    /* The core's edge weight (`link a b --weight w`), carried through so the
     * projection agrees with the model and the CLI. Default 1.0 — the core's
     * own default, and what an edge stored without `--weight` reports.
     *
     * PROJECTED BECAUSE IT IS STORED. Hormiga found this missing the hard way
     * (2026-08-17): the dispatcher accepted a weight, `links` reported it, and
     * `project_scene` silently dropped it — an asymmetry between three layers
     * that otherwise agree, and silent is the part that cost the hour. The rule
     * this settles: **every edge attribute the core stores makes the trip**.
     * Anything richer than what an edge natively carries still belongs on a
     * rune (which is what Hormiga did, and was the better design anyway). */
    double weight = 1.0;
    /* THE WEIGHT IS A VALUE, NOT A STRENGTH (SPEC §3.7.1, Void Core 0.2.14).
     * True when `to` resolves to a MEASURE rune, which makes this edge an
     * attribute assertion — `player --[weight 5]--> speed` says the player's
     * speed is 5, and the unit comes off the measure rune
     * (`scene.find(wire.to)->quantity.unit`, or `value_label` in project.hpp,
     * which is the one spelling).
     *
     * THE DIRECTION IS NORMATIVE: `to` names the measure. An assertion has an
     * owner and a dimension and they are not interchangeable, so this is never
     * inferred from the `from` end.
     *
     * A renderer must branch on it, because the two readings look nothing
     * alike: a strength is properly drawn as thickness or opacity relative to
     * the other wires, and a value is not comparable to anything on the canvas
     * — 900 rpm drawn nine hundred times thicker than "supports, 1.0" is a lie
     * the projection would otherwise be complicit in. Draw a value as a label. */
    bool is_value = false;
    bool active = false;   // host-set after projection (e.g. a rule-bearing
                           // active pair); the canvas renders it emphasized
};

struct Scene {
    std::string mantle;    // which mantle this projects
    std::vector<SceneNode> nodes;
    std::vector<SceneWire> wires;

    const SceneNode* find(std::string_view name) const {
        for (const auto& n : nodes)
            if (n.name == name) return &n;
        return nullptr;
    }
};

} // namespace maiz
