/* measures_smoke.cpp — values on edges against a live core, and Stevens' rule.
 *
 * The commands ensure_measure and write_value emit are dispatched for real, so
 * this tests what a host would actually get: the core accepts them, the
 * projection reads the values back, an upsert does not duplicate, and every
 * statistic a level does not admit is refused with the reason. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/measures.hpp"
#include "voidmaiz/project.hpp"

#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

static void run(maiz::Core& core, const std::vector<std::string>& cmds) {
    for (const auto& c : cmds) {
        const auto r = core.dispatch(c);
        if (!r.ok) std::cerr << "refused: " << c << " -> " << r.text() << "\n";
        CHECK(r.ok);
    }
}

int main() {
    maiz::Core core;
    core.register_glyph(R"({"glyph":"product","label":"Product","fields":["title"]})");
    core.register_glyph(R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})");
    core.dispatch("mantle new shop");
    for (const char* p : {"apples", "bread", "coffee", "dates"}) core.dispatch(std::string("rune new product ") + p);
    core.dispatch("tag apples +aisle:fruit");
    core.dispatch("tag dates +aisle:fruit");

    maiz::Quantity margin;
    margin.level = "ratio";
    margin.unit = "%";
    std::string err;
    run(core, maiz::ensure_measure(maiz::project_scene(core), "margin", "metric", margin, &err));
    CHECK(err.empty());
    // stated again, nothing to do
    CHECK(maiz::ensure_measure(maiz::project_scene(core), "margin", "metric", margin).empty());
    // a name that is not a measure is refused, not repurposed
    CHECK(maiz::ensure_measure(maiz::project_scene(core), "apples", "metric", margin, &err).empty());
    CHECK(!err.empty());

    run(core, {maiz::write_value("apples", "margin", 40), maiz::write_value("bread", "margin", 25),
               maiz::write_value("coffee", "margin", 60.5), maiz::write_value("dates", "margin", 30)});
    run(core, {maiz::write_value("apples", "margin", 42)}); // an upsert
    maiz::Scene s = maiz::project_scene(core);
    CHECK(maiz::read_value(s, "apples", "margin").value_or(-1) == 42);
    CHECK(maiz::values_of(s, "margin").size() == 4); // not five
    CHECK(!maiz::read_value(s, "apples", "nope"));

    auto sum = maiz::summarise(s, "margin", maiz::Statistic::Sum);
    CHECK(sum.ok && std::fabs(sum.value - (42 + 25 + 60.5 + 30)) < 1e-9 && sum.unit == "%");
    auto fruit = maiz::summarise(s, "margin", maiz::Statistic::Max, "aisle:fruit");
    CHECK(fruit.ok && fruit.value == 42 && fruit.n == 2);
    auto med = maiz::summarise(s, "margin", maiz::Statistic::Median);
    CHECK(med.ok && std::fabs(med.value - 36) < 1e-9); // (30 + 42) / 2: a ratio scale may average

    // the same values on an ORDINAL measure: a rank, not a quantity
    maiz::Quantity stars;
    stars.level = "ordinal";
    run(core, maiz::ensure_measure(s, "rating", "metric", stars));
    run(core, {maiz::write_value("apples", "rating", 4), maiz::write_value("bread", "rating", 2)});
    s = maiz::project_scene(core);
    CHECK(maiz::summarise(s, "rating", maiz::Statistic::Max).ok);
    auto mean = maiz::summarise(s, "rating", maiz::Statistic::Mean);
    CHECK(!mean.ok && mean.error.find("equal intervals") != std::string::npos);
    auto omed = maiz::summarise(s, "rating", maiz::Statistic::Median);
    CHECK(omed.ok && omed.value == 2); // the lower middle: a median must be a value the scale has

    // an INTERVAL measure averages but does not sum; an unstated one only counts
    maiz::Quantity temp;
    temp.level = "interval";
    temp.unit = "C";
    run(core, maiz::ensure_measure(s, "storage_temp", "metric", temp));
    run(core, {maiz::write_value("coffee", "storage_temp", 20), maiz::write_value("dates", "storage_temp", 4)});
    s = maiz::project_scene(core);
    CHECK(maiz::summarise(s, "storage_temp", maiz::Statistic::Mean).ok);
    auto tsum = maiz::summarise(s, "storage_temp", maiz::Statistic::Sum);
    CHECK(!tsum.ok && tsum.error.find("true zero") != std::string::npos);
    run(core, maiz::ensure_measure(s, "label", "metric", maiz::Quantity{}));
    run(core, {maiz::write_value("apples", "label", 7)});
    s = maiz::project_scene(core);
    CHECK(maiz::summarise(s, "label", maiz::Statistic::Count).ok);
    CHECK(!maiz::summarise(s, "label", maiz::Statistic::Min).ok);

    CHECK(maiz::admits("ratio", maiz::Statistic::Sum) && !maiz::admits("interval", maiz::Statistic::Sum));
    CHECK(maiz::admits("", maiz::Statistic::Mode) && !maiz::admits("", maiz::Statistic::Median));

    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "OK - values on edges, and only the statistics their level admits\n";
    return failures ? 1 : 0;
}
