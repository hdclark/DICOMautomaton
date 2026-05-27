// Sketch_Mesh_Builder_Tests.cc.

#include "doctest20251212/doctest.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Sketch.h"
#include "Sketch_Mesh_Builder.h"

namespace {

static Sketch::plane_frame_t default_xy_plane(){
    Sketch::plane_frame_t out;
    out.origin = vec3<double>(0.0, 0.0, 0.0);
    out.row_unit = vec3<double>(1.0, 0.0, 0.0);
    out.col_unit = vec3<double>(0.0, 1.0, 0.0);
    return out;
}

static Sketch make_circle_sketch(double cx, double cy, double r){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_circle(vec3<double>(cx, cy, 0.0), vec3<double>(cx + r, cy, 0.0), Sketch::geometry_tag_t::normal);
    return sketch;
}

static Sketch make_rectangle_sketch(double cx, double cy, double hx, double hy){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    const auto a = vec3<double>(cx - hx, cy - hy, 0.0);
    const auto b = vec3<double>(cx + hx, cy - hy, 0.0);
    const auto c = vec3<double>(cx + hx, cy + hy, 0.0);
    const auto d = vec3<double>(cx - hx, cy + hy, 0.0);
    sketch.add_line(a, b, Sketch::geometry_tag_t::normal);
    sketch.add_line(b, c, Sketch::geometry_tag_t::normal);
    sketch.add_line(c, d, Sketch::geometry_tag_t::normal);
    sketch.add_line(d, a, Sketch::geometry_tag_t::normal);
    return sketch;
}

static Sketch make_sparse_circle_sketch(){
    std::stringstream ss;
    ss << "sketch_format_version 1\n"
       << "plane_origin 0 0 0\n"
       << "plane_row_unit 1 0 0\n"
       << "plane_col_unit 0 1 0\n"
       << "vertex 0 0 0 0\n"
       << "vertex 1 5 0 0\n"
       << "primitive circle 3 normal 0 1\n"
       << "sketch_end\n";

    Sketch sketch;
    if(!Sketch::read_from(ss, sketch)){
        throw std::runtime_error("Unable to parse sparse circle sketch test fixture");
    }
    return sketch;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Sketch_Procedure I/O round-trip.
// ---------------------------------------------------------------------------
TEST_CASE("Sketch_Procedure write/read round-trip"){
    Sketch_Procedure proc;
    proc.kind = sketch_procedure_kind_t::extrusion;
    proc.extrusion_options.into_frame_length = 7.5;
    proc.extrusion_options.out_of_frame_length = 3.25;
    proc.extrusion_options.into_frame_angle_degrees = 1.0;
    proc.extrusion_options.out_of_frame_angle_degrees = -2.0;
    proc.extrusion_options.curve_segments = 32U;
    proc.extrusion_options.max_discretization_error = 0.05;

    std::stringstream ss;
    REQUIRE(proc.write_to(ss));

    Sketch_Procedure loaded;
    REQUIRE(Sketch_Procedure::read_from(ss, loaded));
    CHECK(loaded.kind == sketch_procedure_kind_t::extrusion);
    CHECK(loaded.extrusion_options.into_frame_length == doctest::Approx(7.5));
    CHECK(loaded.extrusion_options.out_of_frame_length == doctest::Approx(3.25));
    CHECK(loaded.extrusion_options.into_frame_angle_degrees == doctest::Approx(1.0));
    CHECK(loaded.extrusion_options.out_of_frame_angle_degrees == doctest::Approx(-2.0));
    CHECK(loaded.extrusion_options.curve_segments == 32U);
    CHECK(loaded.extrusion_options.max_discretization_error == doctest::Approx(0.05));
}

TEST_CASE("Sketch_Procedure kind string conversion"){
    for(auto kind : { sketch_procedure_kind_t::clear,
                      sketch_procedure_kind_t::noop,
                      sketch_procedure_kind_t::extrusion,
                      sketch_procedure_kind_t::through_hole,
                      sketch_procedure_kind_t::extend,
                      sketch_procedure_kind_t::carve }){
        const auto s = sketch_procedure_kind_to_string(kind);
        sketch_procedure_kind_t out = sketch_procedure_kind_t::clear;
        REQUIRE(string_to_sketch_procedure_kind(s, out));
        CHECK(out == kind);
    }
    sketch_procedure_kind_t dummy;
    CHECK_FALSE(string_to_sketch_procedure_kind("invalid_procedure", dummy));
}

// ---------------------------------------------------------------------------
// Sketch_Mesh_Builder basic operations.
// ---------------------------------------------------------------------------
TEST_CASE("Sketch_Mesh_Builder default construction"){
    Sketch_Mesh_Builder builder;
    CHECK(builder.node_count() == 1U);
    CHECK(builder.active_node_index() == 0U);
}

TEST_CASE("Sketch_Mesh_Builder append and delete nodes"){
    Sketch_Mesh_Builder builder;
    builder.append_default_node();
    CHECK(builder.node_count() == 2U);
    CHECK(builder.active_node_index() == 1U);

    builder.append_default_node();
    CHECK(builder.node_count() == 3U);
    CHECK(builder.active_node_index() == 2U);

    // Delete the last node.
    builder.delete_leaf_node(2U);
    CHECK(builder.node_count() == 2U);
    CHECK(builder.active_node_index() <= 1U);

    // Delete all but root, then root.
    builder.delete_leaf_node(1U);
    CHECK(builder.node_count() == 1U);

    // Deleting the root resets to a single default node.
    builder.delete_leaf_node(0U);
    CHECK(builder.node_count() == 1U);
}

TEST_CASE("Sketch_Mesh_Builder set_active_node_index clamps"){
    Sketch_Mesh_Builder builder;
    builder.set_active_node_index(100U);
    CHECK(builder.active_node_index() == 0U);

    builder.append_default_node();
    builder.set_active_node_index(0U);
    CHECK(builder.active_node_index() == 0U);
}

TEST_CASE("Sketch_Mesh_Builder compute_all handles sparse primitive slots"){
    const auto sparse_sketch = make_sparse_circle_sketch();
    REQUIRE(sparse_sketch.primitive_count() == 4U);
    REQUIRE(sparse_sketch.primitive(0U) == nullptr);
    REQUIRE(sparse_sketch.primitive(3U) != nullptr);

    Sketch_Mesh_Builder builder;
    builder.node(0).sketch = sparse_sketch;
    builder.node(0).procedure.kind = sketch_procedure_kind_t::extrusion;
    builder.node(0).procedure.extrusion_options.into_frame_length = 1.0;
    builder.node(0).procedure.extrusion_options.out_of_frame_length = 1.0;

    builder.append_default_node();
    builder.node(1).sketch = sparse_sketch;
    builder.node(1).procedure.kind = sketch_procedure_kind_t::through_hole;
    builder.node(1).procedure.extrusion_options.into_frame_length = 1.0;
    builder.node(1).procedure.extrusion_options.out_of_frame_length = 1.0;

    std::string error_message;
    REQUIRE(builder.compute_all(&error_message));
    CHECK(builder.node(0).mesh.has_value());
    CHECK(builder.node(1).mesh.has_value());
}

TEST_CASE("Sketch_Mesh_Builder last_mesh helpers track most recent computed mesh"){
    Sketch_Mesh_Builder builder;
    CHECK(builder.last_mesh_node_index() == std::nullopt);
    CHECK(builder.last_mesh() == nullptr);

    builder.node(0).sketch = make_circle_sketch(0.0, 0.0, 10.0);
    builder.node(0).procedure.kind = sketch_procedure_kind_t::extrusion;
    builder.node(0).procedure.extrusion_options.into_frame_length = 1.0;
    builder.node(0).procedure.extrusion_options.out_of_frame_length = 1.0;
    std::string error_message;
    REQUIRE(builder.compute_node(0U, &error_message));
    REQUIRE(builder.last_mesh_node_index().has_value());
    CHECK(builder.last_mesh_node_index().value() == 0U);
    REQUIRE(builder.last_mesh() != nullptr);

    builder.append_default_node();
    CHECK(builder.last_mesh_node_index().value() == 0U);
    builder.node(1).procedure.kind = sketch_procedure_kind_t::noop;
    REQUIRE(builder.compute_node(1U, &error_message));
    REQUIRE(builder.last_mesh_node_index().has_value());
    CHECK(builder.last_mesh_node_index().value() == 1U);
    REQUIRE(builder.last_mesh() != nullptr);
    CHECK(builder.last_mesh()->vertices.size() == builder.node(1).mesh->vertices.size());
}

TEST_CASE("Sketch_Mesh_Builder extend handles boolean failures without throwing"){
    Sketch_Mesh_Builder builder;
    builder.node(0).sketch = make_rectangle_sketch(0.0, 0.0, 8.0, 5.0);
    builder.node(0).procedure.kind = sketch_procedure_kind_t::extrusion;
    builder.node(0).procedure.extrusion_options.into_frame_length = 2.0;
    builder.node(0).procedure.extrusion_options.out_of_frame_length = 2.0;

    builder.append_default_node();
    builder.node(1).sketch = make_rectangle_sketch(9.0, 0.0, 3.0, 2.5);
    builder.node(1).procedure.kind = sketch_procedure_kind_t::extend;
    builder.node(1).procedure.extrusion_options.into_frame_length = 2.0;
    builder.node(1).procedure.extrusion_options.out_of_frame_length = 2.0;

    std::string error_message;
    REQUIRE(builder.compute_all(&error_message));
    REQUIRE(builder.node(0).mesh.has_value());
    REQUIRE(builder.node(1).mesh.has_value());
    CHECK(builder.node(1).mesh->vertices.empty());
    CHECK(builder.node(1).mesh->faces.empty());
    CHECK(error_message.find("Boolean extend failed") != std::string::npos);
}

TEST_CASE("Sketch_Mesh_Builder carve without a parent yields an empty mesh"){
    Sketch_Mesh_Builder builder;
    builder.node(0).sketch = make_circle_sketch(0.0, 0.0, 5.0);
    builder.node(0).procedure.kind = sketch_procedure_kind_t::carve;
    builder.node(0).procedure.extrusion_options.into_frame_length = 1.5;
    builder.node(0).procedure.extrusion_options.out_of_frame_length = 1.5;

    std::string error_message;
    REQUIRE(builder.compute_node(0U, &error_message));
    REQUIRE(builder.node(0).mesh.has_value());
    CHECK(builder.node(0).mesh->vertices.empty());
    CHECK(builder.node(0).mesh->faces.empty());
}

// ---------------------------------------------------------------------------
// Sketch_Mesh_Builder I/O round-trip.
// ---------------------------------------------------------------------------
TEST_CASE("Sketch_Mesh_Builder write/read round-trip with sketches"){
    Sketch_Mesh_Builder builder;
    builder.node(0).sketch = make_circle_sketch(0.0, 0.0, 10.0);
    builder.node(0).procedure.kind = sketch_procedure_kind_t::extrusion;
    builder.node(0).procedure.extrusion_options.into_frame_length = 5.0;

    builder.append_default_node();
    builder.node(1).sketch = make_circle_sketch(5.0, 5.0, 3.0);
    builder.node(1).procedure.kind = sketch_procedure_kind_t::through_hole;
    builder.set_active_node_index(1U);

    std::stringstream ss;
    REQUIRE(builder.write_to(ss));

    Sketch_Mesh_Builder loaded;
    REQUIRE(Sketch_Mesh_Builder::read_from(ss, loaded));
    CHECK(loaded.node_count() == 2U);
    CHECK(loaded.active_node_index() == 1U);
    CHECK(loaded.node(0).procedure.kind == sketch_procedure_kind_t::extrusion);
    CHECK(loaded.node(0).procedure.extrusion_options.into_frame_length == doctest::Approx(5.0));
    CHECK(loaded.node(1).procedure.kind == sketch_procedure_kind_t::through_hole);
    // Verify sketch data survived.
    CHECK(loaded.node(0).sketch.has_plane());
    CHECK(loaded.node(1).sketch.has_plane());
    CHECK(loaded.node(0).sketch.primitive_count() > 0U);
    CHECK(loaded.node(1).sketch.primitive_count() > 0U);
}

//TEST_CASE("Sketch_Mesh_Builder read_from with empty stream creates default"){
//    std::stringstream ss;
//    Sketch_Mesh_Builder loaded;
//    // An empty stream should still produce a valid builder with one default node.
//    Sketch_Mesh_Builder::read_from(ss, loaded);
//    CHECK(loaded.node_count() >= 1U);
//}

// ---------------------------------------------------------------------------
// Sketch stream I/O (write_to / read_from added to Sketch class).
// ---------------------------------------------------------------------------
TEST_CASE("Sketch stream write/read round-trip"){
    Sketch sketch;
    sketch.set_plane(default_xy_plane());
    sketch.add_line(vec3<double>(0.0, 0.0, 0.0), vec3<double>(10.0, 0.0, 0.0), Sketch::geometry_tag_t::normal);
    sketch.add_circle(vec3<double>(5.0, 5.0, 0.0), vec3<double>(8.0, 5.0, 0.0), Sketch::geometry_tag_t::normal);

    std::stringstream ss;
    REQUIRE(sketch.write_to(ss));

    Sketch loaded;
    REQUIRE(Sketch::read_from(ss, loaded));
    CHECK(loaded.has_plane());
    CHECK(loaded.primitive_count() == sketch.primitive_count());
    CHECK(loaded.vertex_count() == sketch.vertex_count());
}
