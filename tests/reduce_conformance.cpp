/* reduce_conformance.cpp — the C++20 port of conformance/reduce/run.py.
 * Runs the pinned pure-JSON cases against maiz::reduce and compares portable
 * canonical forms (never ids). Usage: maiz_reduce_conformance <cases-dir> */
#include "voidmaiz/reduce.hpp"

#include "../src/reduce/sha256.hpp"

#include "cJSON.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;
using namespace maiz::reduce;

namespace {

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string subtree(const cJSON* doc, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), key);
    if (!v) return {};
    char* printed = cJSON_PrintUnformatted(const_cast<cJSON*>(v));
    std::string out = printed ? printed : "";
    cJSON_free(printed);
    return out;
}

/* A `minter` case (16-minter.json): key->id vectors with no reduction around
 * them. Void Core added it because case 15 pins the minter through six other
 * layers, so a wrong digest, a wrong ordinal and a wrong pair ordering all look
 * the same there. Run through maiz::reduce::derived_id — the SAME function the
 * rewriter mints with, never a rebuilt key, or the vectors would be pinning a
 * copy of the rule rather than the rule. */
std::string execute_minter(const cJSON* doc) {
    std::string out = R"({"ids":[)";
    const cJSON* v = nullptr;
    bool first = true;
    cJSON_ArrayForEach(v, cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "minter")) {
        const cJSON* g = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(v), "glyphs");
        const cJSON* p = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(v), "parents");
        const cJSON* o = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(v), "ordinal");
        if (!cJSON_IsArray(const_cast<cJSON*>(g)) || cJSON_GetArraySize(const_cast<cJSON*>(g)) != 2 ||
            !cJSON_IsArray(const_cast<cJSON*>(p)) || cJSON_GetArraySize(const_cast<cJSON*>(p)) != 2)
            return R"({"error":"minter vector must carry two glyphs and two parents"})";
        auto str = [](const cJSON* arr, int i) {
            const cJSON* e = cJSON_GetArrayItem(const_cast<cJSON*>(arr), i);
            return cJSON_IsString(const_cast<cJSON*>(e)) ? std::string(e->valuestring)
                                                         : std::string();
        };
        const std::string id = maiz::reduce::derived_id(
            str(g, 0), str(g, 1), str(p, 0), str(p, 1),
            cJSON_IsNumber(const_cast<cJSON*>(o)) ? (long)o->valuedouble : 1L);
        if (!first) out += ',';
        first = false;
        out += '"' + id + '"';
    }
    return out + "]}";
}

/* Run one case; return the outcome as canonical JSON text:
 * {"canonical":…} or {"error":"<kind>"} (README §5). */
std::string execute(const cJSON* doc) {
    if (cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "minter"))
        return execute_minter(doc);
    try {
        Spec spec = spec_from_json(subtree(doc, "spec"));
        Net net = to_net(subtree(doc, "input"), spec.signatures);

        std::set<std::string> opaque;
        if (const cJSON* op = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "opaque")) {
            const cJSON* s = nullptr;
            cJSON_ArrayForEach(s, op)
                if (cJSON_IsString(const_cast<cJSON*>(s))) opaque.insert(s->valuestring);
        }
        int max_steps = 100000;
        if (const cJSON* ms = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "max_steps");
            cJSON_IsNumber(const_cast<cJSON*>(ms)))
            max_steps = (int)ms->valuedouble;
        int schedules = 0;
        if (const cJSON* sc = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "schedules");
            cJSON_IsNumber(const_cast<cJSON*>(sc)))
            schedules = (int)sc->valuedouble;
        /* `pin_ids` (README §6): also compare the LITERAL agent names, and —
         * with `schedules` — require every randomized order to produce the same
         * ones. Most cases are id-blind on purpose; this opts into the stronger
         * check where reproducible identity is the point. */
        bool pin_ids = false;
        if (const cJSON* pi = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "pin_ids"))
            pin_ids = cJSON_IsTrue(const_cast<cJSON*>(pi));

        // 0.2.4 contract: the default resolves internal redex wires; the
        // restricted subset is opt-in per case via the "strict_locality" key
        // (case 09 carries it) — forward it, like run.py does.
        bool strict = false;
        if (const cJSON* sl = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON*>(doc), "strict_locality"))
            strict = cJSON_IsTrue(const_cast<cJSON*>(sl));
        /* Agent names, sorted, as a JSON array. `Net::agents` is a std::map, so
         * iteration is already sorted by id. */
        auto ids_text = [](const Net& n) {
            std::string out = "[";
            bool first = true;
            for (const auto& [id, ag] : n.agents) {
                (void)ag;
                if (!first) out += ',';
                first = false;
                out += '"' + id + '"';
            }
            return out + "]";
        };

        Net done = reduce(spec, net, max_steps, opaque, {}, strict);
        std::string result = canonical(done);
        std::string ids = ids_text(done);

        /* confluence law: N randomized schedules must reach the same form
         * (the contract requires convergence, not a specific RNG) */
        for (int seed = 0; seed < schedules; ++seed) {
            std::mt19937 rng((unsigned)seed);
            auto pick = [&rng](size_t n) { return (size_t)(rng() % n); };
            Net alt_net = reduce(spec, net, max_steps, opaque, pick, strict);
            std::string alt = canonical(alt_net);
            if (alt != result)
                return "{\"error\":\"NOT CONFLUENT (schedule seed " +
                       std::to_string(seed) + " diverged)\"}";
            /* The property that must hold in EVERY implementation, whatever
             * hash it mints with: two schedules agree on which agents are the
             * same agent, not merely on the shape. */
            if (pin_ids && ids_text(alt_net) != ids)
                return "{\"error\":\"IDS NOT SCHEDULE-INDEPENDENT (seed " +
                       std::to_string(seed) + " named agents differently)\"}";
        }
        if (pin_ids) return "{\"canonical\":" + result + ",\"ids\":" + ids + "}";
        return "{\"canonical\":" + result + "}";
    } catch (const NetError&) {
        return "{\"error\":\"adapter-ports\"}";
    } catch (const ReduceError& e) {
        switch (e.kind) {
        case ReduceError::Kind::Termination: return "{\"error\":\"termination-guard\"}";
        case ReduceError::Kind::Locality: return "{\"error\":\"locality\"}";
        default: return std::string("{\"error\":\"reduce-error: ") + e.what() + "\"}";
        }
    } catch (const std::exception& e) {
        return std::string("{\"error\":\"spec-error: ") + e.what() + "\"}";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: maiz_reduce_conformance <cases-dir>\n";
        return 2;
    }
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(argv[1]))
        if (entry.path().extension() == ".json") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::cerr << "no cases found in " << argv[1] << "\n";
        return 2;
    }

    int failed = 0;
    for (const auto& f : files) {
        std::string text = slurp(f);
        cJSON* doc = cJSON_ParseWithLength(text.data(), text.size());
        if (!doc) {
            std::cout << "[FAIL] " << f.filename().string() << "  (unparseable)\n";
            ++failed;
            continue;
        }
        std::string got = execute(doc);
        std::string expected = canon_text(subtree(doc, "expect"));
        bool ok = got == expected;
        std::cout << "[" << (ok ? "ok " : "FAIL") << "] " << f.filename().string() << "\n";
        if (!ok) {
            ++failed;
            /* Show the FIRST DIVERGENCE, not the first 200 bytes. Case 15's
             * mismatch lives in `ids`, which the canonical form pushes past
             * byte 200 — so the old window printed two identical-looking lines
             * and the failure read as inexplicable. A diff you cannot read is
             * not a diff (2026-08-18). */
            size_t d = 0;
            while (d < expected.size() && d < got.size() && expected[d] == got[d]) ++d;
            const size_t from = d > 60 ? d - 60 : 0;
            std::cout << "        first difference at byte " << d << ":\n"
                      << "        expected: …" << expected.substr(from, 240) << "\n"
                      << "        got:      …" << got.substr(from, 240) << "\n";
        }
        cJSON_Delete(doc);
    }
    // ── engine tests beyond the pinned cases ────────────────────────────────
    // fuse is OUR extension (upstream 0.2.4: noted, not adopted — no second
    // consumer yet); the swap/internal-wire tests below pin step()-level shapes
    // the contract's reduce()-only cases can't (cases 11–14 pin the same laws
    // at normal-form level). a~b → single `into` agent adopting both boundaries
    // in order
    int fuse_failed = 0;
    auto fuse_check = [&](bool cond, const char* what) {
        if (!cond) {
            ++fuse_failed;
            std::cout << "[FAIL] fuse: " << what << "\n";
        }
    };

    /* SHA-256 against FIPS 180-4's own vectors. Case 15 proves the digest
     * end-to-end, but through six other layers — if the hash were subtly wrong,
     * case 15 would fail and point at the reducer. Case 16 proves the KEY. This
     * pins the primitive itself, so all three failures name different things.
     * (Was BLAKE2b/RFC 7693 until Void Core 0.2.10 made the digest normative
     * and picked SHA-256 — see src/reduce/sha256.hpp for why.) */
    fuse_check(maiz::reduce::detail::sha256_hex("abc", 32) ==
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f200"
                   "15ad",
               "sha256(\"abc\") matches FIPS 180-4");
    fuse_check(maiz::reduce::detail::sha256_hex("", 32) ==
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852"
                   "b855",
               "sha256(\"\") matches FIPS 180-4");
    /* The two-block vector, so a padding bug that only shows past 55 bytes
     * cannot hide: a minter key with long glyph names crosses that boundary. */
    fuse_check(maiz::reduce::detail::sha256_hex(
                   "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 32) ==
                   "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db"
                   "06c1",
               "sha256(two-block vector) matches FIPS 180-4");
    try {
        // arity-0 fusion (the tag-pigment shape): red~yellow → orange, floating
        Spec spec = spec_from_json(
            R"({"signatures":{"red":0,"yellow":0,"orange":0},)"
            R"("rules":[{"glyphs":["red","yellow"],"rule":"fuse","into":"orange"}]})");
        Net net;
        net.add({"a", "red", 0, "{}", {}});
        net.add({"b", "yellow", 0, "{}", {}});
        net.connect({"a", 0}, {"b", 0});
        Net done = reduce(spec, net);
        fuse_check(done.agents.size() == 1, "arity-0 fusion leaves one agent");
        fuse_check(done.agents.begin()->second.glyph == "orange", "red~yellow fuses to orange");

        // boundary adoption: fuse of arity-1 agents rewires both aux partners
        Spec spec2 = spec_from_json(
            R"({"signatures":{"p":1,"q":1,"pq":2,"x":1},)"
            R"("rules":[{"glyphs":["p","q"],"rule":"fuse","into":"pq"}]})");
        Net net2;
        net2.add({"p1", "p", 1, "{}", {}});
        net2.add({"q1", "q", 1, "{}", {}});
        net2.add({"x1", "x", 1, "{}", {}});
        net2.add({"x2", "x", 1, "{}", {}});
        net2.connect({"p1", 0}, {"q1", 0});
        net2.connect({"p1", 1}, {"x1", 1});
        net2.connect({"q1", 1}, {"x2", 1});
        Net done2 = reduce(spec2, net2);
        fuse_check(done2.agents.size() == 3, "arity-1 fusion: pq + both x survive");
        const Agent* fused = nullptr;
        for (const auto& [id, ag] : done2.agents)
            if (ag.glyph == "pq") fused = &ag;
        fuse_check(fused != nullptr, "the pq agent exists");
        if (fused) {
            const Port* p1 = done2.partner({fused->id, 1});
            const Port* p2 = done2.partner({fused->id, 2});
            fuse_check(p1 && p1->first == "x1", "pq aux 1 adopted p's boundary");
            fuse_check(p2 && p2->first == "x2", "pq aux 2 adopted q's boundary");
        }

        // spec guards: fuse without `into` rejects; unknown kinds still reject
        try {
            spec_from_json(R"({"signatures":{},"rules":[{"glyphs":["a","b"],"rule":"fuse"}]})");
            fuse_check(false, "fuse without into must throw");
        } catch (const std::invalid_argument&) {}
        try {
            spec_from_json(R"({"signatures":{},"rules":[{"glyphs":["a","b"],"rule":"zap"}]})");
            fuse_check(false, "unknown rule kind must throw");
        } catch (const std::invalid_argument&) {}

        // ── swap annihilation (Lafont's γγ: x_i ≡ y_{n+1-i}) ────────────────
        {
            Spec sw = spec_from_json(
                R"({"signatures":{"g":2,"leaf":0},)"
                R"("rules":[{"glyphs":["g","g"],"rule":"annihilate","swap":true}]})");
            Net net2;
            net2.add({"x", "g", 2, "{}", {}});
            net2.add({"y", "g", 2, "{}", {}});
            for (const char* l : {"P", "Q", "R", "S"}) net2.add({l, "leaf", 0, "{}", {}});
            net2.connect({"x", 0}, {"y", 0});
            net2.connect({"x", 1}, {"P", 0});
            net2.connect({"x", 2}, {"Q", 0});
            net2.connect({"y", 1}, {"R", 0});
            net2.connect({"y", 2}, {"S", 0});
            Net done2 = step(sw, net2, {"x", "y"});
            const Port* p = done2.partner({"P", 0});
            const Port* q = done2.partner({"Q", 0});
            fuse_check(p && p->first == "S", "swap annihilate: x1 meets y2 (P-S)");
            fuse_check(q && q->first == "R", "swap annihilate: x2 meets y1 (Q-R)");
        }

        // ── internal redex wires resolve (contract default since 0.2.4) ─────
        {
            // the author's crash case: γ~δ principals + γ.aux2 wired δ.aux1.
            // This CROSS-AGENT flavor is legal but divergent under γδ commute
            // (each rewrite regenerates the internal wire a generation down),
            // so the contract pins the aux-to-self flavor instead (case 14)
            // and recommends exactly this: a single-step pin on our side.
            Spec cm = spec_from_json(
                R"({"signatures":{"g":2,"d":2,"leaf":0},)"
                R"("rules":[{"glyphs":["g","d"],"rule":"commute"}]})");
            Net net3;
            net3.add({"G", "g", 2, "{}", {}});
            net3.add({"D", "d", 2, "{}", {}});
            net3.add({"P", "leaf", 0, "{}", {}});
            net3.add({"Q", "leaf", 0, "{}", {}});
            net3.connect({"G", 0}, {"D", 0});
            net3.connect({"G", 2}, {"D", 1}); // the internal loop
            net3.connect({"G", 1}, {"P", 0});
            net3.connect({"D", 2}, {"Q", 0});
            Net done3 = step(cm, net3, {"D", "G"});
            fuse_check(done3.agents.size() == 6, "internal-wire commute: 4 copies + 2 leaves");
            // the internal wire became a principal-principal pair of copies
            bool copy_pair = false;
            for (const auto& [id, ag] : done3.agents) {
                if (ag.glyph != "d" || id == "D") continue;
                const Port* pp = done3.partner({id, 0});
                if (pp && pp->second == 0 && done3.agents.at(pp->first).glyph == "g")
                    copy_pair = true;
            }
            fuse_check(copy_pair, "internal wire → copy principals paired");
            // strict mode still rejects it (the contract's restricted subset)
            try {
                step(cm, net3, {"D", "G"}, /*strict_locality=*/true);
                fuse_check(false, "strict mode must throw Locality");
            } catch (const ReduceError& e) {
                fuse_check(e.kind == ReduceError::Kind::Locality, "strict throws Locality kind");
            }
        }
        {
            // annihilate with an internal wire: the closed loop vanishes, the
            // surviving equation bridges the external ends
            Spec an = spec_from_json(
                R"({"signatures":{"c":2,"leaf":0},)"
                R"("rules":[{"glyphs":["c","c"],"rule":"annihilate"}]})");
            Net net4;
            net4.add({"x", "c", 2, "{}", {}});
            net4.add({"y", "c", 2, "{}", {}});
            net4.add({"P", "leaf", 0, "{}", {}});
            net4.add({"Q", "leaf", 0, "{}", {}});
            net4.connect({"x", 0}, {"y", 0});
            net4.connect({"x", 2}, {"y", 2}); // internal: x2≡y2 loops closed
            net4.connect({"x", 1}, {"P", 0});
            net4.connect({"y", 1}, {"Q", 0});
            Net done4 = step(an, net4, {"x", "y"});
            const Port* p = done4.partner({"P", 0});
            fuse_check(done4.agents.size() == 2, "loop vanished, leaves survive");
            fuse_check(p && p->first == "Q", "external ends bridged (P-Q)");
        }
    } catch (const std::exception& e) {
        ++fuse_failed;
        std::cout << "[FAIL] fuse: unexpected exception: " << e.what() << "\n";
    }
    if (fuse_failed == 0) std::cout << "[ok ] engine tests (fuse extension + step-level pins)\n";
    failed += fuse_failed;

    std::cout << "\nREDUCE CONFORMANCE (C++): " << (files.size() - failed) << "/"
              << files.size() << " cases pass"
              << (fuse_failed ? " (+ engine test FAILURES)" : " (+ engine tests ok)")
              << "\n";
    return failed ? 1 : 0;
}
