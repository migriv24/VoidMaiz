/* measures.hpp — values on edges, and what may be computed from them.
 *
 * Void Core 0.2.14 (SPEC §3.7.1): an edge whose target is a MEASURE rune is an
 * attribute assertion, and its weight is the value. `product -[40]-> margin`
 * says this product's margin is 40, and the unit is stated once, on `margin`.
 * The projection already carries it (SceneWire::weight, ::is_value; the
 * measure's SceneNode::quantity). What every host was re-deriving is the small
 * vocabulary around it, asked for in Void Hormiga's issue #2 (a client that moved
 * a large catalogue's numbers onto measures): make sure a measure exists, write
 * a value, read one back. Scene in, commands out, so a host dispatches through
 * its own door and the change is logged and replayable like any other.
 *
 * ── AND WHAT A SET OF VALUES MAY HONESTLY BE SUMMARISED BY ──────────────────
 *
 * A measure's `level` is Stevens' level of measurement (Science, 1946), and the
 * level decides which statistics mean anything. Each level is defined by the
 * transformations that leave it intact: any one-to-one relabelling (nominal),
 * any increasing function (ordinal), any increasing AFFINE map x -> a*x + b,
 * a > 0 (interval), any SIMILARITY x -> a*x, a > 0 (ratio). A statistic is
 * meaningful exactly when it commutes with every permissible transformation:
 *
 *     count, mode      nominal and up   (relabelling permutes them)
 *     min, max, median ordinal and up   (an increasing f has max f(x) = f(max x))
 *     mean             interval and up  (mean(a*x+b) = a*mean(x)+b, but an
 *                                        increasing f in general breaks it)
 *     sum              ratio only       (sum(a*x+b) = a*sum(x) + n*b: the offset
 *                                        grows with n, so a sum of temperatures
 *                                        in Celsius means nothing)
 *
 * So `summarise` REFUSES a statistic the level does not admit, with the reason,
 * rather than returning a number that looks right. A measure with no stated
 * level is treated as nominal, the weakest claim, which keeps the refusal on
 * the side of saying too little.
 *
 * The other half of SPEC §3.7.1 is not this file's to enforce: a point (a date,
 * a coordinate, a grid column) is not a magnitude and does not belong on an
 * edge at all. A host decides which of its numbers are which. */
#pragma once

#include "voidmaiz/scene.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace maiz {

/* The commands that make `name` a measure rune of `glyph` (a glyph of kind
 * "measure") stating `quantity`, and none when the scene already shows exactly
 * that. A rune of that name that is NOT a measure is refused (an empty vector
 * and `error`), never repurposed. */
std::vector<std::string> ensure_measure(const Scene& scene, const std::string& name,
                                        const std::string& glyph, const Quantity& quantity,
                                        std::string* error = nullptr);

/* The command asserting `subject`'s value of `measure` (an upsert: Void Core's
 * `link` updates the edge if it exists). `relation` defaults to the measure's
 * own name, so a reader of `links` sees "margin 40", not "1:1". */
std::string write_value(const std::string& subject, const std::string& measure, double value,
                        const std::string& relation = "");

/* The value `subject` asserts for `measure`, if it does. */
std::optional<double> read_value(const Scene& scene, const std::string& subject,
                                 const std::string& measure);

/* Every (subject, value) asserting `measure`, in the scene's wire order. */
std::vector<std::pair<std::string, double>> values_of(const Scene& scene, const std::string& measure);

enum class Statistic { Count, Mode, Min, Max, Median, Mean, Sum };

/* Whether `level` admits `stat` (the table above). "" is treated as nominal. */
bool admits(const std::string& level, Statistic stat);

struct Summary {
    bool ok = false;
    double value = 0;
    std::size_t n = 0;  // how many values it was computed from
    std::string unit;   // the measure's, for the label
    std::string error;  // why it was refused
};

/* `stat` over the values asserting `measure`, optionally only those whose
 * subject passes `subjects` (the tag grammar, exactly as node_matches). */
Summary summarise(const Scene& scene, const std::string& measure, Statistic stat,
                  const std::string& subjects = "");

} // namespace maiz
