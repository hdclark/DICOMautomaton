// SDL_Viewer_Utils.cc - Non-OpenGL utility functions for SDL_Viewer.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//
// This file contains utility functions that do not require OpenGL or graphical context.
//

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"

#include "SDL_Viewer_Utils.h"


std::tuple<int64_t, int64_t, int64_t, int64_t>
get_pixelspace_axis_aligned_bounding_box(const planar_image<float, double> &img,
                                         const std::vector<vec3<double>> &points,
                                         double extra_space){

    const auto corner = img.position(0,0) - img.row_unit * img.pxl_dx * 0.5
                                          - img.col_unit * img.pxl_dy * 0.5;
    const auto axis1 = img.row_unit.unit();
    const auto axis2 = img.col_unit.unit();
    //const auto axis3 = img.row_unit.Cross( img.col_unit ).unit();

    const auto inf = std::numeric_limits<double>::infinity();
    vec3<double> bbox_min(inf, inf, inf);
    vec3<double> bbox_max(-inf, -inf, -inf);
    for(const auto& p : points){
        const auto proj1 = (p - corner).Dot(axis1);
        const auto proj2 = (p - corner).Dot(axis2);
        //const auto proj3 = (p - corner).Dot(axis3);
        if((proj1 - extra_space) < bbox_min.x) bbox_min.x = (proj1 - extra_space);
        if((proj2 - extra_space) < bbox_min.y) bbox_min.y = (proj2 - extra_space);
        //if((proj3 - extra_space) < bbox_min.z) bbox_min.z = (proj3 - extra_space);
        if(bbox_max.x < (proj1 + extra_space)) bbox_max.x = (proj1 + extra_space);
        if(bbox_max.y < (proj2 + extra_space)) bbox_max.y = (proj2 + extra_space);
        //if(bbox_max.z < (proj3 + extra_space)) bbox_max.z = (proj3 + extra_space);
    }

    auto col_min = std::clamp<int64_t>(static_cast<int64_t>(std::floor(bbox_min.x/img.pxl_dx)), 0, img.columns-1);
    auto col_max = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(bbox_max.x/img.pxl_dx)), 0, img.columns-1);
    auto row_min = std::clamp<int64_t>(static_cast<int64_t>(std::floor(bbox_min.y/img.pxl_dy)), 0, img.rows-1);
    auto row_max = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(bbox_max.y/img.pxl_dy)), 0, img.rows-1);
    return std::make_tuple( row_min, row_max, col_min, col_max );
}

