# Vendored dependencies

Third-party pieces are vendored whole with their licenses, LiteGraph-style
(ground rule 5: no package managers, no CDNs, no frameworks).

| piece | files | license | origin |
|---|---|---|---|
| cJSON | `cJSON.c`, `cJSON.h` | MIT (header comment in both files) | copied 2026-07-10 from `VoidCore/core/vendor/` (itself vendored from https://github.com/DaveGamble/cJSON) — kept byte-identical to the core's copy so both stacks parse JSON the same way |
| Dear ImGui | `imgui/` (core + `backends/`: glfw+opengl3 pair, android) | MIT (`imgui/LICENSE.txt`) | **v1.92.1-docking** (the docking branch), https://github.com/ocornut/imgui — swapped from stock v1.92.1 on 2026-07-20 to gain **DockSpace** (the author's Q11 ruling: movable/re-dockable panels for Void Hormiga's four workflows). The vendored master was confirmed unpatched before the swap (diff-clean vs stock v1.92.1, whitespace/eol aside), so the swap was wholesale; CRLF→LF normalized to match the tree. **DockSpace only** — the library enables `DockingEnable`, never multi-viewport (which cannot work on the single-surface NDK path). demo compiled in for dev convenience; `imgui_impl_android.*` for the APK substrate |
| GLFW | `glfw/` (source tree; docs/examples/tests/deps stripped) | Zlib (`glfw/LICENSE.md`) | 3.4, https://github.com/glfw/glfw — 2026-07-10; built via `add_subdirectory` with examples/tests/docs/install off |

Notes:
- cJSON is compiled into the `voidmaiz` static library with
  `CJSON_HIDE_SYMBOLS`, mirroring the core. A host that vendors its own cJSON
  may still collide at static-link time; if that ever bites a client, the fix
  is renaming our copy behind a prefix, not un-vendoring.
- ImGui's `imconfig.h` is kept stock for now; project-wide config overrides
  would go there (it is the vendored file we're *allowed* to edit, by design).
