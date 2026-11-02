/* rancho.hpp — a maiz::Core as Void Rancho's CoreLink.
 *
 * Void Rancho (`../VoidRancho`, the family's database layer since 2026-10-30)
 * keeps a store in step with what a Core says after each command. It does not
 * own a Core: its WorkingSet takes a `rancho::CoreLink&`, the four calls it
 * needs (rancho/core.hpp). A Void Maiz host already owns a `maiz::Core`, with
 * its glyphs, log sink, effect handler and undo depth, and must keep using
 * THAT one — a second Core would be a second copy of the model, which is the
 * one thing this library exists not to have.
 *
 * So this is the adapter. Rancho's own message called it "ten lines" and left
 * each host to write it; it is ours, because both sides of it are ours to
 * know:
 *
 *   dispatch      → Core::dispatch, envelope copied field for field;
 *   export_state  → *Core::state_text(), so the export Rancho's full sync
 *                   takes is the one the next projection reads (one export per
 *                   change, embed.hpp) — not a second one;
 *   replace_state → Core::replace_state, which keeps glyphs, sinks, the effect
 *                   handler and the undo settings across a working-set reload;
 *   revision      → Core::revision, which does NOT move on reads. Rancho's
 *                   sync asks `cat`, `links` and bare `config`; all three are
 *                   reads by our list, so a sync never costs the host's cached
 *                   export or scene (tested: tests/rancho_link_smoke.cpp, when
 *                   Rancho is beside us).
 *
 * HEADER-ONLY, AND NO TARGET, ON PURPOSE. Void Maiz does not depend on Void
 * Rancho; a host that links both includes this file. That keeps the add order
 * in a superproject irrelevant. Usage:
 *
 *     maiz::Core core;                 // the host's, as always
 *     maiz::RanchoLink link(core);
 *     rancho::WorkingSet ws(store, link);
 *     ws.dispatch("tag maria +status:called");   // still the one door
 *     ws.sync();
 *
 * The link holds a reference: the Core must outlive it, and a Core that is
 * MOVED (Core is movable) leaves the link pointing at the moved-from shell. */
#pragma once

#include "rancho/core.hpp"
#include "voidmaiz/embed.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace maiz {

class RanchoLink final : public rancho::CoreLink {
public:
    explicit RanchoLink(Core& core) noexcept : core_(&core) {}

    rancho::Reply dispatch(std::string_view command) override {
        Result r = core_->dispatch(command);
        rancho::Reply out;
        out.ok = r.ok;
        out.text = r.text();
        out.data = std::move(r.data);
        return out;
    }

    std::string export_state() override {
        const auto text = core_->state_text();
        return text ? *text : std::string();
    }

    bool replace_state(std::string_view state_json) override { return core_->replace_state(state_json); }

    std::uint64_t revision() const override { return core_->revision(); }

    Core& core() noexcept { return *core_; }

private:
    Core* core_;
};

} // namespace maiz
