/* measures.cpp — see measures.hpp. */
#include "voidmaiz/measures.hpp"

#include "voidmaiz/embed.hpp"   // maiz::arg: the one way a value reaches a command line
#include "voidmaiz/project.hpp" // node_matches: the tag grammar a host already speaks

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace maiz {

namespace {

int rank(const std::string& level) {
    if (level == "ratio") return 3;
    if (level == "interval") return 2;
    if (level == "ordinal") return 1;
    return 0; // nominal, or unstated: the weakest claim
}

int needs(Statistic s) {
    switch (s) {
    case Statistic::Count:
    case Statistic::Mode: return 0;
    case Statistic::Min:
    case Statistic::Max:
    case Statistic::Median: return 1;
    case Statistic::Mean: return 2;
    case Statistic::Sum: return 3;
    }
    return 3;
}

const char* name_of(Statistic s) {
    switch (s) {
    case Statistic::Count: return "a count";
    case Statistic::Mode: return "a mode";
    case Statistic::Min: return "a minimum";
    case Statistic::Max: return "a maximum";
    case Statistic::Median: return "a median";
    case Statistic::Mean: return "a mean";
    case Statistic::Sum: return "a sum";
    }
    return "that";
}

const char* why_not(Statistic s) {
    switch (needs(s)) {
    case 1: return "it needs an order, and a nominal measure has none";
    case 2: return "it needs equal intervals, which an ordinal measure does not promise";
    default: return "it needs a true zero: shifting the scale changes a sum by the shift times the count";
    }
}

std::string number(double v) {
    char b[48];
    std::snprintf(b, sizeof b, "%.17g", v); // exact round trip through text
    return b;
}

} // namespace

bool admits(const std::string& level, Statistic stat) { return rank(level) >= needs(stat); }

std::vector<std::string> ensure_measure(const Scene& scene, const std::string& name,
                                        const std::string& glyph, const Quantity& quantity,
                                        std::string* error) {
    std::vector<std::string> out;
    const SceneNode* n = scene.find(name);
    if (n && n->kind != RuneKind::Measure) {
        if (error) *error = "'" + name + "' exists and is not a measure: it is not repurposed";
        return out;
    }
    if (!n) out.push_back("rune new " + arg(glyph) + " " + arg(name));
    const Quantity have = n ? n->quantity : Quantity{};
    std::string flags;
    if (!quantity.level.empty() && quantity.level != have.level) flags += " --level " + arg(quantity.level);
    if (!quantity.unit.empty() && quantity.unit != have.unit) flags += " --unit " + arg(quantity.unit);
    if (quantity.has_min && !(have.has_min && have.min == quantity.min)) flags += " --min " + number(quantity.min);
    if (quantity.has_max && !(have.has_max && have.max == quantity.max)) flags += " --max " + number(quantity.max);
    if (!flags.empty()) out.push_back("measure " + arg(name) + flags);
    return out;
}

std::string write_value(const std::string& subject, const std::string& measure, double value,
                        const std::string& relation) {
    return "link " + arg(subject) + " " + arg(measure) + " --relation " +
           arg(relation.empty() ? measure : relation) + " --weight " + number(value);
}

std::optional<double> read_value(const Scene& scene, const std::string& subject,
                                 const std::string& measure) {
    for (const auto& w : scene.wires)
        if (w.is_value && w.from == subject && w.to == measure) return w.weight;
    return std::nullopt;
}

std::vector<std::pair<std::string, double>> values_of(const Scene& scene, const std::string& measure) {
    std::vector<std::pair<std::string, double>> out;
    for (const auto& w : scene.wires)
        if (w.is_value && w.to == measure) out.emplace_back(w.from, w.weight);
    return out;
}

Summary summarise(const Scene& scene, const std::string& measure, Statistic stat,
                  const std::string& subjects) {
    Summary s;
    const SceneNode* m = scene.find(measure);
    if (!m || m->kind != RuneKind::Measure) {
        s.error = "'" + measure + "' is not a measure in this mantle";
        return s;
    }
    s.unit = m->quantity.unit;
    if (!admits(m->quantity.level, stat)) {
        const std::string lvl = m->quantity.level.empty() ? "unstated (read as nominal)" : m->quantity.level;
        s.error = std::string(name_of(stat)) + " of '" + measure + "' means nothing: its level is " + lvl +
                  ", and " + why_not(stat);
        return s;
    }
    std::vector<double> v;
    for (const auto& [subject, value] : values_of(scene, measure)) {
        if (!subjects.empty()) {
            const SceneNode* n = scene.find(subject);
            if (!n || !node_matches(subjects, *n)) continue;
        }
        v.push_back(value);
    }
    s.n = v.size();
    if (stat == Statistic::Count) { s.value = (double)v.size(); s.ok = true; return s; }
    if (v.empty()) { s.error = "no values to summarise"; return s; }
    switch (stat) {
    case Statistic::Mode: {
        std::map<double, std::size_t> seen;
        for (double x : v) ++seen[x];
        // the most frequent, and the smallest of a tie: an answer that depends on order is not one
        s.value = std::max_element(seen.begin(), seen.end(),
                                   [](const auto& a, const auto& b) { return a.second < b.second; })->first;
        break;
    }
    case Statistic::Min: s.value = *std::min_element(v.begin(), v.end()); break;
    case Statistic::Max: s.value = *std::max_element(v.begin(), v.end()); break;
    case Statistic::Median: {
        std::sort(v.begin(), v.end());
        const std::size_t k = v.size() / 2;
        // the lower middle of an even count: a median of an ordinal scale must be a value it has
        s.value = v.size() % 2 ? v[k] : (rank(m->quantity.level) >= 2 ? (v[k - 1] + v[k]) / 2 : v[k - 1]);
        break;
    }
    case Statistic::Mean: {
        double sum = 0, c = 0; // Kahan: a catalogue's worth of prices should not drift
        for (double x : v) {
            const double y = x - c, t = sum + y;
            c = (t - sum) - y;
            sum = t;
        }
        s.value = sum / (double)v.size();
        break;
    }
    case Statistic::Sum: {
        double sum = 0, c = 0;
        for (double x : v) {
            const double y = x - c, t = sum + y;
            c = (t - sum) - y;
            sum = t;
        }
        s.value = sum;
        break;
    }
    default: break;
    }
    s.ok = true;
    return s;
}

} // namespace maiz
