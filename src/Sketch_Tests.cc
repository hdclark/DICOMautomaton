#include "doctest20251212/doctest.h"

#include <array>
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
