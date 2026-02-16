// SDL_Viewer_OpenGL.cc - OpenGL utility classes and functions for SDL_Viewer.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <GL/glew.h>

#include "YgorLog.h"
#include "YgorMath.h"
#include "YgorMisc.h"

#include "../String_Parsing.h"
#include "../Surface_Meshes.h"

#include "SDL_Viewer_OpenGL.h"


// opengl_mesh implementation

opengl_mesh::opengl_mesh( const fv_surface_mesh<double, uint64_t> &meshes,
                          bool reverse_normals ){

    this->N_vertices = static_cast<GLsizei>(meshes.vertices.size());
    this->N_triangles = 0;
    for(const auto& f : meshes.faces){
        int64_t l_N_indices = f.size();
        if(l_N_indices < 3) continue; // Ignore faces that cannot be broken into triangles.
        this->N_triangles += static_cast<GLsizei>(l_N_indices - 2);
    }
    const auto N_vert_normals = static_cast<GLsizei>(meshes.vertex_normals.size());
    const bool has_vert_normals = (N_vert_normals == this->N_vertices);

    // Find an axis-aligned bounding box.
    const auto inf = std::numeric_limits<double>::infinity();
    auto x_min = inf;
    auto y_min = inf;
    auto z_min = inf;
    auto x_max = -inf;
    auto y_max = -inf;
    auto z_max = -inf;
    for(const auto &v : meshes.vertices){
        if(v.x < x_min) x_min = v.x;
        if(v.y < y_min) y_min = v.y;
        if(v.z < z_min) z_min = v.z;
        if(x_max < v.x) x_max = v.x;
        if(y_max < v.y) y_max = v.y;
        if(z_max < v.z) z_max = v.z;
    }

    // Adjust individual axes to respect the aspect ratio.
    const auto x_range = x_max - x_min;
    const auto y_range = y_max - y_min;
    const auto z_range = z_max - z_min;
    const auto max_range = std::max<double>({x_range, y_range, z_range});
    x_min = (x_max + x_min) * 0.5 - max_range * 0.5;
    x_max = (x_max + x_min) * 0.5 + max_range * 0.5;
    y_min = (y_max + y_min) * 0.5 - max_range * 0.5;
    y_max = (y_max + y_min) * 0.5 + max_range * 0.5;
    z_min = (z_max + z_min) * 0.5 - max_range * 0.5;
    z_max = (z_max + z_min) * 0.5 + max_range * 0.5;

    // Marshall the vertex and index information in CPU-accessible buffers where they can be freely
    // preprocessed.
    std::vector<vec3<float>> vertices;
    vertices.reserve(this->N_vertices);
    for(const auto &v : meshes.vertices){
        // Scale each of x, y, and z to [-1,+1], respecting the aspect ratio, but shrink down further to
        // [-1/sqrt(3),+1/sqrt(3)] to account for rotation. Scaling down will ensure the corners are not clipped
        // when the cube is rotated.
        vec3<float>  w( (2.0 * (v.x - x_min) / (x_max - x_min) - 1.0) / std::sqrt(3.0),
                        (2.0 * (v.y - y_min) / (y_max - y_min) - 1.0) / std::sqrt(3.0),
                        (2.0 * (v.z - z_min) / (z_max - z_min) - 1.0) / std::sqrt(3.0) );
        vertices.push_back(w);
    }

    std::vector<vec3<float>> normals;
    if(has_vert_normals){
        normals.reserve(this->N_vertices);
    }else{
        normals.resize(this->N_vertices, vec3<float>(0,0,0));
    }
    
    std::vector<unsigned int> indices;
    indices.reserve(3 * this->N_triangles);
    for(const auto& f : meshes.faces){
        int64_t l_N_indices = f.size();
        if(l_N_indices < 3) continue; // Ignore faces that cannot be broken into triangles.

        const auto it_1 = std::cbegin(f);
        const auto it_2 = std::next(it_1);
        const auto end = std::end(f);
        for(auto it_3 = std::next(it_2); it_3 != end; ++it_3){
            const auto i_A = static_cast<unsigned int>( (reverse_normals) ? *it_1 : *it_3 );
            const auto i_B = static_cast<unsigned int>( *it_2 );
            const auto i_C = static_cast<unsigned int>( (reverse_normals) ? *it_3 : *it_1 );

            indices.push_back(i_A);
            indices.push_back(i_B);
            indices.push_back(i_C);

            if(!has_vert_normals){
                // Make area-averaged normals for each vertex by summing the area-weighted normal for each face.
                const auto awn = (meshes.vertices[i_C] - meshes.vertices[i_B]).Cross(meshes.vertices[i_A] - meshes.vertices[i_B]);
                const auto fawn = vec3<float>( static_cast<float>(awn.x),  static_cast<float>(awn.y), static_cast<float>(awn.z) );
                                             
                normals[i_A] += fawn;
                normals[i_B] += fawn;
                normals[i_C] += fawn;
            }
        }
    }
    this->N_indices = static_cast<GLsizei>(indices.size());

    if(has_vert_normals){
        // Convert from double to float.
        for(const auto& v : meshes.vertex_normals){
            normals.push_back( vec3<float>( static_cast<float>(v.x),
                                            static_cast<float>(v.y),
                                            static_cast<float>(v.z) ) );
        }

    }else{
        // Note that this step is not needed if we normalize in the shader. Probably best to keep it correct though.
        for(auto &v : normals) v = v.unit();
    }

    if(vertices.size() != normals.size()){
        throw std::logic_error("Vertex normals not consistent with vertex positions");
    }

    // Push the data into OpenGL buffers.
    CHECK_FOR_GL_ERRORS();

    // Vertex data.
    glGenBuffers(1, &this->vbo); // Create a VBO inside the OpenGL context.
    if(this->vbo == 0) throw std::runtime_error("Unable to generate vertex buffer object");
    CHECK_FOR_GL_ERRORS();
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    CHECK_FOR_GL_ERRORS();
    glBufferData(GL_ARRAY_BUFFER, (3 * vertices.size()) * sizeof(GLfloat), static_cast<void*>(vertices.data()), GL_STATIC_DRAW); // Copy vertex data.
    CHECK_FOR_GL_ERRORS();

    // Normals data.
    glGenBuffers(1, &this->nbo); // Create a VBO inside the OpenGL context.
    if(this->nbo == 0) throw std::runtime_error("Unable to generate vertex buffer object");
    CHECK_FOR_GL_ERRORS();
    glBindBuffer(GL_ARRAY_BUFFER, this->nbo);
    CHECK_FOR_GL_ERRORS();
    glBufferData(GL_ARRAY_BUFFER, (3 * normals.size()) * sizeof(GLfloat), static_cast<void*>(normals.data()), GL_STATIC_DRAW); // Copy normals data.
    CHECK_FOR_GL_ERRORS();

    // Element data.
    glGenBuffers(1, &this->ebo); // Create a EBO inside the OpenGL context.
    if(this->ebo == 0) throw std::runtime_error("Unable to generate element buffer object");
    CHECK_FOR_GL_ERRORS();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    CHECK_FOR_GL_ERRORS();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), static_cast<void*>(indices.data()), GL_STATIC_DRAW); // Copy index data.
    CHECK_FOR_GL_ERRORS();

    // Vertex array object.
    glGenVertexArrays(1, &this->vao); // Create a VAO inside the OpenGL context.
    if(this->vao == 0) throw std::runtime_error("Unable to generate vertex array object");
    CHECK_FOR_GL_ERRORS();
    glBindVertexArray(this->vao);
    CHECK_FOR_GL_ERRORS();

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    CHECK_FOR_GL_ERRORS();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); // Vertex positions, 3 floats per vertex, attrib index 0.
    CHECK_FOR_GL_ERRORS();

    glBindBuffer(GL_ARRAY_BUFFER, this->nbo);
    CHECK_FOR_GL_ERRORS();
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0); // Vertex normals, 3 floats per vertex, attrib index 1.
    CHECK_FOR_GL_ERRORS();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
    CHECK_FOR_GL_ERRORS();
    glVertexAttribPointer(2, 3, GL_UNSIGNED_INT, GL_FALSE, 0, 0); // Indices, 3 coordinates per face (triangles only), attrib index 2.
    CHECK_FOR_GL_ERRORS();


    glEnableVertexAttribArray(0);
    CHECK_FOR_GL_ERRORS();
    glEnableVertexAttribArray(1);
    CHECK_FOR_GL_ERRORS();
    glEnableVertexAttribArray(2);
    CHECK_FOR_GL_ERRORS();

    YLOGINFO("Registered new OpenGL mesh");
}


void opengl_mesh::draw(bool render_wireframe){
    CHECK_FOR_GL_ERRORS();
    glBindVertexArray(this->vao);
    CHECK_FOR_GL_ERRORS();

    if(render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Enable wireframe mode.
    CHECK_FOR_GL_ERRORS();
    glDrawElements(GL_TRIANGLES, this->N_indices, GL_UNSIGNED_INT, 0); // Draw using the current shader setup.
    CHECK_FOR_GL_ERRORS();
    if(render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Disable wireframe mode.
    CHECK_FOR_GL_ERRORS();

    glBindVertexArray(0);
    CHECK_FOR_GL_ERRORS();
}


opengl_mesh::~opengl_mesh() noexcept(false) {
    // Bind the vertex array object so we can unlink the attribute buffers.
    if( (0 < this->vao)
    &&  (0 < this->vbo)
    &&  (0 < this->nbo)
    &&  (0 < this->ebo) ){
        glBindVertexArray(this->vao);
        glDisableVertexAttribArray(0); // Free OpenGL resources.
        glDisableVertexAttribArray(1);
        glBindVertexArray(0);

        // Delete the attribute buffers and then finally the vertex array object.
        glDeleteBuffers(1, &this->ebo);
        glDeleteBuffers(1, &this->nbo);
        glDeleteBuffers(1, &this->vbo);
        glDeleteVertexArrays(1, &this->vao);
        CHECK_FOR_GL_ERRORS();
    }

    // Reset accessible class state for good measure.
    this->ebo = this->vbo = this->nbo = this->vao = 0;
    this->N_triangles = this->N_indices = this->N_vertices = 0;
}


// ogl_shader_program implementation

ogl_shader_program::ogl_shader_program( std::string vert_shader_src,
                                        std::string frag_shader_src,
                                        std::ostream &os){

    // Compile vertex shader.
    GLchar* vert_shader_src_c[2] = { static_cast<GLchar*>(vert_shader_src.data()), nullptr };
    GLuint vert_handle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_handle, 1, vert_shader_src_c, NULL);
    glCompileShader(vert_handle);

    {
        GLint status = 0, log_length = 0;
        glGetShaderiv(vert_handle, GL_COMPILE_STATUS, &status);
        glGetShaderiv(vert_handle, GL_INFO_LOG_LENGTH, &log_length);
        if(1 < log_length){
            std::string buf;
            buf.resize((int64_t)(log_length + 1), '\0');
            glGetShaderInfoLog(vert_handle, log_length, NULL, static_cast<GLchar*>(&(buf[0])) );
            os << "Vertex shader compilation log:\n" << buf;
        }
        if( static_cast<GLboolean>(status) == GL_FALSE ){
            throw std::runtime_error("Unable to compile vertex shader");
        }
    }

    // Compile fragment shader.
    GLchar* frag_shader_src_c[2] = { static_cast<GLchar*>(frag_shader_src.data()), nullptr };
    GLuint frag_handle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_handle, 1, frag_shader_src_c, NULL);
    glCompileShader(frag_handle);

    {
        GLint status = 0, log_length = 0;
        glGetShaderiv(frag_handle, GL_COMPILE_STATUS, &status);
        glGetShaderiv(frag_handle, GL_INFO_LOG_LENGTH, &log_length);
        if(1 < log_length){
            std::string buf;
            buf.resize((int64_t)(log_length + 1), '\0');
            glGetShaderInfoLog(frag_handle, log_length, NULL, static_cast<GLchar*>(&(buf[0])) );
            os << "Fragment shader compilation log:\n" << buf;
        }
        if( static_cast<GLboolean>(status) == GL_FALSE ){
            throw std::runtime_error("Unable to compile fragment shader");
        }
    }

    // Link shaders into a program.
    GLuint custom_gl_program = glCreateProgram();
    glAttachShader(custom_gl_program, vert_handle);
    glAttachShader(custom_gl_program, frag_handle);
    glLinkProgram(custom_gl_program);

    {
        GLint status = 0, log_length = 0;
        glGetProgramiv(custom_gl_program, GL_LINK_STATUS, &status);
        glGetProgramiv(custom_gl_program, GL_INFO_LOG_LENGTH, &log_length);
        if(1 < log_length){
            std::string buf;
            buf.resize((int64_t)(log_length + 1), '\0');
            glGetProgramInfoLog(custom_gl_program, log_length, NULL, static_cast<GLchar*>(&(buf[0])) );
            os << "Shader link log:\n" << buf;
        }
        if( static_cast<GLboolean>(status) == GL_FALSE ){
            throw std::runtime_error("Unable to link shader program");
        }
    }

    // Lazily delete the shaders.
    glDetachShader(custom_gl_program, vert_handle);
    glDetachShader(custom_gl_program, frag_handle);
    glDeleteShader(vert_handle);
    glDeleteShader(frag_handle);

    // Shader program is now valid and registered.
    this->program_ID = custom_gl_program;
}


ogl_shader_program::~ogl_shader_program(){
    glDeleteProgram( this->program_ID );
}


GLuint ogl_shader_program::get_program_ID() const {
    return this->program_ID;
}


// compile_shader_program implementation

std::unique_ptr<ogl_shader_program>
compile_shader_program(const std::array<char, 2048> &vert_shader_src,
                       const std::array<char, 2048> &frag_shader_src,
                       std::array<char, 2048> &shader_log ){
    shader_log.fill('\0');
    std::stringstream ss;
    try{
        auto l_custom_shader = std::make_unique<ogl_shader_program>(array_to_string(vert_shader_src),
                                                                    array_to_string(frag_shader_src),
                                                                    ss);
        return l_custom_shader;
    }catch(const std::exception &e){
        shader_log = string_to_array(ss.str());
        throw;
    }
}


