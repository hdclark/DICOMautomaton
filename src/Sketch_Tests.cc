// Sketch_Tests.cc.

#include "doctest20251212/doctest.h"

#include <array>
#include <chrono>
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

TEST_CASE("Sketch files round-trip through disk serialization"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto line_idx = sketch.add_line(vec3<double>(0.123456789012345, 0.0, 0.0),
                                          vec3<double>(2.123456789012345, 0.0, 0.0),
                                          Sketch::geometry_tag_t::support);
    sketch.add_distance_constraint(line_idx, 2.0);

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

    std::filesystem::remove(path);
}
