// SDL_Viewer_Meshes.h - A part of DICOMautomaton 2026. Written by hal clark.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <GL/glew.h>

#include "YgorMath.h"
#include "YgorMathQuaternions.h"

#include "../Sketch.h"
#include "../Surface_Meshes.h"

// Represents a buffer stored in GPU memory that is accessible by OpenGL.
struct opengl_mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint nbo = 0;
    GLuint ebo = 0;

    GLsizei N_indices = 0;
    GLsizei N_vertices = 0;
    GLsizei N_triangles = 0;
    int64_t N_euler = 0;

    opengl_mesh(const fv_surface_mesh<double, uint64_t> &meshes,
                bool reverse_normals = false);
    void draw(bool render_wireframe = false);
    ~opengl_mesh() noexcept;
};

class Mesh_Widget {
public:
    using mesh_t = fv_surface_mesh<double, uint64_t>;

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

    enum class standard_view_t {
        front,
        back,
        left,
        right,
        top,
        bottom,
    };

    struct viewport_t {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int framebuffer_width = 0;
        int framebuffer_height = 0;
    };

    struct input_state_t {
        bool mouse_inside = false;
        bool allow_navigation = false;
        bool allow_face_hover = false;
        bool collect_coplanar_faces = false;
        double coplanar_eps = 1.0;
        double mouse_x = 0.0;
        double mouse_y = 0.0;
        double mouse_wheel = 0.0;
        std::array<bool, 3> mouse_down = { false, false, false };
        std::array<bool, 3> mouse_clicked = { false, false, false };
    };

    struct matrices_t {
        num_array<float> proj = num_array<float>().identity(4);
        num_array<float> mv = num_array<float>().identity(4);
        num_array<float> mvp = num_array<float>().identity(4);
        num_array<float> norm = num_array<float>().identity(3);
    };

    struct hover_state_t {
        std::optional<std::size_t> face_index;
        std::optional<Sketch::plane_frame_t> plane;
        std::vector<vec3<double>> face_vertices;
        std::vector<std::vector<vec3<double>>> coplanar_faces;
        std::array<vec3<double>, 4> rectangle_world = {};
        bool rectangle_visible = false;
    };

    Mesh_Widget() = default;
    Mesh_Widget(const Mesh_Widget&) = delete;
    Mesh_Widget& operator=(const Mesh_Widget&) = delete;
    Mesh_Widget(Mesh_Widget&&) = delete;
    Mesh_Widget& operator=(Mesh_Widget&&) = delete;
    ~Mesh_Widget() noexcept;

    void clear_mesh();
    void reload_mesh(const mesh_t &mesh, bool reverse_normals = false);
    bool has_mesh() const;
    const opengl_mesh* gpu_mesh() const;
    const mesh_t* source_mesh() const;

    void render(GLuint shader_program,
                display_options_t &display_options,
                const viewport_t &viewport,
                const input_state_t &input_state);

    const hover_state_t& hovered_face() const;
    std::optional<std::size_t> hovered_face_index() const;

    static void sync_orientation_from_euler(display_options_t &display_options);
    static void sync_euler_from_orientation(display_options_t &display_options);
    static void set_standard_view(display_options_t &display_options,
                                  standard_view_t standard_view);
    static matrices_t compute_matrices(const display_options_t &display_options,
                                       const viewport_t &viewport);
    static hover_state_t compute_hover_state(const mesh_t &mesh,
                                             const matrices_t &matrices,
                                             const viewport_t &viewport,
                                             double mouse_x,
                                             double mouse_y,
                                             double coplanar_eps,
                                             bool collect_coplanar_faces);

private:
    struct drag_state_t {
        std::array<bool, 3> mouse_down = { false, false, false };
        double mouse_x = 0.0;
        double mouse_y = 0.0;
        bool has_position = false;
    };

    void reset_hover_state();
    void update_navigation(display_options_t &display_options,
                           const viewport_t &viewport,
                           const input_state_t &input_state);
    void ensure_overlay_buffers();
    void draw_hover_overlay(GLuint shader_program,
                            const display_options_t &display_options,
                            const matrices_t &matrices);

    const mesh_t *mesh_ptr_ = nullptr;
    std::unique_ptr<opengl_mesh> mesh_gpu_;
    hover_state_t hover_state_;
    drag_state_t drag_state_;

    GLuint overlay_vao_ = 0;
    GLuint overlay_vbo_ = 0;
    GLuint overlay_nbo_ = 0;
};
