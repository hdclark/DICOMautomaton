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
    bb.digest( vec3<double>( -this->radius, -this->radius, -this->radius ) );
    bb.digest( vec3<double>(  this->radius,  this->radius,  this->radius ) );
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
    bb.digest( this->radii * -1.0 );
    bb.digest( this->radii );
    return bb;
}

// Connected line segments with rounded edges.
poly_chain::poly_chain(double r, const std::vector<vec3<double>> &v) : radius(r), vertices(v) {};
poly_chain::poly_chain(double r, const std::list<vec3<double>> &v) : radius(r), vertices(std::begin(v), std::end(v)) {};

double poly_chain::evaluate_sdf(const vec3<double>& pos) const {
    if(this->vertices.size() < 2UL){
        throw std::runtime_error("poly_chain: this operation requires at least two vertices");
    }

    // Note: this is a Boolean union of each line segment followed by a dilation.
    double min_sdf = std::numeric_limits<double>::infinity();
    const auto end = std::cend(this->vertices);
    auto A_it = std::cbegin(this->vertices);
    auto B_it = std::next(A_it);
    for( ; B_it != end; ++A_it, ++B_it){
        const auto dPA = pos - *A_it;
        const auto dBA = *B_it - *A_it;
        const auto t = std::clamp( dPA.Dot(dBA) / dBA.Dot(dBA), 0.0, 1.0 );
        const auto sdf = (dPA - dBA * t).length() - this->radius;
        min_sdf = std::min( min_sdf, sdf );
    }

    if(!std::isfinite(min_sdf)){
        throw std::runtime_error("poly_chain: computed non-finite SDF");
    }
    return min_sdf;
}

aa_bbox poly_chain::evaluate_aa_bbox() const {
    aa_bbox bb;
    const vec3<double> rad3(this->radius, this->radius, this->radius);
    for(const auto& v : this->vertices){
        bb.digest(v + rad3);
        bb.digest(v - rad3);
    }
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
    return this->children[0]->evaluate_sdf(pos) - this->offset;
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
    return this->children[0]->evaluate_sdf(pos) + this->offset;
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


// Extrude.
extrude::extrude(double dist, const plane<double> &p) : distance(dist), cut_plane(p) {};

double extrude::evaluate_sdf(const vec3<double> &pos) const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("extrude: this operation requires a single child node");
    }

    // Extrusion of the shape cut by the plane along the normal of the plane.
    const auto proj_pos = this->cut_plane.Project_Onto_Plane_Orthogonally(pos);
    const auto pos_plane_sdist = this->cut_plane.Get_Signed_Distance_To_Point(pos);
    const auto c_sdf = this->children[0]->evaluate_sdf( proj_pos );
    const auto dz = std::abs(pos_plane_sdist) - this->distance;
    const auto sdf = std::min(0.0, std::max(dz, c_sdf))
                   + std::hypot( std::max(0.0, dz), std::max(0.0, c_sdf) );
    return sdf;
}

aa_bbox extrude::evaluate_aa_bbox() const {
    if(this->children.size() != 1UL){
        throw std::runtime_error("extrude: this operation requires a single child node");
    }
    auto bb = this->children[0]->evaluate_aa_bbox();

    // Wasteful upper limit which includes irrelevant original geometry.
    bb.digest( bb.min - this->cut_plane.N_0 * this->distance );
    bb.digest( bb.max - this->cut_plane.N_0 * this->distance );
    bb.digest( bb.min + this->cut_plane.N_0 * this->distance );
    bb.digest( bb.max + this->cut_plane.N_0 * this->distance );
    return bb;

/*
    // More selective and elegant approach which unfortunately doesn't quite work...
    //
    // TODO: FIXME
    const auto proj_min = this->cut_plane.Project_Onto_Plane_Orthogonally(bb.min);
    const auto proj_max = this->cut_plane.Project_Onto_Plane_Orthogonally(bb.max);
    aa_bbox new_bb;
    new_bb.digest( proj_min );
    new_bb.digest( proj_max );
    new_bb.digest( proj_min + this->cut_plane.N_0 * this->distance );
    new_bb.digest( proj_max + this->cut_plane.N_0 * this->distance );
    new_bb.digest( proj_min - this->cut_plane.N_0 * this->distance );
    new_bb.digest( proj_max - this->cut_plane.N_0 * this->distance );
    return new_bb;
*/

}


} // namespace op

// Convert text to a 3D representation using SDFs.
std::shared_ptr<node> text(const std::string& text,
                           double radius,
                           double text_height,
                           double text_width,
                           double char_spacing,
                           double line_spacing ){
    std::shared_ptr<node> root = std::make_shared<csg::sdf::op::join>();

    const auto clean = [](char c){
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return c;
    };

    const auto start  = vec3<double>(0.0, 0.0, 0.0);
    const auto d_w    = vec3<double>(text_width, 0.0, 0.0);
    const auto d_h    = vec3<double>(0.0, text_height, 0.0);
    const auto d_line = vec3<double>(0.0, -line_spacing, 0.0);
    const auto d_char = vec3<double>(char_spacing, 0.0, 0.0);

    auto pos = start;
    for(auto c : text){
        c = clean(c);

        // Approximately emulate 7-segment display.
        //
        //   A ---- B
        //   |      |  
        //   |      |  
        //   C ---- D  
        //   |      |  
        //   |      |  
        //   E ---- F  
        //
        const auto A = pos + d_h;
        const auto B = pos + d_w + d_h;
        const auto C = pos + d_h * 0.5;
        const auto D = pos + d_w + d_h * 0.5;
        const auto E = pos;
        const auto F = pos + d_w;

        const auto AB = (A+B)*0.5;
        const auto AC = (A+C)*0.5;
        const auto BD = (B+D)*0.5;
        const auto CD = (C+D)*0.5;
        const auto CE = (C+E)*0.5;
        const auto DF = (D+F)*0.5;
        const auto EF = (E+F)*0.5;
        const auto ABCD = (A+B+C+D)*0.25;
        const auto CDEF = (C+D+E+F)*0.25;
        const auto eps = 0.0005;

        std::vector<std::vector<vec3<double>>> vv;
        const auto add = [&](const std::vector<vec3<double>>& l){
            vv.emplace_back(l);
        };

        if(false){
        }else if(c == 'A'){ add({ E, C, AB, D, F }); add({ C, D });
        }else if(c == 'B'){ add({ E, A, AB, BD, CD, DF, F, E }); add({ C, CD });
        }else if(c == 'C'){ add({ B, A, E, F }); 
        }else if(c == 'D'){ add({ E, A, AB, BD, DF, EF, E }); 
        }else if(c == 'E'){ add({ B, A, E, F }); add({ C, D }); 
        }else if(c == 'F'){ add({ E, A, B }); add({ C, D });
        }else if(c == 'G'){ add({ B, A, E, F, D, CD }); 
        }else if(c == 'H'){ add({ A, E }); add({ C, D }); add({ B, F }); 
        }else if(c == 'I'){ add({ A, B }); add({ E, F }); add({ AB, EF });
        }else if(c == 'J'){ add({ AB, B, F, E, CE });
        }else if(c == 'K'){ add({ A, E }); add({ C, CD }); add({ B, CD, F });
        }else if(c == 'L'){ add({ A, E, F });
        }else if(c == 'M'){ add({ E, A, CD, B, F });
        }else if(c == 'N'){ add({ E, A, F, B });
        }else if(c == 'O'){ add({ AC, AB, BD, DF, EF, CE, AC });
        }else if(c == 'P'){ add({ E, A, B, D, C });
        }else if(c == 'Q'){ add({ A, E, F, B, A }); add({ F, (C*0.35 + F*0.65) });
        }else if(c == 'R'){ add({ E, A, AB, BD, CD, F }); add({ C, CD });
        }else if(c == 'S'){ add({ E, F, D, C, A, B });
        }else if(c == 'T'){ add({ A, B }); add({ AB, EF });
        }else if(c == 'U'){ add({ A, E, F, B });
        }else if(c == 'V'){ add({ A, EF, B });
        }else if(c == 'W'){ add({ A, E, CD, F, B });
        }else if(c == 'X'){ add({ A, F }); add({ E, B });
        }else if(c == 'Y'){ add({ A, CD, B }); add({ CD, EF });
        }else if(c == 'Z'){ add({ A, B, E, F });
        }else if(c == '1'){ add({ AC, AB, EF }); add({ E, F });
        }else if(c == '2'){ add({ A, B, D, C, E, F });
        }else if(c == '3'){ add({ A, B, F, E }); add({ C, D });
        }else if(c == '4'){ add({ A, C, D }); add({ B, F });
        }else if(c == '5'){ add({ B, A, C, D, F, E });
        }else if(c == '6'){ add({ B, A, C, D, F, E, C });
        }else if(c == '7'){ add({ A, B, EF });
        }else if(c == '8'){ add({ A, B, D, F, E, C, A }); add({ C, D });
        }else if(c == '9'){ add({ D, B, A, C, D, F });
        }else if(c == '0'){ add({ A, E, F, B, A });
        }else if(c == ' '){ // do nothing, just advance the cursor.
        }else if(c == '-'){ add({ C, D });
        }else if(c == '_'){ add({ E, F });
        }else if(c == '\\'){ add({ A, F });
        }else if(c == '/'){ add({ E, B });
        }else if(c == '#'){ add({ A*0.85 + B*0.15, E*0.85 + F*0.15 }); add({ A*0.15 + B*0.85, E*0.15 + F*0.85 });
                            add({ A*0.85 + E*0.15, B*0.85 + F*0.15 }); add({ A*0.15 + E*0.85, B*0.15 + F*0.85 });
        }else if(c == '('){ add({ B, ABCD, CDEF, F });
        }else if(c == ')'){ add({ A, ABCD, CDEF, E });
        }else if(c == '['){ add({ B, AB, EF, F });
        }else if(c == ']'){ add({ A, AB, EF, E });
        }else if(c == '|'){ add({ AB, EF });
        }else if(c == '\''){ add({ (A*0.85 + B*0.15), (AC*0.85 + BD*0.15) }); add({ (A*0.15 + B*0.85), (AC*0.15 + BD*0.85) });
        }else if(c == '"'){ add({ });
        }else if(c == '^'){ add({ AC, AB, BD });
        }else if(c == '+'){ add({ C, D }); add({ CD + d_h*0.25, CD - d_h*0.25 });
        }else if(c == '='){ add({ AC, BD }); add({ CE, DF });
        }else if(c == ','){ add({ (C*0.25 + EF*0.75), EF });
        }else if(c == '.'){ add({ EF - (F - EF) * eps, EF + (F - EF) * eps});
        }else if(c == ':'){ add({ ABCD - (AC - ABCD) * eps, ABCD + (AC - ABCD) * eps }); add({ CDEF - (CE - CDEF) * eps, CDEF + (CE - CDEF) * eps });
        }else if(c == '?'){ add({ AC, A, B, BD, CD, (CD+EF)*0.5 }); add({ EF - (F - EF) * eps, EF + (F - EF) * eps });
        }else if(c == '!'){ add({ AB, (CD+EF)*0.5 }); add({ EF - (F - EF) * eps, EF + (F - EF) * eps });
        }else if(c == '\t'){ pos += d_char * 4.0;
        }else if(c == '\r'){ pos -= d_char;
        }else if(c == '\0'){ // do nothing, just advance the cursor.
        }else if(c == '\n'){
            pos += d_line;
            pos.x = start.x;
            pos -= d_char;
        }else{
            // Draw an interrobang if symbol is not available.
            add({ AC, A, B, BD, CD, (CD+EF)*0.5 });
            add({ AB, (CD+EF)*0.5 }); add({ EF - (F - EF) * eps, EF + (F - EF) * eps });
        }
        pos += d_char;

        for(const auto& verts : vv){
            std::shared_ptr<node> n = std::make_shared<csg::sdf::shape::poly_chain>( radius, verts );
            root->children.emplace_back( n );
        }
    }

    return root;
}

// Convert parsed function nodes to an 'SDF' object that can be evaluated.
std::shared_ptr<node> build_node(const parsed_function& pf){

    // Convert children first.
    std::vector<std::shared_ptr<node>> children;
    for(const auto &pfc : pf.children){
        children.emplace_back( std::move(build_node(pfc)) );
    }

    const auto N_p = static_cast<long int>(pf.parameters.size());

    // Shapes.
    const auto r_sphere            = Compile_Regex("^sphere$");
    const auto r_aa_box            = Compile_Regex("^aa[-_]?box$");
    const auto r_poly_chain        = Compile_Regex("^poly[-_]?chain$");
    const auto r_text              = Compile_Regex("^text$");

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
    const auto r_extrude           = Compile_Regex("^extrude$");

    const vec3<double> u_z(0.0, 0.0, 1.0);
    const vec3<double> zero3(0.0, 0.0, 0.0);

    // Simplify common parameter extractions.
    std::optional<double> s0;
    if( (1 <= N_p) && (pf.parameters[0].number) ){
        s0 = pf.parameters[0].number;
    }
    std::optional<double> s3;
    if( (4 <= N_p) && (pf.parameters[3].number) ){
        s3 = pf.parameters[3].number;
    }
    std::optional<vec3<double>> v012;
    if( (3 <= N_p) && (pf.parameters[0].number)
                   && (pf.parameters[1].number)
                   && (pf.parameters[2].number) ){
        v012 = vec3<double>(pf.parameters[0].number.value(),
                            pf.parameters[1].number.value(),
                            pf.parameters[2].number.value());
    }
    std::optional<vec3<double>> v123;
    if( (4 <= N_p) && (pf.parameters[1].number)
                   && (pf.parameters[2].number)
                   && (pf.parameters[3].number) ){
        v123 = vec3<double>(pf.parameters[1].number.value(),
                            pf.parameters[2].number.value(),
                            pf.parameters[3].number.value());
    }
    std::optional<vec3<double>> v456;
    if( (7 <= N_p) && (pf.parameters[4].number)
                   && (pf.parameters[5].number)
                   && (pf.parameters[6].number) ){
        v456 = vec3<double>(pf.parameters[4].number.value(),
                            pf.parameters[5].number.value(),
                            pf.parameters[6].number.value());
    }

    std::shared_ptr<csg::sdf::node> out;

    // Shapes.
    if(false){
    }else if(std::regex_match(pf.name, r_sphere)){
        if( !s0 || (N_p != 1) ){
            throw std::invalid_argument("'sphere' requires a radius parameter");
        }
        out = std::make_shared<csg::sdf::shape::sphere>( s0.value() );

    }else if(std::regex_match(pf.name, r_aa_box)){
        if( !v012 || (N_p != 3) ){
            throw std::invalid_argument("'aa_box' requires an extent vec3 parameter");
        }
        out = std::make_shared<csg::sdf::shape::aa_box>( v012.value() );

    }else if(std::regex_match(pf.name, r_poly_chain)){
        if( !s0 || (N_p <= 7) ){
            throw std::invalid_argument("'poly_chain' requires a radius vec3 parameter and two or more vertices");
        }
        std::vector<vec3<double>> verts;
        for(auto i = 1U; i < pf.parameters.size(); i += 3U){
            if( (N_p < (i + 3U))
            ||  !(pf.parameters[i+0].number)
            ||  !(pf.parameters[i+1].number)
            ||  !(pf.parameters[i+2].number) ){
                throw std::invalid_argument("'poly_chain' requires a list of three-vector vertices");
            }
            verts.emplace_back( pf.parameters[i+0].number.value(),
                                pf.parameters[i+1].number.value(),
                                pf.parameters[i+2].number.value() );
        }
        out = std::make_shared<csg::sdf::shape::poly_chain>( s0.value(), verts );

    }else if(std::regex_match(pf.name, r_text)){
        if( !s0 || (N_p < 2) ){
            throw std::invalid_argument("'text' requires a radius parameter and a text parameter");
        }
        out = csg::sdf::text(pf.parameters.at(1).raw, s0.value());
        std::swap(out->children, children); // Make children accessible.

    // Operations.
    }else if(std::regex_match(pf.name, r_translate)){
        if( !v012 || (N_p != 3) ){
            throw std::invalid_argument("'translate' requires an extent vec3 parameter");
        }
        out = std::make_shared<csg::sdf::op::translate>( v012.value() );

    }else if(std::regex_match(pf.name, r_rotate)){
        if( !v012 || !s3 || (N_p != 4) ){
            throw std::invalid_argument("'rotate' requires a rotation axis vec3 and an angle parameter");
        }
        out = std::make_shared<csg::sdf::op::rotate>( v012.value(), s3.value() );


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
        if( !s0 || (N_p != 1) ){
            throw std::invalid_argument("'chamfer_join' requires a chamfer distance parameter");
        }
        out = std::make_shared<csg::sdf::op::chamfer_join>( s0.value() );

    }else if(std::regex_match(pf.name, r_chamfer_subtract)){
        if( !s0 || (N_p != 1) ){
            throw std::invalid_argument("'chamfer_subtract' requires a chamfer distance parameter");
        }
        out = std::make_shared<csg::sdf::op::chamfer_subtract>( s0.value() );

    }else if(std::regex_match(pf.name, r_chamfer_intersect)){
        if( !s0 || (N_p != 1) ){
            throw std::invalid_argument("'chamfer_intersect' requires a chamfer distance parameter");
        }
        out = std::make_shared<csg::sdf::op::chamfer_intersect>( s0.value() );


    }else if(std::regex_match(pf.name, r_dilate)){
        if( !s0 || (N_p != 1) ){
            throw std::invalid_argument("'dilate' requires a scalar distance parameter");
        }
        out = std::make_shared<csg::sdf::op::dilate>( s0.value() );

    }else if(std::regex_match(pf.name, r_erode)){
        if( !s0 || (N_p != 1) ){
            throw std::invalid_argument("'erode' requires a scalar distance parameter");
        }
        out = std::make_shared<csg::sdf::op::erode>( s0.value() );

    }else if(std::regex_match(pf.name, r_extrude)){
        if( !s0 || (N_p < 1) || (7 < N_p) ){
            throw std::invalid_argument("'extrude' requires a scalar distance parameter and optional planar normal vec3 and optional planar anchor vec3");
        }
        const auto normal = v123.value_or( u_z ).unit();
        const auto anchor = v456.value_or( zero3 );
FUNCINFO("Extrusion: using normal " << normal << " and anchor " << anchor );
        const plane<double> pln(normal, anchor);
        out = std::make_shared<csg::sdf::op::extrude>( s0.value(), pln );

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

