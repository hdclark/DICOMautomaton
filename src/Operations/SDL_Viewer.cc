//SDL_Viewer.cc - A part of DICOMautomaton 2020, 2021. Written by hal clark.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>            //Needed for exit() calls.
#include <cstdio>
#include <cstddef>
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>    
#include <tuple>
#include <type_traits>
#include <utility>            //Needed for std::pair.
#include <vector>
#include <chrono>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <initializer_list>
#include <thread>

#include <filesystem>

#include "../imgui20210904/imgui.h"
#include "../imgui20210904/imgui_impl_sdl.h"
#include "../imgui20210904/imgui_impl_opengl3.h"
#include "../implot20210904/implot.h"

#include <SDL.h>
#include <GL/glew.h>            // Initialize with glewInit()

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"

#include "../Operation_Dispatcher.h"

#include "../Colour_Maps.h"
#include "../Common_Boost_Serialization.h"
#include "../Common_Plotting.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"

#include "../Font_DCMA_Minimal.h"
#include "../DCMA_Version.h"
#include "../File_Loader.h"
#include "../Script_Loader.h"
#include "../Standard_Scripts.h"
#include "../Thread_Pool.h"
#include "../Dialogs.h"

#ifdef DCMA_USE_CGAL
    #include "../Surface_Meshes.h"
#endif // DCMA_USE_CGAL

#include "SDL_Viewer.h"

//extern const std::string DCMA_VERSION_STR;

#ifndef CHECK_FOR_GL_ERRORS
    #define CHECK_FOR_GL_ERRORS() { \
        while(true){ \
            GLenum err = glGetError(); \
            if(err == GL_NO_ERROR) break; \
            std::cout << "--(W) In function: " << __PRETTY_FUNCTION__; \
            std::cout << " (line " << __LINE__ << ")"; \
            std::cout << " : " << glewGetErrorString(err); \
            std::cout << "(" << std::to_string(err) << ")." << std::endl; \
            std::cout.flush(); \
            throw std::runtime_error("OpenGL error detected. Refusing to continue"); \
        } \
    }
#endif


static
void
array_to_string(std::string &s, const std::array<char, 2048> &a){
    s.clear();
    for(const auto &c : a){
        if(c == '\0') break;
        s.push_back(c);
    }
    return;
}

static
std::string
array_to_string(const std::array<char, 2048> &a){
    std::string s;
    array_to_string(s,a);
    return s;
}

static
void
string_to_array(std::array<char, 2048> &a, const std::string &s){
    a.fill('\0');
    for(size_t i = 0; (i < s.size()) && ((i+1) < a.size()); ++i){
        a[i] = s[i];
        //a[i+1] = '\0';
    }
    return;
}

static
std::array<char, 2048>
string_to_array(const std::string &s){
    std::array<char, 2048> a;
    string_to_array(a,s);
    return a;
}

// Compute an axis-aligned bounding box in pixel coordinates.
std::tuple<long int, long int, long int, long int>
get_pixelspace_axis_aligned_bounding_box(const planar_image<float, double> &img,
                                         const std::vector<vec3<double>> &points,
                                         double extra_space){

    const auto corner = img.position(0,0) - img.row_unit * img.pxl_dx * 0.5
                                          - img.col_unit * img.pxl_dy * 0.5;
    const auto axis1 = img.row_unit.unit();
    const auto axis2 = img.col_unit.unit();
    //const auto axis3 = img.row_unit.Cross( img.col_unit ).unit();

    const auto inf = std::numeric_limits<double>::infinity();
    vec3<double> bbox_min(inf, inf, inf);
    vec3<double> bbox_max(-inf, -inf, -inf);
    for(const auto& p : points){
        const auto proj1 = (p - corner).Dot(axis1);
        const auto proj2 = (p - corner).Dot(axis2);
        //const auto proj3 = (p - corner).Dot(axis3);
        if((proj1 - extra_space) < bbox_min.x) bbox_min.x = (proj1 - extra_space);
        if((proj2 - extra_space) < bbox_min.y) bbox_min.y = (proj2 - extra_space);
        //if((proj3 - extra_space) < bbox_min.z) bbox_min.z = (proj3 - extra_space);
        if(bbox_max.x < (proj1 + extra_space)) bbox_max.x = (proj1 + extra_space);
        if(bbox_max.y < (proj2 + extra_space)) bbox_max.y = (proj2 + extra_space);
        //if(bbox_max.z < (proj3 + extra_space)) bbox_max.z = (proj3 + extra_space);
    }

    auto row_min = std::clamp(static_cast<long int>(std::floor(bbox_min.x/img.pxl_dx)), 0L, img.rows-1);
    auto row_max = std::clamp(static_cast<long int>(std::ceil(bbox_max.x/img.pxl_dx)), 0L, img.rows-1);
    auto col_min = std::clamp(static_cast<long int>(std::floor(bbox_min.y/img.pxl_dy)), 0L, img.columns-1);
    auto col_max = std::clamp(static_cast<long int>(std::ceil(bbox_max.y/img.pxl_dy)), 0L, img.columns-1);
    return std::make_tuple( row_min, row_max, col_min, col_max );
}

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
                 bool reverse_normals = false ){

        this->N_vertices = static_cast<GLsizei>(meshes.vertices.size());
        this->N_triangles = 0;
        for(const auto& f : meshes.faces){
            long int l_N_indices = f.size();
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
        const auto max_range = std::max({x_range, y_range, z_range});
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
            long int l_N_indices = f.size();
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

        FUNCINFO("Registered new OpenGL mesh");
    };

    // Draw the mesh in the current OpenGL context.
    void draw(bool render_wireframe = false){
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
    };

    ~opengl_mesh() noexcept(false) {
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
    };
};

class ogl_shader_program {
    private:
        GLuint program_ID;

    public:
        // Compiles and links the provided shaders. Also registers them with OpenGL.
        // Throws on failure.
        ogl_shader_program( std::string vert_shader_src,
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
                    buf.resize((long int)(log_length + 1), '\0');
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
                    buf.resize((long int)(log_length + 1), '\0');
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
                    buf.resize((long int)(log_length + 1), '\0');
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

        // Unregisters shader program from OpenGL.
        ~ogl_shader_program(){
            glDeleteProgram( this->program_ID );
        }

        // Get the program ID for use in rendering.
        GLuint get_program_ID() const {
            return this->program_ID;
        }
};

static
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
        return std::move(l_custom_shader);
    }catch(const std::exception &e){
        shader_log = string_to_array(ss.str());
        throw;
    };
    return nullptr;
}


enum class brush_t {
    // 2D brushes.
    rigid_circle,
    rigid_square,
    gaussian_2D,
    tanh_2D,
    median_circle,
    median_square,
    mean_circle,
    mean_square,

    // 3D brushes.
    rigid_sphere,
    rigid_cube,
    gaussian_3D,
    tanh_3D,
    median_sphere,
    median_cube,
    mean_sphere,
    mean_cube,
};

void draw_with_brush( const decltype(planar_image_collection<float,double>().get_all_images()) &img_its,
                      const std::vector<line_segment<double>> &lss,
                      brush_t brush,
                      float radius,
                      float intensity,
                      long int channel,
                      float intensity_min = std::numeric_limits<float>::infinity() * -1.0,
                      float intensity_max = std::numeric_limits<float>::infinity() ){

    // Pre-extract the line segment vertices for bounding-box calculation.
    std::vector<vec3<double>> verts;
    for(const auto& l : lss){
        for(const auto &p : { l.Get_R0(), l.Get_R1() }){
            verts.emplace_back(p);
        }
    }
    double buffer_space = radius;
    if( (brush == brush_t::rigid_circle)
    ||  (brush == brush_t::rigid_square)
    ||  (brush == brush_t::median_circle)
    ||  (brush == brush_t::median_square)
    ||  (brush == brush_t::mean_circle)
    ||  (brush == brush_t::mean_square)
    ||  (brush == brush_t::rigid_sphere)
    ||  (brush == brush_t::rigid_cube)
    ||  (brush == brush_t::median_sphere)
    ||  (brush == brush_t::median_cube)
    ||  (brush == brush_t::mean_sphere)
    ||  (brush == brush_t::mean_cube) ){
        buffer_space = radius;

    }else if( (brush == brush_t::gaussian_2D)
    ||        (brush == brush_t::gaussian_3D) ){
        buffer_space = radius * 3.0;

    }else if( (brush == brush_t::tanh_2D)
    ||        (brush == brush_t::tanh_3D) ){
        buffer_space = radius * 1.5;
    }

    const auto apply_to_inner_pixels = [&](const decltype(planar_image_collection<float,double>().get_all_images()) &l_img_its,
                                           const std::function<float(const vec3<double> &, double, float)> &f){
        for(auto &cit : l_img_its){

            // Filter out irrelevant images.
            const auto img_is_relevant = [&]() -> bool {
                if( (cit->rows <= 0)
                ||  (cit->columns <= 0)
                ||  (cit->channels <= 0) ) return false;

                for(const auto& l : lss){
                    const auto plane_dist_R0 = cit->image_plane().Get_Signed_Distance_To_Point(l.Get_R0());
                    const auto plane_dist_R1 = cit->image_plane().Get_Signed_Distance_To_Point(l.Get_R1());

                    if( std::signbit(plane_dist_R0) != std::signbit(plane_dist_R1) ){
                        // Line segment crosses the image plane, so is automatically relevant.
                        return true;
                    }

                    // 2D brushes.
                    if( (brush == brush_t::rigid_circle)
                    ||  (brush == brush_t::rigid_square)
                    ||  (brush == brush_t::tanh_2D)
                    ||  (brush == brush_t::gaussian_2D)
                    ||  (brush == brush_t::median_circle)
                    ||  (brush == brush_t::median_square)
                    ||  (brush == brush_t::mean_circle)
                    ||  (brush == brush_t::mean_square) ){
                        if( (std::abs(plane_dist_R0) <= cit->pxl_dz * 0.5)
                        ||  (std::abs(plane_dist_R1) <= cit->pxl_dz * 0.5) ){
                            return true;
                        }

                    // 3D brushes.
                    }else if( (brush == brush_t::rigid_sphere)
                          ||  (brush == brush_t::rigid_cube) 
                          ||  (brush == brush_t::gaussian_3D)
                          ||  (brush == brush_t::tanh_3D)
                          ||  (brush == brush_t::median_sphere) 
                          ||  (brush == brush_t::median_cube)
                          ||  (brush == brush_t::mean_sphere) 
                          ||  (brush == brush_t::mean_cube) ){
                        if( (std::abs(plane_dist_R0) <= buffer_space)
                        ||  (std::abs(plane_dist_R1) <= buffer_space) ){
                            return true;
                        }
                    }
                }
                return false;
            };
            if(!img_is_relevant()) continue;

            // Compute pixel-space axis-aligned bounding box to reduce overall computation.
            //
            // Process relevant images.
            const auto [row_min, row_max, col_min, col_max] = get_pixelspace_axis_aligned_bounding_box(*cit, verts, buffer_space);
            for(long int r = row_min; r <= row_max; ++r){
                for(long int c = col_min; c <= col_max; ++c){
                    const auto pos = cit->position(r,c);
                    vec3<double> closest;
                    {
                        double closest_dist = 1E99;
                        for(const auto &l : lss){
                            const bool degenerate = ( (l.Get_R0()).sq_dist(l.Get_R1()) < 0.01 );
                            const auto closest_l = (degenerate) ? l.Get_R0() : l.Closest_Point_To(pos);
                            const auto dist = closest_l.distance(pos);
                            if(dist < closest_dist){
                                closest = closest_l;
                                closest_dist = dist;
                            }
                        }
                    }

                    const auto dR = closest.distance(pos);
                    if( (brush == brush_t::rigid_circle)
                    ||  (brush == brush_t::rigid_sphere)
                    ||  (brush == brush_t::median_circle)
                    ||  (brush == brush_t::mean_circle)
                    ||  (brush == brush_t::median_sphere)
                    ||  (brush == brush_t::mean_sphere)
                    ||  (brush == brush_t::tanh_2D)
                    ||  (brush == brush_t::gaussian_2D)
                    ||  (brush == brush_t::gaussian_3D) 
                    ||  (brush == brush_t::tanh_3D) ){
                        if(buffer_space < dR) continue;

                    }else if( (brush == brush_t::rigid_square)
                          ||  (brush == brush_t::median_square)
                          ||  (brush == brush_t::mean_square) ){
                        if( (buffer_space < std::abs((closest - pos).Dot(cit->row_unit)))
                        ||  (buffer_space < std::abs((closest - pos).Dot(cit->col_unit))) ) continue;

                    }else if( (brush == brush_t::median_cube)
                          ||  (brush == brush_t::rigid_cube)
                          ||  (brush == brush_t::mean_cube) ){
                        if( (buffer_space < std::abs((closest - pos).Dot(cit->row_unit)))
                        ||  (buffer_space < std::abs((closest - pos).Dot(cit->col_unit)))
                        ||  (buffer_space < std::abs((closest - pos).Dot(cit->row_unit.Cross(cit->col_unit)))) ) continue;

                        if(buffer_space < dR) continue;
                    }

                    cit->reference( r, c, channel ) = std::clamp(f(pos, dR, cit->value(r, c, channel)), intensity_min, intensity_max);
                }
            }
        }
    };


    // Implement brushes.
    if( (brush == brush_t::rigid_circle)
    ||  (brush == brush_t::rigid_square) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [intensity](const vec3<double> &, double, float) -> float {
                return intensity;
            });
        }

    }else if( (brush == brush_t::gaussian_2D)
          ||  (brush == brush_t::gaussian_3D) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [radius,intensity](const vec3<double> &, double dR, float v) -> float {
                const float scale = 0.5;
                const auto l_exp = std::exp( -std::pow(dR / (0.5 * radius), 2.0f) );
                return (intensity - v) * l_exp + v;
                //return v + intensity * std::exp( -std::pow(dR / (0.5 * radius), 2.0f) );
            });
        }

    }else if( (brush == brush_t::tanh_2D)
          ||  (brush == brush_t::tanh_3D) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [radius,intensity](const vec3<double> &, double dR, float v) -> float {
                const float steepness = 0.75;
                const auto l_tanh = 0.5 * (1.0 + std::tanh( steepness * (radius - dR)));
                return (intensity - v) * l_tanh + v;
                //return v + intensity * 0.5 * (1.0 + std::tanh( steepness * (radius - dR)));
            });
        }

    }else if( (brush == brush_t::median_circle)
          ||  (brush == brush_t::median_square) ){
        for(const auto &img_it : img_its){
            std::vector<float> vals;
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
            const auto median = Stats::Median(vals);
            apply_to_inner_pixels({img_it}, [median](const vec3<double> &, double, float) -> float {
                return median;
            });
        }

    }else if( (brush == brush_t::mean_circle)
          ||  (brush == brush_t::mean_square) ){
        for(const auto &img_it : img_its){
            std::vector<float> vals;
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
            const auto mean = Stats::Mean(vals);
            apply_to_inner_pixels({img_it}, [mean](const vec3<double> &, double, float) -> float {
                return mean;
            });
        }

    }else if( (brush == brush_t::rigid_sphere)
          ||  (brush == brush_t::rigid_cube) ){
        apply_to_inner_pixels(img_its, [intensity](const vec3<double> &, double, float) -> float {
            return intensity;
        });

    }else if( (brush == brush_t::median_sphere)
          ||  (brush == brush_t::median_cube) ){
        std::vector<float> vals;
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
        }
        const auto median = Stats::Median(vals);
        apply_to_inner_pixels(img_its, [median](const vec3<double> &, double, float) -> float {
            return median;
        });

    }else if( (brush == brush_t::mean_sphere)
          ||  (brush == brush_t::mean_cube) ){
        std::vector<float> vals;
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [&vals](const vec3<double> &, double, float v) -> float {
                vals.emplace_back(v);
                return v;
            });
        }
        const auto mean = Stats::Mean(vals);
        apply_to_inner_pixels(img_its, [mean](const vec3<double> &, double, float) -> float {
            return mean;
        });
    }

    return;
}

OperationDoc OpArgDocSDL_Viewer(){
    OperationDoc out;
    out.name = "SDL_Viewer";
    out.desc = 
        "Launch an interactive viewer based on SDL.";

    return out;
}

bool SDL_Viewer(Drover &DICOM_data,
                  const OperationArgPkg& /*OptArgs*/,
                  std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex){

    // --------------------------------------- Operational State ------------------------------------------
    std::shared_timed_mutex drover_mutex;
    const auto mutex_dt = std::chrono::microseconds(5);

    // General-purpose Drover processing offloading worker thread.
    work_queue<std::function<void(void)>> wq;
    wq.submit_task([](){
        FUNCINFO("Worker thread ready");
        return;
    });

    Explicator X(FilenameLex);

    struct View_Toggles {
        bool set_about_popup = false;
        bool view_imgui_demo = false;
        bool view_implot_demo = false;
        bool view_metrics_window = false;
        bool open_files_enabled = false;
        bool view_images_enabled = true;
        bool view_image_metadata_enabled = false;
        bool view_meshes_enabled = true;
        bool view_mesh_metadata_enabled = false;
        bool view_plots_enabled = true;
        bool view_plots_metadata = true;
        bool view_contours_enabled = true;
        bool view_contouring_enabled = false;
        bool view_contouring_debug = false;
        bool view_drawing_enabled = false;
        bool view_row_column_profiles = false;
        bool view_time_profiles = false;
        bool view_parameter_table = false;
        bool view_tables_enabled = true;
        bool view_table_metadata_enabled = false;
        bool save_time_profiles = false;
        bool view_script_editor_enabled = false;
        bool view_script_feedback = true;
        bool show_image_hover_tooltips = true;
        bool adjust_window_level_enabled = false;
        bool adjust_colour_map_enabled = false;
        bool view_shader_editor_enabled = false;
    } view_toggles;

    // Plot viewer state.
    std::map<long int, bool> lsamps_visible;
    enum class plot_norm_t {
        none,
        max,
    } plot_norm = plot_norm_t::none;
    bool show_plot_legend = true;

    // Image viewer state.
    long int img_array_num = -1; // The image array currently displayed.
    long int img_num = -1; // The image currently displayed.
    long int img_channel = -1; // Which channel to display.
    using img_array_ptr_it_t = decltype(DICOM_data.image_data.begin());
    using disp_img_it_t = decltype(DICOM_data.image_data.front()->imagecoll.images.begin());
    bool img_precess = false;
    float img_precess_period = 0.1f; // in seconds.
    std::chrono::time_point<std::chrono::steady_clock> img_precess_last = std::chrono::steady_clock::now();

    //Real-time modifiable sticky window and level.
    std::optional<double> custom_width;
    std::optional<double> custom_centre;
    std::optional<double> custom_low;
    std::optional<double> custom_high;

    //A tagged point for measuring distance.
    std::optional<vec3<double>> tagged_pos;

    //Load available colour maps.
    std::vector< std::pair<std::string, std::function<struct ClampedColourRGB(double)>>> colour_maps = {
        std::make_pair("Viridis", ColourMap_Viridis),
        std::make_pair("Magma", ColourMap_Magma),
        std::make_pair("Plasma", ColourMap_Plasma),
        std::make_pair("Inferno", ColourMap_Inferno),

        std::make_pair("Jet", ColourMap_Jet),

        std::make_pair("MorelandBlueRed", ColourMap_MorelandBlueRed),

        std::make_pair("MorelandBlackBody", ColourMap_MorelandBlackBody),
        std::make_pair("MorelandExtendedBlackBody", ColourMap_MorelandExtendedBlackBody),
        std::make_pair("KRC", ColourMap_KRC),
        std::make_pair("ExtendedKRC", ColourMap_ExtendedKRC),

        std::make_pair("Kovesi_LinKRYW_5-100_c64", ColourMap_Kovesi_LinKRYW_5_100_c64),
        std::make_pair("Kovesi_LinKRYW_0-100_c71", ColourMap_Kovesi_LinKRYW_0_100_c71),

        std::make_pair("Kovesi_Cyclic_cet-c2", ColourMap_Kovesi_Cyclic_mygbm_30_95_c78),

        std::make_pair("LANLOliveGreentoBlue", ColourMap_LANL_OliveGreen_to_Blue),

        std::make_pair("YgorIncandescent", ColourMap_YgorIncandescent),

        std::make_pair("LinearRamp", ColourMap_Linear),

        std::make_pair("Composite_50_90_107_110", ColourMap_Composite_50_90_107_110),
        std::make_pair("Composite_50_90_100_107_110", ColourMap_Composite_50_90_100_107_110)
    };
    size_t colour_map = 0;

    const auto nan_colour = std::array<std::byte, 3>{ std::byte{60}, std::byte{0}, std::byte{0} }; // 8-bit colour.

    auto pos_contour_colour = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    auto neg_contour_colour = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    auto editing_contour_colour = ImVec4(1.0f, 0.45f, 0.0f, 1.0f);

    auto line_numbers_normal_colour = ImVec4(1.0f, 1.0f, 1.0f, 0.3f);
    auto line_numbers_debug_colour  = ImVec4(0.4f, 1.0f, 0.4f, 0.8f);
    auto line_numbers_info_colour   = ImVec4(0.4f, 0.4f, 1.0f, 0.7f);
    auto line_numbers_warn_colour   = ImVec4(0.7f, 0.5f, 0.1f, 0.8f);
    auto line_numbers_error_colour  = ImVec4(1.0f, 0.1f, 0.1f, 0.8f);
    
    const auto get_unique_colour = [](long int i){
        const std::vector<vec3<double>> colours = {
            vec3<double>(1.000,0.702,0.000),   // "vivid_yellow".
            vec3<double>(0.502,0.243,0.459),   // "strong_purple".
            vec3<double>(1.000,0.408,0.000),   // "vivid_orange".
            vec3<double>(0.651,0.741,0.843),   // "very_light_blue".
            vec3<double>(0.757,0.000,0.125),   // "vivid_red".
            vec3<double>(0.808,0.635,0.384),   // "grayish_yellow".
            vec3<double>(0.506,0.439,0.400),   // "medium_gray".
            vec3<double>(0.000,0.490,0.204),   // "vivid_green".
            vec3<double>(0.965,0.463,0.557),   // "strong_purplish_pink".
            vec3<double>(0.000,0.325,0.541),   // "strong_blue".
            vec3<double>(1.000,0.478,0.361),   // "strong_yellowish_pink".
            vec3<double>(0.325,0.216,0.478),   // "strong_violet".
            vec3<double>(1.000,0.557,0.000),   // "vivid_orange_yellow".
            vec3<double>(0.702,0.157,0.318),   // "strong_purplish_red".
            vec3<double>(0.957,0.784,0.000),   // "vivid_greenish_yellow".
            vec3<double>(0.498,0.094,0.051),   // "strong_reddish_brown".
            vec3<double>(0.576,0.667,0.000),   // "vivid_yellowish_green".
            vec3<double>(0.349,0.200,0.082),   // "deep_yellowish_brown".
            vec3<double>(0.945,0.227,0.075),   // "vivid_reddish_orange".
            vec3<double>(0.137,0.173,0.086) }; // "dark_olive_green".
        const auto colour = colours[ i % colours.size() ];
        return ImVec4(colour.x, colour.y, colour.z, 1.0f);
    };

    // Meshes.
    using disp_mesh_it_t = decltype(DICOM_data.smesh_data.begin());
    std::unique_ptr<opengl_mesh> oglm_ptr;
    long int mesh_num = -1;
    std::atomic<bool> need_to_reload_opengl_mesh = true;

    struct mesh_display_transform_t {
        // Viewing options.
        bool render_wireframe = true;
        bool reverse_normals = false;
        bool use_lighting = true;
        bool use_opaque = false;
        bool use_smoothing = true;

        // Camera transformations.
        bool precess = true;
        double precess_rate = 1.0;
        double rot_x = 0.0;
        double rot_y = 0.0;

        double zoom = 1.0;
        double cam_distort = 0.0;

        // Transformations applied to all models.
        num_array<float> model = num_array<float>().identity(4);

        // Nominal colours.
        std::array<float, 4> colours = { 1.000, 0.588, 0.005, 0.8 };
    } mesh_display_transform;

    // Tables.
    using disp_table_it_t = decltype(DICOM_data.table_data.begin());
    long int table_num = -1;

    // ------------------------------------------ Viewer State --------------------------------------------
    auto background_colour = ImVec4(0.025f, 0.087f, 0.118f, 1.00f);

    struct image_mouse_pos_s {
        bool mouse_hovering_image;
        bool image_window_focused;

        float region_x;   // [0,1] clamped position of mouse on image.
        float region_y;

        long int r; // Row and column number of current mouse position.
        long int c;

        vec3<double> zero_pos;  // Position of (0,0) voxel in DICOM coordinate system.
        vec3<double> dicom_pos; // Position of mouse in DICOM coordinate system.
        vec3<double> voxel_pos; // Position of voxel being hovered in DICOM coordinate system.

        float pixel_scale; // Conversion factor from DICOM distance to screen pixels.

        std::function<ImVec2(const vec3<double>&)> DICOM_to_pixels; 
    };
    std::optional<image_mouse_pos_s> image_mouse_pos_opt;

    samples_1D<double> row_profile;
    samples_1D<double> col_profile;
    samples_1D<double> time_profile;

    enum class time_course_image_inclusivity_t {
        current,  // Spatially overlapping pixels from within only the current image array.
        all,      // Spatially overlapping pixels from within any image array.
    } time_course_image_inclusivity = time_course_image_inclusivity_t::current;
    bool time_course_abscissa_relative = false;
    std::array<char, 2048> time_course_abscissa_key;
    string_to_array(time_course_abscissa_key, "ContentTime");
    std::array<char, 2048> time_course_text_entry;
    string_to_array(time_course_text_entry, "");

    // --------------------------------------------- Setup ------------------------------------------------
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0){
        throw std::runtime_error("Unable to initialize SDL: "_s + SDL_GetError());
    }

    // Configure the desired OpenGL version (v3.0).
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_FLAGS: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_PROFILE_MASK: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_MAJOR_VERSION: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_MINOR_VERSION: "_s + SDL_GetError());
    }

    // Create an SDL window and provide a context we can refer to.
    if(0 != SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)){
        throw std::runtime_error("Unable to set SDL_GL_DEPTH_SIZE: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)){
        throw std::runtime_error("Unable to set SDL_GL_DOUBLEBUFFER: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8)){
        throw std::runtime_error("Unable to set SDL_GL_STENCIL_SIZE: "_s + SDL_GetError());
    }
    SDL_Window* window = SDL_CreateWindow("DICOMautomaton Interactive Workspace",
                                           SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           1280, 768,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
    if(window == nullptr){
        throw std::runtime_error("Unable to create an SDL window: "_s + SDL_GetError());
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if(gl_context == nullptr){
        throw std::runtime_error("Unable to create an OpenGL context for SDL: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_MakeCurrent(window, gl_context)){
        throw std::runtime_error("Unable to associate OpenGL context with SDL window: "_s + SDL_GetError());
    }
    if(SDL_GL_SetSwapInterval(-1) != 0){ // Enable adaptive vsync to limit the frame rate.
        if(SDL_GL_SetSwapInterval(1) != 0){ // Enable vsync (non-adaptive).
            FUNCWARN("Unable to enable vsync. Continuing without it");
        }
    }

    glewExperimental = true; // Bug fix for glew v1.13.0 and earlier.
    if(glewInit() != GLEW_OK){
        throw std::runtime_error("Glew was unable to initialize OpenGL");
    }
    try{
        CHECK_FOR_GL_ERRORS(); // Clear any errors encountered during glewInit.
    }catch(const std::exception &e){
        FUNCINFO("Ignoring glew-related error: " << e.what());
    }

    // Create an ImGui context we can use and associate it with the OpenGL context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    [[maybe_unused]] ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    CHECK_FOR_GL_ERRORS();
    if(!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)){
        throw std::runtime_error("ImGui unable to associate SDL window with OpenGL context.");
    }
    CHECK_FOR_GL_ERRORS();
    if(!ImGui_ImplOpenGL3_Init()){
        throw std::runtime_error("ImGui unable to initialize OpenGL with default shader.");
    }
    CHECK_FOR_GL_ERRORS();
    std::string gl_version;
    std::string glsl_version;
    try{
        auto l_gl_version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        auto l_glsl_version = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        if(l_gl_version == nullptr) throw std::runtime_error("OpenGL version not accessible.");
        if(l_glsl_version == nullptr) throw std::runtime_error("GLSL version not accessible.");
        gl_version = std::string(l_gl_version);

        // The string can often have extra characters and punctuation. The standard guarantees a space separates
        // components, but version numbers may still be present.
        auto version_vec = SplitStringToVector(std::string(l_glsl_version), " ", 'd');
        glsl_version = (version_vec.empty()) ? "" : version_vec.front();

        glsl_version.erase( std::remove_if( std::begin(glsl_version),
                                            std::end(glsl_version),
                                            [](unsigned char c){ return !std::isdigit(c); } ),
                            std::end(glsl_version) );

        FUNCINFO("Initialized OpenGL '" << gl_version << "' with GLSL '" << glsl_version << "'");
    }catch(const std::exception &e){
        FUNCWARN("Unable to detect OpenGL/GLSL version");
    }

    // ------------------------------------------ Shaders -------------------------------------------------

    std::array<char, 2048> vert_shader_src = string_to_array(
        "#version " + glsl_version + "\n"
        "\n"
        "in vec3 v_pos;\n"
        "in vec3 v_norm;\n"
        "\n"
        "uniform mat4 mvp_matrix;      // model-view-projection matrix.\n"
        "uniform mat4 mv_matrix;       // model-view matrix.\n"
        "uniform mat3 norm_matrix;     // rotation-only matrix.\n"
        "\n"
        "uniform vec4 diffuse_colour;\n"
        "uniform vec4 user_colour;\n"
        "uniform vec3 light_position;\n"
        "uniform bool use_lighting;\n"
        "uniform bool use_smoothing;\n"
        "\n"
        "out vec4 interp_colour;\n"
        "flat out vec4 flat_colour;\n"
        "\n"
        "void main(){\n"
        "    gl_Position = mvp_matrix * vec4(v_pos, 1.0);\n"
        "\n"
        "    if(use_lighting){\n"
        "        vec3 l_norm = normalize(norm_matrix * v_norm);\n"
        "\n"
        "        vec4 l_pos4 = mv_matrix * vec4(v_pos, 1.0);\n"
        "        vec3 l_pos3 = l_pos4.xyz / l_pos4.w;\n"
        "\n"
        "        vec3 l_light_pos = vec3(-1000.0, -1000.0, 250.0);\n"
        "        vec3 light_dir = normalize( l_light_pos - l_pos3 );\n"
        "\n"
        "        float diffuse_intensity = max(0.0, 1.0 + 0.5*dot(l_norm, light_dir));\n"
        "\n"
        "        interp_colour.rgb = diffuse_intensity * diffuse_colour.rgb;\n"
        "        //interp_colour.a = 1.0;\n"
        "        interp_colour.a = user_colour.a;\n"
        "    }else{\n"
        "        interp_colour = user_colour;\n"
        "    }\n"
        "    flat_colour = interp_colour;\n"
        "}\n" );

    std::array<char, 2048> frag_shader_src = string_to_array(
        "#version " + glsl_version + "\n"
        "\n"
        "in vec4 interp_colour;\n"
        "flat in vec4 flat_colour;\n"
        "\n"
        "uniform vec4 user_colour;\n"
        "uniform bool use_lighting;\n"
        "uniform bool use_smoothing;\n"
        "\n"
        "out vec4 frag_colour;\n"
        "\n"
        "void main(){\n"
        "    frag_colour = 0.65 * (use_smoothing ? interp_colour : flat_colour)\n"
        "                + 0.35 * user_colour;\n"
        "}\n" );

    std::array<char, 2048> shader_log; // Output from most recent compilation and linking.

    //FUNCINFO("Using default vertex shader source: '" << array_to_string(vert_shader_src) << "'");
    //FUNCINFO("Using default fragment shader source: '" << array_to_string(frag_shader_src) << "'");

    // Note: the following will throw if the default shader fails to compile and link.
    auto custom_shader = compile_shader_program(vert_shader_src, frag_shader_src, shader_log);

    // -------------------------------- Functors for various things ---------------------------------------

    // Create an OpenGL texture from an image.
    struct opengl_texture_handle_t {
        GLuint texture_number = 0;
        long int col_count = 0L;
        long int row_count = 0L;
        float aspect_ratio = 1.0; // In image pixel space.
        bool texture_exists = false;

/*
        ~opengl_texture_handle_t(){
            // Release the previous texture, iff needed.
            if(this->texture_number != 0){
                CHECK_FOR_GL_ERRORS();
                glBindTexture(GL_TEXTURE_2D, 0);
                glDeleteTextures(1, &this->texture_number);
                CHECK_FOR_GL_ERRORS();
            }
        }
*/
    };
    opengl_texture_handle_t current_texture;

    // Scale bar for showing current colour map.
    vec3<double> zero3(0.0, 0.0, 0.0);
    planar_image<float,double> scale_bar_img;
    scale_bar_img.init_buffer(1L, 100L, 1L);
    scale_bar_img.init_spatial(1.0, 1.0, 1.0, zero3, zero3);
    scale_bar_img.init_orientation(vec3<double>(0.0, 1.0, 0.0), vec3<double>(1.0, 0.0, 0.0));
    for(long int c = 0; c < scale_bar_img.columns; ++c){
        scale_bar_img.reference(0,c,0) = static_cast<float>(c) / static_cast<float>(scale_bar_img.columns-1);
    }
    opengl_texture_handle_t scale_bar_texture;

    // Contouring mode state.
    opengl_texture_handle_t contouring_texture;
    int contouring_img_row_col_count = 256;
    bool contouring_img_altered = false;
    float contouring_reach = 10.0;
    float contouring_margin = 1.0;
    float contouring_intensity = 1.0;
    std::string contouring_method = "marching-squares";
    brush_t contouring_brush = brush_t::rigid_circle;
    float last_mouse_button_0_down = 1E30;
    float last_mouse_button_1_down = 1E30;
    std::optional<vec3<double>> last_mouse_button_pos;

    Drover contouring_imgs;
    contouring_imgs.Ensure_Contour_Data_Allocated();
    contouring_imgs.image_data.push_back(std::make_unique<Image_Array>());
    contouring_imgs.image_data.back()->imagecoll.images.emplace_back();
    std::string new_contour_name(500, '\0');


    // Resets the contouring image to match the display image characteristics.
    const auto reset_contouring_state = [&contouring_imgs,
                                         &contouring_img_row_col_count]( img_array_ptr_it_t  dimg_array_ptr_it ) -> void {
            contouring_img_row_col_count = std::clamp(contouring_img_row_col_count, 5, 1024);

            // Reset the contouring images.
            contouring_imgs.image_data.back()->imagecoll.images.clear();
            for(const auto& dimg : (*dimg_array_ptr_it)->imagecoll.images){
                if((dimg.rows < 1) || (dimg.columns < 1)) continue;

                // Only add this slice if it fails to overlap spatially with any existing images.
                //const auto ortho_offset = dimg.row_unit.Cross( dimg.col_unit ).unit() * dimg.pxl_dz * 0.25;
                //const auto centre = dimg.center();
                //const std::list<vec3<double>> points = { centre,
                //                                         centre + ortho_offset,
                //                                         centre - ortho_offset,
                //                                         dimg.position(0,0),
                //                                         dimg.position(0,0) + ortho_offset,
                //                                         dimg.position(0,0) - ortho_offset,
                //                                         dimg.position(dimg.rows-1,0),
                //                                         dimg.position(dimg.rows-1,0) + ortho_offset,
                //                                         dimg.position(dimg.rows-1,0) - ortho_offset,
                //                                         dimg.position(dimg.rows-1,dimg.columns-1),
                //                                         dimg.position(dimg.rows-1,dimg.columns-1) + ortho_offset,
                //                                         dimg.position(dimg.rows-1,dimg.columns-1) - ortho_offset,
                //                                         dimg.position(0,dimg.columns-1),
                //                                         dimg.position(0,dimg.columns-1) + ortho_offset,
                //                                         dimg.position(0,dimg.columns-1) - ortho_offset };
                //const auto encompassing_images = contouring_imgs.image_data.back()->imagecoll.get_images_which_encompass_all_points(points);
                //if(!encompassing_images.empty()) continue;

                const auto centre = dimg.center();
                const auto A_corners = dimg.corners2D();
                auto encompassing_images = contouring_imgs.image_data.back()->imagecoll.get_images_which_sandwich_point_within_top_bottom_planes( centre );
                encompassing_images.remove_if([&](const decltype(contouring_imgs.image_data.back()->imagecoll.get_all_images().front()) &img_it){
                    const auto B_corners = img_it->corners2D();

                    //Fixed corner-to-corner distance.
                    double dist = 0.0;
                    for(auto A_it = std::begin(A_corners), B_it = std::begin(B_corners);
                        (A_it != std::end(A_corners)) && (B_it != std::end(B_corners)); ){
                        dist += A_it->sq_dist(*B_it);
                        ++A_it;
                        ++B_it;
                    }
                    return (std::min(dimg.pxl_dx, dimg.pxl_dy) < dist);
                });
                if(!encompassing_images.empty()) continue;

                // Add this image to the list of spatially-distinct images.
                contouring_imgs.image_data.back()->imagecoll.images.emplace_back();
                const auto cimg_ptr = &(contouring_imgs.image_data.back()->imagecoll.images.back());

                // Make the contouring image spatial extent match the display image, except with a different number of
                // rows and columns. This will make it easy to translate contours back and forth.
                const auto cimg_pxl_dx = dimg.pxl_dx * static_cast<float>(dimg.rows)/static_cast<float>(contouring_img_row_col_count);
                const auto cimg_pxl_dy = dimg.pxl_dy * static_cast<float>(dimg.columns)/static_cast<float>(contouring_img_row_col_count);
                const auto cimg_offset = dimg.offset - dimg.row_unit * dimg.pxl_dx * 0.5
                                                     - dimg.col_unit * dimg.pxl_dy * 0.5
                                                     + dimg.row_unit * cimg_pxl_dx * 0.5
                                                     + dimg.col_unit * cimg_pxl_dy * 0.5;
                cimg_ptr->init_buffer(contouring_img_row_col_count, contouring_img_row_col_count, 1L);
                cimg_ptr->init_spatial(cimg_pxl_dx, cimg_pxl_dy, dimg.pxl_dz, dimg.anchor, cimg_offset);
                cimg_ptr->init_orientation(dimg.row_unit, dimg.col_unit);
                cimg_ptr->fill_pixels(-1.0f);
            }

            // Reset any existing contours.
            contouring_imgs.Ensure_Contour_Data_Allocated();
            contouring_imgs.contour_data->ccs.clear();
            FUNCINFO("Reset contouring state with " << contouring_imgs.image_data.back()->imagecoll.images.size() << " images");

            return;
    };

    const auto Free_OpenGL_Texture = [](opengl_texture_handle_t &tex){
        // Release the previous texture, iff needed.
        if(tex.texture_number != 0){
            CHECK_FOR_GL_ERRORS();
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &tex.texture_number);
            CHECK_FOR_GL_ERRORS();
        }

        // Reset all other state.
        tex = opengl_texture_handle_t();
        return;
    };

    std::atomic<bool> need_to_reload_opengl_texture = true;
    const auto Load_OpenGL_Texture = [&colour_maps,
                                      &colour_map,
                                      &nan_colour,
                                      &img_channel ]( const planar_image<float,double>& img,
                                                      const std::optional<double>& custom_centre,
                                                      const std::optional<double>& custom_width ) -> opengl_texture_handle_t {

            const auto img_cols = img.columns;
            const auto img_rows = img.rows;
            const auto img_chns = img.channels;

            if(!isininc(1,img_rows,10000) || !isininc(1,img_cols,10000)){
                throw std::invalid_argument("Image dimensions are not reasonable. Refusing to continue");
            }
            if(!isininc(1,img_channel+1,img_chns)){
                throw std::invalid_argument("Image does not have selected channel. Refusing to continue");
            }

            std::vector<std::byte> animage;
            animage.reserve(img_cols * img_rows * 3);

            //------------------------------------------------------------------------------------------------
            //Apply a window to the data if it seems like the WindowCenter or WindowWidth specified in the image metadata
            // are applicable. Note that it is likely that pixels will be clipped or truncated. This is intentional.
            
            auto img_win_valid = img.GetMetadataValueAs<std::string>("WindowValidFor");
            auto img_desc      = img.GetMetadataValueAs<std::string>("Description");
            auto img_win_c     = img.GetMetadataValueAs<double>("WindowCenter");
            auto img_win_fw    = img.GetMetadataValueAs<double>("WindowWidth"); //Full width or range. (Diameter, not radius.)

            auto custom_win_c  = custom_centre; 
            auto custom_win_fw = custom_width; 

            const auto UseCustomWL = (custom_win_c && custom_win_fw);
            const auto UseImgWL = (UseCustomWL) ? false 
                                                : (   (img_chns == 1) // Only honour for single-channel images.
                                                   && img_win_valid 
                                                   && img_desc
                                                   && img_win_c 
                                                   && img_win_fw 
                                                   && (img_win_valid.value() == img_desc.value()));

            if( UseCustomWL || UseImgWL ){
                //Window/linear scaling transformation parameters.
                //const auto win_r = 0.5*(win_fw.value() - 1.0); //The 'radius' of the range, or half width omitting the centre point.

                //The 'radius' of the range, or half width omitting the centre point.
                const auto win_r  = (UseCustomWL) ? 0.5*custom_win_fw.value()
                                                  : 0.5*img_win_fw.value();
                const auto win_c  = (UseCustomWL) ? custom_win_c.value()
                                                  : img_win_c.value();
                const auto win_fw = (UseCustomWL) ? custom_win_fw.value()
                                                  : img_win_fw.value();

                //The output range we are targeting. In this case, a commodity 8 bit (2^8 = 256 intensities) display.
                const auto destmin = static_cast<double>( 0 );
                const auto destmax = static_cast<double>( std::numeric_limits<uint8_t>::max() );

                for(auto j = 0; j < img_rows; ++j){
                    for(auto i = 0; i < img_cols; ++i){
                        const auto val = static_cast<double>( img.value(j,i,img_channel) ); //The first (R or gray) channel.
                        if(!std::isfinite(val)){
                            animage.push_back( nan_colour[0] );
                            animage.push_back( nan_colour[1] );
                            animage.push_back( nan_colour[2] );
                        }else{
                            double x; // range = [0,1].
                            if(val <= (win_c - win_r)){
                                x = 0.0;
                            }else if(val >= (win_c + win_r)){
                                x = 1.0;
                            }else{
                                x = (val - (win_c - win_r)) / win_fw;
                            }

                            const auto res = colour_maps[colour_map].second(x);
                            const double x_R = res.R;
                            const double x_G = res.G;
                            const double x_B = res.B;

                            const double out_R = x_R * (destmax - destmin) + destmin;
                            const double out_B = x_B * (destmax - destmin) + destmin;
                            const double out_G = x_G * (destmax - destmin) + destmin;

                            const auto scaled_R = static_cast<uint8_t>( std::floor(out_R) );
                            const auto scaled_G = static_cast<uint8_t>( std::floor(out_G) );
                            const auto scaled_B = static_cast<uint8_t>( std::floor(out_B) );

                            animage.push_back( std::byte{scaled_R} );
                            animage.push_back( std::byte{scaled_G} );
                            animage.push_back( std::byte{scaled_B} );
                        }
                    }
                }

            //------------------------------------------------------------------------------------------------
            //Scale pixels to fill the maximum range. None will be clipped or truncated.
            }else{
                //Due to a strange dependence on windowing, some manufacturers spit out massive pixel values.
                // If you don't want to window you need to anticipate and ignore the gigantic numbers being 
                // you might encounter. This is not the place to do this! If you need to do it here, write a
                // filter routine and *call* it from here.
                //
                // NOTE: This routine could definitely use a re-working, especially to make it safe for all
                //       arithmetical types (i.e., handling negatives, ensuring there is no overflow or wrap-
                //       around, ensuring there is minimal precision loss).
                using pixel_value_t = decltype(img.value(0, 0, 0));
                Stats::Running_MinMax<pixel_value_t> rmm;
                img.apply_to_pixels([&rmm,&img_channel](long int /*row*/,
                                                        long int /*col*/,
                                                        long int chnl,
                                                        pixel_value_t val) -> void {
                    if( (img_channel < 0)
                    ||  (chnl == img_channel) ) rmm.Digest(val);
                    return;
                });
                const auto lowest = rmm.Current_Min();
                const auto highest = rmm.Current_Max();

                //const auto lowest = Stats::Percentile(img.data, 0.01);
                //const auto highest = Stats::Percentile(img.data, 0.99);

                const auto pixel_type_max = static_cast<double>(std::numeric_limits<pixel_value_t>::max());
                const auto pixel_type_min = static_cast<double>(std::numeric_limits<pixel_value_t>::min());
                const auto dest_type_max = static_cast<double>(std::numeric_limits<uint8_t>::max()); //Min is implicitly 0.

                const double clamped_low  = static_cast<double>(lowest )/pixel_type_max;
                const double clamped_high = static_cast<double>(highest)/pixel_type_max;

                for(auto j = 0; j < img_rows; ++j){ 
                    for(auto i = 0; i < img_cols; ++i){ 
                        const auto val = img.value(j,i,img_channel);
                        if(!std::isfinite(val)){
                            animage.push_back( nan_colour[0] );
                            animage.push_back( nan_colour[1] );
                            animage.push_back( nan_colour[2] );
                        }else{
                            const double clamped_value = (static_cast<double>(val) - pixel_type_min)/(pixel_type_max - pixel_type_min);
                            auto rescaled_value = (clamped_value - clamped_low)/(clamped_high - clamped_low);
                            if( rescaled_value < 0.0 ){
                                rescaled_value = 0.0;
                            }else if( rescaled_value > 1.0 ){
                                rescaled_value = 1.0;
                            }

                            const auto res = colour_maps[colour_map].second(rescaled_value);
                            const double x_R = res.R;
                            const double x_G = res.G;
                            const double x_B = res.B;
                            
                            const auto scaled_R = static_cast<uint8_t>(x_R * dest_type_max);
                            const auto scaled_G = static_cast<uint8_t>(x_G * dest_type_max);
                            const auto scaled_B = static_cast<uint8_t>(x_B * dest_type_max);

                            animage.push_back( std::byte{scaled_R} );
                            animage.push_back( std::byte{scaled_G} );
                            animage.push_back( std::byte{scaled_B} );
                        }
                    }
                }
            }
        
            opengl_texture_handle_t out;
            out.col_count = img_cols;
            out.row_count = img_rows;
            out.aspect_ratio = (img.pxl_dx / img.pxl_dy) * (static_cast<float>(img_rows) / static_cast<float>(img_cols));
            out.aspect_ratio = (img.pxl_dx / img.pxl_dy) * (static_cast<float>(img_rows) / static_cast<float>(img_cols));
            out.aspect_ratio = std::isfinite(out.aspect_ratio) ? out.aspect_ratio : (img.pxl_dx / img.pxl_dy);

            CHECK_FOR_GL_ERRORS();

            glGenTextures(1, &out.texture_number);
            glBindTexture(GL_TEXTURE_2D, out.texture_number);
            if(out.texture_number == 0){
                throw std::runtime_error("Unable to assign OpenGL texture");
            }
            CHECK_FOR_GL_ERRORS();


            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            CHECK_FOR_GL_ERRORS();

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                         static_cast<int>(out.col_count), static_cast<int>(out.row_count),
                         0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<void*>(animage.data()));
            CHECK_FOR_GL_ERRORS();

            out.texture_exists = true;
            return out;
    };


    // Recompute image array and image iterators for the current image.
    const auto recompute_image_iters = [ &DICOM_data,
                                         &img_array_num,
                                         &img_num ](){
        std::tuple<bool, img_array_ptr_it_t, disp_img_it_t > out;
        std::get<bool>( out ) = false;

        // Set the current image array and image iters and load the texture.
        do{ 
            const auto has_images = DICOM_data.Has_Image_Data();
            if( !has_images ) break;
            if( !isininc(1, img_array_num+1, DICOM_data.image_data.size()) ) break;
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            if( img_array_ptr_it == std::end(DICOM_data.image_data) ) break;

            if( !isininc(1, img_num+1, (*img_array_ptr_it)->imagecoll.images.size()) ) break;
            auto disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);
            if( disp_img_it == std::end((*img_array_ptr_it)->imagecoll.images) ) break;

            if( (disp_img_it->channels <= 0)
            ||  (disp_img_it->rows <= 0)
            ||  (disp_img_it->columns <= 0) ) break;

            std::get<bool>( out ) = true;
            std::get<img_array_ptr_it_t>( out ) = img_array_ptr_it;
            std::get<disp_img_it_t>( out ) = disp_img_it;
        }while(false);
        return out;
    };

    // Recompute image array and image iterators for the current contouring image.
    const auto recompute_cimage_iters = [ &contouring_imgs,
                                          &recompute_image_iters ](){
        std::tuple<bool, img_array_ptr_it_t, disp_img_it_t > out;
        std::get<bool>( out ) = false;
        const long int cimg_array_num = 0; // (Currently only support one image array, but would be useful to support multiple...)

        // Set the current image array and image iters and load the texture.
        const auto has_cimages = contouring_imgs.Has_Image_Data();
        auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
        do{ 
            if( !has_cimages ) break;
            if( !img_valid ) break;
            if(  contouring_imgs.image_data.size() != 1 ) throw std::logic_error("Multiple contouring image arrays not supported");
            if( !isininc(1, cimg_array_num+1, contouring_imgs.image_data.size()) ) break;
            auto cimg_array_ptr_it = std::next(contouring_imgs.image_data.begin(), cimg_array_num);
            if( cimg_array_ptr_it == std::end(contouring_imgs.image_data) ) break;

            if( (disp_img_it->channels <= 0)
            ||  (disp_img_it->rows <= 0)
            ||  (disp_img_it->columns <= 0) ) break;

            if( ((*cimg_array_ptr_it)->imagecoll.images.front().rows <= 0)
            ||  ((*cimg_array_ptr_it)->imagecoll.images.front().columns <= 0)
            ||  ((*cimg_array_ptr_it)->imagecoll.images.front().channels <= 0) ) break;

            // Find the spatially-overlapping image.
            //const auto ortho_offset = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit() * disp_img_it->pxl_dz * 0.25;
            //const std::list<vec3<double>> points = { centre,
            //                                         centre + ortho_offset,
            //                                         centre - ortho_offset,
            //                                         disp_img_it->position(0,0),
            //                                         disp_img_it->position(0,0) + ortho_offset,
            //                                         disp_img_it->position(0,0) - ortho_offset,
            //                                         disp_img_it->position(disp_img_it->rows-1,0),
            //                                         disp_img_it->position(disp_img_it->rows-1,0) + ortho_offset,
            //                                         disp_img_it->position(disp_img_it->rows-1,0) - ortho_offset,
            //                                         disp_img_it->position(disp_img_it->rows-1,disp_img_it->columns-1),
            //                                         disp_img_it->position(disp_img_it->rows-1,disp_img_it->columns-1) + ortho_offset,
            //                                         disp_img_it->position(disp_img_it->rows-1,disp_img_it->columns-1) - ortho_offset,
            //                                         disp_img_it->position(0,disp_img_it->columns-1),
            //                                         disp_img_it->position(0,disp_img_it->columns-1) + ortho_offset,
            //                                         disp_img_it->position(0,disp_img_it->columns-1) - ortho_offset };
            //auto encompassing_images = (*cimg_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

            //auto encompassing_images = (*cimg_array_ptr_it)->imagecoll.get_images_which_encompass_point( disp_img_it->center() );

            try{
                const auto centre = disp_img_it->center();
                const auto A_corners = disp_img_it->corners2D();
                auto encompassing_images = (*cimg_array_ptr_it)->imagecoll.get_images_which_sandwich_point_within_top_bottom_planes( centre );
                encompassing_images.remove_if([&](const decltype((*cimg_array_ptr_it)->imagecoll.get_all_images().front()) &img_it){
                    const auto B_corners = img_it->corners2D();

                    //Fixed corner-to-corner distance.
                    double dist = 0.0;
                    for(auto A_it = std::begin(A_corners), B_it = std::begin(B_corners);
                        (A_it != std::end(A_corners)) && (B_it != std::end(B_corners)); ){
                        dist += A_it->sq_dist(*B_it);
                        ++A_it;
                        ++B_it;
                    }
                    return (std::min(disp_img_it->pxl_dx, disp_img_it->pxl_dy) < dist);
                });
                if(encompassing_images.size() != 1) break;

                std::get<bool>( out ) = true;
                std::get<img_array_ptr_it_t>( out ) = cimg_array_ptr_it;
                std::get<disp_img_it_t>( out ) = encompassing_images.front();
            }catch(const std::exception &e){
                FUNCWARN("Contouring image not valid: '" << e.what() << "'");
                break;
            }
        }while(false);
        return out;
    };

    // Recompute the image viewer state, e.g., after the image data is altered by another operation.
    const auto recompute_image_state = [ &DICOM_data,
                                         &img_array_num,
                                         &img_num,
                                         &img_channel,
                                         &recompute_image_iters,
                                         &current_texture,
                                         &custom_centre,
                                         &custom_width,
                                         &need_to_reload_opengl_texture,
                                         &reset_contouring_state ](){

        //Trim any empty image arrays.
        for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
            if( ((*it) == nullptr)
            ||  (*it)->imagecoll.images.empty()){
                it = DICOM_data.image_data.erase(it);
            }else{
                ++it;
            }
        }

        // Assess whether there is image data.
        bool image_is_valid = false;
        do{
            // See if the current image numbers are valid.
            if( auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters(); img_valid ){
                image_is_valid = true;
                break;
            }

            // Try reset to the first image.
            img_array_num = 0;
            img_num = 0;
            img_channel = 0;
            if( auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters(); img_valid ){
                image_is_valid = true;
                break;
            }

            // At this point, for whatever reason(s), the image data does not appear to be valid.
            // Set negative images numbers to disable showing anything.
            img_array_num = -1;
            img_num = -1;
            img_channel = -1;
        }while(false);

        // Signal the need to reload the texture.
        //if( image_is_valid ){
        need_to_reload_opengl_texture.store(true);
        //}
        return;
    };

    // Recompute mesh iterators for the current mesh.
    const auto recompute_smesh_iters = [ &DICOM_data,
                                         &mesh_num ](){
        std::tuple<bool, disp_mesh_it_t > out;
        std::get<bool>( out ) = false;

        do{ 
            const auto has_meshes = DICOM_data.Has_Mesh_Data();
            if( !has_meshes ) break;
            if( !isininc(1, mesh_num+1, DICOM_data.smesh_data.size()) ) break;
            auto smesh_ptr_it = std::next(DICOM_data.smesh_data.begin(), mesh_num);
            if( smesh_ptr_it == std::end(DICOM_data.smesh_data) ) break;

            std::get<bool>( out ) = true;
            std::get<disp_mesh_it_t>( out ) = smesh_ptr_it;
        }while(false);
        return out;
    };

    // Recompute table iterators for the current table
    const auto recompute_table_iters = [ &DICOM_data,
                                         &table_num ](){
        std::tuple<bool, disp_table_it_t > out;
        std::get<bool>( out ) = false;

        do{ 
            const auto has_tables = DICOM_data.Has_Table_Data();
            if( !has_tables ) break;
            if( !isininc(1, table_num+1, DICOM_data.table_data.size()) ) break;
            auto table_ptr_it = std::next(DICOM_data.table_data.begin(), table_num);
            if( table_ptr_it == std::end(DICOM_data.table_data) ) break;

            std::get<bool>( out ) = true;
            std::get<disp_table_it_t>( out ) = table_ptr_it;
        }while(false);
        return out;
    };


    const auto recompute_scale_bar_image_state = [ &scale_bar_img,
                                                   &scale_bar_texture,
                                                   &recompute_image_iters,
                                                   &Free_OpenGL_Texture,
                                                   &Load_OpenGL_Texture ](){
        auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
        if( img_valid ){ 
            Free_OpenGL_Texture(scale_bar_texture);
            scale_bar_texture = Load_OpenGL_Texture( scale_bar_img, {}, {} );
        }
        return;
    };

    // Contour preprocessing. Expensive pre-processing steps are performed asynchronously in another thread.
    struct preprocessed_contour {
        long int epoch;
        ImU32 colour;
        std::string ROIName;
        std::string NormalizedROIName;
        contour_of_points<double> contour;

        preprocessed_contour() = default;
    };
    using preprocessed_contours_t = std::list<preprocessed_contour>;
    preprocessed_contours_t preprocessed_contours;
    std::map<std::string, ImVec4> contour_colours;
    std::atomic<long int> preprocessed_contour_epoch = 0L;
    std::shared_mutex preprocessed_contour_mutex;
    bool contour_colour_from_orientation = false;

    // Determine which contours should be displayed on the current image.
    const auto preprocess_contours = [ &DICOM_data,
                                       &drover_mutex,
                                       &recompute_image_iters,
                                       &preprocessed_contour_epoch,
                                       &preprocessed_contour_mutex,
                                       &preprocessed_contours,
                                       &contour_colours,
                                       &neg_contour_colour,
                                       &pos_contour_colour,
                                       &get_unique_colour,
                                       &contour_colour_from_orientation ](long int epoch) -> void {
        preprocessed_contours_t out;

        decltype(contour_colours) contour_colours_l;
        bool contour_colour_from_orientation_l;
        {
            std::shared_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
            contour_colours_l = contour_colours;
            contour_colour_from_orientation_l = contour_colour_from_orientation;
        }

        long int n = contour_colours_l.size();

        //Draw any contours that lie in the plane of the current image. Also draw contour names if the cursor is 'within' them.
        {
            std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            if( auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                img_valid
            &&  (DICOM_data.contour_data != nullptr) ){

                // Scan all contours to assign a unique colour to each ROIName.
                for(const auto & cc : DICOM_data.contour_data->ccs){
                    for(const auto & c : cc.contours){
                        const auto ROIName = c.GetMetadataValueAs<std::string>("ROIName").value_or("unknown");

                        if(contour_colours_l.count(ROIName) == 0){
                            contour_colours_l[ROIName] = get_unique_colour( n++ );
                        }
                    }
                }

                // Identify contours appropriate to the current image.
                for(const auto & cc : DICOM_data.contour_data->ccs){
                    for(const auto & c : cc.contours){
                        if(!c.points.empty() 
                        && ( 
                              // Permit contours with any included vertices or at least the 'centre' within the image.
                              //( disp_img_it->sandwiches_point_within_top_bottom_planes(c.Average_Point())
                              disp_img_it->sandwiches_point_within_top_bottom_planes(c.points.front())
                              || disp_img_it->encompasses_any_of_contour_of_points(c)
                              || ( disp_img_it->pxl_dz <= std::numeric_limits<double>::min() ) // Permit contours on purely 2D images.
                           ) ){

                            // If the contour epoch has moved on, this thread is futile. Terminate ASAP.
                            const auto current_epoch = preprocessed_contour_epoch.load();
                            if( epoch != current_epoch ) return;

                            // Access name.
                            const auto ROIName = c.GetMetadataValueAs<std::string>("ROIName").value_or("unknown");
                            const auto NormalizedROIName = c.GetMetadataValueAs<std::string>("NormalizedROIName").value_or("unknown");
                            ImVec4 c_colour = pos_contour_colour;

                            // Override the colour if metadata requests it and we know the colour.
                            if(auto m_color = c.GetMetadataValueAs<std::string>("OutlineColour")){
                                if(auto rgb_c = Colour_from_name(m_color.value())){
                                    c_colour = ImVec4( static_cast<float>(rgb_c.value().R),
                                                       static_cast<float>(rgb_c.value().G),
                                                       static_cast<float>(rgb_c.value().B),
                                                       1.0f );
                                    // Note: what to do here if metadata is not present for all contours? TODO.
                                    contour_colours_l[ROIName] = c_colour;
                                }

                            // Override the colour depending on the orientation.
                            }else if(contour_colour_from_orientation_l){
                                const auto arb_pos_unit = disp_img_it->row_unit.Cross(disp_img_it->col_unit).unit();
                                vec3<double> c_orient;
                                try{ // Protect against degenerate contours. (Should we instead ignore them altogether?)
                                    c_orient = c.Estimate_Planar_Normal();
                                }catch(const std::exception &){
                                    c_orient = arb_pos_unit;
                                }
                                const auto c_orient_pos = (c_orient.Dot(arb_pos_unit) > 0);
                                c_colour = ( c_orient_pos ? pos_contour_colour : neg_contour_colour );

                            // Otherwise use the uniquely-generated colour.
                            }else{
                                c_colour = contour_colours_l[ROIName];
                            }

                            out.push_back( preprocessed_contour{ epoch,
                                                                 ImGui::GetColorU32(c_colour),
                                                                 ROIName,
                                                                 NormalizedROIName,
                                                                 c } );
                        }
                    }
                }
            }
        }

        if( std::unique_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
            (epoch == preprocessed_contour_epoch.load()) ){
            preprocessed_contours = out;
            contour_colours = contour_colours_l;
        }
        return;
    };

    // Launch a contour preprocessing thread that will automatically update the list of preprocessed contours if
    // appropriate.
    const auto launch_contour_preprocessor = [ &preprocessed_contour_epoch,
                                               &preprocess_contours ]() -> void {
        const auto current_epoch = preprocessed_contour_epoch.fetch_add(1) + 1;
        std::thread t(preprocess_contours, current_epoch);
        t.detach();
        return;
    };

    // Terminate contour preprocessing threads.
    const auto terminate_contour_preprocessors = [ &preprocessed_contour_epoch ]() -> void {
        // We currently cannot terminate detached threads, so this helps ensure they exit early.
        preprocessed_contour_epoch.fetch_add(100);
        return;
    };

    const auto clear_preprocessed_contours = [&preprocessed_contours,
                                              &contour_colours]() -> void {
        preprocessed_contours.clear();
        //contour_colours.clear(); // Persist these unless user specifically specifies.
        return;
    };
    
    //Save the current contour collection.
    const auto save_contour_buffer = [ &DICOM_data,
                                       &drover_mutex,

                                       &contouring_imgs,
                                       &recompute_image_iters,
                                       &X,
                                       &reset_contouring_state,
                                       &launch_contour_preprocessor ](const std::string &roi_name) -> bool {
            //std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            //auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_cimage_iters();
            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            try{
                if( !img_valid ) throw std::runtime_error("Contouring image not valid.");

                for(auto &cc : contouring_imgs.contour_data->ccs){
                    contouring_imgs.Ensure_Contour_Data_Allocated();

                    if(roi_name.empty()) throw std::runtime_error("Cannot save with an empty ROI name.");

                    //Trim empty contours from the shuttle.
                    cc.Purge_Contours_Below_Point_Count_Threshold(3);
                    if(cc.contours.empty()) throw std::runtime_error("Given empty contour collection. Contours need >3 points each.");

                    auto FrameOfReferenceUID = disp_img_it->GetMetadataValueAs<std::string>("FrameOfReferenceUID");
                    if(FrameOfReferenceUID){
                        cc.Insert_Metadata("FrameOfReferenceUID", FrameOfReferenceUID.value());
                    }else{
                        throw std::runtime_error("Missing 'FrameOfReferenceUID' metadata element. Cannot continue.");
                    }

                    auto StudyInstanceUID = disp_img_it->GetMetadataValueAs<std::string>("StudyInstanceUID");
                    if(StudyInstanceUID){
                        cc.Insert_Metadata("StudyInstanceUID", StudyInstanceUID.value());
                    }else{
                        throw std::runtime_error("Missing 'StudyInstanceUID' metadata element. Cannot continue.");
                    }

                    const double MinimumSeparation = disp_img_it->pxl_dz; // TODO: use more robust method here.
                    cc.Insert_Metadata("ROIName", roi_name);
                    cc.Insert_Metadata("NormalizedROIName", X(roi_name));
                    cc.Insert_Metadata("ROINumber", "10000"); // TODO: find highest existing and ++ it.
                    cc.Insert_Metadata("MinimumSeparation", std::to_string(MinimumSeparation));
                }

                //Insert the contours into the Drover object.
                DICOM_data.Ensure_Contour_Data_Allocated();
                DICOM_data.contour_data->ccs.splice(std::end(DICOM_data.contour_data->ccs),contouring_imgs.contour_data->ccs);
                contouring_imgs.contour_data->ccs.clear();
                FUNCINFO("Drover class imbued with new contour collection");

                reset_contouring_state(img_array_ptr_it);
                launch_contour_preprocessor();

            }catch(const std::exception &e){
                FUNCWARN("Unable to save contour collection: '" << e.what() << "'");
                return false;
            }
            return true;
    };


    // Advance to the specified Image_Array. Also resets necessary display image iterators.
    const auto advance_to_image_array = [ &DICOM_data,
                                          &img_array_num,
                                          &img_num ](const long int n){
            const long int N_arrays = DICOM_data.image_data.size();
            if((n < 0) || (N_arrays <= n)){
                throw std::invalid_argument("Unwilling to move to specified Image_Array. It does not exist.");
            }
            if(n == img_array_num){
                return; // Already at desired position, so no-op.
            }
            img_array_num = n;

            // Attempt to move to the Nth image, like in the previous array.
            //
            // TODO: It's debatable whether this is even useful. Better to move to same DICOM position, I think.
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            const long int N_images = (*img_array_ptr_it)->imagecoll.images.size();
            if(N_images == 0){
                throw std::invalid_argument("Image_Array contains no images. Refusing to continue");
            }
            img_num = (img_num < 0) ? 0 : img_num; // Clamp below.
            img_num = (N_images <= img_num) ? N_images - 1 : img_num; // Clamp above.
            return;
    };

    // Advance to the specified image in the current Image_Array.
    const auto advance_to_image = [ &DICOM_data,
                                    &img_array_num,
                                    &img_num ](const long int n){
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            const long int N_images = (*img_array_ptr_it)->imagecoll.images.size();

            if((n < 0) || (N_images <= n)){
                throw std::invalid_argument("Unwilling to move to specified image. It does not exist.");
            }
            if(n == img_num){
                return; // Already at desired position, so no-op.
            }
            img_num = n;
            return;
    };

    // Given two points and multiple candidate unit vectors, project the vector from A->B along the most aligned unit.
    const auto largest_projection = []( const vec3<double> &A,
                                        const vec3<double> &B,
                                        const std::vector< vec3<double> > &units ) -> vec3<double> {
            const auto C = B - A;
            vec3<double> best;
            double best_proj = std::numeric_limits<double>::infinity() * -1.0;
            for(const auto& u : units){
                const auto proj = C.Dot(u.unit());
                if(best_proj < std::abs(proj)){
                    best_proj = std::abs(proj);
                    best = A + u.unit() * proj;
                }
            }
            return best;
    };

    // Draw an editable metadata table.
    const auto display_metadata_table = []( metadata_map_t &m ){
            ImVec2 cell_padding(0.0f, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
            ImGui::PushID(&m);
            if(ImGui::BeginTable("Metadata Table", 2,   ImGuiTableFlags_Borders
                                                      | ImGuiTableFlags_RowBg
                                                      | ImGuiTableFlags_BordersV
                                                      | ImGuiTableFlags_BordersInner
                                                      | ImGuiTableFlags_Resizable )){

                ImGui::TableSetupColumn("Key");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                std::array<char, 2048> metadata_text_entry;
                string_to_array(metadata_text_entry, "");

                int i = 0;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                for(const auto &apair : m){
                    auto key = apair.first;
                    auto val = apair.second;

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    string_to_array(metadata_text_entry, key);
                    ImGui::PushID(++i);
                    const bool key_changed = ImGui::InputText("##key", metadata_text_entry.data(), metadata_text_entry.size());
                    ImGui::PopID();

                    // Since key_changed is true whenever any changes have occured, even if the mouse is idling
                    // after a change, then the following causes havoc by continuously editing the key and messing
                    // with the ID system. A better system would only implement the change when the focus is lost
                    // and/or enter is pressed. I'm not sure if there is a simple way to do this at the moment, so
                    // I'll leave key editing disabled until I figure out a reasonable fix.
                    //
                    // TODO.
                    //
                    //if(key_changed){
                    //    std::string new_key;
                    //    array_to_string(new_key, metadata_text_entry);
                    //    if(new_key != key){
                    //        disp_img_it->metadata.erase(key);
                    //        disp_img_it->metadata[new_key] = val;
                    //        key = new_key;
                    //    }
                    //}

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    string_to_array(metadata_text_entry, val);
                    ImGui::PushID(++i);
                    const bool val_changed = ImGui::InputText("val", metadata_text_entry.data(), metadata_text_entry.size());
                    ImGui::PopID();
                    if(val_changed){
                        // Replace key's value with updated value in place.
                        array_to_string(val, metadata_text_entry);
                        m[key] = val;
                    }
                }
                ImGui::PopStyleColor();
                ImGui::EndTable();
            }
            ImGui::PopID();
            ImGui::PopStyleVar();
            return;
    };

    // ------------------------------------------- Main loop ----------------------------------------------


    // Open file dialog state.
    std::filesystem::path open_file_root = std::filesystem::current_path();
    std::array<char,2048> root_entry_text;
    enum class file_selection_intent {
        files,    // Treat files as files that should be loaded using Load_Files() and eagerly converted into operations.
        scripts,  // Treat files as scripts that should be read as text files and the contents not interpretted.
    };
    struct file_selection {
        std::filesystem::path path;
        bool is_dir;
        std::uintmax_t file_size;
        bool selected;
    };
    std::vector<file_selection> open_files_selection;
    std::vector<file_selection> open_scripts_selection;

    const auto query_files = []( std::filesystem::path root ){
        std::vector<file_selection> files;
        try{
            if( !root.empty()
            &&  std::filesystem::exists(root)
            &&  std::filesystem::is_directory(root) ){
                for(const auto &d : std::filesystem::directory_iterator( root )){
                    const auto p = d.path();
                    const std::uintmax_t file_size = ( !std::filesystem::is_directory(p) && std::filesystem::exists(p) )
                                                     ? std::filesystem::file_size(p) : 0;
                    files.push_back( { p,
                                       std::filesystem::is_directory(p),
                                       file_size,
                                       false } );
                }
                std::sort( std::begin(files), std::end(files), 
                           [](const file_selection &L, const file_selection &R){
                                return L.path < R.path;
                           } );
            }
        }catch(const std::exception &e){
            FUNCINFO("Unable to query files: '" << e.what() << "'");
            files.clear();
        }
        return files;
    };

    recompute_image_state();
    recompute_scale_bar_image_state();
    {
        auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
        if(img_valid) reset_contouring_state(img_array_ptr_it);
    }
    launch_contour_preprocessor();

    struct loaded_files_res {
        bool res;
        Drover DICOM_data;
        std::map<std::string,std::string> InvocationMetadata;
    };
    std::future<loaded_files_res> loaded_files;

    // This is meant to be run asynchronously.
    const auto launch_file_open_dialog = [InvocationMetadata,
                                          FilenameLex,
                                          &view_toggles,
                                          &query_files ](std::filesystem::path open_file_root) -> loaded_files_res {
        if(!std::filesystem::is_directory(open_file_root)){
            open_file_root = std::filesystem::current_path();
        }

        // Create a dialog box.
        std::optional<select_files> selector_opt;
        if(!selector_opt){
            selector_opt.emplace("Select file(s) to open"_s);
//                                         open_file_root.string(),
//                                         std::vector<std::string>{ "DICOM Files"_s, "*.dcm *.DCM"_s,
//                                                                   "All Files"_s, "*"_s } );
        }

        // Wait for the user to provide input.
        //
        // Note: the following blocks by continuous polling.
        const auto selection = selector_opt.value().get_selection();
        selector_opt.reset();

        std::list<std::filesystem::path> paths;
        for(const auto &f : selection){
            paths.push_back( f );
        }

        // Load the files.
        loaded_files_res lfs;
        lfs.InvocationMetadata = InvocationMetadata;
        std::list<OperationArgPkg> Operations;
        lfs.res = Load_Files(lfs.DICOM_data, lfs.InvocationMetadata, FilenameLex, Operations, paths);
        if(!Operations.empty()){
             lfs.res = false;
             FUNCWARN("Loaded file contains a script. Currently unable to handle script files here");
        }

        // Notify that the files can be inserted into the 
        return lfs;
    };

    // Script files.
    struct script_file {
        std::filesystem::path path;
        bool altered = false;
        std::vector<char> content;
        std::list<script_feedback_t> feedback; // Compilation/validation messages.
    };
    std::vector<script_file> script_files;
    long int active_script_file = -1;
    std::shared_mutex script_mutex;
    std::atomic<long int> script_epoch = 0L;
    const std::string new_script_content = "#!/usr/bin/env -S dicomautomaton_dispatcher -v\n\n";

    const auto append_to_script = [](std::vector<char> &content, const std::string &s) -> void {
        for(const auto &c : s) content.emplace_back(c);
        return;
    };

    struct loaded_scripts_res {
        bool res;
        std::vector<script_file> script_files;
    };
    std::future<loaded_scripts_res> loaded_scripts;

    // This is meant to be run asynchronously.
    const auto launch_script_open_dialog = [](std::filesystem::path open_file_root) -> loaded_scripts_res {
        if(!std::filesystem::is_directory(open_file_root)){
            open_file_root = std::filesystem::current_path();
        }

        // Create a dialog box.
        std::optional<select_files> selector_opt;
        if(!selector_opt){
            selector_opt.emplace("Select script(s) to open"_s,
                    std::filesystem::path(),
                    std::vector<std::string>{ "DCMA Script Files"_s, "*.txt *.TXT *.scr *.SCR *.dscr *.DSCR"_s,
                                              "All Files"_s, "*"_s } );
        }

        // Wait for the user to provide input.
        //
        // Note: the following blocks by continuous polling.
        const auto selection = selector_opt.value().get_selection();
        selector_opt.reset();

        std::list<std::filesystem::path> paths;
        for(const auto &f : selection){
            paths.push_back( f );
        }

        // Load the files.
        loaded_scripts_res lss;
        lss.res = true;

        for(const auto &p : paths){
            std::ifstream is(p, std::ios::in);
            if(!is){
                lss.res = false;
                FUNCWARN("Unable to access script file '" << p.string() << "'");
                break;
            }
            lss.script_files.emplace_back();
            lss.script_files.back().path = p;

            lss.script_files.back().altered = false;
            const auto end_it = std::istreambuf_iterator<char>();
            for(auto c_it = std::istreambuf_iterator<char>(is); c_it != end_it; ++c_it){
                lss.script_files.back().content.emplace_back(*c_it);
            }
            lss.script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
        }

        if(!lss.res){
            lss.script_files.clear();
        }

        // Notify that the files can be inserted into the 
        return lss;
    };

    const auto execute_script = [ &launch_contour_preprocessor,
                                  &preprocessed_contour_mutex,
                                  &terminate_contour_preprocessors,
                                  &clear_preprocessed_contours,
                                  &tagged_pos,

                                  &drover_mutex,
                                  &DICOM_data,
                                  &InvocationMetadata,
                                  &FilenameLex,

                                  &recompute_image_state,
                                  &recompute_image_iters,
                                  &reset_contouring_state,
                                  &need_to_reload_opengl_mesh,

                                  &wq,
                                  &script_mutex,
                                  &script_files,
                                  &script_epoch ]( const std::string &s,
                                                   std::list<script_feedback_t> &f ) -> bool {

        std::stringstream ss( s );
        f.clear();
        std::list<OperationArgPkg> op_list;
        const auto res = Load_DCMA_Script( ss, f, op_list );
        if(!res){
            //script_files.at(active_script_file).feedback.emplace_back();
            f.back().message = "Compilation failed";

        }else{
            const auto l_script_epoch = script_epoch.fetch_add(1) + 1;

            // Submit the work to the worker thread.
            const auto worker = [&,l_script_epoch,op_list](){
                // Check if this task should be abandoned.
                if(script_epoch.load() != l_script_epoch){
                    FUNCINFO("Abandoning run due to potentially conflicting user activity");
                    return;
                }

                std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);

                // Preemptively destroy any preprocessed contours to avoid dangling references.
                std::unique_lock<std::shared_mutex> ppc_lock(preprocessed_contour_mutex);
                terminate_contour_preprocessors();
                clear_preprocessed_contours();

                // Only perform a single operation at a time, which results in slightly less mutex contention.
                bool success = true;
                for(const auto &op : op_list){
                    success = false;
                    try{
                        success = Operation_Dispatcher(DICOM_data,
                                                       InvocationMetadata,
                                                       FilenameLex,
                                                       {op});
                    }catch(const std::exception &){}
                    if(!success) break;
                }
                if(!success){
                    // Report the failure to the user.
                    //
                    // TODO: provide graphical feedback!
                    FUNCWARN("Script execution failed");
                }

                // Regenerate all Drover state that may have changed.
                {
                    recompute_image_state();
                    auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                    if( img_valid ){
                        launch_contour_preprocessor();
                        reset_contouring_state(img_array_ptr_it);
                    }
                    need_to_reload_opengl_mesh = true;
                    tagged_pos = {};
                }
                return;
            };
            wq.submit_task( worker ); // Offload to waiting worker thread.
        }
        return res;
    };

    // Contour and image display state.
    std::map<std::string, bool> contour_enabled;
    std::map<std::string, bool> contour_hovered;
    float contour_line_thickness = 1.0f;

    ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
    ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
    float zoom = 1.0f;
    ImVec2 pan(0.5f, 0.5f);

    {
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigWindowsMoveFromTitleBarOnly = true;
    }


    long int frame_count = 0;
    while(true){
        ++frame_count;
        image_mouse_pos_opt = {};

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        bool close_window = false;
        while(SDL_PollEvent(&event)){
            ImGui_ImplSDL2_ProcessEvent(&event);
            if(event.type == SDL_QUIT){
                close_window = true;
                break;

            }else if( (event.type == SDL_WINDOWEVENT)
                  &&  (event.window.event == SDL_WINDOWEVENT_CLOSE)
                  &&  (event.window.windowID == SDL_GetWindowID(window)) ){
                close_window = true;
                break;

            }
        }
        if(close_window) break;

        // Build a frame using ImGui.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if(view_toggles.view_imgui_demo){
            ImGui::ShowDemoWindow(&view_toggles.view_imgui_demo);
        }
        if(view_toggles.view_implot_demo){
            ImPlot::ShowDemoWindow(&view_toggles.view_implot_demo);
        }


        const auto display_parameter_table = [&drover_mutex,
                                              &mutex_dt,
                                              &InvocationMetadata,
                                              &display_metadata_table,
                                              &view_toggles ]() -> void {
            if( !view_toggles.view_parameter_table ) return;

            // Attempt to acquire an exclusive lock.
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::Begin("Parameter Table", &view_toggles.view_parameter_table);

            display_metadata_table( InvocationMetadata );

            ImGui::End();
            return;
        };
        try{
            display_parameter_table();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_parameter_table(): '" << e.what() << "'");
            throw;
        }


        // Reload the image texture. Needs to be done by the main thread.
        const auto reload_image_texture = [&drover_mutex,
                                           &recompute_image_iters,
                                           &view_toggles,
                                           &custom_centre,
                                           &custom_width,
                                           &current_texture,
                                           &img_num,
                                           &img_array_num,
                                           &img_channel,
                                           &Free_OpenGL_Texture,
                                           &Load_OpenGL_Texture ]() -> void {
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            if( view_toggles.view_images_enabled
            &&  img_valid ){
                img_channel = std::clamp<long int>(img_channel, 0, disp_img_it->channels-1);
                Free_OpenGL_Texture(current_texture);
                current_texture = Load_OpenGL_Texture(*disp_img_it, custom_centre, custom_width);
            }else{
                img_channel = -1;
                img_array_num = -1;
                img_num = -1;
                current_texture = opengl_texture_handle_t();
            }
            return;
        };
        if(need_to_reload_opengl_texture.exchange(false)){
            reload_image_texture();
        }

        // Contouring -- mask debugging / visualization.
        if(view_toggles.view_contouring_debug){
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();
            if(cimg_valid){
                ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(700, 40), ImGuiCond_FirstUseEver);
                ImGui::Begin("Contour Mask Debugging", &view_toggles.view_contouring_debug, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar ); //| ImGuiWindowFlags_AlwaysAutoResize);
                Free_OpenGL_Texture(contouring_texture);
                contouring_texture = Load_OpenGL_Texture( *cimg_it, {}, {} );
                auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(contouring_texture.texture_number));
                ImGui::Image(gl_tex_ptr, ImVec2(600,600), uv_min, uv_max);
                ImGui::End();
            }
        }

        // Display the main menu bar, which should always be visible.
        const auto display_main_menu_bar = [&view_toggles,
                                            &open_file_root,
                                            &loaded_files,
                                            &launch_file_open_dialog,
                                            &contour_enabled,
                                            &contour_hovered,
                                            &launch_contour_preprocessor,
                                            &contouring_img_altered,
                                            &tagged_pos,

                                            &script_mutex,
                                            &script_files,
                                            &active_script_file,
                                            &script_epoch,
                                            &new_script_content,
                                            &append_to_script,
                                            &execute_script,

                                            &lsamps_visible,
                                            &row_profile,
                                            &col_profile,
                                            &time_profile ](void) -> bool {
            if(ImGui::BeginMainMenuBar()){
                if(ImGui::BeginMenu("File")){
                    if(ImGui::MenuItem("Open", "ctrl+o", &view_toggles.open_files_enabled)){
                        if(!loaded_files.valid()){
                            loaded_files = std::async(std::launch::async, launch_file_open_dialog, open_file_root);
                        }
                    }
                    //if(ImGui::MenuItem("Open", "ctrl+o")){
                    //    ImGui::OpenPopup("OpenFileSelector");
                    //}
                    ImGui::Separator();
                    if(ImGui::MenuItem("Exit", "ctrl+q")){
                        ImGui::EndMenu();
                        return false;
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if(ImGui::BeginMenu("View")){
                    ImGui::MenuItem("Images", nullptr, &view_toggles.view_images_enabled);
                    if(ImGui::MenuItem("Contours", nullptr, &view_toggles.view_contours_enabled)){
                        contour_enabled.clear();
                        contour_hovered.clear();
                        if(view_toggles.view_contours_enabled) launch_contour_preprocessor();
                    }
                    if(ImGui::MenuItem("Contouring", nullptr, &view_toggles.view_contouring_enabled)){
                        view_toggles.view_drawing_enabled = false;
                        contouring_img_altered = true;
                        tagged_pos = {};
                    }
                    if(ImGui::MenuItem("Drawing", nullptr, &view_toggles.view_drawing_enabled)){
                        view_toggles.view_contouring_enabled = false;
                        tagged_pos = {};
                    }
                    ImGui::MenuItem("Image Metadata", nullptr, &view_toggles.view_image_metadata_enabled);
                    ImGui::MenuItem("Image Hover Tooltips", nullptr, &view_toggles.show_image_hover_tooltips);
                    ImGui::MenuItem("Meshes", nullptr, &view_toggles.view_meshes_enabled);
                    if(ImGui::MenuItem("Plots", nullptr, &view_toggles.view_plots_enabled)){
                        lsamps_visible.clear();
                    }
                    ImGui::MenuItem("Mesh Metadata", nullptr, &view_toggles.view_mesh_metadata_enabled);
                    ImGui::MenuItem("Plot Hover Metadata", nullptr, &view_toggles.view_plots_metadata);
                    if(ImGui::MenuItem("Row and Column Profiles", nullptr, &view_toggles.view_row_column_profiles)){
                        row_profile.samples.clear();
                        col_profile.samples.clear();
                    };
                    if(ImGui::MenuItem("Time Profiles", nullptr, &view_toggles.view_time_profiles)){
                        time_profile.samples.clear();
                    };
                    ImGui::MenuItem("Parameter Table", nullptr, &view_toggles.view_parameter_table);
                    ImGui::MenuItem("Tables", nullptr, &view_toggles.view_tables_enabled);
                    ImGui::MenuItem("Table Metadata", nullptr, &view_toggles.view_table_metadata_enabled);
                    ImGui::MenuItem("Script Editor", nullptr, &view_toggles.view_script_editor_enabled);
                    ImGui::MenuItem("Script Feedback", nullptr, &view_toggles.view_script_feedback);
                    ImGui::MenuItem("Shader Editor", nullptr, &view_toggles.view_shader_editor_enabled);
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Adjust")){
                    if(ImGui::BeginMenu("Toggle Style")){
                        if(ImGui::MenuItem("Dark Mode", nullptr, nullptr)){
                            ImGui::StyleColorsDark();
                        }
                        if(ImGui::MenuItem("Light Mode", nullptr, nullptr)){
                            ImGui::StyleColorsLight();
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    ImGui::MenuItem("Image Window and Level", nullptr, &view_toggles.adjust_window_level_enabled);
                    ImGui::MenuItem("Image Colour Map", nullptr, &view_toggles.adjust_colour_map_enabled);
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if(ImGui::BeginMenu("Script")){
                    if(ImGui::BeginMenu("Append Operation")){
                        auto known_ops = Known_Operations();
                        for(auto &anop : known_ops){
                            std::stringstream nss;
                            const auto op_name = anop.first;
                            nss << op_name;

                            std::stringstream ss;
                            auto op_docs = (anop.second.first)();
                            for(const auto &a : op_docs.aliases) nss << ", " << a;
                            ss << op_docs.desc << "\n\n";
                            if(!op_docs.notes.empty()){
                                ss << "Notes:" << std::endl;
                                for(auto &note : op_docs.notes){
                                    ss << "\n" << "- " << note << std::endl;
                                }
                            }

                            if(ImGui::MenuItem(nss.str().c_str())){
                                std::unique_lock<std::shared_mutex> script_lock(script_mutex);

                                auto N_sfs = static_cast<long int>(script_files.size());
                                if( N_sfs == 0 ){
                                    FUNCINFO("No script to append to. Creating new script.");
                                    script_files.emplace_back();
                                    script_files.back().altered = true;
                                    append_to_script(script_files.back().content, new_script_content);
                                    script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
                                    active_script_file = N_sfs;
                                    N_sfs = static_cast<long int>(script_files.size());
                                }
                                if( !script_files.empty()
                                &&  isininc(0, active_script_file, N_sfs-1)){
                                    // Remove terminating '\0' from script.
                                    script_files.at(active_script_file).content.erase(
                                        std::remove_if( std::begin(script_files.at(active_script_file).content),
                                                        std::end(script_files.at(active_script_file).content),
                                                        [](char c) -> bool { return (c == '\0'); }), 
                                        std::end(script_files.at(active_script_file).content) );

                                    // Count whitespace on preceeding line to indent new line accordingly.
                                    // ... TODO ...

                                    // Add operation to script.
                                    std::stringstream sc; // required arguments.
                                    std::stringstream oc; // optional arguments.
                                    sc << std::endl << op_name << "(";
                                    std::set<std::string> args;
                                    for(const auto & a : op_docs.args){
                                        // Avoid duplicate arguments (from composite operations).
                                        const auto name = a.name;
                                        if(args.count(name) != 0) continue;
                                        args.insert(name);

                                        // Escape any quotes in the default value, which will generally be parsed
                                        // fuzzily via regex and should be OK.
                                        std::string escaped_val;
                                        {
                                            bool prev_was_escape = false;
                                            for(const auto &c : a.default_val){
                                                if(!prev_was_escape && (c == '\'')) escaped_val += '\\';
                                                escaped_val += c;
                                                prev_was_escape = (c == '\\');
                                            }
                                        }

                                        // Emit the parameter and default value.
                                        if(a.expected){
                                            // Note the trailing comma. This is valid syntax and makes it easier to
                                            // enable/disable optional arguments.
                                            sc << std::endl << "    " << name << " = '" << escaped_val << "',";
                                        }else{
                                            oc << std::endl << "    # " << name << " = '" << escaped_val << "',";
                                        }
                                    }

                                    // Print optional arguments at the end.
                                    if(!oc.str().empty()) sc << oc.str();

                                    // Avoid all newlines for parameter-less operations (e.g., 'True(){};').
                                    if(!op_docs.args.empty()) sc << std::endl;
                                    sc << "){};" << std::endl;

                                    append_to_script(script_files.at(active_script_file).content, sc.str());
                                    script_files.back().content.emplace_back('\0');
                                    view_toggles.view_script_editor_enabled = true;
                                }
                            }
                            if(ImGui::IsItemHovered()){
                                ImGui::SetNextWindowSizeConstraints(ImVec2(400.0, -1), ImVec2(500.0, -1));
                                ImGui::BeginTooltip();
                                ImGui::TextWrapped("%s", ss.str().c_str());
                                ImGui::EndTooltip();
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if(ImGui::BeginMenu("Edit Action Script")){
                        const auto categories = standard_script_categories();
                        for(const auto &cat : categories){
                            if(ImGui::BeginMenu(cat.c_str())){
                                for(const auto &sscript : standard_scripts_with_category(cat)){
                                    if(ImGui::MenuItem(sscript.name.c_str())){
                                        std::unique_lock<std::shared_mutex> script_lock(script_mutex);
                                        auto N_sfs = static_cast<long int>(script_files.size());
                                        script_files.emplace_back();
                                        script_files.back().altered = false;
                                        script_files.back().path = sscript.name;
                                        script_files.back().content.clear();
                                        append_to_script(script_files.back().content, sscript.text);
                                        script_files.back().content.emplace_back('\0');
                                        active_script_file = N_sfs;
                                        ++N_sfs;
                                        view_toggles.view_script_editor_enabled = true;
                                    }
                                    if(ImGui::IsItemHovered()){
                                        ImGui::SetNextWindowSizeConstraints(ImVec2(600.0, -1), ImVec2(500.0, -1));
                                        ImGui::BeginTooltip();
                                        ImGui::TextWrapped("%s", sscript.text.c_str());
                                        ImGui::EndTooltip();
                                    }
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                if(ImGui::BeginMenu("Actions")){
                    const auto categories = standard_script_categories();
                    for(const auto &cat : categories){
                        if(ImGui::BeginMenu(cat.c_str())){
                            for(const auto &sscript : standard_scripts_with_category(cat)){
                                if(ImGui::MenuItem(sscript.name.c_str())){
                                    std::list<script_feedback_t> feedback;
                                    if(!execute_script(sscript.text, feedback)){
                                        FUNCWARN("Script execution failed");
                                        // TODO: provide feedback to user here...
                                    }
                                }
                                if(ImGui::IsItemHovered()){
                                    ImGui::SetNextWindowSizeConstraints(ImVec2(600.0, -1), ImVec2(500.0, -1));
                                    ImGui::BeginTooltip();
                                    ImGui::TextWrapped("%s", sscript.text.c_str());
                                    ImGui::EndTooltip();
                                }
                            }
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if(ImGui::BeginMenu("Help", "ctrl+h")){
                    if(ImGui::MenuItem("About")){
                        view_toggles.set_about_popup = true;
                    }
                    ImGui::MenuItem("Metrics", nullptr, &view_toggles.view_metrics_window);
                    ImGui::Separator();

                    if(ImGui::BeginMenu("Operations", "ctrl+d")){
                        auto known_ops = Known_Operations();
                        for(auto &anop : known_ops){
                            const auto op_name = anop.first;
                            std::stringstream ss;

                            auto op_docs = (anop.second.first)();
                            ss << op_docs.desc << "\n\n";
                            if(!op_docs.notes.empty()){
                                ss << "Notes:" << std::endl;
                                for(auto &note : op_docs.notes){
                                    ss << "\n" << "- " << note << std::endl;
                                }
                            }

                            if(ImGui::MenuItem(op_name.c_str())){}
                            if(ImGui::IsItemHovered()){
                                ImGui::SetNextWindowSizeConstraints(ImVec2(400.0, -1), ImVec2(500.0, -1));
                                ImGui::BeginTooltip();
                                ImGui::TextWrapped("%s", ss.str().c_str());
                                ImGui::EndTooltip();
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
            return true;
        };
        try{
            if(!display_main_menu_bar()) break;
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_main_menu_bar(): '" << e.what() << "'");
            throw;
        }

        if( view_toggles.view_metrics_window ){
            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::ShowMetricsWindow(&view_toggles.view_metrics_window);
        }

        // Display the shader editor dialog.
        const auto display_shader_editor = [&view_toggles,
                                            &custom_shader,
                                            &vert_shader_src,
                                            &frag_shader_src,
                                            &shader_log ]() -> void {
            if( !view_toggles.view_shader_editor_enabled ) return;

            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
            if(ImGui::Begin("Shader Editor", &view_toggles.view_shader_editor_enabled )){
                ImVec2 window_extent = ImGui::GetContentRegionAvail();
                if(ImGui::Button("Compile", ImVec2(window_extent.x/4, 0))){ 
                    std::stringstream ss;
                    try{
                        auto l_custom_shader = compile_shader_program(vert_shader_src,
                                                                      frag_shader_src,
                                                                      shader_log);
                        custom_shader = std::move(l_custom_shader);
                        shader_log = string_to_array( array_to_string(shader_log) + "\nShader updated" );
                    }catch(const std::exception &e){
                        FUNCWARN("Shader compilation failed: '" << e.what() << "'");
                    };
                }


                ImGui::Text("Vertex shader");
                ImVec2 edit_box_extent = ImGui::GetContentRegionAvail();
                edit_box_extent.y *= 3.0 / 7.0;

                ImGuiInputTextFlags flags;
                ImGui::InputTextMultiline("#vert_shader_editor",
                                          vert_shader_src.data(),
                                          vert_shader_src.size(),
                                          edit_box_extent, flags);

                ImGui::Text("Fragment shader");
                edit_box_extent = ImGui::GetContentRegionAvail();
                edit_box_extent.y *= 3.0 / 4.0;
                ImGui::InputTextMultiline("#frag_shader_editor",
                                          frag_shader_src.data(),
                                          frag_shader_src.size(),
                                          edit_box_extent, flags);

                ImGui::Text("Compilation feedback");
                flags |= ImGuiInputTextFlags_ReadOnly;
                edit_box_extent = ImGui::GetContentRegionAvail();
                ImGui::InputTextMultiline("#shader_compile_feedback",
                                          shader_log.data(),
                                          shader_log.size(),
                                          edit_box_extent, flags);

            }
            ImGui::End();
            return;
        };
        try{
            display_shader_editor();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_shader_editor(): '" << e.what() << "'");
            throw;
        }

        // Display the script editor dialog.
        const auto display_script_editor = [&view_toggles,
                                            &custom_centre,
                                            &custom_width,
                                            &open_file_root,
                                            &root_entry_text,
                                            &launch_script_open_dialog,
                                            &loaded_scripts,

                                            &script_mutex,
                                            &script_files,
                                            &active_script_file,
                                            &script_epoch,
                                            &new_script_content,
                                            &append_to_script,
                                            &execute_script,

                                            &drover_mutex,
                                            &DICOM_data,
                                            &recompute_image_state,
                                            &recompute_image_iters,
                                            &tagged_pos,

                                            &preprocessed_contour_mutex,
                                            &terminate_contour_preprocessors,
                                            &launch_contour_preprocessor,
                                            &clear_preprocessed_contours,
                                            &reset_contouring_state,

                                            &InvocationMetadata,
                                            &FilenameLex,
                                            &wq,
                                            &line_numbers_normal_colour,
                                            &line_numbers_info_colour,
                                            &line_numbers_warn_colour,
                                            &line_numbers_error_colour,
                                            &line_numbers_debug_colour ]() -> void {
            if( std::unique_lock<std::shared_mutex> script_lock(script_mutex);
                view_toggles.view_script_editor_enabled ){
                ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
                if(ImGui::Begin("Script Editor", &view_toggles.view_script_editor_enabled )){
                    ImVec2 window_extent = ImGui::GetContentRegionAvail();

                    auto N_sfs = static_cast<long int>(script_files.size());
                    if(ImGui::Button("New", ImVec2(window_extent.x/4, 0))){ 
                        script_files.emplace_back();
                        script_files.back().altered = true;
                        append_to_script(script_files.back().content, new_script_content);
                        script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
                        active_script_file = N_sfs;
                        N_sfs = static_cast<long int>(script_files.size());
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Open", ImVec2(window_extent.x/4, 0))){ 
                        if(!loaded_scripts.valid()){
                            loaded_scripts = std::async(std::launch::async, launch_script_open_dialog, open_file_root);
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Save As", ImVec2(window_extent.x/4, 0))){ 
                        if( (N_sfs != 0) 
                        &&  isininc(0, active_script_file, N_sfs-1)){
                            try{
                                if( script_files.at(active_script_file).path.empty() ){
                                    // Attempt to determine the absolute path.
                                    auto l_root = open_file_root;
                                    const auto l_root_abs = std::filesystem::absolute(open_file_root);
                                    l_root = (std::filesystem::exists(l_root_abs)) ? l_root_abs : l_root;

                                    const auto open_file_root_str = (l_root / "script.dscr").string();
                                    string_to_array(root_entry_text, open_file_root_str);
                                }else{
                                    // Re-use the existing path.
                                    const auto path_str = script_files.at(active_script_file).path.string();
                                    string_to_array(root_entry_text, path_str);
                                }
                                ImGui::OpenPopup("Save Script Filename Picker");
                            }catch(const std::exception &e){
                                FUNCWARN("Unable to access current filesystem path");
                            }
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Close", ImVec2(window_extent.x/4, 0))){ 
                        if( (N_sfs != 0) 
                        &&  isininc(0, active_script_file, N_sfs-1)){
                            script_files.erase( std::next( std::begin( script_files ), active_script_file ) );
                            --active_script_file; // Default to script on left.
                            --N_sfs;
                        }
                    }

                    if(ImGui::Button("Validate", ImVec2(window_extent.x/4, 0))){ 
                        if( (N_sfs != 0) 
                        &&  isininc(0, active_script_file, N_sfs-1)){
                            std::stringstream ss( std::string( std::begin(script_files.at(active_script_file).content),
                                                               std::end(script_files.at(active_script_file).content) ) );
                            script_files.at(active_script_file).feedback.clear();
                            std::list<OperationArgPkg> op_list;
                            Load_DCMA_Script( ss, script_files.at(active_script_file).feedback, op_list );
                            view_toggles.view_script_feedback = true;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Run", ImVec2(window_extent.x/4, 0))){ 
                        if( (N_sfs != 0) 
                        &&  isininc(0, active_script_file, N_sfs-1)){
                            script_files.at(active_script_file).feedback.clear();
                            const bool res = execute_script( std::string( std::begin(script_files.at(active_script_file).content),
                                                                          std::end(script_files.at(active_script_file).content) ),
                                                        script_files.at(active_script_file).feedback );
                            if(!res) view_toggles.view_script_feedback = true;
                        }
                    }

                    if( (N_sfs != 0) 
                    &&  isininc(0, active_script_file, N_sfs-1)
                    &&  !(script_files.at(active_script_file).feedback.empty())
                    &&  view_toggles.view_script_feedback ){
                        ImGui::SetNextWindowSize(ImVec2(650, 250), ImGuiCond_FirstUseEver);
                        ImGui::SetNextWindowPos(ImVec2(650, 500), ImGuiCond_FirstUseEver);
                        ImGui::Begin("Script Feedback", &view_toggles.view_script_feedback );

                        for(const auto &f : script_files.at(active_script_file).feedback){
                            if(false){
                            }else if(f.severity == script_feedback_severity_t::debug){
                                std::stringstream ss;
                                ss << "Debug:   ";
                                ImGui::TextColored(line_numbers_debug_colour, "%s", const_cast<char *>(ss.str().c_str()));
                            }else if(f.severity == script_feedback_severity_t::info){
                                std::stringstream ss;
                                ss << "Info:    ";
                                ImGui::TextColored(line_numbers_info_colour, "%s", const_cast<char *>(ss.str().c_str()));
                            }else if(f.severity == script_feedback_severity_t::warn){
                                std::stringstream ss;
                                ss << "Warning: ";
                                ImGui::TextColored(line_numbers_warn_colour, "%s", const_cast<char *>(ss.str().c_str()));
                            }else if(f.severity == script_feedback_severity_t::err){
                                std::stringstream ss;
                                ss << "Error:   ";
                                ImGui::TextColored(line_numbers_error_colour, "%s", const_cast<char *>(ss.str().c_str()));
                            }else{
                                throw std::logic_error("Unrecognized severity level");
                            }
                            ImGui::SameLine();

                            std::stringstream ss;
                            if( (0 <= f.line)
                            &&  (0 <= f.line_offset) ){
                                ss << "line " << f.line 
                                   << ", char " << f.line_offset
                                   << ": ";
                            }
                            ss << f.message
                               << std::endl
                               << std::endl;
                            ImGui::Text("%s", const_cast<char *>(ss.str().c_str()));
                        }

                        ImGui::End();
                    }

                    // Pop-up to query the user for a filename.
                    if(ImGui::BeginPopupModal("Save Script Filename Picker", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                        // TODO: add a proper 'Save As' file selector here.

                        ImGui::Text("Save file as...");
                        ImGui::SetNextItemWidth(650.0f);
                        ImGui::InputText("##save_script_as_text_entry", root_entry_text.data(), root_entry_text.size() - 1);

                        if(ImGui::Button("Save")){
                            script_files.at(active_script_file).path.assign(
                                std::begin(root_entry_text),
                                std::find( std::begin(root_entry_text), std::end(root_entry_text), '\0') );
                            script_files.at(active_script_file).path.replace_extension(".dscr");

                            // Write the file contents to the given path.
                            std::ofstream FO(script_files.at(active_script_file).path);
                            FO.write( script_files.at(active_script_file).content.data(),
                                      (script_files.at(active_script_file).content.size() - 1) ); // Disregard trailing null.
                            FO << std::endl;
                            FO.flush();
                            if(FO){
                                script_files.at(active_script_file).altered = false;
                            }else{
                                script_files.at(active_script_file).path.clear();
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if(ImGui::Button("Cancel")){
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }

                    // 'Tabs' for file selection.
                    auto &style = ImGui::GetStyle();
                    for(long int i = 0; i < N_sfs; ++i){
                        auto fname = script_files.at(i).path.filename().string();
                        if(fname.empty()) fname = "(unnamed)";
                        if(script_files.at(i).altered) fname += "**";

                        fname += "##script_file_"_s + std::to_string(i); // Unique identifier for ImGui internals.
                        if(i == active_script_file){
                            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
                        }else{
                            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);
                        }
                        if(ImGui::Button(fname.c_str())){
                            active_script_file = i;
                        }
                        ImGui::PopStyleColor(1);
                        if( (i+1) <  N_sfs ){
                            ImGui::SameLine();
                        }
                    }

                    if( (N_sfs != 0) 
                    &&  isininc(0, active_script_file, N_sfs-1) ){

                        // Implement a callback to handle resize events.
                        const auto text_entry_callback = [](ImGuiInputTextCallbackData *data) -> int {
                            auto sf_ptr = reinterpret_cast<script_file*>(data->UserData);
                            if(sf_ptr == nullptr) throw std::logic_error("Invalid script file ptr found in callback");
                            
                            // Resize the underlying storage.
                            if(data->EventFlag == ImGuiInputTextFlags_CallbackResize){
                                sf_ptr->content.resize(data->BufTextLen, '\0'); // Ensure the file character is a null.
                                data->Buf = sf_ptr->content.data();
                            }

                            // Mark the file as altered.
                            if(data->EventFlag == ImGuiInputTextFlags_CallbackEdit){
                                sf_ptr->altered = true;
                            }

                            return 0;
                        };

                        auto sf_ptr = &(script_files.at(active_script_file));
                        if(sf_ptr == nullptr) throw std::logic_error("Invalid script file ptr");

                        // Ensure there is a trailing null character to avoid issues with c-style string interpretation.
                        if( sf_ptr->content.empty()
                        ||  (sf_ptr->content.back() != '\0') ){
                            sf_ptr->content.emplace_back('\0');
                            sf_ptr->altered = true;
                        }

                        // Leave room for line numbers.
                        const auto orig_cursor_pos = ImGui::GetCursorPosX();
                        const auto orig_screen_pos = ImGui::GetCursorScreenPos();
                        //const auto text_vert_spacing = ImGui::GetTextLineHeightWithSpacing();
                        const auto text_vert_spacing = ImGui::GetTextLineHeight();
                        const auto vert_spacing = ImGui::GetStyle().ItemSpacing.y * 0.5f; // Is this correct??? Seems OK, but is arbitrary.
                        const auto horiz_spacing = ImGui::GetStyle().ItemSpacing.x;
                        const float line_no_width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "12345", nullptr, nullptr).x;
                        ImGui::SetCursorPosX( orig_cursor_pos + line_no_width + horiz_spacing );

                        // Draw text entry box.
                        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput
                                                  | ImGuiInputTextFlags_CallbackResize
                                                  | ImGuiInputTextFlags_CallbackEdit;
                        ImVec2 edit_box_extent = ImGui::GetContentRegionAvail();
                        const auto altered = ImGui::InputTextMultiline("#script_editor_active_content",
                                                                       sf_ptr->content.data(),
                                                                       sf_ptr->content.capacity(),
                                                                       edit_box_extent,
                                                                       flags,
                                                                       text_entry_callback,
                                                                       reinterpret_cast<void*>(sf_ptr));
                        if(altered == true) script_files.at(active_script_file).altered = true;

                        //const auto text_entry_ID = ImGui::GetCurrentContext()->LastActiveId; // ActiveIdPreviousFrame;
                        //const auto text_entry_ID = ImGui::GetID("#script_editor_active_content");
                        ImGui::Begin("Script Editor/#script_editor_active_content_9CF9E0D1"); // Terrible hacky workaround. FIXME. TODO.
                        const auto vert_scroll = ImGui::GetScrollY();
                        ImGui::EndChild();

                        // Draw line numbers, including compilation feedback if applicable.
                        {
                            auto drawList = ImGui::GetWindowDrawList();

                            const auto text_ln = static_cast<int>(std::floor(vert_scroll / text_vert_spacing));
                            const auto text_ln_max = std::max(0, text_ln + static_cast<int>(std::floor((vert_scroll + edit_box_extent.y) / text_vert_spacing)));
                            const auto line_vert_shift = (vert_scroll / text_vert_spacing) - static_cast<float>(text_ln);

                            for(int l = text_ln; l < text_ln_max; ++l){ 
                                ImU32 colour = ImGui::GetColorU32(line_numbers_normal_colour);
                                if(view_toggles.view_script_feedback){
                                    for(const auto &f : script_files.at(active_script_file).feedback){
                                        if(l != f.line) continue;
                                        if(false){
                                        }else if(f.severity == script_feedback_severity_t::debug){
                                            colour = ImGui::GetColorU32(line_numbers_debug_colour);
                                        }else if(f.severity == script_feedback_severity_t::info){
                                            colour = ImGui::GetColorU32(line_numbers_info_colour);
                                        }else if(f.severity == script_feedback_severity_t::warn){
                                            colour = ImGui::GetColorU32(line_numbers_warn_colour);
                                        }else if(f.severity == script_feedback_severity_t::err){
                                            colour = ImGui::GetColorU32(line_numbers_error_colour);
                                        }else{
                                            throw std::logic_error("Unrecognized severity level");
                                        }
                                    }
                                }

                                std::stringstream ss;
                                ss << std::setw(5) << l;
                                drawList->AddText(
                                    ImVec2(orig_screen_pos.x,
                                           orig_screen_pos.y + vert_spacing
                                                             + text_vert_spacing * static_cast<float>(l - text_ln)
                                                             - text_vert_spacing * line_vert_shift),
                                    colour, const_cast<char *>(ss.str().c_str()) );
                            }
                        }
                    }
                }

                ImGui::End();
            }
            return;
        };
        try{
            display_script_editor();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_script_editor(): '" << e.what() << "'");
            throw;
        }


        // Display the image dialog.
        const auto display_image_viewer = [&view_toggles,
                                           //&custom_centre,
                                           //&custom_width,
                                           &current_texture,
                                           &uv_min,
                                           &uv_max,
                                           &image_mouse_pos_opt,
                                           &tagged_pos,
                                           &largest_projection,

                                           //&open_file_root,
                                           //&root_entry_text,

                                           //&script_mutex,
                                           //&script_files,
                                           //&active_script_file,
                                           //&script_epoch,

                                           &drover_mutex,
                                           &mutex_dt,
                                           &DICOM_data,
                                           &InvocationMetadata,
                                           &FilenameLex,
                                           &recompute_image_state,
                                           &recompute_image_iters,

                                           &preprocessed_contour_mutex,
                                           &preprocessed_contour_epoch,
                                           &preprocessed_contours,
                                           //&terminate_contour_preprocessors,
                                           &launch_contour_preprocessor,
                                           //&clear_preprocessed_contours,
                                           //&reset_contouring_state,
                                           &contour_colour_from_orientation,
                                           &contour_colours,
                                           &contour_enabled,
                                           &contour_hovered,
                                           &contour_line_thickness,

                                           &contouring_method,
                                           &contouring_reach,
                                           &contouring_brush,
                                           &contouring_margin,
                                           &contouring_intensity,
                                           &contouring_img_row_col_count,
                                           &new_contour_name,
                                           &save_contour_buffer,

                                           &recompute_cimage_iters,
                                           &contouring_imgs,
                                           &contouring_img_altered,
                                           &last_mouse_button_0_down,
                                           &last_mouse_button_1_down,
                                           &last_mouse_button_pos,
                                           &reset_contouring_state,
                                           &need_to_reload_opengl_texture,
                                           &editing_contour_colour,
                                           &pos_contour_colour,
                                           &neg_contour_colour,

                                           &row_profile,
                                           &col_profile,
                                           &time_profile,
                                           &time_course_abscissa_key,
                                           &time_course_image_inclusivity,
                                           &time_course_abscissa_relative,

                                           &display_metadata_table,

                                           &frame_count ]() -> void {

// Lock the image details mutex in shared mode.
// 
// ... TODO ...
//
            if( !view_toggles.view_images_enabled
            ||  !current_texture.texture_exists
            ||  need_to_reload_opengl_texture.load() ) return;

            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_FirstUseEver);
            ImGui::Begin("Images", &view_toggles.view_images_enabled, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar );
            ImGuiIO &io = ImGui::GetIO();

            // Note: unhappy with this. Can cause feedback loop and flicker/jumpiness when resizing. Works OK for now
            // though. TODO.
            ImVec2 image_extent = ImGui::GetContentRegionAvail();
            image_extent.x = std::max(512.0f, image_extent.x - 5.0f);
            // Ensure images have the same aspect ratio as the true image.
            image_extent.y = current_texture.aspect_ratio * image_extent.x;
            auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(current_texture.texture_number));

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::Image(gl_tex_ptr, image_extent, uv_min, uv_max);
            image_mouse_pos_s image_mouse_pos;
            image_mouse_pos.mouse_hovering_image = ImGui::IsItemHovered();
            image_mouse_pos.image_window_focused = ImGui::IsWindowFocused();

            ImVec2 real_extent; // The true dimensions of the unclipped image, accounting for zoom and panning.
            real_extent.x = image_extent.x / (uv_max.x - uv_min.x);
            real_extent.y = image_extent.y / (uv_max.y - uv_min.y);
            ImVec2 real_pos; // The true position of the top-left corner, accounting for zoom and panning.
            real_pos.x = pos.x - (real_extent.x * uv_min.x);
            real_pos.y = pos.y - (real_extent.y * uv_min.y);
            ImGui::End(); // "Images".

            // Attempt to acquire an exclusive lock.
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            if( !view_toggles.view_images_enabled
            ||  !img_valid ) return;

            //We have three distinct coordinate systems: DICOM, pixel coordinates and screen pixel coordinates,
            // and SFML 'world' coordinates. We need to map from the DICOM coordinates to screen pixel coords.
            //
            //Get a DICOM-coordinate bounding box for the image.
            const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->rows;
            const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->columns; 
            const auto img_top_left = disp_img_it->anchor + disp_img_it->offset
                                    - disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5f
                                    - disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5f;
            //const auto img_top_right = img_top_left + disp_img_it->row_unit * img_dicom_width;
            //const auto img_bottom_left = img_top_left + disp_img_it->col_unit * img_dicom_height;
            const auto img_plane = disp_img_it->image_plane();

            // Continue now that we have an exclusive lock.
            ImGui::Begin("Images", &view_toggles.view_images_enabled);
            ImDrawList *imgs_window_draw_list = ImGui::GetWindowDrawList();

            // Calculate mouse positions if the mouse is hovering the image.
            if( image_mouse_pos.mouse_hovering_image ){
                const auto img_rows = current_texture.row_count;
                const auto img_cols = current_texture.col_count;
                const auto img_rows_f = static_cast<float>(img_rows);
                const auto img_cols_f = static_cast<float>(img_cols);
                image_mouse_pos.region_x = std::clamp((io.MousePos.x - real_pos.x) / real_extent.x, 0.0f, 1.0f);
                image_mouse_pos.region_y = std::clamp((io.MousePos.y - real_pos.y) / real_extent.y, 0.0f, 1.0f);
                image_mouse_pos.r = std::clamp( static_cast<long int>( std::floor( image_mouse_pos.region_y * img_rows_f ) ), 0L, (img_rows-1) );
                image_mouse_pos.c = std::clamp( static_cast<long int>( std::floor( image_mouse_pos.region_x * img_cols_f ) ), 0L, (img_cols-1) );
                image_mouse_pos.zero_pos = disp_img_it->position(0L, 0L);
                image_mouse_pos.dicom_pos = image_mouse_pos.zero_pos 
                                            + disp_img_it->row_unit * image_mouse_pos.region_y * disp_img_it->pxl_dx * img_rows_f
                                            + disp_img_it->col_unit * image_mouse_pos.region_x * disp_img_it->pxl_dy * img_cols_f
                                            - disp_img_it->row_unit * 0.5 * disp_img_it->pxl_dx
                                            - disp_img_it->col_unit * 0.5 * disp_img_it->pxl_dy;
                image_mouse_pos.voxel_pos = disp_img_it->position(image_mouse_pos.r, image_mouse_pos.c);
                image_mouse_pos.pixel_scale = static_cast<float>(real_extent.x) / (disp_img_it->pxl_dx * disp_img_it->rows);

                const auto Z = image_mouse_pos.zero_pos;
                image_mouse_pos.DICOM_to_pixels = [=](const vec3<double> &P) -> ImVec2 {
                    // Convert from absolute DICOM coordinates to ImGui screen pixel coordinates for the image.
                    // This routine basically just inverts the above transformation.
                    const auto region_y = (disp_img_it->row_unit.Dot(P - Z) + 0.5 * disp_img_it->pxl_dx)/(disp_img_it->pxl_dx * img_rows_f);
                    const auto region_x = (disp_img_it->col_unit.Dot(P - Z) + 0.5 * disp_img_it->pxl_dy)/(disp_img_it->pxl_dy * img_cols_f);

                    const auto pixel_x = pos.x + (region_x - uv_min.x) * image_extent.x/(uv_max.x - uv_min.x);
                    const auto pixel_y = pos.y + (region_y - uv_min.y) * image_extent.y/(uv_max.y - uv_min.y);

                    return ImVec2(pixel_x, pixel_y);
                };
            }

            // Display a visual cue of the tagged position.
            if( tagged_pos ){
                const auto box_radius = 3.0f;
                const auto c = ImColor(1.0f, 0.2f, 0.2f, 1.0f);

                ImVec2 p1 = image_mouse_pos.DICOM_to_pixels(tagged_pos.value());
                ImVec2 ul1( p1.x - box_radius, p1.y - box_radius );
                ImVec2 lr1( p1.x + box_radius, p1.y + box_radius );
                imgs_window_draw_list->AddRect(ul1, lr1, c);

                if( image_mouse_pos.mouse_hovering_image ){
                    ImVec2 p2 = io.MousePos;
                    
                    // Project along the image axes to provide a guide line.
                    if(io.KeyCtrl){
                        p2 = image_mouse_pos.DICOM_to_pixels(
                                 largest_projection( tagged_pos.value(),
                                                     image_mouse_pos.dicom_pos,
                                                     { disp_img_it->row_unit,
                                                       disp_img_it->col_unit,
                                                       (disp_img_it->row_unit + disp_img_it->col_unit) * 0.5,
                                                       (disp_img_it->row_unit - disp_img_it->col_unit) * 0.5 } ) );
                    }
                    ImVec2 ul2( p2.x - box_radius, p2.y - box_radius );
                    ImVec2 lr2( p2.x + box_radius, p2.y + box_radius );
                    imgs_window_draw_list->AddRect(ul2, lr2, c);

                    // Connect the boxes with a line if both are contained within the same image volume.
                    if( disp_img_it->sandwiches_point_within_top_bottom_planes(tagged_pos.value())
                    &&  disp_img_it->sandwiches_point_within_top_bottom_planes(image_mouse_pos.dicom_pos) ){
                        imgs_window_draw_list->AddLine(p1, p2, c);
                    }
                }
            }

            // Display a contour legend.
            if( view_toggles.view_contours_enabled
            &&  (DICOM_data.contour_data != nullptr) ){
                ImGui::SetNextWindowSize(ImVec2(510, 500), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(680, 40), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
                ImGui::Begin("Contours", &view_toggles.view_contours_enabled);
                ImVec2 window_extent = ImGui::GetContentRegionAvail();
                bool altered = false;

                ImGui::Text("Contour colour");
                if(ImGui::Button("Unique", ImVec2(window_extent.x/2, 0))){ 
                    std::unique_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                    contour_colour_from_orientation = false;
                    contour_colours.clear();
                    altered = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Orientation", ImVec2(window_extent.x/2, 0))){ 
                    std::unique_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                    contour_colour_from_orientation = true;
                    contour_colours.clear();
                    altered = true;
                }

                decltype(contour_colours) contour_colours_l;
                bool contour_colour_from_orientation_l;
                {
                    std::shared_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                    contour_colours_l = contour_colours;
                    contour_colour_from_orientation_l = contour_colour_from_orientation;
                }
                for(auto &p : contour_colours_l){
                    if(contour_enabled.count(p.first) == 0) contour_enabled[p.first] = true;
                    if(contour_hovered.count(p.first) == 0) contour_hovered[p.first] = false;
                }

                ImGui::Text("Contour display");
                if(ImGui::Button("All", ImVec2(window_extent.x/3, 0))){ 
                    for(auto &p : contour_enabled) p.second = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("None", ImVec2(window_extent.x/3, 0))){ 
                    for(auto &p : contour_enabled) p.second = false;
                }
                ImGui::SameLine();
                if(ImGui::Button("Invert", ImVec2(window_extent.x/3, 0))){ 
                    for(auto &p : contour_enabled) p.second = !p.second;
                }

                float contour_line_thickness_l = 0.1f;
                float contour_line_thickness_h = 5.0f;
                const float drag_speed = 0.01f;

                ImGui::DragScalar("Line thickness",
                                  ImGuiDataType_Float,
                                  &contour_line_thickness,
                                  drag_speed,
                                  &contour_line_thickness_l,
                                  &contour_line_thickness_h,
                                  "%.1f");

                ImGui::Text("Contours");
                for(auto &cc_p : contour_colours_l){
                    auto ROIName = cc_p.first;
                    auto checkbox_id = ("##contour_checkbox_"_s + ROIName);
                    auto colour_id = ("##contour_colour_"_s + ROIName);

                    ImGui::Checkbox(checkbox_id.c_str(), &(contour_enabled[ROIName]));
                    if(!contour_colour_from_orientation_l){
                        ImGui::SameLine();
                        if(ImGui::ColorEdit4(colour_id.c_str(), &(cc_p.second.x))){
                            altered = true;
                        }
                    }
                    ImGui::SameLine();
                    if(contour_hovered[ROIName]){
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s*", ROIName.c_str());
                    }else{
                        ImGui::Text("%s", ROIName.c_str());
                    }
                    // Display (read-only) metadata when hovering.
                    if( ImGui::IsItemHovered() 
                    &&  view_toggles.view_plots_metadata ){
                        ImGui::SetNextWindowSize(ImVec2(600, -1));
                        ImGui::BeginTooltip();
                        ImGui::Text("Shared Contour Metadata");
                        ImGui::Columns(2, "Plot Metadata", true);
                        ImGui::Separator();
                        ImGui::Text("Key"); ImGui::NextColumn();
                        ImGui::Text("Value"); ImGui::NextColumn();
                        ImGui::Separator();

                        // Extract common metadata for all like-named contours.
                        const auto regex_escaped_ROIName = [&](){
                            std::string out;
                            for(const auto &c : ROIName) out += "["_s + c + "]";
                            return out;
                        }();
                        auto cc_all = All_CCs( DICOM_data );
                        auto cc_ROIs = Whitelist( cc_all, { { "ROIName", regex_escaped_ROIName } });
                        metadata_multimap_t shared_metadata;
                        for(auto &cc_refw : cc_ROIs){
                            for(auto &c : cc_refw.get().contours){
                                combine_distinct(shared_metadata, c.metadata);
                            }
                        }
                        for(const auto& [key, val] : singular_keys(shared_metadata)){
                            ImGui::Text("%s", key.c_str()); ImGui::NextColumn();
                            ImGui::Text("%s", val.c_str()); ImGui::NextColumn();
                        }
                        ImGui::EndTooltip();
                    }
                }

                if(altered){
                    std::unique_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                    contour_colours = contour_colours_l;
                    if(view_toggles.view_contours_enabled) launch_contour_preprocessor();
                }
                ImGui::End();
            }

            //Draw any contours that lie in the plane of the current image.
            if( view_toggles.view_contours_enabled
            &&  (DICOM_data.contour_data != nullptr) ){

                for(auto &p : contour_hovered) p.second = false;

                std::shared_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                const auto current_epoch = preprocessed_contour_epoch.load();
                for(const auto &pc : preprocessed_contours){
                    if( pc.epoch != current_epoch ) continue;
                    if( !contour_enabled[pc.ROIName] ) continue;

                    imgs_window_draw_list->PathClear();
                    for(auto & p : pc.contour.points){
                        //Clamp the point to the bounding box, using the top left as zero.
                        const auto dR = p - img_top_left;
                        const auto clamped_col = dR.Dot( disp_img_it->col_unit ) / img_dicom_height;
                        const auto clamped_row = dR.Dot( disp_img_it->row_unit ) / img_dicom_width;

                        //Convert to ImGui coordinates using the top-left position of the display image.
                        const auto world_x = real_pos.x + real_extent.x * clamped_col;
                        const auto world_y = real_pos.y + real_extent.y * clamped_row;

                        ImVec2 v;
                        v.x = world_x;
                        v.y = world_y;
                        imgs_window_draw_list->PathLineTo( v );
                    }

                    // Check if the mouse if within the contour.
                    float thickness = contour_line_thickness;
                    if(image_mouse_pos.mouse_hovering_image){
                        const auto within_poly = pc.contour.Is_Point_In_Polygon_Projected_Orthogonally(img_plane,image_mouse_pos.dicom_pos);
                        thickness *= ( within_poly ) ? 1.5f : 1.0f;
                        if(within_poly) contour_hovered[pc.ROIName] = true;
                    }
                    const bool closed = true;
                    imgs_window_draw_list->PathStroke(pc.colour, closed, thickness);
                    //AddPolyline(const ImVec2* points, int num_points, ImU32 col, bool closed, float thickness);
                }
            }

            // Contouring and drawing interface.
            if( view_toggles.view_contouring_enabled
            ||  view_toggles.view_drawing_enabled ){
                // Provide a visual cue for the contouring brush.
                {
                    const auto pixel_radius = static_cast<float>(contouring_reach) * image_mouse_pos.pixel_scale;
                    const auto c = ImColor(0.0f, 1.0f, 0.8f, 1.0f);

                    if( (contouring_brush == brush_t::rigid_circle)
                    ||  (contouring_brush == brush_t::rigid_sphere)
                    ||  (contouring_brush == brush_t::gaussian_2D)
                    ||  (contouring_brush == brush_t::tanh_2D)
                    ||  (contouring_brush == brush_t::gaussian_3D)
                    ||  (contouring_brush == brush_t::tanh_3D)
                    ||  (contouring_brush == brush_t::median_circle)
                    ||  (contouring_brush == brush_t::mean_circle)
                    ||  (contouring_brush == brush_t::median_sphere)
                    ||  (contouring_brush == brush_t::mean_sphere) ){
                        imgs_window_draw_list->AddCircle(io.MousePos, pixel_radius, c);

                    }else if( (contouring_brush == brush_t::rigid_square)
                          ||  (contouring_brush == brush_t::median_square)
                          ||  (contouring_brush == brush_t::mean_square)
                          ||  (contouring_brush == brush_t::rigid_cube)
                          ||  (contouring_brush == brush_t::median_cube)
                          ||  (contouring_brush == brush_t::mean_cube) ){
                        ImVec2 ul( io.MousePos.x - pixel_radius,
                                   io.MousePos.y - pixel_radius );
                        ImVec2 lr( io.MousePos.x + pixel_radius,
                                   io.MousePos.y + pixel_radius );
                        imgs_window_draw_list->AddRect(ul, lr, c);
                    }
                }

                ImGui::SetNextWindowSize(ImVec2(510, 650), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(680, 400), ImGuiCond_FirstUseEver);
                //ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
                if(view_toggles.view_drawing_enabled){
                    ImGui::Begin("Drawing", &view_toggles.view_drawing_enabled, ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("Note: this functionality is still under active development.");

                }else if(view_toggles.view_contouring_enabled){
                    ImGui::Begin("Contouring", &view_toggles.view_contouring_enabled, ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("Note: this functionality is still under active development.");
                    if(ImGui::Button("Save")){ 
                        ImGui::OpenPopup("Save Contours");

                        // Fully extract contours from the mask images.
                        contouring_imgs.Ensure_Contour_Data_Allocated();
                        contouring_imgs.contour_data->ccs.clear();

                        std::list<OperationArgPkg> Operations;
                        Operations.emplace_back("ContourViaThreshold");
                        Operations.back().insert("Method="_s + contouring_method);
                        Operations.back().insert("Lower=0.5");
                        Operations.back().insert("SimplifyMergeAdjacent=true");
                        if(!Operation_Dispatcher(contouring_imgs, InvocationMetadata, FilenameLex, Operations)){
                            FUNCWARN("ContourViaThreshold failed");
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::BeginPopupModal("Save Contours", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                        const std::string str((frame_count / 15) % 4, '.'); // Simplistic animation.
                        ImGui::Text("Saving contours%s", str.c_str());

                        ImGui::InputText("ROI Name", new_contour_name.data(), new_contour_name.size());
                        std::string entered_text;
                        for(size_t i = 0; i < new_contour_name.size(); ++i){
                            if( (new_contour_name[i] == '\0')
                            ||  !std::isprint( static_cast<unsigned char>(new_contour_name[i]) )) break;
                            entered_text.push_back(new_contour_name[i]);
                        }
                        const auto ok_to_save = !entered_text.empty();
                        //if(!ok_to_save){
                        //    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                        //    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                        //}
                        const bool clicked_save = ImGui::Button("Save");
                        //if(!ok_to_save){
                        //    ImGui::PopItemFlag();
                        //    ImGui::PopStyleVar();
                        //}
                        if( clicked_save
                        &&  ok_to_save
                        &&  save_contour_buffer(entered_text) ){
                            ImGui::CloseCurrentPopup();
                        }

                        if(ImGui::Button("Close")){
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                    if(ImGui::Button("Clear")){ 
                        ImGui::OpenPopup("Contour Clear");
                    }
                    if(ImGui::BeginPopupModal("Contour Clear", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                        ImGui::Text("Clear contour?");
                        if(ImGui::Button("Clear")){
                            ImGui::CloseCurrentPopup();
                            auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();
                            if(cimg_valid){
                                for(auto& cimg : (*cimg_array_ptr_it)->imagecoll.images){
                                    cimg.fill_pixels(0.0f);
                                }
                            }
                            contouring_imgs.Ensure_Contour_Data_Allocated();
                            contouring_imgs.contour_data->ccs.clear();
                            contouring_img_altered = true;
                            last_mouse_button_0_down = 1E30;
                            last_mouse_button_1_down = 1E30;
                            last_mouse_button_pos = {};
                        }
                        ImGui::SameLine();
                        if(ImGui::Button("Cancel")){
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                }

                ImGui::Separator();
                ImGui::Text("Brush");
                ImGui::DragFloat("Radius (mm)", &contouring_reach, 0.1f, 0.5f, 50.0f);
                if(view_toggles.view_drawing_enabled){
                    ImGui::DragFloat("Intensity", &contouring_intensity, 0.1f, -1000.0f, 1000.0f);
                }else if(view_toggles.view_contouring_enabled){
                    contouring_intensity = 1.0f;
                }

                ImGui::Text("2D shapes");
                if(ImGui::Button("Rigid Circle")){
                    contouring_brush = brush_t::rigid_circle;
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Circle")){
                    contouring_brush = brush_t::mean_circle;
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Circle")){
                    contouring_brush = brush_t::median_circle;
                }

                if(ImGui::Button("Rigid Square")){
                    contouring_brush = brush_t::rigid_square;
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Square")){
                    contouring_brush = brush_t::mean_square;
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Square")){
                    contouring_brush = brush_t::median_square;
                }

                if(ImGui::Button("2D Gaussian")){
                    contouring_brush = brush_t::gaussian_2D;
                }
                ImGui::SameLine();
                if(ImGui::Button("2D Tanh")){
                    contouring_brush = brush_t::tanh_2D;
                }

                ImGui::Text("3D shapes");
                if(ImGui::Button("Rigid Sphere")){
                    contouring_brush = brush_t::rigid_sphere;
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Sphere")){
                    contouring_brush = brush_t::mean_sphere;
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Sphere")){
                    contouring_brush = brush_t::median_sphere;
                }

                if(ImGui::Button("Rigid Cube")){
                    contouring_brush = brush_t::rigid_cube;
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Cube")){
                    contouring_brush = brush_t::mean_cube;
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Cube")){
                    contouring_brush = brush_t::median_cube;
                }

                if(ImGui::Button("3D Gaussian")){
                    contouring_brush = brush_t::gaussian_3D;
                }
                ImGui::SameLine();
                if(ImGui::Button("3D Tanh")){
                    contouring_brush = brush_t::tanh_3D;
                }

                ImGui::Separator();
                ImGui::Text("Dilation and Erosion");
                ImGui::DragFloat("Margin (mm)", &contouring_margin, 0.1f, -10.0f, 10.0f);
                if(ImGui::Button("Apply Margin")){
                    std::list<OperationArgPkg> Operations;
                    Operations.emplace_back("ContourWholeImages");
                    Operations.back().insert("ROILabel=___whole_image");

                    Operations.emplace_back("ReduceNeighbourhood");
                    Operations.back().insert("ImageSelection=last");
                    Operations.back().insert("ROILabelRegex=___whole_image");
                    Operations.back().insert("Neighbourhood=spherical");
                    
                    const std::string reduction = (0.0 <= contouring_margin) ? "dilate" : "erode";
                    const std::string distance = std::to_string( std::abs( contouring_margin ) );
                    Operations.back().insert("Reduction="_s + reduction);
                    Operations.back().insert("MaxDistance="_s + distance);

                    Operations.emplace_back("DeleteContours");
                    Operations.back().insert("ROILabelRegex=___whole_image");

                    Drover *d = (view_toggles.view_contouring_enabled) ? &contouring_imgs : &DICOM_data;
                    if(!Operation_Dispatcher(*d, InvocationMetadata, FilenameLex, Operations)){
                        FUNCWARN("Dilation/Erosion failed");
                    }

                    if(view_toggles.view_contouring_enabled){
                        contouring_img_altered = true;
                    }else if(view_toggles.view_drawing_enabled){
                        need_to_reload_opengl_texture.store(true);
                    }
                }

                if(view_toggles.view_contouring_enabled){
                    ImGui::Separator();
                    ImGui::Text("Contour Extraction");
                    if(ImGui::DragInt("Resolution", &contouring_img_row_col_count, 0.1f, 5, 1024)){
                        reset_contouring_state(img_array_ptr_it);
                    }
                    if( ImGui::IsItemHovered() ){
                        ImGui::BeginTooltip();
                        ImGui::Text("Note: any existing contours will be reset.");
                        ImGui::EndTooltip();
                    }
                    if(ImGui::Button("Marching squares")){
                        contouring_method = "marching-squares";
                        contouring_img_altered = true;
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Binary")){
                        contouring_method = "binary";
                        contouring_img_altered = true;
                    }

                    // Regenerate contours from the mask.
                    contouring_imgs.Ensure_Contour_Data_Allocated();
                    auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();
                    if( cimg_valid
                    &&  contouring_img_altered
                    &&  (frame_count % 5 == 0) ){ // Terrible stop-gap until I can parallelize contour extraction. TODO

                        // Only bother extracting contours for the current image.
                        Drover shtl;
                        shtl.Ensure_Contour_Data_Allocated();
                        shtl.image_data.push_back(std::make_unique<Image_Array>());
                        shtl.image_data.back()->imagecoll.images.emplace_back();
                        shtl.image_data.back()->imagecoll.images.back() = *cimg_it;

                        std::list<OperationArgPkg> Operations;
                        Operations.emplace_back("ContourViaThreshold");
                        Operations.back().insert("Method="_s + contouring_method);
                        Operations.back().insert("Lower=0.5");
                        Operations.back().insert("SimplifyMergeAdjacent=true");
                        if(!Operation_Dispatcher(shtl, InvocationMetadata, FilenameLex, Operations)){
                            FUNCWARN("ContourViaThreshold failed");
                        }

                        contouring_imgs.contour_data->ccs.clear();
                        contouring_imgs.Consume( shtl.contour_data );
//FUNCINFO("Contouring image generated " << contouring_imgs.contour_data->ccs.size() << " contour collections");
//if(!contouring_imgs.contour_data->ccs.empty()){
//    FUNCINFO("    First collection contains " << contouring_imgs.contour_data->ccs.front().contours.size() << " contours");
//}


                        contouring_img_altered = false;
                    }

                    // Draw the WIP contours.
                    if( cimg_valid
                    &&  (contouring_imgs.Has_Contour_Data()) ){
                        const auto cimg_dicom_width = cimg_it->pxl_dx * cimg_it->rows;
                        const auto cimg_dicom_height = cimg_it->pxl_dy * cimg_it->columns; 
                        //const auto cimg_top_left = cimg_it->anchor + cimg_it->offset
                        //                         - cimg_it->row_unit * cimg_it->pxl_dx * 0.5f
                        //                         - cimg_it->col_unit * cimg_it->pxl_dy * 0.5f;
                        //const auto cimg_top_right = cimg_top_left + cimg_it->row_unit * cimg_dicom_width;
                        //const auto cimg_bottom_left = cimg_top_left + cimg_it->col_unit * cimg_dicom_height;
                        //const auto cimg_plane = cimg_it->image_plane();

                        for(auto &cc : contouring_imgs.contour_data->ccs){
                            for(auto &cop : cc.contours){
                                if( cop.points.empty() ) continue;
                                if( !cimg_it->sandwiches_point_within_top_bottom_planes( cop.points.front() ) ) continue;

                                imgs_window_draw_list->PathClear();
                                for(auto & p : cop.points){

                                    //Clamp the point to the bounding box, using the top left as zero.
                                    const auto dR = p - img_top_left;
                                    const auto clamped_col = dR.Dot( cimg_it->col_unit ) / cimg_dicom_height;
                                    const auto clamped_row = dR.Dot( cimg_it->row_unit ) / cimg_dicom_width;

                                    //Convert to ImGui coordinates using the top-left position of the display image.
                                    const auto world_x = real_pos.x + real_extent.x * clamped_col;
                                    const auto world_y = real_pos.y + real_extent.y * clamped_row;

                                    ImVec2 v;
                                    v.x = world_x;
                                    v.y = world_y;
                                    imgs_window_draw_list->PathLineTo( v );
                                }

                                float thickness = contour_line_thickness;

                                ImU32 colour = ImGui::GetColorU32(editing_contour_colour);
                                if(contour_colour_from_orientation){
                                    const auto arb_pos_unit = disp_img_it->row_unit.Cross(disp_img_it->col_unit).unit();
                                    vec3<double> c_orient;
                                    try{ // Protect against degenerate contours. (Should we instead ignore them altogether?)
                                        c_orient = cop.Estimate_Planar_Normal();
                                    }catch(const std::exception &){
                                        c_orient = arb_pos_unit;
                                    }
                                    const auto c_orient_pos = (c_orient.Dot(arb_pos_unit) > 0);
                                    colour = ( c_orient_pos ? ImGui::GetColorU32(pos_contour_colour)
                                                            : ImGui::GetColorU32(neg_contour_colour) );
                                }

                                const bool closed = true;
                                imgs_window_draw_list->PathStroke( colour, closed, thickness);
                                //AddPolyline(const ImVec2* points, int num_points, ImU32 col, bool closed, float thickness);
                            }
                        }
                    }
                }
                ImGui::End();
            }

            // Draw a tooltip with position and voxel intensity information.
            if( image_mouse_pos.mouse_hovering_image
            &&  view_toggles.show_image_hover_tooltips
            &&  !view_toggles.view_contouring_enabled ){
                ImGui::BeginTooltip();
                if(tagged_pos){
                    ImGui::Text("Distance: %.4f", tagged_pos.value().distance(image_mouse_pos.dicom_pos));
                }
                ImGui::Text("Image coordinates: %.4f, %.4f", image_mouse_pos.region_y, image_mouse_pos.region_x);
                ImGui::Text("Pixel coordinates: (r, c) = %ld, %ld", image_mouse_pos.r, image_mouse_pos.c);
                ImGui::Text("Mouse coordinates: (x, y, z) = %.4f, %.4f, %.4f", image_mouse_pos.dicom_pos.x, image_mouse_pos.dicom_pos.y, image_mouse_pos.dicom_pos.z);
                ImGui::Text("Voxel coordinates: (x, y, z) = %.4f, %.4f, %.4f", image_mouse_pos.voxel_pos.x, image_mouse_pos.voxel_pos.y, image_mouse_pos.voxel_pos.z);
                if(disp_img_it->channels == 1){
                    ImGui::Text("Voxel intensity:   %.4f", disp_img_it->value(image_mouse_pos.r, image_mouse_pos.c, 0L));
                    try{
                        const auto frc = disp_img_it->fractional_row_column(image_mouse_pos.dicom_pos);
                        const auto bilin_interp = disp_img_it->bilinearly_interpolate_in_pixel_number_space(frc.first, frc.second, 0L);
                        ImGui::Text("Mouse intensity:   %.4f (lin. interp. at %.4f, %.4f)", bilin_interp, frc.first, frc.second);
                    }catch(const std::exception &){}
                }else{
                    std::stringstream ss;
                    for(long int chan = 0; chan < disp_img_it->channels; ++chan){
                        ss << disp_img_it->value(image_mouse_pos.r, image_mouse_pos.c, chan) << " ";
                    }
                    ImGui::Text("Voxel intensities: %s", ss.str().c_str());
                }
                ImGui::EndTooltip();
            }
            ImGui::End();

            // Extract data for row and column profiles.
            if( image_mouse_pos.mouse_hovering_image
            &&  view_toggles.view_row_column_profiles ){
                row_profile.samples.clear();
                col_profile.samples.clear();
                for(auto i = 0; i < disp_img_it->columns; ++i){
                    const auto val_raw = disp_img_it->value(image_mouse_pos.r,i,0);
                    const auto col_num = static_cast<double>(i);
                    if(std::isfinite(val_raw)) row_profile.push_back({ col_num, 0.0, val_raw, 0.0 });
                }
                for(auto i = 0; i < disp_img_it->rows; ++i){
                    const auto val_raw = disp_img_it->value(i,image_mouse_pos.c,0);
                    const auto row_num = static_cast<double>(i);
                    if(std::isfinite(val_raw)) col_profile.push_back({ row_num, 0.0, val_raw, 0.0 });
                }
            }

            // Extract data for time profiles.
            if( image_mouse_pos.mouse_hovering_image
            &&  view_toggles.view_time_profiles ){
                time_profile.samples.clear();
                time_profile.metadata.clear();

                std::string abscissa_key; //As it appears in the metadata. Must convert to a double!
                array_to_string(abscissa_key, time_course_abscissa_key);
                const auto meta_key = disp_img_it->GetMetadataValueAs<double>(abscissa_key);

                double n_img = 0.0;
                const bool sort_on_append = false;

                const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
                const std::list<vec3<double>> points = { image_mouse_pos.dicom_pos,
                                                         image_mouse_pos.dicom_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                         image_mouse_pos.dicom_pos - ortho * disp_img_it->pxl_dz * 0.25 };

                decltype((*img_array_ptr_it)->imagecoll.get_all_images()) selected_imgs;
                if(time_course_image_inclusivity == time_course_image_inclusivity_t::current){
                    auto encompassing_images = (*img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);
                    selected_imgs.splice( std::end(selected_imgs), encompassing_images);

                }else if(time_course_image_inclusivity == time_course_image_inclusivity_t::all){
                    for(const auto &img_arr_ptr : DICOM_data.image_data){
                        auto encompassing_images = img_arr_ptr->imagecoll.get_images_which_encompass_all_points(points);
                        selected_imgs.splice( std::end(selected_imgs), encompassing_images);
                    }

                }else{
                    throw std::invalid_argument("Unrecognized abscissa inclusisivity");
                }
                auto common_metadata = planar_image_collection<float,double>().get_common_metadata(selected_imgs);
                common_metadata = coalesce_metadata_for_lsamp(common_metadata);

                //Cycle over the images, dumping the ordinate (pixel values) vs abscissa (time) derived from metadata.
                long int n_current_img = 0;
                for(const auto &enc_img_it : selected_imgs){
                    const auto l_meta_key = enc_img_it->GetMetadataValueAs<double>(abscissa_key);
                    if(l_meta_key.has_value() != meta_key.has_value()) continue;
                    const auto abscissa = l_meta_key.value_or(n_img);

                    if(std::addressof(*disp_img_it) == std::addressof(*enc_img_it)) n_current_img = n_img;
                    try{
                        const auto val_raw = enc_img_it->value(image_mouse_pos.dicom_pos, 0);
                        if(std::isfinite(val_raw)){
                            time_profile.push_back( abscissa, 0.0, 
                                                    static_cast<double>(val_raw), 0.0,
                                                    sort_on_append );
                        }
                    }catch(const std::exception &){ }
                    n_img += 1.0;
                }
                time_profile.stable_sort();
                time_profile.metadata = common_metadata;
                time_profile.metadata["Abscissa"] = (meta_key) ? abscissa_key : "Image Number";
                time_profile.metadata["CurrentAbscissa"] = (meta_key) ? std::to_string(meta_key.value()) : std::to_string(n_current_img);

                if( time_course_abscissa_relative
                &&  !time_profile.samples.empty() ){
                    const auto first_a = time_profile.Get_Extreme_Datum_x().first[0];
                    time_profile = time_profile.Sum_x_With(-first_a);
                    apply_as<double>(time_profile.metadata, "CurrentAbscissa",
                                     [first_a](double x){ return x - first_a; });
                }
            }

            // Image metadata window.
            if( view_toggles.view_image_metadata_enabled ){
                ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::Begin("Image Metadata", &view_toggles.view_image_metadata_enabled);

                display_metadata_table( disp_img_it->metadata );

                ImGui::End();
            }
            image_mouse_pos_opt = image_mouse_pos;
            return;
        };
        try{
            display_image_viewer();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_image_viewer(): '" << e.what() << "'");
            throw;
        }


        // Handle insertion for the file loading future.
        const auto handle_file_loading = [&view_toggles,

                                          &drover_mutex,
                                          &DICOM_data,
                                          &InvocationMetadata,
                                          &recompute_image_state,
                                          &reload_image_texture,
                                          &recompute_image_iters,
                                          &need_to_reload_opengl_texture,
                                          &tagged_pos,

                                          &launch_contour_preprocessor,
                                          &reset_contouring_state,

                                          &loaded_files,

                                          &frame_count]() -> void {
            if( loaded_files.valid() ){
                ImGui::OpenPopup("Loading");
                if(ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                    const std::string str((frame_count / 15) % 4, '.'); // Simplistic animation.
                    ImGui::Text("Loading files%s", str.c_str());

                    if(ImGui::Button("Close")){
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if(std::future_status::ready == loaded_files.wait_for(std::chrono::microseconds(1))){
                    std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
                    auto f = loaded_files.get();

                    if(f.res){
                        DICOM_data.Consume(f.DICOM_data);
                        f.InvocationMetadata.merge(InvocationMetadata);
                        InvocationMetadata = f.InvocationMetadata;
                    }else{
                        FUNCWARN("Unable to load files");
                        // TODO ... warn about the issue.
                    }

                    view_toggles.open_files_enabled = false;
                    recompute_image_state();
                    need_to_reload_opengl_texture.store(true);
                    loaded_files = decltype(loaded_files)();
                    auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                    if(img_valid){
                        if(view_toggles.view_contours_enabled) launch_contour_preprocessor();
                        reset_contouring_state(img_array_ptr_it);
                        tagged_pos = {};
                    }

                }
            }
        };
        try{
            handle_file_loading();
        }catch(const std::exception &e){
            FUNCWARN("Exception in handle_file_loading(): '" << e.what() << "'");
            throw;
        }

        // Handle insertion for the script loading future.
        const auto handle_script_loading = [&view_toggles,

                                            &script_mutex,
                                            &loaded_scripts,
                                            &script_files,
                                            &active_script_file,

                                            &frame_count]() -> void {
            if( loaded_scripts.valid() ){
                ImGui::OpenPopup("Loading");
                if(ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                    const std::string str((frame_count / 15) % 4, '.'); // Simplistic animation.
                    ImGui::Text("Loading files%s", str.c_str());

                    if(ImGui::Button("Close")){
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                if(std::future_status::ready == loaded_scripts.wait_for(std::chrono::microseconds(1))){
                    std::unique_lock<std::shared_mutex> script_lock(script_mutex);
                    auto f = loaded_scripts.get();

                    if(f.res){
                        script_files.insert( std::end(script_files), std::begin(f.script_files),
                                                                     std::end(f.script_files) );
                        active_script_file = static_cast<long int>(script_files.size()) - 1;
                    }else{
                        FUNCWARN("Unable to load scripts");
                        // TODO ... warn about the issue.
                    }
                    view_toggles.open_files_enabled = false;

                    loaded_scripts = decltype(loaded_scripts)();
                }
            }
        };
        try{
            handle_script_loading();
        }catch(const std::exception &e){
            FUNCWARN("Exception in handle_script_loading(): '" << e.what() << "'");
            throw;
        }


        // Adjust the window and level.
        const auto adjust_window_level = [&view_toggles,
                                          &drover_mutex,
                                          &recompute_image_state,

                                          &custom_low,
                                          &custom_high,
                                          &custom_width,
                                          &custom_centre ]() -> void {
            if( view_toggles.adjust_window_level_enabled ){
                ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);
                ImGui::Begin("Adjust Window and Level", &view_toggles.adjust_window_level_enabled);
                bool reload_texture = false;
                const auto unset_custom_wllh = [&](){
                    custom_low    = std::optional<double>();
                    custom_high   = std::optional<double>();
                    custom_width  = std::optional<double>();
                    custom_centre = std::optional<double>();
                };
                const auto sync_custom_wllh = [&](){
                    if(custom_low && custom_high){
                        custom_width = custom_high.value() - custom_low.value();
                        custom_centre = (custom_high.value() + custom_low.value()) * 0.5;
                    }else if( custom_width && custom_centre ){
                        custom_low  = custom_centre.value() - custom_width.value() * 0.5;
                        custom_high = custom_centre.value() + custom_width.value() * 0.5;
                    }
                };

                if(ImGui::Button("Auto", ImVec2(120, 0))){
                    // Invalidate any custom window and level.
                    unset_custom_wllh();
                    reload_texture = true;
                }

                ImGui::Text("CT Presets");
                if(ImGui::Button("Abdomen", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 400.0;
                    custom_centre  = 40.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Bone", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 2000.0;
                    custom_centre  = 500.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Brain", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 70.0;
                    custom_centre  = 30.0;
                    reload_texture = true;
                }

                if(ImGui::Button("Liver", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 160.0;
                    custom_centre  = 60.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Lung", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 1600.0;
                    custom_centre  = -600.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Mediastinum", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 500.0;
                    custom_centre  = 50.0;
                    reload_texture = true;
                }

                ImGui::Text("QA Presets");
                if(ImGui::Button("0 - 1", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 1.0;
                    custom_centre  = 0.5;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("0 - 5", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 5.0;
                    custom_centre  = 2.5;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("0 - 10", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 10.0;
                    custom_centre  = 5.0;
                    reload_texture = true;
                }

                if(ImGui::Button("0 - 100", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 100.0;
                    custom_centre  = 50.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("0 - 1000", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 1000.0;
                    custom_centre  = 500.0;
                    reload_texture = true;
                }

                if(ImGui::Button("-1 - 1", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 2.0;
                    custom_centre  = 0.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("-5 - 5", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 10.0;
                    custom_centre  = 0.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("-10 - 10", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 20.0;
                    custom_centre  = 0.0;
                    reload_texture = true;
                }

                if(ImGui::Button("-100 - 100", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 200.0;
                    custom_centre  = 0.0;
                    reload_texture = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("-1000 - 1000", ImVec2(100, 0))){
                    unset_custom_wllh();
                    custom_width   = 2000.0;
                    custom_centre  = 0.0;
                    reload_texture = true;
                }

                ImGui::Text("Custom");
                const double clamp_l = -5000.0;
                const double clamp_h  = 5000.0;
                const float drag_speed = 1.0f;
                double custom_width_l  = custom_width.value_or(0.0);
                double custom_centre_l = custom_centre.value_or(0.0);
                double custom_low_l    = custom_low.value_or(0.0);
                double custom_high_l   = custom_high.value_or(0.0);

                if(ImGui::DragScalar("window", ImGuiDataType_Double, &custom_width_l, drag_speed, &clamp_l, &clamp_h, "%f")){//, ImGuiSliderFlags_Logarithmic)){
                    custom_width = custom_width_l;
                    custom_low  = std::optional<double>();
                    custom_high = std::optional<double>();
                    if(custom_centre){
                        reload_texture = true;
                    }
                }
                if(ImGui::DragScalar("level",  ImGuiDataType_Double, &custom_centre_l, drag_speed, &clamp_l, &clamp_h, "%f")){//, ImGuiSliderFlags_Logarithmic)){
                    custom_centre = custom_centre_l;
                    custom_low  = std::optional<double>();
                    custom_high = std::optional<double>();
                    if(custom_width){
                        reload_texture = true;
                    }
                }

                if(ImGui::DragScalar("low", ImGuiDataType_Double, &custom_low_l, drag_speed, &clamp_l, &clamp_h, "%f")){//, ImGuiSliderFlags_Logarithmic)){
                    custom_low    = custom_low_l;
                    custom_width  = std::optional<double>();
                    custom_centre = std::optional<double>();
                    if(custom_high){
                        reload_texture = true;
                    }
                }
                if(ImGui::DragScalar("high", ImGuiDataType_Double, &custom_high_l, drag_speed, &clamp_l, &clamp_h, "%f")){//, ImGuiSliderFlags_Logarithmic)){
                    custom_high   = custom_high_l;
                    custom_width  = std::optional<double>();
                    custom_centre = std::optional<double>();
                    if(custom_low){
                        reload_texture = true;
                    }
                }

                ImGui::End();
                if(reload_texture){
                    std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
                    sync_custom_wllh();
                    recompute_image_state();
                }
            }
            return;
        };
        try{
            adjust_window_level();
        }catch(const std::exception &e){
            FUNCWARN("Exception in adjust_window_level(): '" << e.what() << "'");
            throw;
        }


        // Adjust the colour map.
        const auto adjust_colour_map = [&view_toggles,
                                        &drover_mutex,
                                        &recompute_image_state,

                                        &scale_bar_texture,
                                        &recompute_scale_bar_image_state,
                                        &colour_maps,
                                        &colour_map ]() -> void {
            if( view_toggles.adjust_colour_map_enabled ){
                ImGui::SetNextWindowPos(ImVec2(680, 120), ImGuiCond_FirstUseEver);
                ImGui::Begin("Adjust Colour Map", &view_toggles.adjust_colour_map_enabled, ImGuiWindowFlags_AlwaysAutoResize);
                bool reload_texture = false;

                // Draw the scale bar.
                auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(scale_bar_texture.texture_number));
                ImGui::Image(gl_tex_ptr, ImVec2(250,25), ImVec2(0.0, 0.0), ImVec2(1.0, 1.0));

                // Draw buttons for each available colour map.
                for(size_t i = 0; i < colour_maps.size(); ++i){
                    if( ImGui::Button(colour_maps[i].first.c_str(), ImVec2(250, 0)) ){
                        colour_map = i;
                        reload_texture = true;
                    }
                }

                ImGui::End();

                if(reload_texture){
                    std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
                    recompute_image_state();
                    recompute_scale_bar_image_state();
                }
            }
            return;
        };
        try{
            adjust_colour_map();
        }catch(const std::exception &e){
            FUNCWARN("Exception in adjust_colour_map(): '" << e.what() << "'");
            throw;
        }


        // Display plots.
        const auto display_plots = [&view_toggles,
                                    &drover_mutex,
                                    &mutex_dt,
                                    &DICOM_data,

                                    &lsamps_visible,
                                    &plot_norm,
                                    &show_plot_legend ]() -> void {

            std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_plots_enabled 
            ||  !DICOM_data.Has_LSamp_Data() ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 40), ImGuiCond_FirstUseEver);
            ImGui::Begin("Plot Selection", &view_toggles.view_plots_enabled);

            {
                ImVec2 window_extent = ImGui::GetContentRegionAvail();
                ImGui::Text("Settings");
                ImGui::Checkbox("Show metadata on hover", &view_toggles.view_plots_metadata);
                ImGui::Checkbox("Show legend", &show_plot_legend);

                ImGui::Text("Normalization: ");
                ImGui::SameLine();
                if(ImGui::Button("None", ImVec2(window_extent.x/3, 0))){ 
                    plot_norm = plot_norm_t::none;
                }
                ImGui::SameLine();
                if(ImGui::Button("Max", ImVec2(window_extent.x/3, 0))){ 
                    plot_norm = plot_norm_t::max;
                }
            }
            
            const int N_lsamps = static_cast<int>(DICOM_data.lsamp_data.size());

            {
                ImVec2 window_extent = ImGui::GetContentRegionAvail();
                ImGui::Text("Display");
                if(ImGui::Button("All##plots_display", ImVec2(window_extent.x/3, 0))){ 
                    for(auto &p : lsamps_visible) p.second = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("None##plots_display", ImVec2(window_extent.x/3, 0))){ 
                    for(auto &p : lsamps_visible) p.second = false;
                }
                ImGui::SameLine();
                if(ImGui::Button("Invert##plots_display", ImVec2(window_extent.x/3, 0))){ 
                    for(auto &p : lsamps_visible) p.second = !p.second;
                }
            }

            bool any_selected = false;
            for(int i = 0; i < N_lsamps; ++i){
                auto lsamp_ptr_it = std::next(DICOM_data.lsamp_data.begin(), i);
                const auto name = (*lsamp_ptr_it)->line.GetMetadataValueAs<std::string>("LineName").value_or("unknown"_s);
                const auto modality = (*lsamp_ptr_it)->line.GetMetadataValueAs<std::string>("Modality").value_or("unknown"_s);
                const auto histtype = (*lsamp_ptr_it)->line.GetMetadataValueAs<std::string>("HistogramType").value_or("unknown"_s);
                const auto title = std::to_string(i) + " " + name;

                ImGui::Checkbox(title.c_str(), &lsamps_visible[i]); 
                // Display metadata when hovering.
                if( ImGui::IsItemHovered() 
                &&  view_toggles.view_plots_metadata ){
                    ImGui::SetNextWindowSize(ImVec2(600, -1));
                    ImGui::BeginTooltip();
                    ImGui::Text("Linesample Metadata");
                    ImGui::Columns(2, "Plot Metadata", true);
                    ImGui::Separator();
                    ImGui::Text("Key"); ImGui::NextColumn();
                    ImGui::Text("Value"); ImGui::NextColumn();
                    ImGui::Separator();
                    for(const auto &apair : (*lsamp_ptr_it)->line.metadata){
                        ImGui::Text("%s",  apair.first.c_str()); ImGui::NextColumn();
                        ImGui::Text("%s",  apair.second.c_str()); ImGui::NextColumn();
                    }
                    ImGui::EndTooltip();
                }

                ImGui::SameLine(200);
                ImGui::Text("%s", modality.c_str());
                ImGui::SameLine(300);
                ImGui::Text("%s", histtype.c_str());

                const auto is_visible = lsamps_visible[i];
                if(is_visible) any_selected = true;
            }
            ImGui::End();

            if(any_selected){
                ImGui::SetNextWindowSize(ImVec2(620, 640), ImGuiCond_FirstUseEver);
                ImGui::Begin("Plots", &view_toggles.view_plots_enabled);
                ImVec2 window_extent = ImGui::GetContentRegionAvail();

                auto flags = ImPlotFlags_AntiAliased | ImPlotFlags_NoLegend;
                if(show_plot_legend) flags = ImPlotFlags_AntiAliased;

                if(ImPlot::BeginPlot("Plots",
                                     nullptr,
                                     nullptr,
                                     window_extent,
                                     flags,
                                     ImPlotAxisFlags_AutoFit,
                                     ImPlotAxisFlags_AutoFit )) {

                    for(int i = 0; i < N_lsamps; ++i){
                        if(!lsamps_visible[i]) continue;
                        auto lsamp_ptr_it = std::next(DICOM_data.lsamp_data.begin(), i);
                        if((*lsamp_ptr_it)->line.empty()) continue;

                        decltype((*lsamp_ptr_it)->line) shtl;
                        decltype((*lsamp_ptr_it)->line) *s_ptr;
                        if(plot_norm == plot_norm_t::none){
                            s_ptr = &((*lsamp_ptr_it)->line);
                        }else if(plot_norm == plot_norm_t::max){
                            const auto max_f = (*lsamp_ptr_it)->line.Get_Extreme_Datum_y().second[2];
                            shtl = (*lsamp_ptr_it)->line.Multiply_With( 1.0 / max_f );
                            s_ptr = &shtl;
                        }else{
                            throw std::logic_error("Unanticipated plot normalization encountered.");
                        }
                        const int offset = 0;
                        const int stride = sizeof( decltype( s_ptr->samples[0] ) );
                        const auto name = s_ptr->GetMetadataValueAs<std::string>("LineName").value_or("unknown"_s);
                        const auto title = std::to_string(i) + " " + name;

                        ImPlot::PlotLine<double>(title.c_str(),
                                                 &s_ptr->samples[0][0], 
                                                 &s_ptr->samples[0][2],
                                                 s_ptr->samples.size(),
                                                 offset, stride );
                    }
                    ImPlot::EndPlot();
                }

                ImGui::End();
            }
            return;
        };
        try{
            display_plots();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_plots(): '" << e.what() << "'");
            throw;
        }


        // Display row and column profiles.
        const auto display_row_column_profiles = [&view_toggles,

                                                  &row_profile,
                                                  &col_profile ]() -> void {
            if( view_toggles.view_row_column_profiles 
            &&  !row_profile.empty()
            &&  !col_profile.empty() ){
                ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
                ImGui::Begin("Row and Column Profiles", &view_toggles.view_row_column_profiles);
                ImVec2 window_extent = ImGui::GetContentRegionAvail();

                const int offset = 0;
                const int stride = sizeof( decltype( row_profile.samples[0] ) );

                if(ImPlot::BeginPlot("Row and Column Profiles",
                                     nullptr,
                                     nullptr,
                                     window_extent,
                                     ImPlotFlags_AntiAliased,
                                     ImPlotAxisFlags_AutoFit,
                                     ImPlotAxisFlags_AutoFit )) {
                    ImPlot::PlotLine<double>("Row Profile",
                                             &row_profile.samples[0][0], 
                                             &row_profile.samples[0][2],
                                             row_profile.size(),
                                             offset, stride );
                    ImPlot::PlotLine<double>("Column Profile",
                                             &col_profile.samples[0][0], 
                                             &col_profile.samples[0][2],
                                             col_profile.size(),
                                             offset, stride );
                    ImPlot::EndPlot();
                }

                ImGui::End();
            }
            return;
        };
        try{
            display_row_column_profiles();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_row_column_profiles(): '" << e.what() << "'");
            throw;
        }


        // Display time profile.
        const auto display_time_profiles = [&view_toggles,
                                            &time_profile,
                                            &time_course_abscissa_key,
                                            &time_course_image_inclusivity,
                                            &time_course_abscissa_relative ]() -> void {
            if( view_toggles.view_time_profiles ){
                ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
                ImGui::Begin("Time Profile", &view_toggles.view_time_profiles);

                ImGui::Text("Image selection");
                if(ImGui::Button("Current array only")){
                    time_course_image_inclusivity = time_course_image_inclusivity_t::current;
                }
                ImGui::SameLine();
                if(ImGui::Button("All arrays")){
                    time_course_image_inclusivity = time_course_image_inclusivity_t::all;
                }

                ImGui::Text("Abscissa");
                ImGui::InputText("Metadata key", time_course_abscissa_key.data(), time_course_abscissa_key.size());
                ImGui::Checkbox("Relative", &time_course_abscissa_relative); 

                if( time_profile.samples.empty() ){
                    ImGui::Text("No data available for cursor position");

                }else{
                    const auto abscissa = time_profile.metadata["Abscissa"];
                    ImVec2 window_extent = ImGui::GetContentRegionAvail();

                    if(ImPlot::BeginPlot("Time Profiles",
                                         abscissa.c_str(),
                                         nullptr,
                                         window_extent,
                                         ImPlotFlags_AntiAliased,
                                         ImPlotAxisFlags_AutoFit,
                                         ImPlotAxisFlags_AutoFit )) {
                        long int i = 0;
                        for(auto &tp : { time_profile }){
                            const int offset = 0;
                            const int stride = sizeof( decltype( tp.samples[0] ) );
                            ImPlot::PlotLine<double>(std::string("##time_profile_"_s + std::to_string(i)).c_str(),
                                                     &tp.samples[0][0], 
                                                     &tp.samples[0][2],
                                                     tp.size(),
                                                     offset, stride );

                            if(auto ca = get_as<double>(tp.metadata,"CurrentAbscissa");
                               ca && (2 < tp.samples.size()) ){
                                try{
                                    const auto s = tp.Interpolate_Linearly(ca.value());
                                    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.15f);
                                    ImPlot::PlotScatter<double>(std::string("##current_abscissa_scatter_"_s + std::to_string(i)).c_str(),
                                                                &s[0], 
                                                                &s[2],
                                                                1,
                                                                offset, stride );
                                    ImPlot::PopStyleVar();
                                    ImPlot::PlotVLines(std::string("##current_abscissa_line_"_s + std::to_string(i)).c_str(),&s[0],1);
                                }catch(const std::exception &){}
                            }
                            ++i;
                        }
                        ImPlot::EndPlot();
                    }
                }
                ImGui::End();
            }
            return;
        };
        try{
            display_time_profiles();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_time_profiles(): '" << e.what() << "'");
            throw;
        }

        // Display tables.
        const auto display_tables = [&view_toggles,
                                     &drover_mutex,
                                     &mutex_dt,
                                     &table_num,
                                     &recompute_table_iters,
                                     &display_metadata_table,
                                     &DICOM_data ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_tables_enabled
            ||  !DICOM_data.Has_Table_Data() ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 40), ImGuiCond_FirstUseEver);
            ImGui::Begin("Table Selection", &view_toggles.view_tables_enabled);

            if(ImGui::Button("Create table")){
                DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>() );
                table_num = DICOM_data.table_data.size() - 1;
            }
            ImGui::SameLine();
            if(ImGui::Button("Remove table")){
                auto [table_is_valid, table_ptr_it] = recompute_table_iters();
                if(table_is_valid){
                    DICOM_data.table_data.erase( table_ptr_it );
                    table_num -= 1;
                }
            }

            // Scroll through tables.
            if(DICOM_data.Has_Table_Data()){
                int scroll_tables = table_num;
                const int N_tables = DICOM_data.table_data.size();
                ImGui::SliderInt("Table", &scroll_tables, 0, N_tables - 1);
                const long int new_table_num = std::clamp(scroll_tables, 0, N_tables - 1);
                if(new_table_num != table_num){
                    table_num = new_table_num;
                }
            }

            // Display the table.
            if( auto [table_is_valid, table_ptr_it] = recompute_table_iters();  table_is_valid ){
                const auto [min_col, max_col] = (*table_ptr_it)->table.standard_min_max_col();
                const auto [min_row, max_row] = (*table_ptr_it)->table.standard_min_max_row();

                ImVec2 cell_padding(0.0f, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
                if(ImGui::BeginTable("Table display", (max_col - min_col) + 1,  ImGuiTableFlags_Borders
                                                                   //| ImGuiTableFlags_ScrollX
                                                                   //| ImGuiTableFlags_ScrollY
                                                                   | ImGuiTableFlags_RowBg
                                                                   | ImGuiTableFlags_BordersV
                                                                   | ImGuiTableFlags_BordersInner
                                                                   | ImGuiTableFlags_Resizable )){

                    // Number the columns.
                    for(long int c = min_col; c <= max_col; ++c){
                        ImGui::TableSetupColumn(std::to_string(c).c_str());
                    }
                    ImGui::TableHeadersRow();

                    std::array<char, 2048> buf;
                    string_to_array(buf, "");

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);

                    tables::visitor_func_t f = [&](int64_t row, int64_t col, std::string& v) -> tables::action {
                        ImGui::TableNextColumn();
                        //ImGui::SetNextItemWidth(-FLT_MIN);
                        string_to_array(buf, v);
                        // This ID ensures the table can grow and retain the same ID. It splits an int32_t into two
                        // ranges, allowing rows to span [0,100'000] and columns to span [0,max_int32_t/100'000] =
                        // [0,20'000].
                        int cell_ID = row + col * 100'000;
                        ImGui::PushID(cell_ID);
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        const bool key_changed = ImGui::InputText("##datum", buf.data(), buf.size() - 1);
                        ImGui::PopID();
 
                        //const auto im_row = ImGui::TableGetRowIndex() + min_row - 1; // +1 for the header.
                        //const auto im_col = ImGui::TableGetColumnIndex() + min_col;
                        //
                        // if( (row == 2) && (col == 3) ){
                        //     FUNCINFO("row,col = 2,3 and IMGui row,col = " << im_row << ", " << im_col);
                        // }
                        if( key_changed ) array_to_string(v, buf);

                        return tables::action::automatic; // Retain only non-empty cells.
                    };
                    (*table_ptr_it)->table.visit_standard_block(f);

                    ImGui::PopStyleColor();
                    ImGui::EndTable();
                }
                ImGui::PopID();
                ImGui::PopStyleVar();


                if( view_toggles.view_table_metadata_enabled ){
                    ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                    ImGui::Begin("Image Metadata", &view_toggles.view_table_metadata_enabled);

                    display_metadata_table( (*table_ptr_it)->table.metadata );

                    ImGui::End();
                }
            }

            ImGui::End();
            return;
        };
        try{
            display_tables();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_tables(): '" << e.what() << "'");
            throw;
        }

        // Display the image navigation dialog.
        const auto display_image_navigation = [&view_toggles,
                                               &drover_mutex,
                                               &mutex_dt,
                                               &DICOM_data,
                                               &recompute_image_iters,

                                               &img_array_num,
                                               &img_num,
                                               &img_precess,
                                               &img_precess_period,
                                               &img_precess_last,
                                               &img_channel,

                                               &zoom,
                                               &pan,
                                               &uv_min,
                                               &uv_max,
                                               &image_mouse_pos_opt,
                                               &io,

                                               &recompute_cimage_iters,
                                               &contouring_img_altered,
                                               &contouring_reach,
                                               &contouring_intensity,
                                               &last_mouse_button_0_down,
                                               &last_mouse_button_1_down,
                                               &last_mouse_button_pos,
                                               &largest_projection,
                                               &contouring_brush,

                                               &tagged_pos,

                                               &advance_to_image_array,
                                               &recompute_image_state,
                                               &need_to_reload_opengl_texture,
                                               &launch_contour_preprocessor,
                                               &reset_contouring_state,
                                               &advance_to_image,

                                               &frame_count ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if( !drover_lock
            ||  !image_mouse_pos_opt
            ||  need_to_reload_opengl_texture.load() ) return;

            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            if( !view_toggles.view_images_enabled
            ||  !img_valid ) return;

            ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 100), ImGuiCond_FirstUseEver);
            ImGui::Begin("Image Navigation", &view_toggles.view_images_enabled, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_AlwaysAutoResize);

            int scroll_arrays = img_array_num;
            int scroll_images = img_num;
            int scroll_channel = img_channel;
            {
                //ImVec2 window_extent = ImGui::GetContentRegionAvail();

                ImGui::Text("Image selection");
                const int N_arrays = DICOM_data.image_data.size();
                const int N_images = (*img_array_ptr_it)->imagecoll.images.size();
                //ImGui::SetNextItemWidth(window_extent.x);
                ImGui::SliderInt("Array", &scroll_arrays, 0, N_arrays - 1);
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Shortcut: shift + mouse wheel");
                    ImGui::EndTooltip();
                }
                //ImGui::SetNextItemWidth(window_extent.x);
                ImGui::SliderInt("Image", &scroll_images, 0, N_images - 1);
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Shortcut: mouse wheel or page-up/page-down");
                    ImGui::EndTooltip();
                }

                {
                    if(ImGui::Checkbox("Auto-advance", &img_precess)){
                        // Reset the previous time point.
                        img_precess_last = std::chrono::steady_clock::now();
                    }
                    ImGui::DragFloat("Advance period (s)", &img_precess_period, 0.01f, 0.0f, 10.0f, "%.01f");
                    if(img_precess){
                        const auto t_now = std::chrono::steady_clock::now();
                        const auto dt_since_last = 0.001f * static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(t_now - img_precess_last).count());
                        if(img_precess_period <= dt_since_last){
                            scroll_images = (scroll_images + N_images + 1) % N_images;
                            img_precess_last = t_now; // Note: should we try correct for missing time here?
                        }
                    }
                }

                ImGui::Separator();
                ImGui::Text("Magnification");
                ImGui::DragFloat("Zoom level", &zoom, 0.01f, 1.0f, 10.0f, "%.03f");
                zoom = std::clamp(zoom, 0.1f, 1000.0f);
                const float uv_width = 1.0f / zoom;
                ImGui::DragFloat("Pan horizontal", &pan.x, 0.01f, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f, "%.03f");
                ImGui::DragFloat("Pan vertical",   &pan.y, 0.01f, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f, "%.03f");
                pan.x = std::clamp(pan.x, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f);
                pan.y = std::clamp(pan.y, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f);
                uv_min.x = pan.x - uv_width * 0.5f;
                uv_min.y = pan.y - uv_width * 0.5f;
                uv_max.x = pan.x + uv_width * 0.5f;
                uv_max.y = pan.y + uv_width * 0.5f;

                if(ImGui::Button("Reset zoom")){
                    zoom = 1.0f;
                    pan.x = 0.5f;
                    pan.y = 0.5f;
                }

                ImGui::Separator();
                ImGui::Text("Display");
                ImGui::SliderInt("Channel", &scroll_channel, 0, static_cast<int>(disp_img_it->channels - 1));

                if(ImGui::IsWindowFocused() || image_mouse_pos_opt.value().image_window_focused){
                    auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();

                    const int d_l = static_cast<int>( std::floor(io.MouseWheel) );
                    const int d_h = static_cast<int>( std::ceil(io.MouseWheel) );
                    if(false){
                    }else if(io.KeyCtrl && (0 < io.MouseWheel)){
                        zoom += std::log(zoom + 0.25f);
                        zoom = std::clamp( zoom, 1.0f, 10.0f );
                    }else if(io.KeyCtrl && (io.MouseWheel < 0)){
                        zoom -= std::log(zoom + 0.25f);
                        zoom = std::clamp( zoom, 1.0f, 10.0f );

                    }else if( (2 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (0.0f <= io.MouseDownDuration[2]) ){
                        pan.x -= static_cast<float>( io.MouseDelta.x ) / 600.0f;
                        pan.y -= static_cast<float>( io.MouseDelta.y ) / 600.0f;
                          
                    }else if(io.KeyShift && (0 < io.MouseWheel)){
                        scroll_arrays = std::clamp((scroll_arrays + N_arrays + d_h) % N_arrays, 0, N_arrays - 1);
                    }else if(io.KeyShift && (io.MouseWheel < 0)){
                        scroll_arrays = std::clamp((scroll_arrays + N_arrays + d_l) % N_arrays, 0, N_arrays - 1);

                    }else if( (   (view_toggles.view_contouring_enabled && cimg_valid)
                               || (view_toggles.view_drawing_enabled && img_valid) )
                          &&  (0 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (1 < IM_ARRAYSIZE(io.MouseDown))
                          &&  ((0.0f <= io.MouseDownDuration[0]) || (0.0f <= io.MouseDownDuration[1]))
                          &&  image_mouse_pos_opt.value().mouse_hovering_image ){
                        contouring_img_altered = true;
                        need_to_reload_opengl_texture.store(true);

                        decltype(disp_img_it) l_img_it = (view_toggles.view_contouring_enabled) ? cimg_it : disp_img_it;
                        decltype(img_array_ptr_it) l_img_array_ptr_it = (view_toggles.view_contouring_enabled) ? cimg_array_ptr_it : img_array_ptr_it;

                        // The mapping between contouring image and display image (which uses physical dimensions) is
                        // based on the relative position along row and column axes.
                        const float radius = contouring_reach; // in DICOM units (mm).
                        const auto mouse_button_0 = (0.0f <= io.MouseDownDuration[0]);
                        const auto mouse_button_1 = (0.0f <= io.MouseDownDuration[1]);

                        const auto mouse_button_0_sticky = mouse_button_0
                            && ( io.KeyShift || (last_mouse_button_0_down < io.MouseDownDuration[0]) );
                        const auto mouse_button_1_sticky = mouse_button_1
                            && ( io.KeyShift || (last_mouse_button_1_down < io.MouseDownDuration[1]) );
                        const auto any_mouse_button_sticky = mouse_button_0_sticky || mouse_button_1_sticky;

                        std::vector<line_segment<double>> lss;
                        if( false ){
                        }else if( any_mouse_button_sticky
                              && last_mouse_button_pos
                              && io.KeyCtrl){
                            const auto pA = image_mouse_pos_opt.value().dicom_pos; // Current position.
                            const auto pB = last_mouse_button_pos.value(); // Previous position.
                            // Project along image axes to create a taxi-cab metric corner vertex.
                            const auto corner = largest_projection(pA, pB, {l_img_it->row_unit, l_img_it->col_unit});
                            lss.emplace_back(pA,corner);
                            lss.emplace_back(corner,pB);

                        }else if( any_mouse_button_sticky
                              &&  last_mouse_button_pos ){
                            const auto pA = image_mouse_pos_opt.value().dicom_pos; // Current position.
                            const auto pB = last_mouse_button_pos.value(); // Previous position.
                            lss.emplace_back(pA,pB);

                        }else{
                            // Slightly offset the line segment endpoints to avoid degeneracy.
                            auto pA = image_mouse_pos_opt.value().dicom_pos; // Current position.
                            lss.emplace_back(pA,pA);
                        }

                        decltype(planar_image_collection<float,double>().get_all_images()) cimg_its;
                        if( (contouring_brush == brush_t::rigid_circle)
                        ||  (contouring_brush == brush_t::rigid_square)
                        ||  (contouring_brush == brush_t::gaussian_2D) 
                        ||  (contouring_brush == brush_t::tanh_2D) 
                        ||  (contouring_brush == brush_t::median_circle)
                        ||  (contouring_brush == brush_t::median_square)
                        ||  (contouring_brush == brush_t::mean_circle)
                        ||  (contouring_brush == brush_t::mean_square) ){
                            // TODO: if shift or ctrl are being pressed, include all images that intersect the line
                            // segment path. This will aply the 2D brush in 3D to all images along the path, which would
                            // be useful!
                            cimg_its.emplace_back( l_img_it );

                        }else if( (contouring_brush == brush_t::rigid_sphere)
                              ||  (contouring_brush == brush_t::mean_sphere)
                              ||  (contouring_brush == brush_t::median_sphere)
                              ||  (contouring_brush == brush_t::gaussian_3D) 
                              ||  (contouring_brush == brush_t::tanh_3D) 
                              ||  (contouring_brush == brush_t::rigid_cube)
                              ||  (contouring_brush == brush_t::mean_cube)
                              ||  (contouring_brush == brush_t::median_cube) ){
                            cimg_its = (*l_img_array_ptr_it)->imagecoll.get_all_images();
                        }
                        const float inf = std::numeric_limits<float>::infinity();
                        const float intensity = contouring_intensity * ((mouse_button_0) ? 1.0f : -1.0f);
                        const float intensity_min = (view_toggles.view_contouring_enabled) ?  0.0f : -inf;
                        const float intensity_max = (view_toggles.view_contouring_enabled) ?  1.0f :  inf;
                        const long int channel = 0;
                        draw_with_brush( cimg_its, lss, contouring_brush, radius, intensity, channel, intensity_min, intensity_max );

                        // Update mouse position for next time, if applicable.
                        if( mouse_button_0 ){
                            last_mouse_button_0_down = io.MouseDownDuration[0];
                            last_mouse_button_pos = image_mouse_pos_opt.value().dicom_pos;
                        }
                        if( mouse_button_1 ){
                            last_mouse_button_1_down = io.MouseDownDuration[1];
                            last_mouse_button_pos = image_mouse_pos_opt.value().dicom_pos;
                        }

                    // Left-button mouse click on an image.
                    }else if( image_mouse_pos_opt.value().mouse_hovering_image
                          &&  (0 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (0.0f == io.MouseDownDuration[0]) ){ // Debounced!

                          if(view_toggles.view_time_profiles){
                              view_toggles.save_time_profiles = true;

                          }else{
                              if(!tagged_pos){
                                  tagged_pos = image_mouse_pos_opt.value().dicom_pos;
                              }else{
                                  tagged_pos = {};
                              }
                          }

                    }else if(0 < io.MouseWheel){
                        scroll_images = std::clamp((scroll_images + N_images + d_h) % N_images, 0, N_images - 1);
                    }else if(io.MouseWheel < 0){
                        scroll_images = std::clamp((scroll_images + N_images + d_l) % N_images, 0, N_images - 1);

                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_PageUp) ) ){
                        scroll_images = std::clamp((scroll_images + 50 * N_images + 10) % N_images, 0, N_images - 1);
                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_PageDown) ) ){
                        scroll_images = std::clamp((scroll_images + 50 * N_images - 10) % N_images, 0, N_images - 1);

                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Home) ) ){
                        scroll_images = N_images - 1;
                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_End) ) ){
                        scroll_images = 0;
                    }
                    //ImGui::Text("%.2f secs", io.KeysDownDuration[ (int)(ImGui::GetKeyIndex(ImGuiKey_PageUp)) ]);
                }
            }

            long int new_img_array_num = scroll_arrays;
            long int new_img_num = scroll_images;
            long int new_img_chnl = scroll_channel;

            // Scroll through images.
            if( new_img_array_num != img_array_num ){
                advance_to_image_array(new_img_array_num);
                recompute_image_state();
                auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                if( !img_valid ) throw std::runtime_error("Advanced to inaccessible image array");
                if(view_toggles.view_contours_enabled) launch_contour_preprocessor();
                reset_contouring_state(img_array_ptr_it);
                tagged_pos = {};

            }else if( new_img_num != img_num ){
                advance_to_image(new_img_num);
                recompute_image_state();
                auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                if( !img_valid ) throw std::runtime_error("Advanced to inaccessible image");
                if(view_toggles.view_contours_enabled) launch_contour_preprocessor();
                contouring_img_altered = true;

            }else if( (new_img_chnl != img_channel)
                  &&  (0 < disp_img_it->channels) ){
                img_channel = std::clamp<long int>(new_img_chnl, 0L, disp_img_it->channels - 1L);
                recompute_image_state();
                auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                if( !img_valid ) throw std::runtime_error("Advanced to inaccessible image channel");
            }

            ImGui::End();
            return;
        };
        try{
            display_image_navigation();
        }catch(const std::exception &e){
            FUNCWARN("Exception in display_image_navigation(): '" << e.what() << "'");
            throw;
        }


        // Saving time courses as line samples.
        if(view_toggles.save_time_profiles){
            view_toggles.save_time_profiles = false;
            string_to_array(time_course_text_entry, "unspecified");
            ImGui::OpenPopup("Save Time Profile");
        }
        if(ImGui::BeginPopupModal("Save Time Profile", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
            do{
                if(ImGui::InputText("Name", time_course_text_entry.data(), time_course_text_entry.size() - 1)){
                    std::string text;
                    array_to_string(text, time_course_text_entry);
                    time_profile.metadata["LineName"] = text;
                }

                ImGui::Separator();
                if(ImGui::Button("Save")){
                    std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
                    if(!drover_lock) break;

                    // Save a copy of the current time profile.
                    DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
                    DICOM_data.lsamp_data.back()->line = time_profile;

                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if(ImGui::Button("Cancel")){
                    ImGui::CloseCurrentPopup();
                }
            }while(false);
            ImGui::EndPopup();
        }


        // Render surface meshes.
        const auto draw_surface_meshes = [&view_toggles,
                                          &drover_mutex,
                                          &mutex_dt,
                                          &DICOM_data,
                                          &oglm_ptr,
                                          &mesh_num,
                                          &mesh_display_transform,
                                          &display_metadata_table,
                                          &recompute_smesh_iters,
                                          &need_to_reload_opengl_mesh,
                                          &custom_shader,
                                          &frame_count ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;
            if( !view_toggles.view_meshes_enabled
            ||  !DICOM_data.Has_Mesh_Data() ) return;

            const auto N_meshes = static_cast<long int>(DICOM_data.smesh_data.size());
            const auto new_mesh_num = std::clamp(mesh_num, 0L, N_meshes - 1L);
            if(new_mesh_num != mesh_num){
                mesh_num = new_mesh_num;
                need_to_reload_opengl_mesh = true;
            }

            const auto reload_opengl_mesh = [&](){
                auto [mesh_is_valid, smesh_ptr_it] = recompute_smesh_iters();
                if(!mesh_is_valid) return;
                oglm_ptr = std::make_unique<opengl_mesh>( (*smesh_ptr_it)->meshes, mesh_display_transform.reverse_normals );
                need_to_reload_opengl_mesh = false;
            };
            if(need_to_reload_opengl_mesh){
                reload_opengl_mesh();
            }

            if(!oglm_ptr){
                mesh_num = 0;
                reload_opengl_mesh();
            }

            if(oglm_ptr){
                // Draw the currently-loaded mesh.
                oglm_ptr->draw( mesh_display_transform.render_wireframe );

                //ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_FirstUseEver);
                if(ImGui::Begin("Meshes", &view_toggles.view_meshes_enabled)){

                    // Alter the common model transformation.
                    if(ImGui::IsWindowFocused()){
                        if( ImGui::IsKeyDown( SDL_SCANCODE_RIGHT ) ){
                            mesh_display_transform.model.coeff(0,3) += 0.001;
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_LEFT ) ){
                            mesh_display_transform.model.coeff(0,3) -= 0.001;
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_UP ) ){
                            mesh_display_transform.model.coeff(1,3) += 0.001;
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_DOWN ) ){
                            mesh_display_transform.model.coeff(1,3) -= 0.001;
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_W ) ){
                            mesh_display_transform.model.coeff(2,3) += 0.001;
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_S ) ){
                            mesh_display_transform.model.coeff(2,3) -= 0.001;
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_Q ) ){
                            auto rot = affine_rotate<float>( vec3<float>(0,0,0), vec3<float>(0,0,1), 3.14/100.0 );
                            mesh_display_transform.model = mesh_display_transform.model * static_cast<num_array<float>>(rot);
                        }
                        if( ImGui::IsKeyDown( SDL_SCANCODE_E ) ){
                            auto rot = affine_rotate<float>( vec3<float>(0,0,0), vec3<float>(0,0,1), -3.14/100.0 );
                            mesh_display_transform.model = mesh_display_transform.model * static_cast<num_array<float>>(rot);
                        }
                    }

                    std::string msg = "Drawing "_s
                                    + std::to_string(oglm_ptr->N_vertices) + " vertices, "
                                    + std::to_string(oglm_ptr->N_indices) + " indices, and "
                                    + std::to_string(oglm_ptr->N_triangles) + " triangles.";
                    ImGui::Text("%s", msg.c_str());

                    auto scroll_meshes = static_cast<int>(mesh_num);
                    ImGui::SliderInt("Mesh", &scroll_meshes, 0, N_meshes - 1);
                    if(static_cast<long int>(scroll_meshes) != mesh_num){
                        mesh_num = std::clamp(static_cast<long int>(scroll_meshes), 0L, N_meshes - 1L);
                        reload_opengl_mesh();
                    }

                    ImGui::ColorEdit4("Colour", mesh_display_transform.colours.data());

                    ImGui::Checkbox("Metadata", &view_toggles.view_mesh_metadata_enabled);
                    ImGui::Checkbox("Precess", &mesh_display_transform.precess);
                    ImGui::Checkbox("Wireframe", &mesh_display_transform.render_wireframe);
                    ImGui::Checkbox("Cull back faces", &mesh_display_transform.use_opaque);
                    if(ImGui::Checkbox("Reverse normals", &mesh_display_transform.reverse_normals)){
                        reload_opengl_mesh();
                    }
                    ImGui::Checkbox("Use lighting", &mesh_display_transform.use_lighting);
                    ImGui::Checkbox("Use smoothing", &mesh_display_transform.use_smoothing);
                    float drag_speed = 0.05f;
                    double clamp_l = -10.0;
                    double clamp_h =  10.0;
                    ImGui::DragScalar("Precession rate", ImGuiDataType_Double, &mesh_display_transform.precess_rate, drag_speed, &clamp_l, &clamp_h, "%.1f");
                    drag_speed = 0.3f;
                    clamp_l = -360.0 * 10.0;
                    clamp_h =  360.0 * 10.0;
                    ImGui::DragScalar("A rotation", ImGuiDataType_Double, &mesh_display_transform.rot_x, drag_speed, &clamp_l, &clamp_h, "%.1f");
                    ImGui::DragScalar("B rotation", ImGuiDataType_Double, &mesh_display_transform.rot_y, drag_speed, &clamp_l, &clamp_h, "%.1f");

                    drag_speed = 0.005f;
                    clamp_l = -10.0;
                    clamp_h = 10.0;
                    ImGui::DragScalar("Zoom", ImGuiDataType_Double, &mesh_display_transform.zoom, drag_speed, &clamp_l, &clamp_h, "%.1f");
                    ImGui::DragScalar("Camera distort", ImGuiDataType_Double, &mesh_display_transform.cam_distort, drag_speed, &clamp_l, &clamp_h, "%.1f");
                    if(ImGui::Button("Reset")){
                        mesh_display_transform = mesh_display_transform_t();
                    }

                    // Mesh metadata window.
                    if( view_toggles.view_mesh_metadata_enabled ){
                        auto [mesh_is_valid, smesh_ptr_it] = recompute_smesh_iters();
                        if(mesh_is_valid){
                            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                            ImGui::Begin("Mesh Metadata", &view_toggles.view_mesh_metadata_enabled);

                            display_metadata_table( (*smesh_ptr_it)->meshes.metadata );

                            ImGui::End();
                        }
                    }
                }
                ImGui::End();
            }

            // Release the GPU memory when mesh viewing is disabled. Otherwise, it will just needlessly consume
            // resources.
            if(!view_toggles.view_meshes_enabled){
                mesh_num = -1;
                oglm_ptr = nullptr;
            }
            return;
        };

        // Handle direct OpenGL rendering.
        {
            CHECK_FOR_GL_ERRORS();
            glClearColor(background_colour.x,
                         background_colour.y,
                         background_colour.z,
                         background_colour.w);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            CHECK_FOR_GL_ERRORS();

            if(mesh_display_transform.precess){
                mesh_display_transform.rot_x += 0.0028 * mesh_display_transform.precess_rate;
                mesh_display_transform.rot_y -= 0.0104 * mesh_display_transform.precess_rate;
            }
            mesh_display_transform.rot_x = std::fmod(mesh_display_transform.rot_x, 360.0);
            mesh_display_transform.rot_y = std::fmod(mesh_display_transform.rot_y, 360.0);

            // Locate uniform locations in the custom shader program.
            if(!custom_shader) throw std::logic_error("No available shader, cannot continue");
            const auto custom_gl_program = custom_shader->get_program_ID();
            auto shader_user_colour_loc = glGetUniformLocation(custom_gl_program, "user_colour");
            auto shader_diffuse_colour_loc = glGetUniformLocation(custom_gl_program, "diffuse_colour");
            auto mvp_loc = glGetUniformLocation(custom_gl_program, "mvp_matrix");
            auto mv_loc = glGetUniformLocation(custom_gl_program, "mv_matrix");
            auto norm_loc = glGetUniformLocation(custom_gl_program, "norm_matrix");
            auto use_lighting_loc = glGetUniformLocation(custom_gl_program, "use_lighting");
            auto use_smoothing_loc = glGetUniformLocation(custom_gl_program, "use_smoothing");

            // Activate the custom shader program.
            // Note: this must be done after locating uniforms but before uploading them.
            glUseProgram(custom_gl_program);

            // Account for viewport aspect ratio to make the render square.
            const auto w = static_cast<int>(io.DisplaySize.x);
            const auto h = static_cast<int>(io.DisplaySize.y);
            const auto l_w = std::min(w, h);
            const auto l_h = std::min(h, w);
            //glViewport((w - l_w)/2, (h - l_h)/2, l_w, l_h);
            glViewport(0, 0, w, h);
            CHECK_FOR_GL_ERRORS();

            // Set various matrices that describe the coordinate system transformations.
            const auto wpos  = ImGui::GetMainViewport()->WorkPos;
            const auto wsize = ImGui::GetMainViewport()->WorkSize;
            const auto waspect = wsize.x / wsize.y;

            // Override to normalized and aspect-corrected screen space coords.
            auto l_bound = -waspect/mesh_display_transform.zoom;
            auto r_bound =  waspect/mesh_display_transform.zoom;
            auto b_bound = -1.0/mesh_display_transform.zoom;
            auto t_bound =  1.0/mesh_display_transform.zoom;
            auto n_bound = -1000.0f/mesh_display_transform.zoom;
            auto f_bound =  1000.0f/mesh_display_transform.zoom;

            // Orthographic projection.
            auto make_orthographic_projection_matrix = []( float left_bound   = -1.0f,
                                                           float right_bound  =  1.0f,
                                                           float bottom_bound = -1.0f,
                                                           float top_bound    =  1.0f,
                                                           float near_bound   = -1.0f,
                                                           float far_bound    =  1.0f ){
                num_array<float> proj(4,4,0.0f);
                proj.coeff(0,0) = 2.0f/(right_bound - left_bound);
                proj.coeff(1,1) = 2.0f/(top_bound - bottom_bound);
                proj.coeff(2,2) = 2.0f/(near_bound - far_bound);
                proj.coeff(0,3) = -(right_bound + left_bound) / (right_bound - left_bound);
                proj.coeff(1,3) = -(top_bound + bottom_bound) / (top_bound - bottom_bound);
                proj.coeff(2,3) = -(far_bound + near_bound) / (far_bound - near_bound);
                proj.coeff(3,3) = 1.0f;
                proj = proj.transpose();
                return proj;
            };
            auto proj = make_orthographic_projection_matrix(l_bound, r_bound, b_bound, t_bound, n_bound, f_bound);

            // Model matrix.
            num_array<float> model = mesh_display_transform.model;

            // Camera matrix.
            auto make_camera_matrix = [](const vec3<double> &cam_pos,
                                         const vec3<double> &target_pos,
                                         const vec3<double> &up_unit){

                num_array<float> out(4, 4, 0.0f);

                // Extract the camera-facing coordinate system via a Gram-Schmidt-like process.
                const auto inward   = (cam_pos - target_pos).unit(); // From target point of view.
                const auto leftward = up_unit.Cross(inward).unit();
                const auto upward   = inward.Cross(leftward).unit();

                if( inward.isfinite()
                &&  leftward.isfinite()
                &&  upward.isfinite() ){
                /*
                    // Rotational component (inverted = transposed).
                    out.coeff(0,0) = leftward.x;
                    out.coeff(0,1) = leftward.y;
                    out.coeff(0,2) = leftward.z;
                                 
                    out.coeff(1,0) = upward.x;
                    out.coeff(1,1) = upward.y;
                    out.coeff(1,2) = upward.z;
                                 
                    out.coeff(2,0) = inward.x;
                    out.coeff(2,1) = inward.y;
                    out.coeff(2,2) = inward.z;

                    // Translational component (inverted = negated).
                    out.coeff(0,3) = - cam_pos.Dot(leftward);
                    out.coeff(1,3) = - cam_pos.Dot(upward);
                    out.coeff(2,3) = - cam_pos.Dot(inward);
                */
                    // Rotational component.
                    out.coeff(0,0) = leftward.x;
                    out.coeff(1,0) = leftward.y;
                    out.coeff(2,0) = leftward.z;
                                 
                    out.coeff(0,1) = upward.x;
                    out.coeff(1,1) = upward.y;
                    out.coeff(2,1) = upward.z;
                                 
                    out.coeff(0,2) = inward.x;
                    out.coeff(1,2) = inward.y;
                    out.coeff(2,2) = inward.z;

                    // Translational component.
                    out.coeff(0,3) = cam_pos.Dot(leftward);
                    out.coeff(1,3) = cam_pos.Dot(upward);
                    out.coeff(2,3) = cam_pos.Dot(inward);

                    // Projection component.
                    out.coeff(3,3) = 1.0f;
                    out = out.transpose();

                }else{
                    out = num_array<float>().identity(4);
                }
                return out;
            };

            auto extract_normal_matrix = [](const num_array<float>& mvp){
                // Extract only the rotational component of the MVP matrix. This can be used to transform mesh normals.
                if( (mvp.num_rows() != 4) || (mvp.num_cols() != 4) ){
                    throw std::logic_error("Expected 4x4 matrix");
                }
                num_array<float> out(3, 3, 0.0f);
                for(long int r = 0; r < 3; ++r){
                    for(long int c = 0; c < 3; ++c){
                        out.coeff(r,c) = mvp.read_coeff(r,c);
                    }
                }
                return out;
            };

            // Rotate camera according as per user's settings / precession.
            const auto pi = std::acos(-1.0);
            const auto x_rot = mesh_display_transform.rot_x * (2.0 * pi) / 360.0;
            const auto y_rot = mesh_display_transform.rot_y * (2.0 * pi) / 360.0;
            vec3<double> cam_pos = vec3<double>(0,0,1).rotate_around_y(y_rot)
                                                      .rotate_around_x(x_rot)
                                                      .unit() * std::exp(mesh_display_transform.cam_distort - 5.0);
            vec3<double> target_pos = vec3<double>(0.0, 0.0, 0.0);
            vec3<double> up_unit = vec3<double>(0.0, 1.0, 0.0).unit();
            num_array<float> camera = make_camera_matrix( cam_pos, target_pos, up_unit );

            // Final coordinate system transforms.
            const auto mv = camera * model;
            const auto mvp = proj * mv;
            const auto norm = extract_normal_matrix(mvp);

            // Pass uniforms to custom shader program iff they are needed.
            const std::vector<float> mv_data( mv.cbegin(), mv.cend() );
            if(0 <= mv_loc) glUniformMatrix4fv(mv_loc, 1, GL_FALSE, mv_data.data());

            const std::vector<float> mvp_data( mvp.cbegin(), mvp.cend() );
            if(0 <= mvp_loc) glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp_data.data());

            const std::vector<float> norm_data( norm.cbegin(), norm.cend() );
            if(0 <= norm_loc) glUniformMatrix3fv(norm_loc, 1, GL_FALSE, norm_data.data());

            if(0 <= use_lighting_loc){
                glUniform1ui(use_lighting_loc, (mesh_display_transform.use_lighting ? GL_TRUE : GL_FALSE));
            }
            if(0 <= use_smoothing_loc){
                glUniform1ui(use_smoothing_loc, (mesh_display_transform.use_smoothing ? GL_TRUE : GL_FALSE));
            }
            if(0 < shader_user_colour_loc){
                glUniform4f(shader_user_colour_loc, mesh_display_transform.colours[0],
                                                    mesh_display_transform.colours[1],
                                                    mesh_display_transform.colours[2],
                                                    mesh_display_transform.colours[3] );
            }
            if(0 <= shader_diffuse_colour_loc){
                glUniform4f(shader_diffuse_colour_loc, mesh_display_transform.colours[0],
                                                       mesh_display_transform.colours[1],
                                                       mesh_display_transform.colours[2],
                                                       mesh_display_transform.colours[3] );
            }
            CHECK_FOR_GL_ERRORS();

            // Set how overlapping vertices are rendered.
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);

            if(mesh_display_transform.use_opaque){
                glDisable(GL_BLEND);

                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);

            }else{
                glEnable(GL_BLEND);
                // Order-dependent rendering:
                //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                // Order-independent rendering.
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);

                glDisable(GL_CULL_FACE);
            }

            CHECK_FOR_GL_ERRORS();

            draw_surface_meshes();

            CHECK_FOR_GL_ERRORS();
        }

        // Show a pop-up with information about DICOMautomaton.
        if(view_toggles.set_about_popup){
            view_toggles.set_about_popup = false;
            ImGui::OpenPopup("AboutPopup");
        }
        if(ImGui::BeginPopupModal("AboutPopup")){
            const std::string version = "DICOMautomaton SDL_Viewer version "_s + DCMA_VERSION_STR;
            ImGui::Text("%s", version.c_str());
            ImGui::Separator();

            if(ImGui::Button("View contouring debug window")){
                view_toggles.view_contouring_debug = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Imgui demo")){
                view_toggles.view_imgui_demo = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Implot demo")){
                view_toggles.view_implot_demo = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Close")){
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }


        // Render the ImGui components and swap OpenGL buffers.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }
    terminate_contour_preprocessors();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Hope that this is enough time for preprocessing threads to terminate.
                                                                 // TODO: use a work queue with condition variable to
                                                                 // signal termination!

    oglm_ptr = nullptr;  // Release OpenGL resources while context is valid.
    custom_shader = nullptr;
    Free_OpenGL_Texture(current_texture);
    Free_OpenGL_Texture(contouring_texture);
    Free_OpenGL_Texture(scale_bar_texture);

    // OpenGL and SDL cleanup.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return true;
}
