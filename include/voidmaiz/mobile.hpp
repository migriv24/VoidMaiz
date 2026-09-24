/*
 * voidmaiz/mobile.hpp — the mobile chrome kit: the widgets that are native to
 * glass and have no desktop ancestor.
 *
 * widgets.hpp holds the OBSERVABILITY widgets (log strip, command bar) and the
 * substrate-neutral chrome (splitter, dockspace, touch metrics). This header
 * holds the ones that exist because a hand is holding the device: a bottom
 * sheet instead of a side panel, a snackbar instead of a status line, a thumb-
 * reachable action button instead of a menu bar, a swipe instead of a
 * right-click.
 *
 * Every one of them obeys the widget protocol (okf/concepts/widget-registry.md):
 * projection in, COMMANDS out, never a touch of the model, at most one command
 * per completed gesture. They are ImGui-composed (Q12's sanctioned path) and
 * they are chrome, not field editors, so they do not go through the registry —
 * implementing the shape by hand IS speaking the protocol.
 *
 * Why the library and not each host (Q11, the rung after docking): Void Hormiga
 * going mobile is the second host to need every one of these, and
 * InteractionCombinators already hand-rolled two of them badly — a `toast_until`
 * float and a `touch_mode ? 14 : 6` splitter thickness. The rule the author set
 * is "climb one rung per real need"; a second host with the same need is the
 * rung.
 *
 * View-module header (ImGui types): only available to targets linking
 * voidmaiz_view.
 */
#pragma once

#include "voidmaiz/canvas.hpp"
#include "voidmaiz/touch.hpp"

#include "imgui.h"

#include <string>
#include <vector>

namespace maiz {

/* ── the canvas, read by a finger ────────────────────────────────────────────
 * Set the SCREEN-SPACE budgets from the device's physical scale, and nothing
 * else. The deliberate omission is the node geometry (node_w, header_h,
 * port_row): those are WORLD units, and scaling them by display density would
 * make a phone's graph a different graph — the same document opened on two
 * devices would lay out differently. Screen size for world geometry is the
 * camera's job, which is why a touch host starts zoomed in rather than with
 * fattened nodes.
 *
 * What this does set:
 *  - `port_hit_radius` to about half a fingertip (a fingertip contact is ~9 mm),
 *    so the HIT target is far larger than the DRAWN marker. That asymmetry is
 *    the whole trick of touchable dense UI: nothing on screen gets uglier and
 *    everything gets grabbable.
 *  - `click_slop` small, not large — counter-intuitively. The recognizer's
 *    deferred press has ALREADY decided tap-vs-drag before the synthetic
 *    pointer goes down (voidmaiz/touch.hpp), so the canvas is being handed a
 *    decision, not an ambiguity, and a second fat slop budget would only blur
 *    the long-press right-click.
 *  - `hover_tooltips` off: a finger never hovers, so a tap's tooltip appears
 *    under the finger that caused it and then never leaves. */
void apply_touch_canvas(CanvasStyle& style, const TouchProfile& profile);

/* ── bottom sheet ────────────────────────────────────────────────────────────
 * The phone's answer to a docked side panel: a panel anchored to the bottom
 * edge that a thumb drags between DETENTS — fractions of the screen height it
 * snaps to. Peek (a header strip), half, full. Hormiga's inspector, its record
 * detail and its filter rail are all this widget on a phone.
 *
 * The detent index is VIEW STATE of exactly the kind the camera is: a host that
 * wants it remembered flushes it to the config tier on `settled` (one command,
 * undo-exempt), the same discipline `splitter`'s `released` exists for. The
 * library never persists it.
 *
 * Usage:
 *   if (maiz::begin_bottom_sheet("inspector", sheet)) {
 *       … ordinary ImGui, or maiz::draw_inspector(...) …
 *   }
 *   maiz::end_bottom_sheet(sheet);   // ALWAYS call, like ImGui::End
 */
struct BottomSheetState {
    /* Fractions of the viewport height, ascending. The default is the platform
     * convention: a peeking header, half, and nearly full. */
    std::vector<float> detents{0.12f, 0.5f, 0.92f};
    int detent = 0;      // the index it is settled on (or heading to)
    float height = 0.0f; // current fraction; animates toward the detent
    bool dragging = false;
    float drag_from = 0.0f; // fraction at the moment the drag began
    bool settled = false;   // set for ONE frame when a drag lands on a detent
    bool initialized = false;
    bool body_child = false; // internal: begin_* opened a scroll child to close

    /* Content scrolls, the handle drags. Set false for a sheet whose body is
     * itself a scroll surface the host manages. */
    bool scroll_body = true;
};

/* Returns true when the sheet is tall enough to draw into (the caller's content
 * block should be skipped otherwise, exactly like ImGui::Begin's return). Call
 * end_bottom_sheet unconditionally. */
bool begin_bottom_sheet(const char* str_id, BottomSheetState& st);
void end_bottom_sheet(BottomSheetState& st);

/* Programmatic movement (a host's "show details" button). Animates. */
void bottom_sheet_snap(BottomSheetState& st, int detent);

/* ── snackbar ────────────────────────────────────────────────────────────────
 * A transient message at the bottom of the screen carrying ONE optional action.
 * On the desktop this library's answer to "what just happened" is the log
 * strip; on a phone there is no room for a log strip, and the answer is this.
 *
 * The action exists because of commitment 2. Every gesture is already a command
 * and `undo` is already a verb, so the mobile idiom "Deleted. UNDO" costs
 * nothing but the widget: `show_snackbar(sb, "Deleted 3 nodes", "UNDO")`, and
 * when draw_snackbar returns true the host dispatches `undo`. That is the whole
 * integration — the library does not know what the action means and never
 * dispatches anything itself. */
struct SnackbarState {
    std::string message;
    std::string action;  // "" = no action button
    double until = -1.0; // host clock (ImGui::GetTime()); <0 = hidden
};

/* Replaces whatever is showing. `seconds` is the platform convention (4 s with
 * an action, so a thumb can reach it; 2.5 s without). */
void show_snackbar(SnackbarState& st, const std::string& message, const std::string& action = "",
                   double seconds = 0.0);

/* Draws it if live; returns true the frame the action is tapped (which also
 * dismisses it). Call once per frame, near the end of the host's frame(). */
bool draw_snackbar(SnackbarState& st);

/* ── an action bar that never clips (Q28, 2026-09-21) ───────────────────────
 * A row of buttons laid out to the width it has: everything that fits is drawn,
 * the rest go into an overflow menu behind a "more" button (three drawn dots:
 * the bundled font has no U+22EE). The most important stay visible, in their
 * original order (plan_action_bar in touch.hpp is the pure half, and is tested).
 * The 0.2.0 APK drew its toolbar as a plain row, and on a 317-dp phone half of
 * it was off the edge of the screen: unreachable, not merely cramped.
 *
 * Returns the index of the action pressed this frame, or -1. `width` 0 = the
 * rest of the current line. Same code on both substrates: touch=false draws
 * compact buttons, and a narrow desktop window (a duo-bench half) overflows the
 * same way a phone does. */
struct BarAction {
    std::string label;
    int priority = 0;     // lower = more important = kept visible longer
    bool enabled = true;
    bool pinned = false;  // always visible if anything is
    bool primary = false; // drawn emphasized: the one thing this screen is for
};

int action_bar(const char* str_id, const std::vector<BarAction>& actions, bool touch,
               float width = 0.0f);

/* The "more" affordance on its own (an app bar's trailing menu), and its width
 * so a host can right-align it. */
bool dots_button(const char* str_id, bool touch);
float dots_button_width(bool touch);

/* A menu hanging off a dots button. Between begin/end the host draws ordinary
 * ImGui menus (BeginMenu/MenuItem), so a desktop menu bar's entire content can
 * move behind one button on a phone without being rewritten:
 *
 *   if (maiz::begin_overflow_menu("app-menu", touch)) { draw_menus(); maiz::end_overflow_menu(); }
 */
bool begin_overflow_menu(const char* str_id, bool touch);
void end_overflow_menu();

/* Dim AND wrapped. `ImGui::TextDisabled` does not wrap, so the explaining
 * sentence under a control — exactly the sentence a phone most needs — is cut
 * off at the panel edge (seen in the update dialog's first screenshot). */
void dim_wrapped(const char* text);

/* ── a keyboard, drawn: THE FALLBACK (Q29's lean, built 2026-09-22) ──────────
 * Since 2026-09-23 the keyboard is the PLATFORM's (voidmaiz/textinput.hpp,
 * okf/concepts/text-input.md): the author ruled that a drawn keyboard is not
 * what people should type on. This one remains for a touch host with no
 * platform keyboard (a plain NativeActivity, a kiosk, a VR panel). Never run
 * both. */
/* ── (the drawn keyboard, as first written) ──────────────────────────────────
 * A phone running a Void Maiz application has no system keyboard: showing
 * Android's means Java through JNI, and the APK's whole shape is that there is
 * none. So the library draws one and feeds ImGui the same events a real keyboard
 * would (`AddInputCharacter`, `AddKeyEvent`), which means EVERY text field works
 * — a tag, a rune name, a net name, the command bar — with no change to the
 * field. The author found it the hard way: "going to add a tag, the keyboard
 * feature doesn't work".
 *
 * It is called FIRST, immediately after `ImGui::NewFrame()`, and nowhere else:
 *
 *     ImGui::NewFrame();
 *     maiz::keyboard(kb, touch_mode);   // shows itself only when a field wants input
 *
 * That position is the design, not a convenience. The keyboard draws itself in
 * the foreground draw list and hit-tests its own keys, so that it can do the two
 * things an ImGui window cannot:
 *   - appear ON TOP of a modal (Save As, Settings), whose fields are exactly the
 *     ones you need to type into, and which blocks every ordinary window;
 *   - EAT the touch that pressed a key, before any widget sees it. A key that was
 *     a button would take the click, ImGui would deactivate the text field for
 *     losing focus, and the keyboard would close on its own first keystroke.
 * It appears when ImGui says something wants text (`io.WantTextInput`), takes the
 * bottom of the screen, and reports `height` for a host that wants to keep the
 * focused field above it. */
struct KeyboardState {
    bool shift = false;   // next letter upper case
    bool symbols = false; // the second page: digits and punctuation
    bool visible = false; // set by keyboard(): what it decided this frame
    float height = 0.0f;  // what it covered, in pixels (0 when hidden)
};

/* Draws when a field wants input; returns true if it drew. */
bool keyboard(KeyboardState& state, bool touch);

/* ── the action button (FAB) and its speed dial ──────────────────────────────
 * A menu bar sits where a thumb cannot reach; the primary action belongs in the
 * bottom corner. `fab` is the single button; `speed_dial` expands it into a
 * short labelled column — the mobile form of the canvas's add-palette.
 *
 * speed_dial returns the index of the entry tapped, or -1. `open` is the host's
 * view state (a bool it owns), toggled by the button itself. */
bool fab(const char* label, ImVec2 offset = ImVec2(0, 0));

int speed_dial(const char* str_id, const char* label, const std::vector<std::string>& entries,
               bool& open, ImVec2 offset = ImVec2(0, 0));

/* ── segmented control ───────────────────────────────────────────────────────
 * The touch form of a tab bar or a radio group: equal-width segments across the
 * full width, one selected. Hormiga's four workflows on a phone are this.
 * Returns true the frame the selection changes. */
bool segmented(const char* str_id, const std::vector<std::string>& labels, int& current,
               float width = 0.0f);

/* ── stepper ─────────────────────────────────────────────────────────────────
 * −/+ around a value. A drag-number is a desktop control: it needs sub-pixel
 * aim and it has no discoverable affordance under a finger. Small integer and
 * quantised fields get this instead.
 *
 * This is the raw control (host chrome) and it touches no model: the caller owns
 * the float and compiles whatever it means. The registry-side field editor —
 * `widget_field_stepper`, staged in widget.hpp so a glyph's `hints.editors` can
 * name "stepper" and every surface picks it up — is NOT built yet (backlog,
 * T2). Returns true the frame the value changes. */
bool stepper(const char* str_id, const char* label, float& value, float step = 1.0f,
             float vmin = 0.0f, float vmax = 0.0f, const char* fmt = "%.0f");

/* ── swipe-actionable row ────────────────────────────────────────────────────
 * A list row that reveals actions when dragged sideways — the gesture a phone
 * uses where a desktop uses a right-click. Hormiga's table view is the client.
 *
 * The row owns no truth and the actions are the host's: `actions` are labels,
 * and the return value is the index tapped (or -1). The revealed drawer closes
 * itself on a tap elsewhere or when another row opens, because `SwipeListState`
 * is shared by every row in one list and holds at most one open id.
 *
 * Usage:
 *   for (const auto& n : scene.nodes) {
 *       int hit = maiz::begin_swipe_row(swipe, n.name.c_str(), {"Delete"});
 *       … draw the row's content …
 *       maiz::end_swipe_row(swipe);
 *       if (hit == 0) commands.push_back(maiz::compile_deletes({n.name}));
 *   }
 */
struct SwipeListState {
    std::string open_id;  // the one row showing its drawer ("" = none)
    float offset = 0.0f;  // px the open row is displaced by
    bool dragging = false;
    std::string drag_id;
    float drag_from = 0.0f;
};

/* Returns the index of an action tapped THIS frame, or -1. Always pair with
 * end_swipe_row. */
int begin_swipe_row(SwipeListState& st, const char* row_id,
                    const std::vector<std::string>& actions, float height = 0.0f);
void end_swipe_row(SwipeListState& st);

} // namespace maiz
