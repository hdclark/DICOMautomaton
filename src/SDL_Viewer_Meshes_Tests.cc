// SDL_Viewer_Meshes_Tests.cc.

#include "doctest20251212/doctest.h"

#include "Operations/SDL_Viewer_Meshes.h"

namespace {

fv_surface_mesh<double, uint64_t> make_unit_quad_mesh(){
    fv_surface_mesh<double, uint64_t> mesh;
    mesh.vertices = {
        vec3<double>(-0.5, -0.5, 0.0),
        vec3<double>( 0.5, -0.5, 0.0),
        vec3<double>( 0.5,  0.5, 0.0),
        vec3<double>(-0.5,  0.5, 0.0),
    };
    mesh.faces = {
        std::vector<uint64_t>{ 0U, 1U, 2U, 3U },
    };
    mesh.recreate_involved_face_index();
    return mesh;
}

fv_surface_mesh<double, uint64_t> make_overlapping_mesh(){
    fv_surface_mesh<double, uint64_t> mesh;
    mesh.vertices = {
        vec3<double>(-0.5, -0.5, -0.25),
        vec3<double>( 0.5, -0.5, -0.25),
        vec3<double>( 0.0,  0.5, -0.25),
        vec3<double>(-0.5, -0.5,  0.25),
        vec3<double>( 0.5, -0.5,  0.25),
        vec3<double>( 0.0,  0.5,  0.25),
    };
    mesh.faces = {
        std::vector<uint64_t>{ 0U, 1U, 2U },
        std::vector<uint64_t>{ 3U, 4U, 5U },
    };
    mesh.recreate_involved_face_index();
    return mesh;
}

} // namespace

TEST_CASE("SDL_Viewer_Meshes compute_hover_state identifies polygon faces"){
    const auto mesh = make_unit_quad_mesh();
    const auto mvp = num_array<float>().identity(4);
    const auto hover = SDL_Viewer_Meshes::compute_hover_state(mesh,
                                                              mesh.vertices,
                                                              mvp,
                                                              200,
                                                              120,
                                                              SDL_Viewer_Meshes::point_t{ 100.0, 60.0 },
                                                              SDL_Viewer_Meshes::hover_pick_options_t{});
    REQUIRE(hover.has_value());
    REQUIRE(hover->face_index.has_value());
    CHECK(hover->face_index.value() == 0U);
    CHECK(hover->face_vertices.size() == 4U);
    REQUIRE(hover->plane.has_value());
    CHECK(hover->plane->normal().unit().z == doctest::Approx(1.0));
    CHECK(hover->rectangle_visible);
}

TEST_CASE("SDL_Viewer_Meshes compute_hover_state prefers front-most face"){
    const auto mesh = make_overlapping_mesh();
    const auto mvp = num_array<float>().identity(4);
    const auto hover = SDL_Viewer_Meshes::compute_hover_state(mesh,
                                                              mesh.vertices,
                                                              mvp,
                                                              200,
                                                              200,
                                                              SDL_Viewer_Meshes::point_t{ 100.0, 100.0 },
                                                              SDL_Viewer_Meshes::hover_pick_options_t{});
    REQUIRE(hover.has_value());
    REQUIRE(hover->face_index.has_value());
    CHECK(hover->face_index.value() == 0U);
}
