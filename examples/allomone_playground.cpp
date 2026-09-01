/*
 * allomone_playground.cpp — the Allomone test vehicle (2026-08-06;
 * okf/concepts/allomone/index.md).
 *
 * Two tabs, deliberately meaning-free, so what gets tested is the ENGINE and
 * not a domain:
 *
 *   1. "Allo Dev" — the script workbench. The custom code editor (code.hpp)
 *      with syntax colouring and inline token widgets (click a #rrggbb and the
 *      colour wheel opens ON the token), a script list you can enable/disable,
 *      live diagnostics, the derived annotations, and the conflict panel where
 *      a disagreement between two scripts is settled by a dispatcher command.
 *
 *   2. "Playground" — buttons, sliders, toggles and lists that are RUNES, not
 *      widgets-with-state. Right-click any of them to add or subtract tags;
 *      the enabled scripts re-derive instantly and restyle what you tagged.
 *      Every touch also feeds the user action graph, so `when with "…"` starts
 *      matching things you actually work on together.
 *
 * WHY THE PLAYGROUND HAS NO MEANING. That is the test. If the engine needs to
 * know what a "contact" or a "note" is, it is not the general engine it claims
 * to be — the second-client test (scope-and-clients.md) applied to scripting.
 *
 * THE LOOP THE AUTHOR IS TESTING: the person writes a rule, the application
 * inherits it, the person tags something, the rule fires. Nothing is compiled,
 * nothing is run, and there is no run button — the constraint is simply present
 * and the picture settles. Every step of it is a logged command in the strip.
 */
#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/code.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/sentinel.hpp"
#include "voidmaiz/usergraph.hpp"
#include "voidmaiz/weaver.hpp"
#include "voidmaiz/widgets.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

// ── small helpers ───────────────────────────────────────────────────────────

/* SceneField carries JSON; the editor wants the decoded string. */
static std::string unquote(const std::string& j) {
    if (j.size() < 2 || j.front() != '"') return j;
    std::string out;
    for (size_t i = 1; i + 1 < j.size(); ++i) {
        if (j[i] == '\\' && i + 2 < j.size()) {
            char n = j[++i];
            out += (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
        } else {
            out += j[i];
        }
    }
    return out;
}

/* Wrap a value as ONE argument for the command tokenizer.
 *
 * Single quotes, deliberately: inside them the core treats \' as an escaped
 * quote and passes everything else through literally — including the double
 * quotes an Allomone script is full of, and real newlines (VoidCore
 * src/dispatch/args.c). A DOUBLE-quoted argument could carry neither: `"` would
 * close the token, and `\n` would stay two characters, which is exactly the bug
 * that made a whole script parse as one comment line. */
static std::string cmd_arg(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "\\'";
        else out += c;
    }
    return out + "'";
}

static std::string field_of(const maiz::SceneNode& n, const char* key) {
    for (const auto& f : n.fields)
        if (f.key == key) return unquote(f.value_json);
    return {};
}

/* Permissive on input, canonical on output: `#rrggbb`, bare `rrggbb`, and the
 * `#rgb` short form all count as colours, so typing a fresh literal gets a wheel
 * without having to remember the exact spelling. The wheel always writes back
 * `#rrggbb`, so the script converges on one form as you edit it. */
/* The canonical spelling of a kernel condition, for showing an expansion back
 * to the author. Core's aliases (`has`/`glyph`/`rune`) parse to the same Kind,
 * so an expansion prints in ONE spelling rather than echoing whichever the
 * definition happened to use — the point is to show what it MEANS. */
static const char* term_word(maiz::Term::Kind k) {
    switch (k) {
    case maiz::Term::Kind::Tag:    return "tag";
    case maiz::Term::Kind::Kind_:  return "kind";
    case maiz::Term::Kind::Name:   return "name";
    case maiz::Term::Kind::Mantle: return "mantle";
    case maiz::Term::Kind::Host:   return "";
    }
    return "";
}

static bool is_hex_color(const std::string& s) {
    size_t i = (!s.empty() && s[0] == '#') ? 1 : 0;
    size_t digits = s.size() - i;
    if (digits != 6 && digits != 3) return false;
    if (i == 0 && digits == 3) return false; // a bare "abc" is a word, not a colour
    for (; i < s.size(); ++i)
        if (!std::isxdigit((unsigned char)s[i])) return false;
    return true;
}

/* Any accepted spelling -> 0xrrggbb. */
static unsigned hex_color_value(const std::string& s) {
    std::string d = (!s.empty() && s[0] == '#') ? s.substr(1) : s;
    if (d.size() == 3) // #rgb -> #rrggbb
        d = {d[0], d[0], d[1], d[1], d[2], d[2]};
    return (unsigned)std::strtoul(d.c_str(), nullptr, 16);
}

static bool is_date(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u})
        if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}

static unsigned parse_rgb(const std::string& s, unsigned fallback) {
    if (!is_hex_color(s)) return fallback;
    return hex_color_value(s);
}

/* ── mixing colours: the OKLab mean ──────────────────────────────────────────
 *
 * The author's idea: two scripts disagreeing about a colour do not have to be a
 * conflict — "red and blue make purple". So `color` can be declared a Custom
 * join and the disagreement becomes a MIX.
 *
 * WHICH COLOUR THEORY, and why it matters. Averaging hex digits directly (sRGB)
 * is the obvious thing and looks wrong: sRGB is gamma-encoded, so the midpoint
 * of red and blue comes out a dark muddy plum. Averaging in LINEAR light is
 * physically right for *emitted* light but reads too bright. **OKLab** is built
 * so that a numeric midpoint looks like a perceptual midpoint, which is what
 * "red and blue make purple" actually means to a person — so that is the space
 * used here.
 *
 * ORDER-INDEPENDENCE. The mean is taken over the WHOLE SET at once, never folded
 * pairwise: `avg(avg(a,b),c)` is not `avg(a,avg(b,c))`, so a pairwise fold would
 * quietly break associativity and make the result depend on merge order. The
 * JoinFn contract hands over the entire deduped set for exactly this reason.
 *
 * This lives in the HOST, not the library, because it is colour theory — and the
 * library is not allowed to know what a colour is. */
static float srgb_to_linear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
static float linear_to_srgb(float c) {
    return c <= 0.0031308f ? c * 12.92f
                           : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

static void rgb_to_oklab(unsigned rgb, float& L, float& A, float& B) {
    float r = srgb_to_linear(((rgb >> 16) & 0xff) / 255.0f);
    float g = srgb_to_linear(((rgb >> 8) & 0xff) / 255.0f);
    float b = srgb_to_linear((rgb & 0xff) / 255.0f);
    float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    A = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    B = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}

static unsigned oklab_to_rgb(float L, float A, float B) {
    float l = L + 0.3963377774f * A + 0.2158037573f * B;
    float m = L - 0.1055613458f * A - 0.0638541728f * B;
    float s = L - 0.0894841775f * A - 1.2914855480f * B;
    l = l * l * l; m = m * m * m; s = s * s * s;
    float r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    auto q = [](float v) {
        v = linear_to_srgb(v);
        int i = (int)(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
        return (unsigned)i;
    };
    return (q(r) << 16) | (q(g) << 8) | q(b);
}

/* The JoinFn: a set of colour strings in, one blended `#rrggbb` out. */
static std::string blend_colors(const std::vector<std::string>& values) {
    float L = 0, A = 0, B = 0;
    int n = 0;
    for (const std::string& v : values) {
        if (!is_hex_color(v)) continue; // ignore what is not a colour
        float l, a, b;
        rgb_to_oklab(hex_color_value(v), l, a, b);
        L += l; A += a; B += b;
        ++n;
    }
    if (n == 0) return values.empty() ? std::string() : values.front();
    unsigned mixed = oklab_to_rgb(L / n, A / n, B / n);
    char buf[16];
    std::snprintf(buf, sizeof buf, "#%06x", mixed & 0xffffff);
    return buf;
}

static ImVec4 to_vec4(unsigned rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xff) / 255.0f, ((rgb >> 8) & 0xff) / 255.0f,
                  (rgb & 0xff) / 255.0f, a);
}

// ── the seed: controls as runes, scripts as runes ───────────────────────────

static void seed(maiz::Core& core) {
    // The playground vocabulary. Note what ISN'T here: any meaning. A control
    // has a kind, a label and a value, and the tags carry everything else.
    core.register_glyph(R"({"glyph":"button","label":"Button","fields":["label","value"]})");
    core.register_glyph(R"({"glyph":"slider","label":"Slider","fields":["label","value"]})");
    core.register_glyph(R"({"glyph":"toggle","label":"Toggle","fields":["label","value"]})");
    core.register_glyph(R"({"glyph":"list","label":"List","fields":["label","value"]})");
    core.register_glyph(
        R"({"glyph":"allo-script","label":"Allomone script","fields":["source","enabled"],)"
        R"("hints":{"editors":{"source":"multiline","enabled":"bool"}}})");
    core.register_glyph(
        R"({"glyph":"allomone-resolution","label":"Resolution",)"
        R"("fields":["property","subject","winner","value"]})");
    core.register_glyph(std::string(maiz::UserGraph::affordance_glyph()));

    core.dispatch("mantle new playground");
    struct Seed { const char* glyph; const char* name; const char* label; const char* tags; };
    const Seed seeds[] = {
        {"slider", "volume",   "Volume",    "audio loud"},
        {"slider", "balance",  "Balance",   "audio"},
        {"slider", "warp",     "Warp",      "audio danger"},
        {"button", "play",     "Play",      "transport"},
        {"button", "stop",     "Stop",      "transport"},
        {"button", "detonate", "Detonate",  "danger muted"},
        {"toggle", "loop",     "Loop",      "transport"},
        {"toggle", "safety",   "Safety",    "danger"},
        {"list",   "presets",  "Presets",   "library"},
        {"list",   "history",  "History",   "library muted"},
    };
    for (const Seed& s : seeds) {
        core.dispatch(std::string("rune new ") + s.glyph + " " + s.name);
        core.dispatch(std::string("set ") + s.name + " label \"" + s.label + "\"");
        core.dispatch(std::string("setjson ") + s.name + " value 0.35");
        std::string tags(s.tags), tag;
        for (size_t i = 0; i <= tags.size(); ++i) {
            if (i == tags.size() || tags[i] == ' ') {
                if (!tag.empty()) core.dispatch("tag " + std::string(s.name) + " +" + tag);
                tag.clear();
            } else tag += tags[i];
        }
    }

    core.dispatch("mantle new scripts");
    core.dispatch("rune new allo-script theme");
    core.dispatch("set theme source " +
                  cmd_arg("# the broad default, plus sharper exceptions.\n"
                        "# rules are a SET - move a line, nothing changes.\n"
                        "when all then color \"#7d8590\"\n"
                        "when tag \"transport\" then color \"#3fb950\"\n"
                        "when tag \"audio\" then color \"#58a6ff\"\n"
                        "when tag \"danger\" then color \"#f85149\", weight 2\n"
                        "when tag \"danger\" and tag \"muted\" then color \"#8b3a34\"\n"
                        "# `warp` is both audio AND danger. at equal strength that\n"
                        "# is a silent, deterministic tiebreak inside one script --\n"
                        "# so say which you meant, and specificity settles it.\n"
                        "when tag \"audio\" and tag \"danger\" then color \"#f85149\"\n"
                        "when kind \"slider\" then weight 1\n"
                        "# void core's vocabulary, same meanings: `has` = tag,\n"
                        "# `glyph` = kind, `rune` = name. `mantle` is new.\n"
                        "when has \"loud\" and glyph \"slider\" then weight 1\n"
                        "when mantle \"playground\" then present 1\n"
                        "\n"
                        "# `define` names a condition so it can be reused. it is\n"
                        "# EXPANDED where you call it, so its terms count toward\n"
                        "# strength - a call can never make a rule broader than\n"
                        "# it looks. right-click a call to see what it becomes.\n"
                        "when risky(\"muted\") then weight 2\n"
                        "# the definition sits BELOW its use on purpose:\n"
                        "# definitions are a set, exactly like rules are.\n"
                        "define risky(t) = has t and has \"danger\"\n"
                        "\n"
                        "# `label` is ALSO a field every widget glyph declares.\n"
                        "# deriving it does NOT write that field - allomone is\n"
                        "# derive-only. disable this script and the original\n"
                        "# labels come straight back, with nothing to undo.\n"
                        "# the diagnostics tab says so, discovered not declared.\n"
                        "when has \"danger\" then label \"!! danger\"\n"));
    core.dispatch("setjson theme enabled true");

    core.dispatch("rune new allo-script responsive");
    core.dispatch("set responsive source " +
                  cmd_arg("# `louder` is not a kernel word - it is THIS app's\n"
                        "# domain predicate, registered in C++. parameterized\n"
                        "# and continuous, so a tag could not express it.\n"
                        "when louder \"0.5\" then glow 1\n"
                        "# `with` reads the USER, not the data: whatever you\n"
                        "# touch alongside `volume` starts to glow.\n"
                        "when with \"volume\" then glow 1\n"
                        "when tag \"library\" then weight 1\n"));
    core.dispatch("setjson responsive enabled true");

    core.dispatch("rune new allo-script rival");
    core.dispatch("set rival source " +
                  cmd_arg("# disabled by default. enable it and `danger` becomes\n"
                        "# a genuine disagreement with `theme` — same strength,\n"
                        "# different answer. the engine refuses to guess.\n"
                        "when tag \"danger\" then color \"#d29922\"\n"));
    core.dispatch("setjson rival enabled false");

    core.dispatch("mantle new resolutions");
    core.dispatch("use playground");
}

// ── main ────────────────────────────────────────────────────────────────────

int main() {
    glfwSetErrorCallback([](int e, const char* d) { std::fprintf(stderr, "glfw %d: %s\n", e, d); });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1340, 820, "Void Maiz — Allomone", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    maiz::enable_docking();
    ImGui::StyleColorsDark();
    // Its own layout file: sharing `imgui.ini` with the canvas demo meant this
    // app booted into that one's dock arrangement, and the DockBuilder seed
    // below never fired because the node already existed.
    ImGui::GetIO().IniFilename = "imgui_allomone.ini";

    maiz::Core core;
    std::vector<maiz::LogEntry> log;
    core.set_log_sink([&](std::string_view l, std::string_view o, std::string_view m) {
        log.push_back({std::string(l), std::string(o), std::string(m)});
    });
    core.dispatch("config set actor human:allomone");
    seed(core);

    maiz::CommandBarState cmdbar;
    maiz::UserGraph ugraph;
    maiz::CodeWidgetRegistry code_kit = maiz::CodeWidgetRegistry::defaults();

    // ── this app's domain vocabulary (the host-protocol extension point) ────
    // `louder "0.5"` cannot be a tag: it is PARAMETERIZED and CONTINUOUS, which
    // is exactly the case tags encode badly and the predicate registry exists
    // for. The closure reads the host's own projection — the library never
    // learns what a "value" is.
    //
    // It is a pure function of its arguments given the frame's scene: no clock,
    // no I/O, no mutable state. That rule is what keeps merge order-independent
    // for everyone downstream.
    maiz::Scene pg; // hoisted so the predicate can read this frame's projection
    maiz::PredicateRegistry preds;
    preds.add("louder", [&pg](const maiz::Subject& s, std::string_view arg,
                              const maiz::UserGraph*) {
        const maiz::SceneNode* n = pg.find(s.id);
        if (!n) return false;
        double v = std::strtod(field_of(*n, "value").c_str(), nullptr);
        double threshold = std::strtod(std::string(arg).c_str(), nullptr);
        return v > threshold;
    });
    maiz::CodeEditorState editor;

    std::string selected_script = "theme";
    std::string editing_script;           // which script `editor` currently holds
    // The derived state as it stood just before the last applied edit, so the
    // "edit delta" panel can answer "what did my change actually do?"
    maiz::Merged before_edit;
    std::string before_edit_script;
    bool have_before_edit = false;
    std::string channel = std::string(maiz::channel::pointer);
    bool frame_open = false;
    double frame_idle = 0;                // host-side framing POLICY (a clock is
                                          // fine here; the GRAPH keeps no time)
    std::string tag_target;               // the control whose tag popup is open
    char tag_buf[64] = {};
    float lod_threshold = 7.0f;           // editor level-of-detail switch
    float frame_idle_limit = 2.5f;        // host framing POLICY (see settings tab)
    float heat_cell = 96.0f;              // screen-cell size for click regions
    bool show_heatmap = false;
    bool recency_policy = false;          // ConflictPolicy for every property
    bool blend_colors_on = false;         // `color` as a Custom (OKLab) join
    /* View state for the effect-graph panel. A camera and nothing else: the
     * graph is derived, so there is no selection to keep and no drag to stage
     * — which is exactly why it uses draw_canvas rather than edit_canvas. */
    maiz::Camera effect_cam;
    float drag_value = 0;                 // staged slider value (VLS #14b)
    std::string dragging;

    auto dispatch = [&](const std::string& cmd) { return core.dispatch(cmd); };

    /* Run a command against a SPECIFIC mantle and come back.
     *
     * Every runtime command here targets a rune, and a rune is only reachable
     * from its own mantle — so forgetting the switch makes the command fail
     * silently-looking ("no rune matches"). That mistake has now been made
     * twice, so it gets a helper instead of a comment. */
    auto dispatch_in = [&](const char* mantle, const std::string& cmd) {
        core.dispatch(std::string("use ") + mantle);
        maiz::Result r = core.dispatch(cmd);
        core.dispatch("use playground");
        return r;
    };
    auto dispatch_all = [&](const std::vector<std::string>& cmds) {
        for (const auto& c : cmds) core.dispatch(c);
    };

    /* THE CONFLICT INSPECTOR — a browser's "computed styles" pane.
     *
     * The winner on top, everything it beat struck through beneath, each row
     * saying WHY it ended up there. The classification is `explain_cell()`'s,
     * not this file's: a host sorting contributors by strength and calling the
     * top one the winner gets the interesting cases wrong (a combining law has
     * no winner, a resolution's winner may be absent from the list, agreement
     * is not an override). Asking the library beats keeping a second copy of
     * its reasoning.
     *
     * Usable on ANY cell, not only conflicted ones — "why is this the colour?"
     * is a question worth answering when nothing is wrong. */
    auto inspector = [&](const maiz::MergedCell& c) {
        for (const maiz::CellVerdict& v : maiz::explain_cell(c)) {
            ImGui::PushID(&v);
            // Settling is a COMMAND — logged, replayable, and it feeds straight
            // back into merge(). Only offer it where naming a winner MEANS
            // something: a combining law has no winner to name, and a decided
            // cell does not need one.
            const bool nameable = v.verdict == "tied" || v.verdict == "overridden" ||
                                  v.verdict == "tiebreak";
            if (nameable) {
                if (ImGui::SmallButton("use")) {
                    dispatch_all(maiz::compile_resolution(
                        "resolutions", {c.subject, c.property, v.source, ""}));
                    dispatch("use playground");
                }
                ImGui::SameLine();
            } else {
                ImGui::Dummy(ImVec2(34, 1));
                ImGui::SameLine();
            }

            const ImVec4 col =
                v.verdict == "winner"      ? ImVec4(0.55f, 0.9f, 0.6f, 1)
                : v.verdict == "tied"      ? ImVec4(1, 0.55f, 0.2f, 1)
                : v.lost                   ? ImVec4(0.5f, 0.5f, 0.5f, 1)
                                           : ImVec4(0.75f, 0.75f, 0.8f, 1);
            std::string line = v.source;
            if (!v.origin.empty()) line += " (" + v.origin + ")";
            line += " = " + v.value + "  [str " + std::to_string(v.strength) + "]";
            ImGui::TextColored(col, "%s", line.c_str());

            // ImGui has no strikethrough, so draw one — the losing rows must be
            // visually *discarded*, not merely dimmer, because "this is not in
            // the answer" is the single fact this pane exists to convey.
            if (v.lost) {
                ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                float y = (a.y + b.y) * 0.5f;
                ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, y), ImVec2(b.x, y),
                                                    ImGui::GetColorU32(col));
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", v.verdict.c_str());
            ImGui::PopID();
        }
    };

    // A touch: counts on the affordance and joins the open coherence frame.
    auto touch = [&](const std::string& id, const char* kind) {
        if (!frame_open) { ugraph.open_frame(); frame_open = true; }
        frame_idle = 0;
        ugraph.touch(id, kind, channel);
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) { glfwWaitEvents(); continue; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiIO& gio = ImGui::GetIO();

        // The framing policy: a lull ends the working frame. Host-side and
        // explicit — the graph itself never learns that time passed.
        if (frame_open) {
            frame_idle += gio.DeltaTime;
            if (frame_idle > frame_idle_limit) { ugraph.close_frame(); frame_open = false; }
        }

        // ── every click is an action, wherever it lands ─────────────────────
        // The author's point: a click on nothing in particular is still work,
        // and the graph should know where the person is spending it. So the
        // SCREEN is divided into cells and each cell is an ordinary affordance
        // (kind "region") — an id like `cell:3,7`. Coherence between a cell and
        // a widget then falls out for free: they were touched in one frame.
        //
        // Deliberately on CLICK, not per-frame-under-cursor: sampling every
        // frame would measure DWELL, which is a clock, and a clock in the
        // record is what non-linearity.md forbids. A click is a discrete act,
        // so counting clicks is frequency — order-free and honest.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            int cx = (int)(gio.MousePos.x / heat_cell);
            int cy = (int)(gio.MousePos.y / heat_cell);
            if (cx >= 0 && cy >= 0)
                touch("cell:" + std::to_string(cx) + "," + std::to_string(cy), "region");
        }

        // ── project, derive, compose (every frame; nothing is cached) ────────
        pg = maiz::project_scene(core, {"playground"});
        maiz::Scene sc = maiz::project_scene(core, {"scripts"});
        maiz::Scene rs = maiz::project_scene(core, {"resolutions"});

        std::vector<maiz::Subject> subjects;
        for (const auto& n : pg.nodes)
            subjects.push_back({.id = n.name, .kind = n.glyph, .name = n.name,
                                .mantle = "playground", .tags = n.tags});

        /* THE THIRD VOCABULARY: what a rune actually HAS
         * (okf/concepts/allomone/discovery.md §3a).
         *
         * Two are already discovered — the properties rules WRITE (harvested
         * from the parsed ASTs) and the cells currently in play (`merged`).
         * This is the third: the fields a glyph DECLARES, which is what the
         * surface census would harvest at tier 0.
         *
         * It needs no census and no new parsing. The projection already carries
         * each node's declared fields, because `project_scene` was handed the
         * glyph descriptors — so the vocabulary is a fold over the Scene, in
         * exactly the shape the other two use. (The census header is a contract
         * with no implementation; naming it as the source would have been a
         * dependency on something that does not exist.)
         *
         * WHY IT IS WORTH KNOWING: a property can SHADOW a real field. `label`
         * is declared by every playground glyph AND written by rules, and
         * because Allomone is derive-only, `then label "x"` does NOT write the
         * rune's field — it derives an annotation this host happens to prefer
         * when rendering. That is a reasonable design and an easy thing to
         * misread, so the editor says so rather than leaving it to be
         * discovered. */
        std::map<std::string, std::set<std::string>> glyph_fields; // field -> glyphs
        for (const auto& n : pg.nodes)
            for (const auto& f : n.fields) glyph_fields[f.key].insert(n.glyph);

        maiz::MergeOptions mopts;
        mopts.lattices = {{"weight", maiz::Lattice::Sum},  // contributions add
                          {"glow", maiz::Lattice::Max}};   // loudest wins
        if (blend_colors_on) {
            // A third answer to disagreement, beside "surface it" and "pick a
            // winner": MIX. Declared by the host, because the blend is colour
            // theory and the library does not know what a colour is.
            maiz::PropertyLattice c;
            c.property = "color";
            c.law = maiz::Lattice::Custom;
            c.custom = blend_colors;
            mopts.lattices.push_back(std::move(c));
        }
        mopts.default_policy = recency_policy ? maiz::ConflictPolicy::Recency
                                              : maiz::ConflictPolicy::Surface;
        for (const auto& n : rs.nodes) {
            maiz::Resolution r;
            r.property = field_of(n, "property");
            r.subject = field_of(n, "subject");
            r.winner = field_of(n, "winner");
            r.value = field_of(n, "value");
            if (!r.property.empty()) mopts.resolutions.push_back(r);
        }

        std::vector<maiz::ConstraintMap> maps;
        std::vector<std::pair<std::string, maiz::Script>> parsed;
        for (const auto& n : sc.nodes) {
            maiz::Script s = maiz::allo_parse(n.name, field_of(n, "source"));
            // Unknown-predicate diagnostics are a CHECK, not a parse error: a
            // script naming a predicate this host lacks is still valid text.
            for (const auto& d : maiz::allo_check(s, &preds))
                s.diagnostics.push_back(d);
            parsed.push_back({n.name, s});
            if (field_of(n, "enabled") == "true") {
                maiz::ConstraintMap cm = maiz::allo_eval(s, subjects, &preds);
                // Position in the mantle IS the age: `rune new` appends, so a
                // later script is a newer one. No clock, no stored timestamp —
                // exactly the identity Hormiga's conflicts.md says we already
                // keep (okf/concepts/allomone/composition.md).
                cm.recency = (int)maps.size() + 1;
                maps.push_back(std::move(cm));
            }
        }
        maiz::Merged merged = maiz::merge(maps, mopts);

        // ── shell ───────────────────────────────────────────────────────────
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        unsigned dock = maiz::begin_dockspace();
        if (maiz::dockspace_needs_seed(dock)) {
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
            ImGuiID main_id = dock;
            ImGuiID bottom = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down, 0.26f,
                                                          nullptr, &main_id);
            ImGui::DockBuilderDockWindow("Workbench", main_id);
            ImGui::DockBuilderDockWindow("Log", bottom);
            ImGui::DockBuilderFinish(dock);
        }

        ImGui::Begin("Workbench", nullptr, ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTabBar("##tabs")) {

            // ══ TAB 1 — Allo Dev ════════════════════════════════════════════
            if (ImGui::BeginTabItem("Allo Dev")) {
                ImGui::BeginChild("##scripts", ImVec2(190, 0), ImGuiChildFlags_Borders);
                ImGui::TextDisabled("scripts");
                ImGui::Separator();
                for (const auto& n : sc.nodes) {
                    bool on = field_of(n, "enabled") == "true";
                    ImGui::PushID(n.name.c_str());
                    if (ImGui::Checkbox("##on", &on)) {
                        // Enabling a script is a command. Disabling it removes
                        // its derived effects with nothing to undo — the whole
                        // payoff of derive-only.
                        dispatch_in("scripts", "setjson " + n.name + " enabled " + (on ? "true" : "false"));
                        touch("script:" + n.name, "script");
                    }
                    ImGui::SameLine();
                    if (ImGui::Selectable(n.name.c_str(), selected_script == n.name)) {
                        selected_script = n.name;
                        touch("script:" + n.name, "script");
                    }
                    const maiz::Script* ps = nullptr;
                    for (const auto& p : parsed)
                        if (p.first == n.name) ps = &p.second;
                    if (ps && !ps->ok()) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1, 0.35f, 0.3f, 1), "!");
                    }
                    ImGui::PopID();
                }
                ImGui::Separator();
                if (ImGui::SmallButton("+ script")) {
                    std::string name = "script-" + std::to_string(sc.nodes.size() + 1);
                    dispatch_in("scripts", "rune new allo-script " + name);
                    dispatch_in("scripts", "set " + name + " source " +
                                           cmd_arg("when all then color \"#7d8590\"\n"));
                    dispatch_in("scripts", "setjson " + name + " enabled true");
                    selected_script = name;
                }
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("##edit", ImVec2(0, 0));

                // Reseat the buffer only when the TARGET changes — never while
                // the author is typing (that would fight the staging discipline).
                const maiz::SceneNode* snode = sc.find(selected_script);
                if (snode && editing_script != selected_script) {
                    editor.set_text(field_of(*snode, "source"));
                    editing_script = selected_script;
                    editor.clear_dirty();
                }

                ImGui::Text("%s", selected_script.c_str());
                ImGui::SameLine();
                // An explicit Apply, because "it committed when focus left" is
                // invisible. The button IS the staging discipline made legible:
                // edits stage locally, one command flushes on apply.
                bool apply_now = false;
                if (editor.dirty) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.42f, 0.10f, 1));
                    if (ImGui::SmallButton("apply *")) apply_now = true;
                    ImGui::PopStyleColor();
                } else {
                    ImGui::BeginDisabled();
                    ImGui::SmallButton("apply");
                    ImGui::EndDisabled();
                }
                ImGui::SameLine(0, 16);
                ImGui::TextDisabled(
                    "click a #colour to edit in place (click away to apply)  |  "
                    "ctrl+enter  |  ctrl+wheel zooms  (%.0f%%)", editor.zoom * 100.0f);

                maiz::CodeEditorOptions eo;
                eo.registry = &code_kit;
                eo.lod_threshold = lod_threshold;
                eo.highlight = [](std::string_view src) {
                    std::vector<maiz::CodeSpan> out;
                    for (const maiz::Token& t : maiz::allo_tokens(src)) {
                        unsigned rgb = 0xd0d0d0;
                        switch (t.kind) {
                        case maiz::TokenKind::Comment: rgb = 0x6a7079; break;
                        case maiz::TokenKind::Keyword: rgb = 0xd2a8ff; break;
                        case maiz::TokenKind::Arrow:   rgb = 0xff7b72; break;
                        case maiz::TokenKind::Number:  rgb = 0x79c0ff; break;
                        case maiz::TokenKind::Ident:   rgb = 0xffa657; break;
                        case maiz::TokenKind::Invalid: rgb = 0xff3333; break;
                        case maiz::TokenKind::String:
                            // A colour literal draws in ITS OWN colour: you see
                            // the palette in the script, not just hex.
                            rgb = is_hex_color(t.text) ? parse_rgb(t.text, 0xa5d6ff) : 0xa5d6ff;
                            break;
                        default: continue;
                        }
                        out.push_back({t.begin, t.end, rgb});
                    }
                    return out;
                };
                eo.widgets = [](std::string_view src) {
                    std::vector<maiz::CodeWidgetSpan> out;
                    for (const maiz::Token& t : maiz::allo_tokens(src)) {
                        if (t.kind != maiz::TokenKind::String) continue;
                        if (is_hex_color(t.text)) out.push_back({t.begin, t.end, "color", t.text, true});
                        else if (is_date(t.text)) out.push_back({t.begin, t.end, "date", t.text, true});
                    }
                    return out;
                };
                // ── completion: the vocabulary, offered rather than memorized ─
                // With Core aliases AND host predicates AND whatever tags the
                // data happens to carry, nobody can hold this in their head.
                // The host knows all of it, so the host offers it.
                eo.complete = [&](std::string_view src,
                                  size_t caret) -> maiz::CompletionSet {
                    maiz::CompletionSet cs;
                    std::vector<maiz::Token> toks = maiz::allo_tokens(src);

                    // Which token is the caret in or just after, and what came
                    // before it? That pair is the whole context model.
                    const maiz::Token* cur = nullptr;
                    const maiz::Token* prev = nullptr;
                    for (const auto& t : toks) {
                        if (t.kind == maiz::TokenKind::End) break;
                        if (caret >= t.begin && caret <= t.end) { cur = &t; break; }
                        if (t.end <= caret) prev = &t;
                    }
                    if (cur && cur->kind == maiz::TokenKind::Comment) return cs;

                    auto add = [&](std::string text, std::string detail) {
                        cs.items.push_back({std::move(text), "", std::move(detail)});
                    };
                    // The half-typed word at the caret, if any — what a
                    // suggestion list has to be filtered against.
                    auto typed_word = [&]() -> std::string {
                        return (cur && cur->kind == maiz::TokenKind::Ident) ? cur->text
                                                                            : std::string();
                    };
                    // Filter by what has been typed, ignoring the quotes a
                    // ready-to-insert value carries: typing `au` must still
                    // match the item `"audio"`.
                    auto keep_prefix = [&](const std::string& pre) {
                        if (pre.empty()) return;
                        cs.items.erase(
                            std::remove_if(
                                cs.items.begin(), cs.items.end(),
                                [&](const maiz::Completion& c) {
                                    std::string t = c.text;
                                    if (!t.empty() && t.front() == '"') t.erase(0, 1);
                                    return t.size() < pre.size() ||
                                           t.compare(0, pre.size(), pre) != 0;
                                }),
                            cs.items.end());
                    };

                    // ── inside a string: offer VALUES for the preceding word ──
                    bool in_string = cur && (cur->kind == maiz::TokenKind::String ||
                                             cur->kind == maiz::TokenKind::Invalid) &&
                                     caret > cur->begin;
                    if (in_string) {
                        std::string kw = prev ? prev->text : "";
                        cs.replace_begin = cur->begin + 1;
                        cs.replace_end = (cur->kind == maiz::TokenKind::String &&
                                          cur->end > cur->begin + 1)
                                             ? cur->end - 1
                                             : cur->end;
                        std::string typed = cur->text;

                        if (kw == "tag" || kw == "has") {
                            std::map<std::string, int> counts;
                            for (const auto& sub : subjects)
                                for (const auto& t : sub.tags) ++counts[t];
                            for (const auto& [t, n] : counts)
                                add(t, std::to_string(n) + " subject" + (n == 1 ? "" : "s"));
                        } else if (kw == "kind" || kw == "glyph") {
                            std::map<std::string, int> counts;
                            for (const auto& sub : subjects) ++counts[sub.kind];
                            for (const auto& [k, n] : counts)
                                add(k, std::to_string(n) + " subject" + (n == 1 ? "" : "s"));
                        } else if (kw == "name" || kw == "rune") {
                            for (const auto& sub : subjects) add(sub.name, sub.kind);
                        } else if (kw == "mantle") {
                            add("playground", "mantle");
                            add("scripts", "mantle");
                        } else if (kw == "device") {
                            for (const char* d : {"pointer", "touch", "pen",
                                                  "gamepad", "voice", "xr"})
                                add(d, "modality");
                        } else if (kw == "with") {
                            for (const auto& a : ugraph.affordances())
                                add(a.id, a.kind + ", x" + std::to_string(a.touches));
                        }
                        keep_prefix(typed);
                        return cs;
                    }

                    // ── just after a CONDITION word: the next thing is a value ─
                    // `when tag ` must not offer `tag` again. What follows a
                    // condition keyword is always a quoted argument, so offer
                    // the real ones, already quoted and ready to accept.
                    {
                        std::string kw = prev ? prev->text : "";
                        bool cond_kw = kw == "tag" || kw == "has" || kw == "kind" ||
                                       kw == "glyph" || kw == "name" || kw == "rune" ||
                                       kw == "mantle" || kw == "device" || kw == "with" ||
                                       (!kw.empty() && preds.find(kw) != nullptr);
                        if (cond_kw && (cur == nullptr ||
                                        cur->kind != maiz::TokenKind::String)) {
                            cs.replace_begin = cs.replace_end = caret;
                            if (cur && cur->kind == maiz::TokenKind::Ident) {
                                cs.replace_begin = cur->begin;
                                cs.replace_end = cur->end;
                            }
                            auto q = [&](const std::string& v, const std::string& d) {
                                cs.items.push_back({"\"" + v + "\"", "", d});
                            };
                            if (kw == "tag" || kw == "has") {
                                std::map<std::string, int> counts;
                                for (const auto& sub : subjects)
                                    for (const auto& t : sub.tags) ++counts[t];
                                for (const auto& [t, n] : counts)
                                    q(t, std::to_string(n) + " subject" + (n == 1 ? "" : "s"));
                            } else if (kw == "kind" || kw == "glyph") {
                                std::map<std::string, int> counts;
                                for (const auto& sub : subjects) ++counts[sub.kind];
                                for (const auto& [k, n] : counts)
                                    q(k, std::to_string(n) + " subject" + (n == 1 ? "" : "s"));
                            } else if (kw == "name" || kw == "rune") {
                                for (const auto& sub : subjects) q(sub.name, sub.kind);
                            } else if (kw == "mantle") {
                                q("playground", "mantle");
                                q("scripts", "mantle");
                            } else if (kw == "device") {
                                for (const char* d : {"pointer", "touch", "pen",
                                                      "gamepad", "voice", "xr"})
                                    q(d, "modality");
                            } else if (kw == "with") {
                                for (const auto& a : ugraph.affordances())
                                    q(a.id, a.kind);
                            } else {
                                q("", "argument for `" + kw + "`");
                            }
                            keep_prefix(typed_word());
                            return cs;
                        }
                    }

                    // ── a VALUE position: offer something usable, not blank ──
                    // After `color`, Tab should hand you a real literal you can
                    // immediately click and edit, not an empty pair of quotes.
                    // Half the point of the colour wheel is that you never type
                    // a hex code by hand, so completion should not make you.
                    if (prev && prev->kind == maiz::TokenKind::Ident &&
                        (cur == nullptr || cur->kind != maiz::TokenKind::String)) {
                        cs.replace_begin = cs.replace_end = caret;
                        if (cur && cur->kind == maiz::TokenKind::Ident) {
                            cs.replace_begin = cur->begin;
                            cs.replace_end = cur->end;
                        }
                        if (prev->text == "color") {
                            add("\"#ffffff\"", "white - click it to pick");
                            add("\"#f85149\"", "red");
                            add("\"#58a6ff\"", "blue");
                            add("\"#3fb950\"", "green");
                            return cs;
                        }
                        if (prev->text == "weight" || prev->text == "glow") {
                            add("1", "number");
                            add("2", "number");
                            return cs;
                        }
                    }

                    // ── a bare word: keywords, predicates, or properties ─────
                    std::string typed;
                    if (cur && cur->kind != maiz::TokenKind::String) {
                        typed = cur->text;
                        cs.replace_begin = cur->begin;
                        cs.replace_end = cur->end;
                    } else {
                        cs.replace_begin = cs.replace_end = caret;
                    }

                    std::string before = prev ? prev->text : "";

                    // Does this line already have a `when`? That single fact is
                    // the whole "does the next word make sense" question — you
                    // never write `when when`, so `when` is offered only where
                    // a rule can actually begin.
                    size_t line_start = src.rfind('\n', caret ? caret - 1 : 0);
                    line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;
                    std::string_view line_so_far =
                        src.substr(line_start, caret - line_start);
                    bool line_has_when =
                        line_so_far.find("when") != std::string_view::npos;
                    bool line_has_define =
                        line_so_far.find("define") != std::string_view::npos;

                    bool after_arrow = prev && (prev->kind == maiz::TokenKind::Arrow ||
                                                before == "then" || before == ",");
                    if (after_arrow) {
                        // Effect position: properties are DISCOVERED, not listed.
                        //
                        // There is no hardcoded vocabulary here. A property
                        // exists because some rule somewhere writes it — using
                        // it IS declaring it — so the list is harvested from
                        // every parsed script's AST plus whatever is currently
                        // derived. Add `sound` to one rule and it completes
                        // everywhere from then on, with no registration step.
                        //
                        // What CANNOT be discovered is what a property means:
                        // its merge law and its rendering stay declared. That is
                        // the split — discover the vocabulary, declare the
                        // interactions (okf/concepts/allomone/discovery.md).
                        std::map<std::string, int> props;
                        for (const auto& pr : parsed)
                            for (const auto& r : pr.second.rules)
                                for (const auto& e : r.effects) ++props[e.property];
                        for (const auto& c : merged.cells) props[c.property] += 0;
                        // A property a glyph also DECLARES as a field is worth
                        // flagging here rather than after the confusion: writing
                        // it derives an annotation, it does not set the field.
                        for (const auto& [field, glyphs] : glyph_fields)
                            if (!props.count(field)) props[field] = 0;
                        for (const auto& [name, uses] : props) {
                            std::string d = uses ? std::to_string(uses) + " rule" +
                                                       (uses == 1 ? "" : "s")
                                                 : "in use";
                            maiz::Lattice law = mopts.law_for(name);
                            d += ", " + std::string(maiz::lattice_name(law));
                            auto f = glyph_fields.find(name);
                            if (f != glyph_fields.end())
                                d += " - also a field on " +
                                     std::to_string(f->second.size()) + " glyph" +
                                     (f->second.size() == 1 ? "" : "s");
                            add(name, d);
                        }
                    } else if (!line_has_when && !line_has_define) {
                        add("when", "start a rule");
                        add("define", "name a condition, to reuse it");
                    } else if (before == "when" || before == "and" || before == "not" ||
                               before == "=" || cur == nullptr) {
                        for (const char* k : {"tag", "has", "kind", "glyph", "name",
                                              "rune", "mantle", "device", "with"})
                            add(k, "condition");
                        if (before != "not") add("all", "match everything");
                        if (before != "not") add("not", "negate");
                        for (const auto& e : preds.entries)
                            add(e.first, "host predicate");

                        // Definitions in THIS script. Script-local on purpose —
                        // cross-script definitions would make parsing one script
                        // depend on another, and allo_parse being a pure
                        // function of text is what keeps a foreign script
                        // readable. So the list comes from the buffer itself.
                        for (const maiz::Definition& d :
                             maiz::allo_parse("", src).definitions) {
                            if (!d.usable) continue;
                            const size_t n = d.expanded.size();
                            add(d.name + (d.params.empty() ? "()" : "("),
                                "define, " + std::to_string(n) + " term" +
                                    (n == 1 ? "" : "s") +
                                    (d.params.empty()
                                         ? ""
                                         : ", " + std::to_string(d.params.size()) +
                                               " arg"));
                        }
                    } else {
                        add("then", "begin effects");
                        add("and", "another condition");
                    }
                    keep_prefix(typed);
                    return cs;
                };

                // Right-click any word to be told what it is. Completion tells
                // you a word EXISTS; this tells you what it does — which is the
                // difference between a vocabulary you can type and one you can
                // learn.
                eo.explain = [&](std::string_view src, size_t off) -> std::string {
                    for (const maiz::Token& t : maiz::allo_tokens(src)) {
                        if (t.kind == maiz::TokenKind::End) break;
                        if (off < t.begin || off > t.end) continue;
                        const std::string& w = t.text;

                        if (w == "when")   return "when — begins a rule. A script is a SET of rules;\nmoving a line changes nothing.";
                        if (w == "define") return "define NAME(args) = <condition> — names a condition\nso it can be reused. Expanded where it is called, so its\nterms count toward strength: a call cannot make a rule\nbroader than it looks. Script-local, and order-free.";
                        if (w == "then")   return "then — separates the condition from the effects.\n(`->` is an accepted older spelling.)";
                        if (w == "and")    return "and — another condition. Every term must hold.\nEach one also adds 1 to the rule's strength.";
                        if (w == "not")    return "not — negates the next condition.";
                        if (w == "all")    return "all — matches every subject, at strength 0.\nThe broad default a sharper rule overrides.";
                        if (w == "tag" || w == "has")
                            return "tag / has \"x\" — the subject carries tag x.\n`has` is Void Core's spelling; identical meaning.";
                        if (w == "kind" || w == "glyph")
                            return "kind / glyph \"x\" — the subject's type.\n`glyph` is Void Core's word for it.";
                        if (w == "name" || w == "rune")
                            return "name / rune \"x\" — addresses one subject by name.\n`rune` is Void Core's spelling.";
                        if (w == "mantle") return "mantle \"x\" — the subject lives in mantle x.";
                        if (w == "device") return "device \"x\" — how the subject was last REACHED\n(pointer/touch/pen/gamepad/voice/xr). Reads the user graph.";
                        if (w == "with")   return "with \"x\" — coherent with x in YOUR work: you touched\nthem in the same frame. Reads the user graph, not the data.";

                        if (t.kind == maiz::TokenKind::Ident) {
                            // A definition in this script explains itself by
                            // showing what it becomes — the expansion IS the
                            // documentation, and it cannot go stale.
                            for (const maiz::Definition& d :
                                 maiz::allo_parse("", src).definitions) {
                                if (d.name != w) continue;
                                std::string out = "`" + d.name + "(";
                                for (size_t i = 0; i < d.params.size(); ++i)
                                    out += (i ? ", " : "") + d.params[i];
                                out += ")` — a definition in this script.\n";
                                if (!d.usable)
                                    return out + "It could not be expanded; see the diagnostics.";
                                out += "Expands to " + std::to_string(d.expanded.size()) +
                                       " term(s), so a call adds that much strength:\n  ";
                                for (size_t i = 0; i < d.expanded.size(); ++i) {
                                    const maiz::DefItem& it = d.expanded[i];
                                    if (i) out += " and ";
                                    if (it.negated) out += "not ";
                                    out += it.kind == maiz::Term::Kind::Host
                                               ? it.name
                                               : std::string(term_word(it.kind));
                                    out += " " + std::string(it.arg.is_param ? "" : "\"") +
                                           it.arg.text + (it.arg.is_param ? "" : "\"");
                                }
                                return out;
                            }
                            if (preds.find(w))
                                return "`" + w + "` — a host predicate this application registered.\nThe library carries the name; the app computes the answer.";
                            // A property explains itself from what is DISCOVERED
                            // — its merge law, and whether it collides with a
                            // field some glyph declares — rather than from a
                            // hardcoded list of four.
                            {
                                auto f = glyph_fields.find(w);
                                bool derived = false;
                                for (const auto& pr : parsed)
                                    for (const auto& r : pr.second.rules)
                                        for (const auto& e : r.effects)
                                            if (e.property == w) derived = true;
                                if (derived || f != glyph_fields.end()) {
                                    std::string out = "`" + w + "` — a property. Merge law: " +
                                                      std::string(maiz::lattice_name(
                                                          mopts.law_for(w))) + ".\n";
                                    if (f != glyph_fields.end()) {
                                        out += "ALSO a field declared by: ";
                                        int i = 0;
                                        for (const auto& g : f->second)
                                            out += (i++ ? ", " : "") + g;
                                        out += ".\nDeriving it does NOT write that field — Allomone is\n"
                                               "derive-only. This app renders the derived value in\n"
                                               "preference to the stored one, so disabling the script\n"
                                               "reverts to the field with nothing to undo.";
                                    } else {
                                        out += "Purely derived: no glyph declares a field by this name.\n"
                                               "The library never learns what it means; this app does.";
                                    }
                                    return out;
                                }
                            }
                            return "`" + w + "` — not a keyword here. In condition position it\nparses as a host predicate; unregistered ones never match.";
                        }
                        if (t.kind == maiz::TokenKind::String && is_hex_color(w))
                            return "a colour literal. Click it for the wheel.\n#rrggbb, bare rrggbb and #rgb are all accepted.";
                    }
                    return {};
                };

                // Hovering a rule previews WHICH subjects it refers to, without
                // deriving anything — allo_matches, straight off the AST.
                eo.hover = [&](std::string_view src, size_t off) -> std::string {
                    maiz::Script s = maiz::allo_parse("preview", src);
                    int line = 0;
                    for (size_t i = 0; i < off && i < src.size(); ++i)
                        if (src[i] == '\n') ++line;
                    for (const maiz::Rule& r : s.rules) {
                        if (r.line != line) continue;
                        std::string hit;
                        int n = 0;
                        for (const auto& sub : subjects) {
                            if (!maiz::allo_matches(r, sub, &preds)) continue;
                            if (n < 6) hit += (n ? ", " : "") + sub.id;
                            ++n;
                        }
                        if (!n) return "matches nothing right now";
                        return std::to_string(n) + " subject(s): " + hit +
                               (n > 6 ? ", ..." : "") +
                               "\nstrength " + std::to_string(r.strength());
                    }
                    return {};
                };

                float bottom_h = 190.0f;
                maiz::CodeEditorIO cio =
                    maiz::code_editor("allo", editor, eo, 0,
                                      ImGui::GetContentRegionAvail().y - bottom_h);
                // ONE `set` per gesture, on commit — never per keystroke.
                if ((cio.commit || apply_now) && editor.dirty && snode) {
                    // Snapshot the derived state BEFORE the edit lands, so the
                    // "edit delta" panel can show what this change actually did.
                    // Free, because merge is pure and we already have it.
                    before_edit = merged;
                    before_edit_script = selected_script;
                    have_before_edit = true;

                    dispatch_in("scripts",
                                "set " + selected_script + " source " + cmd_arg(editor.text));
                    editor.clear_dirty();
                    touch("script:" + selected_script, "script");
                }

                ImGui::BeginChild("##under", ImVec2(0, 0), ImGuiChildFlags_Borders);
                if (ImGui::BeginTabBar("##under-tabs")) {
                    if (ImGui::BeginTabItem("diagnostics")) {
                        const maiz::Script* ps = nullptr;
                        for (const auto& p : parsed)
                            if (p.first == selected_script) ps = &p.second;
                        if (!ps || ps->ok()) ImGui::TextDisabled("no diagnostics");
                        else
                            for (const auto& d : ps->diagnostics)
                                ImGui::TextColored(ImVec4(1, 0.4f, 0.35f, 1), "line %d: %s",
                                                   d.line + 1, d.message.c_str());
                        if (ps) {
                            ImGui::TextDisabled("%d rule(s) parsed, %d definition(s)",
                                                (int)ps->rules.size(),
                                                (int)ps->definitions.size());
                            for (const maiz::Definition& d : ps->definitions)
                                if (!d.usable)
                                    ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1),
                                                       "  `%s` is unusable - calls to it "
                                                       "kill their rule",
                                                       d.name.c_str());
                        }

                        // SHADOWED PROPERTIES — the third vocabulary earning its
                        // keep. Not an error: writing a property that a glyph
                        // also declares is legitimate, and this app deliberately
                        // renders the derived value first. But "I set label and
                        // the rune's label did not change" is a confusion worth
                        // pre-empting, because derive-only is the whole point.
                        if (ps) {
                            std::vector<std::string> shadowed;
                            for (const auto& r : ps->rules)
                                for (const auto& e : r.effects)
                                    if (glyph_fields.count(e.property) &&
                                        std::find(shadowed.begin(), shadowed.end(),
                                                  e.property) == shadowed.end())
                                        shadowed.push_back(e.property);
                            if (!shadowed.empty()) {
                                ImGui::Separator();
                                for (const std::string& p : shadowed) {
                                    std::string glyphs;
                                    for (const auto& g : glyph_fields[p])
                                        glyphs += (glyphs.empty() ? "" : ", ") + g;
                                    ImGui::TextColored(
                                        ImVec4(0.7f, 0.75f, 1, 1),
                                        "`%s` shadows a field on: %s", p.c_str(),
                                        glyphs.c_str());
                                }
                                ImGui::TextDisabled(
                                    "deriving a property NEVER writes the rune's field.");
                                ImGui::TextDisabled(
                                    "this app renders the derived value first, so disabling");
                                ImGui::TextDisabled(
                                    "the script reverts to the field - nothing to undo.");
                            }
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("derived")) {
                        if (ImGui::BeginTable("##d", 5, ImGuiTableFlags_Borders |
                                                            ImGuiTableFlags_RowBg |
                                                            ImGuiTableFlags_ScrollY)) {
                            ImGui::TableSetupColumn("subject");
                            ImGui::TableSetupColumn("property");
                            ImGui::TableSetupColumn("value");
                            ImGui::TableSetupColumn("from");
                            ImGui::TableSetupColumn("str");
                            ImGui::TableHeadersRow();
                            for (const auto& c : merged.cells) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                // Any cell can be asked to explain itself, not
                                // only a broken one.
                                ImGui::PushID(&c);
                                const bool open = ImGui::TreeNodeEx(
                                    c.subject.c_str(),
                                    ImGuiTreeNodeFlags_SpanAvailWidth);
                                ImGui::TableNextColumn(); ImGui::TextUnformatted(c.property.c_str());
                                ImGui::TableNextColumn();
                                if (c.conflicted)
                                    ImGui::TextColored(ImVec4(1, 0.55f, 0.2f, 1), "TOP (conflict)");
                                else ImGui::TextUnformatted(c.value.c_str());
                                ImGui::TableNextColumn();
                                // Source, and WHERE in it — a derived value can
                                // now point at the line that produced it.
                                if (c.resolved) ImGui::TextDisabled("(resolved)");
                                else if (c.origin.empty())
                                    ImGui::TextDisabled("%s", c.source.c_str());
                                else
                                    ImGui::TextDisabled("%s (%s)", c.source.c_str(),
                                                        c.origin.c_str());
                                ImGui::TableNextColumn(); ImGui::Text("%d", c.strength);
                                if (open) {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::Indent();
                                    inspector(c);
                                    ImGui::Unindent();
                                    ImGui::TreePop();
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("conflicts")) {
                        std::vector<const maiz::MergedCell*> cf = merged.conflicts();
                        if (cf.empty())
                            ImGui::TextDisabled(
                                "nothing to settle. enable `rival` to create a real one.");
                        for (const maiz::MergedCell* c : cf) {
                            ImGui::PushID(c);
                            ImGui::Text("%s . %s", c->subject.c_str(), c->property.c_str());
                            ImGui::Indent();
                            inspector(*c);
                            ImGui::Unindent();
                            ImGui::PopID();
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("weaver")) {
                        // THE RESPONSE. The system reads what you wrote, finds
                        // the asymmetries, and OFFERS rules. It never writes one
                        // — a rule set that generates rules is a Knuth-Bendix
                        // completion, and completion diverges
                        // (okf/concepts/allomone/materialization.md). So each
                        // proposal is a button a person presses.
                        std::vector<maiz::Script> all;
                        for (const auto& pr : parsed) all.push_back(pr.second);
                        std::vector<maiz::Proposal> props =
                            maiz::propose(merged, all, subjects);

                        ImGui::TextDisabled(
                            "%d proposal(s) - the engine finds what needs deciding; "
                            "you decide it.", (int)props.size());
                        ImGui::Separator();
                        if (props.empty())
                            ImGui::TextDisabled(
                                "  nothing to say: no conflicts, every tag is heard, "
                                "no coverage holes.\n"
                                "  enable `rival` to create a real disagreement, or "
                                "tag a control with\n  something no rule mentions.");

                        for (size_t i = 0; i < props.size(); ++i) {
                            const maiz::Proposal& pr = props[i];
                            ImGui::PushID((int)i);
                            ImVec4 tint = pr.kind == "settle"  ? ImVec4(1, .55f, .2f, 1)
                                        : pr.kind == "unheard" ? ImVec4(.5f, .75f, 1, 1)
                                                               : ImVec4(.6f, .8f, .5f, 1);
                            ImGui::TextColored(tint, "[%s]", pr.kind.c_str());
                            ImGui::SameLine();
                            ImGui::TextWrapped("%s", pr.rationale.c_str());
                            ImGui::Indent();
                            ImGui::TextColored(ImVec4(.82f, .65f, 1, 1), "%s",
                                               pr.line.c_str());
                            // Accepting inserts into the BUFFER, not the model:
                            // it becomes an ordinary staged edit the author can
                            // read, change, or abandon before applying.
                            if (ImGui::SmallButton("insert into editor")) {
                                if (!editor.text.empty() && editor.text.back() != '\n')
                                    editor.text += "\n";
                                editor.text += pr.line + "\n";
                                editor.dirty = true;
                                editor.caret = editor.anchor = editor.text.size();
                                touch("weaver:" + pr.kind, "proposal");
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("copy")) ImGui::SetClipboardText(pr.line.c_str());
                            ImGui::Unindent();
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("edit delta")) {
                        // What did my last change actually DO? The derived
                        // state before the edit, diffed against now. This is
                        // the script change made visible in the output it
                        // caused — the same diff() the counterfactual uses,
                        // pointed at time instead of at absence.
                        if (!have_before_edit) {
                            ImGui::TextDisabled(
                                "edit a script and apply it - the resulting change "
                                "in derived state shows up here.");
                        } else {
                            std::vector<maiz::CellChange> ch =
                                maiz::diff(before_edit, merged);
                            ImGui::Text("last applied edit: %s", before_edit_script.c_str());
                            ImGui::SameLine();
                            ImGui::TextDisabled("- %d cell(s) changed", (int)ch.size());
                            ImGui::Separator();
                            if (ch.empty())
                                ImGui::TextDisabled(
                                    "  the edit changed no derived value "
                                    "(a comment, or a rule that matches nothing)");
                            for (const auto& c : ch) {
                                ImGui::BulletText("%s.%s", c.subject.c_str(),
                                                  c.property.c_str());
                                ImGui::SameLine();
                                ImGui::TextDisabled(
                                    "%s  ->  %s",
                                    c.was_conflicted ? "(conflict)"
                                    : c.before.empty() ? "(none)" : c.before.c_str(),
                                    c.now_conflicted ? "(conflict)"
                                    : c.after.empty() ? "(none)" : c.after.c_str());
                            }
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("effect graph")) {
                        // The Sentinel's static check: can rules feed each other
                        // in a loop? Decidable and cheap, unlike "does this
                        // terminate" which is not
                        // (okf/concepts/allomone/emergence.md).
                        std::vector<maiz::Script> all;
                        for (const auto& pr : parsed) all.push_back(pr.second);
                        maiz::EffectGraph eg = maiz::analyze(all, &preds);

                        if (eg.acyclic())
                            ImGui::TextColored(ImVec4(0.35f, 0.8f, 0.4f, 1), "%s",
                                               maiz::effect_verdict(eg).c_str());
                        else
                            ImGui::TextColored(ImVec4(1, 0.35f, 0.3f, 1), "%s",
                                               maiz::effect_verdict(eg).c_str());
                        ImGui::TextDisabled(
                            "derive-only cannot cycle: effects are prop:*, conditions never are.");
                        ImGui::TextDisabled(
                            "this stays green until a write tier lands - which is the point.");
                        ImGui::Separator();

                        for (const auto& n : eg.nodes) {
                            ImGui::PushID(n.id.c_str());
                            if (ImGui::TreeNode(n.id.c_str())) {
                                std::string r, w;
                                for (const auto& s : n.reads) r += (r.empty() ? "" : ", ") + s;
                                for (const auto& s : n.writes) w += (w.empty() ? "" : ", ") + s;
                                ImGui::TextDisabled("reads : %s", r.empty() ? "(nothing)" : r.c_str());
                                ImGui::TextDisabled("writes: %s", w.empty() ? "(nothing)" : w.c_str());
                                ImGui::TreePop();
                            }
                            ImGui::PopID();
                        }
                        if (!eg.edges.empty()) {
                            ImGui::Separator();
                            ImGui::TextDisabled("dependencies");
                            for (const auto& e : eg.edges)
                                ImGui::BulletText("%s -> %s  (via %s)", e.from.c_str(),
                                                  e.to.c_str(), e.via.c_str());
                        }

                        // THE ONE PLACE THE CANVAS IS THE RIGHT TOOL. An AST is
                        // not a node graph (and the AST-overlay is explicitly
                        // not planned for that reason), but a
                        // script-depends-on-script graph genuinely is one — and
                        // the LAYOUT explains: columns are strata, so
                        // left-to-right is the evaluation order acyclicity
                        // buys, and a cycle collapses into one column because
                        // it cannot be ordered at all.
                        //
                        // `draw_canvas`, NOT `edit_canvas`: this graph is
                        // DERIVED from the scripts' text, so there is nothing
                        // here a person could meaningfully drag or link.
                        // Offering gestures whose commands could not be
                        // honoured is worse than offering none.
                        ImGui::Separator();
                        ImGui::TextDisabled(
                            "columns are strata - left to right IS the evaluation order.");
                        ImGui::TextDisabled(
                            "read-only: the graph is derived, so there is nothing to drag.");
                        maiz::Scene es = maiz::effect_scene(eg);
                        if (!es.nodes.empty()) {
                            ImGui::BeginChild("##eg", ImVec2(0, 220), true);
                            maiz::draw_canvas("effect-graph", es, effect_cam);
                            ImGui::EndChild();
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("interactions")) {
                        // A DIFFERENT graph from the one next door. That one is
                        // directed and asks "does A feed B?"; this one is
                        // undirected and asks "do A and B MEET?" — same subjects,
                        // same property (okf/concepts/allomone/discovery.md §2).
                        //
                        // Its reason for existing: today an interaction is only
                        // visible when it produces a conflict. A rule silently
                        // overridden by a sharper one leaves no trace anywhere,
                        // and that is the one outcome that discards something
                        // somebody wrote.
                        std::vector<maiz::Script> all;
                        for (const auto& pr : parsed) all.push_back(pr.second);
                        std::vector<maiz::Interaction> ix =
                            maiz::interactions(all, subjects, mopts, &preds);

                        int overrides = 0, conflicts = 0;
                        for (const auto& i : ix) {
                            if (i.outcome == "override") ++overrides;
                            if (i.outcome == "conflict") ++conflicts;
                        }
                        ImGui::Text("%d meeting(s): %d override, %d conflict",
                                    (int)ix.size(), overrides, conflicts);
                        ImGui::TextDisabled(
                            "an OVERRIDE is the interesting one - it silently discards");
                        ImGui::TextDisabled(
                            "something an author wrote. inside one script that is the");
                        ImGui::TextDisabled(
                            "defaults-and-exceptions idiom; across scripts it may not be.");
                        ImGui::Separator();

                        if (ix.empty())
                            ImGui::TextDisabled("no two rules touch the same cell.");
                        for (const auto& i : ix) {
                            ImVec4 col =
                                i.outcome == "conflict"  ? ImVec4(1, 0.55f, 0.2f, 1)
                                : i.outcome == "override" ? ImVec4(0.95f, 0.85f, 0.4f, 1)
                                : i.outcome == "blend"    ? ImVec4(0.7f, 0.6f, 1, 1)
                                                          : ImVec4(0.55f, 0.6f, 0.6f, 1);
                            ImGui::TextColored(col, "%s", maiz::interaction_line(i).c_str());
                            if (i.same_script) {
                                ImGui::SameLine();
                                ImGui::TextDisabled("(same script - intended)");
                            }
                        }
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("influence")) {
                        // The Sentinel's read-only instrument: which script is
                        // actually doing the work? One pass over the merge —
                        // no re-derivation (okf/concepts/allomone/governance.md).
                        ImGui::TextDisabled(
                            "concentration %.2f  (1.0 = one script decides everything)"
                            "   |   conflicts %d / %d cells",
                            maiz::influence_concentration(merged),
                            (int)merged.conflicts().size(), (int)merged.cells.size());
                        ImGui::Separator();
                        if (ImGui::BeginTable("##inf", 6,
                                              ImGuiTableFlags_Borders |
                                                  ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("source");
                            ImGui::TableSetupColumn("share");
                            ImGui::TableSetupColumn("won");
                            ImGui::TableSetupColumn("offered");
                            ImGui::TableSetupColumn("overridden");
                            ImGui::TableSetupColumn("in conflict");
                            ImGui::TableHeadersRow();
                            for (const auto& i : maiz::influence(merged)) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(i.source.c_str());
                                ImGui::TableNextColumn(); ImGui::Text("%.0f%%", i.share * 100.0);
                                ImGui::TableNextColumn(); ImGui::Text("%d", i.cells_won);
                                ImGui::TableNextColumn(); ImGui::Text("%d", i.cells_offered);
                                ImGui::TableNextColumn(); ImGui::Text("%d", i.cells_overridden);
                                ImGui::TableNextColumn(); ImGui::Text("%d", i.cells_conflicted);
                            }
                            ImGui::EndTable();
                        }
                        ImGui::Separator();
                        // The counterfactual: re-merge without the selected
                        // script and diff. Cheap because merge is pure.
                        ImGui::TextDisabled("if `%s` vanished:", selected_script.c_str());
                        std::vector<maiz::ConstraintMap> without;
                        for (const auto& mp : maps)
                            if (mp.id != selected_script) without.push_back(mp);
                        std::vector<maiz::CellChange> ch =
                            maiz::diff(merged, maiz::merge(without, mopts));
                        if (ch.empty()) ImGui::TextDisabled("  nothing would change");
                        for (const auto& c : ch)
                            ImGui::BulletText("%s.%s : %s -> %s", c.subject.c_str(),
                                              c.property.c_str(),
                                              c.was_conflicted ? "(conflict)"
                                                               : c.before.c_str(),
                                              c.now_conflicted ? "(conflict)"
                                              : c.after.empty() ? "(none)"
                                                                : c.after.c_str());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("settings")) {
                        // Testing knobs. Everything here is VIEW state — it is
                        // never dispatched and never persisted, because none of
                        // it passes the save/quit/reload razor.
                        ImGui::TextDisabled("editor");
                        ImGui::SliderFloat("zoom", &editor.zoom, 0.25f, 3.0f, "%.2fx");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("reset")) editor.zoom = 1.0f;
                        ImGui::SliderFloat("LOD threshold (px/line)", &lod_threshold,
                                           3.0f, 20.0f, "%.0f");
                        ImGui::TextDisabled(
                            "  below this line height, tokens draw as coloured bars");

                        ImGui::Separator();
                        ImGui::TextDisabled("user graph");
                        ImGui::SliderFloat("frame idle (s)", &frame_idle_limit, 0.5f, 15.0f,
                                           "%.1f");
                        ImGui::TextDisabled(
                            "  how long a lull ends a working frame. host POLICY may\n"
                            "  consult a clock; the graph it writes into keeps no time.");

                        ImGui::Separator();
                        ImGui::TextDisabled("conflict policy");
                        ImGui::Checkbox("blend colours instead of disputing", &blend_colors_on);
                        ImGui::TextDisabled(
                            "  declares `color` a Custom join: contributing colours\n"
                            "  are averaged in OKLab, so red + blue really is purple.\n"
                            "  the mean is over the whole SET at once - a pairwise\n"
                            "  fold would not be associative, and order would leak in.");
                        ImGui::Checkbox("settle ties by recency", &recency_policy);
                        ImGui::TextDisabled(
                            "  off: a genuine tie is TOP and a human decides (default).\n"
                            "  on : the newest script wins, and the cell says so.\n"
                            "  strength still decides first either way - this only\n"
                            "  governs what happens when two sources tie exactly.");

                        ImGui::Separator();
                        ImGui::TextDisabled("merge laws (host vocabulary)");
                        ImGui::BulletText("weight = Sum   (contributions add)");
                        ImGui::BulletText("glow   = Max   (loudest wins)");
                        ImGui::BulletText("everything else = Unique (disagreement -> conflict)");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("user graph")) {
                        ImGui::Text("device:");
                        ImGui::SameLine();
                        /* The six spellings a GUI uses, offered as a
                         * convenience — `channel` is an open string since
                         * 2026-08-29, so this list is a vocabulary and not a
                         * type. A non-GUI host writes its own. */
                        const char* devs[] = {"pointer", "touch", "pen", "gamepad", "voice", "xr"};
                        int di = 0;
                        for (int k = 0; k < 6; ++k) if (channel == devs[k]) di = k;
                        ImGui::SetNextItemWidth(120);
                        if (ImGui::Combo("##dev", &di, devs, 6)) channel = devs[di];
                        ImGui::SameLine(0, 20);
                        ImGui::TextDisabled("frame: %s", frame_open ? "open" : "closed");
                        ImGui::SameLine(0, 20);
                        if (ImGui::SmallButton("materialize")) {
                            // Ephemera become model content only when asked —
                            // and then Allomone can query them like any data.
                            if (frame_open) { ugraph.close_frame(); frame_open = false; }
                            dispatch_all(ugraph.compile("usergraph"));
                            dispatch("use playground");
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("clear")) { ugraph.clear(); frame_open = false; }

                        ImGui::Separator();
                        ImGui::TextDisabled("affordances");
                        for (const auto& a : ugraph.affordances())
                            ImGui::BulletText("%s  (%s, %s) x%d", a.id.c_str(), a.kind.c_str(),
                                              a.channel.c_str(),
                                              a.touches);
                        ImGui::TextDisabled("coherence - symmetric, no order, no time");
                        for (const auto& e : ugraph.coincidences())
                            ImGui::BulletText("%s - %s  x%d", e.a.c_str(), e.b.c_str(), e.weight);
                        ImGui::TextDisabled("frames (hyperedges)");
                        for (const auto& f : ugraph.frames()) {
                            std::string m;
                            for (size_t i = 0; i < f.members.size(); ++i)
                                m += (i ? " + " : "") + f.members[i];
                            ImGui::BulletText("{%s} x%d", m.c_str(), f.weight);
                        }
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndChild();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // ══ TAB 2 — Playground ══════════════════════════════════════════
            if (ImGui::BeginTabItem("Playground")) {
                ImGui::TextDisabled(
                    "right-click any control to add/subtract tags - the scripts re-derive instantly");
                ImGui::Separator();

                int col = 0;
                for (const auto& n : pg.nodes) {
                    std::string label = merged.value(n.name, "label");
                    if (label.empty()) label = field_of(n, "label");
                    if (label.empty()) label = n.name;

                    unsigned rgb = parse_rgb(merged.value(n.name, "color"), 0x555a61);
                    const maiz::MergedCell* ccell = merged.find(n.name, "color");
                    bool conflicted = ccell && ccell->conflicted;
                    double weight = std::strtod(merged.value(n.name, "weight").c_str(), nullptr);
                    double glow = std::strtod(merged.value(n.name, "glow").c_str(), nullptr);

                    if (col++ % 3) ImGui::SameLine();
                    ImGui::PushID(n.name.c_str());
                    ImGui::BeginGroup();

                    // A conflicted cell is rendered as a QUESTION, never as a
                    // guess — merged.value() already refused to hand one over.
                    if (conflicted) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.14f, 0.05f, 1));
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.30f, 0.14f, 0.05f, 1));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, to_vec4(rgb, 0.85f));
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, to_vec4(rgb, 0.55f));
                    }
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_vec4(rgb, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_SliderGrab, to_vec4(rgb, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, to_vec4(rgb, 0.75f));
                    if (glow > 0)
                        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));

                    // `weight` is the summed lattice: contributions ADD, so two
                    // scripts each asking for weight both get their say.
                    float w = 190.0f + 26.0f * (float)weight;
                    float h = ImGui::GetFrameHeight() * (1.0f + 0.22f * (float)weight);

                    if (n.glyph == "button") {
                        if (ImGui::Button(label.c_str(), ImVec2(w, h))) touch(n.name, "control");
                    } else if (n.glyph == "toggle") {
                        bool on = field_of(n, "value") == "true";
                        ImGui::SetNextItemWidth(w);
                        if (ImGui::Checkbox(label.c_str(), &on)) {
                            dispatch("setjson " + n.name + " value " + (on ? "true" : "false"));
                            touch(n.name, "control");
                        }
                    } else if (n.glyph == "slider") {
                        float v = dragging == n.name
                                      ? drag_value
                                      : (float)std::strtod(field_of(n, "value").c_str(), nullptr);
                        ImGui::SetNextItemWidth(w);
                        if (ImGui::SliderFloat(label.c_str(), &v, 0.0f, 1.0f)) {
                            dragging = n.name; // stage locally...
                            drag_value = v;
                            touch(n.name, "control");
                        }
                        if (dragging == n.name && ImGui::IsItemDeactivatedAfterEdit()) {
                            char b[32];
                            std::snprintf(b, sizeof b, "%.3f", drag_value);
                            dispatch("setjson " + n.name + " value " + b); // ...ONE command
                            dragging.clear();
                        }
                    } else {
                        ImGui::SetNextItemWidth(w);
                        if (ImGui::BeginListBox(label.c_str(), ImVec2(w, h * 2.4f))) {
                            for (int i = 0; i < 3; ++i) {
                                char item[32];
                                std::snprintf(item, sizeof item, "%s %d", n.name.c_str(), i + 1);
                                if (ImGui::Selectable(item)) touch(n.name, "control");
                            }
                            ImGui::EndListBox();
                        }
                    }

                    if (glow > 0) ImGui::PopStyleColor();
                    ImGui::PopStyleColor(5);

                    // The tags, visible — this is what the rules match on.
                    std::string tagline;
                    for (const auto& t : n.tags) tagline += (tagline.empty() ? "" : " ") + t;
                    ImGui::TextDisabled("%s", tagline.empty() ? "(no tags)" : tagline.c_str());
                    if (conflicted) ImGui::TextColored(ImVec4(1, 0.55f, 0.2f, 1), "colour disputed (TOP)");

                    ImGui::EndGroup();
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        tag_target = n.name;
                        tag_buf[0] = 0;
                        ImGui::OpenPopup("##tags");
                    }
                    if (tag_target == n.name && ImGui::BeginPopup("##tags")) {
                        ImGui::TextDisabled("tags on %s", n.name.c_str());
                        ImGui::Separator();
                        for (const auto& t : n.tags) {
                            ImGui::PushID(t.c_str());
                            if (ImGui::SmallButton("-")) {
                                dispatch("tag " + n.name + " -" + t);
                                touch(n.name, "control");
                            }
                            ImGui::SameLine();
                            ImGui::TextUnformatted(t.c_str());
                            ImGui::PopID();
                        }
                        ImGui::Separator();
                        ImGui::SetNextItemWidth(140);
                        bool add = ImGui::InputText("##new", tag_buf, sizeof tag_buf,
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
                        ImGui::SameLine();
                        if ((ImGui::SmallButton("+ add") || add) && tag_buf[0]) {
                            dispatch("tag " + n.name + " +" + std::string(tag_buf));
                            touch(n.name, "control");
                            tag_buf[0] = 0;
                        }
                        ImGui::Separator();
                        ImGui::TextDisabled("try: danger, audio, transport, library, muted");
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        ImGui::Begin("Log", nullptr, ImGuiWindowFlags_NoCollapse);
        float footer = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##lines", ImVec2(0, -footer));
        maiz::draw_log_strip(log);
        ImGui::EndChild();
        maiz::CanvasIO bio = maiz::draw_command_bar(cmdbar);
        for (const auto& cmd : bio.commands) {
            maiz::Result r = dispatch(cmd);
            log.push_back({">", cmd, r.text().empty() ? (r.ok ? "ok" : "failed") : r.text()});
        }
        ImGui::End();

        // ── the click heatmap, drawn over everything ────────────────────────
        // Straight off the user graph: a region affordance's touch count IS the
        // heat. No separate store, no decay, no timestamps — the same numbers a
        // script reads with `with`.
        if (show_heatmap) {
            int hottest = 1;
            for (const auto& a : ugraph.affordances())
                if (a.kind == "region" && a.touches > hottest) hottest = a.touches;
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            for (const auto& a : ugraph.affordances()) {
                if (a.kind != "region") continue;
                int cx = 0, cy = 0;
                if (std::sscanf(a.id.c_str(), "cell:%d,%d", &cx, &cy) != 2) continue;
                float t = (float)a.touches / (float)hottest;
                fg->AddRectFilled(ImVec2(cx * heat_cell, cy * heat_cell),
                                  ImVec2((cx + 1) * heat_cell, (cy + 1) * heat_cell),
                                  IM_COL32(255, (int)(200 * (1.0f - t)), 40,
                                           (int)(30 + 90 * t)));
                char n[16];
                std::snprintf(n, sizeof n, "%d", a.touches);
                fg->AddText(ImVec2(cx * heat_cell + 4, cy * heat_cell + 2),
                            IM_COL32(255, 255, 255, 180), n);
            }
        }

        maiz::end_dockspace();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
