// Sketch_Tests.cc.

#include "doctest20251212/doctest.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <set>
#include <vector>

#include "Sketch.h"

namespace {

static Sketch::plane_frame_t default_xy_plane(){
    Sketch::plane_frame_t out;
    out.origin = vec3<double>(0.0, 0.0, 0.0);
    out.row_unit = vec3<double>(1.0, 0.0, 0.0);
    out.col_unit = vec3<double>(0.0, 1.0, 0.0);
    return out;
}

static Sketch::plane_frame_t default_xz_plane(){
    Sketch::plane_frame_t out;
    out.origin = vec3<double>(0.0, 0.0, 0.0);
    out.row_unit = vec3<double>(1.0, 0.0, 0.0);
    out.col_unit = vec3<double>(0.0, 0.0, 1.0);
    return out;
}

}

TEST_CASE("Sketch stores indexed primitives"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto vertex_idx = sketch.add_vertex_primitive(vec3<double>(1.0, 2.0, 0.0), Sketch::geometry_tag_t::normal);
    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(2.0, 0.0, 0.0),
                                          Sketch::geometry_tag_t::support);

    REQUIRE( sketch.vertex_count() == 3U );
    REQUIRE( sketch.primitive_count() == 2U );
    REQUIRE( sketch.primitive(vertex_idx) != nullptr );
    REQUIRE( sketch.primitive(line_idx) != nullptr );

    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_idx));
    REQUIRE( line != nullptr );
    REQUIRE( line->vertices[0] == 1U );
    REQUIRE( line->vertices[1] == 2U );
}

TEST_CASE("Sketch selection helpers find primitives and boxed items"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(4.0, 0.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    sketch.add_circle(vec3<double>(8.0, 8.0, 0.0),
                      vec3<double>(9.0, 8.0, 0.0),
                      Sketch::geometry_tag_t::support);

    const auto near_line = sketch.nearest_primitive(vec3<double>(2.0, 0.2, 0.0), 0.5);
    REQUIRE( near_line.has_value() );
    REQUIRE( near_line.value() == line_idx );

    const auto boxed = sketch.primitives_inside_box(vec3<double>(-1.0, -1.0, 0.0),
                                                    vec3<double>(5.0, 1.0, 0.0));
    REQUIRE( boxed.size() == 1U );
    REQUIRE( boxed.front() == line_idx );
}

TEST_CASE("Sketch selection helpers handle uninitialized plane gracefully"){
    Sketch sketch;
    sketch.add_vertex_primitive(vec3<double>(1.0, 2.0, 3.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                    vec3<double>(4.0, 0.0, 0.0),
                    Sketch::geometry_tag_t::support);
    sketch.add_circle(vec3<double>(8.0, 8.0, 0.0),
                      vec3<double>(9.0, 8.0, 0.0),
                      Sketch::geometry_tag_t::support);
    sketch.add_bezier({ vec3<double>(0.0, 0.0, 0.0),
                        vec3<double>(1.0, 2.0, 0.0),
                        vec3<double>(2.0, 2.0, 0.0),
                        vec3<double>(3.0, 0.0, 0.0) },
                      Sketch::geometry_tag_t::normal);

    REQUIRE_FALSE( sketch.has_plane() );
    REQUIRE_FALSE( sketch.nearest_primitive(vec3<double>(1.0, 0.0, 0.0), 1.0).has_value() );
    REQUIRE_FALSE( sketch.nearest_vertex(vec3<double>(1.0, 0.0, 0.0), 1.0).has_value() );
    REQUIRE_FALSE( sketch.nearest_primitive_vertex(vec3<double>(1.0, 0.0, 0.0), 1.0).has_value() );
    REQUIRE( sketch.primitives_inside_box(vec3<double>(-1.0, -1.0, 0.0),
                                          vec3<double>(5.0, 5.0, 0.0)).empty() );
    REQUIRE( sketch.sample_primitive(2U, 16U).empty() );
    REQUIRE_FALSE( sketch.primitive_bounds(2U).is_valid() );
}

TEST_CASE("Sketch constraints resolve simple geometry"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto horizontal_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                                vec3<double>(3.0, 2.0, 0.0),
                                                Sketch::geometry_tag_t::normal);
    const auto vertical_idx = sketch.add_line(vec3<double>(1.0, 1.0, 0.0),
                                              vec3<double>(4.0, 5.0, 0.0),
                                              Sketch::geometry_tag_t::normal);
    const auto distance_idx = sketch.add_line(vec3<double>(0.0, 3.0, 0.0),
                                              vec3<double>(1.0, 3.0, 0.0),
                                              Sketch::geometry_tag_t::normal);

    sketch.add_horizontal_constraint(horizontal_idx);
    sketch.add_vertical_constraint(vertical_idx);
    sketch.add_distance_constraint(distance_idx, 5.0);

    const auto unresolved = sketch.solve_constraints();
    REQUIRE( unresolved == 0U );

    const auto *horizontal = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(horizontal_idx));
    const auto *vertical = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(vertical_idx));
    const auto *distance = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(distance_idx));
    REQUIRE( horizontal != nullptr );
    REQUIRE( vertical != nullptr );
    REQUIRE( distance != nullptr );

    REQUIRE( doctest::Approx(sketch.vertex(horizontal->vertices[0]).y) == sketch.vertex(horizontal->vertices[1]).y );
    REQUIRE( doctest::Approx(sketch.vertex(vertical->vertices[0]).x) == sketch.vertex(vertical->vertices[1]).x );
    REQUIRE( doctest::Approx(sketch.vertex(distance->vertices[0]).distance(sketch.vertex(distance->vertices[1]))).epsilon(1E-6) == 5.0 );
}

TEST_CASE("Sketch distance constraints can control round primitive radii"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto circle_idx = sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                                              vec3<double>(1.0, 0.0, 0.0),
                                              Sketch::geometry_tag_t::normal);
    const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    REQUIRE( circle != nullptr );

    sketch.add_pin_constraint(circle->center);
    sketch.add_distance_constraint(circle_idx, 3.5);

    const auto unresolved = sketch.solve_constraints();
    REQUIRE( unresolved == 0U );
    CHECK( doctest::Approx(sketch.vertex(circle->center).distance(sketch.vertex(circle->radius_point))).epsilon(1E-6) == 3.5 );
}

TEST_CASE("Sketch tangent constraints solve line-circle tangency"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto circle_idx = sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                                              vec3<double>(1.0, 0.0, 0.0),
                                              Sketch::geometry_tag_t::normal);
    const auto line_idx = sketch.add_line(vec3<double>(-2.0, 0.4, 0.0),
                                          vec3<double>(2.0, 0.4, 0.0),
                                          Sketch::geometry_tag_t::support);

    const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_idx));
    REQUIRE( circle != nullptr );
    REQUIRE( line != nullptr );

    sketch.add_pin_constraint(circle->center);
    sketch.add_pin_constraint(circle->radius_point);
    sketch.add_tangent_constraint(circle_idx, line_idx);

    Sketch::solve_options_t options;
    options.max_iterations = 128U;
    const auto unresolved = sketch.solve_constraints(options);
    REQUIRE( unresolved == 0U );

    const auto centre = sketch.vertex(circle->center);
    const auto p0 = sketch.vertex(line->vertices[0]);
    const auto p1 = sketch.vertex(line->vertices[1]);
    const auto cross_product_mag = std::abs(((centre.x - p0.x) * (p1.y - p0.y)) - ((centre.y - p0.y) * (p1.x - p0.x)));
    const auto denominator = std::hypot(p1.x - p0.x, p1.y - p0.y);
    REQUIRE( denominator > 1.0E-9 );
    REQUIRE( doctest::Approx(cross_product_mag / denominator).epsilon(1E-4) == 1.0 );
}

TEST_CASE("Sketch tangent constraints report infeasible fixed geometry"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto circle_idx = sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                                              vec3<double>(3.0, 0.0, 0.0),
                                              Sketch::geometry_tag_t::normal);
    const auto line_idx = sketch.add_line(vec3<double>(-2.0, 1.0, 0.0),
                                          vec3<double>(2.0, 1.0, 0.0),
                                          Sketch::geometry_tag_t::support);

    const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_idx));
    REQUIRE( circle != nullptr );
    REQUIRE( line != nullptr );

    sketch.add_pin_constraint(circle->center);
    sketch.add_pin_constraint(circle->radius_point);
    sketch.add_pin_constraint(line->vertices[0]);
    sketch.add_pin_constraint(line->vertices[1]);
    sketch.add_tangent_constraint(circle_idx, line_idx);

    Sketch::solve_options_t options;
    options.max_iterations = 128U;
    REQUIRE( sketch.solve_constraints(options) != 0U );
}

TEST_CASE("Sketch sticky constraints stabilize underconstrained solutions"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_a_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                            vec3<double>(4.0, 0.0, 0.0),
                                            Sketch::geometry_tag_t::support);
    const auto line_b_idx = sketch.add_line(vec3<double>(10.0, 5.0, 0.0),
                                            vec3<double>(12.0, 5.2, 0.0),
                                            Sketch::geometry_tag_t::normal);

    const auto *line_a = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_a_idx));
    const auto *line_b = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_b_idx));
    REQUIRE( line_a != nullptr );
    REQUIRE( line_b != nullptr );

    const auto original_line_b_start = sketch.vertex(line_b->vertices[0]);
    const auto original_line_b_stop = sketch.vertex(line_b->vertices[1]);

    sketch.add_pin_constraint(line_a->vertices[0]);
    sketch.add_pin_constraint(line_a->vertices[1]);
    sketch.add_parallel_constraint(line_a_idx, line_b_idx);
    sketch.add_distance_constraint(line_b_idx, 4.0);

    Sketch::solve_options_t options;
    options.max_iterations = 128U;
    options.sticky_weight = 1.0E-1;
    const auto unresolved = sketch.solve_constraints(options);
    REQUIRE( unresolved == 0U );

    const auto solved_line_b_start = sketch.vertex(line_b->vertices[0]);
    const auto solved_line_b_stop = sketch.vertex(line_b->vertices[1]);
    // The reference line is anchored during post-solve refinement, so the free line's leading
    // endpoint should remain closer to its original position than the trailing endpoint that is
    // explicitly adjusted to satisfy the length/direction constraints.
    CHECK( solved_line_b_start.distance(original_line_b_start) < solved_line_b_stop.distance(original_line_b_stop) );
    CHECK( doctest::Approx(solved_line_b_start.y).epsilon(1E-5) == solved_line_b_stop.y );
    CHECK( doctest::Approx(solved_line_b_start.distance(solved_line_b_stop)).epsilon(1E-5) == 4.0 );
}

TEST_CASE("Sketch solver reports conflicting constraints"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(2.0, 1.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_idx));
    REQUIRE( line != nullptr );

    sketch.add_pin_constraint(line->vertices[0]);
    sketch.add_horizontal_constraint(line_idx);
    sketch.add_vertical_constraint(line_idx);
    sketch.add_distance_constraint(line_idx, 5.0);

    Sketch::solve_options_t options;
    options.max_iterations = 128U;
    const auto unresolved = sketch.solve_constraints(options);
    REQUIRE( unresolved != 0U );
    REQUIRE( sketch.last_solve_report().unresolved_constraints == unresolved );
#ifdef DCMA_USE_EIGEN
    REQUIRE( sketch.last_solve_report().used_svd );
    REQUIRE( sketch.last_solve_report().conflicting_constraints );
    REQUIRE( sketch.last_solve_report().jacobian_rank <= sketch.last_solve_report().residual_count );
#endif // DCMA_USE_EIGEN
}

TEST_CASE("Sketch copy preserves the last solve report"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(2.0, 1.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    sketch.add_horizontal_constraint(line_idx);

    Sketch::solve_options_t options;
    options.max_iterations = 32U;
    REQUIRE( sketch.solve_constraints(options) == 0U );

    Sketch copied = sketch;
    REQUIRE( copied.last_solve_report().unresolved_constraints == sketch.last_solve_report().unresolved_constraints );
    REQUIRE( copied.last_solve_report().iterations == sketch.last_solve_report().iterations );
    REQUIRE( copied.last_solve_report().reason == sketch.last_solve_report().reason );
}

TEST_CASE("Sketch solve report resets when no constraints are present"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(2.0, 1.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    sketch.add_horizontal_constraint(line_idx);

    Sketch::solve_options_t options;
    options.max_iterations = 32U;
    REQUIRE( sketch.solve_constraints(options) == 0U );
    REQUIRE( !sketch.last_solve_report().reason.empty() );

    sketch.clear();
    sketch.set_plane(default_xy_plane());
    REQUIRE( sketch.solve_constraints(options) == 0U );
    REQUIRE( sketch.last_solve_report().unresolved_constraints == 0U );
    REQUIRE( sketch.last_solve_report().enabled_constraints == 0U );
    REQUIRE( sketch.last_solve_report().residual_count == 0U );
    REQUIRE( sketch.last_solve_report().reason.empty() );
    REQUIRE( std::isnan(sketch.last_solve_report().cost) );
}

TEST_CASE("Sketch snapshots preserve state"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_bezier({ vec3<double>(0.0, 0.0, 0.0),
                        vec3<double>(1.0, 2.0, 0.0),
                        vec3<double>(2.0, 2.0, 0.0),
                        vec3<double>(3.0, 0.0, 0.0) },
                      Sketch::geometry_tag_t::support);

    Sketch snapshot = sketch;
    sketch.translate_vertices(std::set<Sketch::vertex_index_t>{0U, 1U, 2U, 3U}, vec3<double>(5.0, 0.0, 0.0));

    REQUIRE( snapshot.vertex_count() == sketch.vertex_count() );
    REQUIRE( snapshot.vertex(0U).x == doctest::Approx(0.0) );
    REQUIRE( sketch.vertex(0U).x == doctest::Approx(5.0) );
}

TEST_CASE("Sketch set_plane refreshes derived arc geometry"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto arc_idx = sketch.add_arc(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(1.0, 0.0, 0.0),
                                        vec3<double>(0.0, 0.0, 1.0),
                                        Sketch::geometry_tag_t::normal);

    sketch.set_plane(default_xz_plane());

    const auto samples = sketch.sample_primitive(arc_idx, 8U);
    REQUIRE( !samples.empty() );
    REQUIRE( samples.front().x == doctest::Approx(1.0).epsilon(1E-6) );
    REQUIRE( samples.front().z == doctest::Approx(0.0).epsilon(1E-6) );
    REQUIRE( samples.back().x == doctest::Approx(0.0).epsilon(1E-6) );
    REQUIRE( samples.back().z == doctest::Approx(1.0).epsilon(1E-6) );
}

TEST_CASE("Sketch arcs keep endpoints on the circle"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto arc_idx = sketch.add_arc(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(1.0, 0.0, 0.0),
                                        vec3<double>(2.0, 2.0, 0.0),
                                        Sketch::geometry_tag_t::normal);

    const auto *arc = dynamic_cast<const Sketch::arc_primitive_t*>(sketch.primitive(arc_idx));
    REQUIRE( arc != nullptr );
    REQUIRE( sketch.vertex(arc->start).distance(sketch.vertex(arc->center)) == doctest::Approx(1.0).epsilon(1E-6) );
    REQUIRE( sketch.vertex(arc->stop).distance(sketch.vertex(arc->center)) == doctest::Approx(1.0).epsilon(1E-6) );

    sketch.set_vertex(arc->stop, vec3<double>(3.0, 0.5, 0.0));
    REQUIRE( sketch.vertex(arc->stop).distance(sketch.vertex(arc->center)) == doctest::Approx(1.0).epsilon(1E-6) );

    const auto samples = sketch.sample_primitive(arc_idx, 24U);
    REQUIRE( !samples.empty() );
    REQUIRE( samples.back().distance(sketch.vertex(arc->stop)) <= 1.0E-6 );
}

TEST_CASE("Sketch refresh updates cached geometry after shared arc endpoint snapping"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto circle_idx = sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                                              vec3<double>(1.0, 0.0, 0.0),
                                              Sketch::geometry_tag_t::normal);
    const auto arc_idx = sketch.add_arc(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(1.0, 0.0, 0.0),
                                        vec3<double>(0.0, 1.0, 0.0),
                                        Sketch::geometry_tag_t::normal);

    auto *circle = dynamic_cast<Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    auto *arc = dynamic_cast<Sketch::arc_primitive_t*>(sketch.primitive(arc_idx));
    REQUIRE( circle != nullptr );
    REQUIRE( arc != nullptr );

    circle->center = arc->center;
    circle->radius_point = arc->stop;
    sketch.refresh_geometry();

    sketch.set_vertex(arc->stop, vec3<double>(3.0, 4.0, 0.0));

    const auto *updated_circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    REQUIRE( updated_circle != nullptr );
    REQUIRE( updated_circle->radius == doctest::Approx(1.0).epsilon(1E-6) );
    REQUIRE( sketch.vertex(arc->stop).distance(sketch.vertex(arc->center)) == doctest::Approx(1.0).epsilon(1E-6) );
}

TEST_CASE("Sketch deleting a vertex removes dependent primitives and constraints"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(2.0, 0.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    sketch.add_horizontal_constraint(line_idx);

    REQUIRE( sketch.vertex_count() == 2U );
    REQUIRE( sketch.primitive_count() == 1U );
    REQUIRE( sketch.constraint_count() == 1U );

    REQUIRE( sketch.delete_vertex(0U) );
    REQUIRE( sketch.vertex_count() == 1U );
    REQUIRE( sketch.primitive_count() == 0U );
    REQUIRE( sketch.constraint_count() == 0U );
}

TEST_CASE("Sketch deleting a vertex remaps surviving overlap constraints"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                    vec3<double>(2.0, 0.0, 0.0),
                    Sketch::geometry_tag_t::normal);
    const auto overlap_a = sketch.append_vertex(vec3<double>(5.0, 0.0, 0.0));
    const auto overlap_b = sketch.append_vertex(vec3<double>(6.0, 1.0, 0.0));
    sketch.add_overlap_constraint(overlap_a, overlap_b);

    REQUIRE( sketch.delete_vertex(0U) );
    REQUIRE( sketch.vertex_count() == 3U );
    REQUIRE( sketch.constraint_count() == 1U );

    const auto *overlap = dynamic_cast<const Sketch::overlap_constraint_t*>(sketch.constraint(0U));
    REQUIRE( overlap != nullptr );
    REQUIRE( overlap->vertex_a == 1U );
    REQUIRE( overlap->vertex_b == 2U );
    REQUIRE( sketch.vertex(overlap->vertex_a).x == doctest::Approx(5.0).epsilon(1E-6) );
    REQUIRE( sketch.vertex(overlap->vertex_a).y == doctest::Approx(0.0).epsilon(1E-6) );
    REQUIRE( sketch.vertex(overlap->vertex_b).x == doctest::Approx(6.0).epsilon(1E-6) );
    REQUIRE( sketch.vertex(overlap->vertex_b).y == doctest::Approx(1.0).epsilon(1E-6) );
}

TEST_CASE("Sketch tracks nominal degrees of freedom"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(2.0, 1.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    sketch.add_horizontal_constraint(line_idx);
    const auto disabled_idx = sketch.add_distance_constraint(line_idx, 2.0);
    sketch.constraint(disabled_idx)->enabled = false;

    const auto dof = sketch.summarize_degrees_of_freedom();
    REQUIRE( dof.total == 4U );
    REQUIRE( dof.enabled_constraints == 1U );
    REQUIRE( dof.disabled_constraints == 1U );
    REQUIRE( dof.constrained == 1U );
    REQUIRE( dof.remaining == 3U );
    REQUIRE( dof.overconstrained == 0U );
}

TEST_CASE("Sketch can delete unreferenced vertices without corrupting indices"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                    vec3<double>(1.0, 0.0, 0.0),
                    Sketch::geometry_tag_t::normal);
    sketch.append_vertex(vec3<double>(5.0, 5.0, 0.0));
    const auto second_line_idx = sketch.add_line(vec3<double>(2.0, 0.0, 0.0),
                                                 vec3<double>(3.0, 0.0, 0.0),
                                                 Sketch::geometry_tag_t::support);
    sketch.add_horizontal_constraint(second_line_idx);

    REQUIRE( sketch.vertex_count() == 5U );
    REQUIRE( sketch.delete_unreferenced_vertices() == 1U );
    REQUIRE( sketch.vertex_count() == 4U );
    REQUIRE( sketch.constraint_count() == 1U );

    const auto *second_line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(second_line_idx));
    REQUIRE( second_line != nullptr );
    REQUIRE( second_line->vertices[0] == 2U );
    REQUIRE( second_line->vertices[1] == 3U );
    const auto *constraint = dynamic_cast<const Sketch::horizontal_constraint_t*>(sketch.constraint(0U));
    REQUIRE( constraint != nullptr );
    REQUIRE( constraint->line == second_line_idx );
}

TEST_CASE("Sketch supports shared-vertex construction and primitive vertex hit testing"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto shared = sketch.append_vertex(vec3<double>(1.0, 1.0, 0.0));
    const auto other_a = sketch.append_vertex(vec3<double>(2.0, 1.0, 0.0));
    const auto other_b = sketch.append_vertex(vec3<double>(1.0, 2.0, 0.0));
    const auto line_a = sketch.add_line(shared, other_a, Sketch::geometry_tag_t::normal);
    const auto line_b = sketch.add_line(shared, other_b, Sketch::geometry_tag_t::support);

    REQUIRE( sketch.vertex_count() == 3U );
    REQUIRE( sketch.primitive_count() == 2U );
    REQUIRE( sketch.primitives_referencing_vertex(shared).size() == 2U );

    const auto hit = sketch.nearest_primitive_vertex(vec3<double>(1.02, 1.01, 0.0), 0.1);
    REQUIRE( hit.has_value() );
    REQUIRE( hit->second == shared );
    REQUIRE( (hit->first == line_a || hit->first == line_b) );
}

TEST_CASE("Sketch can insert and collapse vertices"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_idx = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                          vec3<double>(4.0, 0.0, 0.0),
                                          Sketch::geometry_tag_t::normal);

    const auto inserted = sketch.insert_vertex(line_idx, vec3<double>(2.0, 0.0, 0.0));
    REQUIRE( inserted.has_value() );
    REQUIRE( sketch.vertex_count() == 3U );
    REQUIRE( sketch.primitive_count() == 2U );

    const auto line_refs = sketch.primitives_referencing_vertex(inserted.value());
    REQUIRE( line_refs.size() == 2U );
    REQUIRE( sketch.collapse_vertices(0U, inserted.value()) );
    REQUIRE( sketch.primitive_count() == 2U );
    const auto remaining_refs = sketch.primitives_referencing_vertex(0U);
    REQUIRE( remaining_refs.size() == 2U );
    for(const auto primitive_idx : remaining_refs){
        const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(primitive_idx));
        REQUIRE( line != nullptr );
        REQUIRE( (line->vertices[0] == 0U || line->vertices[1] == 0U) );
    }
    REQUIRE( sketch.vertex_count() == 3U );
}

TEST_CASE("Sketch solves perpendicular and overlap constraints"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_a = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(4.0, 0.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto line_b = sketch.add_line(vec3<double>(1.0, 1.0, 0.0),
                                        vec3<double>(3.0, 2.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto vertex_a = sketch.append_vertex(vec3<double>(9.0, 9.0, 0.0));
    const auto vertex_b = sketch.append_vertex(vec3<double>(10.0, 11.0, 0.0));

    sketch.add_perpendicular_constraint(line_a, line_b);
    sketch.add_overlap_constraint(vertex_a, vertex_b);

    REQUIRE( sketch.solve_constraints() == 0U );

    const auto *a = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_a));
    const auto *b = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_b));
    REQUIRE( a != nullptr );
    REQUIRE( b != nullptr );

    const auto dir_a = sketch.vertex(a->vertices[1]) - sketch.vertex(a->vertices[0]);
    const auto dir_b = sketch.vertex(b->vertices[1]) - sketch.vertex(b->vertices[0]);
    REQUIRE( doctest::Approx(dir_a.Dot(dir_b)).epsilon(1E-6) == 0.0 );
    REQUIRE( sketch.vertex(vertex_a).distance(sketch.vertex(vertex_b)) == doctest::Approx(0.0).epsilon(1E-9) );
}

TEST_CASE("Sketch can delete primitives and constraints directly"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_a = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(1.0, 0.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto line_b = sketch.add_line(vec3<double>(0.0, 1.0, 0.0),
                                        vec3<double>(1.0, 1.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    sketch.add_parallel_constraint(line_a, line_b);
    sketch.add_perpendicular_constraint(line_a, line_b);

    REQUIRE( sketch.constraint_count() == 2U );
    REQUIRE( sketch.delete_constraint(0U) );
    REQUIRE( sketch.constraint_count() == 1U );
    REQUIRE( sketch.delete_primitive(line_a) );
    REQUIRE( sketch.primitive_count() == 1U );
    REQUIRE( sketch.constraint_count() == 0U );
}

TEST_CASE("Sketch primitive deletion preserves vertex-only constraints"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_a = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(1.0, 0.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto line_b = sketch.add_line(vec3<double>(0.0, 1.0, 0.0),
                                        vec3<double>(1.0, 1.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto vertex_a = sketch.append_vertex(vec3<double>(5.0, 0.0, 0.0));
    const auto vertex_b = sketch.append_vertex(vec3<double>(6.0, 0.0, 0.0));
    sketch.add_overlap_constraint(vertex_a, vertex_b);

    REQUIRE( sketch.delete_primitive(line_a) );
    REQUIRE( sketch.primitive_count() == 1U );
    REQUIRE( sketch.constraint_count() == 1U );
    REQUIRE( sketch.describe_constraint(0U) == "overlap" );
    const auto *remaining_line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(0U));
    REQUIRE( remaining_line != nullptr );
    REQUIRE( remaining_line->vertices[0] == 2U );
    REQUIRE( remaining_line->vertices[1] == 3U );
}

TEST_CASE("Sketch solves pin and mirror constraints and tracks fully constrained geometry"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto mirror_line = sketch.add_line(vec3<double>(0.0, -2.0, 0.0),
                                             vec3<double>(0.0, 2.0, 0.0),
                                             Sketch::geometry_tag_t::support);
    const auto source_vertex = sketch.append_vertex(vec3<double>(2.0, 1.0, 0.0));
    const auto mirrored_vertex = sketch.append_vertex(vec3<double>(-5.0, 8.0, 0.0));
    const auto mirrored_segment = sketch.add_line(source_vertex,
                                                  mirrored_vertex,
                                                  Sketch::geometry_tag_t::normal);
    const auto *mirror_line_primitive = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(mirror_line));
    REQUIRE( mirror_line_primitive != nullptr );

    sketch.add_pin_constraint(mirror_line_primitive->vertices[0]);
    sketch.add_pin_constraint(mirror_line_primitive->vertices[1]);
    sketch.add_pin_constraint(source_vertex);
    sketch.add_mirror_constraint(mirror_line, source_vertex, mirrored_vertex);

    REQUIRE( sketch.solve_constraints() == 0U );
    REQUIRE( sketch.vertex(mirrored_vertex).x == doctest::Approx(-2.0).epsilon(1E-6) );
    REQUIRE( sketch.vertex(mirrored_vertex).y == doctest::Approx(1.0).epsilon(1E-6) );

    const auto constrained_vertices = sketch.fully_constrained_vertices();
    REQUIRE( constrained_vertices.count(source_vertex) == 1U );
    REQUIRE( constrained_vertices.count(mirrored_vertex) == 1U );
    const auto constrained_primitives = sketch.fully_constrained_primitives();
    REQUIRE( constrained_primitives.count(mirror_line) == 1U );
    REQUIRE( constrained_primitives.count(mirrored_segment) == 1U );
}

TEST_CASE("Sketch pin constraints are enforced before derived geometry is refreshed"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto circle_idx = sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                                              vec3<double>(2.0, 0.0, 0.0),
                                              Sketch::geometry_tag_t::normal);
    const auto *circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    REQUIRE( circle != nullptr );

    sketch.add_pin_constraint(circle->radius_point);
    sketch.translate_vertices(std::set<Sketch::vertex_index_t>{ circle->radius_point }, vec3<double>(3.0, 0.0, 0.0));

    const auto *updated_circle = dynamic_cast<const Sketch::circle_primitive_t*>(sketch.primitive(circle_idx));
    REQUIRE( updated_circle != nullptr );
    REQUIRE( sketch.vertex(circle->radius_point).x == doctest::Approx(2.0).epsilon(1E-6) );
    REQUIRE( updated_circle->radius == doctest::Approx(2.0).epsilon(1E-6) );
}

TEST_CASE("Sketch files round-trip through disk serialization"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_idx = sketch.add_line(vec3<double>(0.123456789012345, 0.0, 0.0),
                                          vec3<double>(2.123456789012345, 0.0, 0.0),
                                          Sketch::geometry_tag_t::support);
    sketch.add_distance_constraint(line_idx, 2.0);
    const auto pin_vertex = sketch.append_vertex(vec3<double>(3.0, 0.0, 0.0));
    const auto overlap_vertex = sketch.append_vertex(vec3<double>(3.0, 1.0, 0.0));
    const auto mirror_vertex = sketch.append_vertex(vec3<double>(4.0, 0.5, 0.0));
    sketch.add_pin_constraint(pin_vertex);
    sketch.add_overlap_constraint(pin_vertex, overlap_vertex);
    sketch.add_mirror_constraint(line_idx, pin_vertex, mirror_vertex);

    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / ("dcma_sketch_roundtrip_test_" + unique_suffix + ".dcmasketch");
    std::string error_message;
    REQUIRE( sketch.save_to_file(path, &error_message) );

    Sketch loaded;
    REQUIRE( Sketch::load_from_file(path, loaded, &error_message) );
    REQUIRE( loaded.has_plane() );
    REQUIRE( loaded.vertex_count() == sketch.vertex_count() );
    REQUIRE( loaded.primitive_count() == sketch.primitive_count() );
    REQUIRE( loaded.constraint_count() == sketch.constraint_count() );
    const auto *loaded_line = dynamic_cast<const Sketch::line_primitive_t*>(loaded.primitive(0U));
    REQUIRE( loaded_line != nullptr );
    REQUIRE( loaded_line->tag == Sketch::geometry_tag_t::support );
    REQUIRE( loaded.vertex(loaded_line->vertices[0]).x == doctest::Approx(0.123456789012345).epsilon(1E-15) );
    REQUIRE( loaded.vertex(loaded_line->vertices[1]).x == doctest::Approx(2.123456789012345).epsilon(1E-15) );
    REQUIRE( loaded.describe_constraint(1U) == "pin" );
    REQUIRE( loaded.describe_constraint(2U) == "overlap" );
    REQUIRE( loaded.describe_constraint(3U) == "mirror" );

    std::filesystem::remove(path);
}

TEST_CASE("Sketch parallel constraints preserve reversed line orientation"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_a = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(4.0, 0.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto line_b = sketch.add_line(vec3<double>(2.0, 2.0, 0.0),
                                        vec3<double>(-1.0, 1.0, 0.0),
                                        Sketch::geometry_tag_t::normal);

    sketch.add_parallel_constraint(line_a, line_b);

    REQUIRE( sketch.solve_constraints() == 0U );

    const auto *parallel_line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_b));
    REQUIRE( parallel_line != nullptr );
    const auto b0 = sketch.vertex(parallel_line->vertices[0]);
    const auto b1 = sketch.vertex(parallel_line->vertices[1]);
    REQUIRE( b1.x < b0.x );
    REQUIRE( b1.y == doctest::Approx(b0.y).epsilon(1E-6) );
}

TEST_CASE("Sketch parallel and perpendicular constraints reject degenerate lines"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto reference_line = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                                vec3<double>(4.0, 0.0, 0.0),
                                                Sketch::geometry_tag_t::normal);
    const auto degenerate_parallel = sketch.add_line(vec3<double>(1.0, 1.0, 0.0),
                                                     vec3<double>(1.0, 1.0, 0.0),
                                                     Sketch::geometry_tag_t::normal);
    const auto degenerate_perpendicular = sketch.add_line(vec3<double>(2.0, 2.0, 0.0),
                                                          vec3<double>(2.0, 2.0, 0.0),
                                                          Sketch::geometry_tag_t::normal);

    sketch.add_parallel_constraint(reference_line, degenerate_parallel);
    sketch.add_perpendicular_constraint(reference_line, degenerate_perpendicular);

    REQUIRE( sketch.solve_constraints() == 2U );
    REQUIRE( sketch.last_solve_report().unresolved_constraints == 2U );
}

TEST_CASE("Sketch perpendicular constraints preserve reversed line orientation"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    const auto line_a = sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                                        vec3<double>(4.0, 0.0, 0.0),
                                        Sketch::geometry_tag_t::normal);
    const auto line_b = sketch.add_line(vec3<double>(2.0, 2.0, 0.0),
                                        vec3<double>(2.5, -1.0, 0.0),
                                        Sketch::geometry_tag_t::normal);

    sketch.add_perpendicular_constraint(line_a, line_b);

    REQUIRE( sketch.solve_constraints() == 0U );

    const auto *perpendicular_line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_b));
    REQUIRE( perpendicular_line != nullptr );
    const auto b0 = sketch.vertex(perpendicular_line->vertices[0]);
    const auto b1 = sketch.vertex(perpendicular_line->vertices[1]);
    REQUIRE( b1.y < b0.y );
    REQUIRE( b1.x == doctest::Approx(b0.x).epsilon(1E-6) );
}

TEST_CASE("Sketch image-frame bounds clamp and solver keep vertices inside"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_idx = sketch.add_line(vec3<double>(-2.0, -1.0, 0.0),
                                          vec3<double>(12.0, 11.0, 0.0),
                                          Sketch::geometry_tag_t::normal);
    const auto *line = dynamic_cast<const Sketch::line_primitive_t*>(sketch.primitive(line_idx));
    REQUIRE( line != nullptr );

    Sketch::bounding_box_t bounds;
    bounds.min = { 0.0, 0.0 };
    bounds.max = { 10.0, 10.0 };

    REQUIRE( sketch.clamp_vertices_to_bounds(bounds) );
    for(const auto vertex_idx : line->referenced_vertices()){
        const auto projected = sketch.project(sketch.vertex(vertex_idx));
        CHECK( projected.u >= (bounds.min.u - 1.0E-6) );
        CHECK( projected.u <= (bounds.max.u + 1.0E-6) );
        CHECK( projected.v >= (bounds.min.v - 1.0E-6) );
        CHECK( projected.v <= (bounds.max.v + 1.0E-6) );
    }

    sketch.set_vertex(line->vertices[0], vec3<double>(-5.0, 5.0, 0.0));
    sketch.set_vertex(line->vertices[1], vec3<double>(15.0, 5.0, 0.0));
    Sketch::solve_options_t options;
    options.max_iterations = 64U;
    options.enable_sticky_constraints = false;
    options.constrain_to_bounds = true;
    options.bounds = bounds;
    REQUIRE( sketch.solve_constraints(options) == 0U );
    for(const auto vertex_idx : line->referenced_vertices()){
        const auto projected = sketch.project(sketch.vertex(vertex_idx));
        CHECK( projected.u >= (bounds.min.u - 1.0E-6) );
        CHECK( projected.u <= (bounds.max.u + 1.0E-6) );
        CHECK( projected.v >= (bounds.min.v - 1.0E-6) );
        CHECK( projected.v <= (bounds.max.v + 1.0E-6) );
    }
}

TEST_CASE("Sketch extrusion validates span direction"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_line(vec3<double>(0.0, 0.0, 0.0),
                    vec3<double>(5.0, 0.0, 0.0),
                    Sketch::geometry_tag_t::normal);

    Sketch::extrusion_options_t options;
    options.into_frame_length = 5.0;
    options.out_of_frame_length = -6.0;
    fv_surface_mesh<double, uint64_t> mesh;
    std::string error_message;
    REQUIRE_FALSE( sketch.build_extruded_surface_mesh(options, mesh, nullptr, &error_message) );
    REQUIRE_FALSE( error_message.empty() );
}

TEST_CASE("Sketch extrusion defaults use ten millimetre spans"){
    Sketch::extrusion_options_t options;
    CHECK( options.into_frame_length == doctest::Approx(10.0) );
    CHECK( options.out_of_frame_length == doctest::Approx(10.0) );
    CHECK( options.into_frame_angle_degrees == doctest::Approx(0.0) );
    CHECK( options.out_of_frame_angle_degrees == doctest::Approx(0.0) );
}

TEST_CASE("Sketch extrusion produces uniformly oriented capped meshes"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                      vec3<double>(2.0, 0.0, 0.0),
                      Sketch::geometry_tag_t::normal);

    Sketch::extrusion_options_t options;
    options.into_frame_length = 5.0;
    options.out_of_frame_length = 5.0;
    options.curve_segments = 24U;

    fv_surface_mesh<double, uint64_t> mesh;
    REQUIRE( sketch.build_extruded_surface_mesh(options, mesh) );
    REQUIRE( !mesh.vertices.empty() );
    REQUIRE( !mesh.faces.empty() );

    double min_z = std::numeric_limits<double>::infinity();
    double max_z = -std::numeric_limits<double>::infinity();
    bool saw_near_cap = false;
    bool saw_far_cap = false;
    for(const auto &vertex : mesh.vertices){
        min_z = std::min(min_z, vertex.z);
        max_z = std::max(max_z, vertex.z);
    }
    CHECK( min_z == doctest::Approx(-5.0).epsilon(1E-6) );
    CHECK( max_z == doctest::Approx(5.0).epsilon(1E-6) );

    for(const auto &face : mesh.faces){
        REQUIRE( face.size() == 3U );
        const auto &a = mesh.vertices.at(face.at(0));
        const auto &b = mesh.vertices.at(face.at(1));
        const auto &c = mesh.vertices.at(face.at(2));
        const auto normal = (b - a).Cross(c - a);
        const bool near_cap = (std::abs(a.z + 5.0) <= 1.0E-6)
                           && (std::abs(b.z + 5.0) <= 1.0E-6)
                           && (std::abs(c.z + 5.0) <= 1.0E-6);
        const bool far_cap = (std::abs(a.z - 5.0) <= 1.0E-6)
                          && (std::abs(b.z - 5.0) <= 1.0E-6)
                          && (std::abs(c.z - 5.0) <= 1.0E-6);
        if(near_cap){
            saw_near_cap = true;
            CHECK( normal.z < 0.0 );
        }
        if(far_cap){
            saw_far_cap = true;
            CHECK( normal.z > 0.0 );
        }
    }

    CHECK( saw_near_cap );
    CHECK( saw_far_cap );
}

TEST_CASE("Sketch extrusion supports taper angles and cap mesh export"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_circle(vec3<double>(0.0, 0.0, 0.0),
                      vec3<double>(2.0, 0.0, 0.0),
                      Sketch::geometry_tag_t::normal);

    Sketch::extrusion_options_t options;
    options.into_frame_length = 1.0;
    options.out_of_frame_length = 1.0;
    options.into_frame_angle_degrees = 45.0;
    options.out_of_frame_angle_degrees = -45.0;
    options.curve_segments = 24U;

    fv_surface_mesh<double, uint64_t> mesh;
    std::vector<fv_surface_mesh<double, uint64_t>> cap_meshes;
    REQUIRE( sketch.build_extruded_surface_mesh(options, mesh, &cap_meshes) );
    REQUIRE( cap_meshes.size() == 2U );

    double near_radius = 0.0;
    double far_radius = 0.0;
    for(const auto &vertex : cap_meshes.at(0).vertices){
        near_radius = std::max(near_radius, std::hypot(vertex.x, vertex.y));
        CHECK( vertex.z == doctest::Approx(-1.0).epsilon(1E-6) );
    }
    for(const auto &vertex : cap_meshes.at(1).vertices){
        far_radius = std::max(far_radius, std::hypot(vertex.x, vertex.y));
        CHECK( vertex.z == doctest::Approx(1.0).epsilon(1E-6) );
    }

    CHECK( near_radius == doctest::Approx(1.0).epsilon(1E-4) );
    CHECK( far_radius == doctest::Approx(3.0).epsilon(1E-4) );
}

TEST_CASE("Sketch extrusion stitches line loops and preserves holes in end caps"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());

    // Outer square loop built from separate line primitives.
    sketch.add_line(vec3<double>(-5.0, -5.0, 0.0), vec3<double>( 5.0, -5.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>( 5.0, -5.0, 0.0), vec3<double>( 5.0,  5.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>( 5.0,  5.0, 0.0), vec3<double>(-5.0,  5.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>(-5.0,  5.0, 0.0), vec3<double>(-5.0, -5.0, 0.0), Sketch::geometry_tag_t::normal);

    // Inner square loop should become a hole.
    sketch.add_line(vec3<double>(-2.0, -2.0, 0.0), vec3<double>( 2.0, -2.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>( 2.0, -2.0, 0.0), vec3<double>( 2.0,  2.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>( 2.0,  2.0, 0.0), vec3<double>(-2.0,  2.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_line(vec3<double>(-2.0,  2.0, 0.0), vec3<double>(-2.0, -2.0, 0.0), Sketch::geometry_tag_t::normal);

    Sketch::extrusion_options_t options;
    options.into_frame_length = 3.0;
    options.out_of_frame_length = 2.0;

    fv_surface_mesh<double, uint64_t> mesh;
    REQUIRE( sketch.build_extruded_surface_mesh(options, mesh) );
    REQUIRE( !mesh.faces.empty() );

    bool saw_cap_face = false;
    bool saw_annulus_face = false;
    for(const auto &face : mesh.faces){
        REQUIRE( face.size() == 3U );
        const auto &a = mesh.vertices.at(face.at(0));
        const auto &b = mesh.vertices.at(face.at(1));
        const auto &c = mesh.vertices.at(face.at(2));
        const bool is_cap = (std::abs(a.z + 2.0) <= 1.0E-6 && std::abs(b.z + 2.0) <= 1.0E-6 && std::abs(c.z + 2.0) <= 1.0E-6)
                         || (std::abs(a.z - 3.0) <= 1.0E-6 && std::abs(b.z - 3.0) <= 1.0E-6 && std::abs(c.z - 3.0) <= 1.0E-6);
        if(!is_cap) continue;
        saw_cap_face = true;

        const vec3<double> face_center = (a + b + c) / 3.0;
        const auto projected = sketch.project(face_center);
        const bool inside_outer = (std::abs(projected.u) < 5.0) && (std::abs(projected.v) < 5.0);
        const bool inside_hole = (std::abs(projected.u) < 2.0) && (std::abs(projected.v) < 2.0);
        CHECK( inside_outer );
        CHECK_FALSE( inside_hole );
        if((2.2 < std::abs(projected.u)) || (2.2 < std::abs(projected.v))){
            saw_annulus_face = true;
        }
    }

    CHECK( saw_cap_face );
    CHECK( saw_annulus_face );
}
