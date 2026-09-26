/*
 * MaizActivity — the one piece of Java a Void Maiz APK carries: the system
 * keyboard's side of the text-input holiday (okf/concepts/text-input.md), the
 * safe area, and (2026-09-25) the system's document picker and save dialog
 * (voidmaiz/documents.hpp), which is how a photo or a file gets on and off a
 * phone.
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
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.HashMap;

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

    /* THE SAFE AREA (voidmaiz/mobile.hpp, maiz::android_safe_area): pixels at
     * {left, top, right, bottom} that belong to the system. A NativeActivity's
     * surface runs under the status bar, the cutout and the navigation area; on a
     * gesture-navigation phone the bottom strip takes every touch as a gesture.
     * Bottom is therefore the larger of the navigation bar and the MANDATORY
     * gesture area. Read-only, and called from the native thread. */
    @SuppressWarnings("deprecation") // the pre-Android-11 branch: deliberate, and javac's note broke the build script
    public int[] maizSafeInsets() {
        int[] out = new int[4];
        WindowInsets wi = getWindow().getDecorView().getRootWindowInsets();
        if (wi == null) return out;
        if (Build.VERSION.SDK_INT >= 30) {
            android.graphics.Insets bars =
                wi.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            android.graphics.Insets gestures = wi.getInsets(WindowInsets.Type.mandatorySystemGestures());
            out[0] = bars.left;
            out[1] = bars.top;
            out[2] = bars.right;
            out[3] = Math.max(bars.bottom, gestures.bottom);
        } else {
            out[0] = wi.getSystemWindowInsetLeft();
            out[1] = wi.getSystemWindowInsetTop();
            out[2] = wi.getSystemWindowInsetRight();
            out[3] = wi.getSystemWindowInsetBottom();
            if (Build.VERSION.SDK_INT >= 28 && wi.getDisplayCutout() != null) {
                android.view.DisplayCutout c = wi.getDisplayCutout();
                out[0] = Math.max(out[0], c.getSafeInsetLeft());
                out[1] = Math.max(out[1], c.getSafeInsetTop());
                out[2] = Math.max(out[2], c.getSafeInsetRight());
                out[3] = Math.max(out[3], c.getSafeInsetBottom());
            }
        }
        return out;
    }

    // ── documents: the system's picker and save dialog (voidmaiz/documents.hpp) ─
    /* A pick or a save is a request now and a result later: the system's own UI
     * runs over this activity, which is paused meanwhile. The picked bytes are
     * COPIED into a folder native code named, on a background thread, and only
     * the copy's path is reported, so native code never meets a content URI.
     * Results wait in a queue until native code asks (maizTakeDocument). */

    static final int DOC_BASE = 0x4d00; // request codes this class owns
    private final ArrayDeque<String[]> docResults = new ArrayDeque<>();
    private final HashMap<Integer, String[]> docPending = new HashMap<>();

    private void docResult(int request, String status, String path, String name) {
        synchronized (docResults) {
            docResults.add(new String[] {Integer.toString(request), status, path, name == null ? "" : name});
        }
    }

    public boolean maizPickDocument(final int request, final String mime, final String destDir) {
        runOnUiThread(() -> {
            Intent i;
            if (mime.startsWith("image/") && Build.VERSION.SDK_INT >= 33) {
                // the photo picker: no permission, the person picks, the pick is the consent
                i = new Intent(MediaStore.ACTION_PICK_IMAGES);
            } else {
                i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType(mime);
            }
            docPending.put(request, new String[] {"pick", destDir});
            try {
                startActivityForResult(i, DOC_BASE + request);
            } catch (Exception e) {
                docPending.remove(request);
                docResult(request, "error", "", "no app on this device can open that picker");
            }
        });
        return true;
    }

    public boolean maizSaveDocument(final int request, final String src, final String name, final String mime) {
        runOnUiThread(() -> {
            Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType(mime);
            i.putExtra(Intent.EXTRA_TITLE, name);
            docPending.put(request, new String[] {"save", src});
            try {
                startActivityForResult(i, DOC_BASE + request);
            } catch (Exception e) {
                docPending.remove(request);
                docResult(request, "error", "", "no app on this device can save a file");
            }
        });
        return true;
    }

    public String[] maizTakeDocument() {
        synchronized (docResults) {
            return docResults.poll();
        }
    }

    /* The file this activity was started or resumed WITH (an "Open with" from a
     * file manager or a chat): copied like a pick, reported as request -1, and
     * forgotten so a resume does not deliver it twice. */
    public void maizTakeOpenedDocument(final String destDir) {
        runOnUiThread(() -> {
            Intent it = getIntent();
            if (it == null || !Intent.ACTION_VIEW.equals(it.getAction()) || it.getData() == null) return;
            final Uri uri = it.getData();
            setIntent(new Intent()); // consumed
            copyIn(-1, uri, destDir);
        });
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent); // the next maizTakeOpenedDocument sees it
    }

    @Override
    protected void onActivityResult(int code, int result, Intent data) {
        final int request = code - DOC_BASE;
        final String[] p = docPending.remove(request);
        if (p == null) {
            super.onActivityResult(code, result, data);
            return;
        }
        if (result != RESULT_OK || data == null || data.getData() == null) {
            docResult(request, "cancelled", "", "");
            return;
        }
        final Uri uri = data.getData();
        if (p[0].equals("pick")) {
            copyIn(request, uri, p[1]);
        } else {
            new Thread(() -> {
                try (InputStream in = new FileInputStream(p[1]);
                     OutputStream out = getContentResolver().openOutputStream(uri)) {
                    pipe(in, out);
                    docResult(request, "ok", uri.toString(), displayName(uri));
                } catch (Exception e) {
                    docResult(request, "error", "", String.valueOf(e.getMessage()));
                }
            }).start();
        }
    }

    private void copyIn(final int request, final Uri uri, final String destDir) {
        new Thread(() -> {
            try {
                File dir = new File(destDir);
                dir.mkdirs();
                String name = safeName(displayName(uri));
                File out = new File(dir, name);
                int dot = name.lastIndexOf('.');
                String stem = dot > 0 ? name.substring(0, dot) : name, ext = dot > 0 ? name.substring(dot) : "";
                for (int n = 2; out.exists(); ++n) out = new File(dir, stem + "-" + n + ext);
                try (InputStream in = getContentResolver().openInputStream(uri);
                     OutputStream o = new FileOutputStream(out)) {
                    pipe(in, o);
                }
                docResult(request, "ok", out.getPath(), name);
            } catch (Exception e) {
                docResult(request, "error", "", String.valueOf(e.getMessage()));
            }
        }).start();
    }

    private String displayName(Uri uri) {
        String name = null;
        try (Cursor c = getContentResolver().query(uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) name = c.getString(0);
        } catch (Exception ignored) {
        }
        if (name == null || name.isEmpty()) name = uri.getLastPathSegment();
        return name == null || name.isEmpty() ? "picked" : name;
    }

    /* A name for OUR folder: no separators, nothing hidden, nothing empty. */
    static String safeName(String n) {
        n = n.replaceAll("[/\\\\:*?\"<>|\\p{Cntrl}]", "_");
        while (n.startsWith(".")) n = n.substring(1);
        return n.isEmpty() ? "picked" : n;
    }

    static void pipe(InputStream in, OutputStream out) throws java.io.IOException {
        if (in == null || out == null) throw new java.io.IOException("the file could not be opened");
        byte[] buf = new byte[64 * 1024];
        for (int r; (r = in.read(buf)) > 0;) out.write(buf, 0, r);
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
