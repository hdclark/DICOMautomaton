// Sketch_Extrude.h - Extrusion helpers for planar sketch surface meshes.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "YgorMath.h"

class Sketch;

namespace sketch_extrude {

// Options controlling how a sketch is extruded into a 3D surface mesh.
struct extrusion_options_t {
    // Positive values extrude away from the sketch plane along the named direction.
    // Negative values are also legitimate and allow the user to place either cap on the
    // opposite side of the sketch plane, provided the combined span remains positive.
    double into_frame_length = 10.0;
    double out_of_frame_length = 10.0;
    double into_frame_angle_degrees = 0.0;
    double out_of_frame_angle_degrees = 0.0;
    std::size_t curve_segments = 48U;
    double max_discretization_error = 0.1;
};

// Extrude the sketch into an fv_surface_mesh. The Sketch's build_extruded_surface_mesh
// member delegates to this function.
bool build_extruded_surface_mesh(const Sketch &sketch,
                                 const extrusion_options_t &options,
                                 fv_surface_mesh<double, uint64_t> &mesh,
                                 std::vector<fv_surface_mesh<double, uint64_t>> *cap_meshes = nullptr,
                                 std::string *error_message = nullptr);

} // namespace sketch_extrude
