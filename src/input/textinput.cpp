/* src/input/textinput.cpp — the keyboard holiday's pure half (voidmaiz/textinput.hpp).
 * No ImGui, no platform: text in, keystrokes out. */
#include "voidmaiz/textinput.hpp"

#include <algorithm>

namespace maiz {

namespace {

bool continuation(char c) { return ((unsigned char)c & 0xC0) == 0x80; }

/* Codepoints from `from` to `to` in `s`, signed: negative when `to` is left. */
int signed_count(std::string_view s, int from, int to) {
    if (to >= from) return utf8_count(s.substr((size_t)from, (size_t)(to - from)));
    return -utf8_count(s.substr((size_t)to, (size_t)(from - to)));
}

int clamp_byte(std::string_view s, int b) {
    return utf8_floor(s, std::clamp(b, 0, (int)s.size()));
}

/* The key's words: `display_name` → {display, name}; `homePhone` stays one word,
 * which is fine — matching is on whole words or suffixes below. */
std::vector<std::string> words_of(std::string_view key) {
    std::vector<std::string> w;
    std::string cur;
    for (char c : key) {
        if (c == '_' || c == '-' || c == '.' || c == ' ') {
            if (!cur.empty()) w.push_back(cur), cur.clear();
        } else {
            cur += (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
        }
    }
    if (!cur.empty()) w.push_back(cur);
    return w;
}

} // namespace

int utf8_count(std::string_view s) {
    int n = 0;
    for (char c : s)
        if (!continuation(c)) ++n;
    return n;
}

int utf8_floor(std::string_view s, int byte) {
    byte = std::clamp(byte, 0, (int)s.size());
    while (byte > 0 && byte < (int)s.size() && continuation(s[(size_t)byte])) --byte;
    return byte;
}

EditPlan plan_edit(std::string_view a, int cursor, const EditingState& to) {
    const std::string_view b = to.text;
    EditPlan plan;
    cursor = clamp_byte(a, cursor);

    // the common prefix, backed off to a codepoint start (bytes agree up to it,
    // so it is a boundary in both strings)
    size_t p = 0;
    const size_t lim = std::min(a.size(), b.size());
    while (p < lim && a[p] == b[p]) ++p;
    p = (size_t)utf8_floor(a, (int)p);

    // the common suffix, never overlapping the prefix, starting on a codepoint
    size_t s = 0;
    while (s < a.size() - p && s < b.size() - p && a[a.size() - 1 - s] == b[b.size() - 1 - s]) ++s;
    while (s > 0 && continuation(a[a.size() - s])) --s;

    const int del_end = (int)(a.size() - s);
    plan.move = signed_count(a, cursor, del_end);
    plan.backspaces = utf8_count(a.substr(p, (size_t)del_end - p));
    plan.insert = std::string(b.substr(p, b.size() - s - p));

    const int ins_end = (int)(p + plan.insert.size());
    plan.move_after = signed_count(b, ins_end, clamp_byte(b, to.sel_end));
    return plan;
}

InputKind input_kind_for(std::string_view field_key, std::string_view editor) {
    // the editor spec speaks first: it is what the glyph DECLARED
    const std::string_view kind = editor.substr(0, editor.find(':'));
    if (kind == "multiline" || kind == "richtext") return InputKind::Multiline;
    if (kind == "number" || kind == "knob" || kind == "stepper" || kind == "date") return InputKind::Number;
    if (kind == "email") return InputKind::Email;
    if (kind == "url" || kind == "link") return InputKind::Url;
    if (kind == "phone" || kind == "tel") return InputKind::Phone;

    for (const std::string& w : words_of(field_key)) {
        if (w == "phone" || w == "mobile" || w == "tel" || w == "telephone" || w == "cell" || w == "fax")
            return InputKind::Phone;
        if (w == "email" || w == "mail") return InputKind::Email;
        if (w == "url" || w == "website" || w == "link" || w == "href" || w == "site")
            return InputKind::Url;
        if (w == "name" || w == "username" || w == "surname" || w == "firstname" || w == "lastname")
            return InputKind::Name;
        if (w == "bio" || w == "notes" || w == "description" || w == "summary" || w == "body")
            return InputKind::Multiline;
    }
    return InputKind::Text;
}

InputAction default_action(InputKind kind) {
    if (kind == InputKind::Multiline) return InputAction::Newline;
    if (kind == InputKind::Search) return InputAction::Search;
    return InputAction::Done;
}

/* android.text.InputType: a CLASS, a VARIATION and FLAGS, OR'd together. */
int android_input_type(InputKind kind) {
    constexpr int CLASS_TEXT = 0x1, CLASS_NUMBER = 0x2, CLASS_PHONE = 0x3;
    constexpr int CAP_WORDS = 0x2000, CAP_SENTENCES = 0x4000, AUTO_CORRECT = 0x8000,
                  MULTI_LINE = 0x20000, NO_SUGGESTIONS = 0x80000;
    constexpr int VAR_URI = 0x10, VAR_EMAIL = 0x20, VAR_PERSON_NAME = 0x60;
    constexpr int NUM_SIGNED = 0x1000, NUM_DECIMAL = 0x2000;
    switch (kind) {
    case InputKind::Text: return CLASS_TEXT | CAP_SENTENCES | AUTO_CORRECT;
    case InputKind::Multiline: return CLASS_TEXT | CAP_SENTENCES | AUTO_CORRECT | MULTI_LINE;
    case InputKind::Name: return CLASS_TEXT | CAP_WORDS | VAR_PERSON_NAME;
    case InputKind::Number: return CLASS_NUMBER | NUM_SIGNED | NUM_DECIMAL;
    case InputKind::Phone: return CLASS_PHONE;
    case InputKind::Email: return CLASS_TEXT | VAR_EMAIL;
    case InputKind::Url: return CLASS_TEXT | VAR_URI;
    case InputKind::Search: return CLASS_TEXT | AUTO_CORRECT;
    case InputKind::Code: return CLASS_TEXT | NO_SUGGESTIONS;
    }
    return CLASS_TEXT;
}

/* android.view.inputmethod.EditorInfo.imeOptions. Never full-screen: an IME in
 * "extract" mode covers the application with its own text box, which is the
 * one thing a GUI that wants complete control of its pixels cannot allow. */
int android_ime_options(InputKind kind, InputAction action) {
    constexpr int ACTION_NONE = 1, ACTION_GO = 2, ACTION_SEARCH = 3, ACTION_SEND = 4,
                  ACTION_NEXT = 5, ACTION_DONE = 6;
    constexpr int NO_FULLSCREEN = 0x2000000, NO_EXTRACT_UI = 0x10000000,
                  NO_ENTER_ACTION = 0x40000000;
    int o = NO_FULLSCREEN | NO_EXTRACT_UI;
    if (kind == InputKind::Multiline || action == InputAction::Newline)
        return o | ACTION_NONE | NO_ENTER_ACTION; // the key types a newline
    switch (action) {
    case InputAction::Next: return o | ACTION_NEXT;
    case InputAction::Go: return o | ACTION_GO;
    case InputAction::Search: return o | ACTION_SEARCH;
    case InputAction::Send: return o | ACTION_SEND;
    default: return o | ACTION_DONE;
    }
}

#ifndef __ANDROID__
std::unique_ptr<TextInputPlatform> android_text_input(ANativeActivity*) { return nullptr; }
#endif

} // namespace maiz
