/*
 * voidmaiz/annotate.hpp — the merge, as Void Maiz spells it.
 *
 * THE IMPLEMENTATION LIVES IN VOID ALLOMONE (`../VoidAllomone`, 2026-08-29).
 * This header re-exports it into `maiz::` so that existing hosts — Void Maiz's
 * own projection layer and Void Hormiga's conflicts pane — compile unchanged.
 *
 * Nothing about the algebra changed in the move. `merge()` is the same
 * function, the seven laws are the same laws, and ⊤ is still a first-class
 * answer rather than an error.
 */
#pragma once

#include "allomone/annotate.hpp"

namespace maiz {

using allomone::Lattice;
using allomone::JoinFn;
using allomone::Annotation;
using allomone::ConstraintMap;
using allomone::ConflictPolicy;
using allomone::PropertyLattice;
using allomone::Resolution;
using allomone::MergedCell;
using allomone::MergeOptions;
using allomone::Merged;
using allomone::CellVerdict;
using allomone::CellChange;

using allomone::merge;
using allomone::explain_cell;
using allomone::diff;
using allomone::SourceInfluence;
using allomone::influence;
using allomone::influence_concentration;
using allomone::lattice_name;
using allomone::lattice_from_name;
using allomone::compile_resolution;
using allomone::resolution_glyph;

} // namespace maiz
