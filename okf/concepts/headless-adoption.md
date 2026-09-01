---
type: Concept
title: Headless — the adoption guide for an existing application
description: "The five steps an application already built on Void Maiz takes to gain a headless front-end, what it costs (a declaration, not a port), the two things a GUI host MUST change, and the mistakes we can already predict because we made them."
resource: include/voidmaiz/headless.hpp
tags: [status:current, audience:host, confidence:asserted]
timestamp: 2026-08-18T00:00:00Z
---

Audience: **an application already built on Void Maiz** — Void Hormiga,
InteractionCombinators, NodeBlocks, the native VLS. Read
[headless](/concepts/headless.md) first for *why*; this page is *how*.

The short version: **you are not porting anything.** Your gestures already
compile to dispatcher commands, your model already lives in Void Core, and the
briefing an agent reads is assembled from registries you already built. What you
add is a *declaration* and a second `main`.

# The whole thing, in one file

```cpp
#include "voidmaiz/headless.hpp"

maiz::HostApp build_app() {           // <- YOU ALREADY HAVE ALL OF THIS
    maiz::HostApp app;
    app.id       = "hormiga";
    app.label    = "Void Hormiga";
    app.version  = "1.4.0";
    app.okf_root = "okf";
    app.glyphs   = my_glyph_descriptors();   // the same list your GUI registers
    app.actions  = my_action_registry();     // your ActionRegistry, as-is
    app.predicates = my_preds.names();       // your Allomone vocabulary
    app.configure  = [](maiz::Core& c) { /* config set, transforms, domain */ };
    app.effects    = my_effect_handler;      // your save/deploy adapter
    return app;
}

int main(int argc, char** argv) { return maiz::run_cli(build_app(), argc, argv); }
```

Link `voidmaiz_headless`. It pulls in no ImGui, so the headless binary carries
no window system at all.

# The five steps

## 1. Hoist `HostApp` out of your GUI `main`

**This is the only step that touches existing code, and it is a move, not a
rewrite.** Your GUI already registers glyphs, builds an `ActionRegistry` and
installs an effect handler — today those calls are probably inline in `main` or
a `App::init`. Pull them into one `build_app()` and have *both* front-ends call
it.

**Why it matters more than it looks.** Glyphs are host config and are **not**
carried by the state document, so a headless session must register exactly what
the GUI registers. A glyph missing from one side means `rune new` fails there
and nowhere else; a glyph declared with a different field list means the
projection silently carries less. **One declaration is the only structural
defence against two front-ends disagreeing about one document.**

## 2. Add the headless entry point

A second `main`, or a flag on your existing one. If your GUI binary should also
work headlessly, branch early — before any window is created:

```cpp
int main(int argc, char** argv) {
    maiz::HostApp app = build_app();
    if (argc > 1 && std::string(argv[1]) != "--gui")
        return maiz::run_cli(app, argc, argv);
    return run_gui(app);
}
```

## 3. Agree on the state file

The headless session and the GUI must open the **same document**, and the
default is `<app.id>.state.json` in the working directory. If your GUI keeps its
document somewhere else — a config dir, a recent-files list, a project folder —
teach the headless side the same rule, or a user's agent will helpfully edit an
empty file forever.

**This failure is silent and shows stale data rather than erroring**, which is
the worst shape available. It is worth one test.

## 4. Declare your effects

```cpp
app.effect_ops = {
  {"save",   "Write to the real backend.", false,
             "overwrites the stored copy and moves the review baseline"},
  {"deploy", "Publish the site.",          false,
             "pushes to the public URL — visible to everyone, immediately"},
};
```

Effects are refused by default. Declaring an op does not enable it; it makes the
refusal *informative*, because the message quotes your `consequence` back at
whoever asked. Leave `consequence` empty and a person deciding whether to type
`--allow-effects` has nothing to decide with.

If an effect of yours is slow — a deploy, a build, a send — use
`streaming_effects` instead of `effects` and call `emit` as you go. Same
function, one extra parameter, and the difference between a watchable operation
and ninety silent seconds.

## 5. **Set `SessionKind::Human` in your GUI** ← do not skip this

If your GUI ever takes a `Session` (and it should, for the lock), it **must**
set `opts.kind = maiz::SessionKind::Human`.

That single line is the entire mechanism by which *a person outranks an agent*.
A GUI that leaves the default identifies itself as an agent and can be locked
out of its own document by a background task — which is precisely backwards, and
is the author's rule (2026-08-18): *"a person should always have more power than
an agent, even in the headless mode."*

# What you get for it

- `app --describe` — a JSON briefing: identity, verbs, glyph vocabulary, your
  action manifest with parameter schemas, your predicate names, your declared
  effects and whether they are permitted, the OKF bundle and how to read it, and
  the house rules an agent needs (including the quoting rule that has now bitten
  three codebases).
- `app <command>` / `--script` / `--repl` / `--json` / `--atomic`, plus `--`
  to end the options. Every argv element is passed through as exactly ONE
  argument, so shell-level quoting is all a caller needs — a value with
  spaces or apostrophes arrives whole, and an empty one arrives empty.
- A **journal** beside the document (`<state>.log`) in Void Core's line format,
  because the undo stack does not survive the process and the review story
  cannot rest on something that evaporates.
- An **advisory lock** with human precedence.
- The **effect gate**.

# Mistakes we can predict, because we made them

1. **Assuming `undo` reaches an earlier run.** It does not — Void Core's undo
   stack and log are session-scoped. Use `status` / `diff` / `revert` against
   `_baseline`, which *is* model content. Tell your users that, in those words.
2. **Dispatching `save` at the end of a headless run** because it sounds like
   the tidy thing to do. It snapshots `_baseline` and **erases the diff** that
   shows a person what the agent did. `Session::save()` writes the document and
   deliberately does not dispatch the verb; keep that distinction.
3. **Writing task scripts with naive quoting.** `set n text 'Don't forget'`
   closes the quote at the apostrophe. Inside single quotes the only escape is
   `\'`. This has now caused content corruption in C++ (Hormiga), Python
   (Reyna) and a plain text file (us). Since 2026-08-25 it **halts the run and
   names the line** rather than truncating silently, and a quoted value **may
   span several lines** — a task file is split into statements by Void Core's
   own reader, so a newline inside quotes is data. `maiz::arg()` fixes it in
   code and cannot fix it in a text file, so say it in your docs.
4. **Writing your own quoting, or your own tokenizer.** Don't — in either
   direction. `maiz::arg` / `command_line` encode, `maiz::split_argv` /
   `split_transcript` decode, and all four forward to Void Core's exported §6.1
   codec. Five implementations of that rule have been wrong, including ours and
   including the reference core; the careful ones are the dangerous ones,
   because they are right when written and go quietly wrong when the rule moves.
   In particular, **if you gate commands, read `split_argv(...).argv[0]`, never
   the leading token of the text** — our own effect gate let `'deploy'` through
   for exactly that reason.
5. **Leaving gestures anonymous.** An action that emits raw commands from a
   click handler is invisible to `--describe` by construction. Register it
   (`ActionDescriptor`) and the same `compile` serves the gesture and the agent —
   which is what the registry was for in the first place.
6. **A GUI that does not set `SessionKind::Human`.** See step 5.

# What headless does not give you

It adds **no verbs**. Anything an agent can do, a person could type at your
in-app console, and vice versa — that is the property that makes "paste this
voidscript" a legitimate answer to a support question. If a task needs a new
capability, that capability is a dispatcher verb or a registered action, and it
lands in both surfaces at once.

It is also **not concurrent with a live GUI** beyond the lock: one writer at a
time, with a person able to take the floor. Whether a running application should
instead *accept* an agent's commands over a local seam is **Q20**
([developer questions](/developer_questions.md)), leaning no, because it turns
every application into a server.
