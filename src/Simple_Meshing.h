//Simple_Meshing.h - A part of DICOMautomaton 2020. Written by hal clark.

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

std::vector< std::array<size_t, 3> > Tile_Contours(
        std::reference_wrapper<contour_of_points<double>> A,
        std::reference_wrapper<contour_of_points<double>> B );

// Low-level routine that joins the vertices of two contours.
// Returns a list of faces where the vertex indices refer to A followed by B.
std::vector< std::array<size_t, 3> >
Estimate_Contour_Correspondence(
        std::reference_wrapper<contour_of_points<double>> A,
        std::reference_wrapper<contour_of_points<double>> B );

contour_of_points<double>
Minimally_Amalgamate_Contours(
        const vec3<double> &ortho_unit,
        const vec3<double> &pseudo_vert_offset,
        std::list<std::reference_wrapper<contour_of_points<double>>> B );

/*
Polyhedron
Estimate_Surface_Mesh(
        std::list<std::reference_wrapper<contour_collection<double>>> cc_ROIs,
        Parameters p );
*/

