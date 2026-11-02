---
type: Concept
title: Void Rancho, beside us
description: "How a Void Maiz host uses Void Rancho, the family's database layer (2026-10-30): one Core shared through maiz::RanchoLink, one copy of graph analysis (Rancho's since the author ruled Q43 on 2026-11-02; voidmaiz_graph is now a bridge to voidrancho_graph), one libvoidcore per process on desktop and on phones, and what Rancho needs to run on iOS and Android."
resource: include/voidmaiz/rancho.hpp
tags: [status:current, audience:dev, audience:library, confidence:measured]
timestamp: 2026-11-02T00:00:00Z
---

**Void Rancho** (`../VoidRancho`, founded 2026-10-30) stores, indexes, queries
and pages runes and mantles at any size. It is Void Hormiga's `src/store`,
extracted and generalized, and Hormiga is its first client. It sits one layer
above Void Core, as we do, and it never becomes a second door: changes still
enter through Core's dispatcher, and Rancho brings its rows into step with
what Core then says.

That makes it a sibling with exactly the same rule as ours (*the model lives
in Core*), and the seam between us is small. It has three parts.

# 1. One Core: `maiz::RanchoLink`

Rancho's `WorkingSet` does not own a Core. It takes a `rancho::CoreLink&`:
`dispatch`, `export_state`, `replace_state` and `revision`. A Void Maiz host
already owns a `maiz::Core` (glyphs, log sink, effect handler, undo depth),
and must keep using that one, because a second Core would be a second copy of
the model.

`voidmaiz/rancho.hpp` is the adapter. It is header-only and has no target, so
Void Maiz never links Rancho and the add order in a superproject does not
matter. It is ours because both of its sides are:

| CoreLink | maiz::Core | why it matters |
|---|---|---|
| `dispatch` | `dispatch` | the envelope, field for field |
| `export_state` | `*state_text()` | the export Rancho's full sync takes is the one the next projection reads |
| `replace_state` | `replace_state` | glyphs, sinks, effect handler and undo settings survive a working-set reload |
| `revision` | `revision` | does not move on reads, and Rancho's sync asks only reads (`cat`, `links`, bare `config`) |

The last row is the one that would have cost real time. At 5×10^5 runes, one
export is 1.6 s (log, 2026-10-25). If Rancho's sync reads had moved the
revision, every edit would have paid a second export for nothing.
`tests/rancho_link_smoke.cpp` pins it: after a targeted sync the revision is
unchanged and `state_text()` is the same object.

# 2. One Leiden, and it is Rancho's

Rancho clusters with the graph analysis that was written here on 2026-10-25
for Void Hormiga's store (CSR, components, PageRank, Leiden, its hierarchy,
NMI). Its 0.1.0 compiled our `src/graph/graph.cpp` into its own archive.
Hormiga links both us and Rancho, so the same symbols reached one link line
from two archives. **Measured 2026-10-31:** it links and runs, because the
linker keeps whichever copy it meets first. That holds only while both copies
come from the same file with compatible flags. It is an ODR violation that
happens to be harmless, not a design. The first fix was a target of its own,
`voidmaiz_graph`, behind a guard, so every build has one copy.

**Then the author ruled (Q43, 2026-11-02): graph analysis lives in Void Rancho.**
No Void Maiz library code calls it. Its callers are Rancho, Hormiga's store
(which is moving to Rancho) and our tests. Analysing a graph is not drawing one,
the same reason Allomone and networking left us.

The code moves to Rancho as **`voidrancho_graph`**: a leaf target with no
SQLite and no Core, so a view-only host can cluster without taking a database.
It has its own header, `rancho/graph.hpp` (namespace `rancho::graph`), and is
defined by Rancho's guarded `cmake/voidrancho_graph.cmake`, which a project
can include without adding Rancho.

Until that lands, `cmake/voidmaiz_graph.cmake` is a **bridge**, the way
`voidmaiz_lan` was for Void Hammer:

| the build has | `voidmaiz_graph` is | `voidmaiz/graph.hpp` is |
|---|---|---|
| Rancho's target, or Rancho's fragment beside us | INTERFACE over `voidrancho_graph`, `VOIDMAIZ_GRAPH_IS_RANCHO=1` | `namespace maiz { namespace graph = ::rancho::graph; }` |
| no Rancho (our public CI, a host that never cloned it) | our copy, `src/graph/graph.cpp`, **frozen** | the declarations |

So `maiz::graph::leiden` keeps compiling either way, and the switch happens by
itself when Rancho's fragment appears. **Measured 2026-11-02 against a
simulated move** (Rancho's tree copied to a scratch folder and changed as we
proposed):
- Rancho's 8 tests pass;
- `graph_smoke` and `rancho_link_smoke` run on Rancho's target;
- a Hormiga-shaped superproject holds one `libvoidrancho_graph.a`, with
  `graph::leiden` defined in it alone, in both add orders.

The frozen copy is deleted once Rancho is published and our CI clones it.
Until then it is what keeps the public build whole. `graph_smoke` links
`voidmaiz_graph` and nothing else, so "depends on nothing" is checked by the
linker on both paths.

# 3. One libvoidcore

On desktop, Void Maiz linked Core's prebuilt shared library as a bare file
path. A later sibling with an `if(NOT TARGET voidcore)` guard (Void Jelly,
Void Rancho) could not see it, so it built a **second** `libvoidcore` from
source, with the same file name, and the process loaded whichever the loader
met first. Void Squid had fixed this for itself by wrapping the file before
adding us. **We now do the same for every host**: the prebuilt becomes
`voidcore`, `SHARED IMPORTED GLOBAL`. Measured in both add orders: one Core,
built or found once.

`VOIDCORE_ROOT` was the other collision. We and Squid mean Core's `core/`
directory, while Rancho means the repository root. It is one cache variable,
so in a superproject whoever comes first wins and the other's check fails.
**We accept both spellings now**, as a directory-local variable, and we skip
the check when a parent already supplied `voidcore`.

**On a phone the same rule needed a rename (2026-11-02).** There are no
prebuilts for Android or iOS, so Void Maiz compiles Core from source into a
static library, as it always has. It was called `voidcore_static`, so Rancho's
`if(NOT TARGET voidcore)` could not see it and built Core's own **shared**
library beside it: two Cores in one app. It is now `voidcore`, with
`voidcore_static` kept as an alias (no sibling spelled it). Measured with the
Android NDK, Maiz added first: one static `libvoidcore.a` inside the app's
`.so`.

Rancho added first, or alone, still gets Core's shared library, because Core's
CMake knows no other kind. That is Core's to fix
(`../VoidCore/MESSAGE_FOR_VOIDCORE_maiz-a-static-core-on-phones-2026-11-02.md`,
which asks for a static Core on Android and iOS). It would also let us delete
our copy of Core's file list.

# 4. Rancho on a phone

The author wants Rancho ready for iOS and Android (2026-11-02). **Measured:**
Rancho as it is today compiles and links for Android arm64 (NDK r28, API 26):
the library and all 8 of its tests. They were not run, because there is no
device here. iOS is reasoned, not compiled; nobody in the family has a Mac, and
Rancho is not on GitHub for a CI to reach. What we sent Rancho
(`MESSAGE_FOR_VOIDRANCHO_maiz-graph-analysis-is-yours-and-phones-2026-11-02.md`):

- **One Core:** until Core builds static on phones, refuse to build its shared
  library there (see §3).
- **SQLite's temp files** (`temp_store=FILE`):
  - On Android no default directory is writable.
  - **Inside a `maiz::App` this is handled**: our Android shell sets `TMPDIR`
    to `data_dir()/tmp` before the app starts.
  - iOS sets `TMPDIR` itself.
  - So only a host without our shell is exposed.
- **Where files go:** the store in `Device::data_dir()` (private, backed up);
  what can be rebuilt in `cache_dir()`. Rancho says which of its files are which.
- **Backgrounding:**
  - On `AppEvent::Background` a host gets a few seconds on iOS, and on Android
    possibly the end of the process.
  - WAL with `synchronous=NORMAL` keeps every commit, so the store is safe.
  - Rancho names the one call that makes a working set durable, and how long
    it takes.
- **Memory:** `Budget::for_machine` is fine for phones. `AppEvent::LowMemory`
  wants something to call (a `release_memory()`; optional).
- **Proof:** a CI of Rancho's own with an iOS compile and an Android compile,
  once it is published.


# What a host does

```cpp
#include "voidmaiz/rancho.hpp"      // brings rancho/core.hpp
#include "rancho/working_set.hpp"

maiz::Core core;                     // the host's own, as always
rancho::Store store("org.store");
maiz::RanchoLink link(core);
rancho::WorkingSet ws(store, link);
ws.load(rancho::plan(store, budget).sel);
ws.dispatch("tag maria +status:called"); // still the one door
ws.sync();
```

CMake: `add_subdirectory` both siblings, in either order on desktop (on a
phone, Void Maiz first until Core builds static there), with either spelling
of `VOIDCORE_ROOT` (or none), and link `voidmaiz` and `voidrancho`. Nothing
else. A host that clusters includes `rancho/graph.hpp` once the move lands;
`voidmaiz/graph.hpp` keeps working through the forward. Neither sibling's tests reach the host's `ctest` unless the host asks
(`MAIZ_BUILD_TESTS`, `RANCHO_BUILD_TESTS`).

# What we asked Rancho, and what they did

Asked 2026-10-31
(`../VoidRancho/MESSAGE_FOR_VOIDRANCHO_maiz-one-core-one-leiden-and-a-link-2026-10-31.md`).
**All three were taken on 2026-11-02**
(`MESSAGE_FOR_VOIDMAIZ_rancho-all-three-taken-2026-11-02.md`), and they checked
all four combinations of add order and root spelling:

1. Rancho links `voidmaiz_graph` PUBLIC. If a parent already defined it, they
   use it without reading `VOIDMAIZ_ROOT`, so a host whose Maiz is not at
   `../VoidMaiz` still gets one Leiden. The `graph.cpp` fallback remains for
   a Maiz older than the fragment.
2. Rancho accepts either spelling of `VOIDCORE_ROOT`, and skips its check when
   a parent already has `voidcore`.
3. `RANCHO_BUILD_TESTS` is **on only when Rancho is the top-level project.**
   We made `MAIZ_WITH_RANCHO` the same: on by default when we are top-level and
   Rancho is cloned beside us, and never as a subdirectory, because the host
   adds Rancho itself and two `add_subdirectory` calls on one repository
   collide. So `rancho_link_smoke` is now in our default suite. A parent that
   added Rancho before us gets it too. Our shadowing shim for their old
   `VOIDCORE_ROOT` reading is gone.

They also reported that `maiz_rancho_link_smoke.exe` exited 127 before `main`
on a shell with Git's `mingw64/bin` ahead of `ucrt64/bin`. That was ours. The
toolchain runtime and the Core DLL were staged beside `maiz_smoke` only, so a
test built on its own, or one in another `bin/`, had neither.
`maiz_stage_runtime(<target>)` now stages both beside any test binary, and it
takes the Core from **whichever** `voidcore` target the build has: our
imported prebuilt, a parent's, or Rancho's source build. Before this, when
Rancho was added first, nothing was staged at all. Reproduced (exit 127 with
the runtime removed) and fixed (exit 0) on 2026-11-02.

# Asked 2026-11-02: graph analysis, and phones

Sent `MESSAGE_FOR_VOIDRANCHO_maiz-graph-analysis-is-yours-and-phones-2026-11-02.md`:
1. Take graph analysis under the names above.
2. Refuse to build a shared Core on a phone.
3. Say where SQLite's temp files go outside our shell.
4. Say which files go in `data_dir` and which in `cache_dir`, and give the
   durable-now call and its cost.
5. Optionally, `phys_footprint` and a `release_memory()`.
6. A CI with iOS and Android compiles, once published.

Nothing of ours waits on 2–6.

# Still open

- **Rancho's move** (ask 1). Our bridge switches by itself when
  `cmake/voidrancho_graph.cmake` appears. After that, delete our frozen copy once
  Rancho is public and our CI clones it.
- **A static Core on phones** (Void Core). Until then a phone host adds Void
  Maiz before Rancho.
- **Two read lists**, now the same set (Rancho added `related` and `bindings`,
  2026-11-02). We and Rancho each keep a list of the verbs that do not write
  (ours in `embed.cpp`, theirs in `core.cpp`), because Void Core does not say
  which verbs mutate (asked). Both retire when Core answers.
- The matrix view's PageRank and components overlap ours. With the move both are
  Rancho's, and Rancho decides whether to merge them (directed semiring products
  against an undirected modularity graph).
