// SDL_Viewer_Meshes.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "SDL_Viewer_Meshes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <GL/glew.h>

#include "YgorLog.h"
#include "YgorMisc.h"

#ifndef CHECK_FOR_GL_ERRORS
    #define CHECK_FOR_GL_ERRORS() { \
        while(true){ \
            GLenum err = glGetError(); \
            if(err == GL_NO_ERROR) break; \
            std::lock_guard<std::mutex> lock(ygor::g_term_sync); \
            std::cout << "--(W) In function: " << __PRETTY_FUNCTION__; \
            std::cout << " (line " << __LINE__ << ")"; \
            std::cout << " : " << glewGetErrorString(err); \
            std::cout << "(" << std::to_string(err) << ")." << std::endl; \
            std::cout.flush(); \
            throw std::runtime_error("OpenGL error detected. Refusing to continue"); \
        } \
    }
#endif

namespace {

struct mesh_cpu_cache_t {
    struct bounds_t {
        vec3<double> centre = vec3<double>(0.0, 0.0, 0.0);
        double uniform_scale = 1.0;
    } bounds;

    std::vector<vec3<float>> render_vertices_f;
    std::vector<vec3<double>> render_vertices_d;
    std::vector<vec3<float>> render_normals_f;
    std::vector<unsigned int> triangle_indices;
    SDL_Viewer_Meshes::render_stats_t stats;

    vec3<double> normalize_point(const vec3<double> &point) const {
        const auto scaled = (point - bounds.centre) * bounds.uniform_scale;
        return vec3<double>(scaled.x, scaled.y, scaled.z);
    }
};

mesh_cpu_cache_t build_mesh_cpu_cache(const fv_surface_mesh<double, uint64_t> &mesh,
                                      bool reverse_normals){
    mesh_cpu_cache_t out;

    out.stats.n_vertices = static_cast<int64_t>(mesh.vertices.size());
    out.stats.n_triangles = 0;
    for(const auto &face : mesh.faces){
        if(3U <= face.size()){
            out.stats.n_triangles += static_cast<int64_t>(face.size() - 2U);
        }
    }

    struct edge_pair_t {
        uint64_t a = 0U;
        uint64_t b = 0U;

        bool operator<(const edge_pair_t &other) const {
            return std::tie(a, b) < std::tie(other.a, other.b);
        }
    };
    std::set<edge_pair_t> unique_edges;
    for(const auto &face : mesh.faces){
        for(std::size_t i = 0U; i < face.size(); ++i){
            auto a = face.at(i);
            auto b = face.at((i + 1U) % face.size());
            if(a > b) std::swap(a, b);
            unique_edges.insert(edge_pair_t{ a, b });
        }
    }
    out.stats.n_euler = static_cast<int64_t>(mesh.vertices.size())
                      - static_cast<int64_t>(unique_edges.size())
                      + static_cast<int64_t>(mesh.faces.size());

    const auto inf = std::numeric_limits<double>::infinity();
    auto x_min = inf;
    auto y_min = inf;
    auto z_min = inf;
    auto x_max = -inf;
    auto y_max = -inf;
    auto z_max = -inf;
    for(const auto &vertex : mesh.vertices){
        x_min = std::min(x_min, vertex.x);
        y_min = std::min(y_min, vertex.y);
        z_min = std::min(z_min, vertex.z);
        x_max = std::max(x_max, vertex.x);
        y_max = std::max(y_max, vertex.y);
        z_max = std::max(z_max, vertex.z);
    }
    if(!std::isfinite(x_min) || !std::isfinite(y_min) || !std::isfinite(z_min)
    || !std::isfinite(x_max) || !std::isfinite(y_max) || !std::isfinite(z_max)){
        x_min = y_min = z_min = -1.0;
        x_max = y_max = z_max = 1.0;
    }

    const auto x_range = x_max - x_min;
    const auto y_range = y_max - y_min;
    const auto z_range = z_max - z_min;
    const auto max_range = std::max<double>({ x_range, y_range, z_range, 1.0 });
    out.bounds.centre = vec3<double>((x_min + x_max) * 0.5,
                                     (y_min + y_max) * 0.5,
                                     (z_min + z_max) * 0.5);
    out.bounds.uniform_scale = 2.0 / (std::sqrt(3.0) * max_range);

    out.render_vertices_f.reserve(mesh.vertices.size());
    out.render_vertices_d.reserve(mesh.vertices.size());
    for(const auto &vertex : mesh.vertices){
        const auto normalized = out.normalize_point(vertex);
        out.render_vertices_d.emplace_back(normalized);
        out.render_vertices_f.emplace_back(static_cast<float>(normalized.x),
                                           static_cast<float>(normalized.y),
                                           static_cast<float>(normalized.z));
    }

    const auto has_vertex_normals = (mesh.vertex_normals.size() == mesh.vertices.size());
    if(has_vertex_normals){
        out.render_normals_f.reserve(mesh.vertex_normals.size());
        for(const auto &normal : mesh.vertex_normals){
            const auto unit = normal.unit();
            out.render_normals_f.emplace_back(static_cast<float>(unit.x),
                                              static_cast<float>(unit.y),
                                              static_cast<float>(unit.z));
        }
    }else{
        out.render_normals_f.assign(mesh.vertices.size(), vec3<float>(0.0f, 0.0f, 0.0f));
    }

    out.triangle_indices.reserve(static_cast<std::size_t>(3 * out.stats.n_triangles));
    for(const auto &face : mesh.faces){
        if(face.size() < 3U) continue;

        const auto i0 = face.at(0U);
        for(std::size_t tri_idx = 2U; tri_idx < face.size(); ++tri_idx){
            const auto i1 = face.at(tri_idx - 1U);
            const auto i2 = face.at(tri_idx);

            const auto a = static_cast<unsigned int>(reverse_normals ? i2 : i0);
            const auto b = static_cast<unsigned int>(i1);
            const auto c = static_cast<unsigned int>(reverse_normals ? i0 : i2);
            out.triangle_indices.push_back(a);
            out.triangle_indices.push_back(b);
            out.triangle_indices.push_back(c);

            if(!has_vertex_normals){
                const auto awn = (mesh.vertices.at(c) - mesh.vertices.at(b)).Cross(mesh.vertices.at(a) - mesh.vertices.at(b));
                const auto weighted = vec3<float>(static_cast<float>(awn.x),
                                                  static_cast<float>(awn.y),
                                                  static_cast<float>(awn.z));
                out.render_normals_f.at(a) += weighted;
                out.render_normals_f.at(b) += weighted;
                out.render_normals_f.at(c) += weighted;
            }
        }
    }
    out.stats.n_indices = static_cast<int64_t>(out.triangle_indices.size());

    if(!has_vertex_normals){
        for(auto &normal : out.render_normals_f){
            normal = normal.unit();
        }
    }

    return out;
}

void upload_static_buffer(GLuint &buffer_id,
                          GLenum target,
                          GLsizeiptr size_bytes,
                          const void *data,
                          GLenum usage){
    if(buffer_id == 0U){
        glGenBuffers(1, &buffer_id);
        if(buffer_id == 0U){
            throw std::runtime_error("Unable to allocate OpenGL buffer");
        }
    }
    glBindBuffer(target, buffer_id);
    glBufferData(target, size_bytes, data, usage);
    CHECK_FOR_GL_ERRORS();
}

} // namespace

struct SDL_Viewer_Meshes::impl_t {
    GLuint mesh_vao = 0U;
    GLuint mesh_vbo = 0U;
    GLuint mesh_nbo = 0U;
    GLuint mesh_ebo = 0U;

    GLuint overlay_vao = 0U;
    GLuint overlay_vbo = 0U;
    GLuint overlay_nbo = 0U;

    GLuint framebuffer = 0U;
    GLuint colour_texture = 0U;
    GLuint depth_renderbuffer = 0U;
    int render_width = 0;
    int render_height = 0;

    const fv_surface_mesh<double, uint64_t> *mesh_ptr = nullptr;
    bool reverse_normals = false;
    mesh_cpu_cache_t cache;
    SDL_Viewer_Meshes::hover_state_t hover;
    std::array<vec3<float>, 4> overlay_render_vertices = {};
    std::array<vec3<float>, 4> overlay_render_normals = {};
    bool overlay_ready = false;

    void release_mesh_resources(){
        if(mesh_vao != 0U){
            glDeleteVertexArrays(1, &mesh_vao);
            mesh_vao = 0U;
        }
        if(mesh_vbo != 0U){
            glDeleteBuffers(1, &mesh_vbo);
            mesh_vbo = 0U;
        }
        if(mesh_nbo != 0U){
            glDeleteBuffers(1, &mesh_nbo);
            mesh_nbo = 0U;
        }
        if(mesh_ebo != 0U){
            glDeleteBuffers(1, &mesh_ebo);
            mesh_ebo = 0U;
        }
        if(overlay_vao != 0U){
            glDeleteVertexArrays(1, &overlay_vao);
            overlay_vao = 0U;
        }
        if(overlay_vbo != 0U){
            glDeleteBuffers(1, &overlay_vbo);
            overlay_vbo = 0U;
        }
        if(overlay_nbo != 0U){
            glDeleteBuffers(1, &overlay_nbo);
            overlay_nbo = 0U;
        }
        mesh_ptr = nullptr;
        reverse_normals = false;
        cache = mesh_cpu_cache_t{};
        hover = SDL_Viewer_Meshes::hover_state_t{};
        overlay_ready = false;
    }

    void release_render_target(){
        if(depth_renderbuffer != 0U){
            glDeleteRenderbuffers(1, &depth_renderbuffer);
            depth_renderbuffer = 0U;
        }
        if(colour_texture != 0U){
            glDeleteTextures(1, &colour_texture);
            colour_texture = 0U;
        }
        if(framebuffer != 0U){
            glDeleteFramebuffers(1, &framebuffer);
            framebuffer = 0U;
        }
        render_width = 0;
        render_height = 0;
    }

    void ensure_render_target(int width_px,
                              int height_px){
        width_px = std::max(width_px, 1);
        height_px = std::max(height_px, 1);
        if((framebuffer != 0U) && (render_width == width_px) && (render_height == height_px)){
            return;
        }

        release_render_target();

        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

        glGenTextures(1, &colour_texture);
        glBindTexture(GL_TEXTURE_2D, colour_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     width_px,
                     height_px,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               colour_texture,
                               0);

        glGenRenderbuffers(1, &depth_renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_px, height_px);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  depth_renderbuffer);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
            throw std::runtime_error("Unable to allocate off-screen framebuffer for SDL_Viewer_Meshes");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        render_width = width_px;
        render_height = height_px;
    }

    void load_mesh(const fv_surface_mesh<double, uint64_t> &mesh,
                   bool requested_reverse_normals){
        release_mesh_resources();

        cache = build_mesh_cpu_cache(mesh, requested_reverse_normals);
        mesh_ptr = &mesh;
        reverse_normals = requested_reverse_normals;

        upload_static_buffer(mesh_vbo,
                             GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(cache.render_vertices_f.size() * sizeof(vec3<float>)),
                             cache.render_vertices_f.data(),
                             GL_STATIC_DRAW);
        upload_static_buffer(mesh_nbo,
                             GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(cache.render_normals_f.size() * sizeof(vec3<float>)),
                             cache.render_normals_f.data(),
                             GL_STATIC_DRAW);
        upload_static_buffer(mesh_ebo,
                             GL_ELEMENT_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(cache.triangle_indices.size() * sizeof(unsigned int)),
                             cache.triangle_indices.data(),
                             GL_STATIC_DRAW);

        glGenVertexArrays(1, &mesh_vao);
        if(mesh_vao == 0U){
            throw std::runtime_error("Unable to allocate OpenGL vertex array object");
        }
        glBindVertexArray(mesh_vao);

        glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, mesh_nbo);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ebo);

        glBindVertexArray(0);
        CHECK_FOR_GL_ERRORS();

        glGenVertexArrays(1, &overlay_vao);
        glGenBuffers(1, &overlay_vbo);
        glGenBuffers(1, &overlay_nbo);
        if((overlay_vao == 0U) || (overlay_vbo == 0U) || (overlay_nbo == 0U)){
            throw std::runtime_error("Unable to allocate hovered-face overlay buffers");
        }
        glBindVertexArray(overlay_vao);
        glBindBuffer(GL_ARRAY_BUFFER, overlay_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(overlay_render_vertices.size() * sizeof(vec3<float>)),
                     overlay_render_vertices.data(),
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, overlay_nbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(overlay_render_normals.size() * sizeof(vec3<float>)),
                     overlay_render_normals.data(),
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        CHECK_FOR_GL_ERRORS();
    }
};

SDL_Viewer_Meshes::SDL_Viewer_Meshes()
    : impl_(std::make_unique<impl_t>()){
}

SDL_Viewer_Meshes::~SDL_Viewer_Meshes(){
    clear();
}

void SDL_Viewer_Meshes::invalidate_mesh_cache(){
    if(impl_){
        impl_->release_mesh_resources();
    }
}

void SDL_Viewer_Meshes::clear(){
    if(impl_){
        impl_->release_mesh_resources();
        impl_->release_render_target();
    }
}

unsigned int SDL_Viewer_Meshes::texture_id() const {
    return (impl_) ? impl_->colour_texture : 0U;
}

int SDL_Viewer_Meshes::texture_width() const {
    return (impl_) ? impl_->render_width : 0;
}

int SDL_Viewer_Meshes::texture_height() const {
    return (impl_) ? impl_->render_height : 0;
}

const SDL_Viewer_Meshes::render_stats_t& SDL_Viewer_Meshes::render_stats() const {
    static const render_stats_t empty = {};
    return (impl_) ? impl_->cache.stats : empty;
}

const SDL_Viewer_Meshes::hover_state_t& SDL_Viewer_Meshes::hover_state() const {
    static const hover_state_t empty = {};
    return (impl_) ? impl_->hover : empty;
}

num_array<float> SDL_Viewer_Meshes::make_orthographic_projection_matrix(float left_bound,
                                                                        float right_bound,
                                                                        float bottom_bound,
                                                                        float top_bound,
                                                                        float near_bound,
                                                                        float far_bound){
    num_array<float> proj(4, 4, 0.0f);
    proj.coeff(0,0) = 2.0f / (right_bound - left_bound);
    proj.coeff(1,1) = 2.0f / (top_bound - bottom_bound);
    proj.coeff(2,2) = 2.0f / (near_bound - far_bound);
    proj.coeff(0,3) = -(right_bound + left_bound) / (right_bound - left_bound);
    proj.coeff(1,3) = -(top_bound + bottom_bound) / (top_bound - bottom_bound);
    proj.coeff(2,3) = -(far_bound + near_bound) / (far_bound - near_bound);
    proj.coeff(3,3) = 1.0f;
    return proj.transpose();
}

num_array<float> SDL_Viewer_Meshes::make_camera_matrix(const vec3<double> &cam_pos,
                                                       const vec3<double> &target_pos,
                                                       const vec3<double> &up_unit){
    num_array<float> out(4, 4, 0.0f);

    const auto inward = (cam_pos - target_pos).unit();
    const auto leftward = up_unit.Cross(inward).unit();
    const auto upward = inward.Cross(leftward).unit();

    if(inward.isfinite() && leftward.isfinite() && upward.isfinite()){
        out.coeff(0,0) = leftward.x;
        out.coeff(1,0) = leftward.y;
        out.coeff(2,0) = leftward.z;

        out.coeff(0,1) = upward.x;
        out.coeff(1,1) = upward.y;
        out.coeff(2,1) = upward.z;

        out.coeff(0,2) = inward.x;
        out.coeff(1,2) = inward.y;
        out.coeff(2,2) = inward.z;

        out.coeff(0,3) = cam_pos.Dot(leftward);
        out.coeff(1,3) = cam_pos.Dot(upward);
        out.coeff(2,3) = cam_pos.Dot(inward);
        out.coeff(3,3) = 1.0f;
        out = out.transpose();
    }else{
        out = num_array<float>().identity(4);
    }
    return out;
}

num_array<float> SDL_Viewer_Meshes::extract_normal_matrix(const num_array<float> &mv){
    if((mv.num_rows() != 4) || (mv.num_cols() != 4)){
        throw std::logic_error("Expected 4x4 model-view matrix");
    }
    num_array<float> out(3, 3, 0.0f);
    for(int64_t r = 0; r < 3; ++r){
        for(int64_t c = 0; c < 3; ++c){
            out.coeff(r,c) = mv.read_coeff(r,c);
        }
    }
    return out;
}

void SDL_Viewer_Meshes::sync_orientation_from_euler(display_options_t &display_options){
    const auto pi = std::acos(-1.0);
    const auto deg_to_rad = pi / 180.0;
    display_options.orientation = quaternion().from_euler_ypr(display_options.rot_y * deg_to_rad,
                                                              display_options.rot_p * deg_to_rad,
                                                              display_options.rot_r * deg_to_rad);
}

void SDL_Viewer_Meshes::sync_euler_from_orientation(display_options_t &display_options){
    const auto pi = std::acos(-1.0);
    const auto rad_to_deg = 180.0 / pi;
    double y_rot = 0.0;
    double p_rot = 0.0;
    double r_rot = 0.0;
    display_options.orientation.to_euler_ypr(y_rot, p_rot, r_rot);
    display_options.rot_y = y_rot * rad_to_deg;
    display_options.rot_p = p_rot * rad_to_deg;
    display_options.rot_r = r_rot * rad_to_deg;
}

void SDL_Viewer_Meshes::apply_standard_view(display_options_t &display_options,
                                            standard_view_t standard_view){
    const auto pi = std::acos(-1.0);
    switch(standard_view){
        case standard_view_t::front:
            display_options.rot_y = 0.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            display_options.orientation = quaternion().from_euler_ypr(0.0, 0.0, 0.0);
            break;
        case standard_view_t::back:
            display_options.rot_y = 180.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            display_options.orientation = quaternion().from_euler_ypr(pi, 0.0, 0.0);
            break;
        case standard_view_t::left:
            display_options.rot_y = 90.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            display_options.orientation = quaternion().from_euler_ypr(0.5 * pi, 0.0, 0.0);
            break;
        case standard_view_t::right:
            display_options.rot_y = -90.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            display_options.orientation = quaternion().from_euler_ypr(-0.5 * pi, 0.0, 0.0);
            break;
        case standard_view_t::top:
            display_options.rot_y = 0.0;
            display_options.rot_p = 90.0;
            display_options.rot_r = 0.0;
            display_options.orientation = quaternion().from_euler_ypr(0.0, 0.5 * pi, 0.0);
            break;
        case standard_view_t::bottom:
            display_options.rot_y = 0.0;
            display_options.rot_p = -90.0;
            display_options.rot_r = 0.0;
            display_options.orientation = quaternion().from_euler_ypr(0.0, -0.5 * pi, 0.0);
            break;
        case standard_view_t::reset:
            display_options = display_options_t();
            break;
    }
}

std::optional<SDL_Viewer_Meshes::hover_state_t>
SDL_Viewer_Meshes::compute_hover_state(const fv_surface_mesh<double, uint64_t> &mesh,
                                       const std::vector<vec3<double>> &render_vertices,
                                       const num_array<float> &mvp,
                                       int width_px,
                                       int height_px,
                                       const point_t &mouse_position_px,
                                       const hover_pick_options_t &options){
    if((width_px <= 0) || (height_px <= 0) || mesh.faces.empty() || (mesh.vertices.size() != render_vertices.size())){
        return {};
    }

    const auto transform_point = [&](const vec3<double> &point) -> std::array<double, 4> {
        const std::array<double, 4> in = { point.x, point.y, point.z, 1.0 };
        std::array<double, 4> out = { 0.0, 0.0, 0.0, 0.0 };
        for(int r = 0; r < 4; ++r){
            for(int c = 0; c < 4; ++c){
                out.at(r) += static_cast<double>(mvp.read_coeff(r, c)) * in.at(c);
            }
        }
        return out;
    };
    const auto to_screen = [&](const vec3<double> &point) -> std::optional<point_t> {
        const auto clip = transform_point(point);
        if(std::abs(clip.at(3)) <= std::numeric_limits<double>::epsilon()){
            return {};
        }
        const auto ndc_x = clip.at(0) / clip.at(3);
        const auto ndc_y = clip.at(1) / clip.at(3);
        if(!std::isfinite(ndc_x) || !std::isfinite(ndc_y)){
            return {};
        }
        return point_t{
            (0.5 * (ndc_x + 1.0)) * static_cast<double>(width_px),
            (0.5 * (1.0 - ndc_y)) * static_cast<double>(height_px)
        };
    };
    const auto point_in_triangle_2d = [](const point_t &p,
                                         const point_t &a,
                                         const point_t &b,
                                         const point_t &c) -> bool {
        const auto sign = [](const point_t &p1, const point_t &p2, const point_t &p3) -> double {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };
        const auto d1 = sign(p, a, b);
        const auto d2 = sign(p, b, c);
        const auto d3 = sign(p, c, a);
        const bool has_neg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
        const bool has_pos = (0.0 < d1) || (0.0 < d2) || (0.0 < d3);
        return !(has_neg && has_pos);
    };

    std::optional<std::size_t> hovered_face_index;
    double hovered_depth = std::numeric_limits<double>::infinity();
    for(std::size_t face_idx = 0U; face_idx < mesh.faces.size(); ++face_idx){
        const auto &face = mesh.faces.at(face_idx);
        if(face.size() < 3U) continue;
        const auto &face_a = render_vertices.at(face.at(0U));
        for(std::size_t tri_idx = 2U; tri_idx < face.size(); ++tri_idx){
            const auto &face_b = render_vertices.at(face.at(tri_idx - 1U));
            const auto &face_c = render_vertices.at(face.at(tri_idx));
            const auto screen_a = to_screen(face_a);
            const auto screen_b = to_screen(face_b);
            const auto screen_c = to_screen(face_c);
            if(!screen_a || !screen_b || !screen_c) continue;
            if(!point_in_triangle_2d(mouse_position_px, screen_a.value(), screen_b.value(), screen_c.value())){
                continue;
            }
            const auto clip_a = transform_point(face_a);
            const auto clip_b = transform_point(face_b);
            const auto clip_c = transform_point(face_c);
            const auto depth = (clip_a.at(2) / clip_a.at(3)
                              + clip_b.at(2) / clip_b.at(3)
                              + clip_c.at(2) / clip_c.at(3)) / 3.0;
            if(depth < hovered_depth){
                hovered_depth = depth;
                hovered_face_index = face_idx;
            }
        }
    }

    if(!hovered_face_index){
        return {};
    }

    hover_state_t out;
    out.face_index = hovered_face_index;
    const auto &face = mesh.faces.at(hovered_face_index.value());
    out.face_vertices.reserve(face.size());
    for(const auto vertex_idx : face){
        out.face_vertices.emplace_back(mesh.vertices.at(vertex_idx));
    }
    if(out.face_vertices.size() < 3U){
        return {};
    }

    vec3<double> centroid(0.0, 0.0, 0.0);
    for(const auto &vertex : out.face_vertices){
        centroid += vertex;
    }
    centroid /= static_cast<double>(out.face_vertices.size());
    const auto normal = (out.face_vertices.at(1U) - out.face_vertices.at(0U))
                      .Cross(out.face_vertices.at(2U) - out.face_vertices.at(0U))
                      .unit();
    if(!normal.isfinite()){
        return {};
    }

    out.plane = Sketch::plane_frame_t::from_plane(plane<double>(normal, centroid),
                                                  out.face_vertices.at(1U) - out.face_vertices.at(0U));

    auto face_min = Sketch::projection_t{  std::numeric_limits<double>::infinity(),  std::numeric_limits<double>::infinity() };
    auto face_max = Sketch::projection_t{ -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
    for(const auto &vertex : out.face_vertices){
        const auto projected = Sketch::projection_t{
            out.plane->row_unit.Dot(vertex - out.plane->origin),
            out.plane->col_unit.Dot(vertex - out.plane->origin)
        };
        face_min.u = std::min(face_min.u, projected.u);
        face_min.v = std::min(face_min.v, projected.v);
        face_max.u = std::max(face_max.u, projected.u);
        face_max.v = std::max(face_max.v, projected.v);
    }

    const auto pad_u = std::max(options.rectangle_min_padding, (face_max.u - face_min.u) * options.rectangle_padding_scale);
    const auto pad_v = std::max(options.rectangle_min_padding, (face_max.v - face_min.v) * options.rectangle_padding_scale);
    face_min.u -= pad_u;
    face_min.v -= pad_v;
    face_max.u += pad_u;
    face_max.v += pad_v;

    out.rectangle_world = std::array<vec3<double>, 4>{
        out.plane->origin + out.plane->row_unit * face_min.u + out.plane->col_unit * face_min.v,
        out.plane->origin + out.plane->row_unit * face_max.u + out.plane->col_unit * face_min.v,
        out.plane->origin + out.plane->row_unit * face_max.u + out.plane->col_unit * face_max.v,
        out.plane->origin + out.plane->row_unit * face_min.u + out.plane->col_unit * face_max.v,
    };
    out.rectangle_visible = true;

    if(options.include_coplanar_geometry){
        const auto hovered_plane = out.plane->to_plane();
        for(std::size_t face_idx = 0U; face_idx < mesh.faces.size(); ++face_idx){
            if(face_idx == hovered_face_index.value()) continue;
            const auto &candidate_face = mesh.faces.at(face_idx);
            if(candidate_face.size() < 3U) continue;
            std::vector<vec3<double>> candidate_loop;
            candidate_loop.reserve(candidate_face.size());
            bool candidate_valid = true;
            for(const auto vertex_idx : candidate_face){
                const auto &vertex = mesh.vertices.at(vertex_idx);
                const auto projected = Sketch::projection_t{
                    out.plane->row_unit.Dot(vertex - out.plane->origin),
                    out.plane->col_unit.Dot(vertex - out.plane->origin)
                };
                const bool is_coplanar = (std::abs(hovered_plane.Get_Signed_Distance_To_Point(vertex)) <= options.coplanar_eps);
                const bool in_bounds = (face_min.u <= projected.u) && (projected.u <= face_max.u)
                                    && (face_min.v <= projected.v) && (projected.v <= face_max.v);
                if(!is_coplanar || !in_bounds){
                    candidate_valid = false;
                    break;
                }
                candidate_loop.emplace_back(vertex);
            }
            if(candidate_valid){
                out.coplanar_faces.emplace_back(std::move(candidate_loop));
            }
        }
    }

    return out;
}

bool SDL_Viewer_Meshes::render(const render_request_t &request,
                               display_options_t &display_options){
    if(!impl_) return false;

    const auto width_px = std::max(request.width_px, 1);
    const auto height_px = std::max(request.height_px, 1);
    impl_->ensure_render_target(width_px, height_px);

    GLint prior_framebuffer = 0;
    GLint prior_viewport[4] = { 0, 0, 0, 0 };
    GLint prior_program = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prior_framebuffer);
    glGetIntegerv(GL_VIEWPORT, prior_viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prior_program);

    glBindFramebuffer(GL_FRAMEBUFFER, impl_->framebuffer);
    glViewport(0, 0, width_px, height_px);
    glClearColor(request.clear_colour.at(0),
                 request.clear_colour.at(1),
                 request.clear_colour.at(2),
                 request.clear_colour.at(3));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if((request.mesh == nullptr) || (request.shader_program == 0U)){
        impl_->hover = hover_state_t{};
        impl_->overlay_ready = false;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prior_framebuffer));
        glViewport(prior_viewport[0], prior_viewport[1], prior_viewport[2], prior_viewport[3]);
        glUseProgram(static_cast<GLuint>(prior_program));
        return false;
    }

    if((impl_->mesh_ptr != request.mesh) || (impl_->reverse_normals != display_options.reverse_normals)){
        impl_->load_mesh(*request.mesh, display_options.reverse_normals);
    }

    const auto pi = std::acos(-1.0);
    const auto kDegToRad = pi / 180.0;
    const auto kCameraForward = vec3<double>(0.0, 0.0, 1.0);
    const auto kCameraUp = vec3<double>(0.0, 1.0, 0.0);
    const auto kCameraPitchAxis = vec3<double>(1.0, 0.0, 0.0);
    const auto kCameraYawAxis = vec3<double>(0.0, 1.0, 0.0);
    const auto kCameraRollAxis = vec3<double>(0.0, 0.0, 1.0);

    constexpr double kPrecessionYawRate = 0.0100;
    constexpr double kPrecessionPitchRate = -0.0029;
    constexpr double kPrecessionRollRate = 0.0003;
    if(display_options.precess){
        const auto q_y = quaternion().from_axis_angle(kCameraYawAxis, (kPrecessionYawRate * display_options.precess_rate) * kDegToRad);
        const auto q_x = quaternion().from_axis_angle(kCameraPitchAxis, (kPrecessionPitchRate * display_options.precess_rate) * kDegToRad);
        const auto q_z = quaternion().from_axis_angle(kCameraRollAxis, (kPrecessionRollRate * display_options.precess_rate) * kDegToRad);
        display_options.orientation = (q_y * q_z * q_x * display_options.orientation).normalized();
    }

    if(request.mouse.hovered || request.mouse.active){
        constexpr double kTrackballRollDegreesPerPixel = 0.30;
        constexpr double kPanMultiplier = 1.0;
        constexpr double kZoomScalePerWheelNotch = 1.10;
        constexpr double kMinZoom = 0.1;
        constexpr double kMaxZoom = 100.0;

        const auto nav_w = std::max<double>(static_cast<double>(width_px), 1.0);
        const auto nav_h = std::max<double>(static_cast<double>(height_px), 1.0);
        const auto nav_aspect = nav_w / nav_h;
        display_options.zoom = std::clamp(display_options.zoom, kMinZoom, kMaxZoom);
        const auto zoom = display_options.zoom;
        const auto world_width = (2.0 * nav_aspect) / zoom;
        const auto world_height = 2.0 / zoom;
        const auto world_dx_per_px = world_width / nav_w;
        const auto world_dy_per_px = world_height / nav_h;

        const auto to_trackball = [](double x_px, double y_px, double w_px, double h_px){
            const auto normalized_x = std::clamp((2.0 * x_px - w_px) / w_px, -1.0, 1.0);
            const auto normalized_y = std::clamp((h_px - 2.0 * y_px) / h_px, -1.0, 1.0);
            const auto r2 = normalized_x * normalized_x + normalized_y * normalized_y;
            if(r2 <= 1.0){
                return vec3<double>(normalized_x, normalized_y, std::sqrt(std::max(0.0, 1.0 - r2))).unit();
            }
            return vec3<double>(normalized_x, normalized_y, 0.0).unit();
        };

        if(request.mouse.dragging.at(0)){
            const auto curr = to_trackball(request.mouse.position_px.x,
                                           request.mouse.position_px.y,
                                           nav_w,
                                           nav_h);
            const auto prev = to_trackball(request.mouse.position_px.x - request.mouse.drag_delta_px.at(0).x,
                                           request.mouse.position_px.y - request.mouse.drag_delta_px.at(0).y,
                                           nav_w,
                                           nav_h);
            const auto q_drag = quaternion().from_two_unit_vectors(prev, curr);
            display_options.orientation = (display_options.orientation * q_drag).normalized();
        }
        if(request.mouse.dragging.at(1)){
            display_options.model.coeff(0,3) += static_cast<double>(request.mouse.drag_delta_px.at(1).x) * world_dx_per_px * kPanMultiplier;
            display_options.model.coeff(1,3) -= static_cast<double>(request.mouse.drag_delta_px.at(1).y) * world_dy_per_px * kPanMultiplier;
        }
        if(request.mouse.dragging.at(2)){
            const auto roll_rad = request.mouse.drag_delta_px.at(2).x * kTrackballRollDegreesPerPixel * kDegToRad;
            const auto q_roll = quaternion().from_axis_angle(kCameraForward, -roll_rad);
            display_options.orientation = (display_options.orientation * q_roll).normalized();
        }
        if(request.mouse.hovered && (std::abs(request.mouse.wheel_delta) > 0.0)){
            const auto scale = std::pow(kZoomScalePerWheelNotch, request.mouse.wheel_delta);
            display_options.zoom = std::clamp(display_options.zoom * scale, kMinZoom, kMaxZoom);
        }
    }
    sync_euler_from_orientation(display_options);

    const auto aspect = static_cast<double>(width_px) / static_cast<double>(height_px);
    const auto l_bound = static_cast<float>(-aspect / display_options.zoom);
    const auto r_bound = static_cast<float>( aspect / display_options.zoom);
    const auto b_bound = static_cast<float>(-1.0 / display_options.zoom);
    const auto t_bound = static_cast<float>( 1.0 / display_options.zoom);
    const auto n_bound = static_cast<float>(-1000.0 / display_options.zoom);
    const auto f_bound = static_cast<float>( 1000.0 / display_options.zoom);
    const auto proj = make_orthographic_projection_matrix(l_bound, r_bound, b_bound, t_bound, n_bound, f_bound);

    const auto axis_1 = display_options.orientation.rotate(kCameraForward);
    const auto axis_3 = display_options.orientation.rotate(kCameraUp);
    const auto target_pos = vec3<double>(0.0, 0.0, 0.0);
    const auto cam_pos = axis_1.unit() * std::exp(display_options.cam_distort - 5.0);
    const auto camera = make_camera_matrix(cam_pos, target_pos, axis_3.unit());

    const auto mv = camera * display_options.model;
    const auto mvp = proj * mv;
    const auto norm = extract_normal_matrix(mv);

    impl_->hover = hover_state_t{};
    impl_->overlay_ready = false;
    if(request.hover_faces_enabled && (request.mouse.hovered || request.mouse.active)){
        hover_pick_options_t hover_options;
        hover_options.include_coplanar_geometry = request.include_coplanar_geometry;
        hover_options.coplanar_eps = request.coplanar_eps;
        const auto hover = compute_hover_state(*request.mesh,
                                               impl_->cache.render_vertices_d,
                                               mvp,
                                               width_px,
                                               height_px,
                                               request.mouse.position_px,
                                               hover_options);
        if(hover){
            impl_->hover = hover.value();
            if(impl_->hover.plane){
                const auto overlay_normal = impl_->hover.plane->normal().unit();
                for(std::size_t i = 0U; i < impl_->overlay_render_vertices.size(); ++i){
                    const auto nudged = impl_->hover.rectangle_world.at(i) + overlay_normal * 1.0E-4;
                    const auto normalized = impl_->cache.normalize_point(nudged);
                    impl_->overlay_render_vertices.at(i) = vec3<float>(static_cast<float>(normalized.x),
                                                                       static_cast<float>(normalized.y),
                                                                       static_cast<float>(normalized.z));
                    impl_->overlay_render_normals.at(i) = vec3<float>(static_cast<float>(overlay_normal.x),
                                                                      static_cast<float>(overlay_normal.y),
                                                                      static_cast<float>(overlay_normal.z));
                }
                glBindBuffer(GL_ARRAY_BUFFER, impl_->overlay_vbo);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(impl_->overlay_render_vertices.size() * sizeof(vec3<float>)),
                             impl_->overlay_render_vertices.data(),
                             GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, impl_->overlay_nbo);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(impl_->overlay_render_normals.size() * sizeof(vec3<float>)),
                             impl_->overlay_render_normals.data(),
                             GL_DYNAMIC_DRAW);
                impl_->overlay_ready = true;
            }
        }
    }

    glUseProgram(request.shader_program);
    CHECK_FOR_GL_ERRORS();

    const auto shader_user_colour_loc = glGetUniformLocation(request.shader_program, "user_colour");
    const auto shader_diffuse_colour_loc = glGetUniformLocation(request.shader_program, "diffuse_colour");
    const auto mvp_loc = glGetUniformLocation(request.shader_program, "mvp_matrix");
    const auto mv_loc = glGetUniformLocation(request.shader_program, "mv_matrix");
    const auto norm_loc = glGetUniformLocation(request.shader_program, "norm_matrix");
    const auto use_lighting_loc = glGetUniformLocation(request.shader_program, "use_lighting");
    const auto use_smoothing_loc = glGetUniformLocation(request.shader_program, "use_smoothing");

    const std::vector<float> mv_data(mv.cbegin(), mv.cend());
    const std::vector<float> mvp_data(mvp.cbegin(), mvp.cend());
    const std::vector<float> norm_data(norm.cbegin(), norm.cend());
    if(0 <= mv_loc) glUniformMatrix4fv(mv_loc, 1, GL_FALSE, mv_data.data());
    if(0 <= mvp_loc) glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp_data.data());
    if(0 <= norm_loc) glUniformMatrix3fv(norm_loc, 1, GL_FALSE, norm_data.data());
    if(0 <= use_lighting_loc) glUniform1ui(use_lighting_loc, display_options.use_lighting ? GL_TRUE : GL_FALSE);
    if(0 <= use_smoothing_loc) glUniform1ui(use_smoothing_loc, display_options.use_smoothing ? GL_TRUE : GL_FALSE);
    if(0 <= shader_user_colour_loc){
        glUniform4f(shader_user_colour_loc,
                    display_options.colours.at(0),
                    display_options.colours.at(1),
                    display_options.colours.at(2),
                    display_options.colours.at(3));
    }
    if(0 <= shader_diffuse_colour_loc){
        glUniform4f(shader_diffuse_colour_loc,
                    display_options.colours.at(0),
                    display_options.colours.at(1),
                    display_options.colours.at(2),
                    display_options.colours.at(3));
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if(display_options.use_opaque){
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }else{
        glDisable(GL_CULL_FACE);
    }

    glBindVertexArray(impl_->mesh_vao);
    if(display_options.render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(impl_->cache.triangle_indices.size()),
                   GL_UNSIGNED_INT,
                   nullptr);
    if(display_options.render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);

    if(impl_->overlay_ready){
        const auto overlay_colour = std::array<float, 4>{ 1.0f, 0.85f, 0.20f, 1.0f };
        if(0 <= use_lighting_loc) glUniform1ui(use_lighting_loc, GL_FALSE);
        if(0 <= use_smoothing_loc) glUniform1ui(use_smoothing_loc, GL_FALSE);
        if(0 <= shader_user_colour_loc){
            glUniform4f(shader_user_colour_loc,
                        overlay_colour.at(0),
                        overlay_colour.at(1),
                        overlay_colour.at(2),
                        overlay_colour.at(3));
        }
        if(0 <= shader_diffuse_colour_loc){
            glUniform4f(shader_diffuse_colour_loc,
                        overlay_colour.at(0),
                        overlay_colour.at(1),
                        overlay_colour.at(2),
                        overlay_colour.at(3));
        }

        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(impl_->overlay_vao);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(impl_->overlay_render_vertices.size()));
        glBindVertexArray(0);
        glLineWidth(1.0f);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prior_framebuffer));
    glViewport(prior_viewport[0], prior_viewport[1], prior_viewport[2], prior_viewport[3]);
    glUseProgram(static_cast<GLuint>(prior_program));
    CHECK_FOR_GL_ERRORS();
    return true;
}
