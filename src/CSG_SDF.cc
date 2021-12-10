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
#include "String_Parsing.h"
#include "Regex_Selectors.h"

namespace csg {
namespace sdf {

aa_bbox::aa_bbox(){
    const auto inf = std::numeric_limits<double>::infinity();
    this->min = vec3<double>( inf, inf, inf );
    this->max = vec3<double>( -inf, -inf, -inf );
};

void aa_bbox::digest(const vec3<double>& r){
    this->min.x = std::min( this->min.x, r.x );
    this->min.y = std::min( this->min.y, r.y );
    this->min.z = std::min( this->min.z, r.z );

    this->max.x = std::max( this->max.x, r.x );
    this->max.y = std::max( this->max.y, r.y );
    this->max.z = std::max( this->max.z, r.z );
};


// -------------------------------- 3D Shapes -------------------------------------
namespace shape {


// Sphere centred at (0,0,0).
sphere::sphere(double r) : radius(r) {};

double sphere::evaluate_sdf(const vec3<double>& pos) const {
    const auto sdf = pos.length() - this->radius;
    return sdf;
}

aa_bbox sphere::evaluate_aa_bbox() const {
    aa_bbox bb;
    bb.min = vec3<double>( -this->radius, -this->radius, -this->radius );
    bb.max = vec3<double>(  this->radius,  this->radius,  this->radius );
    return bb;
}

// Axis-aligned box centred at (0,0,0).
aa_box::aa_box(const vec3<double>& r) : radii(r) {};

double aa_box::evaluate_sdf(const vec3<double>& pos) const {
    const auto positive_octant_pos = vec3<double>(std::abs(pos.x), std::abs(pos.y), std::abs(pos.z));
    const auto positive_octant_rad = vec3<double>(std::abs(radii.x), std::abs(radii.y), std::abs(radii.z));
    const auto dL = positive_octant_pos - positive_octant_rad;
    const auto sdf = vec3<double>(std::max(dL.x, 0.0),
                                  std::max(dL.y, 0.0),
                                  std::max(dL.z, 0.0)).length()
                   + std::min( std::max({dL.x, dL.y, dL.z}), 0.0 );
    return sdf;
}

aa_bbox aa_box::evaluate_aa_bbox() const {
    aa_bbox bb;
    bb.min = this->radii * -1.0;
    bb.max = this->radii;
    return bb;
}

} // namespace shape

// -------------------------------- Operations ------------------------------------
 
namespace op {

// Translate.
translate::translate(const vec3<double>& offset) : dR(offset) {};

double translate::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("translate: this operation requires a single child node");
    }
    const auto sdf = this->children[0]->evaluate_sdf(pos - this->dR);
    return sdf;
}

aa_bbox translate::evaluate_aa_bbox() const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("translate: this operation requires a single child node");
    }
    auto bb = this->children[0]->evaluate_aa_bbox();
    bb.min += this->dR;
    bb.max += this->dR;
    return bb;
}

// Rotate.
rotate::rotate(const vec3<double>& axis, double theta) : rot(affine_rotate(vec3<double>(0,0,0), axis, -theta)) {};

double rotate::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("rotate: this operation requires a single child node");
    }
    auto rot_pos = pos;
    this->rot.apply_to(rot_pos);
    const auto sdf = this->children[0]->evaluate_sdf(rot_pos);
    return sdf;
}

aa_bbox rotate::evaluate_aa_bbox() const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("translate: this operation requires a single child node");
    }
    auto bb = this->children[0]->evaluate_aa_bbox();

    // There is probably a clever way to rotate an axis-aligned bounding box.
    // For now, I'll simply transform all eight corners of the box and then re-assess.
    auto c1 = bb.min;
    auto c8 = bb.max;
    auto c2 = vec3<double>( c1.x, c1.y, c8.z );
    auto c3 = vec3<double>( c1.x, c8.y, c1.z );
    auto c4 = vec3<double>( c8.x, c1.y, c1.z );
    auto c5 = vec3<double>( c1.x, c8.y, c8.z );
    auto c6 = vec3<double>( c8.x, c8.y, c1.z );
    auto c7 = vec3<double>( c8.x, c1.y, c8.z );

    this->rot.apply_to(c1);
    this->rot.apply_to(c2);
    this->rot.apply_to(c3);
    this->rot.apply_to(c4);
    this->rot.apply_to(c5);
    this->rot.apply_to(c6);
    this->rot.apply_to(c7);
    this->rot.apply_to(c8);
    bb.min.x = std::min({ c1.x, c2.x, c3.x, c4.x, c5.x, c6.x, c7.x, c8.x });
    bb.min.y = std::min({ c1.y, c2.y, c3.y, c4.y, c5.y, c6.y, c7.y, c8.y });
    bb.min.z = std::min({ c1.z, c2.z, c3.z, c4.z, c5.z, c6.z, c7.z, c8.z });
    bb.max.x = std::max({ c1.x, c2.x, c3.x, c4.x, c5.x, c6.x, c7.x, c8.x });
    bb.max.y = std::max({ c1.y, c2.y, c3.y, c4.y, c5.y, c6.y, c7.y, c8.y });
    bb.max.z = std::max({ c1.z, c2.z, c3.z, c4.z, c5.z, c6.z, c7.z, c8.z });
    return bb;
}

// Boolean 'AND' or 'add' or 'union' or 'join.'
join::join(){};

double join::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.empty()){
        throw std::runtime_error("join: no children present");
    }

    // Union = take the minimum of all children SDF.
    double min_sdf = std::numeric_limits<double>::quiet_NaN();
    for(const auto& c_it : this->children){
        const auto sdf = c_it->evaluate_sdf(pos);
        if(!std::isfinite(min_sdf) || (sdf < min_sdf)) min_sdf = sdf;
    }

    return min_sdf;
}

static
inline
aa_bbox join_aa_bbox_impl(const std::vector<std::shared_ptr<node>>& nodes){
    if(nodes.empty()){
        throw std::logic_error("join_aa_bbox_impl: no nodes present");
    }
    aa_bbox bb;
    for(const auto& c_it : nodes){
        const auto c_bb = c_it->evaluate_aa_bbox();
        bb.digest( c_bb.min );
        bb.digest( c_bb.max );
    }
    return bb;
}

aa_bbox join::evaluate_aa_bbox() const {
    if(this->children.empty()){
        throw std::runtime_error("join: no children present");
    }
    return join_aa_bbox_impl(this->children);
}

// Boolean 'difference' or 'subtract.'
subtract::subtract(){};

double subtract::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 2UL){
        throw std::runtime_error("subtract: incorrect number of children present, subtraction requires exactly two");
    }

    // Difference -- can be APPROXIMATED by taking the maximum of children SDF, but negating one of them.
    const auto cA_sdf = this->children[0]->evaluate_sdf(pos);
    const auto cB_sdf = this->children[1]->evaluate_sdf(pos);
    const auto sdf = std::max( cA_sdf, -cB_sdf );
    return sdf;
}

static
inline
aa_bbox subtract_aa_bbox_impl(const std::vector<std::shared_ptr<node>>& nodes){
    if(nodes.size() != 2UL){
        throw std::logic_error("subtract_aa_bbox_impl: incorrect number of children present");
    }

    // Without evaluating the Boolean itself, we can't tell how the bounding box changes.
    // The easiest approximation is to pass on the positive object's bounding box, since it represents an upper bound.
    const auto bb = nodes[0]->evaluate_aa_bbox();
    return bb;
}

aa_bbox subtract::evaluate_aa_bbox() const {
    if(this->children.size() != 2UL){
        throw std::runtime_error("subtract: incorrect number of children present, subtraction requires exactly two");
    }

    // Without evaluating the Boolean itself, we can't tell how the bounding box changes.
    // The easiest approximation is to pass on the positive object's bounding box, since it represents an upper bound.
    return subtract_aa_bbox_impl(this->children);
}

// Boolean 'OR' or 'intersect.'
intersect::intersect(){};

double intersect::evaluate_sdf(const vec3<double> &pos) const {
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

aa_bbox intersect::evaluate_aa_bbox() const {
    if(this->children.size() < 2UL){
        throw std::runtime_error("intersect: insufficient children present, cannot compute intersect");
    }

    // Without evaluating the Boolean itself, we can't tell how the bounding box changes.
    // The easiest approximation is to use the join/union box, since it represents an upper bound.
    return join_aa_bbox_impl(this->children);
}

// Chamfer-Booleans.
chamfer_join::chamfer_join(double t) : thickness(t) {};

double chamfer_join::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.empty()){
        throw std::runtime_error("chamfer_join: no children present, cannot compute chamfer_join");
    }

    std::vector<double> sdfs;
    sdfs.reserve(this->children.size());
    for(const auto& c_it : this->children){
        const auto sdf = c_it->evaluate_sdf(pos);
        sdfs.emplace_back( sdf );
    }
    double min_sdf = std::numeric_limits<double>::infinity();
    const auto end = std::end(sdfs);
    for(auto c1_it = std::begin(sdfs); c1_it != end; ++c1_it){
        for(auto c2_it = std::next(c1_it); c2_it != end; ++c2_it){
            const auto l_min = std::min({ *c1_it, *c2_it, (*c1_it + *c2_it - this->thickness)*std::sqrt(0.5) });
            if(l_min < min_sdf) min_sdf = l_min;
        }
    }
    return min_sdf;
}

aa_bbox chamfer_join::evaluate_aa_bbox() const {
    if(this->children.empty()){
        throw std::runtime_error("chamfer_join: no children present, cannot compute chamfer_join");
    }
    return join_aa_bbox_impl(this->children);
}


chamfer_subtract::chamfer_subtract(double t) : thickness(t) {};

double chamfer_subtract::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 2UL){
        throw std::runtime_error("chamfer_subtract: incorrect number of children present, chamfer_subtraction requires exactly two");
    }

    // Difference -- can be APPROXIMATED by taking the maximum of children SDF, but negating one of them.
    const auto cA_sdf = this->children[0]->evaluate_sdf(pos);
    const auto cB_sdf = this->children[1]->evaluate_sdf(pos);
    const auto sdf = std::max({ cA_sdf, -cB_sdf, (cA_sdf - cB_sdf + this->thickness)*std::sqrt(0.5) });
    return sdf;
}

aa_bbox chamfer_subtract::evaluate_aa_bbox() const {
    if(this->children.size() != 2UL){
        throw std::runtime_error("chamfer_subtract: incorrect number of children present, chamfer_subtraction requires exactly two");
    }

    // Without evaluating the Boolean itself, we can't tell how the bounding box changes.
    // The easiest approximation is to pass on the positive object's bounding box, since it represents an upper bound.
    return subtract_aa_bbox_impl(this->children);
}


chamfer_intersect::chamfer_intersect(double t) : thickness(t) {};

double chamfer_intersect::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.empty()){
        throw std::runtime_error("chamfer_intersect: no children present, cannot compute chamfer_intersect");
    }

    std::vector<double> sdfs;
    sdfs.reserve(this->children.size());
    for(const auto& c_it : this->children){
        const auto sdf = c_it->evaluate_sdf(pos);
        sdfs.emplace_back( sdf );
    }
    double max_sdf = -std::numeric_limits<double>::infinity();
    const auto end = std::end(sdfs);
    for(auto c1_it = std::begin(sdfs); c1_it != end; ++c1_it){
        for(auto c2_it = std::next(c1_it); c2_it != end; ++c2_it){
            const auto l_max = std::max({ *c1_it, *c2_it, (*c1_it + *c2_it + this->thickness)*std::sqrt(0.5) });
            if(max_sdf < l_max) max_sdf = l_max;
        }
    }
    return max_sdf;
}

aa_bbox chamfer_intersect::evaluate_aa_bbox() const {
    if(this->children.empty()){
        throw std::runtime_error("chamfer_intersect: no children present, cannot compute chamfer_intersect");
    }

    // Without evaluating the Boolean itself, we can't tell how the bounding box changes.
    // The easiest approximation is to use the join/union box, since it represents an upper bound.
    return join_aa_bbox_impl(this->children);
}


// Dilation and erosion.
dilate::dilate(double dist) : offset(dist) {};

double dilate::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("dilate: this operation requires a single child node");
    }
    return this->children[0]->evaluate_sdf(pos) + this->offset;
}

aa_bbox dilate::evaluate_aa_bbox() const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("dilate: this operation requires a single child node");
    }
    auto bb = this->children[0]->evaluate_aa_bbox();
    bb.min.x -= this->offset;
    bb.min.y -= this->offset;
    bb.min.z -= this->offset;
    bb.max.x += this->offset;
    bb.max.y += this->offset;
    bb.max.z += this->offset;
    return bb;
}


erode::erode(double dist) : offset(dist) {};

double erode::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("erode: this operation requires a single child node");
    }
    return this->children[0]->evaluate_sdf(pos) - this->offset;
}

aa_bbox erode::evaluate_aa_bbox() const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("erode: this operation requires a single child node");
    }
    auto bb = this->children[0]->evaluate_aa_bbox();
    bb.min.x -= this->offset;
    bb.min.y -= this->offset;
    bb.min.z -= this->offset;
    bb.max.x += this->offset;
    bb.max.y += this->offset;
    bb.max.z += this->offset;

    // Protect against negative volumes / collapse.
    //
    // Note: This will overestimate the bounding box if it collapses, but a zero-volume box is
    //       an edge case that might not be anticipated...
    if(bb.max.x < bb.min.x) std::swap(bb.min.x, bb.max.x);
    if(bb.max.y < bb.min.y) std::swap(bb.min.y, bb.max.y);
    if(bb.max.z < bb.min.z) std::swap(bb.min.z, bb.max.z);
    return bb;
}

} // namespace op

// Convert parsed function nodes to an 'SDF' object that can be evaluated.
std::shared_ptr<node> build_node(const parsed_function& pf){

/*    
struct function_parameter {
    std::string raw;
    std::optional<double> number;
    bool is_fractional = false;
    bool is_percentage = false;
};

struct parsed_function {
    std::string name;
    std::vector<function_parameter> parameters;

    std::vector<parsed_function> children;
};

struct node {
    std::vector<std::shared_ptr<node>> children;

    virtual double evaluate_sdf(const vec3<double>&) const = 0;
    virtual ~node(){};
};
*/

    // Convert children first.
    std::vector<std::shared_ptr<node>> children;
    for(const auto &pfc : pf.children){
        children.emplace_back( std::move(build_node(pfc)) );
    }

    const auto N_p = static_cast<long int>(pf.parameters.size());

    // Shapes.
    const auto r_sphere            = Compile_Regex("^sphere$");
    const auto r_aa_box            = Compile_Regex("^aa[-_]?box$");

    // Operations.
    const auto r_translate         = Compile_Regex("^translate$");
    const auto r_rotate            = Compile_Regex("^rotate$");
    const auto r_join              = Compile_Regex("^join$");
    const auto r_subtract          = Compile_Regex("^subtract$");
    const auto r_intersect         = Compile_Regex("^intersect$");
    const auto r_chamfer_join      = Compile_Regex("^chamfer[-_]?join$");
    const auto r_chamfer_subtract  = Compile_Regex("^chamfer[-_]?subtract$");
    const auto r_chamfer_intersect = Compile_Regex("^chamfer[-_]?intersect$");
    const auto r_dilate            = Compile_Regex("^dilate$");
    const auto r_erode             = Compile_Regex("^erode$");


    // Simplify common parameter extractions.
    std::optional<double> s1;
    if( (1 <= N_p) && (pf.parameters[0].number) ){
        s1 = pf.parameters[0].number;
    }
    std::optional<double> s4;
    if( (4 <= N_p) && (pf.parameters[3].number) ){
        s1 = pf.parameters[3].number;
    }
    std::optional<vec3<double>> v123;
    if( (3 <= N_p) && (pf.parameters[0].number)
                   && (pf.parameters[1].number)
                   && (pf.parameters[2].number) ){
        v123 = vec3<double>(pf.parameters[0].number.value(),
                            pf.parameters[1].number.value(),
                            pf.parameters[2].number.value());
    }

    std::shared_ptr<csg::sdf::node> out;

    // Shapes.
    if(false){
    }else if(std::regex_match(pf.name, r_sphere)){
        if( !s1 || (N_p != 1) ){
            throw std::invalid_argument("'sphere' requires a radius parameter");
        }
        out = std::make_shared<csg::sdf::shape::sphere>( s1.value() );

    }else if(std::regex_match(pf.name, r_aa_box)){
        if( !v123 || (N_p != 3) ){
            throw std::invalid_argument("'aa_box' requires an extent vec3 parameter");
        }
FUNCINFO("Passing v123 = " << v123.value() << " to aa_box constructor");
        out = std::make_shared<csg::sdf::shape::aa_box>( v123.value() );

    // Operations.
    }else if(std::regex_match(pf.name, r_translate)){
        if( !v123 || (N_p != 3) ){
            throw std::invalid_argument("'translate' requires an extent vec3 parameter");
        }
        out = std::make_shared<csg::sdf::op::translate>( v123.value() );

    }else if(std::regex_match(pf.name, r_rotate)){
        if( !v123 || !s4 || (N_p != 4) ){
            throw std::invalid_argument("'rotate' requires a rotation axis vec3 and an angle parameter");
        }
        out = std::make_shared<csg::sdf::op::rotate>( v123.value(), s4.value() );


    }else if(std::regex_match(pf.name, r_join)){
        if( N_p != 0 ){
            throw std::invalid_argument("'join' requires no parameters");
        }
        out = std::make_shared<csg::sdf::op::join>();

    }else if(std::regex_match(pf.name, r_subtract)){
        if( N_p != 0 ){
            throw std::invalid_argument("'subtract' requires no parameters");
        }
        out = std::make_shared<csg::sdf::op::subtract>();

    }else if(std::regex_match(pf.name, r_intersect)){
        if( N_p != 0 ){
            throw std::invalid_argument("'intersect' requires no parameters");
        }
        out = std::make_shared<csg::sdf::op::intersect>();


    }else if(std::regex_match(pf.name, r_chamfer_join)){
        if( !s1 || (N_p != 1) ){
            throw std::invalid_argument("'chamfer_join' requires a chamfer distance parameter");
        }
        out = std::make_shared<csg::sdf::op::chamfer_join>( s1.value() );

    }else if(std::regex_match(pf.name, r_chamfer_subtract)){
        if( !s1 || (N_p != 1) ){
            throw std::invalid_argument("'chamfer_subtract' requires a chamfer distance parameter");
        }
        out = std::make_shared<csg::sdf::op::chamfer_subtract>( s1.value() );

    }else if(std::regex_match(pf.name, r_chamfer_intersect)){
        if( !s1 || (N_p != 1) ){
            throw std::invalid_argument("'chamfer_intersect' requires a chamfer distance parameter");
        }
        out = std::make_shared<csg::sdf::op::chamfer_intersect>( s1.value() );


    }else if(std::regex_match(pf.name, r_dilate)){
        if( !s1 || (N_p != 1) ){
            throw std::invalid_argument("'dilate' requires a scalar distance parameter");
        }
        out = std::make_shared<csg::sdf::op::dilate>( s1.value() );

    }else if(std::regex_match(pf.name, r_erode)){
        if( !s1 || (N_p != 1) ){
            throw std::invalid_argument("'erode' requires a scalar distance parameter");
        }
        out = std::make_shared<csg::sdf::op::erode>( s1.value() );

    }else{
        throw std::invalid_argument("Unrecognized CSG-SDF node name '"_s + pf.name + "'");
    }

    if(!out){
        throw std::logic_error("Outward node is invalid");
    }
    std::swap(out->children, children);
    return out;
}

} // namespace csg
} // namespace sdf

