---
type: Concept
title: Platforms
description: "What `platforms` in void.json claims and what it does not: a shipping record, not a compile claim. Why the distinction cost Void Hormiga a message, what CI now measures on all three desktops, and the one thing no runner can witness."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-09-08T00:00:00Z
---

Void Hormiga asked on 2026-09-08 whether Void Maiz builds on Linux and macOS.
They could not tell from anything we had published, and the reason is worth
more than the answer: **we had a field that looked like it answered the
question and did not.**

```json
"platforms": ["windows-x64", "android-arm64"]
```

They read that, correctly, as ambiguous — it could mean *what has been built*
or *what compiles* — and guessed it was the former, which it was. But both of
their binaries link us, so there is no configuration of Void Hormiga that
reaches a non-Windows desktop without Void Maiz on it, and a guess was not good
enough to plan on.

# The two claims, kept apart

| claim | what it means | where it lives |
|---|---|---|
| **shipped** | a binary has RUN there, in front of a person | `void.json` `platforms` |
| **compiles** | the library, tests and examples build; the headless suite passes | `.github/workflows/ci.yml` |

`platforms` is the **shipping record**. It is the narrower, more expensive
claim, and it is the one a download page should be built from — Void Hormiga's
already is, with two of three cards saying *"no build yet"* rather than
implying one exists.

**A platform joins the array when a GUI binary has actually run there in front
of a person.** No runner can witness that: CI has no display. That is not a
technicality — the whole point of this library is a canvas somebody looks at,
and "the shader compiled" is not "the window opened."

# What CI measures

`.github/workflows/ci.yml` builds on `windows-latest`, `macos-latest` and
`ubuntu-latest`: Void Core from source, then the library, the tests **and the
GUI examples** (so a compile failure in the window layer fails the job even
though nothing is displayed), then the headless ctest suite.

Two deliberate choices:

- **`fail-fast: false`.** The point is to see all three verdicts. A red Windows
  leg must never hide a green Linux one.
- **`reduce_conformance` is excluded from the gating run and reported
  separately.** It is knowingly at 17/25 because Void Core extended the portable
  reduce contract on 2026-09-01 with boxes, a reserved separator and a `patch`
  rule this port does not implement. Excluding it buys no green badge — it runs
  immediately afterwards where its number is visible. The reason it is excluded
  is that **a known-red test cannot also be a portability signal**: if it gates,
  every platform is red for a reason that has nothing to do with the platform,
  and the workflow answers nothing.

# What the audit found, before CI existed

Worth keeping, because it is why we expected a yes:

- **Exactly one `_WIN32` in `src/`** — `iso_now()` in `headless.cpp`, choosing
  `gmtime_s` over `gmtime_r`, properly `#else`-guarded.
- **Two platform conditionals in `CMakeLists.txt`**, both `if(WIN32)`-guarded:
  staging the Core DLL, and the MinGW runtime loader-order fix.
- **`find_package(OpenGL REQUIRED)` + `OpenGL::GL`**, not a hardcoded
  `opengl32`. GLFW 3.4 is vendored **as source**, and its own CMake builds Cocoa
  on Apple and both X11 and Wayland on other Unixes.
- **`VC_DLL` is safe off Windows.** It looks like the thing that would break —
  we define it unconditionally on desktop — but `voidcore.h` guards it as
  `#if defined(_WIN32) && defined(VC_DLL)`, so `VC_API` is simply empty
  elsewhere.

# The one real defect it found, and its shape

**All three examples asked for an OpenGL 3.0 context and a `#version 130`
shader.** macOS ships no OpenGL 3.0: it offers legacy 2.1, or 3.2+ **core
profile with forward compatibility**, and nothing between. A plain 3.0 request
does not fail — `glfwCreateWindow` succeeds and hands back 2.1 — and then the
`#version 130` shader fails to compile against it.

**The window opens and stays blank**, which is the worst shape a portability
bug can take: it presents as a rendering bug in the host's own code, in a file
the host wrote by copying ours.

That last clause is why the fix is a library function rather than three edits.
The examples are, by the build's own comment, *"the first thing a new client
copies"* — so a bug in them propagates by design.
[`voidmaiz/glhost.hpp`](/../include/voidmaiz/glhost.hpp)'s `gl_context_hints()`
sets the hints **and returns the matching GLSL version string**, so the two
halves cannot drift apart, because there is no longer a second place to write
either of them.

It is **reasoned and compiled, not witnessed.** Nobody has seen it draw on a
Mac. That is exactly the distinction this page exists to keep.

# What a first-time Linux builder hits

Not a source problem: GLFW 3.4 builds an X11 **and** a Wayland backend on Linux
by default, so configure needs both sets of development headers.

    sudo apt-get install xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols

Void Core must also be built first, as on every platform — desktop links it as a
prebuilt shared library.

# Status

`measured` (2026-09-08) for *compiles*; `asserted` for nothing. The array stays
`["windows-x64", "android-arm64"]` until somebody runs a GUI binary on a third
platform and says so. Raised by
`MESSAGE_FOR_VOIDMAIZ_hormiga-linux-and-macos-2026-09-08.md`; answered in
`../VoidHormiga/MESSAGE_FOR_VOIDHORMIGA_maiz-the-answer-is-two-and-here-is-the-runner-2026-09-08.md`.
