/*
 * voidmaiz/textinput.hpp — the platform keyboard as a HOLIDAY.
 *
 * The author, 2026-09-23: *"Custom keyboard is too much of a hassle (and other
 * apps already deal with that, in fact, we should focus on the integration of
 * the keyboard) ... the integration of a keyboard is very similar to a holiday
 * is it not? An external domain that has to map onto our GUI mantle?"*
 *
 * It is exactly that shape (okf/concepts/text-input.md). A holiday is an
 * effectful crossing plus a pure mapping. Here:
 *
 *   the world    the platform's input method (Android's IME, and later iOS's):
 *                 it owns composing, autocorrect, accents, predictions, voice,
 *                 other alphabets — everything a keyboard app is good at and a
 *                 drawn keyboard never will be.
 *   the pivot    `EditingState`: text, selection, composing region. The same
 *                 value Flutter's `TextEditingValue` carries across ITS
 *                 platform channel (checked against Flutter 3.41.6), because
 *                 it is the one shape every IME protocol reduces to.
 *   the mapping  `plan_edit`: from the text the field holds to the text the
 *                 keyboard says it should hold, as ordinary key presses and
 *                 characters. PURE, and tested without a window
 *                 (tests/textinput_smoke.cpp).
 *   the crossing `TextInputPlatform`: show / update / hide, and poll what the
 *                 keyboard did. The only impure part, and the only part that
 *                 differs per platform (src/view/textinput_android.cpp).
 *
 * WHY KEY PRESSES AND NOT A BUFFER SWAP: every text field in every Void Maiz
 * application is an ImGui InputText, most of them drawn by the widget registry,
 * none of them written to cooperate with a keyboard. Keys and characters are
 * the one input every one of them already accepts. So the holiday reaches every
 * field there is, and a field needs no change to be typed into — the property
 * the drawn keyboard also had, kept.
 *
 * WHAT THE LIBRARY DOES NOT OWN: which fields exist. What it DOES own is what a
 * field IS for the keyboard (`InputKind`: a phone number wants a dial pad, an
 * email address wants an @), declared by the widget registry from the field's
 * key and editor, so registration — the thing Void Maiz is particular about —
 * is also what picks the keyboard.
 *
 * UI-free: no ImGui here. The glue that watches ImGui's active field and emits
 * the plan is voidmaiz/textinputview.hpp (voidmaiz_view).
 */
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ANativeActivity; // <android/native_activity.h>; Android only

namespace maiz {

/* What a field is, to a keyboard. The platform picks the layout (a dial pad, a
 * row with @ and .com, a newline key) from this. Flutter's TextInputType, less
 * the kinds no Void application has asked for yet. */
enum class InputKind {
    Text,      // prose: capitalised sentences, autocorrect, suggestions
    Multiline, // prose with a newline key
    Name,      // a person's or thing's name: capitalised words, no autocorrect
    Number,    // digits, sign and decimal point
    Phone,     // a dial pad
    Email,     // @ and .com
    Url,       // / and .com
    Search,    // a search box: the action key says "search"
    Code,      // a rune name, a tag, a command: no capitals, no autocorrect
};

/* What the keyboard's action key does. Pressing it reaches the field as Enter,
 * so a field that commits on Enter (every registry text field) commits. */
enum class InputAction { Done, Next, Go, Search, Send, Newline };

/* The pivot: one field's text as the keyboard sees it. Offsets are UTF-8 BYTE
 * offsets into `text` (the Android shim converts from Java's UTF-16 before they
 * cross), so they index the same bytes ImGui holds. compose_* are -1 when
 * nothing is being composed. */
struct EditingState {
    std::string text;
    int sel_start = 0;
    int sel_end = 0;
    int compose_start = -1;
    int compose_end = -1;
};

/* One keystroke the plan needs, in the order it must happen. */
enum class TextKey { Left, Right, Backspace, Delete, Enter, Tab };

/* The mapping's output: get from (text, cursor) to an EditingState using only
 * what any text field accepts. Applied in order: `move` (negative = left) to the
 * end of the replaced span, `backspaces`, type `insert`, then `move_after` to
 * the keyboard's cursor. Counts are in CODEPOINTS, because that is what one
 * arrow or one backspace moves over. */
struct EditPlan {
    int move = 0;
    int backspaces = 0;
    std::string insert;
    int move_after = 0;
    bool empty() const { return !move && !backspaces && insert.empty() && !move_after; }
};

/* The holiday's pure half. `text`/`cursor` are what the field holds now (byte
 * offset); `to` is what the keyboard says it should hold. The result always
 * reaches `to.text` exactly and puts the cursor at `to.sel_end`; the replaced
 * span is the smallest one (common prefix and suffix kept), which is what
 * composing and autocorrect look like: a word, not the field. */
EditPlan plan_edit(std::string_view text, int cursor, const EditingState& to);

/* UTF-8 helpers the plan and its tests share. */
int utf8_count(std::string_view s);                       // codepoints
int utf8_floor(std::string_view s, int byte);             // back to a codepoint start

/* What a registry field is, from its key and editor spec ("multiline:70",
 * "email", …). Names are matched as words in the key: `phone`, `mobile`,
 * `tel` → Phone; `email` → Email; `url`, `website`, `link` → Url;
 * `name`, `display_name`, `username` → Name. Unknown → Text. The host can
 * always say better with text_input_kind() after its own field. */
InputKind input_kind_for(std::string_view field_key, std::string_view editor = {});

/* The action key a kind gets by default: a newline for Multiline, Search for
 * Search, Done for everything else. */
InputAction default_action(InputKind kind);

/* Android's InputType and EditorInfo imeOptions for a kind and action. Pure
 * integers (android.text.InputType constants), so the table is tested on a
 * desktop and the Java shim holds no mapping of its own. */
int android_input_type(InputKind kind);
int android_ime_options(InputKind kind, InputAction action);

/* ── the crossing ────────────────────────────────────────────────────────────
 * What a platform keyboard can be asked to do, and what it reports. One per
 * platform; the glue holds a pointer and never learns which. */
struct TextInputEvent {
    enum class Type { State, Key, Action } type = Type::State;
    EditingState state; // Type::State: the keyboard's whole editing state
    TextKey key = TextKey::Enter; // Type::Key: a raw key the keyboard sent
};

struct TextInputPlatform {
    virtual ~TextInputPlatform() = default;
    /* A field became active: bring the keyboard up for it, seeded with what it
     * already holds, so autocorrect and backspace see the real text. */
    virtual void show(InputKind kind, InputAction action, const EditingState& state) = 0;
    /* The field changed without the keyboard (a tap moved the cursor): tell it. */
    virtual void update(const EditingState& state) = 0;
    virtual void hide() = 0;
    /* What the keyboard did since the last poll, in order. Thread-safe: the
     * platform calls in on its own thread. */
    virtual void poll(std::vector<TextInputEvent>& out) = 0;
    /* Pixels the keyboard covers at the bottom of the window (0 = hidden), so a
     * host can keep the field it is typing into above it. */
    virtual float covered_px() const { return 0.0f; }
};

/* Android: the system keyboard, through org.voidmaiz.MaizActivity (the Java
 * shim in android/java/). Returns null when the activity is a plain
 * NativeActivity — the host then keeps the drawn keyboard (mobile.hpp) as its
 * fallback. Call on the render thread, once. Null on every other platform. */
std::unique_ptr<TextInputPlatform> android_text_input(ANativeActivity* activity);

} // namespace maiz
