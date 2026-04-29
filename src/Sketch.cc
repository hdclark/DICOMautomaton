//Sketch.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "Sketch.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>

#ifdef DCMA_USE_EIGEN
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/Sparse>
#endif // DCMA_USE_EIGEN

#include "YgorOptimizeLM.h"

namespace {

constexpr double solver_min_epsilon = 1.0E-9;
constexpr double jacobian_sparsity_threshold = 1.0E-12;
constexpr std::size_t max_refinement_passes = 8U;

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

static Sketch::projection_t reflect_across_line(const Sketch::projection_t &point,
                                                const Sketch::projection_t &line_a,
                                                const Sketch::projection_t &line_b){
    const auto ab_u = line_b.u - line_a.u;
    const auto ab_v = line_b.v - line_a.v;
    const auto denom = (ab_u * ab_u) + (ab_v * ab_v);
    if(denom <= std::numeric_limits<double>::epsilon()){
        return point;
    }

    const auto ap_u = point.u - line_a.u;
    const auto ap_v = point.v - line_a.v;
    const auto t = ((ap_u * ab_u) + (ap_v * ab_v)) / denom;
    const Sketch::projection_t closest = { line_a.u + t * ab_u, line_a.v + t * ab_v };
    return { 2.0 * closest.u - point.u, 2.0 * closest.v - point.v };
}

static std::pair<double, double> orient_unit_direction(double ref_u,
                                                       double ref_v,
                                                       double candidate_u,
                                                       double candidate_v){
    if(((ref_u * candidate_u) + (ref_v * candidate_v)) < 0.0){
        candidate_u = -candidate_u;
        candidate_v = -candidate_v;
    }
    return std::make_pair(candidate_u, candidate_v);
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

static std::size_t constraint_dof_contribution(const Sketch::constraint_t &constraint){
    switch(constraint.kind()){
        case Sketch::constraint_kind_t::horizontal:
        case Sketch::constraint_kind_t::vertical:
        case Sketch::constraint_kind_t::distance:
        case Sketch::constraint_kind_t::parallel:
        case Sketch::constraint_kind_t::perpendicular:
        case Sketch::constraint_kind_t::tangent:
            return 1U;
        case Sketch::constraint_kind_t::pin:
        case Sketch::constraint_kind_t::mirror:
        case Sketch::constraint_kind_t::overlap:
            return 2U;
    }
    return 0U;
}

struct residual_block_t {
    std::size_t constraint_idx = 0U;
    std::size_t begin = 0U;
    std::size_t count = 0U;
    bool sticky = false;
};

struct line_binding_t {
    Sketch::vertex_index_t a = 0U;
    Sketch::vertex_index_t b = 0U;
};

struct round_binding_t {
    Sketch::vertex_index_t center = 0U;
    Sketch::vertex_index_t radius_point = 0U;
};

struct tangent_descriptor_t {
    enum class mode_t {
        unsupported,
        line_round,
        round_round_external,
        round_round_internal,
    };

    mode_t mode = mode_t::unsupported;
    double internal_sign = 1.0;
};

static void append_residual_block(std::vector<double> &residuals,
                                  std::vector<residual_block_t> *blocks,
                                  std::size_t constraint_idx,
                                  std::initializer_list<double> values,
                                  bool sticky = false){
    const auto begin = residuals.size();
    residuals.insert(std::end(residuals), std::begin(values), std::end(values));
    if(blocks != nullptr){
        residual_block_t block;
        block.constraint_idx = constraint_idx;
        block.begin = begin;
        block.count = values.size();
        block.sticky = sticky;
        blocks->push_back(block);
    }
}

static std::optional<line_binding_t> get_line_binding(const Sketch &sketch,
                                                      Sketch::primitive_index_t primitive_idx){
    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(primitive_idx));
    if(line == nullptr) return {};
    return line_binding_t{ line->vertices[0], line->vertices[1] };
}

static std::optional<round_binding_t> get_round_binding(const Sketch &sketch,
                                                        Sketch::primitive_index_t primitive_idx){
    if(const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(primitive_idx)); circle != nullptr){
        return round_binding_t{ circle->center, circle->radius_point };
    }
    if(const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(sketch.primitive(primitive_idx)); arc != nullptr){
        return round_binding_t{ arc->center, arc->start };
    }
    return {};
}

static double signed_distance_to_line(const Sketch::projection_t &point,
                                      const Sketch::projection_t &line_a,
                                      const Sketch::projection_t &line_b){
    const auto du = line_b.u - line_a.u;
    const auto dv = line_b.v - line_a.v;
    const auto denom = std::hypot(du, dv);
    if(denom <= std::numeric_limits<double>::epsilon()){
        const auto offset_u = point.u - line_a.u;
        const auto offset_v = point.v - line_a.v;
        const auto magnitude = std::hypot(offset_u, offset_v);
        const auto sign_basis = (std::abs(offset_v) >= std::abs(offset_u)) ? offset_v : offset_u;
        return std::copysign(magnitude, (sign_basis == 0.0) ? 1.0 : sign_basis);
    }
    return ((point.u - line_a.u) * dv - (point.v - line_a.v) * du) / denom;
}

static double projected_distance(const Sketch::projection_t &a,
                                 const Sketch::projection_t &b){
    return std::hypot(a.u - b.u, a.v - b.v);
}

// Treat segments shorter than the solver epsilon as ill-defined so directional
// constraints can report them as unresolved instead of silently satisfying a
// zero determinant/dot-product identity.
static bool projected_segment_is_degenerate(const Sketch::projection_t &a,
                                            const Sketch::projection_t &b){
    return projected_distance(a, b) <= solver_min_epsilon;
}

struct sketch_solver_context_t {
    const Sketch &sketch;
    const Sketch::solve_options_t &options;
    std::vector<Sketch::projection_t> initial_vertices;
    std::vector<tangent_descriptor_t> tangent_descriptors;

    explicit sketch_solver_context_t(const Sketch &sketch_in,
                                     const Sketch::solve_options_t &options_in)
        : sketch(sketch_in),
          options(options_in) {
        initial_vertices.reserve(sketch.vertex_count());
        for(std::size_t i = 0U; i < sketch.vertex_count(); ++i){
            initial_vertices.push_back(sketch.project(sketch.vertex(i)));
        }

        tangent_descriptors.resize(sketch.constraint_count());
        for(std::size_t i = 0U; i < sketch.constraint_count(); ++i){
            const auto *tangent = dynamic_cast<const Sketch::tangent_constraint_t*>(sketch.constraint(i));
            if(tangent == nullptr) continue;

            const auto line_a = get_line_binding(sketch, tangent->primitive_a);
            const auto line_b = get_line_binding(sketch, tangent->primitive_b);
            const auto round_a = get_round_binding(sketch, tangent->primitive_a);
            const auto round_b = get_round_binding(sketch, tangent->primitive_b);

            auto &descriptor = tangent_descriptors.at(i);
            if(line_a && round_b){
                descriptor.mode = tangent_descriptor_t::mode_t::line_round;
            }else if(round_a && line_b){
                descriptor.mode = tangent_descriptor_t::mode_t::line_round;
            }else if(round_a && round_b){
                const auto centre_a = initial_vertices.at(round_a->center);
                const auto centre_b = initial_vertices.at(round_b->center);
                const auto radius_a = projected_distance(centre_a, initial_vertices.at(round_a->radius_point));
                const auto radius_b = projected_distance(centre_b, initial_vertices.at(round_b->radius_point));
                const auto centre_distance = projected_distance(centre_a, centre_b);
                const auto external_error = std::abs(centre_distance - (radius_a + radius_b));
                const auto internal_error = std::abs(centre_distance - std::abs(radius_a - radius_b));
                if(internal_error < external_error){
                    descriptor.mode = tangent_descriptor_t::mode_t::round_round_internal;
                    descriptor.internal_sign = ((radius_a - radius_b) >= 0.0) ? 1.0 : -1.0;
                }else{
                    descriptor.mode = tangent_descriptor_t::mode_t::round_round_external;
                }
            }
        }
    }

    std::size_t state_size() const{
        return initial_vertices.size() * 2U;
    }

    std::vector<double> initial_state() const{
        std::vector<double> out;
        out.reserve(state_size());
        for(const auto &projected_vertex : initial_vertices){
            out.push_back(projected_vertex.u);
            out.push_back(projected_vertex.v);
        }
        return out;
    }

    Sketch::projection_t projected_vertex(const std::vector<double> &state,
                                          Sketch::vertex_index_t vertex_idx) const{
        const auto base = vertex_idx * 2U;
        return { state.at(base), state.at(base + 1U) };
    }

    double round_radius(const std::vector<double> &state,
                        const round_binding_t &round) const{
        return projected_distance(projected_vertex(state, round.center),
                                  projected_vertex(state, round.radius_point));
    }

    std::vector<double> residual_vector(const std::vector<double> &state,
                                        std::vector<residual_block_t> *blocks = nullptr,
                                        std::size_t *enabled_constraints = nullptr) const{
        std::vector<double> residuals;
        residuals.reserve((sketch.constraint_count() * 2U) + state.size());
        if(blocks != nullptr) blocks->clear();
        if(enabled_constraints != nullptr) *enabled_constraints = 0U;

        for(std::size_t constraint_idx = 0U; constraint_idx < sketch.constraint_count(); ++constraint_idx){
            const auto *constraint_ptr = sketch.constraint(constraint_idx);
            if((constraint_ptr == nullptr) || !constraint_ptr->enabled) continue;
            if(enabled_constraints != nullptr) ++(*enabled_constraints);

            if(const auto *horizontal = dynamic_cast<const Sketch::horizontal_constraint_t*>(constraint_ptr); horizontal != nullptr){
                const auto line = get_line_binding(sketch, horizontal->line);
                if(!line){
                    // Use a unit penalty to keep the residual clearly non-zero and
                    // therefore unresolved, without dominating the rest of the system.
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a = projected_vertex(state, line->a);
                const auto b = projected_vertex(state, line->b);
                append_residual_block(residuals, blocks, constraint_idx, { b.v - a.v });

            }else if(const auto *vertical = dynamic_cast<const Sketch::vertical_constraint_t*>(constraint_ptr); vertical != nullptr){
                const auto line = get_line_binding(sketch, vertical->line);
                if(!line){
                    // Use a unit penalty to keep the residual clearly non-zero and
                    // therefore unresolved, without dominating the rest of the system.
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a = projected_vertex(state, line->a);
                const auto b = projected_vertex(state, line->b);
                append_residual_block(residuals, blocks, constraint_idx, { b.u - a.u });

            }else if(const auto *distance = dynamic_cast<const Sketch::distance_constraint_t*>(constraint_ptr); distance != nullptr){
                const auto line = get_line_binding(sketch, distance->line);
                if(!line){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a = projected_vertex(state, line->a);
                const auto b = projected_vertex(state, line->b);
                const auto du = b.u - a.u;
                const auto dv = b.v - a.v;
                append_residual_block(residuals, blocks, constraint_idx, {
                    (du * du) + (dv * dv) - (distance->target_distance * distance->target_distance)
                });

            }else if(const auto *parallel = dynamic_cast<const Sketch::parallel_constraint_t*>(constraint_ptr); parallel != nullptr){
                const auto line_a = get_line_binding(sketch, parallel->line_a);
                const auto line_b = get_line_binding(sketch, parallel->line_b);
                if(!line_a || !line_b){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a0 = projected_vertex(state, line_a->a);
                const auto a1 = projected_vertex(state, line_a->b);
                const auto b0 = projected_vertex(state, line_b->a);
                const auto b1 = projected_vertex(state, line_b->b);
                if(projected_segment_is_degenerate(a0, a1) || projected_segment_is_degenerate(b0, b1)){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                append_residual_block(residuals, blocks, constraint_idx, {
                    ((a1.u - a0.u) * (b1.v - b0.v)) - ((a1.v - a0.v) * (b1.u - b0.u))
                });

            }else if(const auto *perpendicular = dynamic_cast<const Sketch::perpendicular_constraint_t*>(constraint_ptr); perpendicular != nullptr){
                const auto line_a = get_line_binding(sketch, perpendicular->line_a);
                const auto line_b = get_line_binding(sketch, perpendicular->line_b);
                if(!line_a || !line_b){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a0 = projected_vertex(state, line_a->a);
                const auto a1 = projected_vertex(state, line_a->b);
                const auto b0 = projected_vertex(state, line_b->a);
                const auto b1 = projected_vertex(state, line_b->b);
                if(projected_segment_is_degenerate(a0, a1) || projected_segment_is_degenerate(b0, b1)){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                append_residual_block(residuals, blocks, constraint_idx, {
                    ((a1.u - a0.u) * (b1.u - b0.u)) + ((a1.v - a0.v) * (b1.v - b0.v))
                });

            }else if(const auto *pin = dynamic_cast<const Sketch::pin_constraint_t*>(constraint_ptr); pin != nullptr){
                if(pin->vertex >= initial_vertices.size()){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0, 1.0 });
                    continue;
                }
                const auto projected = projected_vertex(state, pin->vertex);
                const auto target = sketch.project(pin->pinned_position);
                append_residual_block(residuals, blocks, constraint_idx, {
                    projected.u - target.u,
                    projected.v - target.v
                });

            }else if(const auto *tangent = dynamic_cast<const Sketch::tangent_constraint_t*>(constraint_ptr); tangent != nullptr){
                const auto descriptor = tangent_descriptors.at(constraint_idx);
                const auto line_a = get_line_binding(sketch, tangent->primitive_a);
                const auto line_b = get_line_binding(sketch, tangent->primitive_b);
                const auto round_a = get_round_binding(sketch, tangent->primitive_a);
                const auto round_b = get_round_binding(sketch, tangent->primitive_b);

                if(descriptor.mode == tangent_descriptor_t::mode_t::line_round){
                    const auto line = line_a ? line_a : line_b;
                    const auto round = round_a ? round_a : round_b;
                    if(!line || !round){
                        append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                        continue;
                    }
                    const auto p0 = projected_vertex(state, line->a);
                    const auto p1 = projected_vertex(state, line->b);
                    const auto centre = projected_vertex(state, round->center);
                    const auto radius = round_radius(state, *round);
                    append_residual_block(residuals, blocks, constraint_idx, {
                        std::abs(signed_distance_to_line(centre, p0, p1)) - radius
                    });
                }else if(descriptor.mode == tangent_descriptor_t::mode_t::round_round_external
                      || descriptor.mode == tangent_descriptor_t::mode_t::round_round_internal){
                    if(!round_a || !round_b){
                        append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                        continue;
                    }
                    const auto centre_a = projected_vertex(state, round_a->center);
                    const auto centre_b = projected_vertex(state, round_b->center);
                    const auto radius_a = round_radius(state, *round_a);
                    const auto radius_b = round_radius(state, *round_b);
                    const auto centre_distance = projected_distance(centre_a, centre_b);
                    if(descriptor.mode == tangent_descriptor_t::mode_t::round_round_external){
                        append_residual_block(residuals, blocks, constraint_idx, {
                            centre_distance - (radius_a + radius_b)
                        });
                    }else{
                        append_residual_block(residuals, blocks, constraint_idx, {
                            centre_distance - (descriptor.internal_sign * (radius_a - radius_b))
                        });
                    }
                }else{
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                }

            }else if(const auto *mirror = dynamic_cast<const Sketch::mirror_constraint_t*>(constraint_ptr); mirror != nullptr){
                const auto line = get_line_binding(sketch, mirror->line);
                if(!line || (mirror->vertex_a >= initial_vertices.size()) || (mirror->vertex_b >= initial_vertices.size())){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0, 1.0 });
                    continue;
                }
                const auto line_a = projected_vertex(state, line->a);
                const auto line_b = projected_vertex(state, line->b);
                const auto reflected = reflect_across_line(projected_vertex(state, mirror->vertex_a), line_a, line_b);
                const auto vertex_b = projected_vertex(state, mirror->vertex_b);
                append_residual_block(residuals, blocks, constraint_idx, {
                    vertex_b.u - reflected.u,
                    vertex_b.v - reflected.v
                });

            }else if(const auto *overlap = dynamic_cast<const Sketch::overlap_constraint_t*>(constraint_ptr); overlap != nullptr){
                if((overlap->vertex_a >= initial_vertices.size()) || (overlap->vertex_b >= initial_vertices.size())){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0, 1.0 });
                    continue;
                }
                const auto a = projected_vertex(state, overlap->vertex_a);
                const auto b = projected_vertex(state, overlap->vertex_b);
                append_residual_block(residuals, blocks, constraint_idx, {
                    b.u - a.u,
                    b.v - a.v
                });

            }else{
                append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
            }
        }

        if(options.enable_sticky_constraints && (options.sticky_weight > 0.0)){
            for(std::size_t i = 0U; i < initial_vertices.size(); ++i){
                const auto projected = projected_vertex(state, i);
                const auto anchor = initial_vertices.at(i);
                append_residual_block(residuals, blocks, i, {
                    options.sticky_weight * (projected.u - anchor.u),
                    options.sticky_weight * (projected.v - anchor.v)
                }, true);
            }
        }

        return residuals;
    }

#ifdef DCMA_USE_EIGEN
    struct jacobian_bundle_t {
        Eigen::SparseMatrix<double> sparse;
        Eigen::MatrixXd dense;
    };

    jacobian_bundle_t jacobian(const std::vector<double> &state) const{
        std::vector<residual_block_t> blocks;
        const auto base_residuals = residual_vector(state, &blocks, nullptr);
        jacobian_bundle_t out;
        out.dense = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(base_residuals.size()),
                                          static_cast<Eigen::Index>(state.size()));
        std::vector<Eigen::Triplet<double>> triplets;
        const auto step = std::max(options.finite_difference_step, solver_min_epsilon);

        for(std::size_t col = 0U; col < state.size(); ++col){
            auto plus = state;
            auto minus = state;
            plus.at(col) += step;
            minus.at(col) -= step;
            const auto residuals_plus = residual_vector(plus, nullptr, nullptr);
            const auto residuals_minus = residual_vector(minus, nullptr, nullptr);
            for(std::size_t row = 0U; row < base_residuals.size(); ++row){
                const auto derivative = (residuals_plus.at(row) - residuals_minus.at(row)) / (2.0 * step);
                if(std::abs(derivative) > jacobian_sparsity_threshold){
                    out.dense(static_cast<Eigen::Index>(row), static_cast<Eigen::Index>(col)) = derivative;
                    triplets.emplace_back(static_cast<Eigen::Index>(row),
                                          static_cast<Eigen::Index>(col),
                                          derivative);
                }
            }
        }

        out.sparse.resize(static_cast<Eigen::Index>(base_residuals.size()),
                          static_cast<Eigen::Index>(state.size()));
        out.sparse.setFromTriplets(std::begin(triplets), std::end(triplets));
        return out;
    }
#endif // DCMA_USE_EIGEN
};

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
        // Curved planar primitives cache or derive their geometry in plane coordinates, while vertices and lines can be
        // sampled directly from stored world-space points.
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

Sketch::dof_summary_t
Sketch::summarize_degrees_of_freedom() const{
    dof_summary_t out;
    out.total = vertices_.size() * 2U;
    for(const auto &constraint_ptr : constraints_){
        if(constraint_ptr == nullptr) continue;
        if(constraint_ptr->enabled){
            ++out.enabled_constraints;
            out.constrained += constraint_dof_contribution(*constraint_ptr);
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

std::size_t
Sketch::solve_constraints(std::size_t max_iterations){
    solve_options_t options;
    options.max_iterations = std::max<std::size_t>(max_iterations, 1U);
    return solve_constraints(options);
}

std::size_t
Sketch::solve_constraints(const solve_options_t &options){
    last_solve_report_ = {};

    if(!has_plane_ || constraints_.empty() || vertices_.empty()){
        return 0U;
    }

    sketch_solver_context_t context(*this, options);
    auto initial_state = context.initial_state();
    std::size_t enabled_constraints = 0U;
    const auto initial_residuals = context.residual_vector(initial_state, nullptr, &enabled_constraints);
    last_solve_report_.enabled_constraints = enabled_constraints;
    last_solve_report_.residual_count = initial_residuals.size();

    lm_optimizer optimizer;
    optimizer.initial_params = initial_state;
    optimizer.max_iterations = static_cast<int64_t>(std::max<std::size_t>(options.max_iterations, 1U));
    if(options.absolute_tolerance > 0.0){
        optimizer.abs_tol = options.absolute_tolerance;
    }
    if(options.relative_tolerance > 0.0){
        optimizer.rel_tol = options.relative_tolerance;
    }
    if(options.max_time_seconds > 0.0){
        optimizer.max_time = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(options.max_time_seconds));
    }
    optimizer.fd_step = std::max(options.finite_difference_step, solver_min_epsilon);
    optimizer.initial_lambda = std::max(options.initial_lambda, solver_min_epsilon);
    optimizer.lambda_increase_factor = std::max(options.lambda_increase_factor, 1.000001);
    optimizer.lambda_decrease_factor = std::clamp(options.lambda_decrease_factor, 1.0E-6, 0.999999);
    optimizer.f = [&context](const std::vector<double> &state) -> double {
        const auto residuals = context.residual_vector(state, nullptr, nullptr);
        return std::inner_product(std::begin(residuals), std::end(residuals), std::begin(residuals), 0.0);
    };

    const auto result = optimizer.optimize();
    last_solve_report_.iterations = result.iterations;
    last_solve_report_.converged = result.converged;
    last_solve_report_.reason = result.reason;
    last_solve_report_.cost = result.cost;

    for(std::size_t vertex_idx = 0U; vertex_idx < vertices_.size(); ++vertex_idx){
        vertices_.at(vertex_idx) = lift(context.projected_vertex(result.params, vertex_idx));
    }

    bool has_tangent_constraints = false;
    std::set<vertex_index_t> refinement_anchor_vertices;
    for(const auto &constraint_ptr : constraints_){
        if((constraint_ptr == nullptr) || !constraint_ptr->enabled){
            continue;
        }
        if(const auto *horizontal = dynamic_cast<const horizontal_constraint_t*>(constraint_ptr.get()); horizontal != nullptr){
            const auto *line = dynamic_cast<const line_primitive_t*>(primitive(horizontal->line));
            if(line != nullptr) refinement_anchor_vertices.insert(line->vertices[0]);
        }else if(const auto *vertical = dynamic_cast<const vertical_constraint_t*>(constraint_ptr.get()); vertical != nullptr){
            const auto *line = dynamic_cast<const line_primitive_t*>(primitive(vertical->line));
            if(line != nullptr) refinement_anchor_vertices.insert(line->vertices[0]);
        }else if(const auto *distance = dynamic_cast<const distance_constraint_t*>(constraint_ptr.get()); distance != nullptr){
            const auto *line = dynamic_cast<const line_primitive_t*>(primitive(distance->line));
            if(line != nullptr) refinement_anchor_vertices.insert(line->vertices[0]);
        }else if(const auto *parallel = dynamic_cast<const parallel_constraint_t*>(constraint_ptr.get()); parallel != nullptr){
            if(const auto *line = dynamic_cast<const line_primitive_t*>(primitive(parallel->line_a)); line != nullptr){
                refinement_anchor_vertices.insert(line->vertices[0]);
                refinement_anchor_vertices.insert(line->vertices[1]);
            }
        }else if(const auto *perpendicular = dynamic_cast<const perpendicular_constraint_t*>(constraint_ptr.get()); perpendicular != nullptr){
            if(const auto *line = dynamic_cast<const line_primitive_t*>(primitive(perpendicular->line_a)); line != nullptr){
                refinement_anchor_vertices.insert(line->vertices[0]);
                refinement_anchor_vertices.insert(line->vertices[1]);
            }
        }else if(const auto *tangent = dynamic_cast<const tangent_constraint_t*>(constraint_ptr.get()); tangent != nullptr){
            has_tangent_constraints = true;
            if(const auto *primitive_ptr = primitive(tangent->primitive_a); primitive_ptr != nullptr){
                const auto refs = primitive_ptr->referenced_vertices();
                refinement_anchor_vertices.insert(std::begin(refs), std::end(refs));
            }
        }else if(const auto *mirror = dynamic_cast<const mirror_constraint_t*>(constraint_ptr.get()); mirror != nullptr){
            if(const auto *line = dynamic_cast<const line_primitive_t*>(primitive(mirror->line)); line != nullptr){
                refinement_anchor_vertices.insert(line->vertices[0]);
                refinement_anchor_vertices.insert(line->vertices[1]);
            }
            refinement_anchor_vertices.insert(mirror->vertex_a);
        }else if(const auto *overlap = dynamic_cast<const overlap_constraint_t*>(constraint_ptr.get()); overlap != nullptr){
            refinement_anchor_vertices.insert(overlap->vertex_a);
        }
    }

    // Tangency is solved by the LM pass itself; the linear post-pass can move
    // vertices in ways that undo that nonlinear solution, so skip refinement
    // entirely whenever tangent constraints are active.
    if(!has_tangent_constraints){
        const auto refinement_passes = std::min<std::size_t>(std::max<std::size_t>(options.max_iterations, 1U),
                                                             max_refinement_passes);
        for(std::size_t iter = 0U; iter < refinement_passes; ++iter){
            for(const auto vertex_idx : refinement_anchor_vertices){
                if(vertex_idx < context.initial_vertices.size()){
                    vertices_.at(vertex_idx) = lift(context.initial_vertices.at(vertex_idx));
                }
            }
            bool updated_vertices = false;
            for(const auto &constraint_ptr : constraints_){
                if((constraint_ptr == nullptr) || !constraint_ptr->enabled){
                    continue;
                }

                if(const auto *horizontal = dynamic_cast<const horizontal_constraint_t*>(constraint_ptr.get()); horizontal != nullptr){
                    const auto *line = dynamic_cast<const line_primitive_t*>(primitive(horizontal->line));
                    if(line == nullptr) continue;
                    auto a = project(vertex(line->vertices[0]));
                    auto b = project(vertex(line->vertices[1]));
                    b.v = a.v;
                    vertices_.at(line->vertices[1]) = lift(b);
                    updated_vertices = true;

                }else if(const auto *vertical = dynamic_cast<const vertical_constraint_t*>(constraint_ptr.get()); vertical != nullptr){
                    const auto *line = dynamic_cast<const line_primitive_t*>(primitive(vertical->line));
                    if(line == nullptr) continue;
                    auto a = project(vertex(line->vertices[0]));
                    auto b = project(vertex(line->vertices[1]));
                    b.u = a.u;
                    vertices_.at(line->vertices[1]) = lift(b);
                    updated_vertices = true;

                }else if(const auto *distance = dynamic_cast<const distance_constraint_t*>(constraint_ptr.get()); distance != nullptr){
                    const auto *line = dynamic_cast<const line_primitive_t*>(primitive(distance->line));
                    if(line == nullptr) continue;
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
                    if((line_a == nullptr) || (line_b == nullptr)) continue;
                    const auto a0 = project(vertex(line_a->vertices[0]));
                    const auto a1 = project(vertex(line_a->vertices[1]));
                    auto b0 = project(vertex(line_b->vertices[0]));
                    auto b1 = project(vertex(line_b->vertices[1]));
                    auto dir_u = a1.u - a0.u;
                    auto dir_v = a1.v - a0.v;
                    const auto dir_len = std::hypot(dir_u, dir_v);
                    auto len_b = std::hypot(b1.u - b0.u, b1.v - b0.v);
                    if((dir_len <= std::numeric_limits<double>::epsilon()) || (len_b <= std::numeric_limits<double>::epsilon())){
                        continue;
                    }
                    dir_u /= dir_len;
                    dir_v /= dir_len;
                    std::tie(dir_u, dir_v) = orient_unit_direction(b1.u - b0.u,
                                                                   b1.v - b0.v,
                                                                   dir_u,
                                                                   dir_v);
                    b1.u = b0.u + dir_u * len_b;
                    b1.v = b0.v + dir_v * len_b;
                    vertices_.at(line_b->vertices[1]) = lift(b1);
                    updated_vertices = true;

                }else if(const auto *perpendicular = dynamic_cast<const perpendicular_constraint_t*>(constraint_ptr.get()); perpendicular != nullptr){
                    const auto *line_a = dynamic_cast<const line_primitive_t*>(primitive(perpendicular->line_a));
                    const auto *line_b = dynamic_cast<const line_primitive_t*>(primitive(perpendicular->line_b));
                    if((line_a == nullptr) || (line_b == nullptr)) continue;
                    const auto a0 = project(vertex(line_a->vertices[0]));
                    const auto a1 = project(vertex(line_a->vertices[1]));
                    auto b0 = project(vertex(line_b->vertices[0]));
                    auto b1 = project(vertex(line_b->vertices[1]));
                    auto dir_u = a1.u - a0.u;
                    auto dir_v = a1.v - a0.v;
                    const auto dir_len = std::hypot(dir_u, dir_v);
                    auto len_b = std::hypot(b1.u - b0.u, b1.v - b0.v);
                    if((dir_len <= std::numeric_limits<double>::epsilon()) || (len_b <= std::numeric_limits<double>::epsilon())){
                        continue;
                    }
                    dir_u /= dir_len;
                    dir_v /= dir_len;
                    const auto perp_u = -dir_v;
                    const auto perp_v = dir_u;
                    auto [oriented_perp_u, oriented_perp_v] = orient_unit_direction(b1.u - b0.u,
                                                                                    b1.v - b0.v,
                                                                                    perp_u,
                                                                                    perp_v);
                    b1.u = b0.u + oriented_perp_u * len_b;
                    b1.v = b0.v + oriented_perp_v * len_b;
                    vertices_.at(line_b->vertices[1]) = lift(b1);
                    updated_vertices = true;

                }else if(const auto *mirror = dynamic_cast<const mirror_constraint_t*>(constraint_ptr.get()); mirror != nullptr){
                    const auto *line = dynamic_cast<const line_primitive_t*>(primitive(mirror->line));
                    if((line == nullptr) || !vertex_index_valid(mirror->vertex_a) || !vertex_index_valid(mirror->vertex_b)){
                        continue;
                    }
                    const auto a = project(vertex(line->vertices[0]));
                    const auto b = project(vertex(line->vertices[1]));
                    const auto reflected = reflect_across_line(project(vertex(mirror->vertex_a)), a, b);
                    vertices_.at(mirror->vertex_b) = lift(reflected);
                    updated_vertices = true;

                }else if(const auto *overlap = dynamic_cast<const overlap_constraint_t*>(constraint_ptr.get()); overlap != nullptr){
                    if(!vertex_index_valid(overlap->vertex_a) || !vertex_index_valid(overlap->vertex_b)){
                        continue;
                    }
                    vertices_.at(overlap->vertex_b) = vertices_.at(overlap->vertex_a);
                    updated_vertices = true;
                }
            }
            enforce_pinned_vertices();
            if(updated_vertices){
                refresh_all_derived_geometry();
            }else{
                break;
            }
        }
    }
    refresh_all_derived_geometry();

    std::vector<residual_block_t> final_blocks;
    std::size_t final_enabled_constraints = 0U;
    auto final_state = context.initial_state();
    for(std::size_t vertex_idx = 0U; vertex_idx < vertices_.size(); ++vertex_idx){
        const auto base = vertex_idx * 2U;
        const auto projected = project(vertices_.at(vertex_idx));
        final_state.at(base) = projected.u;
        final_state.at(base + 1U) = projected.v;
    }
    const auto final_residuals = context.residual_vector(final_state, &final_blocks, &final_enabled_constraints);
    last_solve_report_.enabled_constraints = final_enabled_constraints;
    last_solve_report_.residual_count = final_residuals.size();

    const auto tolerance = std::max(options.residual_tolerance, solver_min_epsilon);
    for(const auto &block : final_blocks){
        if(block.sticky) continue;
        double block_norm_sq = 0.0;
        for(std::size_t i = 0U; i < block.count; ++i){
            block_norm_sq += final_residuals.at(block.begin + i) * final_residuals.at(block.begin + i);
        }
        if(std::sqrt(block_norm_sq) > tolerance){
            ++last_solve_report_.unresolved_constraints;
        }
    }

#ifdef DCMA_USE_EIGEN
    last_solve_report_.used_svd = false;
    if(last_solve_report_.unresolved_constraints != 0U){
        std::size_t diagnostic_row_count = 0U;
        for(const auto &block : final_blocks){
            if(block.sticky) continue;
            diagnostic_row_count += block.count;
        }

        if(diagnostic_row_count > 0U){
            const auto jacobian = context.jacobian(final_state);
            if((jacobian.dense.rows() >= static_cast<Eigen::Index>(diagnostic_row_count))
            && (jacobian.dense.cols() > 0)){
                Eigen::MatrixXd diagnostic_jacobian(static_cast<Eigen::Index>(diagnostic_row_count),
                                                   jacobian.dense.cols());
                Eigen::VectorXd residual_vector(static_cast<Eigen::Index>(diagnostic_row_count));

                Eigen::Index diagnostic_row = 0;
                for(const auto &block : final_blocks){
                    if(block.sticky) continue;
                    for(std::size_t i = 0U; i < block.count; ++i, ++diagnostic_row){
                        const auto source_row = static_cast<Eigen::Index>(block.begin + i);
                        diagnostic_jacobian.row(diagnostic_row) = jacobian.dense.row(source_row);
                        residual_vector(diagnostic_row) = final_residuals.at(block.begin + i);
                    }
                }

                last_solve_report_.used_svd = true;
                Eigen::BDCSVD<Eigen::MatrixXd> svd(diagnostic_jacobian, Eigen::ComputeThinU | Eigen::ComputeThinV);
                const auto singular_values = svd.singularValues();
                if(singular_values.size() > 0){
                    const auto max_singular = singular_values(0);
                    const auto rank_tolerance = std::numeric_limits<double>::epsilon()
                                              * static_cast<double>(std::max(diagnostic_jacobian.rows(), diagnostic_jacobian.cols()))
                                              * std::max(max_singular, 1.0);
                    last_solve_report_.jacobian_rank = static_cast<std::size_t>((singular_values.array() > rank_tolerance).count());

                    if(last_solve_report_.jacobian_rank > 0U){
                        const auto active_u = svd.matrixU().leftCols(static_cast<Eigen::Index>(last_solve_report_.jacobian_rank));
                        const auto projected_residuals = active_u * (active_u.transpose() * residual_vector);
                        last_solve_report_.conflict_norm = (residual_vector - projected_residuals).norm();
                    }else{
                        last_solve_report_.conflict_norm = residual_vector.norm();
                    }
                    // Treat large off-subspace residual energy as a genuine conflict only when at least one
                    // user-facing constraint also remains unsatisfied; this avoids flagging benign rank
                    // deficiency or slow-but-feasible convergence as a hard conflict.
                    last_solve_report_.conflicting_constraints = (last_solve_report_.conflict_norm > tolerance)
                                                              && (last_solve_report_.unresolved_constraints != 0U);
                }
            }
        }
    }
#endif // DCMA_USE_EIGEN

    return last_solve_report_.unresolved_constraints;
}

const Sketch::solve_report_t&
Sketch::last_solve_report() const{
    return last_solve_report_;
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
        }else if(const auto *perpendicular = dynamic_cast<const perpendicular_constraint_t*>(constraint_ptr.get()); perpendicular != nullptr){
            file << "perpendicular " << (perpendicular->enabled ? 1 : 0) << ' ' << perpendicular->line_a << ' ' << perpendicular->line_b << '\n';
        }else if(const auto *pin = dynamic_cast<const pin_constraint_t*>(constraint_ptr.get()); pin != nullptr){
            file << "pin " << (pin->enabled ? 1 : 0) << ' ' << pin->vertex
                 << ' ' << pin->pinned_position.x
                 << ' ' << pin->pinned_position.y
                 << ' ' << pin->pinned_position.z << '\n';
        }else if(const auto *tangent = dynamic_cast<const tangent_constraint_t*>(constraint_ptr.get()); tangent != nullptr){
            file << "tangent " << (tangent->enabled ? 1 : 0) << ' ' << tangent->primitive_a << ' ' << tangent->primitive_b << '\n';
        }else if(const auto *mirror = dynamic_cast<const mirror_constraint_t*>(constraint_ptr.get()); mirror != nullptr){
            file << "mirror " << (mirror->enabled ? 1 : 0) << ' ' << mirror->line << ' ' << mirror->vertex_a << ' ' << mirror->vertex_b << '\n';
        }else if(const auto *overlap = dynamic_cast<const overlap_constraint_t*>(constraint_ptr.get()); overlap != nullptr){
            file << "overlap " << (overlap->enabled ? 1 : 0) << ' ' << overlap->vertex_a << ' ' << overlap->vertex_b << '\n';
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
        }else if(kind_token == "perpendicular"){
            auto out_constraint = std::make_unique<perpendicular_constraint_t>();
            if(!(file >> out_constraint->line_a >> out_constraint->line_b)) return fail("Unable to read perpendicular constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "pin"){
            auto out_constraint = std::make_unique<pin_constraint_t>();
            if(!(file >> out_constraint->vertex
                      >> out_constraint->pinned_position.x
                      >> out_constraint->pinned_position.y
                      >> out_constraint->pinned_position.z)) return fail("Unable to read pin constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "tangent"){
            auto out_constraint = std::make_unique<tangent_constraint_t>();
            if(!(file >> out_constraint->primitive_a >> out_constraint->primitive_b)) return fail("Unable to read tangent constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "mirror"){
            auto out_constraint = std::make_unique<mirror_constraint_t>();
            if(!(file >> out_constraint->line >> out_constraint->vertex_a >> out_constraint->vertex_b)) return fail("Unable to read mirror constraint");
            constraint = std::move(out_constraint);
        }else if(kind_token == "overlap"){
            auto out_constraint = std::make_unique<overlap_constraint_t>();
            if(!(file >> out_constraint->vertex_a >> out_constraint->vertex_b)) return fail("Unable to read overlap constraint");
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
        for(const auto vertex_idx : constraint->referenced_vertices()){
            if(vertex_count <= vertex_idx){
                return fail("Sketch constraint references an invalid vertex");
            }
        }
        loaded.constraints_.push_back(std::move(constraint));
    }

    loaded.refresh_all_derived_geometry();
    out = std::move(loaded);
    return true;
}
