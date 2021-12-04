//CSG_SDF.cc - A part of DICOMautomaton 2021. Written by hal clark.
//
// This file contains routines for constructive solid geometry (CSG) using signed distance functions (SDF),
// which can be used to programmatically build solids in 3D.

#include <string>
#include <list>
#include <numeric>
#include <initializer_list>
#include <functional>
#include <regex>
#include <optional>
#include <utility>
#include <random>
#include <chrono>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorTime.h"

#include "CSG_SDF.h"

namespace csg {
namespace sdf {

// -------------------------------- 3D Shapes -------------------------------------
namespace shape {


// Sphere centred at (0,0,0).
sphere::sphere(double r) : radius(r) {};

double sphere::evaluate_sdf(const vec3<double>& pos) const {
//    FUNCINFO("df_node_sphere::evaluate_sdf()");
    const auto sdf = pos.length() - this->radius;
    return sdf;
}


// Axis-aligned box centred at (0,0,0).
aa_box::aa_box(const vec3<double>& r) : radii(r) {};

double aa_box::evaluate_sdf(const vec3<double>& pos) const {
//    FUNCINFO("df_node_aa_box::evaluate_sdf()");
    const auto positive_octant_pos = vec3<double>(std::abs(pos.x), std::abs(pos.y), std::abs(pos.z));
    const auto positive_octant_rad = vec3<double>(std::abs(radii.x), std::abs(radii.y), std::abs(radii.z));
    const auto dL = positive_octant_pos - positive_octant_rad;
    const auto sdf = vec3<double>(std::max(dL.x, 0.0),
                                  std::max(dL.y, 0.0),
                                  std::max(dL.z, 0.0)).length()
                   + std::min( std::max({dL.x, dL.y, dL.z}), 0.0 );
    return sdf;
}

} // namespace shape

// -------------------------------- Operations ------------------------------------
 
namespace op {

// Translate.
translate::translate(const vec3<double>& offset) : dR(offset) {};

double translate::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_translate::evaluate_sdf()");

    if(this->children.size() != 1UL){
        throw std::runtime_error("translate: this operation requires a single child node");
    }
    const auto sdf = this->children[0]->evaluate_sdf(pos - this->dR);
    return sdf;
}

// Rotate.
rotate::rotate(const vec3<double>& axis, double theta) : rot(affine_rotate(vec3<double>(0,0,0), axis, -theta)) {};

double rotate::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_rotate::evaluate_sdf()");

    if(this->children.size() != 1UL){
        throw std::runtime_error("rotate: this operation requires a single child node");
    }
    auto rot_pos = pos;
    this->rot.apply_to(rot_pos);
    const auto sdf = this->children[0]->evaluate_sdf(rot_pos);
    return sdf;
}

// Boolean 'AND' or 'add' or 'union' or 'join.'
join::join(){};

double join::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_join::evaluate_sdf()");
    if(this->children.empty()){
        throw std::runtime_error("join: no children present, cannot compute join");
    }

    // Union = take the minimum of all children SDF.
    double min_sdf = std::numeric_limits<double>::quiet_NaN();
    for(const auto& c_it : this->children){
        const auto sdf = c_it->evaluate_sdf(pos);
        if(!std::isfinite(min_sdf) || (sdf < min_sdf)) min_sdf = sdf;
    }

    return min_sdf;
}

// Boolean 'difference' or 'subtract.'
subtract::subtract(){};

double subtract::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_subtract::evaluate_sdf()");
    if(this->children.size() != 2UL){
        throw std::runtime_error("subtract: incorrect number of children present, subtraction requires exactly two");
    }

    // Difference -- can be APPROXIMATED by taking the maximum of children SDF, but negating one of them.
    const auto cA_sdf = this->children[0]->evaluate_sdf(pos);
    const auto cB_sdf = this->children[1]->evaluate_sdf(pos);
    const auto sdf = std::max( cA_sdf, -cB_sdf );
    return sdf;
}

// Boolean 'OR' or 'intersect.'
intersect::intersect(){};

double intersect::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_intersect::evaluate_sdf()");
    if(this->children.size() < 2UL){
        throw std::runtime_error("intersect: insufficient children present, cannot compute intersect");
    }

    // Intersection -- can be APPROXIMATED by taking the maximum of all children SDF.
    double max_sdf = std::numeric_limits<double>::quiet_NaN();
    for(const auto& c_it : this->children){
        const auto sdf = c_it->evaluate_sdf(pos);
        if(!std::isfinite(max_sdf) || (max_sdf < sdf)) max_sdf = sdf;
    }

    return max_sdf;
}

// Dilation and erosion.
dilate::dilate(double dist) : offset(dist) {};

double dilate::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_dilate::evaluate_sdf()");

    if(this->children.size() != 1UL){
        throw std::runtime_error("dilate: this operation requires a single child node");
    }
    return this->children[0]->evaluate_sdf(pos) + this->offset;
}

erode::erode(double dist) : offset(dist) {};

double erode::evaluate_sdf(const vec3<double> &pos) const {
//    FUNCINFO("df_node_erode::evaluate_sdf()");

    if(this->children.size() != 1UL){
        throw std::runtime_error("erode: this operation requires a single child node");
    }
    return this->children[0]->evaluate_sdf(pos) - this->offset;
}

} // namespace op
} // namespace csg
} // namespace sdf

