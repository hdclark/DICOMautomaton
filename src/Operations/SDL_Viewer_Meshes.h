// SDL_Viewer_Meshes.h - A part of DICOMautomaton 2026. Written by hal clark.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "YgorMath.h"
#include "YgorMathQuaternions.h"

#include "../Sketch.h"
#include "../Surface_Meshes.h"

class SDL_Viewer_Meshes {
public:
    struct point_t {
        double x = 0.0;
        double y = 0.0;
    };

    struct mouse_state_t {
        bool hovered = false;
        bool active = false;
        point_t position_px = {};
        std::array<bool, 3> clicked = { false, false, false };
        std::array<bool, 3> dragging = { false, false, false };
        std::array<point_t, 3> drag_delta_px = {};
        double wheel_delta = 0.0;
    };

    struct display_options_t {
        bool render_wireframe = true;
        bool reverse_normals = false;
        bool use_lighting = true;
        bool use_opaque = false;
        bool use_smoothing = true;

        bool precess = true;
        double precess_rate = 1.0;

        double rot_y = 0.0;
        double rot_p = 0.0;
        double rot_r = 0.0;
        quaternion orientation = quaternion().identity();

        double zoom = 1.0;
        double cam_distort = 0.0;

        num_array<float> model = num_array<float>().identity(4);

        std::array<float, 4> colours = { 1.000f, 0.588f, 0.005f, 0.8f };
    };

    struct render_request_t {
        const fv_surface_mesh<double, uint64_t> *mesh = nullptr;
        int width_px = 0;
        int height_px = 0;
        unsigned int shader_program = 0U;
        mouse_state_t mouse = {};
        bool hover_faces_enabled = false;
        bool include_coplanar_geometry = false;
        double coplanar_eps = 1.0;
        std::array<float, 4> clear_colour = { 0.08f, 0.08f, 0.08f, 1.0f };
    };

    struct render_stats_t {
        int64_t n_vertices = 0;
        int64_t n_indices = 0;
        int64_t n_triangles = 0;
        int64_t n_euler = 0;
    };

    struct hover_state_t {
        std::optional<std::size_t> face_index;
        std::optional<Sketch::plane_frame_t> plane;
        std::vector<vec3<double>> face_vertices;
        std::vector<std::vector<vec3<double>>> coplanar_faces;
        std::array<vec3<double>, 4> rectangle_world = {};
        bool rectangle_visible = false;
    };

    struct hover_pick_options_t {
        bool include_coplanar_geometry = false;
        double coplanar_eps = 1.0;
        double rectangle_min_padding = 1.0;
        double rectangle_padding_scale = 0.15;
    };

    enum class standard_view_t {
        front,
        back,
        left,
        right,
        top,
        bottom,
        reset,
    };

    SDL_Viewer_Meshes();
    ~SDL_Viewer_Meshes();

    void invalidate_mesh_cache();
    void clear();

    bool render(const render_request_t &request,
                display_options_t &display_options);

    unsigned int texture_id() const;
    int texture_width() const;
    int texture_height() const;

    const render_stats_t& render_stats() const;
    const hover_state_t& hover_state() const;

    static num_array<float> make_orthographic_projection_matrix(float left_bound = -1.0f,
                                                                float right_bound = 1.0f,
                                                                float bottom_bound = -1.0f,
                                                                float top_bound = 1.0f,
                                                                float near_bound = -1.0f,
                                                                float far_bound = 1.0f);
    static num_array<float> make_camera_matrix(const vec3<double> &cam_pos,
                                               const vec3<double> &target_pos,
                                               const vec3<double> &up_unit);
    static num_array<float> extract_normal_matrix(const num_array<float> &mv);

    static void sync_orientation_from_euler(display_options_t &display_options);
    static void sync_euler_from_orientation(display_options_t &display_options);
    static void apply_standard_view(display_options_t &display_options,
                                    standard_view_t standard_view);

    static std::optional<hover_state_t> compute_hover_state(const fv_surface_mesh<double, uint64_t> &mesh,
                                                            const std::vector<vec3<double>> &render_vertices,
                                                            const num_array<float> &mvp,
                                                            int width_px,
                                                            int height_px,
                                                            const point_t &mouse_position_px,
                                                            const hover_pick_options_t &options);

private:
    struct impl_t;
    std::unique_ptr<impl_t> impl_;
};
