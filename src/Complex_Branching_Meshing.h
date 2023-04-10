//Complex_Branching_Meshing.h - A part of DICOMautomaton 2020. Written by hal clark.

#pragma once

#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <optional>

#include "YgorMath.h"         //Needed for vec3 class.

// returns contour of points of convex hull by following
// https://www.geeksforgeeks.org/convex-hull-using-graham-scan/
contour_of_points<double>
Contour_From_Convex_Hull_2(
    std::vector<contour_of_points<double>> &cops,
    const vec3<double> &normal
);

// assumes there are only two contours in cops
// modifies convex hull by adding midpoints and including more original contour points
// returns closed modified convex hull, two non-convex hull contours (one for each original contour), and list of midpoints
// the non-convex hull contours start and end with closest point on convex hull
std::tuple<contour_of_points<double>,contour_of_points<double>,contour_of_points<double>, std::vector<vec3<double>>>
Modify_Convex_Hull(
    const contour_of_points<double> &convex_hull,
    const std::vector<contour_of_points<double>> cops,
    const vec3<double> &pseudo_vert_offset
);

// creates faces by connecting non convex hull points to the midpoints based on distance
// returns faces and associated ordered points
std::tuple<std::vector<std::array<size_t, 3>>, std::vector<vec3<double>>>
Mesh_Inner_Points_With_Midpoints(
    const std::list<vec3<double>> &left_points,
    const std::list<vec3<double>> &right_points,
    const std::vector<vec3<double>> &midpoints,
    const vec3<double> &pseudo_vert_offset
);

// meshes 2 to 1 (will need to mesh convex hull contour with other contour outside of this routine)
// returns non-convex hull faces, non-convex hull points, and convex hull contour (with midpoints)
std::tuple<std::vector<std::array<size_t,3>>, std::vector<vec3<double>>, contour_of_points<double>>
Mesh_With_Convex_Hull_2(
    const std::list<std::reference_wrapper<contour_of_points<double>>> &A,
    const vec3<double> &ortho_unit_A,
    const vec3<double> &pseudo_vert_offset
);