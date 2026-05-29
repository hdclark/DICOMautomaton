// Sketch_Constraints.cc - Constraint solver helpers for planar sketches.

#include "Sketch_Constraints.h"
#include "Sketch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifdef DCMA_USE_EIGEN
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/Sparse>
#endif // DCMA_USE_EIGEN

#include "YgorOptimizeLM.h"

namespace {

constexpr double constraint_min_epsilon = 1.0E-9;
constexpr double jacobian_sparsity_threshold = 1.0E-12;
constexpr std::size_t max_refinement_passes = 8U;

Sketch::projection_t reflect_across_line(const Sketch::projection_t &point,
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

std::pair<double, double> orient_unit_direction(double ref_u,
                                                double ref_v,
                                                double candidate_u,
                                                double candidate_v){
    if(((ref_u * candidate_u) + (ref_v * candidate_v)) < 0.0){
        candidate_u = -candidate_u;
        candidate_v = -candidate_v;
    }
    return std::make_pair(candidate_u, candidate_v);
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

    // Heuristic: if the round primitive in a line-round tangent does not
    // have a radius constraint, we insert a fictious one equal to the
    // current radius. This stabilises the solve by preventing the arc
    // from collapsing or jittering while the tangent is being resolved.
    // The value is NaN when no fictious constraint is needed, otherwise
    // it is the target radius to preserve.
    double fictious_radius_target = std::numeric_limits<double>::quiet_NaN();
};

void append_residual_block(std::vector<double> &residuals,
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

std::optional<line_binding_t> get_line_binding(const Sketch &sketch,
                                               Sketch::primitive_index_t primitive_idx){
    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(primitive_idx));
    if(line == nullptr) return {};
    return line_binding_t{ line->vertices[0], line->vertices[1] };
}

std::optional<round_binding_t> get_round_binding(const Sketch &sketch,
                                                 Sketch::primitive_index_t primitive_idx){
    if(const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(primitive_idx)); circle != nullptr){
        return round_binding_t{ circle->center, circle->radius_point };
    }
    if(const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(sketch.primitive(primitive_idx)); arc != nullptr){
        return round_binding_t{ arc->center, arc->start };
    }
    return {};
}

double signed_distance_to_line(const Sketch::projection_t &point,
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

double projected_distance(const Sketch::projection_t &a,
                          const Sketch::projection_t &b){
    return std::hypot(a.u - b.u, a.v - b.v);
}

bool projected_segment_is_degenerate(const Sketch::projection_t &a,
                                     const Sketch::projection_t &b){
    return projected_distance(a, b) <= constraint_min_epsilon;
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
            }
            if(descriptor.mode == tangent_descriptor_t::mode_t::line_round){
                // Determine which primitive index is the round one.
                const auto round_primitive_idx = round_a ? tangent->primitive_a : tangent->primitive_b;
                // Check whether this round primitive already has a distance (radius) constraint.
                bool has_radius_constraint = false;
                for(std::size_t ci = 0U; ci < sketch.constraint_count(); ++ci){
                    const auto *dc = dynamic_cast<const Sketch::distance_constraint_t*>(sketch.constraint(ci));
                    if(dc != nullptr && dc->enabled && dc->primitive == round_primitive_idx){
                        has_radius_constraint = true;
                        break;
                    }
                }
                if(!has_radius_constraint){
                    // Heuristic: preserve the current radius to reduce jitter.
                    // This is a heuristic — it helps the LM solver converge by
                    // preventing the arc from collapsing or expanding during the
                    // tangent solve.
                    const auto round = round_a ? round_a : round_b;
                    const auto centre = initial_vertices.at(round->center);
                    const auto rp = initial_vertices.at(round->radius_point);
                    descriptor.fictious_radius_target = projected_distance(centre, rp);
                }
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
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a = projected_vertex(state, line->a);
                const auto b = projected_vertex(state, line->b);
                append_residual_block(residuals, blocks, constraint_idx, { b.v - a.v });

            }else if(const auto *vertical = dynamic_cast<const Sketch::vertical_constraint_t*>(constraint_ptr); vertical != nullptr){
                const auto line = get_line_binding(sketch, vertical->line);
                if(!line){
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }
                const auto a = projected_vertex(state, line->a);
                const auto b = projected_vertex(state, line->b);
                append_residual_block(residuals, blocks, constraint_idx, { b.u - a.u });

            }else if(const auto *distance = dynamic_cast<const Sketch::distance_constraint_t*>(constraint_ptr); distance != nullptr){
                if(const auto line = get_line_binding(sketch, distance->primitive); line){
                    const auto a = projected_vertex(state, line->a);
                    const auto b = projected_vertex(state, line->b);
                    const auto du = b.u - a.u;
                    const auto dv = b.v - a.v;
                    append_residual_block(residuals, blocks, constraint_idx, {
                        (du * du) + (dv * dv) - (distance->target_distance * distance->target_distance)
                    });
                }else if(const auto round = get_round_binding(sketch, distance->primitive); round){
                    const auto centre = projected_vertex(state, round->center);
                    const auto radius_point = projected_vertex(state, round->radius_point);
                    const auto du = radius_point.u - centre.u;
                    const auto dv = radius_point.v - centre.v;
                    append_residual_block(residuals, blocks, constraint_idx, {
                        (du * du) + (dv * dv) - (distance->target_distance * distance->target_distance)
                    });
                }else{
                    append_residual_block(residuals, blocks, constraint_idx, { 1.0 });
                    continue;
                }

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
                    // Use a smooth squared residual instead of abs() to avoid the
                    // non-differentiable kink at zero, which helps the LM solver converge.
                    const auto sd = signed_distance_to_line(centre, p0, p1);
                    // Detect the non-smooth "5" configuration where the arc's centre
                    // lies between the two line endpoints projected along the line
                    // direction. In the smooth tangent case both endpoints extend away
                    // from the centre in the same direction; in the "5" case they
                    // straddle it. Penalise this configuration to guide the solver
                    // toward the smooth solution.
                    double between_penalty = 0.0;
                    const auto line_du = p1.u - p0.u;
                    const auto line_dv = p1.v - p0.v;
                    const auto line_len = std::hypot(line_du, line_dv);
                    if(line_len > constraint_min_epsilon){
                        const auto lu = line_du / line_len;
                        const auto lv = line_dv / line_len;
                        const auto proj0 = ((p0.u - centre.u) * lu) + ((p0.v - centre.v) * lv);
                        const auto proj1 = ((p1.u - centre.u) * lu) + ((p1.v - centre.v) * lv);
                        const auto proj_product = proj0 * proj1;
                        // If the signed projections have opposite signs, the centre
                        // lies between the endpoints — the non-smooth case.
                        if(proj_product < 0.0){
                            // Penalty proportional to how deeply the centre straddles
                            // the endpoints. The product is negative in the forbidden
                            // zone; zero the residual drives it non-negative.
                            between_penalty = -proj_product;
                        }
                    }
                    append_residual_block(residuals, blocks, constraint_idx, {
                        (sd * sd) - (radius * radius),
                        between_penalty
                    });
                    // Heuristic: if the round primitive lacks a radius constraint,
                    // insert a fictious one to stabilise the tangent solve and
                    // reduce jitter by preventing the arc from collapsing or
                    // expanding while the tangent is being resolved.
                    if(std::isfinite(descriptor.fictious_radius_target)){
                        const auto r_err = (descriptor.fictious_radius_target * descriptor.fictious_radius_target)
                                         - (radius * radius);
                        append_residual_block(residuals, blocks, constraint_idx, { r_err });
                    }
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

        if(options.constrain_to_bounds && options.bounds && options.bounds->is_valid()){
            for(std::size_t i = 0U; i < initial_vertices.size(); ++i){
                const auto projected = projected_vertex(state, i);
                const auto min_u_violation = std::max(options.bounds->min.u - projected.u, 0.0);
                const auto max_u_violation = std::max(projected.u - options.bounds->max.u, 0.0);
                const auto min_v_violation = std::max(options.bounds->min.v - projected.v, 0.0);
                const auto max_v_violation = std::max(projected.v - options.bounds->max.v, 0.0);
                append_residual_block(residuals, blocks, sketch.constraint_count() + i, {
                    min_u_violation,
                    max_u_violation,
                    min_v_violation,
                    max_v_violation
                }, true);
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
        const auto step = std::max(options.finite_difference_step, constraint_min_epsilon);

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

} // anonymous namespace

namespace sketch_constraints {

std::size_t solve_constraints(Sketch &sketch,
                              const Sketch::solve_options_t &options,
                              Sketch::solve_report_t &report){
    report = {};

    const bool solve_with_bounds = options.constrain_to_bounds && options.bounds && options.bounds->is_valid();
    if(!sketch.has_plane() || sketch.vertex_count() == 0U || (sketch.constraint_count() == 0U && !solve_with_bounds)){
        return 0U;
    }

    sketch_solver_context_t context(sketch, options);
    auto initial_state = context.initial_state();
    std::size_t enabled_constraints = 0U;
    const auto initial_residuals = context.residual_vector(initial_state, nullptr, &enabled_constraints);
    report.enabled_constraints = enabled_constraints;
    report.residual_count = initial_residuals.size();

    // Pre-position geometry for tangent constraints involving a line and a round primitive.
    if(sketch.has_plane()){
        for(std::size_t constraint_idx = 0U; constraint_idx < sketch.constraint_count(); ++constraint_idx){
            const auto *tangent = dynamic_cast<const Sketch::tangent_constraint_t*>(sketch.constraint(constraint_idx));
            if((tangent == nullptr) || !sketch.constraint(constraint_idx)->enabled) continue;

            const auto line_binding = get_line_binding(sketch, tangent->primitive_a);
            const auto round_binding_local = get_round_binding(sketch, tangent->primitive_b);
            const bool a_is_line = line_binding.has_value();
            const bool b_is_line = get_line_binding(sketch, tangent->primitive_b).has_value();
            const bool a_is_round = round_binding_local.has_value();
            const bool b_is_round = get_round_binding(sketch, tangent->primitive_a).has_value();

            std::optional<line_binding_t> line;
            std::optional<round_binding_t> round;
            if(a_is_line && b_is_round){
                line = get_line_binding(sketch, tangent->primitive_a);
                round = get_round_binding(sketch, tangent->primitive_b);
            }else if(a_is_round && b_is_line){
                line = get_line_binding(sketch, tangent->primitive_b);
                round = get_round_binding(sketch, tangent->primitive_a);
            }
            if(!line || !round) continue;

            const auto centre_proj = context.projected_vertex(initial_state, round->center);
            const auto radius_point_proj = context.projected_vertex(initial_state, round->radius_point);
            const auto radius = projected_distance(centre_proj, radius_point_proj);
            if(radius <= constraint_min_epsilon) continue;

            const auto line_a_proj = context.projected_vertex(initial_state, line->a);
            const auto line_b_proj = context.projected_vertex(initial_state, line->b);
            const auto line_dir_u = line_b_proj.u - line_a_proj.u;
            const auto line_dir_v = line_b_proj.v - line_a_proj.v;
            const auto line_len = std::hypot(line_dir_u, line_dir_v);
            if(line_len <= constraint_min_epsilon) continue;

            // The line's endpoint nearest the arc centre is the likely contact point.
            const auto dist_a = projected_distance(line_a_proj, centre_proj);
            const auto dist_b = projected_distance(line_b_proj, centre_proj);
            const auto contact_vertex = (dist_a <= dist_b) ? line->a : line->b;
            const auto other_vertex = (dist_a <= dist_b) ? line->b : line->a;

            const auto contact_proj = context.projected_vertex(initial_state, contact_vertex);
            const auto other_proj = context.projected_vertex(initial_state, other_vertex);
            auto radial_u = contact_proj.u - centre_proj.u;
            auto radial_v = contact_proj.v - centre_proj.v;
            const auto radial_len = std::hypot(radial_u, radial_v);
            if(radial_len <= constraint_min_epsilon) continue;
            radial_u /= radial_len;
            radial_v /= radial_len;

            // Tangent direction is perpendicular to the radius.
            const auto tangent_u = -radial_v;
            const auto tangent_v = radial_u;

            // Project the far line endpoint onto the tangent direction to position the contact
            // point on the tangent line at the correct radius distance from the centre.
            const auto other_radial_u = other_proj.u - centre_proj.u;
            const auto other_radial_v = other_proj.v - centre_proj.v;
            const auto dot = (other_radial_u * tangent_u) + (other_radial_v * tangent_v);

            const auto contact_u = centre_proj.u + radial_u * radius;
            const auto contact_v = centre_proj.v + radial_v * radius;
            const auto other_on_tangent_u = contact_u + tangent_u * dot;
            const auto other_on_tangent_v = contact_v + tangent_v * dot;

            const auto base_contact = contact_vertex * 2U;
            const auto base_other = other_vertex * 2U;
            initial_state.at(base_contact)     = contact_u;
            initial_state.at(base_contact + 1U) = contact_v;
            initial_state.at(base_other)     = other_on_tangent_u;
            initial_state.at(base_other + 1U) = other_on_tangent_v;

            // Also move the round primitive's vertex nearest the contact point to the
            // tangent location.
            {
                const auto rp_vert = round->radius_point;
                const auto rp_proj = context.projected_vertex(initial_state, rp_vert);
                const auto dist_rp = projected_distance(rp_proj, { contact_u, contact_v });
                std::optional<Sketch::vertex_index_t> stop_vert;
                double dist_stop = std::numeric_limits<double>::max();
                if(const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(
                        sketch.primitive(tangent->primitive_a));
                   arc != nullptr){
                    stop_vert = arc->stop;
                }else if(const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(
                             sketch.primitive(tangent->primitive_b));
                         arc != nullptr){
                    stop_vert = arc->stop;
                }
                if(stop_vert && (stop_vert.value() < sketch.vertex_count())){
                    const auto stop_proj = context.projected_vertex(initial_state, *stop_vert);
                    dist_stop = projected_distance(stop_proj, { contact_u, contact_v });
                }
                auto round_vertex_to_move = rp_vert;
                if(stop_vert && (dist_stop < dist_rp)){
                    round_vertex_to_move = *stop_vert;
                }
                if((round_vertex_to_move != contact_vertex) && (round_vertex_to_move != other_vertex)){
                    const auto base_rv = round_vertex_to_move * 2U;
                    initial_state.at(base_rv)     = contact_u;
                    initial_state.at(base_rv + 1U) = contact_v;
                }
            }
        }
    }

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
    optimizer.fd_step = std::max(options.finite_difference_step, constraint_min_epsilon);
    optimizer.initial_lambda = std::max(options.initial_lambda, constraint_min_epsilon);
    optimizer.lambda_increase_factor = std::max(options.lambda_increase_factor, 1.000001);
    optimizer.lambda_decrease_factor = std::clamp(options.lambda_decrease_factor, 1.0E-6, 0.999999);
    optimizer.f = [&context](const std::vector<double> &state) -> double {
        const auto residuals = context.residual_vector(state, nullptr, nullptr);
        return std::inner_product(std::begin(residuals), std::end(residuals), std::begin(residuals), 0.0);
    };

    const auto result = optimizer.optimize();
    report.iterations = result.iterations;
    report.converged = result.converged;
    report.reason = result.reason;
    report.cost = result.cost;

    for(std::size_t vertex_idx = 0U; vertex_idx < sketch.vertex_count(); ++vertex_idx){
        sketch.set_vertex(vertex_idx, sketch.lift(context.projected_vertex(result.params, vertex_idx)));
    }

    bool has_tangent_constraints = false;
    std::set<Sketch::vertex_index_t> refinement_anchor_vertices;
    for(std::size_t ci = 0U; ci < sketch.constraint_count(); ++ci){
        const auto *constraint_ptr = sketch.constraint(ci);
        if((constraint_ptr == nullptr) || !constraint_ptr->enabled){
            continue;
        }
        if(const auto *horizontal = dynamic_cast<const Sketch::horizontal_constraint_t*>(constraint_ptr); horizontal != nullptr){
            const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(horizontal->line));
            if(line != nullptr) refinement_anchor_vertices.insert(line->vertices[0]);
        }else if(const auto *vertical = dynamic_cast<const Sketch::vertical_constraint_t*>(constraint_ptr); vertical != nullptr){
            const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(vertical->line));
            if(line != nullptr) refinement_anchor_vertices.insert(line->vertices[0]);
        }else if(const auto *distance = dynamic_cast<const Sketch::distance_constraint_t*>(constraint_ptr); distance != nullptr){
            const auto *distance_primitive = sketch.primitive(distance->primitive);
            const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(distance_primitive);
            if(line != nullptr) refinement_anchor_vertices.insert(line->vertices[0]);
            if(const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(distance_primitive); circle != nullptr){
                refinement_anchor_vertices.insert(circle->center);
            }
            if(const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(distance_primitive); arc != nullptr){
                refinement_anchor_vertices.insert(arc->center);
            }
        }else if(const auto *parallel = dynamic_cast<const Sketch::parallel_constraint_t*>(constraint_ptr); parallel != nullptr){
            if(const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(parallel->line_a)); line != nullptr){
                refinement_anchor_vertices.insert(line->vertices[0]);
                refinement_anchor_vertices.insert(line->vertices[1]);
            }
        }else if(const auto *perpendicular = dynamic_cast<const Sketch::perpendicular_constraint_t*>(constraint_ptr); perpendicular != nullptr){
            if(const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(perpendicular->line_a)); line != nullptr){
                refinement_anchor_vertices.insert(line->vertices[0]);
                refinement_anchor_vertices.insert(line->vertices[1]);
            }
        }else if(const auto *tangent = dynamic_cast<const Sketch::tangent_constraint_t*>(constraint_ptr); tangent != nullptr){
            has_tangent_constraints = true;
            if(const auto *primitive_ptr = sketch.primitive(tangent->primitive_a); primitive_ptr != nullptr){
                const auto refs = primitive_ptr->referenced_vertices();
                refinement_anchor_vertices.insert(std::begin(refs), std::end(refs));
            }
        }else if(const auto *mirror = dynamic_cast<const Sketch::mirror_constraint_t*>(constraint_ptr); mirror != nullptr){
            if(const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(mirror->line)); line != nullptr){
                refinement_anchor_vertices.insert(line->vertices[0]);
                refinement_anchor_vertices.insert(line->vertices[1]);
            }
            refinement_anchor_vertices.insert(mirror->vertex_a);
        }else if(const auto *overlap = dynamic_cast<const Sketch::overlap_constraint_t*>(constraint_ptr); overlap != nullptr){
            refinement_anchor_vertices.insert(overlap->vertex_a);
        }
    }

    // Tangency is solved by the LM pass itself; the linear post-pass can move
    // vertices in ways that undo that nonlinear solution, so skip refinement
    // entirely whenever tangent constraints are active.
    if(!has_tangent_constraints && sketch.constraint_count() > 0U){
        const auto refinement_passes = std::min<std::size_t>(std::max<std::size_t>(options.max_iterations, 1U),
                                                             max_refinement_passes);
        for(std::size_t iter = 0U; iter < refinement_passes; ++iter){
            for(const auto vertex_idx : refinement_anchor_vertices){
                if(vertex_idx < context.initial_vertices.size()){
                    sketch.set_vertex(vertex_idx, sketch.lift(context.initial_vertices.at(vertex_idx)));
                }
            }
            bool updated_vertices = false;
            for(std::size_t ci = 0U; ci < sketch.constraint_count(); ++ci){
                const auto *constraint_ptr = sketch.constraint(ci);
                if((constraint_ptr == nullptr) || !constraint_ptr->enabled){
                    continue;
                }

                if(const auto *horizontal = dynamic_cast<const Sketch::horizontal_constraint_t*>(constraint_ptr); horizontal != nullptr){
                    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(horizontal->line));
                    if(line == nullptr) continue;
                    auto a = sketch.project(sketch.vertex(line->vertices[0]));
                    auto b = sketch.project(sketch.vertex(line->vertices[1]));
                    b.v = a.v;
                    sketch.set_vertex(line->vertices[1], sketch.lift(b));
                    updated_vertices = true;

                }else if(const auto *vertical = dynamic_cast<const Sketch::vertical_constraint_t*>(constraint_ptr); vertical != nullptr){
                    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(vertical->line));
                    if(line == nullptr) continue;
                    auto a = sketch.project(sketch.vertex(line->vertices[0]));
                    auto b = sketch.project(sketch.vertex(line->vertices[1]));
                    b.u = a.u;
                    sketch.set_vertex(line->vertices[1], sketch.lift(b));
                    updated_vertices = true;

                }else if(const auto *distance = dynamic_cast<const Sketch::distance_constraint_t*>(constraint_ptr); distance != nullptr){
                    if(const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(distance->primitive)); line != nullptr){
                        const auto a = sketch.project(sketch.vertex(line->vertices[0]));
                        auto b = sketch.project(sketch.vertex(line->vertices[1]));
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
                        sketch.set_vertex(line->vertices[1], sketch.lift(b));
                        updated_vertices = true;
                    }else if(const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(distance->primitive)); circle != nullptr){
                        const auto centre = sketch.project(sketch.vertex(circle->center));
                        auto radius_point = sketch.project(sketch.vertex(circle->radius_point));
                        auto du = radius_point.u - centre.u;
                        auto dv = radius_point.v - centre.v;
                        const auto len = std::hypot(du, dv);
                        if(len <= std::numeric_limits<double>::epsilon()){
                            du = 1.0;
                            dv = 0.0;
                        }else{
                            du /= len;
                            dv /= len;
                        }
                        radius_point.u = centre.u + du * distance->target_distance;
                        radius_point.v = centre.v + dv * distance->target_distance;
                        sketch.set_vertex(circle->radius_point, sketch.lift(radius_point));
                        updated_vertices = true;
                    }else if(const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(sketch.primitive(distance->primitive)); arc != nullptr){
                        const auto centre = sketch.project(sketch.vertex(arc->center));
                        auto start = sketch.project(sketch.vertex(arc->start));
                        auto stop = sketch.project(sketch.vertex(arc->stop));
                        auto start_u = start.u - centre.u;
                        auto start_v = start.v - centre.v;
                        auto stop_u = stop.u - centre.u;
                        auto stop_v = stop.v - centre.v;
                        const auto start_len = std::hypot(start_u, start_v);
                        const auto stop_len = std::hypot(stop_u, stop_v);
                        if(start_len <= std::numeric_limits<double>::epsilon()){
                            start_u = 1.0;
                            start_v = 0.0;
                        }else{
                            start_u /= start_len;
                            start_v /= start_len;
                        }
                        if(stop_len <= std::numeric_limits<double>::epsilon()){
                            stop_u = 1.0;
                            stop_v = 0.0;
                        }else{
                            stop_u /= stop_len;
                            stop_v /= stop_len;
                        }
                        start.u = centre.u + start_u * distance->target_distance;
                        start.v = centre.v + start_v * distance->target_distance;
                        stop.u = centre.u + stop_u * distance->target_distance;
                        stop.v = centre.v + stop_v * distance->target_distance;
                        sketch.set_vertex(arc->start, sketch.lift(start));
                        sketch.set_vertex(arc->stop, sketch.lift(stop));
                        updated_vertices = true;
                    }

                }else if(const auto *parallel = dynamic_cast<const Sketch::parallel_constraint_t*>(constraint_ptr); parallel != nullptr){
                    const auto *line_a = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(parallel->line_a));
                    const auto *line_b = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(parallel->line_b));
                    if((line_a == nullptr) || (line_b == nullptr)) continue;
                    const auto a0 = sketch.project(sketch.vertex(line_a->vertices[0]));
                    const auto a1 = sketch.project(sketch.vertex(line_a->vertices[1]));
                    auto b0 = sketch.project(sketch.vertex(line_b->vertices[0]));
                    auto b1 = sketch.project(sketch.vertex(line_b->vertices[1]));
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
                    sketch.set_vertex(line_b->vertices[1], sketch.lift(b1));
                    updated_vertices = true;

                }else if(const auto *perpendicular = dynamic_cast<const Sketch::perpendicular_constraint_t*>(constraint_ptr); perpendicular != nullptr){
                    const auto *line_a = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(perpendicular->line_a));
                    const auto *line_b = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(perpendicular->line_b));
                    if((line_a == nullptr) || (line_b == nullptr)) continue;
                    const auto a0 = sketch.project(sketch.vertex(line_a->vertices[0]));
                    const auto a1 = sketch.project(sketch.vertex(line_a->vertices[1]));
                    auto b0 = sketch.project(sketch.vertex(line_b->vertices[0]));
                    auto b1 = sketch.project(sketch.vertex(line_b->vertices[1]));
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
                    sketch.set_vertex(line_b->vertices[1], sketch.lift(b1));
                    updated_vertices = true;

                }else if(const auto *mirror = dynamic_cast<const Sketch::mirror_constraint_t*>(constraint_ptr); mirror != nullptr){
                    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(mirror->line));
                    if((line == nullptr) || (mirror->vertex_a >= sketch.vertex_count()) || (mirror->vertex_b >= sketch.vertex_count())){
                        continue;
                    }
                    const auto a = sketch.project(sketch.vertex(line->vertices[0]));
                    const auto b = sketch.project(sketch.vertex(line->vertices[1]));
                    const auto reflected = reflect_across_line(sketch.project(sketch.vertex(mirror->vertex_a)), a, b);
                    sketch.set_vertex(mirror->vertex_b, sketch.lift(reflected));
                    updated_vertices = true;

                }else if(const auto *overlap = dynamic_cast<const Sketch::overlap_constraint_t*>(constraint_ptr); overlap != nullptr){
                    if((overlap->vertex_a >= sketch.vertex_count()) || (overlap->vertex_b >= sketch.vertex_count())){
                        continue;
                    }
                    sketch.set_vertex(overlap->vertex_b, sketch.vertex(overlap->vertex_a));
                    updated_vertices = true;
                }
            }
            sketch.enforce_pinned_vertices();
            if(updated_vertices){
                sketch.refresh_geometry();
            }else{
                break;
            }
        }
    }
    sketch.refresh_geometry();
    if(solve_with_bounds){
        sketch.clamp_vertices_to_bounds(options.bounds.value());
    }

    std::vector<residual_block_t> final_blocks;
    std::size_t final_enabled_constraints = 0U;
    auto final_state = context.initial_state();
    for(std::size_t vertex_idx = 0U; vertex_idx < sketch.vertex_count(); ++vertex_idx){
        const auto base = vertex_idx * 2U;
        const auto projected = sketch.project(sketch.vertex(vertex_idx));
        final_state.at(base) = projected.u;
        final_state.at(base + 1U) = projected.v;
    }
    const auto final_residuals = context.residual_vector(final_state, &final_blocks, &final_enabled_constraints);
    report.enabled_constraints = final_enabled_constraints;
    report.residual_count = final_residuals.size();

    const auto tolerance = std::max(options.residual_tolerance, constraint_min_epsilon);
    for(const auto &block : final_blocks){
        if(block.sticky) continue;
        double block_norm_sq = 0.0;
        for(std::size_t i = 0U; i < block.count; ++i){
            block_norm_sq += final_residuals.at(block.begin + i) * final_residuals.at(block.begin + i);
        }
        if(std::sqrt(block_norm_sq) > tolerance){
            ++report.unresolved_constraints;
        }
    }

#ifdef DCMA_USE_EIGEN
    report.used_svd = false;
    if(report.unresolved_constraints != 0U){
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

                report.used_svd = true;
                Eigen::BDCSVD<Eigen::MatrixXd> svd(diagnostic_jacobian, Eigen::ComputeThinU | Eigen::ComputeThinV);
                const auto singular_values = svd.singularValues();
                if(singular_values.size() > 0){
                    const auto max_singular = singular_values(0);
                    const auto rank_tolerance = std::numeric_limits<double>::epsilon()
                                              * static_cast<double>(std::max(diagnostic_jacobian.rows(), diagnostic_jacobian.cols()))
                                              * std::max(max_singular, 1.0);
                    report.jacobian_rank = static_cast<std::size_t>((singular_values.array() > rank_tolerance).count());

                    if(report.jacobian_rank > 0U){
                        const auto active_u = svd.matrixU().leftCols(static_cast<Eigen::Index>(report.jacobian_rank));
                        const auto projected_residuals = active_u * (active_u.transpose() * residual_vector);
                        report.conflict_norm = (residual_vector - projected_residuals).norm();
                    }else{
                        report.conflict_norm = residual_vector.norm();
                    }
                    report.conflicting_constraints = (report.conflict_norm > tolerance)
                                                  && (report.unresolved_constraints != 0U);
                }
            }
        }
    }
#endif // DCMA_USE_EIGEN

    return report.unresolved_constraints;
}

} // namespace sketch_constraints
