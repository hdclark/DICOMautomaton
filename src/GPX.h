//GPX.h -- GPX-specific helpers.

#pragma once

#include <string>
#include <vector>

#include "YgorMath.h"

namespace dcma {
namespace gpx {

struct named_boundary_contour {
    std::string name;
    contour_of_points<double> contour;
};

struct split_open_contour_segment {
    std::string location_name;
    contour_of_points<double> contour;
};

std::vector<named_boundary_contour>
generate_predefined_boundary_contours_lower_mainland_mtb();

std::vector<named_boundary_contour>
parse_boundary_contours(const std::string &boundary_spec);

std::vector<split_open_contour_segment>
split_open_contour_on_boundary_crossings(const contour_of_points<double> &track,
                                         const std::vector<named_boundary_contour> &boundaries,
                                         double debounce_distance_metres);

} // namespace gpx
} // namespace dcma
