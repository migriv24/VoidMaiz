# voidmaiz_graph — the graph analysis behind voidmaiz/graph.hpp: a CSR graph,
# components, PageRank, Leiden, its hierarchy, NMI.
#
# GRAPH ANALYSIS IS VOID RANCHO'S (the author, 2026-11-02, Q43). Rancho is the
# one that clusters (Store::cluster, the matrix view); no Void Maiz library code
# calls these functions. The code moves to Rancho as a leaf target,
# `voidrancho_graph` (rancho/graph.hpp, namespace rancho::graph; no SQLite, no
# Core), defined by Rancho's guarded cmake/voidrancho_graph.cmake. This file is
# the bridge, the way voidmaiz_lan was when networking moved to Void Hammer:
#
#  1. Rancho's target already exists (a parent added Rancho first), or Rancho's
#     fragment is beside us: `voidmaiz_graph` is an INTERFACE target over
#     `voidrancho_graph`, and voidmaiz/graph.hpp forwards maiz::graph to
#     rancho::graph (VOIDMAIZ_GRAPH_IS_RANCHO). One copy of the code per build.
#  2. No Rancho (our public CI until Rancho is published, a host that never
#     cloned it): our own copy, src/graph/graph.cpp. It is FROZEN — fixes go to
#     Rancho — and it is deleted once Rancho is public and our CI clones it.
#
# Either way it depends on nothing — not Void Core, not ImGui, not Allomone —
# and tests/graph_smoke.cpp links this target and nothing else, so the claim is
# a build fact. INCLUDE THIS FILE; do not compile graph.cpp yourself:
#
#   include("${VOIDMAIZ_ROOT}/cmake/voidmaiz_graph.cmake")
#   target_link_libraries(yours PRIVATE voidmaiz_graph)
#
# The guard makes the first include define the target and every later one
# reuse it, in any add order (two archives each holding maiz::graph::* was the
# 2026-10-30 bug: the linker kept whichever it met first, silently).
if(NOT TARGET voidmaiz_graph)
  get_filename_component(_maiz_graph_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  if(DEFINED VOIDRANCHO_ROOT)
    set(_maiz_graph_rancho "${VOIDRANCHO_ROOT}")
  else()
    get_filename_component(_maiz_graph_rancho "${_maiz_graph_root}/../VoidRancho" ABSOLUTE)
  endif()
  if(NOT TARGET voidrancho_graph AND EXISTS "${_maiz_graph_rancho}/cmake/voidrancho_graph.cmake")
    include("${_maiz_graph_rancho}/cmake/voidrancho_graph.cmake")
  endif()

  if(TARGET voidrancho_graph)
    add_library(voidmaiz_graph INTERFACE)
    target_link_libraries(voidmaiz_graph INTERFACE voidrancho_graph)
    target_include_directories(voidmaiz_graph INTERFACE "${_maiz_graph_root}/include")
    target_compile_definitions(voidmaiz_graph INTERFACE VOIDMAIZ_GRAPH_IS_RANCHO=1)
    set(MAIZ_GRAPH_SOURCE "Void Rancho (voidrancho_graph)")
  else()
    add_library(voidmaiz_graph STATIC "${_maiz_graph_root}/src/graph/graph.cpp")
    target_include_directories(voidmaiz_graph PUBLIC "${_maiz_graph_root}/include")
    target_compile_features(voidmaiz_graph PUBLIC cxx_std_20)
    set_target_properties(voidmaiz_graph PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(NOT MSVC)
      target_compile_options(voidmaiz_graph PRIVATE -Wall -Wextra)
    endif()
    set(MAIZ_GRAPH_SOURCE "Void Maiz's frozen copy (no ${_maiz_graph_rancho}/cmake/voidrancho_graph.cmake)")
  endif()
  message(STATUS "voidmaiz_graph: ${MAIZ_GRAPH_SOURCE}")
  unset(_maiz_graph_root)
  unset(_maiz_graph_rancho)
endif()
