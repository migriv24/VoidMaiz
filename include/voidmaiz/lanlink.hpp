/* voidmaiz/lanlink.hpp: MOVED to Void Hammer on 2026-10-17 (voidhammer/lanlink.hpp).

   Networking (reaching devices, the sync engine, the radios) is Void Hammer's
   now; Void Maiz keeps what networking looks like (presence, marks, panels).
   This header forwards so every host that included it compiles unchanged, and
   the names are still maiz:: on purpose: the move changed where the code
   lives, not what anything is called (renaming is Void Hammer's Q10, for
   later). New code should include voidhammer/lanlink.hpp. */
#pragma once
#include "voidhammer/lanlink.hpp"
