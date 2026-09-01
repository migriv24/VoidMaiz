---
type: Concept
title: Headless — a Void Maiz application with no front-end
description: "The secondary objective (author, 2026-08-18): an agent in a real terminal should be able to USE a Void Maiz application — its engine, its vocabulary, its scripts — to complete a real task, and the changes must be waiting in the GUI when a person next opens it. The seam already existed; headless is the removal of a projection, not the addition of a capability."
resource: include/voidmaiz/headless.hpp
tags: [status:draft, audience:all, confidence:asserted]
timestamp: 2026-08-18T00:00:00Z
---

Part of [Void Maiz](/index.md). Founded 2026-08-18 on the author's direction.

# The ask, in the author's words

> *"I want to give an agent a task, and I want that agent to USE stuff from
> these applications (such as their engines and other modules) to actually
> complete the task. Likely from a real terminal."*

The worked example given: hand an agent a list of emails and some fliers and say
*"from this, update the database, and create a new newsletter. Also update the
website."* Void Hormiga already has every tool that needs. A second: *"set up a
basic looping drum pattern in VLS for me."*

And the constraint that makes it a real design problem rather than a scripting
convenience:

> *"We have to consider from a client perspective, someone who just installed an
> exe file or a deb file or something. Not a developer, not ME, but a client
> should also have the ability to do this as well."*

Plus the closing loop, which is the part that decides the architecture:

> *"Once an agent were to use an application in some headless manner, there are
> actual changes on the application itself. So when I open up Hormiga, it should
> be updated — I should still be able to see the newly added stuff. If something
> is slightly wrong, I should be able to modify it."*

# This does not contradict the GUI-first stance. It is its consequence.

Void Maiz's primary objective is unchanged: it is a **node-graph UI library**,
and the interface is the point. Headless is a **secondary objective**, and the
honest reason it costs almost nothing is that *the interface was never the
interaction surface*. Founding commitment 2 has said so since day one:

> **Every gesture is a visible, replayable command.** Humans, scripts, and AI
> agents share ONE interaction surface.
> — [total observability](/concepts/total-observability.md)

We have been writing "and AI agents" in that sentence for a month without ever
handing an agent a terminal. Headless is the sentence being cashed.

So the claim this concept makes is deliberately small:

> **A Void Maiz application is a dispatcher, a glyph vocabulary, an action
> vocabulary, and a state document. The GUI is one front-end over that. Headless
> is the same application with no front-end attached.**

If building it had required a second code path, a sync protocol, or an export
format, that would have been evidence the architecture was wrong. It requires
none of the three, and *that* is the finding worth recording.

# Why there is no synchronization problem

The hardest-sounding part of the ask — *"when I open up Hormiga it should be
updated"* — is the part that needs no work at all, and the reason is founding
commitment 1:

> **The model lives in Void Core; Void Maiz owns no truth.**
> — [log-first inheritance](/concepts/log-first-inheritance.md)

A headless session and a GUI session are not two copies of an application that
must be reconciled. They are two front-ends over **one state document**. There
is no sync because there is nothing to sync — the same property that let the
canvas be a pure projection lets a terminal be one.

Every node library that owns its graph would have to build an import path, a
merge, and a conflict story here. We inherit the absence of the problem, which
is the same argument [differentiation](/concepts/differentiation.md) already
makes about undo and persistence.

## And "if something is slightly wrong, I should be able to modify it" is free

The agent's work arrives as ordinary dispatcher commands, so:

- **It is editable**, because it produced ordinary runes — not an import, not a
  generated artifact with a "do not edit" header.
- **It is reviewable as a diff**, because a headless session persists the
  document without snapshotting `_baseline`. A person opening the app afterwards
  finds the agent's work as **unsaved changes**: `status` and `diff` name exactly
  what changed, and `revert` discards the whole run.
- **It is attributed**, through Void Core's attribution tier
  (`config set actor <name>`, SPEC §9) — persisted to a journal beside the
  document.

## Two corrections, found by driving it rather than by designing it

Both were written into this page as confident claims, and both were wrong. They
are recorded rather than quietly fixed, because the shape of the error is the
useful part: **the durable half of Void Core's history is model content, and the
convenient half is not.**

**1. `undo` does not survive the process.** The first draft said the agent's work
"is undoable, frame by frame, through the same `undo` the GUI uses." It is not:
Void Core's undo stack and log are **session-scoped**. The state document carries
the model, not the history of how it got there, so a second session's `history`
is empty and `undo` reports "nothing to undo" (measured 2026-08-18). Undo is live
and correct *within* a run and evaporates at exit.

That is invisible in a GUI, where a session and a sitting are the same thing. It
is exactly wrong for headless, where the entire point is that a person arrives
*later*.

Two consequences, both now built:

- **The journal.** A session appends the log spine to `<state>.log` in Void
  Core's own line format. The core performs no file I/O by contract (SPEC §9), so
  persisting the spine is the front-end's job — and without it, "what did the
  agent do" degrades to diffing JSON.
- **`revert`, not `undo`.** `_baseline` *is* model content and does survive. So
  the honest cross-process affordance is the saved-vs-working diff, which is
  better suited to the question anyway: a person reviewing an agent's run wants
  "show me everything it did and let me throw it away", not frame-by-frame.

**This makes `Session::save()` vs the `save` verb a real distinction.** Writing
the document is serialization; the `save` verb *also* snapshots `_baseline`. A
headless session deliberately does the first and not the second, because
snapshotting would erase the diff and make an agent's work indistinguishable
from the user's own. What began as an implementation detail turned out to be the
mechanism the review story rests on.

**2. An agent writing a task file by hand hits the quoting trap.** A script line
`set n text 'Don't forget'` closes the quote at the apostrophe — the same failure
that cost Void Hormiga a truncated script and Void Reyna a corpus of mangled
apostrophes, arriving a third time through a third door. `maiz::arg()` cannot
help here, because there is no C++ between the agent and the text. The only
available fixes are to **say the rule in the briefing**, which the house rules
do, and to **fail loudly when it is broken**, which since 2026-08-25 the reader
does: an unterminated quote halts the run and names the line it opened on,
instead of silently swallowing the rest of the file.

Three independent hits on one four-line rule is no longer a papercut; it is
evidence that the rule needs to be stated wherever text meets the dispatcher, not
just wherever C++ does — and, in the end, that no host should be writing the rule
at all. **Since 2026-08-25 Void Maiz implements none of §6.1**: `arg`,
`command_line`, `split_argv` and `split_transcript` forward to the codec Void
Core 0.2.7 exports, and the front-end reads a task file with
`vc_transcript_split_json` — the dispatcher's own reader — so a value containing
a newline is data rather than a second command. See the [log](/log.md),
2026-08-25.

An agent's session is therefore a *reviewable proposal that already landed*
rather than a black box. This is the propose-and-confirm shape Void Reyna
arrived at independently ([their finding](/log.md), relayed 2026-08-17), one
level down: Reyna proposes and Hormiga confirms; here the agent acts and the
human reviews, with the log as the diff.

# The client, not the developer

The distinguishing constraint. A developer can already do all of this — open a
REPL, link the library, write C++. The ask is that **someone who installed a
binary** can hand their agent a folder and a sentence.

That forces three properties:

1. **The capability must ship inside the application binary**, not in a
   developer toolchain. An app built on Void Maiz gets its headless front-end
   from the same `HostApp` declaration that builds its GUI — one declaration,
   two front-ends, which is [canvas actions](/concepts/canvas-actions.md)'
   one-definition property scaled up from a single action to a whole
   application.
2. **The application must describe itself to a stranger.** An agent arriving
   cold at an installed binary knows nothing. It cannot be expected to have read
   the source. So the headless front-end must answer *"what is this, what can I
   do, and how do I say it"* as a first-class command — see the briefing below.
3. **The state file must be discoverable and shared.** The agent must write
   where the GUI reads, without being told by a developer.

# The agent briefing

An agent needs one artifact that makes an unfamiliar application drivable. It is
assembled entirely from things that already exist, which is why it is honest:

| what | where it comes from | already existed? |
|---|---|---|
| identity, version, state path | the host's `HostApp` | new, one struct |
| the verb list | Void Core's `help` | yes |
| the glyph vocabulary — fields, ports, editors | Void Core's `glyphs` | yes |
| what is in the document now | `mantles`, `tree`, `ls` | yes |
| the view's interaction vocabulary | `ActionRegistry::manifest()` | **yes — since 2026-07-21** |
| the domain's condition words | `PredicateRegistry::names()` | yes |
| the design rationale | the app's own OKF bundle | yes |

The `ActionRegistry` row is the load-bearing one, and it is worth naming why: it
was built for Void Hormiga's Territory map so that *"a volunteer's click and an
agent's `map place contact @here` become the SAME transcript entry"*. That is
the headless requirement, written down and implemented four weeks before anyone
asked for headless. **The briefing is a reader for registries the library
already had.**

## The OKF is part of the briefing

The author's requirement: *"somehow it should have access to the OKF so that it
knows what it's doing."* We do not reimplement retrieval — Void Core's OKF
holiday already serves a bundle (`python -m okf --bundle <dir> ls | query |
get --head`), and `get --head` exists precisely so an agent can read a page's
header, description and link graph for ~1.3 KB instead of pulling 17 KB of body
(measured, 2026-08-09).

So the briefing **names the bundle and the retrieval verb** rather than
embedding documentation. An agent that needs to know why the application is
shaped as it is reads the concept; an agent that only needs to act reads the
vocabulary. That split is the whole reason `--head` was asked for.

## It is not the surface census, and the two must not merge

[Surface census](/concepts/surface-census.md) harvests the same registries. The
difference is the consumer and therefore the output:

- **Census → documentation.** Emits dispatcher commands building `okf-concept`
  runes, so Void Core's OKF engine can produce markdown. Its output is a *doc
  mantle*. It is for humans and for drift detection.
- **Briefing → drivability.** Emits one JSON document describing how to operate
  *this* installed application right now. Its output is *ephemeral* and touches
  no mantle.

They read the same inputs and must keep doing so — if the two ever disagree
about what an application affords, one of them is lying. But a census that
wrote a doc mantle every time an agent asked "what can you do" would be putting
documentation into the user's model, which is exactly the kind of
source-of-truth creep the library refuses.

# Effects — the one-way door

The whole review story above rests on one property: **everything the agent did
is in the document, so `revert` can throw it away.** An effect is where that
stops being true. `deploy` pushes a live site. `save` writes a real backend.
"Create a newsletter" ends, eventually, in mail leaving a server.

So the boundary is not between *reading* and *writing* — a headless agent writes
freely, and should. It is between **work and publication**, and those are two
different grants:

| | reversible? | who should grant it |
|---|---|---|
| dispatcher commands | yes — `diff`, `revert` | implied by running the agent at all |
| effect verbs | **no** | a person, explicitly, per run |

This maps exactly onto the author's own example. *"From this, update the
database, and create a newsletter. Also update the website."* The database work
and the newsletter draft are document work: an agent should do all of it
unattended, and a person should review it afterwards as a diff. **Updating the
website is the part a person wants to look at first anyway.** The gate is not a
restriction on the workflow; it is the shape the workflow already had.

## The default refuses, and says what you were asking for

`EffectPolicy::Refuse` is the default. `--allow-effects[=a,b]` opens the door
for a run (all ops, or named ones); `--dry-run-effects` rehearses.

A refusal quotes the host's own `EffectOp::consequence` back at the caller:

```
refused: `deploy` reaches outside the document and this session was not
granted effects (deploy: pushes to the public URL — visible to everyone,
immediately). Re-run with --allow-effects=deploy if that is intended, or
--dry-run-effects to rehearse it.
```

That sentence is the reason `EffectOp` exists at all. `Core::EffectHandler` is
one opaque `std::function`, so nothing could enumerate what an application can
do to the world, let alone what it would mean — the same gap `ActionDescriptor`
closed for a view's gestures, at the other seam, closed the same way: **name it,
document it, and let the library enumerate without ever deciding what it means.**
`consequence` is the field a host will be tempted to leave empty and the only
one a person actually needs.

## Two mechanisms, because of two things upstream

Read out of `verbs_lifecycle.c` on 2026-08-18, and both are why the gate is not
just a wrapper around the effect handler:

1. **`save` snapshots the baseline unconditionally.** `vc_snapshot_baseline`
   runs after the adapter call regardless of what the adapter returned. So a
   refusal at the *handler* would still move `_baseline` — erasing precisely the
   diff that tells a person what the agent did. The gate therefore intercepts at
   the **verb**, before the core sees the command.
2. **An effect handler cannot fail a dispatch.** The result is built with
   `res_make(1)` whatever comes back; the handler's value becomes `data`, never
   `ok`. A refusal that only spoke through the handler would be reported to the
   caller as a **success**, which is the worst possible outcome for a safety
   mechanism.

Hence the verb gate is the real one, backed by a second gate inside the handler
wrapper for anything reaching the seam by a route the verb gate cannot read (a
`script` body). `batch` payloads are parsed and gated too, because an atomic
batch is the obvious way to hide a `save` behind an innocent leading verb.

Both upstream behaviours are reported to Void Core
(`MESSAGE_FOR_VOIDCORE_maiz-headless-history-and-derived-ids-2026-08-18.md`) as
questions rather than demands — a save adapter that fails should probably not
report success, but that is their call.

## What is NOT the answer

- **Not a permission prompt.** Unattended means unattended; a gate that asks a
  question nobody is there to answer is a hang.
- **Not a heuristic.** The gate never guesses whether an effect "looks safe". It
  matches op names against an explicit grant, and that is all.
- **Not silent success.** A refused command fails. Everything else invites an
  agent to believe it published.

# The two workflows, and why both exist

The author's framing, kept because it is the right division:

> *"For a longer task, get an agent to do it headless. For a short task, open up
> the application and paste in some voidscript to do stuff (in the future, using
> Allomone)."*

| | **in-app console** | **headless** |
|---|---|---|
| who drives | a person, watching | an agent, unattended |
| task length | short, one gesture's worth | long, many steps |
| feedback | immediate, visual | the log, reviewed after |
| what it needs | the app already open | a terminal and a state file |

Both go through the same dispatcher, so neither is a lesser path. The in-app
console is not a debug affordance and headless is not an export — they are the
same seam approached from two directions, which is the property that makes
"paste this voidscript" a legitimate answer to a support question.

# What headless is NOT

Naming the boundary so it does not creep:

- **Not a daemon or a server.** A session is a process: load, act, save, exit.
  Long-running multi-client access is a different design with a locking story we
  do not have.
- **Not concurrent with a running GUI.** Two writers over one state file is a
  real problem, and the first cut does not solve it — it detects it. See
  "Concurrency" below.
- **Not a second command language.** The headless front-end adds *no* verbs a
  human at the in-app console could not type. If a task needs a new capability,
  that capability is a dispatcher verb or a registered action, available to
  both.
- **Not a way around the log.** Every mutation an agent makes is logged and
  attributed. An agent that could act silently would break the founding
  commitment, and there is deliberately no flag for it.
- **Not headless *rendering*.** No offscreen canvas, no image output. That is a
  separate and much larger question.

# A person outranks an agent

The author's rule, 2026-08-18:

> *"A live GUI overrides whatever is in the headless component. … I don't think
> we should have a big distinction between them though (because I don't assume
> rogue agents or anything), but a person should always have more power than an
> agent, even in the headless mode."*

The first cut had a plain advisory lock: first come, first served. That quietly
encoded the opposite rule — an agent that started at 09:00 could keep someone
locked out of their own application all morning.

**The rule is one line: a Human session may take the lock from an Agent session.
Nothing else preempts anything.** Two humans collide normally. An agent never
displaces anyone. There is no trust model here and no threat model; it is the
ordinary observation that when two parties want the same document, the human's
want is the one that counts.

## What "overrides" actually has to mean

Taking a lock is the easy half and the unimportant half. The property that
matters is that **the evicted agent cannot overwrite the person's document
afterwards** — otherwise precedence is a courtesy message and the agent still
wins at save time, which is the only moment that counts.

So the lock file carries a **token** the holder minted, and a session asks *"is
the lock still mine?"* rather than *"is there a lock file?"* — the second stays
true after someone else takes it. Then:

- **`dispatch` checks before running**, so a long agent run stops at the next
  step rather than doing another four hundred into a document it can no longer
  write. One filesystem read per command is nothing at headless rates, and the
  alternative is discovering it after all the work.
- **`save` refuses**, re-checked there because it is reachable directly and it
  is the only place the harm actually happens.
- **`close` writes nothing and does not delete the lock** — tidying up a lock
  that is no longer ours would hand the document to whoever asks next.
- **The flag is sticky.** Even if the person closes and the lock frees, whatever
  they did in between *is* the document now, and the agent's in-memory copy is a
  fork of it.

Losing an agent's uncommitted half-hour is a far smaller harm than silently
overwriting the document a person is editing, and it is the only one of the two
that can be explained afterwards.

Two smaller decisions, both conservative on purpose: a lock file that cannot be
parsed is treated as held by an unknown **human** (precedence must never evict a
person on the strength of a corrupt file), and the refusal message keys on **who
is holding it**, not who is asking — an agent refused by another agent was
briefly being told "a person is using this document", which is both false and
unactionable. Found by driving it.

**A GUI host must therefore set `SessionKind::Human`.** That one line is the
entire mechanism; a GUI that leaves the default identifies itself as an agent
and can be locked out of its own document by a background task. It is step 5 of
[the adoption guide](/concepts/headless-adoption.md) and the only step there we
would call mandatory.

## Still not concurrent

One writer at a time, with a person able to take the floor. Whether a *running*
application should instead accept an agent's commands over a local seam — making
the in-app console and the headless agent literally the same thing — stays
**Q20** in [developer questions](/developer_questions.md), leaning no, because
it turns every application into a server with a port, a lifetime and an
authorization question.

# Staging

- **Phase H0 — the session and the front-end.** `HostApp`, `Session` (load →
  dispatch → save, with attribution and the advisory lock), and `run_cli`:
  one-shot, script file, stdin, REPL, `--json`. The state document is the whole
  persistence story.
- **Phase H1 — the briefing.** `capabilities()` as JSON, assembled from `help`,
  `glyphs`, `mantles`, `ActionRegistry::manifest()`, predicate names, and the
  OKF root.
- **Phase H2 — actions from the terminal.** Invoking a registered
  `ActionDescriptor` by name with named arguments, which is the piece that lets
  an agent use a *view's* vocabulary without the view.
- **Phase H3 — the effect seam** *(built)*. `save` / `deploy` / `build` from
  headless, so "also update the website" is one command rather than a
  developer's errand — behind an explicit grant, because effects are the one-way
  door (above). `EffectOp` declares what an application can do to the world;
  `EffectPolicy` decides whether this run may. What is still missing is a
  **worked client task** proving it against a real backend rather than a witness
  file.
- **Phase H4 — precedence and streaming** *(built)*. A person outranks an agent
  (above); streaming effects (`StreamingEffectHandler` + `emit`) reach the
  terminal and the journal line by line, as SPEC §9 requires. Plus
  [the adoption guide](/concepts/headless-adoption.md), which is what the other
  applications actually consume.
- **Later** — true concurrency with a live GUI (**Q20**); a dry-run for the
  *whole* transcript, not only effects; and getting emitted effect lines into
  Void Core's own `log` buffer, which today they cannot reach (the C ABI exposes
  no host log-write, so they live in our journal only — reported upstream).
