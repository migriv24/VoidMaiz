/* code.cpp — the from-scratch code editor with live token widgets.
 *
 * Layout assumes a MONOSPACE font (ImGui's built-in ProggyClean is one, and a
 * host that loads JetBrains Mono keeps the property). That is what makes
 * caret hit-testing an integer division instead of a per-glyph walk, and it is
 * the one assumption in here — everything else is byte offsets.
 *
 * Offsets are BYTES throughout. Caret motion steps over UTF-8 continuation
 * bytes so multi-byte text is never split mid-codepoint, but column arithmetic
 * counts bytes, so a line mixing wide characters will measure slightly off. The
 * honest trade for owning the layout; a proportional-font pass would replace
 * the whole measure path, not patch it. */
#include "voidmaiz/code.hpp"

#include "imgui.h"
#include "imgui_internal.h" // ImGui::GetCurrentWindow — for the item rect

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace maiz {

namespace {

constexpr size_t npos = std::string::npos;

bool is_cont(char c) { return ((unsigned char)c & 0xC0) == 0x80; }
bool word_char(char c) {
    return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '#';
}

size_t next_char(const std::string& s, size_t i) {
    if (i >= s.size()) return s.size();
    ++i;
    while (i < s.size() && is_cont(s[i])) ++i;
    return i;
}
size_t prev_char(const std::string& s, size_t i) {
    if (i == 0) return 0;
    --i;
    while (i > 0 && is_cont(s[i])) --i;
    return i;
}

struct Line {
    size_t begin = 0, end = 0; // end excludes the newline
};

std::vector<Line> split_lines(const std::string& s) {
    std::vector<Line> lines;
    size_t b = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == '\n') {
            lines.push_back({b, i});
            b = i + 1;
        }
    }
    return lines;
}

size_t line_of(const std::vector<Line>& lines, size_t off) {
    for (size_t i = 0; i < lines.size(); ++i)
        if (off <= lines[i].end) return i;
    return lines.empty() ? 0 : lines.size() - 1;
}

ImU32 to_col(unsigned rgb) {
    return IM_COL32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, 255);
}

/* Per-editor state ImGui owns for us: the undo stack and the drag flag. Keyed
 * by the widget's ImGui id so two editors never share one. */
struct Internal {
    std::vector<std::pair<std::string, size_t>> undo, redo;
    bool dragging = false;
    bool pending_commit = false;
};

Internal& internal_for(ImGuiID id) {
    static std::vector<std::pair<ImGuiID, Internal>> table;
    for (auto& e : table)
        if (e.first == id) return e.second;
    table.push_back({id, Internal{}});
    return table.back().second;
}

/* Permissive colour parsing: `#rrggbb`, bare `rrggbb`, `#rgb`. A host's span
 * detector decides WHAT is a colour; this only has to read whatever it claimed,
 * so being strict here just means a valid-looking token renders as black. */
bool read_hex_color(const std::string& s, unsigned& r, unsigned& g, unsigned& b) {
    std::string d = (!s.empty() && s[0] == '#') ? s.substr(1) : s;
    if (d.size() == 3) d = {d[0], d[0], d[1], d[1], d[2], d[2]};
    if (d.size() != 6) return false;
    for (char c : d)
        if (!std::isxdigit((unsigned char)c)) return false;
    unsigned v = (unsigned)std::strtoul(d.c_str(), nullptr, 16);
    r = (v >> 16) & 0xff; g = (v >> 8) & 0xff; b = v & 0xff;
    return true;
}

/* Colour tokens well enough to read while the host's own highlighter decides
 * the real palette — used only when opts.highlight is absent. */
unsigned default_rgb() { return 0xd0d0d0; }

} // namespace

// ── CodeEditorState ─────────────────────────────────────────────────────────

std::string CodeEditorState::selected_text() const {
    if (caret == anchor) return {};
    size_t a = std::min(caret, anchor), b = std::max(caret, anchor);
    return text.substr(a, b - a);
}

void CodeEditorState::select_all() {
    anchor = 0;
    caret = text.size();
}

void CodeEditorState::set_text(std::string t) {
    text = std::move(t);
    caret = std::min(caret, text.size());
    anchor = std::min(anchor, text.size());
    open_span = npos;
}

// ── CodeWidgetRegistry ──────────────────────────────────────────────────────

void CodeWidgetRegistry::add(std::string kind, SpanRenderer r) {
    for (auto& e : renderers) {
        if (e.first == kind) {
            e.second = std::move(r);
            return;
        }
    }
    renderers.push_back({std::move(kind), std::move(r)});
}

const SpanRenderer* CodeWidgetRegistry::find(std::string_view kind) const {
    for (const auto& e : renderers)
        if (e.first == kind) return &e.second;
    return nullptr;
}

CodeWidgetRegistry CodeWidgetRegistry::defaults() {
    CodeWidgetRegistry reg;

    reg.add("color", [](const char* id, std::string& value, bool) {
        float col[3] = {0, 0, 0};
        unsigned r = 0, g = 0, b = 0;
        read_hex_color(value, r, g, b);
        col[0] = r / 255.0f;
        col[1] = g / 255.0f;
        col[2] = b / 255.0f;

        bool changed = false;
        ImGui::PushID(id);
        if (ImGui::ColorPicker3("##wheel", col,
                                ImGuiColorEditFlags_PickerHueWheel |
                                    ImGuiColorEditFlags_NoSidePreview |
                                    ImGuiColorEditFlags_NoSmallPreview)) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "#%02x%02x%02x",
                          (unsigned)(col[0] * 255.0f + 0.5f),
                          (unsigned)(col[1] * 255.0f + 0.5f),
                          (unsigned)(col[2] * 255.0f + 0.5f));
            value = buf;
            changed = true; // live: the hex in the script tracks the drag
        }
        ImGui::PopID();
        return changed;
    });

    reg.add("date", [](const char* id, std::string& value, bool) {
        int y = 2026, m = 1, d = 1;
        std::sscanf(value.c_str(), "%d-%d-%d", &y, &m, &d);
        bool changed = false;
        ImGui::PushID(id);
        ImGui::SetNextItemWidth(70);
        if (ImGui::InputInt("##y", &y, 0)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        if (ImGui::InputInt("##m", &m, 0)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        if (ImGui::InputInt("##d", &d, 0)) changed = true;

        // A compact month grid: click a day and the token rewrites in place.
        static const char* dow[] = {"S", "M", "T", "W", "T", "F", "S"};
        for (int i = 0; i < 7; ++i) {
            if (i) ImGui::SameLine();
            ImGui::TextDisabled("%s", dow[i]);
        }
        int days = 31;
        if (m == 4 || m == 6 || m == 9 || m == 11) days = 30;
        else if (m == 2) days = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : 28;
        for (int day = 1; day <= days; ++day) {
            if ((day - 1) % 7) ImGui::SameLine();
            char lbl[8];
            std::snprintf(lbl, sizeof lbl, "%d", day);
            if (ImGui::SmallButton(lbl)) {
                d = day;
                changed = true;
            }
        }
        ImGui::PopID();
        if (changed) {
            m = std::clamp(m, 1, 12);
            d = std::clamp(d, 1, days);
            char buf[16];
            std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
            value = buf;
        }
        return changed;
    });

    return reg;
}

// ── the editor ──────────────────────────────────────────────────────────────

CodeEditorIO code_editor(const char* id, CodeEditorState& st,
                         const CodeEditorOptions& opts, float width, float height) {
    CodeEditorIO io_out;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::PushID(id);
    const ImGuiID wid = ImGui::GetID("##code");
    Internal& in = internal_for(wid);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 size(width > 0 ? width : avail.x, height > 0 ? height : avail.y);
    if (size.x < 32) size.x = 32;
    if (size.y < 32) size.y = 32;

    ImGui::BeginChild("##code", size, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

    // Ctrl+Wheel zooms. Plain wheel keeps scrolling, so the common gesture is
    // undisturbed — the modifier is what makes zoom discoverable without
    // stealing anything.
    if (opts.allow_zoom && ImGui::IsWindowHovered() && io.KeyCtrl && io.MouseWheel != 0.0f) {
        st.zoom *= (io.MouseWheel > 0 ? 1.1f : 1.0f / 1.1f);
        st.zoom = std::clamp(st.zoom, opts.zoom_min, opts.zoom_max);
    }
    ImGui::SetWindowFontScale(st.zoom);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float char_w = ImGui::CalcTextSize("M").x;
    const float line_h = ImGui::GetTextLineHeight() * opts.line_spacing;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    // Level of detail: past a point glyphs are unreadable noise, so draw the
    // COLOUR STRUCTURE instead. You lose the words and keep the shape.
    const bool lod = line_h < opts.lod_threshold;

    std::vector<Line> lines = split_lines(st.text);

    // Reserve the whole document so the child scrolls over it.
    ImGui::Dummy(ImVec2(char_w * 100, line_h * (float)lines.size() + line_h));
    ImGui::SetCursorScreenPos(origin);

    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    const bool popup_open = st.open_span != npos;

    auto offset_at = [&](ImVec2 mouse) -> size_t {
        int li = (int)((mouse.y - origin.y) / line_h);
        li = std::clamp(li, 0, (int)lines.size() - 1);
        const Line& L = lines[(size_t)li];
        int col = (int)((mouse.x - origin.x) / char_w + 0.5f);
        col = std::max(col, 0);
        size_t off = L.begin + (size_t)col;
        return std::min(off, L.end);
    };

    auto push_undo = [&] {
        in.undo.push_back({st.text, st.caret});
        if (in.undo.size() > 200) in.undo.erase(in.undo.begin());
        in.redo.clear();
    };
    auto erase_selection = [&]() -> bool {
        if (!st.has_selection()) return false;
        size_t a = std::min(st.caret, st.anchor), b = std::max(st.caret, st.anchor);
        st.text.erase(a, b - a);
        st.caret = st.anchor = a;
        return true;
    };
    auto mark_edited = [&] {
        st.dirty = true;
        io_out.edited = true;
        in.pending_commit = true;
        lines = split_lines(st.text);
    };

    // ── widget spans (claimed before input, so a click can hit one) ──────────
    std::vector<CodeWidgetSpan> wspans;
    if (opts.widgets && opts.registry) wspans = opts.widgets(st.text);

    // ── completion: ask the host what fits here ─────────────────────────────
    // Computed BEFORE input so Tab/Enter can be claimed by an open popup rather
    // than inserting a soft tab or a newline — the standard bargain, and the
    // reason it has to run first.
    CompletionSet comp;
    if (opts.complete && st.focused && !popup_open && !opts.read_only) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space)) {
            st.completion_forced = true;
            st.completion_off = false;
        }
        if (st.completion_off && st.completion_at != st.caret) {
            st.completion_off = false; // the caret moved: offer again
        }
        if (!st.completion_off) comp = opts.complete(st.text, st.caret);
    }
    // ONLY suggest while a word is actually being typed, or when asked. An
    // editor that pops a list on every blank line and after every space is
    // something you spend the day dismissing — the suggestion has to be a
    // response to what you are doing, not an ambient state.
    const bool typing_a_word = comp.replace_end > comp.replace_begin;
    if (!comp.items.empty() && !typing_a_word && !st.completion_forced)
        comp.items.clear();
    const bool completing = !comp.items.empty();
    if (!completing) st.completion_forced = false;
    if (completing) {
        st.completion_pick =
            std::clamp(st.completion_pick, 0, (int)comp.items.size() - 1);
    } else {
        st.completion_pick = 0;
    }

    // ── input ───────────────────────────────────────────────────────────────
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImGui::SetKeyboardFocusHere();
        st.focused = true;
    }
    if (ImGui::IsWindowFocused()) st.focused = true;

    if (!opts.read_only && !popup_open && st.focused) {
        const bool ctrl = io.KeyCtrl, shift = io.KeyShift;

        // Mouse: click to place, drag to select, double-click for a word.
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            size_t off = offset_at(io.MousePos);
            bool on_widget = false;
            for (const auto& w : wspans) {
                if (off >= w.begin && off < w.end && opts.registry->find(w.kind)) {
                    st.open_span = w.begin; // the token becomes its widget
                    // Open it HERE, exactly once, on the click that asked for
                    // it. Re-opening every frame from the draw loop (because
                    // `open_span` is still set) makes the popup immortal: ImGui
                    // closes it on an outside click and the next frame puts it
                    // straight back.
                    char pid[64];
                    std::snprintf(pid, sizeof pid, "##span%zu", w.begin);
                    ImGui::OpenPopup(pid);
                    on_widget = true;
                    break;
                }
            }
            if (!on_widget) {
                st.caret = off;
                if (!shift) st.anchor = off;
                in.dragging = true;
                st.caret_col_hint = -1;
            }
        }
        if (in.dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            st.caret = offset_at(io.MousePos);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) in.dragging = false;

        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            size_t off = offset_at(io.MousePos);
            size_t a = off, b = off;
            while (a > 0 && word_char(st.text[a - 1])) --a;
            while (b < st.text.size() && word_char(st.text[b])) ++b;
            st.anchor = a;
            st.caret = b;
        }

        // Characters.
        if (!io.InputQueueCharacters.empty()) {
            push_undo();
            erase_selection();
            for (ImWchar c : io.InputQueueCharacters) {
                if (c < 32 && c != '\t') continue;
                char utf8[5] = {};
                int n = ImTextCharToUtf8(utf8, (unsigned)c) ? (int)strlen(utf8) : 0;
                if (n <= 0) continue;
                st.text.insert(st.caret, utf8, (size_t)n);
                st.caret += (size_t)n;
            }
            st.anchor = st.caret;
            mark_edited();
        }

        auto key = [&](ImGuiKey k) { return ImGui::IsKeyPressed(k, true); };

        // The popup steals exactly four keys while open — Tab and Enter to
        // accept, Up/Down to choose. Everything else still edits, so typing
        // never stalls waiting on a suggestion.
        bool consumed_by_completion = false;

        // ── the completion popup owns these keys while it is open ───────────
        if (completing) {
            if (key(ImGuiKey_DownArrow)) ++st.completion_pick;
            if (key(ImGuiKey_UpArrow)) --st.completion_pick;
            st.completion_pick =
                (st.completion_pick + (int)comp.items.size()) % (int)comp.items.size();

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                st.completion_off = true;   // say no once, stay quiet
                st.completion_at = st.caret;
                st.completion_forced = false;
            } else if (key(ImGuiKey_Tab)) {
                // TAB ACCEPTS. Enter never does — it is the newline key and
                // stealing it means a suggestion you were ignoring eats the
                // line break you actually wanted. Two keys, two jobs, no
                // guessing which one the editor thinks you meant.
                push_undo();
                const std::string& ins = comp.items[(size_t)st.completion_pick].text;
                size_t b = std::min(comp.replace_begin, st.text.size());
                size_t e = std::min(comp.replace_end, st.text.size());
                if (e < b) std::swap(b, e);
                st.text.replace(b, e - b, ins);
                st.caret = st.anchor = b + ins.size();
                mark_edited();
                st.completion_pick = 0;
                st.completion_forced = false;
                consumed_by_completion = true;
            }
        }

        if (!consumed_by_completion &&
            (key(ImGuiKey_Enter) || key(ImGuiKey_KeypadEnter))) {
            if (ctrl) {
                io_out.commit = true; // the explicit "done" signal
                in.pending_commit = false;
            } else {
                push_undo();
                erase_selection();
                // Auto-indent: carry the current line's leading whitespace.
                size_t li = line_of(lines, st.caret);
                std::string indent;
                for (size_t i = lines[li].begin;
                     i < lines[li].end && (st.text[i] == ' ' || st.text[i] == '\t'); ++i)
                    indent += st.text[i];
                st.text.insert(st.caret, "\n" + indent);
                st.caret += 1 + indent.size();
                st.anchor = st.caret;
                mark_edited();
            }
        }
        if (key(ImGuiKey_Backspace)) {
            push_undo();
            if (!erase_selection() && st.caret > 0) {
                size_t p = prev_char(st.text, st.caret);
                st.text.erase(p, st.caret - p);
                st.caret = st.anchor = p;
            }
            mark_edited();
        }
        if (key(ImGuiKey_Delete)) {
            push_undo();
            if (!erase_selection() && st.caret < st.text.size()) {
                size_t n = next_char(st.text, st.caret);
                st.text.erase(st.caret, n - st.caret);
            }
            mark_edited();
        }
        if (!consumed_by_completion && key(ImGuiKey_Tab)) {
            push_undo();
            erase_selection();
            st.text.insert(st.caret, "  "); // soft tab
            st.caret += 2;
            st.anchor = st.caret;
            mark_edited();
        }

        // Caret motion. Shift extends (anchor stays put); otherwise it follows.
        auto moved = [&](size_t to) {
            st.caret = to;
            if (!shift) st.anchor = to;
        };
        if (key(ImGuiKey_LeftArrow)) {
            size_t to = prev_char(st.text, st.caret);
            if (ctrl) {
                while (to > 0 && !word_char(st.text[to - 1])) --to;
                while (to > 0 && word_char(st.text[to - 1])) --to;
            }
            moved(to);
            st.caret_col_hint = -1;
        }
        if (key(ImGuiKey_RightArrow)) {
            size_t to = next_char(st.text, st.caret);
            if (ctrl) {
                while (to < st.text.size() && !word_char(st.text[to])) ++to;
                while (to < st.text.size() && word_char(st.text[to])) ++to;
            }
            moved(to);
            st.caret_col_hint = -1;
        }
        if (!completing && (key(ImGuiKey_UpArrow) || key(ImGuiKey_DownArrow))) {
            size_t li = line_of(lines, st.caret);
            int col = st.caret_col_hint >= 0 ? st.caret_col_hint
                                             : (int)(st.caret - lines[li].begin);
            st.caret_col_hint = col; // preserved across a run of up/downs
            size_t target = li;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) && li > 0) target = li - 1;
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) && li + 1 < lines.size())
                target = li + 1;
            const Line& T = lines[target];
            moved(std::min(T.begin + (size_t)col, T.end));
        }
        if (key(ImGuiKey_Home)) {
            size_t li = line_of(lines, st.caret);
            moved(ctrl ? 0 : lines[li].begin);
            st.caret_col_hint = -1;
        }
        if (key(ImGuiKey_End)) {
            size_t li = line_of(lines, st.caret);
            moved(ctrl ? st.text.size() : lines[li].end);
            st.caret_col_hint = -1;
        }

        // Clipboard + undo, through the platform seam ImGui already owns.
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) st.select_all();
        if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_C) || ImGui::IsKeyPressed(ImGuiKey_X))) {
            std::string sel = st.selected_text();
            if (!sel.empty()) {
                ImGui::SetClipboardText(sel.c_str());
                if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                    push_undo();
                    erase_selection();
                    mark_edited();
                }
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
            const char* clip = ImGui::GetClipboardText();
            if (clip && *clip) {
                push_undo();
                erase_selection();
                std::string s(clip);
                st.text.insert(st.caret, s);
                st.caret += s.size();
                st.anchor = st.caret;
                mark_edited();
            }
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !in.undo.empty()) {
            in.redo.push_back({st.text, st.caret});
            st.text = in.undo.back().first;
            st.caret = st.anchor = std::min(in.undo.back().second, st.text.size());
            in.undo.pop_back();
            mark_edited();
        }
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y) && !in.redo.empty()) {
            in.undo.push_back({st.text, st.caret});
            st.text = in.redo.back().first;
            st.caret = st.anchor = std::min(in.redo.back().second, st.text.size());
            in.redo.pop_back();
            mark_edited();
        }
    }

    st.caret = std::min(st.caret, st.text.size());
    st.anchor = std::min(st.anchor, st.text.size());
    lines = split_lines(st.text);
    if (opts.widgets && opts.registry) wspans = opts.widgets(st.text);

    // ── colouring ───────────────────────────────────────────────────────────
    std::vector<CodeSpan> spans;
    if (opts.highlight) spans = opts.highlight(st.text);
    auto colour_at = [&](size_t off) -> unsigned {
        for (const auto& s : spans)
            if (off >= s.begin && off < s.end) return s.rgb; // first wins
        return default_rgb();
    };

    // ── selection highlight ─────────────────────────────────────────────────
    if (st.has_selection()) {
        size_t a = std::min(st.caret, st.anchor), b = std::max(st.caret, st.anchor);
        for (size_t li = 0; li < lines.size(); ++li) {
            size_t s = std::max(a, lines[li].begin), e = std::min(b, lines[li].end);
            if (s > e) continue;
            if (b < lines[li].begin || a > lines[li].end) continue;
            float x0 = origin.x + (float)(s - lines[li].begin) * char_w;
            float x1 = origin.x + (float)(e - lines[li].begin) * char_w;
            if (x1 <= x0) x1 = x0 + char_w * 0.4f; // a caught newline reads as a sliver
            float y0 = origin.y + (float)li * line_h;
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y0 + line_h),
                              IM_COL32(70, 110, 180, 110));
        }
    }

    // ── text, run by run ────────────────────────────────────────────────────
    for (size_t li = 0; li < lines.size(); ++li) {
        const Line& L = lines[li];
        float y = origin.y + (float)li * line_h;
        if (y + line_h < ImGui::GetWindowPos().y ||
            y > ImGui::GetWindowPos().y + ImGui::GetWindowSize().y)
            continue; // cheap cull: only draw what is on screen

        size_t i = L.begin;
        while (i < L.end) {
            unsigned rgb = colour_at(i);
            size_t j = i;
            while (j < L.end && colour_at(j) == rgb) ++j;
            float x = origin.x + (float)(i - L.begin) * char_w;
            if (lod) {
                // A filled bar per coloured run. Whitespace is skipped so the
                // indentation structure stays legible as gaps — which is most
                // of what you can still read at this scale.
                size_t s = i, e = j;
                while (s < e && std::isspace((unsigned char)st.text[s])) ++s;
                while (e > s && std::isspace((unsigned char)st.text[e - 1])) --e;
                if (e > s) {
                    float bx = origin.x + (float)(s - L.begin) * char_w;
                    float bw = (float)(e - s) * char_w;
                    dl->AddRectFilled(ImVec2(bx, y + line_h * 0.15f),
                                      ImVec2(bx + bw, y + line_h * 0.85f),
                                      to_col(rgb));
                }
            } else {
                dl->AddText(ImVec2(x, y), to_col(rgb), st.text.c_str() + i,
                            st.text.c_str() + j);
            }
            i = j;
        }
    }

    // ── inline widgets: a chip drawn AT the token, opening its editor there ──
    if (opts.registry) {
        for (const auto& w : wspans) {
            const SpanRenderer* r = opts.registry->find(w.kind);
            if (!r) continue;
            size_t li = line_of(lines, w.begin);
            float x0 = origin.x + (float)(w.begin - lines[li].begin) * char_w;
            float x1 = origin.x + (float)(w.end - lines[li].begin) * char_w;
            float y = origin.y + (float)li * line_h;

            // A colour token gets a swatch of ITSELF; anything else an outline.
            if (w.kind == "color") {
                unsigned rr = 0, gg = 0, bb = 0;
                read_hex_color(w.value, rr, gg, bb);
                dl->AddRectFilled(ImVec2(x1 + 3, y + 2),
                                  ImVec2(x1 + 3 + line_h - 4, y + line_h - 2),
                                  IM_COL32(rr, gg, bb, 255), 3.0f);
                dl->AddRect(ImVec2(x1 + 3, y + 2),
                            ImVec2(x1 + 3 + line_h - 4, y + line_h - 2),
                            IM_COL32(255, 255, 255, 90), 3.0f);
            }

            ImVec2 lo(x0, y), hi(x1 + line_h + 4, y + line_h);
            bool over = hovered && io.MousePos.x >= lo.x && io.MousePos.x <= hi.x &&
                        io.MousePos.y >= lo.y && io.MousePos.y <= hi.y;
            if (over) {
                dl->AddRect(ImVec2(x0 - 1, y), ImVec2(x1 + 1, y + line_h),
                            IM_COL32(255, 210, 120, 200), 2.0f);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (st.open_span == w.begin) {
                char pid[64];
                std::snprintf(pid, sizeof pid, "##span%zu", w.begin);

                // ImGui's OWN popup, not a hand-rolled window plus a hit test.
                // The hand-rolled version got dismissal wrong in the one case
                // that matters: dragging inside a colour wheel registers a
                // mouse-down, and the "did the click land outside?" test could
                // not reliably tell that apart from a click on the editor. A
                // real popup closes on outside-click and keeps inside-clicks
                // for free. It is OPENED in the click handler above, never here
                // — reopening from the draw loop made it impossible to dismiss.
                ImGui::SetNextWindowPos(ImVec2(x0, y + line_h + 2),
                                        ImGuiCond_Appearing);
                if (ImGui::BeginPopup(pid)) {
                    std::string value = w.value;
                    if ((*r)(pid, value, true) && value != w.value) {
                        // Splice the new value back over the token — the widget
                        // edits the SCRIPT, not a copy of it.
                        std::string replacement =
                            w.quoted ? "\"" + value + "\"" : value;
                        st.text.replace(w.begin, w.end - w.begin, replacement);
                        st.dirty = true;
                        io_out.edited = true;
                        in.pending_commit = true;
                    }
                    ImGui::EndPopup();
                } else {
                    // Closed by clicking away or Escape — ImGui's own rules.
                    st.open_span = npos;
                    // CLOSING THE PICKER IS THE RELEASE. The VLS #14b staging
                    // discipline says stage during a gesture and flush ONE
                    // command when it ends — so the edit commits here rather
                    // than waiting for focus to leave the whole editor, which
                    // is what previously made a colour change appear to do
                    // nothing at all.
                    if (in.pending_commit) {
                        io_out.commit = true;
                        in.pending_commit = false;
                    }
                }
            }
        }
    }

    // ── the caret ───────────────────────────────────────────────────────────
    if (st.focused && !popup_open) {
        st.blink += io.DeltaTime;
        if (st.blink > 1.06) st.blink = 0;
        if (st.blink < 0.53) {
            size_t li = line_of(lines, st.caret);
            float cx = origin.x + (float)(st.caret - lines[li].begin) * char_w;
            float cy = origin.y + (float)li * line_h;
            dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + line_h),
                        IM_COL32(230, 230, 230, 230), 1.5f);
        }
        // Keep the caret in view — the one place the editor scrolls itself.
        size_t li = line_of(lines, st.caret);
        float cy = (float)li * line_h;
        if (cy < ImGui::GetScrollY()) ImGui::SetScrollY(cy);
        else if (cy + line_h > ImGui::GetScrollY() + ImGui::GetWindowSize().y)
            ImGui::SetScrollY(cy + line_h - ImGui::GetWindowSize().y);
    }

    // ── right-click: explain this word, and the clipboard ───────────────────
    // Right-click puts the caret under the cursor FIRST, so the menu always
    // acts on the word you pointed at — no selecting first, which is the whole
    // point of asking about a word you do not recognize.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        size_t off = offset_at(io.MousePos);
        if (!st.has_selection()) st.caret = st.anchor = off;
        ImGui::OpenPopup("##code-ctx");
    }
    if (ImGui::BeginPopup("##code-ctx")) {
        // Match the editor's zoom: the menu is ABOUT the text, so reading
        // one at 300% and the other at 100% is a jarring change of scale
        // in the middle of one thought.
        ImGui::SetWindowFontScale(st.zoom);
        std::string what;
        if (opts.explain) what = opts.explain(st.text, st.caret);
        if (!what.empty()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
            ImGui::TextUnformatted(what.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Separator();
        }
        const bool sel = st.has_selection();
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, sel && !opts.read_only)) {
            ImGui::SetClipboardText(st.selected_text().c_str());
            push_undo();
            erase_selection();
            mark_edited();
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, sel))
            ImGui::SetClipboardText(st.selected_text().c_str());
        const char* clip = ImGui::GetClipboardText();
        if (ImGui::MenuItem("Paste", "Ctrl+V", false,
                            !opts.read_only && clip && *clip)) {
            push_undo();
            erase_selection();
            std::string p(clip);
            st.text.insert(st.caret, p);
            st.caret = st.anchor = st.caret + p.size();
            mark_edited();
        }
        if (ImGui::MenuItem("Select all", "Ctrl+A")) st.select_all();
        ImGui::EndPopup();
    }

    // ── the completion list, drawn at the caret ─────────────────────────────
    if (completing) {
        size_t li = line_of(lines, st.caret);
        float cx = origin.x + (float)(st.caret - lines[li].begin) * char_w;
        float cy = origin.y + (float)li * line_h + line_h + 2;

        // Measure so the box fits its widest row: a list that clips the thing
        // you are choosing between defeats the purpose.
        float w = 0;
        for (const auto& it : comp.items) {
            const std::string& lbl = it.label.empty() ? it.text : it.label;
            float row = ImGui::CalcTextSize(lbl.c_str()).x +
                        (it.detail.empty() ? 0
                                           : ImGui::CalcTextSize(it.detail.c_str()).x + 24);
            w = std::max(w, row);
        }
        w += 16;
        const int shown = std::min((int)comp.items.size(), 8);
        // Follow the highlighted row when it walks past the visible window.
        const int first = std::clamp(st.completion_pick - shown + 1, 0,
                                     std::max(0, (int)comp.items.size() - shown));
        float h = line_h * (float)shown + 8;

        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + w, cy + h),
                          IM_COL32(28, 30, 36, 245), 3.0f);
        dl->AddRect(ImVec2(cx, cy), ImVec2(cx + w, cy + h),
                    IM_COL32(120, 130, 150, 200), 3.0f);

        for (int i = 0; i < shown; ++i) {
            const Completion& it = comp.items[(size_t)(first + i)];
            float ry = cy + 4 + line_h * (float)i;
            if (first + i == st.completion_pick)
                dl->AddRectFilled(ImVec2(cx + 2, ry), ImVec2(cx + w - 2, ry + line_h),
                                  IM_COL32(70, 110, 180, 180), 2.0f);
            const std::string& lbl = it.label.empty() ? it.text : it.label;
            dl->AddText(ImVec2(cx + 6, ry), IM_COL32(235, 235, 240, 255), lbl.c_str());
            if (!it.detail.empty()) {
                float dx = cx + w - 6 - ImGui::CalcTextSize(it.detail.c_str()).x;
                dl->AddText(ImVec2(dx, ry), IM_COL32(140, 150, 165, 255),
                            it.detail.c_str());
            }
        }
        if ((int)comp.items.size() > shown) {
            char more[32];
            std::snprintf(more, sizeof more, "+%d more",
                          (int)comp.items.size() - shown);
            dl->AddText(ImVec2(cx + 6, cy + h - 2), IM_COL32(140, 150, 165, 255), more);
        }
    }

    // ── hover tooltip ───────────────────────────────────────────────────────
    if (hovered && opts.hover && !popup_open) {
        std::string tip = opts.hover(st.text, offset_at(io.MousePos));
        if (!tip.empty()) ImGui::SetTooltip("%s", tip.c_str());
    }

    ImGui::EndChild();

    // Leaving the editor after an edit is also a commit: the author moved on,
    // and an edit that never flushes would be an edit the transcript never saw.
    if (in.pending_commit && !ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        st.open_span == npos) {
        io_out.commit = true;
        in.pending_commit = false;
        st.focused = false;
    }

    ImGui::PopID();
    return io_out;
}

} // namespace maiz
