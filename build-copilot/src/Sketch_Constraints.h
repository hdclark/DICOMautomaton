// Sketch_Constraints.h - Constraint solver helpers for planar sketches.

#pragma once

#include "Sketch.h"

namespace sketch_constraints {

// Solve the active constraints in a sketch, updating vertex positions in-place.
// Returns the number of unresolved constraints and populates the report.
std::size_t solve_constraints(Sketch &sketch,
                              const Sketch::solve_options_t &options,
                              Sketch::solve_report_t &report);

} // namespace sketch_constraints
