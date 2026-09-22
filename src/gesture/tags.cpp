/* tags.cpp — the tag recommender (voidmaiz/tags.hpp).
 *
 * Ported from Void Hormiga's `src/ui/tags.cpp` (2026-08-05, author's ask),
 * with the app state taken out: it was always a read of the scene. The scoring
 * is unchanged, so Hormiga's verification stands — on a synthetic two-cluster-
 * plus-stranded graph the three modes rank differently and each as designed,
 * which tags_smoke re-checks on this side. */
#include "voidmaiz/tags.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace maiz {

std::vector<std::string> suggest_tags(const Scene& scene, const SceneNode& target,
                                      const Suggest& opt) {
    auto meaningful = [&](const std::vector<std::string>& tags) {
        std::set<std::string> m;
        for (const auto& t : tags) {
            bool skip = false;
            for (const auto& p : opt.skip_prefixes)
                if (t.rfind(p, 0) == 0) skip = true;
            if (!skip) m.insert(t);
        }
        return m;
    };
    const std::set<std::string> T = meaningful(target.tags);

    struct Cand {
        std::set<std::string> tags;
        double sim = 0;
        int degree = 0;
    };
    std::vector<Cand> cands;
    for (const auto& n : scene.nodes) {
        if (n.glyph != target.glyph || n.name == target.name) continue;
        cands.push_back({meaningful(n.tags), 0, 0});
    }
    if (cands.empty()) return {};

    auto jaccard = [](const std::set<std::string>& a, const std::set<std::string>& b) {
        if (a.empty() || b.empty()) return 0.0;
        int inter = 0;
        for (const auto& x : a)
            if (b.count(x)) ++inter;
        const int uni = (int)a.size() + (int)b.size() - inter;
        return uni > 0 ? (double)inter / uni : 0.0;
    };
    double sim_sum = 0;
    for (auto& c : cands) {
        c.sim = jaccard(T, c.tags);
        sim_sum += c.sim;
    }
    const double sim_mean = sim_sum / (double)cands.size();
    const int mode = (int)opt.mode;

    if (mode == 2) // how connected each peer is, for the comprehensive mode
        for (std::size_t i = 0; i < cands.size(); ++i)
            for (std::size_t j = i + 1; j < cands.size(); ++j)
                if (jaccard(cands[i].tags, cands[j].tags) > 0) {
                    ++cands[i].degree;
                    ++cands[j].degree;
                }

    std::map<std::string, double> score;
    std::map<std::string, int> freq;
    for (const auto& c : cands)
        for (const auto& t : c.tags) {
            if (T.count(t)) continue; // already on the target
            ++freq[t];
            if (mode == 2) {
                /* The BEST link this tag would forge, not the sum of them.
                 * Hormiga summed, and summing quietly defeats the mode's own
                 * rule: two well-connected peers holding the same tag outscore
                 * one stranded peer, so the stranded node — the whole reason
                 * for this mode — sinks (tags_smoke, 2026-09-22). */
                if (c.sim == 0.0)
                    score[t] = std::max(score[t], 1.0 / (1.0 + c.degree));
                else
                    score[t] += 0.0;
                continue;
            }
            double s = 0;
            if (mode == 0) s = T.empty() ? 1.0 : c.sim; // similarity, or cold-start frequency
            else s = sim_mean - c.sim;                  // dissimilarity, pivoted
            score[t] += s;
        }

    std::vector<std::pair<std::string, double>> ranked;
    for (const auto& [t, s] : score) {
        double v = s;
        if (mode == 1 && sim_mean == 0.0) v = -(double)freq[t]; // no signal → rarest first
        ranked.push_back({t, v});
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });

    std::vector<std::string> out;
    for (const auto& [t, s] : ranked) {
        if ((mode == 0 || mode == 2) && s <= 0) continue; // require real signal
        out.push_back(t);
        if ((int)out.size() >= opt.limit) break;
    }
    return out;
}

} // namespace maiz
