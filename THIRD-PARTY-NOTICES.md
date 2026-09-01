# Third-party notices

Void Maiz is MIT licensed (see [LICENSE](LICENSE)). It **vendors** its
dependencies rather than fetching them — no package manager, no CDN — so every
third-party component ships in this repository with its own license beside the
code it covers. This file is an index; those license texts are authoritative.
The full provenance table, including why each version was chosen, is in
[`vendor/README.md`](vendor/README.md).

| component | version | where | license |
|---|---|---|---|
| Dear ImGui (docking branch) | 1.92.1-docking | `vendor/imgui/` | MIT — `vendor/imgui/LICENSE.txt` |
| GLFW | 3.4 | `vendor/glfw/` | Zlib — `vendor/glfw/LICENSE.md` |
| cJSON | 1.7.18 | `vendor/cJSON.c`, `vendor/cJSON.h` | MIT — header comment in both files |

None of these conditions travel further than ordinary permissive attribution:
MIT and Zlib both require only that the notice go with the source. No font,
icon set or asset ships in this repository, so there is no SIL OFL or
CC-licensed material to carry.

## Siblings are not vendored

Void Core and Void Allomone are **sibling repositories**, built from their own
trees beside this one — not copies inside `vendor/`. Their licenses are their
own. See [`void.json`](void.json) and the Building section of the
[README](README.md).
