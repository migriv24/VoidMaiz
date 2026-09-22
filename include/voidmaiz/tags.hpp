/* tags.hpp — suggesting tags, over the tags a scene already carries.
 *
 * Void Hormiga built this first (its `okf/concepts/allomone/tag-recommender.md`,
 * 2026-08-05) and the author asked for it here (2026-09-22):
 *
 *     "I also think tags in general, we should copy some stuff from hormiga.
 *      mainly the tag suggestion feature. I think that should be a maiz native
 *      thing (because multiple other void based applications should be able to
 *      do it)"
 *
 * He is right that it was never Hormiga's: the whole computation is a read of
 * `Scene::nodes`, which every Void Maiz host has. Nothing about contacts or
 * organizations appears below. So it moves here, unchanged in behaviour, and
 * Hormiga's copy becomes a caller.
 *
 * What it does: given a node, look at its SAME-GLYPH peers — the author's
 * framing, "if you're adding tags to a script or a contact or an event, what
 * other tags do events/scripts have?" — and rank the tags they carry that this
 * one does not. Similarity between two nodes is Jaccard overlap of their
 * meaningful tags.
 *
 * `Suggest::skip_prefixes` is what "meaningful" excludes: a `type:` tag is the
 * glyph again, and `icon:`/`color:` are render directives, so counting them
 * makes everything look alike. A host with its own directive prefixes says so
 * here rather than teaching this file its vocabulary.
 *
 * Three modes, which are three answers to how the graph should grow:
 *   Similar       reinforce clusters — things alike get tagged alike.
 *   Dissimilar    make this one distinct, pivoted at the mean similarity.
 *   Comprehensive no stranded nodes: prefer a tag that forges a NEW link, and
 *                 among those the least-connected peer's tags first.
 *
 * It is a pure function: no Core, no ImGui, no cache. A host that draws it every
 * frame should cache on (node · its tags · mode), as Hormiga does. */
#ifndef VOIDMAIZ_TAGS_HPP
#define VOIDMAIZ_TAGS_HPP

#include "voidmaiz/scene.hpp"

#include <string>
#include <vector>

namespace maiz {

enum class SuggestMode {
    Similar = 0,      // tags held by things like this one
    Dissimilar = 1,   // tags that set it apart
    Comprehensive = 2 // tags that connect it to what it is not connected to
};

struct Suggest {
    SuggestMode mode = SuggestMode::Similar;
    int limit = 6;
    /* Tag prefixes that carry no meaning for similarity and are never suggested.
     * The defaults are the ones every Void host has grown. */
    std::vector<std::string> skip_prefixes{"type:", "icon:", "color:"};
};

/* Ranked tags to offer for `target`, best first, at most `opt.limit`. Empty when
 * the scene holds no other node of that glyph — there is nothing to learn from,
 * and an invented suggestion is worse than none. */
std::vector<std::string> suggest_tags(const Scene& scene, const SceneNode& target,
                                      const Suggest& opt = {});

} // namespace maiz

#endif
