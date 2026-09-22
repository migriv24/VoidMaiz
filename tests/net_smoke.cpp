/* net_smoke.cpp — stage C: two (and three) Void Maiz applications, each a real
 * maiz::Core, syncing through Void Palabra over an IN-MEMORY transport.
 *
 * Real network transport is gated on the trust model (Palabra's open-questions
 * §6.1). Everything above the byte pipe is not, and this is where it is proved:
 * convergence, deletions that stay deleted, a private rune that never leaves in
 * any form, presence keyed on the identity the session established, a splice
 * that keeps the host's callbacks and glyphs, and — the one this module could
 * most plausibly get wrong — NO PHANTOM LOOP: once converged, idle ticks must
 * observe nothing, splice nothing, and trade nothing.
 */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/net.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/wires.hpp"
#include "voidpalabra/links.hpp"
#include "voidmaiz/project.hpp"

#include "voidpalabra/canonical.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

using namespace maiz;
using maiz::NetMillis;

/* The log sink's shape, without the view module (this test links no UI). */
struct Line {
    std::string level, op, msg;
};

static const char* kCard = R"({"glyph":"card","label":"Card","fields":["text","image"]})";

/* One device: a Core the way an application holds one, its log, its saved
 * replica bytes, its file store, and the Network over them. */
struct Device {
    std::string name;
    Core core;
    std::vector<Line> log;
    std::string saved;       // what `persist` last wrote
    int persists = 0;
    std::map<std::string, std::string> files;
    std::vector<std::string> selection; // rune ids
    CollabOut collab;                   // the canvas's in-flight half
    Surfaces surfaces;
    std::unique_ptr<Network> net;
    std::vector<NetNote> notes;

    /* Only the FOUNDER creates the shared mantle. A joiner receives it: Palabra
     * keys mantles by name, so two devices that each run `mantle new team`
     * mint two ids for one name and conflict on it forever (found by this test,
     * 2026-09-19 — `mantle=team field=id` on every merge). */
    Device(std::string n, const std::string& replica_id, bool cautious = false,
           bool founder = true)
        : name(std::move(n)) {
        core.set_log_sink([this](std::string_view l, std::string_view op, std::string_view m) {
            log.push_back({std::string(l), std::string(op), std::string(m)});
        });
        core.register_glyph(kCard);
        if (founder) {
            core.dispatch("mantle new team");
            core.dispatch("use team");
        }

        voidpalabra::Replica r;
        std::string why;
        if (!voidpalabra::Replica::create(replica_id, r, &why))
            std::cerr << "replica create failed: " << why << "\n";

        NetOptions o;
        o.persist = [this](const std::string& bytes) {
            saved = bytes;
            ++persists;
        };
        o.files.references.fields["content.image"] = voidpalabra::sha256_hex_anywhere();
        o.files.have = [this](const std::string& a) { return files.count(a) > 0; };
        o.files.read = [this](const std::string& a, std::string& out) {
            auto it = files.find(a);
            if (it == files.end()) return false;
            out = it->second;
            return true;
        };
        o.files.store = [this](const std::string& a, const std::string& b) { files[a] = b; };
        // fast timings: the test advances a simulated clock, not a real one
        o.timing.resend = 300;
        o.timing.keepalive = 2000;
        net = std::make_unique<Network>(core, std::move(r), std::move(o));
        net->settings().self.name = name;
        net->settings().cautious_files = cautious;
    }

    std::string state() const { return core.export_state(); }
    /* A joiner adopts the mantle it received (`active` is local, never synced). */
    void adopt() { core.dispatch("use team"); }
    /* By name, across every mantle — works before a joiner has adopted. */
    std::string id_of(const std::string& rune_name) const {
        std::string out;
        cJSON* root = cJSON_Parse(core.export_state().c_str());
        const cJSON* m = nullptr;
        cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(root, "mantles")) {
            const cJSON* r = nullptr;
            cJSON_ArrayForEach(r, cJSON_GetObjectItemCaseSensitive(m, "runes")) {
                const cJSON* sp = cJSON_GetObjectItemCaseSensitive(r, "spirit");
                const cJSON* nm = cJSON_GetObjectItemCaseSensitive(sp, "name");
                const cJSON* id = cJSON_GetObjectItemCaseSensitive(sp, "id");
                if (cJSON_IsString(nm) && rune_name == nm->valuestring && cJSON_IsString(id))
                    out = id->valuestring;
            }
        }
        cJSON_Delete(root);
        return out;
    }
    void tick(NetMillis now) {
        surfaces.begin_frame();
        surfaces.declare("canvas", "canvas");
        surfaces.focus("canvas");
        net->tick(now, selection, surfaces, collab);
        for (auto& n : net->take_notes()) notes.push_back(n);
    }
};

/* A deterministic lossy wire: drops a fraction of frames, never reorders (the
 * reorder/duplicate/partition schedule is Palabra's own test; this one checks the
 * module above it). */
struct Wire {
    std::uint32_t seed = 12345;
    double drop = 0.0;
    std::size_t delivered = 0, dropped = 0;
    bool lose() {
        seed = seed * 1664525u + 1013904223u;
        return (double)(seed >> 8) / (double)(1u << 24) < drop;
    }
};

/* Everyone ticks, then every outgoing frame is delivered to the device named by
 * its link. Links are named after the peer they reach. */
static void pump(std::vector<Device*> devs, NetMillis& now, int steps, Wire& wire) {
    for (int i = 0; i < steps; ++i) {
        now += 50;
        for (auto* d : devs) d->tick(now);
        for (auto* d : devs) {
            for (auto& out : d->net->take_outgoing()) {
                auto to = std::find_if(devs.begin(), devs.end(),
                                       [&](Device* x) { return x->name == out.link; });
                if (to == devs.end()) continue;
                if (wire.lose()) {
                    ++wire.dropped;
                    continue;
                }
                ++wire.delivered;
                (*to)->net->receive(d->name, out.frame, now);
                for (auto& n : (*to)->net->take_notes()) (*to)->notes.push_back(n);
            }
        }
    }
}

/* "The same document", canonically: Palabra's slice hash over the versioned
 * slice. Two devices can hold one document with different JSON key orders — the
 * first version of this test compared printed text and reported a divergence
 * that was only `{"image","text"}` against `{"text","image"}`. */
static std::string same_doc(const Core& core) {
    cJSON* root = cJSON_Parse(core.export_state().c_str());
    std::string h = voidpalabra::to_hex(voidpalabra::slice_hash(root));
    cJSON_Delete(root);
    return h;
}

/* A readable listing, for the failure dump only. */
static std::string fingerprint(const Core& core) {
    std::vector<std::string> rows;
    cJSON* root = cJSON_Parse(core.export_state().c_str());
    const cJSON* m = nullptr;
    cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(root, "mantles")) {
        const cJSON* r = nullptr;
        cJSON_ArrayForEach(r, cJSON_GetObjectItemCaseSensitive(m, "runes")) {
            std::string row;
            for (const char* k : {"spirit", "tags", "content"}) {
                char* t = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(r, k));
                row += t ? t : "";
                cJSON_free(t);
                row += "|";
            }
            rows.push_back(row);
        }
    }
    cJSON_Delete(root);
    std::sort(rows.begin(), rows.end());
    std::string out;
    for (auto& r : rows) out += r + "\n";
    return out;
}

static bool has_text(const std::string& hay, const std::string& needle) {
    return !needle.empty() && hay.find(needle) != std::string::npos;
}

static bool warned_round_trip(const Device& d) {
    for (const auto& n : d.notes)
        if (n.text.find("did not round-trip") != std::string::npos) return true;
    return false;
}

int main() {
    std::cout << "net_smoke\n";

    // ── two devices converge, and a deletion stays deleted ───────────────────
    {
        Device a("ana", "replica-ana-0000000001"), b("bo", "replica-bo-00000000002", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);

        a.core.dispatch("rune new card alpha");
        a.core.dispatch("set alpha text hello");
        pump({&a, &b}, now, 40, wire);
        b.adopt();

        std::string alpha = a.id_of("alpha");
        CHECK(!alpha.empty());
        CHECK(b.id_of("alpha") == alpha); // same rune, same immutable id
        CHECK(has_text(b.state(), "hello"));
        CHECK(a.persists > 0 && !a.saved.empty()); // persisted before sharing

        b.core.dispatch("set alpha text world");
        pump({&a, &b}, now, 40, wire);
        CHECK(has_text(a.state(), "world"));
        CHECK(same_doc(a.core) == same_doc(b.core));

        a.core.dispatch("rm alpha");
        pump({&a, &b}, now, 40, wire);
        CHECK(b.id_of("alpha").empty());
        pump({&a, &b}, now, 100, wire); // and it does not come back on later ticks
        CHECK(b.id_of("alpha").empty() && a.id_of("alpha").empty());

        // ── the splice kept what the host installed ──────────────────────────
        std::size_t lines = b.log.size();
        b.core.dispatch("rune new card beta");
        CHECK(b.log.size() > lines); // the log sink survived every splice
        Scene sb = project_scene(b.core);
        const SceneNode* beta = sb.find("beta");
        CHECK(beta && beta->label == "Card"); // the registered glyph survived too

        // ── NO PHANTOM LOOP: converged and idle means nothing moves ─────────
        pump({&a, &b}, now, 60, wire);
        CHECK(same_doc(a.core) == same_doc(b.core));
        NetStats sa = a.net->stats(), sbs = b.net->stats();
        std::size_t frames = wire.delivered;
        pump({&a, &b}, now, 200, wire); // 10 simulated seconds of nothing
        CHECK(a.net->stats().observed_changes == sa.observed_changes);
        CHECK(b.net->stats().observed_changes == sbs.observed_changes);
        CHECK(a.net->stats().splices == sa.splices);
        CHECK(b.net->stats().splices == sbs.splices);
        CHECK(!warned_round_trip(a) && !warned_round_trip(b));
        // idle traffic is presence refreshes and keepalives, never a stream of docs
        CHECK(wire.delivered - frames < 120);
        for (const auto& l : a.net->links()) CHECK(l.open && l.in_sync);
    }

    // ── an idle peer costs nobody their undo; a real merge must ──────────────
    {
        Device a("ana", "replica-ana-0000000011"), b("bo", "replica-bo-00000000012", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        pump({&a, &b}, now, 40, wire);
        b.adopt();

        b.core.dispatch("rune new card mine");
        pump({&a, &b}, now, 60, wire); // b's own edit round-trips through a
        CHECK(b.core.dispatch("history").text() != "(no history)"); // still undoable

        a.core.dispatch("rune new card theirs");
        pump({&a, &b}, now, 40, wire);
        CHECK(!b.id_of("theirs").empty());
        /* The documented cost, pinned so nobody is surprised by it: a merge that
         * changed the document starts the undo history over, because a memento
         * taken before it would revert the peer's work on undo. */
        CHECK(b.core.dispatch("history").text() == "(no history)");
    }

    // ── a private rune never leaves, in any form ─────────────────────────────
    {
        Device a("ana", "replica-ana-0000000021"), b("bo", "replica-bo-00000000022", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);

        a.core.dispatch("rune new card public-note");
        a.core.dispatch("rune new card diary");
        a.core.dispatch("tag diary +private");
        a.core.dispatch("set diary text do-not-send-this");
        a.core.dispatch("link public-note diary --relation mentions");
        std::string diary = a.id_of("diary");
        pump({&a, &b}, now, 60, wire);

        CHECK(!b.id_of("public-note").empty());
        std::string bs = b.state();
        CHECK(!has_text(bs, "do-not-send-this")); // not its content
        CHECK(!has_text(bs, diary));              // not its id
        CHECK(b.id_of("diary").empty());          // not its name, as a rune or a link end

        // presence never names it either — even when it is what `a` has selected
        a.selection = {diary, a.id_of("public-note")};
        pump({&a, &b}, now, 20, wire);
        const Peer* seen = b.net->roster().find(a.net->replica().id());
        CHECK(seen != nullptr);
        CHECK(seen && seen->state.selection == std::vector<std::string>{a.id_of("public-note")});
    }

    // ── two rewrites sharing a wire commute when the wire is a class ────────
    // collaborative-canvas §4.2 and Palabra's normative answer (SPEC §5.11): the
    // exact case that loses a wire under plain edges, run through the whole stack.
    {
        Device a("ana", "replica-ana-0000000061"), b("bo", "replica-bo-00000000062", false, false);
        const char* kGamma = R"({"glyph":"gamma","label":"gamma","fields":[]})";
        const char* kWire = R"({"glyph":"wire","label":"wire","fields":[]})";
        for (Device* d : {&a, &b}) {
            d->core.register_glyph(kGamma);
            d->core.register_glyph(kWire);
        }
        WireEncoding enc;
        NetMillis now = 1000;
        Wire wire;
        wire.drop = 0.3;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        for (const char* n : {"a", "b", "c", "d"}) a.core.dispatch(std::string("rune new gamma ") + n);
        a.core.dispatch(compile_wire(enc, "w-ab", {"a", 0}, {"b", 0})); // redex 1
        a.core.dispatch(compile_wire(enc, "w-cd", {"c", 0}, {"d", 0})); // redex 2
        a.core.dispatch(compile_wire(enc, "w-ac", {"a", 1}, {"c", 1})); // the SHARED wire
        pump({&a, &b}, now, 80, wire);
        b.adopt();
        CHECK(fingerprint(a.core) == fingerprint(b.core));

        // partition, then each device fires ONE of the two redexes
        a.net->disconnect("bo", now);
        b.net->disconnect("ana", now);
        for (const char* cmd : {"rm a", "rm b", "rune new gamma a2"}) a.core.dispatch(cmd);
        a.core.dispatch(compile_segment(enc, fresh_wire_name("ana", 1)));
        a.core.dispatch(compile_attach(fresh_wire_name("ana", 1), {"a2", 1}));
        a.core.dispatch(compile_fuse(enc, fresh_wire_name("ana", 1), "w-ac"));
        for (const char* cmd : {"rm c", "rm d", "rune new gamma c2"}) b.core.dispatch(cmd);
        b.core.dispatch(compile_segment(enc, fresh_wire_name("bo", 1)));
        b.core.dispatch(compile_attach(fresh_wire_name("bo", 1), {"c2", 1}));
        b.core.dispatch(compile_fuse(enc, fresh_wire_name("bo", 1), "w-ac"));

        // heal over a lossy wire
        pump({&a, &b}, now, 5, wire);
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        pump({&a, &b}, now, 200, wire);

        CHECK(fingerprint(a.core) == fingerprint(b.core)); // one document
        for (Device* d : {&a, &b}) {
            Scene drawn = collapse_wires(project_scene(d->core), enc);
            // the wire nobody wrote, read from what both wrote
            bool joined = false;
            for (const auto& w : drawn.wires)
                joined |= ((w.from == "a2" && w.to == "c2") || (w.from == "c2" && w.to == "a2")) &&
                          w.from_port == 1 && w.to_port == 1 && !w.contested;
            CHECK(joined);
            CHECK(d->net->anomalies().empty());
            CHECK(d->net->conflicts().empty());

            // Palabra's normative check over the same document
            voidpalabra::LinkRules ic;
            ic.equivalence = {enc.fuse};
            voidpalabra::Capacity port;
            port.name = "one wire per port";
            port.slot = voidpalabra::Slot::from_port;
            port.max = 1;
            voidpalabra::Capacity ends;
            ends.name = "a wire has two ends";
            ends.slot = voidpalabra::Slot::to;
            ends.max = 2;
            ends.through_equivalence = true;
            ic.capacity = {port, ends};
            cJSON* st = cJSON_Parse(d->core.export_state().c_str());
            CHECK(voidpalabra::check_links(st, ic).empty());
            cJSON_Delete(st);
        }
    }

    // ── the author's report (2026-09-22): wires arrive late, or not at all ───
    // "on one device i can be connecting ports together, and the other device
    // will just show EMPTY NODES ... this might be because i was wiring things
    // with a vicious cycle." Positions synced; the wiring did not. Every wiring
    // shape the author used, compared as the two devices DRAW it.
    {
        Device a("ana", "replica-ana-0000000081"), b("bo", "replica-bo-00000000082", false, false);
        const char* kGamma = R"({"glyph":"gamma","label":"gamma","fields":[],"hints":{"ports":[{"name":"prin","principal":true},{"name":"a"},{"name":"b"}]}})";
        const char* kWire = R"({"glyph":"wire","label":"wire","fields":[]})";
        for (Device* d : {&a, &b}) {
            d->core.register_glyph(kGamma);
            d->core.register_glyph(kWire);
        }
        WireEncoding enc;
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        unsigned long k = 1;
        auto fresh = [&] { return fresh_wire_name("ana", k++); };

        for (const char* n : {"c1", "c2", "c3"}) a.core.dispatch(std::string("rune new gamma ") + n);
        // 1. an ordinary wire
        a.core.dispatch(compile_wire(enc, fresh(), {"c1", 1}, {"c2", 1}));
        // 2. a SELF-LOOP: a constructor wired to itself (the author's case)
        a.core.dispatch(compile_wire(enc, fresh(), {"c3", 1}, {"c3", 2}));
        // 3. a vicious circle: principal -> aux around a ring
        a.core.dispatch(compile_wire(enc, fresh(), {"c1", 0}, {"c2", 2}));
        a.core.dispatch(compile_wire(enc, fresh(), {"c2", 0}, {"c3", 0}));
        pump({&a, &b}, now, 80, wire);
        b.adopt();

        auto drawn = [&](Device& d) {
            Scene s = collapse_wires(project_scene(d.core), enc);
            std::vector<std::string> w;
            for (const auto& x : s.wires) {
                std::string p = x.from + "." + std::to_string(x.from_port);
                std::string q = x.to + "." + std::to_string(x.to_port);
                if (q < p) std::swap(p, q);
                w.push_back(p + "-" + q + (x.contested ? "!" : ""));
            }
            std::sort(w.begin(), w.end());
            return w;
        };
        std::vector<std::string> host = drawn(a), joiner = drawn(b);
        CHECK(host.size() == 4);
        CHECK(joiner == host); // the whole point: the same net on both screens
        if (joiner != host) {
            std::cerr << "  host  :";
            for (auto& x : host) std::cerr << " " << x;
            std::cerr << "\n  joiner:";
            for (auto& x : joiner) std::cerr << " " << x;
            std::cerr << "\n";
        }
        CHECK(b.net->anomalies().empty());
        CHECK(fingerprint(a.core) == fingerprint(b.core));

        // and wiring made AFTER the join arrives without another edit to nudge it
        a.core.dispatch(compile_wire(enc, fresh(), {"c1", 2}, {"c3", 0}));
        pump({&a, &b}, now, 60, wire);
        CHECK(drawn(b).size() == drawn(a).size());
        CHECK(drawn(b) == drawn(a));

        // the joiner wires something, and the host sees it
        b.core.dispatch(compile_wire(enc, fresh_wire_name("bo", 1), {"c2", 1}, {"c3", 1}));
        pump({&a, &b}, now, 60, wire);
        CHECK(drawn(a) == drawn(b));
    }

    // ── a rule of the mantle travels (live physics is one) ──────────────────
    // The author: "I think 'live physics' is a rule or state of the mantle, and
    // therefore, if live physics is turned on, it should be turned on for all
    // synced devices. rather than a constant update of position information."
    // A mantle HAS `rules` in the Core, so the question is only whether one
    // crosses. It does, and it comes back off the same way.
    {
        Device a("ana", "replica-ana-0000000095"), b("bo", "replica-bo-00000000096", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        a.core.dispatch("card new anchor");
        pump({&a, &b}, now, 60, wire);
        b.adopt();

        auto physics_of = [](Device& d) {
            std::string out;
            cJSON* root = cJSON_Parse(d.core.export_state().c_str());
            const cJSON* m = nullptr;
            cJSON_ArrayForEach(m, cJSON_GetObjectItemCaseSensitive(root, "mantles")) {
                const cJSON* r = nullptr;
                cJSON_ArrayForEach(r, cJSON_GetObjectItemCaseSensitive(m, "rules")) {
                    const cJSON* k = cJSON_GetObjectItemCaseSensitive(r, "rule");
                    const cJSON* v = cJSON_GetObjectItemCaseSensitive(r, "driver");
                    if (cJSON_IsString(k) && std::string(k->valuestring) == "physics")
                        out = cJSON_IsString(v) ? v->valuestring : "?";
                }
            }
            cJSON_Delete(root);
            return out;
        };

        CHECK(physics_of(a).empty() && physics_of(b).empty());
        a.core.dispatch("rule add " + arg(R"({"rule":"physics","driver":"ana"})"));
        pump({&a, &b}, now, 80, wire);
        CHECK(physics_of(a) == "ana");
        CHECK(physics_of(b) == "ana"); // the other screen is now running it too
        a.core.dispatch("rule rm 0");
        pump({&a, &b}, now, 80, wire);
        CHECK(physics_of(a).empty());
        CHECK(physics_of(b).empty()); // and off is just as shared as on
    }

    // ── the author's second report: a node made on EACH device, and a wire gone ─
    // "drag a port, and from that create a node with a connection, that doesn't
    // sync" (2026-09-22). Both devices minted `gamma-1`, so one name meant two
    // nodes and every wire naming it became ambiguous.
    {
        Device a("ana", "replica-ana-0000000091"), b("bo", "replica-bo-00000000092", false, false);
        const char* kG = R"({"glyph":"gamma","label":"gamma","fields":[],"hints":{"ports":[{"name":"prin","principal":true},{"name":"a"},{"name":"b"}]}})";
        const char* kW = R"({"glyph":"wire","label":"wire","fields":[]})";
        for (Device* d : {&a, &b}) {
            d->core.register_glyph(kG);
            d->core.register_glyph(kW);
        }
        WireEncoding enc;
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        a.core.dispatch("rune new gamma anchor");
        pump({&a, &b}, now, 60, wire);
        b.adopt();

        auto add_and_link = [&](Device& d, const std::string& tag, const char* to_node, int to_port) {
            Scene s = collapse_wires(project_scene(d.core), enc);
            std::string name = unique_name(s, "gamma", tag); // the canvas's own minter
            d.core.dispatch(compile_batch({"rune new gamma " + name, compile_move(name, 10, 10)}));
            d.core.dispatch(compile_wire(enc, fresh_wire_name(tag.empty() ? "x" : tag, 7 + tag.size()),
                                         {name, 0}, {to_node, to_port}));
            return name;
        };
        auto wires_of = [&](Device& d) {
            Scene s = collapse_wires(project_scene(d.core), enc);
            return s.wires.size();
        };

        // WITHOUT a device tag: both devices mint the same name
        std::string na = add_and_link(a, "", "anchor", 1);
        std::string nb = add_and_link(b, "", "anchor", 2);
        CHECK(na == nb); // "gamma-1" on both: the bug, in one line
        pump({&a, &b}, now, 80, wire);
        bool duplicate = false;
        for (const auto& an : a.net->anomalies())
            if (an.kind == "duplicate_name") duplicate = true;
        CHECK(duplicate);                        // Palabra sees it
        CHECK(wires_of(a) != 3 || wires_of(b) != 3); // and a wire is missing somewhere

        // WITH one: the same gesture on both devices, and both screens agree
        Device c("cy", "replica-cy-0000000093"), d("di", "replica-di-00000000094", false, false);
        for (Device* x : {&c, &d}) {
            x->core.register_glyph(kG);
            x->core.register_glyph(kW);
        }
        NetMillis now2 = 1000;
        Wire wire2;
        c.net->connect("di", now2);
        d.net->connect("cy", now2);
        c.core.dispatch("rune new gamma anchor");
        pump({&c, &d}, now2, 60, wire2);
        d.adopt();
        auto add2 = [&](Device& dev, const std::string& tag, const char* to_node, int to_port) {
            Scene s = collapse_wires(project_scene(dev.core), enc);
            std::string name = unique_name(s, "gamma", tag);
            dev.core.dispatch(compile_batch({"rune new gamma " + name, compile_move(name, 10, 10)}));
            dev.core.dispatch(compile_wire(enc, fresh_wire_name(tag, 1), {name, 0}, {to_node, to_port}));
            return name;
        };
        std::string nc = add2(c, "cy-", "anchor", 1);
        std::string nd = add2(d, "di-", "anchor", 2);
        CHECK(nc != nd);
        pump({&c, &d}, now2, 80, wire2);
        CHECK(c.net->anomalies().empty() && d.net->anomalies().empty());
        CHECK(wires_of(c) == 2 && wires_of(d) == 2); // both wires, on both screens
        CHECK(fingerprint(c.core) == fingerprint(d.core));
    }

    // ── …and the same case with PLAIN edges loses the wire (the contrast) ────
    // Kept so the test above cannot pass vacuously: this is the failure it fixes.
    {
        Device a("ana", "replica-ana-0000000071"), b("bo", "replica-bo-00000000072", false, false);
        for (Device* d : {&a, &b}) d->core.register_glyph(R"({"glyph":"gamma","label":"gamma","fields":[]})");
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        for (const char* n : {"a", "b", "c", "d"}) a.core.dispatch(std::string("rune new gamma ") + n);
        a.core.dispatch("link a b --relation 0:0");
        a.core.dispatch("link c d --relation 0:0");
        a.core.dispatch("link a c --relation 1:1");
        pump({&a, &b}, now, 60, wire);
        b.adopt();
        a.net->disconnect("bo", now);
        b.net->disconnect("ana", now);
        for (const char* cmd : {"rm a", "rm b", "rune new gamma a2", "link a2 c --relation 1:1"})
            a.core.dispatch(cmd);
        for (const char* cmd : {"rm c", "rm d", "rune new gamma c2", "link a c2 --relation 1:1"})
            b.core.dispatch(cmd);
        pump({&a, &b}, now, 5, wire);
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        pump({&a, &b}, now, 80, wire);

        CHECK(fingerprint(a.core) == fingerprint(b.core)); // it converges…
        Scene s = project_scene(a.core);
        bool joined = false;
        for (const auto& w : s.wires)
            joined |= (w.from == "a2" && w.to == "c2") || (w.from == "c2" && w.to == "a2");
        CHECK(!joined);                          // …on a net missing the wire
        CHECK(!a.net->anomalies().empty());      // and says so: link_broken
    }

    // ── two people drag one node: they converge, nobody is asked ────────────
    {
        Device a("ana", "replica-ana-0000000051"), b("bo", "replica-bo-00000000052", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        a.core.dispatch("rune new card node");
        a.core.dispatch("set node text hello");
        pump({&a, &b}, now, 40, wire);
        b.adopt();

        // partition: each moves the node, and each edits the TEXT differently
        a.net->disconnect("bo", now);
        b.net->disconnect("ana", now);
        a.core.dispatch("setjson node pos [10,20]");
        b.core.dispatch("setjson node pos [300,400]");
        a.core.dispatch("set node text ana-wrote-this");
        b.core.dispatch("set node text bo-wrote-this");
        pump({&a, &b}, now, 5, wire);
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        pump({&a, &b}, now, 60, wire);

        CHECK(fingerprint(a.core) == fingerprint(b.core)); // one document
        // the position is view state: picked, identically on both, no question
        // the text is content: still a question, on both
        auto rows = a.net->conflicts();
        bool pos_q = false, text_q = false;
        for (const auto& r : rows) {
            pos_q |= r.field == "content.pos";
            text_q |= r.field == "content.text";
        }
        CHECK(!pos_q);
        CHECK(text_q);
        CHECK(b.net->conflicts().size() == rows.size());
    }

    // ── the collaborative canvas: gestures in flight cross a real session ────
    {
        Device a("ana", "replica-ana-0000000041"), b("bo", "replica-bo-00000000042", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        a.core.dispatch("rune new card gam1");
        a.core.dispatch("rune new card secret");
        a.core.dispatch("tag secret +private");
        pump({&a, &b}, now, 40, wire);
        b.adopt();
        std::string gam1 = a.id_of("gam1"), secret = a.id_of("secret");

        // ana drags a wire out of gam1's principal and has claimed that port;
        // she also has a claim on the private rune and on the mantle (the crank)
        CanvasPresence c;
        c.surface = "canvas";
        c.has_cursor = true;
        c.cursor_x = 300;
        c.cursor_y = 140;
        c.gesture = CanvasGesture::Wire;
        c.wire_rune = gam1;
        c.wire_port = 0;
        a.collab.canvas = {c};
        a.collab.claims = {{gam1, "port:0", 4}, {secret, "", 5}, {"team", "crank", 6}};
        a.collab.clock = 6;
        pump({&a, &b}, now, 20, wire);

        const Peer* seen = b.net->roster().find(a.net->replica().id());
        CHECK(seen != nullptr);
        if (seen) {
            CHECK(seen->state.clock == 6);
            CHECK(seen->state.canvas.size() == 1);
            CHECK(!seen->state.canvas.empty() && seen->state.canvas[0].gesture == CanvasGesture::Wire &&
                  seen->state.canvas[0].wire_rune == gam1 && seen->state.canvas[0].cursor_x == 300);
            // the port claim and the crank arrive; the private rune's claim does not
            bool port = false, crank = false, leaked = false;
            for (const auto& cl : seen->state.claims) {
                port |= cl.rune == gam1 && cl.part == "port:0";
                crank |= cl.rune == "team" && cl.part == "crank";
                leaked |= cl.rune == secret;
            }
            CHECK(port && crank && !leaked);
        }

        // now the wire names the private rune: the gesture is withheld, the cursor is not
        a.collab.canvas[0].wire_rune = secret;
        pump({&a, &b}, now, 20, wire);
        seen = b.net->roster().find(a.net->replica().id());
        CHECK(seen && !seen->state.canvas.empty() &&
              seen->state.canvas[0].gesture == CanvasGesture::None &&
              seen->state.canvas[0].has_cursor);
    }

    // ── presence is keyed on the session's identity, not the payload's ───────
    {
        Device a("ana", "replica-ana-0000000031"), b("bo", "replica-bo-00000000032", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        a.core.dispatch("rune new card shared");
        pump({&a, &b}, now, 40, wire);

        // `a` claims to be `b` in its presence payload
        a.net->settings().self.id = b.net->replica().id();
        a.net->settings().self.name = "definitely bo";
        a.selection = {a.id_of("shared")};
        pump({&a, &b}, now, 20, wire);

        CHECK(b.net->roster().peers().size() == 1);
        const Peer* p = b.net->roster().find(a.net->replica().id());
        CHECK(p != nullptr); // filed under who the session says it is
        CHECK(b.net->roster().on_rune(a.id_of("shared")).size() == 1);
        CHECK(b.net->stats().presence_in > 0);

        // the sender's switch: stop sharing selection, and it stops arriving
        a.net->settings().send.selection = false;
        pump({&a, &b}, now, 20, wire);
        CHECK(b.net->roster().on_rune(a.id_of("shared")).empty());

        // leaving removes them
        a.net->disconnect("bo", now);
        pump({&a, &b}, now, 10, wire);
        CHECK(b.net->roster().peers().empty());
    }

    // ── cautious file transfer: known, not fetched, until asked ──────────────
    {
        Device a("ana", "replica-ana-0000000041"), b("bo", "replica-bo-00000000042",
                                                     /*cautious=*/true, /*founder=*/false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);

        std::string bytes = "\x89PNG pretend picture";
        std::string addr = voidpalabra::to_hex(voidpalabra::sha256(bytes));
        a.files[addr] = bytes;
        a.core.dispatch("rune new card photo");
        a.core.dispatch("set photo image " + addr);
        pump({&a, &b}, now, 60, wire);

        CHECK(!b.id_of("photo").empty()); // the rune arrived…
        CHECK(b.files.count(addr) == 0);  // …the file did not
        CHECK(b.net->content_state(addr) == voidpalabra::sync::ContentState::deferred);

        b.net->fetch(addr, now); // the person chose to download it
        pump({&a, &b}, now, 40, wire);
        CHECK(b.files.count(addr) == 1 && b.files[addr] == bytes);
        CHECK(b.net->content_state(addr) == voidpalabra::sync::ContentState::held);
    }

    // ── a conflict is a question, and resolving it is an ordinary change ─────
    {
        Device a("ana", "replica-ana-0000000061"),
            b("bo", "replica-bo-00000000062", false, false);
        NetMillis now = 1000;
        Wire wire;
        a.net->connect("bo", now);
        b.net->connect("ana", now);
        a.core.dispatch("rune new card plan");
        a.core.dispatch("set plan text original");
        pump({&a, &b}, now, 60, wire);
        b.adopt();
        CHECK(a.net->conflicts().empty() && b.net->conflicts().empty());

        // partition: both devices keep working, nothing crosses
        Wire cut;
        cut.drop = 1.0;
        a.core.dispatch("set plan text ana-version");
        b.core.dispatch("set plan text bo-version");
        pump({&a, &b}, now, 40, cut);
        CHECK(cut.dropped > 0 && cut.delivered == 0);

        // heal
        pump({&a, &b}, now, 80, wire);
        std::vector<ConflictRow> rows = a.net->conflicts();
        CHECK(rows.size() == 1);
        CHECK(!rows.empty() && rows[0].kind == "values");
        CHECK(!rows.empty() && rows[0].field == "content.text");
        CHECK(!rows.empty() && rows[0].rune_name == "plan"); // the name a person knows
        CHECK(!rows.empty() && rows[0].sides.size() == 2);
        // both devices see the SAME question, with the same id (its content address)
        std::vector<ConflictRow> theirs = b.net->conflicts();
        CHECK(theirs.size() == 1);
        CHECK(!theirs.empty() && !rows.empty() && theirs[0].id == rows[0].id);
        CHECK(a.net->anomalies().empty());

        // answer it on one device
        std::size_t pick = rows[0].sides[0].find("ana") != std::string::npos ? 0 : 1;
        CHECK(a.net->resolve(rows[0].id, pick, now));
        CHECK(a.net->conflicts().empty());
        CHECK(has_text(a.state(), "ana-version"));
        CHECK(!a.net->resolve(rows[0].id, pick, now)); // gone now: stale, refused

        // …and the other device sees it ANSWERED, not re-asked
        pump({&a, &b}, now, 80, wire);
        CHECK(b.net->conflicts().empty());
        CHECK(has_text(b.state(), "ana-version"));
        CHECK(same_doc(a.core) == same_doc(b.core));
    }

    // ── three devices, a lossy wire, concurrent edits: one document ──────────
    {
        Device a("ana", "replica-ana-0000000051"), b("bo", "replica-bo-00000000052", false, false),
            c("cy", "replica-cy-00000000053", false, false);
        NetMillis now = 1000;
        Wire wire;
        wire.drop = 0.30;
        for (auto* x : {&a, &b, &c})
            for (auto* y : {&a, &b, &c})
                if (x != y) x->net->connect(y->name, now);

        pump({&a, &b, &c}, now, 200, wire); // the joiners receive the mantle
        b.adopt();
        c.adopt();
        CHECK(a.net->replica().conflicts().empty());

        a.core.dispatch("rune new card from-a");
        b.core.dispatch("rune new card from-b");
        c.core.dispatch("rune new card from-c");
        pump({&a, &b, &c}, now, 400, wire);
        a.core.dispatch("set from-b text edited-by-a");
        c.core.dispatch("rm from-a");
        pump({&a, &b, &c}, now, 600, wire);

        CHECK(wire.dropped > 0);
        if (same_doc(a.core) != same_doc(b.core) || same_doc(b.core) != same_doc(c.core)) {
            for (auto* d : {&a, &b, &c}) {
                std::cerr << "--- " << d->name << " fingerprint:\n" << fingerprint(d->core);
                for (const auto& l : d->net->links())
                    std::cerr << "  link " << l.link << " open=" << l.open << " closed=" << l.closed
                              << " in_sync=" << l.in_sync << "\n";
                for (const auto& cf : d->net->replica().conflicts())
                    std::cerr << "  conflict mantle=" << cf.mantle << " rune=" << cf.rune
                              << " field=" << cf.field << "\n";
            }
        }
        CHECK(same_doc(a.core) == same_doc(b.core));
        CHECK(same_doc(b.core) == same_doc(c.core));
        CHECK(a.id_of("from-a").empty());
        CHECK(has_text(c.state(), "edited-by-a"));
        CHECK(a.net->replica().conflicts().empty()); // nothing standing once converged
        CHECK(!warned_round_trip(a) && !warned_round_trip(b) && !warned_round_trip(c));
    }

    if (failures) {
        std::cerr << "net_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "net_smoke: all ok\n";
    return 0;
}
