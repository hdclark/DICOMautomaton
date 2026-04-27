//Sketch.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "Sketch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

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
        distance->line = primitive_remap.at(distance->line).value();
    }else if(auto *parallel = dynamic_cast<Sketch::parallel_constraint_t*>(&constraint); parallel != nullptr){
        parallel->line_a = primitive_remap.at(parallel->line_a).value();
        parallel->line_b = primitive_remap.at(parallel->line_b).value();
    }else if(auto *tangent = dynamic_cast<Sketch::tangent_constraint_t*>(&constraint); tangent != nullptr){
        tangent->primitive_a = primitive_remap.at(tangent->primitive_a).value();
        tangent->primitive_b = primitive_remap.at(tangent->primitive_b).value();
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

}

vec3<double>
Sketch::plane_frame_t::normal() const{
    return row_unit.Cross(col_unit).unit();
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
    return { line };
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

const Sketch::plane_frame_t&
Sketch::plane() const{
    if(!has_plane_) throw std::logic_error("Sketch plane has not been initialized");
    return plane_;
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
    refresh_primitive_geometry(primitives_.size() - 1U);
    return primitives_.size() - 1U;
}

Sketch::constraint_index_t
Sketch::append_constraint(std::unique_ptr<constraint_t> constraint){
    if(!constraint) throw std::invalid_argument("Cannot append empty sketch constraint");
    constraints_.push_back(std::move(constraint));
    return constraints_.size() - 1U;
}

Sketch::primitive_index_t
Sketch::add_vertex_primitive(const vec3<double> &point, geometry_tag_t tag){
    auto primitive = std::make_unique<vertex_primitive_t>();
    primitive->tag = tag;
    primitive->vertex = append_vertex(point);
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_line(const vec3<double> &start, const vec3<double> &stop, geometry_tag_t tag){
    auto primitive = std::make_unique<line_primitive_t>();
    primitive->tag = tag;
    primitive->vertices[0] = append_vertex(start);
    primitive->vertices[1] = append_vertex(stop);
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_circle(const vec3<double> &center, const vec3<double> &radius_point, geometry_tag_t tag){
    auto primitive = std::make_unique<circle_primitive_t>();
    primitive->tag = tag;
    primitive->center = append_vertex(center);
    primitive->radius_point = append_vertex(radius_point);
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_arc(const vec3<double> &center, const vec3<double> &start, const vec3<double> &stop, geometry_tag_t tag){
    auto primitive = std::make_unique<arc_primitive_t>();
    primitive->tag = tag;
    primitive->center = append_vertex(center);
    primitive->start = append_vertex(start);
    primitive->stop = append_vertex(stop);
    return append_primitive(std::move(primitive));
}

Sketch::primitive_index_t
Sketch::add_bezier(const std::array<vec3<double>, 4> &control_points, geometry_tag_t tag){
    auto primitive = std::make_unique<bezier_primitive_t>();
    primitive->tag = tag;
    for(std::size_t i = 0U; i < primitive->control_vertices.size(); ++i){
        primitive->control_vertices[i] = append_vertex(control_points[i]);
    }
    return append_primitive(std::move(primitive));
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
Sketch::refresh_primitive_geometry(primitive_index_t idx){
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

            // The start vertex defines the canonical arc radius during interactive editing; the stop vertex contributes
            // the terminal angle unless the start point collapses onto the centre.
            arc->radius = (start_radius <= std::numeric_limits<double>::epsilon()) ? stop_radius : start_radius;
            arc->start_angle = normalize_angle((start_radius <= std::numeric_limits<double>::epsilon())
                                               ? 0.0
                                               : std::atan2(start_dv, start_du));
            arc->stop_angle = normalize_angle((stop_radius <= std::numeric_limits<double>::epsilon())
                                              ? arc->start_angle
                                              : std::atan2(stop_dv, stop_du));

            if(arc->radius > std::numeric_limits<double>::epsilon()){
                // Arc endpoints are constrained to the shared radius so that editing the stored vertices keeps the
                // rendered arc and draggable endpoints synchronized.
                if(start_on_plane){
                    const auto snapped_start = point_on_circle(plane(), centre, arc->radius, arc->start_angle);
                    if(vertices_.at(arc->start).distance(snapped_start) > on_plane_tolerance){
                        vertices_.at(arc->start) = snapped_start;
                        snapped_vertex = true;
                    }
                }
                if(stop_on_plane){
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
Sketch::refresh_all_derived_geometry(){
    const auto max_passes = std::max<std::size_t>(primitives_.size(), 1U);
    for(std::size_t pass = 0U; pass < max_passes; ++pass){
        bool any_snapped_vertices = false;
        for(std::size_t i = 0U; i < primitives_.size(); ++i){
            any_snapped_vertices = refresh_primitive_geometry(i) || any_snapped_vertices;
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
    if(primitives_.empty()) return {};

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

std::vector<Sketch::primitive_index_t>
Sketch::primitives_inside_box(const vec3<double> &a, const vec3<double> &b) const{
    std::vector<primitive_index_t> out;
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

Sketch::dof_summary_t
Sketch::summarize_degrees_of_freedom() const{
    dof_summary_t out;
    out.total = vertices_.size() * 2U;
    for(const auto &constraint_ptr : constraints_){
        if(constraint_ptr == nullptr) continue;
        if(constraint_ptr->enabled){
            ++out.enabled_constraints;
        }else{
            ++out.disabled_constraints;
        }
    }
    out.constrained = out.enabled_constraints;
    out.remaining = (out.constrained < out.total) ? (out.total - out.constrained) : 0U;
    out.overconstrained = (out.total < out.constrained) ? (out.constrained - out.total) : 0U;
    return out;
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
Sketch::add_distance_constraint(primitive_index_t line_idx, double target_distance){
    if(!primitive_index_valid(line_idx)) throw std::out_of_range("Sketch primitive index is out of range");
    auto constraint = std::make_unique<distance_constraint_t>();
    constraint->line = line_idx;
    if(std::isfinite(target_distance)){
        constraint->target_distance = target_distance;
    }else{
        const auto *line = dynamic_cast<const line_primitive_t*>(primitive(line_idx));
        if(line == nullptr) throw std::invalid_argument("Distance constraints currently require a line primitive");
        constraint->target_distance = vertex(line->vertices[0]).distance(vertex(line->vertices[1]));
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
Sketch::add_tangent_constraint(primitive_index_t primitive_a, primitive_index_t primitive_b){
    if(!primitive_index_valid(primitive_a) || !primitive_index_valid(primitive_b)){
        throw std::out_of_range("Sketch primitive index is out of range");
    }
    auto constraint = std::make_unique<tangent_constraint_t>();
    constraint->primitive_a = primitive_a;
    constraint->primitive_b = primitive_b;
    return append_constraint(std::move(constraint));
}

std::size_t
Sketch::solve_constraints(std::size_t max_iterations){
    std::size_t unresolved = 0U;
    for(std::size_t iter = 0U; iter < std::max<std::size_t>(max_iterations, 1U); ++iter){
        unresolved = 0U;
        bool updated_vertices = false;
        for(const auto &constraint_ptr : constraints_){
            if((constraint_ptr == nullptr) || !constraint_ptr->enabled){
                continue;
            }

            if(const auto *horizontal = dynamic_cast<const horizontal_constraint_t*>(constraint_ptr.get()); horizontal != nullptr){
                const auto *line = dynamic_cast<const line_primitive_t*>(primitive(horizontal->line));
                if(line == nullptr){
                    ++unresolved;
                    continue;
                }
                auto a = project(vertex(line->vertices[0]));
                auto b = project(vertex(line->vertices[1]));
                b.v = a.v;
                vertices_.at(line->vertices[1]) = lift(b);
                updated_vertices = true;

            }else if(const auto *vertical = dynamic_cast<const vertical_constraint_t*>(constraint_ptr.get()); vertical != nullptr){
                const auto *line = dynamic_cast<const line_primitive_t*>(primitive(vertical->line));
                if(line == nullptr){
                    ++unresolved;
                    continue;
                }
                auto a = project(vertex(line->vertices[0]));
                auto b = project(vertex(line->vertices[1]));
                b.u = a.u;
                vertices_.at(line->vertices[1]) = lift(b);
                updated_vertices = true;

            }else if(const auto *distance = dynamic_cast<const distance_constraint_t*>(constraint_ptr.get()); distance != nullptr){
                const auto *line = dynamic_cast<const line_primitive_t*>(primitive(distance->line));
                if(line == nullptr){
                    ++unresolved;
                    continue;
                }
                const auto a = project(vertex(line->vertices[0]));
                auto b = project(vertex(line->vertices[1]));
                auto du = b.u - a.u;
                auto dv = b.v - a.v;
                const auto len = std::hypot(du, dv);
                if(len <= std::numeric_limits<double>::epsilon()){
                    du = 1.0;
                    dv = 0.0;
                }else{
                    du /= len;
                    dv /= len;
                }
                b.u = a.u + du * distance->target_distance;
                b.v = a.v + dv * distance->target_distance;
                vertices_.at(line->vertices[1]) = lift(b);
                updated_vertices = true;

            }else if(const auto *parallel = dynamic_cast<const parallel_constraint_t*>(constraint_ptr.get()); parallel != nullptr){
                const auto *line_a = dynamic_cast<const line_primitive_t*>(primitive(parallel->line_a));
                const auto *line_b = dynamic_cast<const line_primitive_t*>(primitive(parallel->line_b));
                if((line_a == nullptr) || (line_b == nullptr)){
                    ++unresolved;
                    continue;
                }
                const auto a0 = project(vertex(line_a->vertices[0]));
                const auto a1 = project(vertex(line_a->vertices[1]));
                auto b0 = project(vertex(line_b->vertices[0]));
                auto b1 = project(vertex(line_b->vertices[1]));
                auto dir_u = a1.u - a0.u;
                auto dir_v = a1.v - a0.v;
                const auto dir_len = std::hypot(dir_u, dir_v);
                auto len_b = std::hypot(b1.u - b0.u, b1.v - b0.v);
                if((dir_len <= std::numeric_limits<double>::epsilon()) || (len_b <= std::numeric_limits<double>::epsilon())){
                    ++unresolved;
                    continue;
                }
                dir_u /= dir_len;
                dir_v /= dir_len;
                b1.u = b0.u + dir_u * len_b;
                b1.v = b0.v + dir_v * len_b;
                vertices_.at(line_b->vertices[1]) = lift(b1);
                updated_vertices = true;

            }else{
                ++unresolved;
            }
        }
        if(updated_vertices){
            refresh_all_derived_geometry();
        }
    }
    return unresolved;
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
            ss << "distance";
            break;
        case constraint_kind_t::parallel:
            ss << "parallel";
            break;
        case constraint_kind_t::tangent:
            ss << "tangent";
            break;
    }
    return ss.str();
}

bool
Sketch::save_to_file(const std::filesystem::path &path,
                     std::string *error_message) const{
    std::ofstream file(path);
    if(!file){
        store_error(error_message, "Unable to open sketch file for writing");
        return false;
    }
    file << std::setprecision(std::numeric_limits<double>::max_digits10);

    file << "DCMA_SKETCH 1\n";
    file << "plane " << (has_plane_ ? 1 : 0) << '\n';
    if(has_plane_){
        file << "origin " << plane_.origin.x << ' ' << plane_.origin.y << ' ' << plane_.origin.z << '\n';
        file << "row " << plane_.row_unit.x << ' ' << plane_.row_unit.y << ' ' << plane_.row_unit.z << '\n';
        file << "col " << plane_.col_unit.x << ' ' << plane_.col_unit.y << ' ' << plane_.col_unit.z << '\n';
    }

    file << "vertices " << vertices_.size() << '\n';
    for(const auto &vertex : vertices_){
        file << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
    }

    file << "primitives " << primitives_.size() << '\n';
    for(const auto &primitive_ptr : primitives_){
        if(const auto *vertex_primitive = dynamic_cast<const vertex_primitive_t*>(primitive_ptr.get()); vertex_primitive != nullptr){
            file << "vertex " << geometry_tag_to_string(vertex_primitive->tag) << ' ' << vertex_primitive->vertex << '\n';
        }else if(const auto *line = dynamic_cast<const line_primitive_t*>(primitive_ptr.get()); line != nullptr){
            file << "line " << geometry_tag_to_string(line->tag) << ' ' << line->vertices[0] << ' ' << line->vertices[1] << '\n';
        }else if(const auto *circle = dynamic_cast<const circle_primitive_t*>(primitive_ptr.get()); circle != nullptr){
            file << "circle " << geometry_tag_to_string(circle->tag) << ' ' << circle->center << ' ' << circle->radius_point << '\n';
        }else if(const auto *arc = dynamic_cast<const arc_primitive_t*>(primitive_ptr.get()); arc != nullptr){
            file << "arc " << geometry_tag_to_string(arc->tag) << ' ' << arc->center << ' ' << arc->start << ' ' << arc->stop << '\n';
        }else if(const auto *bezier = dynamic_cast<const bezier_primitive_t*>(primitive_ptr.get()); bezier != nullptr){
            file << "bezier " << geometry_tag_to_string(bezier->tag)
                 << ' ' << bezier->control_vertices[0]
                 << ' ' << bezier->control_vertices[1]
                 << ' ' << bezier->control_vertices[2]
                 << ' ' << bezier->control_vertices[3] << '\n';
        }else{
            store_error(error_message, "Encountered unsupported sketch primitive while saving");
            return false;
        }
    }

    file << "constraints " << constraints_.size() << '\n';
    for(const auto &constraint_ptr : constraints_){
        if(const auto *horizontal = dynamic_cast<const horizontal_constraint_t*>(constraint_ptr.get()); horizontal != nullptr){
            file << "horizontal " << (horizontal->enabled ? 1 : 0) << ' ' << horizontal->line << '\n';
        }else if(const auto *vertical = dynamic_cast<const vertical_constraint_t*>(constraint_ptr.get()); vertical != nullptr){
            file << "vertical " << (vertical->enabled ? 1 : 0) << ' ' << vertical->line << '\n';
        }else if(const auto *distance = dynamic_cast<const distance_constraint_t*>(constraint_ptr.get()); distance != nullptr){
            file << "distance " << (distance->enabled ? 1 : 0) << ' ' << distance->line << ' ' << distance->target_distance << '\n';
        }else if(const auto *parallel = dynamic_cast<const parallel_constraint_t*>(constraint_ptr.get()); parallel != nullptr){
            file << "parallel " << (parallel->enabled ? 1 : 0) << ' ' << parallel->line_a << ' ' << parallel->line_b << '\n';
        }else if(const auto *tangent = dynamic_cast<const tangent_constraint_t*>(constraint_ptr.get()); tangent != nullptr){
            file << "tangent " << (tangent->enabled ? 1 : 0) << ' ' << tangent->primitive_a << ' ' << tangent->primitive_b << '\n';
        }else{
            store_error(error_message, "Encountered unsupported sketch constraint while saving");
            return false;
        }
    }

    if(!file){
        store_error(error_message, "Failed while writing sketch file");
        return false;
    }
    return true;
}

bool
Sketch::load_from_file(const std::filesystem::path &path,
                       Sketch &out,
                       std::string *error_message){
    std::ifstream file(path);
    if(!file){
        store_error(error_message, "Unable to open sketch file for reading");
        return false;
    }

    auto fail = [&error_message](const std::string &message) -> bool {
        store_error(error_message, message);
        return false;
    };

    std::string header;
    int version = 0;
    if(!(file >> header >> version) || (header != "DCMA_SKETCH") || (version != 1)){
        return fail("Unrecognized sketch file header");
    }

    Sketch loaded;

    std::string token;
    int has_plane = 0;
    if(!(file >> token >> has_plane) || (token != "plane")){
        return fail("Missing sketch plane header");
    }
    if(has_plane != 0){
        plane_frame_t plane;
        if(!(file >> token >> plane.origin.x >> plane.origin.y >> plane.origin.z) || (token != "origin")){
            return fail("Unable to read sketch plane origin");
        }
        if(!(file >> token >> plane.row_unit.x >> plane.row_unit.y >> plane.row_unit.z) || (token != "row")){
            return fail("Unable to read sketch plane row axis");
        }
        if(!(file >> token >> plane.col_unit.x >> plane.col_unit.y >> plane.col_unit.z) || (token != "col")){
            return fail("Unable to read sketch plane column axis");
        }
        loaded.set_plane(plane);
    }

    std::size_t vertex_count = 0U;
    if(!(file >> token >> vertex_count) || (token != "vertices")){
        return fail("Missing sketch vertex section");
    }
    loaded.vertices_.reserve(vertex_count);
    for(std::size_t i = 0U; i < vertex_count; ++i){
        vec3<double> vertex;
        if(!(file >> vertex.x >> vertex.y >> vertex.z)){
            return fail("Unable to read sketch vertex");
        }
        loaded.vertices_.push_back(vertex);
    }

    std::size_t primitive_count = 0U;
    if(!(file >> token >> primitive_count) || (token != "primitives")){
        return fail("Missing sketch primitive section");
    }
    loaded.primitives_.reserve(primitive_count);
    for(std::size_t i = 0U; i < primitive_count; ++i){
        std::string kind_token;
        std::string tag_token;
        geometry_tag_t tag = geometry_tag_t::normal;
        if(!(file >> kind_token >> tag_token) || !parse_geometry_tag(tag_token, tag)){
            return fail("Unable to read sketch primitive header");
        }

        std::unique_ptr<primitive_t> primitive;
        if(kind_token == "vertex"){
            auto out_primitive = std::make_unique<vertex_primitive_t>();
            if(!(file >> out_primitive->vertex)) return fail("Unable to read vertex primitive");
            primitive = std::move(out_primitive);
        }else if(kind_token == "line"){
            auto out_primitive = std::make_unique<line_primitive_t>();
            if(!(file >> out_primitive->vertices[0] >> out_primitive->vertices[1])) return fail("Unable to read line primitive");
            primitive = std::move(out_primitive);
        }else if(kind_token == "circle"){
            auto out_primitive = std::make_unique<circle_primitive_t>();
            if(!(file >> out_primitive->center >> out_primitive->radius_point)) return fail("Unable to read circle primitive");
            primitive = std::move(out_primitive);
        }else if(kind_token == "arc"){
            auto out_primitive = std::make_unique<arc_primitive_t>();
            if(!(file >> out_primitive->center >> out_primitive->start >> out_primitive->stop)) return fail("Unable to read arc primitive");
            primitive = std::move(out_primitive);
        }else if(kind_token == "bezier"){
            auto out_primitive = std::make_unique<bezier_primitive_t>();
            if(!(file >> out_primitive->control_vertices[0]
                      >> out_primitive->control_vertices[1]
                      >> out_primitive->control_vertices[2]
                      >> out_primitive->control_vertices[3])) return fail("Unable to read bezier primitive");
            primitive = std::move(out_primitive);
        }else{
            return fail("Unrecognized sketch primitive type");
        }

        primitive->tag = tag;
        for(const auto vertex_idx : primitive->referenced_vertices()){
            if(vertex_count <= vertex_idx){
                return fail("Sketch primitive references an invalid vertex");
            }
        }
        loaded.primitives_.push_back(std::move(primitive));
    }

    std::size_t constraint_count = 0U;
    if(!(file >> token >> constraint_count) || (token != "constraints")){
        return fail("Missing sketch constraint section");
    }
    loaded.constraints_.reserve(constraint_count);
    for(std::size_t i = 0U; i < constraint_count; ++i){
        std::string kind_token;
        int enabled = 1;
        if(!(file >> kind_token >> enabled)){
            return fail("Unable to read sketch constraint header");
        }

        std::unique_ptr<constraint_t> constraint;
        if(kind_token == "horizontal"){
            auto out_constraint = std::make_unique<horizontal_constraint_t>();
            if(!(file >> out_constraint->line)) return fail("Unable to read horizontal constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "vertical"){
            auto out_constraint = std::make_unique<vertical_constraint_t>();
            if(!(file >> out_constraint->line)) return fail("Unable to read vertical constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "distance"){
            auto out_constraint = std::make_unique<distance_constraint_t>();
            if(!(file >> out_constraint->line >> out_constraint->target_distance)) return fail("Unable to read distance constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "parallel"){
            auto out_constraint = std::make_unique<parallel_constraint_t>();
            if(!(file >> out_constraint->line_a >> out_constraint->line_b)) return fail("Unable to read parallel constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "tangent"){
            auto out_constraint = std::make_unique<tangent_constraint_t>();
            if(!(file >> out_constraint->primitive_a >> out_constraint->primitive_b)) return fail("Unable to read tangent constraint");
            constraint = std::move(out_constraint);
        }else{
            return fail("Unrecognized sketch constraint type");
        }

        constraint->enabled = (enabled != 0);
        for(const auto primitive_idx : constraint->referenced_primitives()){
            if(primitive_count <= primitive_idx){
                return fail("Sketch constraint references an invalid primitive");
            }
        }
        loaded.constraints_.push_back(std::move(constraint));
    }

    loaded.refresh_all_derived_geometry();
    out = std::move(loaded);
    return true;
}
