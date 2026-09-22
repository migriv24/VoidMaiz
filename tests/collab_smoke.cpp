/* collab_smoke.cpp — the collaborative canvas, the half that needs no screen.
 * No socket, no Palabra.
 *
 * okf/concepts/collaborative-canvas.md. Two things are pinned here:
 *
 *  1. The in-flight gesture payload: it round-trips, it refuses hostile input
 *     whole, and composing it applies rule 3 — a drag, a wire, a typing preview,
 *     a claim or a recent command line can never name a private rune.
 *  2. Claims: the author's rule, "whoever selected this specific port on the
 *     node first, is the one who is doing stuff with it", for people and agents
 *     alike, with a Lamport clock standing in for the shared clock nobody has.
 */
#include "voidmaiz/claims.hpp"
#include "voidmaiz/presence.hpp"

#include <iostream>
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

static SceneNode node(const char* id, const char* name, std::vector<std::string> tags = {}) {
    SceneNode n;
    n.id = id;
    n.name = name;
    n.glyph = "gamma";
    n.tags = std::move(tags);
    return n;
}

/* `era1` is private. Ids look nothing like names, on purpose. */
static Scene scene3() {
    Scene s;
    s.mantle = "lafont";
    s.nodes = {node("r-a1", "gam1"), node("r-b2", "del1"),
               node("r-c3", "era1", {std::string(net_tag::private_)})};
    return s;
}

static Profile me() { return {"dev-me", "Me", 0x3366cc, ""}; }

static PresenceState peer(const char* id, const char* name, std::vector<Claim> claims,
                          Participant kind = Participant::Person, std::uint64_t clock = 0) {
    PresenceState p;
    p.who = {id, name, 0xcc6633, ""};
    p.kind = kind;
    p.claims = std::move(claims);
    p.clock = clock;
    return p;
}

int main() {
    // ── 1. the payload round-trips ───────────────────────────────────────────
    {
        PresenceState s;
        s.who = me();
        s.selection = {"r-a1"};
        s.kind = Participant::Agent;
        s.device = "headless";
        s.compat = "ic-0.2/spec-9f2c";
        s.clock = 41;
        CanvasPresence c;
        c.surface = "canvas";
        c.has_cursor = true;
        c.cursor_x = 120.5f;
        c.cursor_y = -40.0f;
        c.has_view = true;
        c.view_x0 = -10; c.view_y0 = -20; c.view_x1 = 800; c.view_y1 = 600;
        c.gesture = CanvasGesture::Wire;
        c.wire_rune = "r-a1";
        c.wire_port = 0;
        c.ping_seq = 3;
        c.ping_x = 5;
        c.ping_y = 6;
        s.canvas.push_back(c);
        s.claims.push_back({"r-a1", claim_port(0), 40});
        s.recent = {"link gam1 del1 --relation 0:0"};

        PresenceState back;
        std::string err;
        CHECK(presence_from_json(presence_to_json(s), back, &err));
        CHECK(err.empty());
        CHECK(back.kind == Participant::Agent);
        CHECK(back.device == "headless" && back.compat == "ic-0.2/spec-9f2c");
        CHECK(back.clock == 41);
        CHECK(back.canvas.size() == 1);
        const CanvasPresence& b = back.canvas[0];
        CHECK(b.has_cursor && b.cursor_x == 120.5f && b.cursor_y == -40.0f);
        CHECK(b.has_view && b.view_x1 == 800 && b.view_y1 == 600);
        CHECK(b.gesture == CanvasGesture::Wire && b.wire_rune == "r-a1" && b.wire_port == 0);
        CHECK(b.ping_seq == 3 && b.ping_x == 5);
        CHECK(back.claims.size() == 1 && back.claims[0].part == "port:0" &&
              back.claims[0].stamp == 40);
        CHECK(back.recent == s.recent);

        // an old peer's payload (no collaboration fields) still parses, as a person
        PresenceState old;
        CHECK(presence_from_json(R"({"who":{"id":"x","name":"X"}})", old));
        CHECK(old.kind == Participant::Person && old.canvas.empty() && old.claims.empty());
    }

    // ── 2. hostile input is refused whole, never clamped ──────────────────────
    {
        auto refused = [](const char* json) {
            PresenceState st;
            st.who.id = "untouched";
            bool ok = presence_from_json(json, st);
            return !ok && st.who.id == "untouched"; // refused whole: nothing written
        };
        const char* who = R"("who":{"id":"p","name":"P"})";
        auto j = [&](const std::string& rest) { return "{" + std::string(who) + "," + rest + "}"; };
        CHECK(refused(j(R"("canvas":[{"surface":"c","cursor":[1e9,0]}])").c_str()));   // out of range
        CHECK(refused(j(R"("canvas":[{"surface":"c","cursor":[1]}])").c_str()));       // wrong shape
        CHECK(refused(j(R"("canvas":[{"surface":"c","cursor":["1","2"]}])").c_str())); // not numbers
        CHECK(refused(j(R"("canvas":[{"surface":"c","gesture":"teleport"}])").c_str()));
        CHECK(refused(j(R"("canvas":[{"surface":"c","gesture":"wire"}])").c_str()));   // names nothing
        CHECK(refused(j(R"("canvas":[{"surface":"c","gesture":"typing","field_rune":"r"}])").c_str()));
        CHECK(refused(j(R"("claims":[{"rune":"","stamp":1}])").c_str()));
        CHECK(refused(j(R"("claims":[{"rune":"r","stamp":-1}])").c_str()));
        CHECK(refused(j(R"("claims":[{"rune":"r","stamp":1.5}])").c_str()));
        CHECK(refused(j(R"("kind":"robot")").c_str()));
        CHECK(refused(j(R"("clock":-3)").c_str()));

        // too many canvases, claims, recent lines
        std::string many = "[";
        for (int i = 0; i < 9; ++i) many += std::string(i ? "," : "") + R"({"surface":"c"})";
        CHECK(refused(j("\"canvas\":" + many + "]").c_str()));
        std::string claims = "[";
        for (int i = 0; i < 65; ++i) claims += std::string(i ? "," : "") + R"({"rune":"r","stamp":1})";
        CHECK(refused(j("\"claims\":" + claims + "]").c_str()));
        std::string recent = "[";
        for (int i = 0; i < 9; ++i) recent += std::string(i ? "," : "") + "\"mv a 1 2\"";
        CHECK(refused(j("\"recent\":" + recent + "]").c_str()));

        // a preview may be longer than a name, but not unbounded
        std::string ok_preview(1000, 'x'), long_preview(1100, 'x');
        PresenceState st;
        CHECK(presence_from_json(j(R"("canvas":[{"surface":"c","gesture":"typing","field_rune":"r","field_key":"label","preview":")" +
                                   ok_preview + "\"}]"),
                                 st));
        CHECK(refused(j(R"("canvas":[{"surface":"c","gesture":"typing","field_rune":"r","field_key":"label","preview":")" +
                        long_preview + "\"}]")
                          .c_str()));
    }

    // ── 3. composing applies rule 3 to everything in flight ──────────────────
    {
        Scene scene = scene3();
        Surfaces surfaces;
        ShareFilter filter = share_by_tag();

        CollabOut out;
        out.clock = 7;
        CanvasPresence wire_to_private;
        wire_to_private.surface = "canvas";
        wire_to_private.has_cursor = true;
        wire_to_private.gesture = CanvasGesture::Wire;
        wire_to_private.wire_rune = "r-c3"; // era1 is private
        wire_to_private.wire_port = 0;
        out.canvas.push_back(wire_to_private);

        CanvasPresence typing_public;
        typing_public.surface = "canvas2";
        typing_public.gesture = CanvasGesture::Typing;
        typing_public.field_rune = "r-a1";
        typing_public.field_key = "label";
        typing_public.preview = "half a wor";
        out.canvas.push_back(typing_public);

        out.claims = {{"r-a1", claim_port(1), 5},
                      {"r-c3", "", 6},             // private: never named
                      {"r-zz", "", 6},             // unknown: not vouched for
                      {"lafont", std::string(claim_crank), 6}}; // the mantle itself
        out.recent = {"mv gam1 10 20", "tag era1 +red", "link gam1 del1", "set gam1 'unterminated"};

        PresenceState s = compose_presence(me(), {"r-a1", "r-c3"}, surfaces, SharePolicy{},
                                           scene, filter, out);
        CHECK(s.selection == (std::vector<std::string>{"r-a1"}));
        CHECK(s.clock == 7);
        CHECK(s.canvas.size() == 2);
        CHECK(s.canvas[0].gesture == CanvasGesture::None); // the wire named a private rune
        CHECK(s.canvas[0].wire_rune.empty());
        CHECK(s.canvas[0].has_cursor);                     // the cursor itself still goes
        CHECK(s.canvas[1].gesture == CanvasGesture::Typing && s.canvas[1].preview == "half a wor");
        CHECK(s.claims.size() == 2);
        CHECK(s.claims[0].rune == "r-a1" && s.claims[1].rune == "lafont");
        // the line naming era1 is dropped whole; the unreadable one too
        CHECK(s.recent == (std::vector<std::string>{"mv gam1 10 20", "link gam1 del1"}));

        // and the payload that leaves really does not contain the private rune
        std::string bytes = presence_to_json(s);
        CHECK(bytes.find("r-c3") == std::string::npos);
        CHECK(bytes.find("era1") == std::string::npos);

        // the sender's switches
        SharePolicy quiet;
        quiet.cursor = false;
        quiet.preview = false;
        quiet.recent = false;
        PresenceState q = compose_presence(me(), {"r-a1"}, surfaces, quiet, scene, filter, out);
        CHECK(!q.canvas[0].has_cursor);
        CHECK(q.canvas[1].gesture == CanvasGesture::Typing && q.canvas[1].preview.empty());
        CHECK(q.recent.empty());

        // a Move with nothing selected shows nothing (the ghost IS the selection)
        CollabOut mv;
        CanvasPresence m;
        m.surface = "canvas";
        m.gesture = CanvasGesture::Move;
        m.dx = 30;
        mv.canvas.push_back(m);
        PresenceState moved_private = compose_presence(me(), {"r-c3"}, surfaces, SharePolicy{},
                                                       scene, filter, mv);
        CHECK(moved_private.canvas[0].gesture == CanvasGesture::None);
    }

    // ── 4. claims: first to select is the one doing stuff with it ───────────
    {
        // Gary claimed port 0 of gam1 at stamp 3. I have SEEN his presence,
        // so my clock moves past it and my claim can only come after his.
        Roster roster;
        roster.set_self("dev-me");
        PresenceState gary = peer("dev-gary", "Gary", {{"r-a1", claim_port(0), 3}}, Participant::Person, 3);
        roster.update(gary, 0.0);

        Claims mine("dev-me");
        mine.observe(gary);
        CHECK(mine.clock() >= 3);

        ClaimHolder h = mine.acquire(roster, "r-a1", claim_port(0));
        CHECK(!h.self && h.who == "dev-gary" && h.name == "Gary");
        CHECK(mine.mine().empty()); // a losing claim is not broadcast
        CHECK(!mine.may_act(roster, "r-a1", claim_port(0)));
        CHECK(mine.gate(roster, "r-a1", claim_port(0)).find("Gary") != std::string::npos);

        // a different port of the same node is free: ports do not contend
        ClaimHolder p1 = mine.acquire(roster, "r-a1", claim_port(1));
        CHECK(p1.self);
        CHECK(p1.claim.stamp > 3);
        CHECK(mine.may_act(roster, "r-a1", claim_port(1)));
        CHECK(mine.gate(roster, "r-a1", claim_port(1)).empty());

        // but claiming the whole node contends with Gary's port
        ClaimHolder whole = mine.acquire(roster, "r-a1");
        CHECK(!whole.self && whole.who == "dev-gary");

        // re-selecting keeps the ORIGINAL stamp
        std::uint64_t first = p1.claim.stamp;
        mine.acquire(roster, "r-a1", claim_port(1));
        CHECK(mine.mine().size() == 1 && mine.mine()[0].stamp == first);

        // releasing frees it for everyone
        mine.release("r-a1", claim_port(1));
        CHECK(mine.mine().empty());
    }

    // ── 5. concurrent claims resolve the same way on both devices ───────────
    {
        // Mike and John claim del1 in the same round-trip, both at stamp 5.
        // Neither saw the other. Each device must name the same winner.
        Roster at_mike, at_john;
        at_mike.set_self("dev-mike");
        at_john.set_self("dev-john");
        Claims mike("dev-mike"), john("dev-john");
        ClaimHolder mh = mike.acquire(at_mike, "r-b2");
        ClaimHolder jh = john.acquire(at_john, "r-b2");
        CHECK(mh.self && jh.self); // both believed they had it — the race window

        // presence arrives
        PresenceState mike_p = peer("dev-mike", "Mike", mike.mine(), Participant::Person, mike.clock());
        PresenceState john_p = peer("dev-john", "John", john.mine(), Participant::Person, john.clock());
        at_mike.update(john_p, 1.0);
        at_john.update(mike_p, 1.0);

        auto w_mike = mike.holder(at_mike, "r-b2");
        auto w_john = john.holder(at_john, "r-b2");
        CHECK(w_mike && w_john);
        CHECK(w_mike->who == w_john->who); // the same answer everywhere
        CHECK(w_mike->who == "dev-john");  // equal stamps → the smaller id

        // the loser yields, and learns who won so the gesture can say so
        std::vector<ClaimHolder> lost_mike = mike.yield(at_mike);
        std::vector<ClaimHolder> lost_john = john.yield(at_john);
        CHECK(lost_mike.size() == 1 && lost_mike[0].who == "dev-john");
        CHECK(lost_john.empty());
        CHECK(mike.mine().empty() && john.mine().size() == 1);
    }

    // ── 6. a person outranks an agent, whatever the stamps say ──────────────
    {
        Roster roster;
        roster.set_self("dev-me");
        // the agent claimed first (stamp 1)
        roster.update(peer("agent-7", "Refactor bot", {{"r-a1", "", 1}}, Participant::Agent, 1), 0.0);
        Claims person("dev-me");
        person.observe(roster.peers()[0].state);
        ClaimHolder h = person.acquire(roster, "r-a1");
        CHECK(h.self); // the person takes it

        // and the agent, seeing that, yields and its assign is gated
        Roster agent_view;
        agent_view.set_self("agent-7");
        Claims agent("agent-7", Participant::Agent);
        agent.acquire(agent_view, "r-a1");
        CHECK(agent.mine().size() == 1);
        agent_view.update(peer("dev-me", "Me", person.mine(), Participant::Person, person.clock()), 1.0);
        CHECK(agent.yield(agent_view).size() == 1);
        std::string why = agent.gate(agent_view, "r-a1", claim_port(0));
        CHECK(!why.empty() && why.find("Me") != std::string::npos);

        // between two agents, first still wins
        CHECK(claim_wins(Participant::Agent, {"r", "", 1}, "b", Participant::Agent, {"r", "", 2}, "a"));
        CHECK(!claim_wins(Participant::Agent, {"r", "", 1}, "a", Participant::Person, {"r", "", 9}, "b"));
    }

    if (failures) {
        std::cerr << "collab_smoke: " << failures << " FAILED\n";
        return 1;
    }
    std::cout << "collab_smoke: all ok\n";
    return 0;
}
