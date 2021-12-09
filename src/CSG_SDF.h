//CSG_SDF.h.

#pragma once

#include <string>
#include <map>
#include <set>
#include <optional>
#include <list>
#include <initializer_list>
#include <functional>
#include <regex>
#include <memory>

#include "YgorString.h"
#include "YgorMath.h"
#include "YgorTime.h"

#include "Structs.h"
#include "String_Parsing.h"
#include "Regex_Selectors.h"

namespace csg {
namespace sdf {

// Simple axis-aligned bounding box.
struct aa_bbox {
    vec3<double> min;
    vec3<double> max;
};

// Abstract base expression tree node.
struct node {
    std::vector<std::shared_ptr<node>> children;

    virtual double evaluate_sdf(const vec3<double>&) const = 0;
    virtual aa_bbox evaluate_aa_bbox() const = 0;
    virtual ~node(){};
};

// -------------------------------- 3D Shapes -------------------------------------

namespace shape {

// Sphere centred at (0,0,0).
struct sphere : public node {
    double radius;

    sphere(double);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

// Axis-aligned box centred at (0,0,0).
struct aa_box : public node {
    vec3<double> radii; // i.e., half-extent or box-radius.

    aa_box(const vec3<double>& dR);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

} // namespace shape

// -------------------------------- Operations ------------------------------------
 
namespace op {


// Translate.
struct translate : public node {
    vec3<double> dR;

    translate(const vec3<double>& dR);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};


// Rotate.
struct rotate : public node {
    affine_transform<double> rot;

    rotate(const vec3<double>& axis, double angle_rad);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};


// Boolean 'AND' or 'add' or 'union' or 'join.'
struct join : public node {
    join();
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

// Boolean 'difference' or 'subtract.'
struct subtract : public node {
    subtract();
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

// Boolean 'OR' or 'intersect.'
struct intersect : public node {
    intersect();
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};


// Chamfer-Booleans, which produce a 45-degree edge where surfaces meet.
struct chamfer_join : public node {
    double thickness;

    chamfer_join(double thickness);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

struct chamfer_subtract : public node {
    double thickness;

    chamfer_subtract(double thickness);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

struct chamfer_intersect : public node {
    double thickness;

    chamfer_intersect(double thickness);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};


// Dilation and erosion.
//
// Note: Dilation and erosion are complementary and using a negative offset distance
//       for one will recover the other. Two separate classes are provided to simplify
//       reasoning.
//
// Note: Beware that large offset distances are likely to fail since in some cases
//       the SDF has to be approximated (e.g., interior of complicated shapes).
struct dilate : public node {
    double offset;

    dilate(double);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};

struct erode : public node {
    double offset;

    erode(double);
    double evaluate_sdf(const vec3<double>& pos) const override;
    aa_bbox evaluate_aa_bbox() const override;
};


} // namespace op

// Convert parsed function nodes to an 'SDF' object that can be evaluated.
std::shared_ptr<node> build_node(const parsed_function& pf);

} // namespace sdf
} // namespace csg

