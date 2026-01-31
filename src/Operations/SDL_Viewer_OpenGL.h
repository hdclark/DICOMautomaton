// SDL_Viewer_OpenGL.h - OpenGL utility classes and functions for SDL_Viewer.
//
// A part of DICOMautomaton 2020-2025. Written by hal clark.
//

#pragma once

#include <memory>
#include <string>
#include <array>
#include <ostream>
#include <sstream>

#include <GL/glew.h>

#include "../Surface_Meshes.h"
#include "../String_Parsing.h"
#include "YgorMath.h"
#include "YgorLog.h"

#include "SDL_Viewer_Utils.h"


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


// Represents a buffer stored in GPU memory that is accessible by OpenGL.
struct opengl_mesh {
    GLuint vao = 0;  // vertex array object.
    GLuint vbo = 0;  // vertex buffer object (vertex positions).
    GLuint nbo = 0;  // normals buffer object (per-vertex normals)
    GLuint ebo = 0;  // element buffer object (per-face integer vertex coordinates)

    GLsizei N_indices = 0;
    GLsizei N_vertices = 0;
    GLsizei N_triangles = 0;

    // Constructor. Allocates space in GPU memory.
    opengl_mesh( const fv_surface_mesh<double, uint64_t> &meshes,
                 bool reverse_normals = false );

    // Draw the mesh in the current OpenGL context.
    void draw(bool render_wireframe = false);

    ~opengl_mesh() noexcept(false);
};


// Represents a compiled and linked OpenGL shader program.
class ogl_shader_program {
    private:
        GLuint program_ID;

    public:
        // Compiles and links the provided shaders. Also registers them with OpenGL.
        // Throws on failure.
        ogl_shader_program( std::string vert_shader_src,
                            std::string frag_shader_src,
                            std::ostream &os);

        // Unregisters shader program from OpenGL.
        ~ogl_shader_program();

        // Get the program ID for use in rendering.
        GLuint get_program_ID() const;
};


// Compile and link a shader program from source arrays.
std::unique_ptr<ogl_shader_program>
compile_shader_program(const std::array<char, 2048> &vert_shader_src,
                       const std::array<char, 2048> &frag_shader_src,
                       std::array<char, 2048> &shader_log );


