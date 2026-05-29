// Sketch_Fillets.h - Fillet helpers for planar sketches.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "YgorMath.h"

class Sketch;

namespace sketch_fillets {

// Insert a rounded corner (fillet) at the shared vertex of two line primitives.
// The Sketch's add_fillet member delegates to this function.
bool add_fillet(Sketch &sketch,
                std::size_t line_a_idx,
                std::size_t line_b_idx,
                double radius,
                std::string *error_message = nullptr);

} // namespace sketch_fillets
