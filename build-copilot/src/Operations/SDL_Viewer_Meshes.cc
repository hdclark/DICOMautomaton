// SDL_Viewer_Meshes.cc - A part of DICOMautomaton 2026. Written by hal clark.

#include "SDL_Viewer_Meshes.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "YgorLog.h"

namespace {

void check_for_gl_errors(const char *func, int line){
    while(true){
        const auto err = glGetError();
        if(err == GL_NO_ERROR) break;
        throw std::runtime_error(std::string("OpenGL error in ")
                                 + func
                                 + " at line "
                                 + std::to_string(line)
                                 + ": "
                                 + reinterpret_cast<const char*>(glewGetErrorString(err)));
    }
}

void log_gl_errors(const char *func, int line) noexcept {
    try{
        while(true){
            const auto err = glGetError();
            if(err == GL_NO_ERROR) break;
            const auto *err_str = reinterpret_cast<const char*>(glewGetErrorString(err));
            YLOGWARN(std::string("OpenGL error in ")
                     + func
                     + " at line "
                     + std::to_string(line)
                     + ": "
                     + ((err_str != nullptr) ? err_str : "unknown error"));
        }
    }catch(...){
    }
}

#define CHECK_FOR_GL_ERRORS_MESHES() check_for_gl_errors(__PRETTY_FUNCTION__, __LINE__)
#define LOG_GL_ERRORS_MESHES() log_gl_errors(__PRETTY_FUNCTION__, __LINE__)

void cleanup_mesh_handles(GLuint &vao,
                          GLuint &vbo,
                          GLuint &nbo,
                          GLuint &ebo,
                          GLuint &primitive_face_index_bo,
                          GLuint &primitive_face_index_tex,
                          GLuint &primitive_edge_index_bo,
                          GLuint &primitive_edge_index_tex) noexcept {
    if(0 < vao){
        glBindVertexArray(vao);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    if(0 < primitive_edge_index_tex) glDeleteTextures(1, &primitive_edge_index_tex);
    if(0 < primitive_edge_index_bo) glDeleteBuffers(1, &primitive_edge_index_bo);
    if(0 < primitive_face_index_tex) glDeleteTextures(1, &primitive_face_index_tex);
    if(0 < primitive_face_index_bo) glDeleteBuffers(1, &primitive_face_index_bo);
    if(0 < ebo) glDeleteBuffers(1, &ebo);
    if(0 < nbo) glDeleteBuffers(1, &nbo);
    if(0 < vbo) glDeleteBuffers(1, &vbo);
    if(0 < vao) glDeleteVertexArrays(1, &vao);
    primitive_edge_index_tex = primitive_edge_index_bo = 0;
    primitive_face_index_tex = primitive_face_index_bo = 0;
    ebo = vbo = nbo = vao = 0;
}

num_array<float> make_orthographic_projection_matrix(float left_bound,
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

num_array<float> make_camera_matrix(const vec3<double> &cam_pos,
                                    const vec3<double> &target_pos,
                                    const vec3<double> &up_unit){
    num_array<float> out(4, 4, 0.0f);

    const auto inward = (cam_pos - target_pos).unit();
    const auto leftward = up_unit.Cross(inward).unit();
    const auto upward = inward.Cross(leftward).unit();

    if(!(inward.isfinite() && leftward.isfinite() && upward.isfinite())){
        return num_array<float>().identity(4);
    }

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

    return out.transpose();
}

num_array<float> extract_normal_matrix(const num_array<float> &mv){
    if((mv.num_rows() != 4) || (mv.num_cols() != 4)){
        throw std::logic_error("Expected 4x4 matrix");
    }
    num_array<float> out(3, 3, 0.0f);
    for(int64_t r = 0; r < 3; ++r){
        for(int64_t c = 0; c < 3; ++c){
            out.coeff(r, c) = mv.read_coeff(r, c);
        }
    }
    return out;
}

std::array<double, 4> transform_point(const num_array<float> &mvp,
                                      const vec3<double> &point){
    const std::array<double, 4> in = { point.x, point.y, point.z, 1.0 };
    std::array<double, 4> out = { 0.0, 0.0, 0.0, 0.0 };
    for(int r = 0; r < 4; ++r){
        for(int c = 0; c < 4; ++c){
            out[r] += static_cast<double>(mvp.read_coeff(r, c)) * in.at(c);
        }
    }
    return out;
}

std::optional<vec2<double>> to_screen_point(const Mesh_Widget::viewport_t &viewport,
                                            const num_array<float> &mvp,
                                            const vec3<double> &point){
    const auto clip = transform_point(mvp, point);
    if(std::abs(clip.at(3)) <= std::numeric_limits<double>::epsilon()){
        return {};
    }

    const auto ndc_x = clip.at(0) / clip.at(3);
    const auto ndc_y = clip.at(1) / clip.at(3);
    if(!std::isfinite(ndc_x) || !std::isfinite(ndc_y)){
        return {};
    }

    return vec2<double>(static_cast<double>(viewport.x) + (0.5 * (ndc_x + 1.0)) * viewport.width,
                        static_cast<double>(viewport.y) + (0.5 * (1.0 - ndc_y)) * viewport.height);
}

bool point_in_triangle_2d(const vec2<double> &p,
                          const vec2<double> &a,
                          const vec2<double> &b,
                          const vec2<double> &c){
    const auto sign = [](const vec2<double> &p1,
                         const vec2<double> &p2,
                         const vec2<double> &p3) -> double {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    const auto d1 = sign(p, a, b);
    const auto d2 = sign(p, b, c);
    const auto d3 = sign(p, c, a);
    const bool has_neg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
    const bool has_pos = (0.0 < d1) || (0.0 < d2) || (0.0 < d3);
    return !(has_neg && has_pos);
}

void upload_mesh_uniforms(GLuint shader_program,
                          const Mesh_Widget::display_options_t &display_options,
                          const Mesh_Widget::matrices_t &matrices){
    const auto mvp_loc = glGetUniformLocation(shader_program, "mvp_matrix");
    const auto mv_loc = glGetUniformLocation(shader_program, "mv_matrix");
    const auto norm_loc = glGetUniformLocation(shader_program, "norm_matrix");
    const auto user_colour_loc = glGetUniformLocation(shader_program, "user_colour");
    const auto diffuse_colour_loc = glGetUniformLocation(shader_program, "diffuse_colour");
    const auto use_lighting_loc = glGetUniformLocation(shader_program, "use_lighting");
    const auto use_smoothing_loc = glGetUniformLocation(shader_program, "use_smoothing");

    const std::vector<float> mvp_data(matrices.mvp.cbegin(), matrices.mvp.cend());
    const std::vector<float> mv_data(matrices.mv.cbegin(), matrices.mv.cend());
    const std::vector<float> norm_data(matrices.norm.cbegin(), matrices.norm.cend());

    if(0 <= mvp_loc) glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp_data.data());
    if(0 <= mv_loc) glUniformMatrix4fv(mv_loc, 1, GL_FALSE, mv_data.data());
    if(0 <= norm_loc) glUniformMatrix3fv(norm_loc, 1, GL_FALSE, norm_data.data());
    if(0 <= use_lighting_loc){
        glUniform1ui(use_lighting_loc, display_options.use_lighting ? GL_TRUE : GL_FALSE);
    }
    if(0 <= use_smoothing_loc){
        glUniform1ui(use_smoothing_loc, display_options.use_smoothing ? GL_TRUE : GL_FALSE);
    }
    if(0 <= user_colour_loc){
        glUniform4f(user_colour_loc,
                    display_options.colours[0],
                    display_options.colours[1],
                    display_options.colours[2],
                    display_options.colours[3]);
    }
    if(0 <= diffuse_colour_loc){
        glUniform4f(diffuse_colour_loc,
                    display_options.colours[0],
                    display_options.colours[1],
                    display_options.colours[2],
                    display_options.colours[3]);
    }
}

} // namespace

opengl_mesh::opengl_mesh(const fv_surface_mesh<double, uint64_t> &meshes,
                         bool reverse_normals){
    this->N_vertices = static_cast<GLsizei>(meshes.vertices.size());
    this->N_triangles = 0;
    for(const auto &f : meshes.faces){
        const auto l_N_indices = static_cast<int64_t>(f.size());
        if(l_N_indices < 3) continue;
        this->N_triangles += static_cast<GLsizei>(l_N_indices - 2);
    }

    struct edge_pair_t {
        uint64_t a;
        uint64_t b;
        bool operator<(const edge_pair_t &other) const {
            return std::tie(a, b) < std::tie(other.a, other.b);
        }
    };
    std::map<edge_pair_t, uint32_t> unique_edges;
    uint32_t next_edge_idx = 0U;
    for(const auto &f : meshes.faces){
        for(std::size_t k = 0U; k < f.size(); ++k){
            auto v0 = f[k];
            auto v1 = f[(k + 1U) % f.size()];
            if(v1 < v0) std::swap(v0, v1);
            const bool inserted = unique_edges.try_emplace(edge_pair_t{ v0, v1 }, next_edge_idx).second;
            if(inserted){
                ++next_edge_idx;
            }
        }
    }
    this->N_euler = static_cast<int64_t>(this->N_vertices)
                  - static_cast<int64_t>(unique_edges.size())
                  + static_cast<int64_t>(meshes.faces.size());

    const auto N_vert_normals = static_cast<GLsizei>(meshes.vertex_normals.size());
    const bool has_vert_normals = (N_vert_normals == this->N_vertices);

    const auto inf = std::numeric_limits<double>::infinity();
    auto x_min = inf;
    auto y_min = inf;
    auto z_min = inf;
    auto x_max = -inf;
    auto y_max = -inf;
    auto z_max = -inf;
    for(const auto &v : meshes.vertices){
        x_min = std::min(x_min, v.x);
        y_min = std::min(y_min, v.y);
        z_min = std::min(z_min, v.z);
        x_max = std::max(x_max, v.x);
        y_max = std::max(y_max, v.y);
        z_max = std::max(z_max, v.z);
    }

    const auto x_range = x_max - x_min;
    const auto y_range = y_max - y_min;
    const auto z_range = z_max - z_min;
    const auto max_range = std::max<double>({ x_range, y_range, z_range, std::numeric_limits<double>::epsilon() });
    const auto x_mid = (x_max + x_min) * 0.5;
    const auto y_mid = (y_max + y_min) * 0.5;
    const auto z_mid = (z_max + z_min) * 0.5;
    x_min = x_mid - max_range * 0.5;
    x_max = x_mid + max_range * 0.5;
    y_min = y_mid - max_range * 0.5;
    y_max = y_mid + max_range * 0.5;
    z_min = z_mid - max_range * 0.5;
    z_max = z_mid + max_range * 0.5;

    std::vector<vec3<float>> vertices;
    vertices.reserve(this->N_vertices);
    this->normalized_vertices_.reserve(this->N_vertices);
    // The normalized mesh is fit into the unit sphere. Dividing the maximum half-extent
    // by sqrt(3) leaves each axis at +/-1/sqrt(3), so the bounding cube stays within
    // the unit sphere and the default orthographic view volume.
    const auto normalization_denom = std::max<double>(0.5 * max_range * std::sqrt(3.0),
                                                      std::numeric_limits<double>::epsilon());
    for(const auto &v : meshes.vertices){
        const auto nv = vec3<double>((v.x - x_mid) / normalization_denom,
                                     (v.y - y_mid) / normalization_denom,
                                     (v.z - z_mid) / normalization_denom);
        this->normalized_vertices_.push_back(nv);
        vertices.emplace_back(static_cast<float>(nv.x),
                              static_cast<float>(nv.y),
                              static_cast<float>(nv.z));
    }

    std::vector<vec3<float>> normals;
    if(has_vert_normals){
        normals.reserve(this->N_vertices);
    }else{
        normals.resize(this->N_vertices, vec3<float>(0.0f, 0.0f, 0.0f));
    }

    std::vector<unsigned int> indices;
    indices.reserve(3 * this->N_triangles);
    std::vector<uint32_t> primitive_face_indices;
    std::vector<uint32_t> primitive_edge_indices;
    primitive_face_indices.reserve(this->N_triangles);
    primitive_edge_indices.reserve(4 * this->N_triangles);
    for(const auto &f : meshes.faces){
        const auto face_idx = static_cast<uint32_t>(&f - meshes.faces.data());
        if(f.size() < 3U) continue;
        const auto it_1 = std::cbegin(f);
        const auto it_2 = std::next(it_1);
        const auto end = std::cend(f);
        for(auto it_3 = std::next(it_2); it_3 != end; ++it_3){
            const auto i_A = static_cast<unsigned int>(reverse_normals ? *it_1 : *it_3);
            const auto i_B = static_cast<unsigned int>(*it_2);
            const auto i_C = static_cast<unsigned int>(reverse_normals ? *it_3 : *it_1);

            indices.push_back(i_A);
            indices.push_back(i_B);
            indices.push_back(i_C);
            primitive_face_indices.push_back(face_idx);

            const auto edge_id = [&unique_edges](uint64_t v0, uint64_t v1) -> uint32_t {
                if(v1 < v0) std::swap(v0, v1);
                return unique_edges.at(edge_pair_t{ v0, v1 });
            };
            primitive_edge_indices.push_back(edge_id(i_A, i_B));
            primitive_edge_indices.push_back(edge_id(i_B, i_C));
            primitive_edge_indices.push_back(edge_id(i_C, i_A));
            primitive_edge_indices.push_back(0U);

            if(!has_vert_normals){
                const auto awn = (meshes.vertices[i_C] - meshes.vertices[i_B]).Cross(meshes.vertices[i_A] - meshes.vertices[i_B]);
                const auto fawn = vec3<float>(static_cast<float>(awn.x),
                                              static_cast<float>(awn.y),
                                              static_cast<float>(awn.z));
                normals[i_A] += fawn;
                normals[i_B] += fawn;
                normals[i_C] += fawn;
            }
        }
    }
    this->N_indices = static_cast<GLsizei>(indices.size());

    if(has_vert_normals){
        for(const auto &v : meshes.vertex_normals){
            normals.emplace_back(static_cast<float>(v.x),
                                 static_cast<float>(v.y),
                                 static_cast<float>(v.z));
        }
    }else{
        for(auto &v : normals){
            v = v.unit();
        }
    }

    if(vertices.size() != normals.size()){
        throw std::logic_error("Vertex normals not consistent with vertex positions");
    }

    try{
        CHECK_FOR_GL_ERRORS_MESHES();

        glGenBuffers(1, &this->vbo);
        if(this->vbo == 0) throw std::runtime_error("Unable to generate vertex buffer object");
        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>((3 * vertices.size()) * sizeof(GLfloat)),
                     static_cast<void*>(vertices.data()),
                     GL_STATIC_DRAW);

        glGenBuffers(1, &this->nbo);
        if(this->nbo == 0) throw std::runtime_error("Unable to generate normals buffer object");
        glBindBuffer(GL_ARRAY_BUFFER, this->nbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>((3 * normals.size()) * sizeof(GLfloat)),
                     static_cast<void*>(normals.data()),
                     GL_STATIC_DRAW);

        glGenBuffers(1, &this->ebo);
        if(this->ebo == 0) throw std::runtime_error("Unable to generate element buffer object");
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                     static_cast<void*>(indices.data()),
                     GL_STATIC_DRAW);

        glGenVertexArrays(1, &this->vao);
        if(this->vao == 0) throw std::runtime_error("Unable to generate vertex array object");
        glBindVertexArray(this->vao);

        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, this->nbo);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        glGenBuffers(1, &this->primitive_face_index_bo);
        if(this->primitive_face_index_bo == 0) throw std::runtime_error("Unable to generate primitive face-index buffer object");
        glBindBuffer(GL_TEXTURE_BUFFER, this->primitive_face_index_bo);
        glBufferData(GL_TEXTURE_BUFFER,
                     static_cast<GLsizeiptr>(primitive_face_indices.size() * sizeof(uint32_t)),
                     static_cast<void*>(primitive_face_indices.data()),
                     GL_STATIC_DRAW);

        glGenTextures(1, &this->primitive_face_index_tex);
        if(this->primitive_face_index_tex == 0) throw std::runtime_error("Unable to generate primitive face-index texture object");
        glBindTexture(GL_TEXTURE_BUFFER, this->primitive_face_index_tex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, this->primitive_face_index_bo);

        glGenBuffers(1, &this->primitive_edge_index_bo);
        if(this->primitive_edge_index_bo == 0) throw std::runtime_error("Unable to generate primitive edge-index buffer object");
        glBindBuffer(GL_TEXTURE_BUFFER, this->primitive_edge_index_bo);
        glBufferData(GL_TEXTURE_BUFFER,
                     static_cast<GLsizeiptr>(primitive_edge_indices.size() * sizeof(uint32_t)),
                     static_cast<void*>(primitive_edge_indices.data()),
                     GL_STATIC_DRAW);

        glGenTextures(1, &this->primitive_edge_index_tex);
        if(this->primitive_edge_index_tex == 0) throw std::runtime_error("Unable to generate primitive edge-index texture object");
        glBindTexture(GL_TEXTURE_BUFFER, this->primitive_edge_index_tex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, this->primitive_edge_index_bo);

        glBindTexture(GL_TEXTURE_BUFFER, 0);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
        CHECK_FOR_GL_ERRORS_MESHES();
    }catch(...){
        cleanup_mesh_handles(this->vao,
                             this->vbo,
                             this->nbo,
                             this->ebo,
                             this->primitive_face_index_bo,
                             this->primitive_face_index_tex,
                             this->primitive_edge_index_bo,
                             this->primitive_edge_index_tex);
        LOG_GL_ERRORS_MESHES();
        throw;
    }

    YLOGINFO("Registered new OpenGL mesh");
}

void opengl_mesh::draw(bool render_wireframe){
    CHECK_FOR_GL_ERRORS_MESHES();
    glBindVertexArray(this->vao);
    if(render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_TRIANGLES, this->N_indices, GL_UNSIGNED_INT, 0);
    if(render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
    CHECK_FOR_GL_ERRORS_MESHES();
}

GLuint opengl_mesh::primitive_face_index_texture() const {
    return this->primitive_face_index_tex;
}

GLuint opengl_mesh::primitive_edge_index_texture() const {
    return this->primitive_edge_index_tex;
}

const std::vector<vec3<double>>& opengl_mesh::normalized_vertices() const {
    return this->normalized_vertices_;
}

opengl_mesh::~opengl_mesh() noexcept {
    const auto had_resources = ((0 < this->vao) || (0 < this->vbo) || (0 < this->nbo) || (0 < this->ebo)
                             || (0 < this->primitive_face_index_bo) || (0 < this->primitive_face_index_tex)
                             || (0 < this->primitive_edge_index_bo) || (0 < this->primitive_edge_index_tex));
    cleanup_mesh_handles(this->vao,
                         this->vbo,
                         this->nbo,
                         this->ebo,
                         this->primitive_face_index_bo,
                         this->primitive_face_index_tex,
                         this->primitive_edge_index_bo,
                         this->primitive_edge_index_tex);
    if(had_resources){
        LOG_GL_ERRORS_MESHES();
    }
    this->N_triangles = this->N_indices = this->N_vertices = 0;
    this->N_euler = 0;
}

Mesh_Widget::~Mesh_Widget() noexcept {
    this->clear_mesh();
}

void Mesh_Widget::clear_mesh(){
    this->mesh_ptr_ = nullptr;
    this->mesh_gpu_ = nullptr;
    this->reset_hover_state();
}

void Mesh_Widget::reload_mesh(const mesh_t &mesh, bool reverse_normals){
    this->mesh_ptr_ = &mesh;
    this->mesh_gpu_ = std::make_unique<opengl_mesh>(mesh, reverse_normals);
    this->reset_hover_state();
}

bool Mesh_Widget::has_mesh() const {
    return static_cast<bool>(this->mesh_gpu_);
}

const opengl_mesh* Mesh_Widget::gpu_mesh() const {
    return this->mesh_gpu_.get();
}

const Mesh_Widget::mesh_t* Mesh_Widget::source_mesh() const {
    return this->mesh_ptr_;
}

const Mesh_Widget::hover_state_t& Mesh_Widget::hovered_face() const {
    return this->hover_state_;
}

std::optional<std::size_t> Mesh_Widget::hovered_face_index() const {
    return this->hover_state_.face_index;
}

void Mesh_Widget::sync_orientation_from_euler(display_options_t &display_options){
    const auto pi = std::acos(-1.0);
    const auto deg_to_rad = pi / 180.0;
    display_options.orientation = quaternion().from_euler_ypr(display_options.rot_y * deg_to_rad,
                                                              display_options.rot_p * deg_to_rad,
                                                              display_options.rot_r * deg_to_rad);
}

void Mesh_Widget::sync_euler_from_orientation(display_options_t &display_options){
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

void Mesh_Widget::set_standard_view(display_options_t &display_options,
                                    standard_view_t standard_view){
    const auto pi = std::acos(-1.0);
    switch(standard_view){
        case standard_view_t::front:
            display_options.rot_y = 0.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            break;
        case standard_view_t::back:
            display_options.rot_y = 180.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            break;
        case standard_view_t::left:
            display_options.rot_y = 90.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            break;
        case standard_view_t::right:
            display_options.rot_y = -90.0;
            display_options.rot_p = 0.0;
            display_options.rot_r = 0.0;
            break;
        case standard_view_t::top:
            display_options.rot_y = 0.0;
            display_options.rot_p = 90.0;
            display_options.rot_r = 0.0;
            break;
        case standard_view_t::bottom:
            display_options.rot_y = 0.0;
            display_options.rot_p = -90.0;
            display_options.rot_r = 0.0;
            break;
    }
    sync_orientation_from_euler(display_options);

    // Avoid small angle drift from round-tripping through Euler angles.
    switch(standard_view){
        case standard_view_t::front:  display_options.orientation = quaternion().from_euler_ypr(0.0, 0.0, 0.0); break;
        case standard_view_t::back:   display_options.orientation = quaternion().from_euler_ypr(pi, 0.0, 0.0); break;
        case standard_view_t::left:   display_options.orientation = quaternion().from_euler_ypr(0.5 * pi, 0.0, 0.0); break;
        case standard_view_t::right:  display_options.orientation = quaternion().from_euler_ypr(-0.5 * pi, 0.0, 0.0); break;
        case standard_view_t::top:    display_options.orientation = quaternion().from_euler_ypr(0.0, 0.5 * pi, 0.0); break;
        case standard_view_t::bottom: display_options.orientation = quaternion().from_euler_ypr(0.0, -0.5 * pi, 0.0); break;
    }
}

Mesh_Widget::matrices_t
Mesh_Widget::compute_matrices(const display_options_t &display_options,
                              const viewport_t &viewport){
    constexpr double kMinZoom = 0.1;
    const auto pi = std::acos(-1.0);
    const auto aspect = static_cast<double>(std::max(viewport.width, 1))
                      / static_cast<double>(std::max(viewport.height, 1));
    const auto zoom = std::max(display_options.zoom, kMinZoom);

    const auto proj = make_orthographic_projection_matrix(static_cast<float>(-aspect / zoom),
                                                          static_cast<float>( aspect / zoom),
                                                          static_cast<float>(-1.0 / zoom),
                                                          static_cast<float>( 1.0 / zoom),
                                                          static_cast<float>(-1000.0 / zoom),
                                                          static_cast<float>( 1000.0 / zoom));

    const auto kCameraForward = vec3<double>(0.0, 0.0, 1.0);
    const auto kCameraUp = vec3<double>(0.0, 1.0, 0.0);
    const auto axis_1 = display_options.orientation.rotate(kCameraForward);
    const auto axis_3 = display_options.orientation.rotate(kCameraUp);
    const auto target_pos = vec3<double>(0.0, 0.0, 0.0);
    const auto up_unit = axis_3.unit();
    const auto cam_pos = axis_1.unit() * std::exp(display_options.cam_distort - 5.0);

    matrices_t out;
    out.proj = proj;
    out.mv = make_camera_matrix(cam_pos, target_pos, up_unit) * display_options.model;
    out.mvp = out.proj * out.mv;
    out.norm = extract_normal_matrix(out.mv);
    (void)pi;
    return out;
}

Mesh_Widget::hover_state_t
Mesh_Widget::compute_hover_state(const mesh_t &mesh,
                                 const std::vector<vec3<double>> &display_vertices,
                                 const matrices_t &matrices,
                                 const viewport_t &viewport,
                                 double mouse_x,
                                 double mouse_y,
                                 double coplanar_eps,
                                 bool collect_coplanar_faces){
    hover_state_t out;
    const auto mouse = vec2<double>(mouse_x, mouse_y);

    double hovered_depth = std::numeric_limits<double>::infinity();
    for(std::size_t face_idx = 0U; face_idx < mesh.faces.size(); ++face_idx){
        const auto &face = mesh.faces.at(face_idx);
        if(face.size() < 3U) continue;
        const auto &face_a = display_vertices.at(face.at(0));
        for(std::size_t tri_idx = 2U; tri_idx < face.size(); ++tri_idx){
            const auto &face_b = display_vertices.at(face.at(tri_idx - 1U));
            const auto &face_c = display_vertices.at(face.at(tri_idx));
            const auto screen_a = to_screen_point(viewport, matrices.mvp, face_a);
            const auto screen_b = to_screen_point(viewport, matrices.mvp, face_b);
            const auto screen_c = to_screen_point(viewport, matrices.mvp, face_c);
            if(!screen_a || !screen_b || !screen_c) continue;
            if(!point_in_triangle_2d(mouse, *screen_a, *screen_b, *screen_c)) continue;

            const auto clip_a = transform_point(matrices.mvp, face_a);
            const auto clip_b = transform_point(matrices.mvp, face_b);
            const auto clip_c = transform_point(matrices.mvp, face_c);
            const auto depth = (clip_a.at(2) / clip_a.at(3)
                              + clip_b.at(2) / clip_b.at(3)
                              + clip_c.at(2) / clip_c.at(3)) / 3.0;
            if(depth < hovered_depth){
                hovered_depth = depth;
                out.face_index = face_idx;
            }
        }
    }

    if(!out.face_index){
        return out;
    }

    const auto &face = mesh.faces.at(*out.face_index);
    if(face.size() < 3U){
        out.face_index = {};
        return out;
    }

    const auto &a = mesh.vertices.at(face.at(0));
    const auto &b = mesh.vertices.at(face.at(1));
    const auto &c = mesh.vertices.at(face.at(2));
    const auto normal = (b - a).Cross(c - a).unit();
    const auto centroid = (a + b + c) / 3.0;
    out.plane = Sketch::plane_frame_t::from_plane(plane<double>(normal, centroid), b - a);
    out.face_vertices.clear();
    out.face_vertices.reserve(face.size());
    for(const auto vertex_idx : face){
        out.face_vertices.push_back(mesh.vertices.at(vertex_idx));
    }

    auto face_min = Sketch::projection_t{  std::numeric_limits<double>::infinity(),  std::numeric_limits<double>::infinity() };
    auto face_max = Sketch::projection_t{ -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
    for(const auto &vertex : out.face_vertices){
        const auto p = Sketch::projection_t{
            out.plane->row_unit.Dot(vertex - out.plane->origin),
            out.plane->col_unit.Dot(vertex - out.plane->origin)
        };
        face_min.u = std::min(face_min.u, p.u);
        face_min.v = std::min(face_min.v, p.v);
        face_max.u = std::max(face_max.u, p.u);
        face_max.v = std::max(face_max.v, p.v);
    }

    if(collect_coplanar_faces && out.plane){
        const auto hovered_plane = out.plane->to_plane();
        for(const auto &candidate_face : mesh.faces){
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
                const bool is_coplanar = (std::abs(hovered_plane.Get_Signed_Distance_To_Point(vertex)) <= coplanar_eps);
                const bool in_bounds = (face_min.u <= projected.u) && (projected.u <= face_max.u)
                                    && (face_min.v <= projected.v) && (projected.v <= face_max.v);
                if(!is_coplanar || !in_bounds){
                    candidate_valid = false;
                    break;
                }
                candidate_loop.push_back(vertex);
            }
            if(candidate_valid){
                out.coplanar_faces.emplace_back(std::move(candidate_loop));
            }
        }
    }

    return out;
}

Mesh_Widget::hover_state_t
Mesh_Widget::compute_hover_state(const mesh_t &mesh,
                                 const matrices_t &matrices,
                                 const viewport_t &viewport,
                                 double mouse_x,
                                 double mouse_y,
                                 double coplanar_eps,
                                 bool collect_coplanar_faces){
    return compute_hover_state(mesh,
                               mesh.vertices,
                               matrices,
                               viewport,
                               mouse_x,
                               mouse_y,
                               coplanar_eps,
                               collect_coplanar_faces);
}

void Mesh_Widget::reset_hover_state(){
    this->hover_state_ = hover_state_t{};
}

void Mesh_Widget::update_navigation(display_options_t &display_options,
                                    const viewport_t &viewport,
                                    const input_state_t &input_state){
    const auto pi = std::acos(-1.0);
    const auto kDegToRad = pi / 180.0;
    const auto kCameraForward = vec3<double>(0.0, 0.0, 1.0);
    const auto kCameraPitchAxis = vec3<double>(1.0, 0.0, 0.0);
    const auto kCameraYawAxis = vec3<double>(0.0, 1.0, 0.0);
    const auto kCameraRollAxis = vec3<double>(0.0, 0.0, 1.0);
    constexpr double kPrecessionYawRate = 0.0100;
    constexpr double kPrecessionPitchRate = -0.0029;
    constexpr double kPrecessionRollRate = 0.0003;
    constexpr double kTrackballRollDegreesPerPixel = 0.30;
    constexpr double kPanMultiplier = 1.0;
    constexpr double kZoomScalePerWheelNotch = 1.10;
    constexpr double kMinZoom = 0.1;
    constexpr double kMaxZoom = 100.0;

    if(display_options.precess){
        const auto q_y = quaternion().from_axis_angle(kCameraYawAxis,
                                                      (kPrecessionYawRate * display_options.precess_rate) * kDegToRad);
        const auto q_x = quaternion().from_axis_angle(kCameraPitchAxis,
                                                      (kPrecessionPitchRate * display_options.precess_rate) * kDegToRad);
        const auto q_z = quaternion().from_axis_angle(kCameraRollAxis,
                                                      (kPrecessionRollRate * display_options.precess_rate) * kDegToRad);
        display_options.orientation = (q_y * q_z * q_x * display_options.orientation).normalized();
    }

    display_options.zoom = std::clamp(display_options.zoom, kMinZoom, kMaxZoom);

    if(input_state.allow_navigation && input_state.mouse_inside){
        const auto nav_w = static_cast<double>(std::max(viewport.width, 1));
        const auto nav_h = static_cast<double>(std::max(viewport.height, 1));
        const auto nav_aspect = nav_w / nav_h;
        const auto zoom = std::clamp(display_options.zoom, kMinZoom, kMaxZoom);
        const auto world_width = (2.0 * nav_aspect) / zoom;
        const auto world_height = 2.0 / zoom;
        const auto world_dx_per_px = world_width / nav_w;
        const auto world_dy_per_px = world_height / nav_h;

        const auto local_x = input_state.mouse_x - static_cast<double>(viewport.x);
        const auto local_y = input_state.mouse_y - static_cast<double>(viewport.y);
        const auto delta_x = this->drag_state_.has_position ? (local_x - this->drag_state_.mouse_x) : 0.0;
        const auto delta_y = this->drag_state_.has_position ? (local_y - this->drag_state_.mouse_y) : 0.0;

        const auto to_trackball = [](double x_px, double y_px, double w_px, double h_px){
            const auto normalized_x = std::clamp((2.0 * x_px - w_px) / w_px, -1.0, 1.0);
            const auto normalized_y = std::clamp((h_px - 2.0 * y_px) / h_px, -1.0, 1.0);
            const auto r2 = normalized_x * normalized_x + normalized_y * normalized_y;
            if(r2 <= 1.0){
                return vec3<double>(normalized_x,
                                    normalized_y,
                                    std::sqrt(std::max(0.0, 1.0 - r2))).unit();
            }
            return vec3<double>(normalized_x, normalized_y, 0.0).unit();
        };

        if(input_state.mouse_down.at(0) && this->drag_state_.mouse_down.at(0) && this->drag_state_.has_position){
            const auto curr = to_trackball(local_x, local_y, nav_w, nav_h);
            const auto prev = to_trackball(this->drag_state_.mouse_x, this->drag_state_.mouse_y, nav_w, nav_h);
            const auto q_drag = quaternion().from_two_unit_vectors(prev, curr);
            display_options.orientation = (display_options.orientation * q_drag).normalized();
        }
        if(input_state.mouse_down.at(1) && this->drag_state_.mouse_down.at(1) && this->drag_state_.has_position){
            display_options.model.coeff(0,3) += static_cast<float>(delta_x * world_dx_per_px * kPanMultiplier);
            display_options.model.coeff(1,3) -= static_cast<float>(delta_y * world_dy_per_px * kPanMultiplier);
        }
        if(input_state.mouse_down.at(2) && this->drag_state_.mouse_down.at(2) && this->drag_state_.has_position){
            const auto roll_rad = delta_x * kTrackballRollDegreesPerPixel * kDegToRad;
            const auto q_roll = quaternion().from_axis_angle(kCameraForward, -roll_rad);
            display_options.orientation = (display_options.orientation * q_roll).normalized();
        }

        if(std::abs(input_state.mouse_wheel) > 0.0){
            const auto scale = std::pow(kZoomScalePerWheelNotch, input_state.mouse_wheel);
            display_options.zoom = std::clamp(display_options.zoom * scale, kMinZoom, kMaxZoom);
        }

        this->drag_state_.mouse_x = local_x;
        this->drag_state_.mouse_y = local_y;
        this->drag_state_.has_position = true;
    }else{
        this->drag_state_.has_position = false;
    }

    this->drag_state_.mouse_down = input_state.mouse_down;
    sync_euler_from_orientation(display_options);
}

void Mesh_Widget::render(GLuint shader_program,
                         display_options_t &display_options,
                         const viewport_t &viewport,
                         const input_state_t &input_state){
    if(!this->mesh_gpu_ || !this->mesh_ptr_){
        this->reset_hover_state();
        return;
    }
    if((viewport.width <= 0) || (viewport.height <= 0)
    || (viewport.framebuffer_width <= 0) || (viewport.framebuffer_height <= 0)){
        this->reset_hover_state();
        return;
    }

    this->update_navigation(display_options, viewport, input_state);
    const auto matrices = compute_matrices(display_options, viewport);

    GLint prior_program = 0;
    GLint prior_viewport[4] = { 0, 0, 0, 0 };
    GLint prior_scissor_box[4] = { 0, 0, 0, 0 };
    GLint prior_depth_func = GL_LESS;
    GLint prior_cull_face_mode = GL_BACK;
    GLint prior_blend_src_rgb = GL_ONE;
    GLint prior_blend_dst_rgb = GL_ZERO;
    GLint prior_blend_src_alpha = GL_ONE;
    GLint prior_blend_dst_alpha = GL_ZERO;
    GLint prior_active_texture = GL_TEXTURE0;
    GLint prior_texture_buffer_binding_face = 0;
    GLint prior_texture_buffer_binding_edge = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prior_program);
    glGetIntegerv(GL_VIEWPORT, prior_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, prior_scissor_box);
    glGetIntegerv(GL_DEPTH_FUNC, &prior_depth_func);
    glGetIntegerv(GL_CULL_FACE_MODE, &prior_cull_face_mode);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prior_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prior_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prior_blend_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prior_blend_dst_alpha);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prior_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &prior_texture_buffer_binding_face);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &prior_texture_buffer_binding_edge);
    glActiveTexture(static_cast<GLenum>(prior_active_texture));
    const auto scissor_enabled = (glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);

    const auto depth_enabled = (glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
    const auto blend_enabled = (glIsEnabled(GL_BLEND) == GL_TRUE);
    const auto cull_enabled = (glIsEnabled(GL_CULL_FACE) == GL_TRUE);

    const auto restore_state = [&]() {
        glDepthFunc(static_cast<GLenum>(prior_depth_func));
        glCullFace(static_cast<GLenum>(prior_cull_face_mode));
        glBlendFuncSeparate(static_cast<GLenum>(prior_blend_src_rgb),
                            static_cast<GLenum>(prior_blend_dst_rgb),
                            static_cast<GLenum>(prior_blend_src_alpha),
                            static_cast<GLenum>(prior_blend_dst_alpha));
        if(depth_enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if(blend_enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if(cull_enabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if(scissor_enabled){
            glEnable(GL_SCISSOR_TEST);
            glScissor(prior_scissor_box[0], prior_scissor_box[1], prior_scissor_box[2], prior_scissor_box[3]);
        }else{
            glDisable(GL_SCISSOR_TEST);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, static_cast<GLuint>(prior_texture_buffer_binding_face));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, static_cast<GLuint>(prior_texture_buffer_binding_edge));
        glActiveTexture(static_cast<GLenum>(prior_active_texture));
        glViewport(prior_viewport[0], prior_viewport[1], prior_viewport[2], prior_viewport[3]);
        glUseProgram(static_cast<GLuint>(prior_program));
    };

    try{
        const auto gl_x = viewport.x;
        const auto gl_y = viewport.framebuffer_height - viewport.y - viewport.height;
        glViewport(gl_x, gl_y, viewport.width, viewport.height);
        glEnable(GL_SCISSOR_TEST);
        glScissor(gl_x, gl_y, viewport.width, viewport.height);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);
        upload_mesh_uniforms(shader_program, display_options, matrices);
        const auto invalid_hover_index = std::numeric_limits<uint32_t>::max();
        const auto hovered_face_index_loc = glGetUniformLocation(shader_program, "hovered_face_index");
        const auto hovered_edge_index_loc = glGetUniformLocation(shader_program, "hovered_edge_index");
        const auto primitive_face_indices_loc = glGetUniformLocation(shader_program, "primitive_face_indices");
        const auto primitive_edge_indices_loc = glGetUniformLocation(shader_program, "primitive_edge_indices");

        if(input_state.allow_face_hover && input_state.mouse_inside){
            this->hover_state_ = compute_hover_state(*this->mesh_ptr_,
                                                     this->mesh_gpu_->normalized_vertices(),
                                                     matrices,
                                                     viewport,
                                                     input_state.mouse_x,
                                                     input_state.mouse_y,
                                                     std::max(0.0, input_state.coplanar_eps),
                                                     input_state.collect_coplanar_faces);
        }else{
            this->reset_hover_state();
        }

        if(0 <= hovered_face_index_loc){
            glUniform1ui(hovered_face_index_loc,
                         this->hover_state_.face_index.has_value()
                             ? static_cast<GLuint>(this->hover_state_.face_index.value())
                             : static_cast<GLuint>(invalid_hover_index));
        }
        if(0 <= hovered_edge_index_loc){
            glUniform1ui(hovered_edge_index_loc, static_cast<GLuint>(invalid_hover_index));
        }
        if(0 <= primitive_face_indices_loc){
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_BUFFER, this->mesh_gpu_->primitive_face_index_texture());
            glUniform1i(primitive_face_indices_loc, 0);
        }
        if(0 <= primitive_edge_indices_loc){
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_BUFFER, this->mesh_gpu_->primitive_edge_index_texture());
            glUniform1i(primitive_edge_indices_loc, 1);
        }
        glActiveTexture(static_cast<GLenum>(prior_active_texture));

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        if(display_options.use_opaque){
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }else{
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDisable(GL_CULL_FACE);
        }

        this->mesh_gpu_->draw(display_options.render_wireframe);
    }catch(...){
        restore_state();
        throw;
    }

    restore_state();
    CHECK_FOR_GL_ERRORS_MESHES();
}
