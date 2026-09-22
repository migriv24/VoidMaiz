/* tags_smoke.cpp — the tag recommender (voidmaiz/tags.hpp).
 *
 * The graph is Hormiga's verification graph, which is the point: the scoring
 * moved repositories and must still rank the same way. Two clusters and one
 * stranded node, and for a target inside cluster A:
 *   similar       → the cluster-A tag it lacks
 *   dissimilar    → the OTHER cluster's tags first, its own last
 *   comprehensive → the stranded node's tag first
 */
#include "voidmaiz/tags.hpp"

#include <algorithm>
#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz;

static SceneNode node(std::string name, std::string glyph, std::vector<std::string> tags) {
    SceneNode n;
    n.id = "id-" + name;
    n.name = std::move(name);
    n.glyph = std::move(glyph);
    n.tags = std::move(tags);
    return n;
}

static bool has(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}
static int at(const std::vector<std::string>& v, const std::string& s) {
    auto it = std::find(v.begin(), v.end(), s);
    return it == v.end() ? -1 : (int)(it - v.begin());
}

int main() {
    Scene scene;
    // cluster A: alpha/beta/gamma share "a1","a2"; gamma also has "a3"
    scene.nodes.push_back(node("a-one", "card", {"a1", "a2"}));
    scene.nodes.push_back(node("a-two", "card", {"a1", "a2", "a3"}));
    // cluster B: far away
    scene.nodes.push_back(node("b-one", "card", {"b1", "b2"}));
    scene.nodes.push_back(node("b-two", "card", {"b1", "b2"}));
    // the stranded one: shares with nobody
    scene.nodes.push_back(node("lonely", "card", {"z1"}));
    // another glyph entirely: never consulted
    scene.nodes.push_back(node("other", "note", {"never"}));

    SceneNode target = node("target", "card", {"a1", "a2"});
    scene.nodes.push_back(target);

    {   // similar: the tag its own cluster has and it lacks
        auto s = suggest_tags(scene, target, {SuggestMode::Similar, 6});
        CHECK(!s.empty());
        CHECK(s.front() == "a3");
        CHECK(!has(s, "never")); // a different glyph is a different question
        CHECK(!has(s, "a1"));    // nothing it already carries
    }
    {   // dissimilar: the far cluster first, its own neighbourhood last
        auto s = suggest_tags(scene, target, {SuggestMode::Dissimilar, 6});
        CHECK(at(s, "b1") >= 0 && at(s, "a3") >= 0);
        CHECK(at(s, "b1") < at(s, "a3"));
    }
    {   // comprehensive: the most stranded node's tag first
        auto s = suggest_tags(scene, target, {SuggestMode::Comprehensive, 6});
        CHECK(!s.empty());
        CHECK(s.front() == "z1");
        CHECK(!has(s, "a3")); // a3's holder already overlaps: no new link
    }
    {   // a directive prefix is neither a signal nor a suggestion
        Scene s2 = scene;
        for (auto& n : s2.nodes)
            if (n.glyph == "card") n.tags.push_back("color:blue");
        auto s = suggest_tags(s2, target, {SuggestMode::Similar, 6});
        CHECK(!has(s, "color:blue"));
    }
    {   // nothing of this glyph to learn from: say nothing
        Scene lone;
        lone.nodes.push_back(node("only", "card", {"a1"}));
        CHECK(suggest_tags(lone, lone.nodes.front(), {}).empty());
    }
    {   // cold start: no tags yet, so frequency decides
        SceneNode fresh = node("fresh", "card", {});
        Scene s2 = scene;
        s2.nodes.push_back(fresh);
        auto s = suggest_tags(s2, fresh, {SuggestMode::Similar, 3});
        CHECK(!s.empty());
        CHECK(s.front() == "a1" || s.front() == "a2"); // the commonest tags
    }

    std::cout << "tags_smoke: " << (failures ? "FAILED" : "all ok") << "\n";
    return failures ? 1 : 0;
}
