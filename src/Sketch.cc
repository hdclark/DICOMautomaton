//Sketch.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "Sketch.h"
#include "Sketch_Constraints.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "YgorLog.h"

namespace {

Sketch::projection_t cubic_bezier_point(const Sketch::projection_t &p0,
                                        const Sketch::projection_t &p1,
                                        const Sketch::projection_t &p2,
                                        const Sketch::projection_t &p3,
                                        double t){
    const double omt = 1.0 - t;
    const double b0 = omt * omt * omt;
    const double b1 = 3.0 * omt * omt * t;
    const double b2 = 3.0 * omt * t * t;
    const double b3 = t * t * t;
    return { (b0 * p0.u) + (b1 * p1.u) + (b2 * p2.u) + (b3 * p3.u),
             (b0 * p0.v) + (b1 * p1.v) + (b2 * p2.v) + (b3 * p3.v) };
}

static void remap_primitive_vertices(Sketch::primitive_t &primitive,
                                     Sketch::vertex_index_t removed_vertex){
    if(auto *vertex_primitive = dynamic_cast<Sketch::vertex_primitive_t*>(&primitive); vertex_primitive != nullptr){
        if(removed_vertex < vertex_primitive->vertex) --vertex_primitive->vertex;
    }else if(auto *line = dynamic_cast<Sketch::line_primitive_t*>(&primitive); line != nullptr){
        for(auto &idx : line->vertices){
            if(removed_vertex < idx) --idx;
        }
    }else if(auto *circle = dynamic_cast<Sketch::circle_primitive_t*>(&primitive); circle != nullptr){
        if(removed_vertex < circle->center) --circle->center;
        if(removed_vertex < circle->radius_point) --circle->radius_point;
    }else if(auto *arc = dynamic_cast<Sketch::arc_primitive_t*>(&primitive); arc != nullptr){
        if(removed_vertex < arc->center) --arc->center;
        if(removed_vertex < arc->start) --arc->start;
        if(removed_vertex < arc->stop) --arc->stop;
    }else if(auto *bezier = dynamic_cast<Sketch::bezier_primitive_t*>(&primitive); bezier != nullptr){
        for(auto &idx : bezier->control_vertices){
            if(removed_vertex < idx) --idx;
        }
    }
}

static void remap_primitive_vertices(Sketch::primitive_t &primitive,
                                     const std::vector<std::optional<Sketch::vertex_index_t>> &vertex_remap){
    const auto remap_vertex = [&vertex_remap](Sketch::vertex_index_t &idx) -> void {
        idx = vertex_remap.at(idx).value();
    };

    if(auto *vertex_primitive = dynamic_cast<Sketch::vertex_primitive_t*>(&primitive); vertex_primitive != nullptr){
        remap_vertex(vertex_primitive->vertex);
    }else if(auto *line = dynamic_cast<Sketch::line_primitive_t*>(&primitive); line != nullptr){
        for(auto &idx : line->vertices) remap_vertex(idx);
    }else if(auto *circle = dynamic_cast<Sketch::circle_primitive_t*>(&primitive); circle != nullptr){
        remap_vertex(circle->center);
        remap_vertex(circle->radius_point);
    }else if(auto *arc = dynamic_cast<Sketch::arc_primitive_t*>(&primitive); arc != nullptr){
        remap_vertex(arc->center);
        remap_vertex(arc->start);
        remap_vertex(arc->stop);
    }else if(auto *bezier = dynamic_cast<Sketch::bezier_primitive_t*>(&primitive); bezier != nullptr){
        for(auto &idx : bezier->control_vertices) remap_vertex(idx);
    }
}

static void remap_constraint_primitives(Sketch::constraint_t &constraint,
                                        const std::vector<std::optional<Sketch::primitive_index_t>> &primitive_remap){
    if(auto *horizontal = dynamic_cast<Sketch::horizontal_constraint_t*>(&constraint); horizontal != nullptr){
        horizontal->line = primitive_remap.at(horizontal->line).value();
    }else if(auto *vertical = dynamic_cast<Sketch::vertical_constraint_t*>(&constraint); vertical != nullptr){
        vertical->line = primitive_remap.at(vertical->line).value();
    }else if(auto *distance = dynamic_cast<Sketch::distance_constraint_t*>(&constraint); distance != nullptr){
        distance->primitive = primitive_remap.at(distance->primitive).value();
    }else if(auto *parallel = dynamic_cast<Sketch::parallel_constraint_t*>(&constraint); parallel != nullptr){
        parallel->line_a = primitive_remap.at(parallel->line_a).value();
        parallel->line_b = primitive_remap.at(parallel->line_b).value();
    }else if(auto *perpendicular = dynamic_cast<Sketch::perpendicular_constraint_t*>(&constraint); perpendicular != nullptr){
        perpendicular->line_a = primitive_remap.at(perpendicular->line_a).value();
        perpendicular->line_b = primitive_remap.at(perpendicular->line_b).value();
    }else if(auto *mirror = dynamic_cast<Sketch::mirror_constraint_t*>(&constraint); mirror != nullptr){
        mirror->line = primitive_remap.at(mirror->line).value();
    }else if(auto *tangent = dynamic_cast<Sketch::tangent_constraint_t*>(&constraint); tangent != nullptr){
        tangent->primitive_a = primitive_remap.at(tangent->primitive_a).value();
        tangent->primitive_b = primitive_remap.at(tangent->primitive_b).value();
    }
}

static void remap_constraint_vertices(Sketch::constraint_t &constraint,
                                      Sketch::vertex_index_t removed_vertex){
    if(auto *pin = dynamic_cast<Sketch::pin_constraint_t*>(&constraint); pin != nullptr){
        if(removed_vertex < pin->vertex) --pin->vertex;
    }else if(auto *mirror = dynamic_cast<Sketch::mirror_constraint_t*>(&constraint); mirror != nullptr){
        if(removed_vertex < mirror->vertex_a) --mirror->vertex_a;
        if(removed_vertex < mirror->vertex_b) --mirror->vertex_b;
    }else if(auto *overlap = dynamic_cast<Sketch::overlap_constraint_t*>(&constraint); overlap != nullptr){
        if(removed_vertex < overlap->vertex_a) --overlap->vertex_a;
        if(removed_vertex < overlap->vertex_b) --overlap->vertex_b;
    }
}

static void remap_constraint_vertices(Sketch::constraint_t &constraint,
                                      const std::vector<std::optional<Sketch::vertex_index_t>> &vertex_remap){
    if(auto *pin = dynamic_cast<Sketch::pin_constraint_t*>(&constraint); pin != nullptr){
        pin->vertex = vertex_remap.at(pin->vertex).value();
    }else if(auto *mirror = dynamic_cast<Sketch::mirror_constraint_t*>(&constraint); mirror != nullptr){
        mirror->vertex_a = vertex_remap.at(mirror->vertex_a).value();
        mirror->vertex_b = vertex_remap.at(mirror->vertex_b).value();
    }else if(auto *overlap = dynamic_cast<Sketch::overlap_constraint_t*>(&constraint); overlap != nullptr){
        overlap->vertex_a = vertex_remap.at(overlap->vertex_a).value();
        overlap->vertex_b = vertex_remap.at(overlap->vertex_b).value();
    }
}

static vec3<double> point_on_circle(const Sketch::plane_frame_t &plane,
                                    const vec3<double> &centre,
                                    double radius,
                                    double angle){
    return centre + plane.row_unit * (std::cos(angle) * radius)
                  + plane.col_unit * (std::sin(angle) * radius);
}

static void store_error(std::string *error_message,
                        const std::string &message){
    if(error_message != nullptr) *error_message = message;
}

static std::string geometry_tag_to_string(Sketch::geometry_tag_t tag){
    switch(tag){
        case Sketch::geometry_tag_t::normal:
            return "normal";
        case Sketch::geometry_tag_t::support:
            return "support";
    }
    return "normal";
}

static bool parse_geometry_tag(const std::string &token,
                               Sketch::geometry_tag_t &tag){
    if(token == "normal"){
        tag = Sketch::geometry_tag_t::normal;
        return true;
    }
    if(token == "support"){
        tag = Sketch::geometry_tag_t::support;
        return true;
    }
    return false;
}

static double squared_distance_between(const Sketch::projection_t &a,
                                       const Sketch::projection_t &b){
    const auto du = a.u - b.u;
    const auto dv = a.v - b.v;
    return (du * du) + (dv * dv);
}

static Sketch::projection_t lerp(const Sketch::projection_t &a,
                                 const Sketch::projection_t &b,
                                 double t){
    return { a.u + (b.u - a.u) * t,
             a.v + (b.v - a.v) * t };
}

static double nearest_cubic_bezier_parameter(const Sketch::projection_t &query,
                                             const Sketch::projection_t &p0,
                                             const Sketch::projection_t &p1,
                                             const Sketch::projection_t &p2,
                                             const Sketch::projection_t &p3){
    constexpr std::size_t coarse_samples = 128U;
    double best_t = 0.0;
    double best_dist_sq = std::numeric_limits<double>::infinity();
    for(std::size_t i = 0U; i <= coarse_samples; ++i){
        const auto t = static_cast<double>(i) / static_cast<double>(coarse_samples);
        const auto sample = cubic_bezier_point(p0, p1, p2, p3, t);
        const auto dist_sq = squared_distance_between(query, sample);
        if(dist_sq < best_dist_sq){
            best_t = t;
            best_dist_sq = dist_sq;
        }
    }
    return best_t;
}

static Sketch::projection_t clamp_to_bounds(const Sketch::projection_t &p,
                                            const Sketch::bounding_box_t &bounds){
    return {
        std::clamp(p.u, bounds.min.u, bounds.max.u),
        std::clamp(p.v, bounds.min.v, bounds.max.v)
    };
}

}

vec3<double>
Sketch::plane_frame_t::normal() const{
    return row_unit.Cross(col_unit).unit();
}

::plane<double>
Sketch::plane_frame_t::to_plane() const{
    return ::plane<double>(normal(), origin);
}

Sketch::plane_frame_t
Sketch::plane_frame_t::from_plane(const ::plane<double> &plane,
                                  const std::optional<vec3<double>> &row_hint){
    plane_frame_t out;
    out.origin = plane.R_0;
    const auto normal = plane.N_0.unit();
    auto try_orthogonal_unit = [&](vec3<double> candidate) -> vec3<double> {
        candidate = candidate - normal * candidate.Dot(normal);
        if(candidate.sq_length() <= std::numeric_limits<double>::epsilon()){
            return {};
        }
        return candidate.unit();
    };

    if(row_hint){
        out.row_unit = try_orthogonal_unit(row_hint.value());
    }
    if(out.row_unit.sq_length() <= std::numeric_limits<double>::epsilon()){
        for(const auto &candidate : { vec3<double>(1.0, 0.0, 0.0),
                                      vec3<double>(0.0, 1.0, 0.0),
                                      vec3<double>(0.0, 0.0, 1.0) }){
            out.row_unit = try_orthogonal_unit(candidate);
            if(out.row_unit.sq_length() > std::numeric_limits<double>::epsilon()){
                break;
            }
        }
    }
    if(out.row_unit.sq_length() <= std::numeric_limits<double>::epsilon()){
        throw std::logic_error("Unable to derive sketch plane basis from plane normal "
                             + std::to_string(normal.x) + ", "
                             + std::to_string(normal.y) + ", "
                             + std::to_string(normal.z)
                             + (row_hint ? " with supplied row hint" : " without a row hint"));
    }

    out.col_unit = normal.Cross(out.row_unit).unit();
    out.row_unit = out.col_unit.Cross(normal).unit();
    return out;
}

bool
Sketch::bounding_box_t::is_valid() const{
    return std::isfinite(min.u) && std::isfinite(min.v)
        && std::isfinite(max.u) && std::isfinite(max.v)
        && (min.u <= max.u) && (min.v <= max.v);
}

bool
Sketch::bounding_box_t::contains(const projection_t &p) const{
    if(!is_valid()) return false;
    return (min.u <= p.u) && (p.u <= max.u)
        && (min.v <= p.v) && (p.v <= max.v);
}

bool
Sketch::bounding_box_t::contains(const bounding_box_t &b) const{
    if(!is_valid() || !b.is_valid()) return false;
    return contains(b.min) && contains(b.max);
}

std::unique_ptr<Sketch::primitive_t>
Sketch::vertex_primitive_t::clone() const{
    return std::make_unique<vertex_primitive_t>(*this);
}

Sketch::primitive_kind_t
Sketch::vertex_primitive_t::kind() const{
    return primitive_kind_t::vertex;
}

std::vector<Sketch::vertex_index_t>
Sketch::vertex_primitive_t::referenced_vertices() const{
    return { vertex };
}

std::unique_ptr<Sketch::primitive_t>
Sketch::line_primitive_t::clone() const{
    return std::make_unique<line_primitive_t>(*this);
}

Sketch::primitive_kind_t
Sketch::line_primitive_t::kind() const{
    return primitive_kind_t::line;
}

std::vector<Sketch::vertex_index_t>
Sketch::line_primitive_t::referenced_vertices() const{
    return { vertices[0], vertices[1] };
}

std::unique_ptr<Sketch::primitive_t>
Sketch::circle_primitive_t::clone() const{
    return std::make_unique<circle_primitive_t>(*this);
}

Sketch::primitive_kind_t
Sketch::circle_primitive_t::kind() const{
    return primitive_kind_t::circle;
}

std::vector<Sketch::vertex_index_t>
Sketch::circle_primitive_t::referenced_vertices() const{
    return { center, radius_point };
}

std::unique_ptr<Sketch::primitive_t>
Sketch::arc_primitive_t::clone() const{
    return std::make_unique<arc_primitive_t>(*this);
}

Sketch::primitive_kind_t
Sketch::arc_primitive_t::kind() const{
    return primitive_kind_t::arc;
}

std::vector<Sketch::vertex_index_t>
Sketch::arc_primitive_t::referenced_vertices() const{
    return { center, start, stop };
}

std::unique_ptr<Sketch::primitive_t>
Sketch::bezier_primitive_t::clone() const{
    return std::make_unique<bezier_primitive_t>(*this);
}

Sketch::primitive_kind_t
Sketch::bezier_primitive_t::kind() const{
    return primitive_kind_t::bezier;
}

std::vector<Sketch::vertex_index_t>
Sketch::bezier_primitive_t::referenced_vertices() const{
    return { control_vertices[0], control_vertices[1], control_vertices[2], control_vertices[3] };
}

std::vector<Sketch::vertex_index_t>
Sketch::constraint_t::referenced_vertices() const{
    return {};
}

std::unique_ptr<Sketch::constraint_t>
Sketch::horizontal_constraint_t::clone() const{
    return std::make_unique<horizontal_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::horizontal_constraint_t::kind() const{
    return constraint_kind_t::horizontal;
}

std::vector<Sketch::primitive_index_t>
Sketch::horizontal_constraint_t::referenced_primitives() const{
    return { line };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::vertical_constraint_t::clone() const{
    return std::make_unique<vertical_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::vertical_constraint_t::kind() const{
    return constraint_kind_t::vertical;
}

std::vector<Sketch::primitive_index_t>
Sketch::vertical_constraint_t::referenced_primitives() const{
    return { line };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::distance_constraint_t::clone() const{
    return std::make_unique<distance_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::distance_constraint_t::kind() const{
    return constraint_kind_t::distance;
}

std::vector<Sketch::primitive_index_t>
Sketch::distance_constraint_t::referenced_primitives() const{
    return { primitive };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::parallel_constraint_t::clone() const{
    return std::make_unique<parallel_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::parallel_constraint_t::kind() const{
    return constraint_kind_t::parallel;
}

std::vector<Sketch::primitive_index_t>
Sketch::parallel_constraint_t::referenced_primitives() const{
    return { line_a, line_b };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::perpendicular_constraint_t::clone() const{
    return std::make_unique<perpendicular_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::perpendicular_constraint_t::kind() const{
    return constraint_kind_t::perpendicular;
}

std::vector<Sketch::primitive_index_t>
Sketch::perpendicular_constraint_t::referenced_primitives() const{
    return { line_a, line_b };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::pin_constraint_t::clone() const{
    return std::make_unique<pin_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::pin_constraint_t::kind() const{
    return constraint_kind_t::pin;
}

std::vector<Sketch::primitive_index_t>
Sketch::pin_constraint_t::referenced_primitives() const{
    return {};
}

std::vector<Sketch::vertex_index_t>
Sketch::pin_constraint_t::referenced_vertices() const{
    return { vertex };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::tangent_constraint_t::clone() const{
    return std::make_unique<tangent_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::tangent_constraint_t::kind() const{
    return constraint_kind_t::tangent;
}

std::vector<Sketch::primitive_index_t>
Sketch::tangent_constraint_t::referenced_primitives() const{
    return { primitive_a, primitive_b };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::mirror_constraint_t::clone() const{
    return std::make_unique<mirror_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::mirror_constraint_t::kind() const{
    return constraint_kind_t::mirror;
}

std::vector<Sketch::primitive_index_t>
Sketch::mirror_constraint_t::referenced_primitives() const{
    return { line };
}

std::vector<Sketch::vertex_index_t>
Sketch::mirror_constraint_t::referenced_vertices() const{
    return { vertex_a, vertex_b };
}

std::unique_ptr<Sketch::constraint_t>
Sketch::overlap_constraint_t::clone() const{
    return std::make_unique<overlap_constraint_t>(*this);
}

Sketch::constraint_kind_t
Sketch::overlap_constraint_t::kind() const{
    return constraint_kind_t::overlap;
}

std::vector<Sketch::primitive_index_t>
Sketch::overlap_constraint_t::referenced_primitives() const{
    return {};
}

std::vector<Sketch::vertex_index_t>
Sketch::overlap_constraint_t::referenced_vertices() const{
    return { vertex_a, vertex_b };
}

Sketch::Sketch() = default;

Sketch::Sketch(const Sketch &in){
    copy_from(in);
}

Sketch&
Sketch::operator=(const Sketch &in){
    if(this != std::addressof(in)){
        copy_from(in);
    }
    return *this;
}

void
Sketch::copy_from(const Sketch &in){
    vertices_ = in.vertices_;
    primitives_.clear();
    constraints_.clear();
    for(const auto &primitive : in.primitives_){
        primitives_.push_back(primitive ? primitive->clone() : nullptr);
    }
    for(const auto &constraint : in.constraints_){
        constraints_.push_back(constraint ? constraint->clone() : nullptr);
    }
    has_plane_ = in.has_plane_;
    plane_ = in.plane_;
    last_solve_report_ = in.last_solve_report_;
}

void
Sketch::clear(){
    vertices_.clear();
    primitives_.clear();
    constraints_.clear();
    has_plane_ = false;
    plane_ = {};
}

bool
Sketch::empty() const{
    return primitives_.empty() && constraints_.empty() && vertices_.empty();
}

bool
Sketch::has_plane() const{
    return has_plane_;
}

void
Sketch::set_plane(const plane_frame_t &frame){
    plane_ = frame;
    plane_.row_unit = plane_.row_unit.unit();
    plane_.col_unit = plane_.col_unit.unit();
    has_plane_ = true;
    refresh_all_derived_geometry();
}

void
Sketch::set_plane(const ::plane<double> &plane,
                  const std::optional<vec3<double>> &row_hint){
    set_plane(plane_frame_t::from_plane(plane, row_hint));
}

const Sketch::plane_frame_t&
Sketch::plane() const{
    if(!has_plane_) throw std::logic_error("Sketch plane has not been initialized");
    return plane_;
}

::plane<double>
Sketch::supporting_plane() const{
    return plane().to_plane();
}

Sketch::projection_t
Sketch::project(const vec3<double> &point) const{
    const auto &f = plane();
    const auto rel = point - f.origin;
    return { f.row_unit.Dot(rel), f.col_unit.Dot(rel) };
}

vec3<double>
Sketch::lift(const projection_t &point) const{
    const auto &f = plane();
    return f.origin + f.row_unit * point.u + f.col_unit * point.v;
}

Sketch::vertex_index_t
Sketch::append_vertex(const vec3<double> &point){
    vertices_.push_back(point);
    return vertices_.size() - 1U;
}

Sketch::primitive_index_t
Sketch::append_primitive(std::unique_ptr<primitive_t> primitive){
    if(!primitive) throw std::invalid_argument("Cannot append empty sketch primitive");
    primitives_.push_back(std::move(primitive));
    refresh_primitive_geometry(primitives_.size() - 1U, {});
    return primitives_.size() - 1U;
}

Sketch::constraint_index_t
Sketch::append_constraint(std::unique_ptr<constraint_t> constraint){
    if(!constraint) throw std::invalid_argument("Cannot append empty sketch constraint");
    constraints_.push_back(std::move(constraint));
    return constraints_.size() - 1U;
}

Sketch::primitive_index_t
Sketch::add_vertex_primitive(vertex_index_t vertex, geometry_tag_t tag){
    if(!vertex_index_valid(vertex)) throw std::out_of_range("Sketch vertex index is out of range");
    auto primitive = std::make_unique<vertex_primitive_t>();
    primitive->tag = tag;
    primitive->vertex = vertex;
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_vertex_primitive(const vec3<double> &point, geometry_tag_t tag){
    return add_vertex_primitive(append_vertex(point), tag);
}

Sketch::primitive_index_t
Sketch::add_line(vertex_index_t start, vertex_index_t stop, geometry_tag_t tag){
    if(!vertex_index_valid(start) || !vertex_index_valid(stop)){
        throw std::out_of_range("Sketch vertex index is out of range");
    }
    auto primitive = std::make_unique<line_primitive_t>();
    primitive->tag = tag;
    primitive->vertices[0] = start;
    primitive->vertices[1] = stop;
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_line(const vec3<double> &start, const vec3<double> &stop, geometry_tag_t tag){
    const auto start_idx = append_vertex(start);
    const auto stop_idx = append_vertex(stop);
    return add_line(start_idx, stop_idx, tag);
}

Sketch::primitive_index_t
Sketch::add_circle(vertex_index_t center, vertex_index_t radius_point, geometry_tag_t tag){
    if(!vertex_index_valid(center) || !vertex_index_valid(radius_point)){
        throw std::out_of_range("Sketch vertex index is out of range");
    }
    auto primitive = std::make_unique<circle_primitive_t>();
    primitive->tag = tag;
    primitive->center = center;
    primitive->radius_point = radius_point;
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_circle(const vec3<double> &center, const vec3<double> &radius_point, geometry_tag_t tag){
    const auto center_idx = append_vertex(center);
    const auto radius_idx = append_vertex(radius_point);
    return add_circle(center_idx, radius_idx, tag);
}

Sketch::primitive_index_t
Sketch::add_arc(vertex_index_t center, vertex_index_t start, vertex_index_t stop, geometry_tag_t tag){
    if(!vertex_index_valid(center) || !vertex_index_valid(start) || !vertex_index_valid(stop)){
        throw std::out_of_range("Sketch vertex index is out of range");
    }
    auto primitive = std::make_unique<arc_primitive_t>();
    primitive->tag = tag;
    primitive->center = center;
    primitive->start = start;
    primitive->stop = stop;
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_arc(const vec3<double> &center, const vec3<double> &start, const vec3<double> &stop, geometry_tag_t tag){
    const auto center_idx = append_vertex(center);
    const auto start_idx = append_vertex(start);
    const auto stop_idx = append_vertex(stop);
    return add_arc(center_idx, start_idx, stop_idx, tag);
}

Sketch::primitive_index_t
Sketch::add_bezier(const std::array<vertex_index_t, 4> &control_vertices, geometry_tag_t tag){
    for(const auto vertex_idx : control_vertices){
        if(!vertex_index_valid(vertex_idx)){
            throw std::out_of_range("Sketch vertex index is out of range");
        }
    }
    auto primitive = std::make_unique<bezier_primitive_t>();
    primitive->tag = tag;
    primitive->control_vertices = control_vertices;
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_bezier(const std::array<vec3<double>, 4> &control_points, geometry_tag_t tag){
    std::array<vertex_index_t, 4> control_vertices = {};
    for(std::size_t i = 0U; i < control_vertices.size(); ++i){
        control_vertices[i] = append_vertex(control_points[i]);
    }
    return add_bezier(control_vertices, tag);
}

std::size_t
Sketch::vertex_count() const{
    return vertices_.size();
}

std::size_t
Sketch::primitive_count() const{
    return primitives_.size();
}

std::size_t
Sketch::constraint_count() const{
    return constraints_.size();
}

vec3<double>&
Sketch::vertex(vertex_index_t idx){
    if(!vertex_index_valid(idx)) throw std::out_of_range("Sketch vertex index is out of range");
    return vertices_.at(idx);
}

const vec3<double>&
Sketch::vertex(vertex_index_t idx) const{
    if(!vertex_index_valid(idx)) throw std::out_of_range("Sketch vertex index is out of range");
    return vertices_.at(idx);
}

Sketch::primitive_t*
Sketch::primitive(primitive_index_t idx){
    return primitive_index_valid(idx) ? primitives_.at(idx).get() : nullptr;
}

const Sketch::primitive_t*
Sketch::primitive(primitive_index_t idx) const{
    return primitive_index_valid(idx) ? primitives_.at(idx).get() : nullptr;
}

Sketch::constraint_t*
Sketch::constraint(constraint_index_t idx){
    return (idx < constraints_.size()) ? constraints_.at(idx).get() : nullptr;
}

const Sketch::constraint_t*
Sketch::constraint(constraint_index_t idx) const{
    return (idx < constraints_.size()) ? constraints_.at(idx).get() : nullptr;
}

double
Sketch::normalize_angle(double x){
    // TODO: replace these while loops with fmod, which has much better error handling.
    const auto pi = std::acos(-1.0);
    while(x < 0.0) x += 2.0 * pi;
    while((2.0 * pi) <= x) x -= 2.0 * pi;
    return x;
}

double
Sketch::squared_distance_to_segment(const projection_t &p,
                                    const projection_t &a,
                                    const projection_t &b){
    const double ab_u = b.u - a.u;
    const double ab_v = b.v - a.v;
    const double denom = (ab_u * ab_u) + (ab_v * ab_v);
    if(denom <= std::numeric_limits<double>::epsilon()){
        const double du = p.u - a.u;
        const double dv = p.v - a.v;
        return (du * du) + (dv * dv);
    }
    const double ap_u = p.u - a.u;
    const double ap_v = p.v - a.v;
    const double t = std::clamp(((ap_u * ab_u) + (ap_v * ab_v)) / denom, 0.0, 1.0);
    const double du = p.u - (a.u + t * ab_u);
    const double dv = p.v - (a.v + t * ab_v);
    return (du * du) + (dv * dv);
}

bool
Sketch::refresh_primitive_geometry(primitive_index_t idx,
                                   const std::set<vertex_index_t> &pinned_vertices){
    if(!primitive_index_valid(idx)) return false;
    bool snapped_vertex = false;
    auto *base = primitives_.at(idx).get();
    if(auto *circle = dynamic_cast<circle_primitive_t*>(base); circle != nullptr){
        if(vertex_index_valid(circle->center) && vertex_index_valid(circle->radius_point)){
            circle->radius = vertex(circle->center).distance(vertex(circle->radius_point));
        }
    }else if(auto *arc = dynamic_cast<arc_primitive_t*>(base); arc != nullptr){
        if(!has_plane_) return false;
        if( vertex_index_valid(arc->center)
        &&  vertex_index_valid(arc->start)
        &&  vertex_index_valid(arc->stop) ){
            const auto centre = vertex(arc->center);
            const auto start_point = vertex(arc->start);
            const auto stop_point = vertex(arc->stop);
            const auto plane_normal = plane().normal();
            const auto on_plane_tolerance = 1.0E-6;
            const auto start_on_plane = (std::abs((start_point - plane().origin).Dot(plane_normal)) <= on_plane_tolerance);
            const auto stop_on_plane = (std::abs((stop_point - plane().origin).Dot(plane_normal)) <= on_plane_tolerance);
            const auto start_p = project(vertex(arc->start));
            const auto stop_p = project(vertex(arc->stop));
            const auto centre_p = project(centre);
            const auto start_du = start_p.u - centre_p.u;
            const auto start_dv = start_p.v - centre_p.v;
            const auto stop_du = stop_p.u - centre_p.u;
            const auto stop_dv = stop_p.v - centre_p.v;
            const auto start_radius = std::hypot(start_du, start_dv);
            const auto stop_radius = std::hypot(stop_du, stop_dv);
            if( (start_radius <= std::numeric_limits<double>::epsilon())
            &&  (stop_radius <= std::numeric_limits<double>::epsilon()) ){
                arc->radius = 0.0;
                arc->start_angle = 0.0;
                arc->stop_angle = 0.0;
                return false;
            }

            arc->radius = (start_radius <= std::numeric_limits<double>::epsilon()) ? stop_radius : start_radius;
            arc->start_angle = normalize_angle((start_radius <= std::numeric_limits<double>::epsilon())
                                               ? 0.0
                                               : std::atan2(start_dv, start_du));
            arc->stop_angle = normalize_angle((stop_radius <= std::numeric_limits<double>::epsilon())
                                              ? arc->start_angle
                                              : std::atan2(stop_dv, stop_du));

            if(arc->radius > std::numeric_limits<double>::epsilon()){
                if(start_on_plane && (pinned_vertices.count(arc->start) == 0U)){
                    const auto snapped_start = point_on_circle(plane(), centre, arc->radius, arc->start_angle);
                    if(vertices_.at(arc->start).distance(snapped_start) > on_plane_tolerance){
                        vertices_.at(arc->start) = snapped_start;
                        snapped_vertex = true;
                    }
                }
                if(stop_on_plane && (pinned_vertices.count(arc->stop) == 0U)){
                    const auto snapped_stop = point_on_circle(plane(), centre, arc->radius, arc->stop_angle);
                    if(vertices_.at(arc->stop).distance(snapped_stop) > on_plane_tolerance){
                        vertices_.at(arc->stop) = snapped_stop;
                        snapped_vertex = true;
                    }
                }
            }
        }
    }
    return snapped_vertex;
}

void
Sketch::enforce_pinned_vertices(){
    for(const auto &constraint_ptr : constraints_){
        if((constraint_ptr == nullptr) || !constraint_ptr->enabled) continue;
        if(const auto *pin = dynamic_cast<const pin_constraint_t*>(constraint_ptr.get()); pin != nullptr){
            if(vertex_index_valid(pin->vertex)){
                vertices_.at(pin->vertex) = pin->pinned_position;
            }
        }
    }
}

void
Sketch::refresh_all_derived_geometry(){
    std::set<vertex_index_t> pinned_vertices;
    for(const auto &constraint_ptr : constraints_){
        if((constraint_ptr == nullptr) || !constraint_ptr->enabled) continue;
        if(const auto *pin = dynamic_cast<const pin_constraint_t*>(constraint_ptr.get()); pin != nullptr){
            pinned_vertices.insert(pin->vertex);
        }
    }

    const auto max_passes = std::max<std::size_t>(primitives_.size(), 1U);
    for(std::size_t pass = 0U; pass < max_passes; ++pass){
        enforce_pinned_vertices();
        bool any_snapped_vertices = false;
        for(std::size_t i = 0U; i < primitives_.size(); ++i){
            any_snapped_vertices = refresh_primitive_geometry(i, pinned_vertices) || any_snapped_vertices;
        }
        if(!any_snapped_vertices) break;
    }
}

bool
Sketch::primitive_index_valid(primitive_index_t idx) const{
    return idx < primitives_.size();
}

bool
Sketch::vertex_index_valid(vertex_index_t idx) const{
    return idx < vertices_.size();
}

std::vector<vec3<double>>
Sketch::sample_primitive(primitive_index_t idx, std::size_t segments) const{
    if(!primitive_index_valid(idx)) return {};
    const auto pi = std::acos(-1.0);
    const auto *base = primitives_.at(idx).get();
    if(base == nullptr) return {};
    if(!has_plane_){
        switch(base->kind()){
            case primitive_kind_t::circle:
            case primitive_kind_t::arc:
            case primitive_kind_t::bezier:
                return {};
            default:
                break;
        }
    }

    if(const auto *vertex_prim = dynamic_cast<const vertex_primitive_t*>(base); vertex_prim != nullptr){
        return { vertex(vertex_prim->vertex) };
    }
    if(const auto *line = dynamic_cast<const line_primitive_t*>(base); line != nullptr){
        return { vertex(line->vertices[0]), vertex(line->vertices[1]) };
    }
    if(const auto *circle = dynamic_cast<const circle_primitive_t*>(base); circle != nullptr){
        const auto centre = vertex(circle->center);
        std::vector<vec3<double>> out;
        const auto N = std::max<std::size_t>(segments, 16U);
        out.reserve(N + 1U);
        for(std::size_t i = 0U; i <= N; ++i){
            const double theta = (2.0 * pi * static_cast<double>(i)) / static_cast<double>(N);
            out.push_back(centre + plane().row_unit * (std::cos(theta) * circle->radius)
                                 + plane().col_unit * (std::sin(theta) * circle->radius));
        }
        return out;
    }
    if(const auto *arc = dynamic_cast<const arc_primitive_t*>(base); arc != nullptr){
        const auto centre = vertex(arc->center);
        std::vector<vec3<double>> out;
        const auto N = std::max<std::size_t>(segments, 16U);
        out.reserve(N + 1U);
        double start = arc->start_angle;
        double stop = arc->stop_angle;
        if(stop < start) stop += 2.0 * pi;
        for(std::size_t i = 0U; i <= N; ++i){
            const double theta = start + (stop - start) * (static_cast<double>(i) / static_cast<double>(N));
            out.push_back(centre + plane().row_unit * (std::cos(theta) * arc->radius)
                                 + plane().col_unit * (std::sin(theta) * arc->radius));
        }
        return out;
    }
    if(const auto *bezier = dynamic_cast<const bezier_primitive_t*>(base); bezier != nullptr){
        const auto p0 = project(vertex(bezier->control_vertices[0]));
        const auto p1 = project(vertex(bezier->control_vertices[1]));
        const auto p2 = project(vertex(bezier->control_vertices[2]));
        const auto p3 = project(vertex(bezier->control_vertices[3]));
        std::vector<vec3<double>> out;
        const auto N = std::max<std::size_t>(segments, 24U);
        out.reserve(N + 1U);
        for(std::size_t i = 0U; i <= N; ++i){
            const double t = static_cast<double>(i) / static_cast<double>(N);
            out.push_back(lift(cubic_bezier_point(p0, p1, p2, p3, t)));
        }
        return out;
    }
    return {};
}

Sketch::bounding_box_t
Sketch::primitive_bounds(primitive_index_t idx) const{
    bounding_box_t box;
    if(!has_plane_) return box;
    const auto samples = sample_primitive(idx, 48U);
    if(samples.empty()) return box;

    const auto first = project(samples.front());
    box.min = first;
    box.max = first;
    for(const auto &sample : samples){
        const auto p = project(sample);
        box.min.u = std::min(box.min.u, p.u);
        box.min.v = std::min(box.min.v, p.v);
        box.max.u = std::max(box.max.u, p.u);
        box.max.v = std::max(box.max.v, p.v);
    }
    return box;
}

std::optional<Sketch::primitive_index_t>
Sketch::nearest_primitive(const vec3<double> &point, double tolerance) const{
    if(primitives_.empty() || !has_plane_) return {};

    const auto p = project(point);
    const double tol_sq = tolerance * tolerance;
    std::optional<primitive_index_t> best;
    double best_dist_sq = tol_sq;

    for(std::size_t i = 0U; i < primitives_.size(); ++i){
        const auto *base = primitives_.at(i).get();
        if(base == nullptr) continue;

        double dist_sq = std::numeric_limits<double>::infinity();
        if(const auto *vertex_prim = dynamic_cast<const vertex_primitive_t*>(base); vertex_prim != nullptr){
            const auto q = project(vertex(vertex_prim->vertex));
            dist_sq = squared_distance_to_segment(p, q, q);
        }else if(const auto *line = dynamic_cast<const line_primitive_t*>(base); line != nullptr){
            dist_sq = squared_distance_to_segment(p,
                                                  project(vertex(line->vertices[0])),
                                                  project(vertex(line->vertices[1])));
        }else{
            const auto samples = sample_primitive(i, 48U);
            for(std::size_t j = 1U; j < samples.size(); ++j){
                dist_sq = std::min(dist_sq, squared_distance_to_segment(p, project(samples[j - 1U]), project(samples[j])));
            }
        }

        if(dist_sq <= best_dist_sq){
            best = i;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

std::optional<Sketch::vertex_index_t>
Sketch::nearest_vertex(const vec3<double> &point,
                       double tolerance,
                       const std::set<primitive_index_t> &primitive_mask) const{
    if(!has_plane_) return {};
    std::set<vertex_index_t> candidate_vertices;
    if(primitive_mask.empty()){
        for(std::size_t i = 0U; i < vertices_.size(); ++i) candidate_vertices.insert(i);
    }else{
        candidate_vertices = collect_vertices(primitive_mask);
    }

    const auto p = project(point);
    const double tol_sq = tolerance * tolerance;
    std::optional<vertex_index_t> best;
    double best_dist_sq = tol_sq;
    for(const auto idx : candidate_vertices){
        const auto q = project(vertex(idx));
        const double du = q.u - p.u;
        const double dv = q.v - p.v;
        const double dist_sq = (du * du) + (dv * dv);
        if(dist_sq <= best_dist_sq){
            best = idx;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

std::optional<std::pair<Sketch::primitive_index_t, Sketch::vertex_index_t>>
Sketch::nearest_primitive_vertex(const vec3<double> &point,
                                 double tolerance,
                                 const std::set<primitive_index_t> &primitive_mask) const{
    if(!has_plane_) return {};
    const auto p = project(point);
    const double tol_sq = tolerance * tolerance;
    std::optional<std::pair<primitive_index_t, vertex_index_t>> best;
    double best_dist_sq = tol_sq;

    const auto primitive_is_allowed = [&primitive_mask](primitive_index_t idx) -> bool {
        return primitive_mask.empty() || (primitive_mask.count(idx) != 0U);
    };

    for(std::size_t primitive_idx = 0U; primitive_idx < primitives_.size(); ++primitive_idx){
        if(!primitive_is_allowed(primitive_idx) || (primitives_.at(primitive_idx) == nullptr)){
            continue;
        }
        for(const auto vertex_idx : primitives_.at(primitive_idx)->referenced_vertices()){
            const auto q = project(vertex(vertex_idx));
            const auto dist_sq = squared_distance_between(p, q);
            if(dist_sq <= best_dist_sq){
                best = std::make_pair(primitive_idx, vertex_idx);
                best_dist_sq = dist_sq;
            }
        }
    }

    return best;
}

std::vector<Sketch::primitive_index_t>
Sketch::primitives_inside_box(const vec3<double> &a, const vec3<double> &b) const{
    std::vector<primitive_index_t> out;
    if(!has_plane_) return out;
    auto pa = project(a);
    auto pb = project(b);
    bounding_box_t selection;
    selection.min.u = std::min(pa.u, pb.u);
    selection.min.v = std::min(pa.v, pb.v);
    selection.max.u = std::max(pa.u, pb.u);
    selection.max.v = std::max(pa.v, pb.v);

    for(std::size_t i = 0U; i < primitives_.size(); ++i){
        const auto bounds = primitive_bounds(i);
        if(!bounds.is_valid()) continue;
        if(selection.contains(bounds)){
            out.push_back(i);
        }
    }
    return out;
}

std::set<Sketch::vertex_index_t>
Sketch::collect_vertices(const std::set<primitive_index_t> &primitives) const{
    std::set<vertex_index_t> out;
    for(const auto idx : primitives){
        if(!primitive_index_valid(idx)) continue;
        const auto refs = primitives_.at(idx)->referenced_vertices();
        out.insert(refs.begin(), refs.end());
    }
    return out;
}

std::vector<Sketch::primitive_index_t>
Sketch::primitives_referencing_vertex(vertex_index_t idx) const{
    std::vector<primitive_index_t> out;
    if(!vertex_index_valid(idx)) return out;
    for(std::size_t primitive_idx = 0U; primitive_idx < primitives_.size(); ++primitive_idx){
        const auto *primitive_ptr = primitives_.at(primitive_idx).get();
        if(primitive_ptr == nullptr) continue;
        const auto refs = primitive_ptr->referenced_vertices();
        if(std::find(std::begin(refs), std::end(refs), idx) != std::end(refs)){
            out.push_back(primitive_idx);
        }
    }
    return out;
}

void
Sketch::set_vertex(vertex_index_t idx, const vec3<double> &point){
    if(!vertex_index_valid(idx)) throw std::out_of_range("Sketch vertex index is out of range");
    vertices_.at(idx) = point;
    refresh_all_derived_geometry();
}

void
Sketch::translate_vertices(const std::set<vertex_index_t> &indices, const vec3<double> &delta){
    for(const auto idx : indices){
        if(vertex_index_valid(idx)){
            vertices_.at(idx) += delta;
        }
    }
    refresh_all_derived_geometry();
}

void
Sketch::refresh_geometry(){
    refresh_all_derived_geometry();
}

bool
Sketch::clamp_vertices_to_bounds(const bounding_box_t &bounds){
    if(!has_plane_ || !bounds.is_valid()) return false;

    bool changed = false;
    constexpr double min_epsilon = 1.0E-9;

    for(auto &point : vertices_){
        const auto projected = project(point);
        const auto clamped = clamp_to_bounds(projected, bounds);
        if((std::abs(projected.u - clamped.u) > min_epsilon)
        || (std::abs(projected.v - clamped.v) > min_epsilon)){
            point = lift(clamped);
            changed = true;
        }
    }
    if(changed){
        refresh_all_derived_geometry();
    }
    return changed;
}

Sketch::dof_summary_t
Sketch::summarize_degrees_of_freedom() const{
    dof_summary_t out;
    out.total = vertices_.size() * 2U;
    for(const auto &constraint_ptr : constraints_){
        if(constraint_ptr == nullptr) continue;
        if(constraint_ptr->enabled){
            ++out.enabled_constraints;
            switch(constraint_ptr->kind()){
                case constraint_kind_t::horizontal:
                case constraint_kind_t::vertical:
                case constraint_kind_t::distance:
                case constraint_kind_t::parallel:
                case constraint_kind_t::perpendicular:
                case constraint_kind_t::tangent:
                    out.constrained += 1U;
                    break;
                case constraint_kind_t::pin:
                case constraint_kind_t::mirror:
                case constraint_kind_t::overlap:
                    out.constrained += 2U;
                    break;
            }
        }else{
            ++out.disabled_constraints;
        }
    }
    out.remaining = (out.constrained < out.total) ? (out.total - out.constrained) : 0U;
    out.overconstrained = (out.total < out.constrained) ? (out.constrained - out.total) : 0U;
    return out;
}

std::set<Sketch::vertex_index_t>
Sketch::fully_constrained_vertices() const{
    std::set<vertex_index_t> constrained_vertices;

    for(const auto &constraint_ptr : constraints_){
        if((constraint_ptr == nullptr) || !constraint_ptr->enabled) continue;
        if(const auto *pin = dynamic_cast<const pin_constraint_t*>(constraint_ptr.get()); pin != nullptr){
            constrained_vertices.insert(pin->vertex);
        }
    }

    bool changed = true;
    while(changed){
        changed = false;
        for(const auto &constraint_ptr : constraints_){
            if((constraint_ptr == nullptr) || !constraint_ptr->enabled) continue;

            if(const auto *overlap = dynamic_cast<const overlap_constraint_t*>(constraint_ptr.get()); overlap != nullptr){
                const auto has_a = (constrained_vertices.count(overlap->vertex_a) != 0U);
                const auto has_b = (constrained_vertices.count(overlap->vertex_b) != 0U);
                if(has_a || has_b){
                    changed = constrained_vertices.insert(overlap->vertex_a).second || changed;
                    changed = constrained_vertices.insert(overlap->vertex_b).second || changed;
                }
            }else if(const auto *mirror = dynamic_cast<const mirror_constraint_t*>(constraint_ptr.get()); mirror != nullptr){
                const auto *line = dynamic_cast<const line_primitive_t*>(primitive(mirror->line));
                if(line == nullptr) continue;
                const bool line_fixed = (constrained_vertices.count(line->vertices[0]) != 0U)
                                     && (constrained_vertices.count(line->vertices[1]) != 0U);
                if(!line_fixed) continue;
                if(constrained_vertices.count(mirror->vertex_a) != 0U){
                    changed = constrained_vertices.insert(mirror->vertex_b).second || changed;
                }
                if(constrained_vertices.count(mirror->vertex_b) != 0U){
                    changed = constrained_vertices.insert(mirror->vertex_a).second || changed;
                }
            }
        }
    }

    return constrained_vertices;
}

std::set<Sketch::primitive_index_t>
Sketch::fully_constrained_primitives() const{
    const auto constrained_vertices = fully_constrained_vertices();
    std::set<primitive_index_t> constrained_primitives;
    for(std::size_t primitive_idx = 0U; primitive_idx < primitives_.size(); ++primitive_idx){
        const auto *primitive_ptr = primitives_.at(primitive_idx).get();
        if(primitive_ptr == nullptr) continue;
        const auto refs = primitive_ptr->referenced_vertices();
        if(!refs.empty() && std::all_of(std::begin(refs), std::end(refs), [&](const auto vertex_idx){
            return constrained_vertices.count(vertex_idx) != 0U;
        })){
            constrained_primitives.insert(primitive_idx);
        }
    }
    return constrained_primitives;
}

void
Sketch::clear_vertices(){
    vertices_.clear();
    primitives_.clear();
    constraints_.clear();
}

void
Sketch::clear_primitives(){
    primitives_.clear();
    constraints_.clear();
}

void
Sketch::clear_constraints(){
    constraints_.clear();
}

bool
Sketch::delete_constraint(constraint_index_t idx){
    if(!(idx < constraints_.size())) return false;
    constraints_.erase(std::next(std::begin(constraints_), static_cast<std::ptrdiff_t>(idx)));
    return true;
}

bool
Sketch::delete_primitive(primitive_index_t idx){
    if(!primitive_index_valid(idx)) return false;

    primitives_.erase(std::next(std::begin(primitives_), static_cast<std::ptrdiff_t>(idx)));

    std::vector<std::optional<primitive_index_t>> primitive_remap(primitives_.size() + 1U);
    for(std::size_t old_idx = 0U, new_idx = 0U; old_idx < primitive_remap.size(); ++old_idx){
        if(old_idx == idx){
            primitive_remap[old_idx] = {};
        }else{
            primitive_remap[old_idx] = new_idx++;
        }
    }

    std::vector<std::unique_ptr<constraint_t>> new_constraints;
    new_constraints.reserve(constraints_.size());
    for(auto &constraint_ptr : constraints_){
        if(!constraint_ptr) continue;
        bool keep_constraint = true;
        for(const auto referenced_idx : constraint_ptr->referenced_primitives()){
            if( (primitive_remap.size() <= referenced_idx)
            ||  !primitive_remap[referenced_idx] ){
                keep_constraint = false;
                break;
            }
        }
        if(!keep_constraint) continue;
        remap_constraint_primitives(*constraint_ptr, primitive_remap);
        new_constraints.push_back(std::move(constraint_ptr));
    }
    constraints_ = std::move(new_constraints);
    refresh_all_derived_geometry();
    return true;
}

bool
Sketch::delete_vertex(vertex_index_t idx){
    if(!vertex_index_valid(idx)) return false;

    vertices_.erase(std::next(std::begin(vertices_), static_cast<std::ptrdiff_t>(idx)));

    std::vector<std::optional<primitive_index_t>> primitive_remap(primitives_.size());
    std::vector<std::unique_ptr<primitive_t>> new_primitives;
    new_primitives.reserve(primitives_.size());

    for(std::size_t old_idx = 0U; old_idx < primitives_.size(); ++old_idx){
        auto &primitive_ptr = primitives_.at(old_idx);
        if(!primitive_ptr){
            primitive_remap[old_idx] = {};
            continue;
        }

        const auto refs = primitive_ptr->referenced_vertices();
        if(std::find(std::begin(refs), std::end(refs), idx) != std::end(refs)){
            primitive_remap[old_idx] = {};
            continue;
        }

        remap_primitive_vertices(*primitive_ptr, idx);
        primitive_remap[old_idx] = new_primitives.size();
        new_primitives.push_back(std::move(primitive_ptr));
    }
    primitives_ = std::move(new_primitives);

    std::vector<std::unique_ptr<constraint_t>> new_constraints;
    new_constraints.reserve(constraints_.size());
    for(auto &constraint_ptr : constraints_){
        if(!constraint_ptr) continue;
        bool keep_constraint = true;
        for(const auto referenced_vertex : constraint_ptr->referenced_vertices()){
            if(referenced_vertex == idx){
                keep_constraint = false;
                break;
            }
        }
        if(!keep_constraint) continue;
        for(const auto referenced_idx : constraint_ptr->referenced_primitives()){
            if( (primitive_remap.size() <= referenced_idx)
            ||  !primitive_remap[referenced_idx] ){
                keep_constraint = false;
                break;
            }
        }
        if(!keep_constraint) continue;
        remap_constraint_primitives(*constraint_ptr, primitive_remap);
        remap_constraint_vertices(*constraint_ptr, idx);
        new_constraints.push_back(std::move(constraint_ptr));
    }
    constraints_ = std::move(new_constraints);

    refresh_all_derived_geometry();
    return true;
}

std::size_t
Sketch::delete_unreferenced_vertices(){
    std::vector<bool> referenced(vertices_.size(), false);
    std::size_t referenced_count = 0U;
    for(const auto &primitive_ptr : primitives_){
        if(!primitive_ptr) continue;
        for(const auto vertex_idx : primitive_ptr->referenced_vertices()){
            if(vertex_idx < referenced.size() && !referenced[vertex_idx]){
                referenced[vertex_idx] = true;
                ++referenced_count;
            }
        }
    }
    for(const auto &constraint_ptr : constraints_){
        if(!constraint_ptr) continue;
        for(const auto vertex_idx : constraint_ptr->referenced_vertices()){
            if(vertex_idx < referenced.size() && !referenced[vertex_idx]){
                referenced[vertex_idx] = true;
                ++referenced_count;
            }
        }
    }

    if(referenced_count == vertices_.size()) return 0U;

    std::vector<std::optional<vertex_index_t>> vertex_remap(vertices_.size());
    std::vector<vec3<double>> new_vertices;
    new_vertices.reserve(referenced_count);
    for(std::size_t old_idx = 0U; old_idx < vertices_.size(); ++old_idx){
        if(!referenced[old_idx]) continue;
        vertex_remap[old_idx] = new_vertices.size();
        new_vertices.push_back(vertices_.at(old_idx));
    }

    for(auto &primitive_ptr : primitives_){
        if(primitive_ptr != nullptr){
            remap_primitive_vertices(*primitive_ptr, vertex_remap);
        }
    }
    for(auto &constraint_ptr : constraints_){
        if(constraint_ptr != nullptr){
            remap_constraint_vertices(*constraint_ptr, vertex_remap);
        }
    }

    const auto removed = vertices_.size() - new_vertices.size();
    vertices_ = std::move(new_vertices);
    refresh_all_derived_geometry();
    return removed;
}

Sketch::constraint_index_t
Sketch::add_horizontal_constraint(primitive_index_t line_idx){
    if(!primitive_index_valid(line_idx)) throw std::out_of_range("Sketch primitive index is out of range");
    auto constraint = std::make_unique<horizontal_constraint_t>();
    constraint->line = line_idx;
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_vertical_constraint(primitive_index_t line_idx){
    if(!primitive_index_valid(line_idx)) throw std::out_of_range("Sketch primitive index is out of range");
    auto constraint = std::make_unique<vertical_constraint_t>();
    constraint->line = line_idx;
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_distance_constraint(primitive_index_t primitive_idx, double target_distance){
    if(!primitive_index_valid(primitive_idx)) throw std::out_of_range("Sketch primitive index is out of range");
    auto constraint = std::make_unique<distance_constraint_t>();
    constraint->primitive = primitive_idx;
    if(std::isfinite(target_distance)){
        constraint->target_distance = target_distance;
    }else{
        if(const auto *line = dynamic_cast<const line_primitive_t*>(primitive(primitive_idx)); line != nullptr){
            constraint->target_distance = vertex(line->vertices[0]).distance(vertex(line->vertices[1]));
        }else if(const auto *circle = dynamic_cast<const circle_primitive_t*>(primitive(primitive_idx)); circle != nullptr){
            constraint->target_distance = vertex(circle->center).distance(vertex(circle->radius_point));
        }else if(const auto *arc = dynamic_cast<const arc_primitive_t*>(primitive(primitive_idx)); arc != nullptr){
            const auto start_radius = vertex(arc->center).distance(vertex(arc->start));
            const auto stop_radius = vertex(arc->center).distance(vertex(arc->stop));
            constraint->target_distance = 0.5 * (start_radius + stop_radius);
        }else{
            throw std::invalid_argument("Distance constraints currently require a line, circle, or arc primitive");
        }
    }
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_parallel_constraint(primitive_index_t line_a, primitive_index_t line_b){
    if(!primitive_index_valid(line_a) || !primitive_index_valid(line_b)){
        throw std::out_of_range("Sketch primitive index is out of range");
    }
    auto constraint = std::make_unique<parallel_constraint_t>();
    constraint->line_a = line_a;
    constraint->line_b = line_b;
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_perpendicular_constraint(primitive_index_t line_a, primitive_index_t line_b){
    if(!primitive_index_valid(line_a) || !primitive_index_valid(line_b)){
        throw std::out_of_range("Sketch primitive index is out of range");
    }
    auto constraint = std::make_unique<perpendicular_constraint_t>();
    constraint->line_a = line_a;
    constraint->line_b = line_b;
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_pin_constraint(vertex_index_t vertex_idx){
    if(!vertex_index_valid(vertex_idx)){
        throw std::out_of_range("Sketch vertex index is out of range");
    }
    for(std::size_t constraint_idx = 0U; constraint_idx < constraints_.size(); ++constraint_idx){
        if(auto *pin = dynamic_cast<pin_constraint_t*>(constraints_.at(constraint_idx).get()); pin != nullptr){
            if(pin->vertex == vertex_idx){
                pin->enabled = true;
                pin->pinned_position = vertex(vertex_idx);
                return constraint_idx;
            }
        }
    }
    auto constraint = std::make_unique<pin_constraint_t>();
    constraint->vertex = vertex_idx;
    constraint->pinned_position = vertex(vertex_idx);
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_tangent_constraint(primitive_index_t primitive_a, primitive_index_t primitive_b){
    if(!primitive_index_valid(primitive_a) || !primitive_index_valid(primitive_b)){
        throw std::out_of_range("Sketch primitive index is out of range");
    }
    auto constraint = std::make_unique<tangent_constraint_t>();
    constraint->primitive_a = primitive_a;
    constraint->primitive_b = primitive_b;
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_mirror_constraint(primitive_index_t line_idx, vertex_index_t vertex_a, vertex_index_t vertex_b){
    if(!primitive_index_valid(line_idx)){
        throw std::out_of_range("Sketch primitive index is out of range");
    }
    if(!vertex_index_valid(vertex_a) || !vertex_index_valid(vertex_b)){
        throw std::out_of_range("Sketch vertex index is out of range");
    }
    auto constraint = std::make_unique<mirror_constraint_t>();
    constraint->line = line_idx;
    constraint->vertex_a = vertex_a;
    constraint->vertex_b = vertex_b;
    return append_constraint(std::move(constraint));
}

Sketch::constraint_index_t
Sketch::add_overlap_constraint(vertex_index_t vertex_a, vertex_index_t vertex_b){
    if(!vertex_index_valid(vertex_a) || !vertex_index_valid(vertex_b)){
        throw std::out_of_range("Sketch vertex index is out of range");
    }
    auto constraint = std::make_unique<overlap_constraint_t>();
    constraint->vertex_a = vertex_a;
    constraint->vertex_b = vertex_b;
    return append_constraint(std::move(constraint));
}

std::optional<Sketch::vertex_index_t>
Sketch::insert_vertex(primitive_index_t idx, const vec3<double> &point){
    if(!primitive_index_valid(idx) || !has_plane_) return {};

    const auto *base = primitive(idx);
    if(base == nullptr) return {};

    if(const auto *line = dynamic_cast<const line_primitive_t*>(base); line != nullptr){
        const auto new_vertex = append_vertex(point);
        const auto tail_vertex = line->vertices[1];
        if(auto *editable_line = dynamic_cast<line_primitive_t*>(primitive(idx)); editable_line != nullptr){
            editable_line->vertices[1] = new_vertex;
        }
        add_line(new_vertex, tail_vertex, line->tag);
        refresh_all_derived_geometry();
        return new_vertex;
    }

    if(const auto *arc = dynamic_cast<const arc_primitive_t*>(base); arc != nullptr){
        const auto centre = vertex(arc->center);
        const auto clicked = project(point);
        const auto centre_p = project(centre);
        const auto theta = std::atan2(clicked.v - centre_p.v, clicked.u - centre_p.u);
        const auto inserted_point = point_on_circle(plane(), centre, arc->radius, theta);
        const auto new_vertex = append_vertex(inserted_point);
        const auto tail_vertex = arc->stop;
        if(auto *editable_arc = dynamic_cast<arc_primitive_t*>(primitive(idx)); editable_arc != nullptr){
            editable_arc->stop = new_vertex;
        }
        add_arc(arc->center, new_vertex, tail_vertex, arc->tag);
        refresh_all_derived_geometry();
        return new_vertex;
    }

    if(const auto *bezier = dynamic_cast<const bezier_primitive_t*>(base); bezier != nullptr){
        const auto p0 = project(vertex(bezier->control_vertices[0]));
        const auto p1 = project(vertex(bezier->control_vertices[1]));
        const auto p2 = project(vertex(bezier->control_vertices[2]));
        const auto p3 = project(vertex(bezier->control_vertices[3]));
        const auto t = nearest_cubic_bezier_parameter(project(point), p0, p1, p2, p3);

        const auto p01 = lerp(p0, p1, t);
        const auto p12 = lerp(p1, p2, t);
        const auto p23 = lerp(p2, p3, t);
        const auto p012 = lerp(p01, p12, t);
        const auto p123 = lerp(p12, p23, t);
        const auto p0123 = lerp(p012, p123, t);

        const auto v01 = append_vertex(lift(p01));
        const auto v012 = append_vertex(lift(p012));
        const auto v0123 = append_vertex(lift(p0123));
        const auto v123 = append_vertex(lift(p123));
        const auto v23 = append_vertex(lift(p23));

        const auto first_vertex = bezier->control_vertices[0];
        const auto last_vertex = bezier->control_vertices[3];
        const auto tag = bezier->tag;
        if(auto *editable_bezier = dynamic_cast<bezier_primitive_t*>(primitive(idx)); editable_bezier != nullptr){
            editable_bezier->control_vertices = { first_vertex, v01, v012, v0123 };
        }
        add_bezier({ v0123, v123, v23, last_vertex }, tag);
        refresh_all_derived_geometry();
        return v0123;
    }

    return {};
}

bool
Sketch::collapse_vertices(vertex_index_t keep_idx, vertex_index_t remove_idx){
    if(!vertex_index_valid(keep_idx) || !vertex_index_valid(remove_idx) || (keep_idx == remove_idx)){
        return false;
    }

    for(auto &primitive_ptr : primitives_){
        if(primitive_ptr == nullptr) continue;
        if(auto *vertex_primitive = dynamic_cast<vertex_primitive_t*>(primitive_ptr.get()); vertex_primitive != nullptr){
            if(vertex_primitive->vertex == remove_idx) vertex_primitive->vertex = keep_idx;
        }else if(auto *line = dynamic_cast<line_primitive_t*>(primitive_ptr.get()); line != nullptr){
            for(auto &idx_ref : line->vertices){
                if(idx_ref == remove_idx) idx_ref = keep_idx;
            }
        }else if(auto *circle = dynamic_cast<circle_primitive_t*>(primitive_ptr.get()); circle != nullptr){
            if(circle->center == remove_idx) circle->center = keep_idx;
            if(circle->radius_point == remove_idx) circle->radius_point = keep_idx;
        }else if(auto *arc = dynamic_cast<arc_primitive_t*>(primitive_ptr.get()); arc != nullptr){
            if(arc->center == remove_idx) arc->center = keep_idx;
            if(arc->start == remove_idx) arc->start = keep_idx;
            if(arc->stop == remove_idx) arc->stop = keep_idx;
        }else if(auto *bezier = dynamic_cast<bezier_primitive_t*>(primitive_ptr.get()); bezier != nullptr){
            for(auto &idx_ref : bezier->control_vertices){
                if(idx_ref == remove_idx) idx_ref = keep_idx;
            }
        }
    }

    for(auto &constraint_ptr : constraints_){
        if(auto *pin = dynamic_cast<pin_constraint_t*>(constraint_ptr.get()); pin != nullptr){
            if(pin->vertex == remove_idx) pin->vertex = keep_idx;
        }else if(auto *mirror = dynamic_cast<mirror_constraint_t*>(constraint_ptr.get()); mirror != nullptr){
            if(mirror->vertex_a == remove_idx) mirror->vertex_a = keep_idx;
            if(mirror->vertex_b == remove_idx) mirror->vertex_b = keep_idx;
        }else if(auto *overlap = dynamic_cast<overlap_constraint_t*>(constraint_ptr.get()); overlap != nullptr){
            if(overlap->vertex_a == remove_idx) overlap->vertex_a = keep_idx;
            if(overlap->vertex_b == remove_idx) overlap->vertex_b = keep_idx;
        }
    }

    refresh_all_derived_geometry();
    return true;
}

Sketch::support_geometry_result_t
Sketch::add_projected_support_polyline(const std::vector<vec3<double>> &points,
                                       bool closed,
                                       bool add_vertex_primitives){
    if(!has_plane()){
        throw std::logic_error("Sketch plane has not been initialized; call set_plane() before adding projected support geometry");
    }
    support_geometry_result_t out;
    if(points.empty()) return out;

    out.vertices.reserve(points.size());
    const auto edge_primitive_count = (1U < points.size()) ? (closed ? points.size() : (points.size() - 1U)) : 0U;
    const auto vertex_primitive_count = add_vertex_primitives ? points.size() : 0U;
    if(add_vertex_primitives){
        out.primitives.reserve(vertex_primitive_count + edge_primitive_count);
    }else if(0U < edge_primitive_count){
        out.primitives.reserve(edge_primitive_count);
    }
    out.constraints.reserve(points.size());

    for(const auto &point : points){
        const auto projected = lift(project(point));
        const auto vertex_idx = append_vertex(projected);
        out.vertices.emplace_back(vertex_idx);
        if(add_vertex_primitives){
            out.primitives.emplace_back(add_vertex_primitive(vertex_idx, geometry_tag_t::support));
        }
        out.constraints.emplace_back(add_pin_constraint(vertex_idx));
    }

    if(1U < out.vertices.size()){
        for(std::size_t i = 1U; i < out.vertices.size(); ++i){
            out.primitives.emplace_back(add_line(out.vertices.at(i - 1U),
                                                 out.vertices.at(i),
                                                 geometry_tag_t::support));
        }
        if(closed && (2U < out.vertices.size())){
            out.primitives.emplace_back(add_line(out.vertices.back(),
                                                 out.vertices.front(),
                                                 geometry_tag_t::support));
        }
    }
    return out;
}

bool
Sketch::add_fillet(primitive_index_t line_a_idx,
                   primitive_index_t line_b_idx,
                   double radius,
                   std::string *error_message){
    return sketch_fillets::add_fillet(*this, line_a_idx, line_b_idx, radius, error_message);
}

bool
Sketch::swap_arc_orientation(primitive_index_t arc_idx){
    if(!primitive_index_valid(arc_idx)) return false;
    auto *base = primitives_.at(arc_idx).get();
    auto *arc = dynamic_cast<arc_primitive_t*>(base);
    if(arc == nullptr) return false;
    std::swap(arc->start, arc->stop);
    refresh_all_derived_geometry();
    return true;
}

std::size_t
Sketch::solve_constraints(std::size_t max_iterations){
    solve_options_t options;
    options.max_iterations = std::max<std::size_t>(max_iterations, 1U);
    return solve_constraints(options);
}

std::size_t
Sketch::solve_constraints(const solve_options_t &options){
    return sketch_constraints::solve_constraints(*this, options, last_solve_report_);
}

const Sketch::solve_report_t&
Sketch::last_solve_report() const{
    return last_solve_report_;
}

bool
Sketch::build_extruded_surface_mesh(const extrusion_options_t &options,
                                    fv_surface_mesh<double, uint64_t> &mesh,
                                    std::vector<fv_surface_mesh<double, uint64_t>> *cap_meshes,
                                    std::string *error_message) const{
    return sketch_extrude::build_extruded_surface_mesh(*this, options, mesh, cap_meshes, error_message);
}

std::string
Sketch::describe_constraint(constraint_index_t idx) const{
    const auto *c = constraint(idx);
    if(c == nullptr) return "invalid";

    std::ostringstream ss;
    switch(c->kind()){
        case constraint_kind_t::horizontal:
            ss << "horizontal";
            break;
        case constraint_kind_t::vertical:
            ss << "vertical";
            break;
        case constraint_kind_t::distance:
            if(const auto *distance = dynamic_cast<const distance_constraint_t*>(c); distance != nullptr){
                if(dynamic_cast<const line_primitive_t*>(primitive(distance->primitive)) != nullptr){
                    ss << "length";
                }else if([&](){
                    if(const auto *prim = primitive(distance->primitive); prim != nullptr){
                        return dynamic_cast<const circle_primitive_t*>(prim) != nullptr
                            || dynamic_cast<const arc_primitive_t*>(prim) != nullptr;
                    }
                    return false;
                }()){
                    ss << "radius";
                }else{
                    ss << "distance";
                }
            }else{
                ss << "distance";
            }
            break;
        case constraint_kind_t::parallel:
            ss << "parallel";
            break;
        case constraint_kind_t::perpendicular:
            ss << "perpendicular";
            break;
        case constraint_kind_t::pin:
            ss << "pin";
            break;
        case constraint_kind_t::tangent:
            ss << "tangent";
            break;
        case constraint_kind_t::mirror:
            ss << "mirror";
            break;
        case constraint_kind_t::overlap:
            ss << "overlap";
            break;
    }
    return ss.str();
}

bool
Sketch::save_to_file(const std::filesystem::path &path, std::string *error_message) const{
    try{
        std::ofstream fout(path);
        if(!fout.is_open() || !fout.good()){
            store_error(error_message, "Unable to open file for writing");
            return false;
        }
        const auto defaultprecision = fout.precision();
        fout.precision(std::numeric_limits<double>::max_digits10 + 1);

        fout << "sketch_format_version 1\n";

        const auto &p = plane();
        fout << "plane_origin " << p.origin.x << " " << p.origin.y << " " << p.origin.z << "\n";
        fout << "plane_row_unit " << p.row_unit.x << " " << p.row_unit.y << " " << p.row_unit.z << "\n";
        fout << "plane_col_unit " << p.col_unit.x << " " << p.col_unit.y << " " << p.col_unit.z << "\n";

        for(std::size_t i = 0U; i < vertices_.size(); ++i){
            const auto &v = vertices_.at(i);
            fout << "vertex " << i << " " << v.x << " " << v.y << " " << v.z << "\n";
        }

        for(std::size_t i = 0U; i < primitives_.size(); ++i){
            const auto *prim_ptr = primitives_.at(i).get();
            if(prim_ptr == nullptr) continue;
            const auto tag_str = geometry_tag_to_string(prim_ptr->tag);
            if(const auto *vp = dynamic_cast<const vertex_primitive_t*>(prim_ptr); vp != nullptr){
                fout << "primitive vertex " << i << " " << tag_str << " " << vp->vertex << "\n";
            }else if(const auto *lp = dynamic_cast<const line_primitive_t*>(prim_ptr); lp != nullptr){
                fout << "primitive line " << i << " " << tag_str << " " << lp->vertices[0] << " " << lp->vertices[1] << "\n";
            }else if(const auto *cp = dynamic_cast<const circle_primitive_t*>(prim_ptr); cp != nullptr){
                fout << "primitive circle " << i << " " << tag_str << " " << cp->center << " " << cp->radius_point << "\n";
            }else if(const auto *ap = dynamic_cast<const arc_primitive_t*>(prim_ptr); ap != nullptr){
                fout << "primitive arc " << i << " " << tag_str << " " << ap->center << " " << ap->start << " " << ap->stop << "\n";
            }else if(const auto *bp = dynamic_cast<const bezier_primitive_t*>(prim_ptr); bp != nullptr){
                fout << "primitive bezier " << i << " " << tag_str;
                for(const auto &v : bp->control_vertices) fout << " " << v;
                fout << "\n";
            }
        }

        for(std::size_t i = 0U; i < constraints_.size(); ++i){
            const auto *c = constraints_.at(i).get();
            if(c == nullptr) continue;
            if(const auto *hc = dynamic_cast<const horizontal_constraint_t*>(c); hc != nullptr){
                fout << "constraint horizontal " << i << " " << (hc->enabled ? "enabled" : "disabled") << " " << hc->line << "\n";
            }else if(const auto *vc = dynamic_cast<const vertical_constraint_t*>(c); vc != nullptr){
                fout << "constraint vertical " << i << " " << (vc->enabled ? "enabled" : "disabled") << " " << vc->line << "\n";
            }else if(const auto *dc = dynamic_cast<const distance_constraint_t*>(c); dc != nullptr){
                fout << "constraint distance " << i << " " << (dc->enabled ? "enabled" : "disabled") << " " << dc->primitive << " " << dc->target_distance << "\n";
            }else if(const auto *pc = dynamic_cast<const parallel_constraint_t*>(c); pc != nullptr){
                fout << "constraint parallel " << i << " " << (pc->enabled ? "enabled" : "disabled") << " " << pc->line_a << " " << pc->line_b << "\n";
            }else if(const auto *ppc = dynamic_cast<const perpendicular_constraint_t*>(c); ppc != nullptr){
                fout << "constraint perpendicular " << i << " " << (ppc->enabled ? "enabled" : "disabled") << " " << ppc->line_a << " " << ppc->line_b << "\n";
            }else if(const auto *pic = dynamic_cast<const pin_constraint_t*>(c); pic != nullptr){
                fout << "constraint pin " << i << " " << (pic->enabled ? "enabled" : "disabled") << " " << pic->vertex << " "
                     << pic->pinned_position.x << " " << pic->pinned_position.y << " " << pic->pinned_position.z << "\n";
            }else if(const auto *tc = dynamic_cast<const tangent_constraint_t*>(c); tc != nullptr){
                fout << "constraint tangent " << i << " " << (tc->enabled ? "enabled" : "disabled") << " " << tc->primitive_a << " " << tc->primitive_b << "\n";
            }else if(const auto *mc = dynamic_cast<const mirror_constraint_t*>(c); mc != nullptr){
                fout << "constraint mirror " << i << " " << (mc->enabled ? "enabled" : "disabled") << " " << mc->line << " " << mc->vertex_a << " " << mc->vertex_b << "\n";
            }else if(const auto *oc = dynamic_cast<const overlap_constraint_t*>(c); oc != nullptr){
                fout << "constraint overlap " << i << " " << (oc->enabled ? "enabled" : "disabled") << " " << oc->vertex_a << " " << oc->vertex_b << "\n";
            }
        }

        fout.precision(defaultprecision);
        fout.close();
        return true;
    }catch(const std::exception &e){
        store_error(error_message, std::string("Exception while saving sketch: ") + e.what());
        return false;
    }
}

bool
Sketch::load_from_file(const std::filesystem::path &path, Sketch &out, std::string *error_message){
    out.clear();
    try{
        std::ifstream fin(path);
        if(!fin.is_open() || !fin.good()){
            store_error(error_message, "Unable to open file for reading");
            return false;
        }

        std::string line;
        bool plane_set = false;
        while(std::getline(fin, line)){
            std::istringstream iss(line);
            std::string keyword;
            iss >> keyword;
            if(keyword == "sketch_format_version"){
                int version = 0;
                iss >> version;
                if(version != 1){
                    store_error(error_message, "Unsupported sketch format version");
                    return false;
                }
            }else if(keyword == "plane_origin"){
                double x, y, z;
                if(iss >> x >> y >> z){
                    out.plane_.origin = vec3<double>(x, y, z);
                }
            }else if(keyword == "plane_row_unit"){
                double x, y, z;
                if(iss >> x >> y >> z){
                    out.plane_.row_unit = vec3<double>(x, y, z);
                }
            }else if(keyword == "plane_col_unit"){
                double x, y, z;
                if(iss >> x >> y >> z){
                    out.plane_.col_unit = vec3<double>(x, y, z);
                    plane_set = true;
                }
            }else if(keyword == "vertex"){
                std::size_t idx = 0U;
                double x, y, z;
                if(iss >> idx >> x >> y >> z){
                    if(idx >= out.vertices_.size()){
                        out.vertices_.resize(idx + 1U);
                    }
                    out.vertices_.at(idx) = vec3<double>(x, y, z);
                }
            }else if(keyword == "primitive"){
                std::string prim_type;
                std::size_t idx = 0U;
                std::string tag_str;
                iss >> prim_type >> idx >> tag_str;
                geometry_tag_t tag = geometry_tag_t::normal;
                parse_geometry_tag(tag_str, tag);

                auto make_primitive = [&](auto p) -> void {
                    if(idx >= out.primitives_.size()){
                        out.primitives_.resize(idx + 1U);
                    }
                    out.primitives_.at(idx) = std::move(p);
                };

                if(prim_type == "vertex"){
                    std::size_t vertex = 0U;
                    if(iss >> vertex){
                        auto p = std::make_unique<vertex_primitive_t>();
                        p->tag = tag;
                        p->vertex = vertex;
                        make_primitive(std::move(p));
                    }
                }else if(prim_type == "line"){
                    std::size_t v0, v1;
                    if(iss >> v0 >> v1){
                        auto p = std::make_unique<line_primitive_t>();
                        p->tag = tag;
                        p->vertices = { v0, v1 };
                        make_primitive(std::move(p));
                    }
                }else if(prim_type == "circle"){
                    std::size_t center, radius_point;
                    if(iss >> center >> radius_point){
                        auto p = std::make_unique<circle_primitive_t>();
                        p->tag = tag;
                        p->center = center;
                        p->radius_point = radius_point;
                        make_primitive(std::move(p));
                    }
                }else if(prim_type == "arc"){
                    std::size_t center, start, stop;
                    if(iss >> center >> start >> stop){
                        auto p = std::make_unique<arc_primitive_t>();
                        p->tag = tag;
                        p->center = center;
                        p->start = start;
                        p->stop = stop;
                        make_primitive(std::move(p));
                    }
                }else if(prim_type == "bezier"){
                    std::array<std::size_t, 4> cvs = {};
                    if(iss >> cvs[0] >> cvs[1] >> cvs[2] >> cvs[3]){
                        auto p = std::make_unique<bezier_primitive_t>();
                        p->tag = tag;
                        p->control_vertices = cvs;
                        make_primitive(std::move(p));
                    }
                }
            }else if(keyword == "constraint"){
                std::string constraint_type;
                std::size_t idx = 0U;
                std::string enabled_str;
                iss >> constraint_type >> idx >> enabled_str;
                bool enabled = (enabled_str == "enabled");

                auto make_constraint = [&](auto c) -> void {
                    if(idx >= out.constraints_.size()){
                        out.constraints_.resize(idx + 1U);
                    }
                    c->enabled = enabled;
                    out.constraints_.at(idx) = std::move(c);
                };

                if(constraint_type == "horizontal"){
                    std::size_t line = 0U;
                    if(iss >> line){
                        auto c = std::make_unique<horizontal_constraint_t>();
                        c->line = line;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "vertical"){
                    std::size_t line = 0U;
                    if(iss >> line){
                        auto c = std::make_unique<vertical_constraint_t>();
                        c->line = line;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "distance"){
                    std::size_t primitive_idx = 0U;
                    double target_distance = 0.0;
                    if(iss >> primitive_idx >> target_distance){
                        auto c = std::make_unique<distance_constraint_t>();
                        c->primitive = primitive_idx;
                        c->target_distance = target_distance;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "parallel"){
                    std::size_t line_a, line_b;
                    if(iss >> line_a >> line_b){
                        auto c = std::make_unique<parallel_constraint_t>();
                        c->line_a = line_a;
                        c->line_b = line_b;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "perpendicular"){
                    std::size_t line_a, line_b;
                    if(iss >> line_a >> line_b){
                        auto c = std::make_unique<perpendicular_constraint_t>();
                        c->line_a = line_a;
                        c->line_b = line_b;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "pin"){
                    std::size_t vertex_idx = 0U;
                    double x, y, z;
                    if(iss >> vertex_idx >> x >> y >> z){
                        auto c = std::make_unique<pin_constraint_t>();
                        c->vertex = vertex_idx;
                        c->pinned_position = vec3<double>(x, y, z);
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "tangent"){
                    std::size_t pa, pb;
                    if(iss >> pa >> pb){
                        auto c = std::make_unique<tangent_constraint_t>();
                        c->primitive_a = pa;
                        c->primitive_b = pb;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "mirror"){
                    std::size_t line, va, vb;
                    if(iss >> line >> va >> vb){
                        auto c = std::make_unique<mirror_constraint_t>();
                        c->line = line;
                        c->vertex_a = va;
                        c->vertex_b = vb;
                        make_constraint(std::move(c));
                    }
                }else if(constraint_type == "overlap"){
                    std::size_t va, vb;
                    if(iss >> va >> vb){
                        auto c = std::make_unique<overlap_constraint_t>();
                        c->vertex_a = va;
                        c->vertex_b = vb;
                        make_constraint(std::move(c));
                    }
                }
            }
        }

        if(!plane_set){
            store_error(error_message, "Sketch file does not contain a valid plane definition");
            out.clear();
            return false;
        }
        out.has_plane_ = true;
        out.refresh_all_derived_geometry();
        return true;
    }catch(const std::exception &e){
        store_error(error_message, std::string("Exception while loading sketch: ") + e.what());
        out.clear();
        return false;
    }
}
