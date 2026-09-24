/* textinput_smoke.cpp — the keyboard holiday's mapping (voidmaiz/textinput.hpp).
 *
 * Every plan is checked the way a field would receive it: `replay` is a model of
 * a text field (arrows move one codepoint, backspace deletes one, characters
 * insert at the cursor), and the field must end up holding EXACTLY what the
 * keyboard said, with the cursor where the keyboard put it. No window, no
 * device: the cases below are what a phone keyboard actually does. */
#include "voidmaiz/textinput.hpp"

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

namespace {

bool cont(char c) { return ((unsigned char)c & 0xC0) == 0x80; }
int left(const std::string& s, int b) {
    if (b <= 0) return 0;
    --b;
    while (b > 0 && cont(s[(size_t)b])) --b;
    return b;
}
int right(const std::string& s, int b) {
    if (b >= (int)s.size()) return (int)s.size();
    ++b;
    while (b < (int)s.size() && cont(s[(size_t)b])) ++b;
    return b;
}

/* A text field receiving the plan's keystrokes. */
void replay(std::string& text, int& cur, const EditPlan& p) {
    for (int i = 0; i < -p.move; ++i) cur = left(text, cur);
    for (int i = 0; i < p.move; ++i) cur = right(text, cur);
    for (int i = 0; i < p.backspaces; ++i) {
        int l = left(text, cur);
        text.erase((size_t)l, (size_t)(cur - l));
        cur = l;
    }
    text.insert((size_t)cur, p.insert);
    cur += (int)p.insert.size();
    for (int i = 0; i < -p.move_after; ++i) cur = left(text, cur);
    for (int i = 0; i < p.move_after; ++i) cur = right(text, cur);
}

EditingState to(std::string text, int sel) {
    EditingState e;
    e.text = std::move(text);
    e.sel_start = e.sel_end = sel;
    return e;
}

/* The one property: whatever the field held, after the plan it holds what the
 * keyboard says. Returns the plan so a case can also check its SIZE. */
EditPlan lands(std::string text, int cur, const EditingState& target) {
    EditPlan p = plan_edit(text, cur, target);
    replay(text, cur, p);
    if (text != target.text || cur != target.sel_end) {
        ++failures;
        std::cerr << "FAIL: got \"" << text << "\" @" << cur << ", wanted \"" << target.text << "\" @"
                  << target.sel_end << "\n";
    }
    return p;
}

} // namespace

int main() {
    // ── typing ────────────────────────────────────────────────────────────────
    {
        EditPlan p = lands("", 0, to("a", 1));
        CHECK(p.insert == "a" && p.backspaces == 0 && p.move == 0 && p.move_after == 0);
        p = lands("Mari", 4, to("Maria", 5));
        CHECK(p.insert == "a" && p.backspaces == 0); // one letter, not the field
    }
    // ── composing: the keyboard rewrites the word being typed ────────────────
    {
        // "hel" composing, then the suggestion "hello" is taken
        EditPlan p = lands("say hel", 7, to("say hello", 9));
        CHECK(p.insert == "lo" && p.backspaces == 0);
        // autocorrect replaces a word: "teh" -> "the "
        p = lands("I saw teh", 9, to("I saw the ", 10));
        CHECK(p.backspaces == 2 && p.insert == "he "); // "t" kept, "eh" gone
    }
    // ── the accents a Spanish-speaking organization types ────────────────────
    {
        // a long-press on n gives ñ: the keyboard rewrites "espan" to "españ"
        EditPlan p = lands("espan", 5, to("españ", 6));
        CHECK(p.backspaces == 1 && p.insert == "ñ"); // one CODEPOINT replaced, two bytes
        lands("españ", 6, to("español", 8));
        lands("Jose", 4, to("José", 5));
        lands("¿Que", 5, to("¿Qué?", 7));
        lands("pinguino", 8, to("pingüino", 9));
    }
    // ── deleting ─────────────────────────────────────────────────────────────
    {
        EditPlan p = lands("abc", 3, to("ab", 2));
        CHECK(p.backspaces == 1 && p.insert.empty());
        lands("abc", 3, to("", 0));          // clear the field
        lands("café", 5, to("caf", 3));      // a two-byte letter, one backspace
    }
    // ── editing in the middle, and cursor moves ──────────────────────────────
    {
        lands("helo world", 3, to("hello world", 4));
        EditPlan p = lands("hello", 5, to("hello", 2)); // a tap moved the cursor
        CHECK(p.insert.empty() && p.backspaces == 0 && p.move_after == -3);
        lands("aaa", 1, to("aaaa", 2)); // repeated letters: ambiguous diff, same result
        lands("abc", 0, to("xabc", 1));
    }
    // ── emoji and other four-byte codepoints ─────────────────────────────────
    {
        EditPlan p = lands("hi", 2, to("hi 😀", 7));
        CHECK(p.insert == " 😀");
        p = lands("hi 😀", 7, to("hi ", 3));
        CHECK(p.backspaces == 1); // one codepoint, four bytes
    }
    // ── a cursor the field reports mid-codepoint is floored, never split ─────
    {
        CHECK(utf8_floor("ñ", 1) == 0);
        CHECK(utf8_count("añb") == 3);
        lands("añb", 2, to("añb!", 5)); // cursor inside ñ: treated as before it
    }

    // ── what a field IS, from its key and editor ─────────────────────────────
    CHECK(input_kind_for("phone") == InputKind::Phone);
    CHECK(input_kind_for("home_phone") == InputKind::Phone);
    CHECK(input_kind_for("email") == InputKind::Email);
    CHECK(input_kind_for("website") == InputKind::Url);
    CHECK(input_kind_for("display_name") == InputKind::Name);
    CHECK(input_kind_for("bio_en") == InputKind::Multiline);
    CHECK(input_kind_for("role") == InputKind::Text);
    CHECK(input_kind_for("notes", "multiline:70") == InputKind::Multiline);
    CHECK(input_kind_for("anything", "email") == InputKind::Email); // the editor speaks first
    CHECK(default_action(InputKind::Multiline) == InputAction::Newline);
    CHECK(default_action(InputKind::Phone) == InputAction::Done);

    // ── the Android integers (android.text.InputType / EditorInfo) ───────────
    CHECK(android_input_type(InputKind::Phone) == 0x3);                 // TYPE_CLASS_PHONE
    CHECK(android_input_type(InputKind::Email) == (0x1 | 0x20));         // TEXT | EMAIL_ADDRESS
    CHECK((android_input_type(InputKind::Multiline) & 0x20000) != 0);    // MULTI_LINE
    CHECK((android_input_type(InputKind::Code) & 0x80000) != 0);         // NO_SUGGESTIONS
    CHECK((android_ime_options(InputKind::Text, InputAction::Done) & 0xff) == 6);   // IME_ACTION_DONE
    CHECK((android_ime_options(InputKind::Search, InputAction::Search) & 0xff) == 3);
    CHECK((android_ime_options(InputKind::Multiline, InputAction::Newline) & 0x40000000) != 0);
    // never full-screen: the keyboard must not cover the application with its own box
    CHECK((android_ime_options(InputKind::Text, InputAction::Done) & 0x10000000) != 0);

    // ── off Android, there is no platform keyboard to find ───────────────────
    CHECK(android_text_input(nullptr) == nullptr);

    std::cout << "textinput_smoke: " << (failures ? "FAILED" : "all ok") << "\n";
    return failures ? 1 : 0;
}
