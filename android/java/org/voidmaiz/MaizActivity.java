/*
 * MaizActivity — the one piece of Java a Void Maiz APK carries: the system
 * keyboard's side of the text-input holiday (okf/concepts/text-input.md).
 *
 * A NativeActivity cannot receive a keyboard properly. With no View that is a
 * text editor, Android's input methods fall back to sending bare key events,
 * and composing, autocorrect, accents (ñ, á, ü), suggestions, voice typing and
 * every non-Latin keyboard are lost. That is the wall Q29 described, and it is
 * why an ImGui-drawn keyboard was tried first. The author, 2026-09-23: *"Custom
 * keyboard is too much of a hassle (and other apps already deal with that, in
 * fact, we should focus on the integration of the keyboard)."*
 *
 * So this class adds ONE invisible View that tells Android "I am a text editor"
 * and hands the input method a real InputConnection over its own small buffer.
 * The keyboard edits that buffer as it would any text box; after every edit the
 * WHOLE editing state (text, selection, composing region) goes to native code
 * as UTF-8 with byte offsets. Native code (voidmaiz/textinput.hpp) turns it into
 * keystrokes for whichever ImGui field is active. The pixels stay Void Maiz's;
 * only the keyboard is Android's. That is the same split Flutter makes, and
 * its TextEditingValue is the same value.
 *
 * THIS CLASS HOLDS NO DECISIONS. Which keyboard a field gets (the InputType and
 * imeOptions integers) is computed in C++ (android_input_type, tested on a
 * desktop) and passed in. If you are about to add an `if` about a field here,
 * put it in textinput.cpp instead.
 *
 * Use: the manifest names this activity instead of android.app.NativeActivity,
 * with android:hasCode="true" and android:windowSoftInputMode="adjustNothing"
 * (the native surface must not be panned or resized; covered_px() reports how
 * much the keyboard covers, and the host decides what to do about it).
 * android/build_java.ps1 compiles it into classes.dex.
 */
package org.voidmaiz;

import android.app.NativeActivity;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.nio.charset.StandardCharsets;

public class MaizActivity extends NativeActivity {

    // TextKey's ordinals (voidmaiz/textinput.hpp)
    static final int KEY_LEFT = 0, KEY_RIGHT = 1, KEY_BACKSPACE = 2, KEY_DELETE = 3, KEY_ENTER = 4;

    // registered by maiz::android_text_input (RegisterNatives), never by name lookup
    static native void nativeState(byte[] text, int selStart, int selEnd, int composeStart, int composeEnd);
    static native void nativeKey(int key);
    static native void nativeAction(int action);
    static native void nativeCovered(int px);

    /* The natives exist only once native code has called android_text_input.
     * Until then (and in an APK that never does), nothing may call them. */
    private volatile boolean bound = false;

    private InputView view;
    private int inputType = EditorInfo.TYPE_CLASS_TEXT;
    private int imeOptions = EditorInfo.IME_ACTION_DONE;
    private int lastCovered = -1;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        view = new InputView(this);
        addContentView(view, new ViewGroup.LayoutParams(1, 1));
        final View decor = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= 30) {
            decor.setOnApplyWindowInsetsListener((v, insets) -> {
                covered(insets.getInsets(WindowInsets.Type.ime()).bottom);
                return v.onApplyWindowInsets(insets);
            });
        } else {
            decor.getViewTreeObserver().addOnGlobalLayoutListener(() -> {
                Rect r = new Rect();
                decor.getWindowVisibleDisplayFrame(r);
                covered(Math.max(0, decor.getRootView().getHeight() - r.bottom));
            });
        }
    }

    private void covered(int px) {
        if (!bound || px == lastCovered) return;
        lastCovered = px;
        nativeCovered(px);
    }

    private InputMethodManager imm() {
        return (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    // ── native → Java (any thread; the work happens on the UI thread) ────────

    public void maizShowKeyboard(final int type, final int options, final byte[] text,
                                 final int selStart, final int selEnd) {
        bound = true;
        runOnUiThread(() -> {
            inputType = type;
            imeOptions = options;
            view.load(text, selStart, selEnd);
            view.requestFocus();
            imm().restartInput(view); // a new field: a fresh session with its own keyboard
            imm().showSoftInput(view, 0);
        });
    }

    public void maizUpdateKeyboard(final byte[] text, final int selStart, final int selEnd) {
        bound = true;
        runOnUiThread(() -> {
            String s = new String(text, StandardCharsets.UTF_8);
            if (!s.contentEquals(view.buffer)) {
                view.load(text, selStart, selEnd);
                imm().restartInput(view); // the text changed under the keyboard
            } else {
                int a = chars(text, selStart), b = chars(text, selEnd);
                Selection.setSelection(view.buffer, a, b);
                imm().updateSelection(view, a, b, -1, -1);
            }
        });
    }

    public void maizHideKeyboard() {
        runOnUiThread(() -> imm().hideSoftInputFromWindow(view.getWindowToken(), 0));
    }

    // ── offsets: native speaks UTF-8 bytes, Java speaks UTF-16 chars ─────────

    static int chars(byte[] utf8, int bytes) {
        bytes = Math.max(0, Math.min(bytes, utf8.length));
        return new String(utf8, 0, bytes, StandardCharsets.UTF_8).length();
    }

    static int bytes(CharSequence s, int chars) {
        if (chars < 0) return -1;
        chars = Math.min(chars, s.length());
        return s.subSequence(0, chars).toString().getBytes(StandardCharsets.UTF_8).length;
    }

    // ── the invisible text editor ────────────────────────────────────────────

    final class InputView extends View {
        final SpannableStringBuilder buffer = new SpannableStringBuilder();

        InputView(Context c) {
            super(c);
            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        void load(byte[] utf8, int selStart, int selEnd) {
            buffer.clear();
            buffer.append(new String(utf8, StandardCharsets.UTF_8));
            BaseInputConnection.removeComposingSpans(buffer);
            Selection.setSelection(buffer, chars(utf8, selStart), chars(utf8, selEnd));
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo out) {
            out.inputType = inputType;
            out.imeOptions = imeOptions;
            out.initialSelStart = Selection.getSelectionStart(buffer);
            out.initialSelEnd = Selection.getSelectionEnd(buffer);
            return new Connection(this);
        }
    }

    /* A BaseInputConnection in full-editor mode over the view's buffer does all
     * the editing (composing spans, surrounding-text deletes, selection). We
     * only report the result, once per batch. */
    final class Connection extends BaseInputConnection {
        private int batch = 0;

        Connection(InputView v) {
            super(v, true);
        }

        @Override
        public Editable getEditable() {
            return view.buffer;
        }

        private boolean report(boolean r) {
            if (batch > 0 || !bound) return r;
            Editable e = view.buffer;
            int ss = Selection.getSelectionStart(e), se = Selection.getSelectionEnd(e);
            int cs = getComposingSpanStart(e), ce = getComposingSpanEnd(e);
            nativeState(e.toString().getBytes(StandardCharsets.UTF_8), bytes(e, ss), bytes(e, se),
                        cs < 0 ? -1 : bytes(e, cs), ce < 0 ? -1 : bytes(e, ce));
            return r;
        }

        @Override public boolean beginBatchEdit() { batch++; return super.beginBatchEdit(); }
        @Override public boolean endBatchEdit() {
            boolean r = super.endBatchEdit();
            if (batch > 0) batch--;
            return report(r);
        }
        @Override public boolean commitText(CharSequence t, int p) { return report(super.commitText(t, p)); }
        @Override public boolean setComposingText(CharSequence t, int p) { return report(super.setComposingText(t, p)); }
        @Override public boolean setComposingRegion(int a, int b) { return report(super.setComposingRegion(a, b)); }
        @Override public boolean finishComposingText() { return report(super.finishComposingText()); }
        @Override public boolean deleteSurroundingText(int a, int b) { return report(super.deleteSurroundingText(a, b)); }
        @Override public boolean deleteSurroundingTextInCodePoints(int a, int b) {
            return report(super.deleteSurroundingTextInCodePoints(a, b));
        }
        @Override public boolean setSelection(int a, int b) { return report(super.setSelection(a, b)); }

        /* The action key. It reaches the field as Enter (native_action). */
        @Override
        public boolean performEditorAction(int action) {
            if (bound) nativeAction(action);
            return true;
        }

        /* Some keyboards still send raw keys: backspace in an empty field,
         * Enter, arrows, and hardware keys. They edit the same buffer, so there
         * is still one path, and a raw key never reaches native twice. */
        @Override
        public boolean sendKeyEvent(KeyEvent ev) {
            if (ev.getAction() != KeyEvent.ACTION_DOWN) return true;
            Editable e = view.buffer;
            int ss = Selection.getSelectionStart(e), se = Selection.getSelectionEnd(e);
            switch (ev.getKeyCode()) {
            case KeyEvent.KEYCODE_DEL:
                if (ss != se) e.delete(ss, se);
                else if (ss > 0) e.delete(Character.offsetByCodePoints(e, ss, -1), ss);
                else if (bound) nativeKey(KEY_BACKSPACE); // nothing to delete here; let the field decide
                return report(true);
            case KeyEvent.KEYCODE_FORWARD_DEL:
                if (ss != se) e.delete(ss, se);
                else if (ss < e.length()) e.delete(ss, Character.offsetByCodePoints(e, ss, 1));
                return report(true);
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_NUMPAD_ENTER:
                if ((inputType & EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE) != 0) return commitText("\n", 1);
                if (bound) nativeKey(KEY_ENTER);
                return true;
            case KeyEvent.KEYCODE_DPAD_LEFT:
                if (se > 0) Selection.setSelection(e, Character.offsetByCodePoints(e, se, -1));
                return report(true);
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                if (se < e.length()) Selection.setSelection(e, Character.offsetByCodePoints(e, se, 1));
                return report(true);
            default:
                int c = ev.getUnicodeChar();
                if (c != 0) return commitText(new String(Character.toChars(c)), 1);
                return true;
            }
        }
    }
}
