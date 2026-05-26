// SDL_Viewer_Meshes_Tests.cc.

#include "doctest20251212/doctest.h"

#include <cmath>

#include "SDL_Viewer_Meshes.h"

namespace {

Mesh_Widget::mesh_t make_unit_square_mesh(){
    Mesh_Widget::mesh_t mesh;
    mesh.vertices = {
        vec3<double>(-1.0, -1.0, 0.0),
        vec3<double>( 1.0, -1.0, 0.0),
        vec3<double>( 1.0,  1.0, 0.0),
        vec3<double>(-1.0,  1.0, 0.0),
    };
    mesh.faces = { { 0U, 1U, 2U, 3U } };
    return mesh;
}

} // namespace

TEST_CASE("Mesh_Widget standard views synchronize Euler angles"){
    Mesh_Widget::display_options_t display_options;
    Mesh_Widget::set_standard_view(display_options, Mesh_Widget::standard_view_t::top);
    CHECK(display_options.rot_y == doctest::Approx(0.0));
    CHECK(display_options.rot_p == doctest::Approx(90.0));
    CHECK(display_options.rot_r == doctest::Approx(0.0));

    Mesh_Widget::set_standard_view(display_options, Mesh_Widget::standard_view_t::left);
    CHECK(display_options.rot_y == doctest::Approx(90.0));
    CHECK(display_options.rot_p == doctest::Approx(0.0));
    CHECK(display_options.rot_r == doctest::Approx(0.0));
}

TEST_CASE("Mesh_Widget hover selection identifies visible face and overlay"){
    const auto mesh = make_unit_square_mesh();

    Mesh_Widget::display_options_t display_options;
    display_options.precess = false;
    display_options.zoom = 1.0;
    Mesh_Widget::set_standard_view(display_options, Mesh_Widget::standard_view_t::front);

    Mesh_Widget::viewport_t viewport;
    viewport.x = 10;
    viewport.y = 20;
    viewport.width = 200;
    viewport.height = 160;
    viewport.framebuffer_width = 320;
    viewport.framebuffer_height = 240;

    const auto matrices = Mesh_Widget::compute_matrices(display_options, viewport);
    const auto hover = Mesh_Widget::compute_hover_state(mesh,
                                                        matrices,
                                                        viewport,
                                                        viewport.x + viewport.width * 0.5,
                                                        viewport.y + viewport.height * 0.5,
                                                        0.01,
                                                        true);

    REQUIRE(hover.face_index.has_value());
    CHECK(hover.face_index.value() == 0U);
    REQUIRE(hover.plane.has_value());
    CHECK(hover.face_vertices.size() == 3U);
    CHECK(hover.rectangle_visible);
    CHECK(hover.coplanar_faces.size() == 1U);

    for(const auto &corner : hover.rectangle_world){
        CHECK(std::isfinite(corner.x));
        CHECK(std::isfinite(corner.y));
        CHECK(std::isfinite(corner.z));
    }
}
