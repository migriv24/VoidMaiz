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
