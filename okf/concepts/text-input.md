---
type: Concept
title: Text input — the platform keyboard as a holiday
description: "Q29 answered by the author on 2026-09-23: integrate the device's keyboard, don't draw one. The keyboard is a holiday: an external domain (the platform's input method) mapped onto the GUI through one pure mapping. What the device controls versus what we control; the pivot (an editing state, the same value Flutter's TextEditingValue carries); the mapping (plan_edit: states in, keystrokes out, so EVERY existing ImGui field accepts it unchanged); the Android crossing (one Java class, MaizActivity, with an InputConnection); field kinds chosen by registration; the Flutter investigation and its verdict; and three things found on the way: emoji were being destroyed on input (IMGUI_USE_WCHAR32), Android's Back button reached no application, and FindClass cannot see an app's classes from a native thread."
resource: include/voidmaiz/textinput.hpp
tags: [status:current, audience:dev, audience:host, confidence:measured]
timestamp: 2026-09-23T00:00:00Z
---

The author, 2026-09-23:

> at least in the interaction Combinators demo app, a keyboard exists but it
> doesn't work at all. I do plan on the key board being important for mobile
> (lots of fields to enter things into, not just tags but descriptions and
> eventually stuff for a document builder). I think the limitation is that the
> system isn't calling for the device's keyboard? Rather it's making it's own? I
> don't think we want to do that.

> we really want complete control of the GUI. That's why maiz is very particular
> about the registration of GUI things and such ... we definitely should begin
> considering what we want the device to control and what we want to control.
> Custom keyboard is too much of a hassle (and other apps already deal with that,
> in fact, we should focus on the integration of the keyboard)!

> Actually yeah, the integration of a keyboard is very similar to a holiday is it
> not? An external domain that has to map onto our GUI mantle?

That answers **Q29** (see [developer questions](/developer_questions.md), where
it is cleared). The lean there had been an ImGui-drawn keyboard, to keep the APK
free of Java. The author chose the other way, and the reasons are in the
quotations: a drawn keyboard cannot do what people expect of a keyboard, and
other applications already solved it.

# What the device controls, and what we control

| the device controls | Void Maiz controls |
|---|---|
| the keyboard: layout, language, accents (ñ, á, ü), composing, autocorrect, predictions, swipe typing, voice typing, emoji, other alphabets, accessibility | every pixel of the application, including the field being typed into, its caret and its selection |
| when the keyboard is shown, animated and hidden, and how much it covers | **which kind of keyboard a field gets** (a phone number gets a dial pad), decided by registration |
| the action key's drawing ("Done", "Search", "Go") | what the action key *does* (it reaches the field as Enter) |
| the Back button when the keyboard is up (it closes the keyboard) | what Back means otherwise (`android_system_key` → `ImGuiKey_AppBack`) |

The line is the same one Flutter draws. Flutter renders every pixel itself and
still hands the keyboard to the platform, across a channel that carries an
**editing state**. Investigating Flutter (below) found no reason to adopt it and
every reason to copy that line.

# The holiday

A holiday is an effectful crossing plus a pure mapping
([Void Hormiga's account](../../../VoidHormiga/okf/concepts/platform/antfarm/holidays.md)).
Here:

| part | what it is | where |
|---|---|---|
| **the world** | the platform's input method | Android's IME, through `org.voidmaiz.MaizActivity` |
| **the pivot** | `EditingState`: the field's text, selection and composing region, as UTF-8 with byte offsets | `voidmaiz/textinput.hpp` |
| **the mapping** | `plan_edit(text, cursor, state) → EditPlan`: arrows, backspaces, characters, arrows. **Pure** | `src/input/textinput.cpp`, pinned by `textinput_smoke` |
| **the crossing** | `TextInputPlatform`: show, update, hide, poll | `src/input/textinput_android.cpp` + the Java class |
| **the glue** | `text_input_frame`: watch ImGui's active field, drive the platform, emit the plan | `src/view/textinput.cpp`, pinned by `textinput_view_smoke` |

**Why keystrokes and not a buffer swap.** Every text field in every Void Maiz
application is an ImGui `InputText`. Most are drawn by the widget registry and
none was written to cooperate with a keyboard. Arrows, backspace and characters
are the one input every one of them already accepts, so the holiday reaches
every field there is, and **no field changes to be typed into**. The drawn
keyboard had the same property, and it is kept.

**Why whole states and not events.** An input method thinks in states: it
rewrites the word being composed, autocorrect replaces a word behind the cursor,
a suggestion replaces the composing span. Deltas lose that. States reconcile.
`plan_edit` keeps the common prefix and suffix, replaces only what changed, and
always lands on the keyboard's text with the cursor where the keyboard put it.
The test replays each plan on a model of a field to prove exactly that, for
typing, composing, autocorrect, Spanish accents, emoji and cursor moves.

**When the field changes without the keyboard** (a tap moves the caret, a
hardware key types), the glue tells the keyboard with `update`, but only after
two quiet frames with ImGui's input queue empty. ImGui trickles key presses
across frames, so a field read in mid-plan is half-applied, not diverged.

# Registration picks the keyboard

This is where Void Maiz's particularity about registration pays. The kind of
keyboard is a property of **what the field is**, and the registry already knows
that:

- `widget_field_text` calls `text_input_kind(input_kind_for(field_key))`, so a
  field keyed `phone` gets a dial pad, `email` an @ row, `website` a URL
  keyboard, and `display_name` capitalised words without autocorrect;
- `widget_field_multiline` asks for a newline key;
- a declared editor speaks first (`email`, `multiline:70`, `number`, …);
- a host's own field says it in one line after the field:
  `maiz::text_input_kind(maiz::InputKind::Search)`.

`InputKind` is Flutter's `TextInputType` minus the kinds nobody has asked for,
plus `Code` for rune names, tags and commands (no capitals, no autocorrect).
The mapping to Android's integers (`android_input_type`,
`android_ime_options`) is pure and tested on a desktop, so the Java shim holds
no mapping of its own. The IME is **never allowed full-screen**
(`IME_FLAG_NO_EXTRACT_UI`): an input method in extract mode covers the
application with its own text box, which is exactly the loss of control the
author ruled out.

# The Android crossing

**One Java class**, `android/java/org/voidmaiz/MaizActivity.java`, a
`NativeActivity` subclass with one invisible `View` that says it is a text
editor and returns a `BaseInputConnection` in full-editor mode over its own
small buffer. The input method edits that buffer as it would any text box.
After every edit (or at the end of a batch), the whole state goes to native code
as UTF-8 with byte offsets. Native code calls back to seed the buffer when a
field activates (`restartInput`, so each field gets a fresh session with its own
keyboard type), to report a moved caret (`updateSelection`), and to hide.

- **The zero-Java claim ends here, knowingly.** [Substrates](/concepts/substrates.md)
  recorded that the first APK shipped with no Java at all. Without Java there
  is no InputConnection, and without one an input method falls back to bare key
  events: no composing, no accents, no autocorrect. The rule that survives is
  the footnote substrates already had: *platform shims are build scaffolding with
  zero logic in them.* The class comment says to move any `if` about a field
  into `textinput.cpp`.
- **No Gradle.** `android/build_java.ps1` runs `javac` (Android Studio's JBR)
  against the platform's `android.jar`, then `d8`, and yields a 10.8 KB
  `classes.dex` that a host's APK script adds at the root. Interaction
  Combinators' `build_apk.ps1` does, in four lines.
- **Found, and designed around:** `FindClass` on a native thread sees only the
  system class loader and cannot find an application's classes, which is the
  usual way this kind of shim fails silently. Everything here goes through
  `activity->clazz` (the activity *object*) and `RegisterNatives` on its class.
- **A plain NativeActivity still works.** `android_text_input` returns null,
  logs one line saying why, and the host keeps `maiz::keyboard()` (the drawn one)
  as its fallback.
- **How much the keyboard covers** is reported (`covered_px`, from WindowInsets
  on API 30+ and the visible frame below that), and the manifest says
  `adjustNothing`, so the native surface is never panned or resized behind the
  application's back. What to do about the covered strip is the host's call.

# The Flutter investigation

The author: *"In general I would like to implement flutter and dart into void
maiz, specifically for mobile devices ... So while we can't directly use flutter
and dart (maybe we can? It's worth investigating)."* Checked against the Flutter
SDK on this machine (3.41.6), `packages/flutter/lib/src/services/text_input.dart`.

**What Flutter is:** a rendering engine (Skia, and now Impeller) that draws
every pixel of the UI itself, a widget framework in Dart, and a Dart VM, shipped
as a native engine library inside every APK. Dart reaches C through `dart:ffi`,
so Void Core's C ABI could be called from Dart.

**Three ways it could have joined, and why none of them did:**

1. **Flutter as the mobile UI**, with Void Core underneath through FFI. That
   means **a second UI**: every widget, face, canvas, presence mark and registry
   editor Void Maiz has would need a Dart twin, and the two would drift. It also
   inverts "complete control of the GUI". Flutter's widgets would own the
   pixels, and Void Maiz's registration would become advice.
2. **Flutter for the text fields only**, as views over the ImGui surface. That
   means two rendering stacks, two input systems and two layout systems on one
   screen, plus the engine and the Dart runtime in the APK, to get a keyboard.
3. **Take what Flutter does for the keyboard.** Flutter's own text input is a
   platform channel carrying `TextEditingValue` (text, selection, composing),
   configured by a `TextInputType` and a `TextInputAction`
   (`TextInput.setClient`, `setEditingState`, `show`, and
   `updateEditingState` back). On Android that channel ends in Java: an
   InputConnection over an editable buffer. **That is this design.**

**Verdict: (3).** Flutter is the best existing proof that a toolkit can own every
pixel and still integrate the platform keyboard properly. The author's two
requirements are Flutter's own two choices. Nothing needed Dart. If a Flutter
question comes back, it should be about something Flutter has that Void Maiz
lacks (its accessibility tree is the likeliest), and it should be asked the
same way: what does it do at the platform seam, and can that seam be ours?

# Found on the way

1. **Every emoji typed into any field became U+FFFD.** ImGui's `ImWchar` is 16
   bits by default, so any character beyond U+FFFF is replaced *as it is typed*.
   Text already stored as UTF-8 was never touched, which is why nobody saw it on
   a desktop. On a phone keyboard emoji are ordinary, and a contact whose name is
   quietly rewritten is data loss. `IMGUI_USE_WCHAR32` is now a PUBLIC
   definition on `voidmaiz_imgui`, so every file of every host agrees. Found by
   `textinput_view_smoke`, the first test in this repository that runs ImGui.
2. **Android's Back button reached no application.** ImGui's android backend
   maps no `AKEYCODE_BACK`. `android_system_key(event)` turns it into
   `ImGuiKey_AppBack`. It is opt-in, because consuming Back means the activity
   no longer finishes on it: Interaction Combinators keeps the platform's Back
   (exit), and Void Hormiga's phone takes it for navigation.
3. **Why the drawn keyboard "doesn't work at all" in Interaction Combinators was
   not diagnosed.** It is now the fallback rather than the path, and the system
   keyboard replaces it wherever the shim is present. If the fallback matters
   again, it needs a device and ten minutes, in the standing rule's words.

# Using it (a host)

```cpp
// once, after the app's init (Android shell):
auto keyboard = maiz::android_text_input(activity);   // null without MaizActivity
// every frame, first thing after NewFrame:
maiz::text_input_frame(session, keyboard.get());
if (!keyboard) maiz::keyboard(drawn, touch_mode);      // the fallback only
// in the input handler, if Back should navigate rather than exit:
if (maiz::android_system_key(event)) return 1;
```

and in the manifest: `android:name="org.voidmaiz.MaizActivity"`,
`android:hasCode="true"`, `android:windowSoftInputMode="adjustNothing"`; in the
APK script: `android/build_java.ps1`, then add `classes.dex`.

# What this does not prove

**Nobody has typed on a phone with it.** There is no device or emulator image
on the machine it was built on. What is measured:
- the mapping, on a model of a field (`textinput_smoke`);
- the glue driving a real ImGui field with a fake platform keyboard
  (`textinput_view_smoke`: typing, composing, accents, emoji, a moved caret
  reported back, the action key committing and the keyboard hiding);
- the Java compiling against API 36 and dexing;
- the NDK compiling the JNI side into Interaction Combinators' APK;
- every Java↔native signature matching in the built APK (`dexdump` against the
  strings the `.so` looks up).

What is reasoned and not witnessed: that Gboard and Samsung's keyboard drive
`BaseInputConnection` the way the documentation says, that
`showSoftInput` succeeds for a view added to a NativeActivity's window, and the
covered-height reporting. The first person to try it needs the Interaction
Combinators APK, a text field (a tag, a rune name, Save As), and the checklist
at the end of the [log](/log.md) entry for 2026-09-23.
