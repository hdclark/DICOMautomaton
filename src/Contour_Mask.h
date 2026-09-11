// Contour_Mask.h - A part of DICOMautomaton 2026. Written by hal clark.

#pragma once

#include <string>
#include <vector>

#include "YgorMath.h"

#include "Structs.h"

namespace dcma::mask_contours {

    struct mask_polygon {
        std::vector<vec2<double>> vertices;
    };
    
    struct mask_region {
        std::string name;
        std::vector<mask_polygon> polygons;
    };
    
    struct atomic_segment {
        vec3<double> a;
        vec3<double> b;
        bool inside = false;
    
        double length() const;
    };
    
    std::vector<mask_region> parse_regions(const std::string &spec);
    
    bool point_in_region_xy(const vec3<double> &p, const mask_region &r);
    
    std::vector<atomic_segment> atomize_path(const std::vector<vec3<double>> &points,
                                             bool closed,
                                             const mask_region &region);
    
    void debounce_atoms(std::vector<atomic_segment> &atoms, double debounce_distance);
    
    std::vector<contour_of_points<double>> slice_contour(const contour_of_points<double> &source,
                                                         const mask_region &region,
                                                         double debounce_distance);
    
} // namespace dcma::mask_contours
