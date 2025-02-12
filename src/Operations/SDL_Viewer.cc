//SDL_Viewer.cc - A part of DICOMautomaton 2020-2025. Written by hal clark.

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
#include <random>

#include <filesystem>

#include "../imgui20210904/imgui.h"
#include "../imgui20210904/imgui_internal.h"
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
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorBase64.h"

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
#include "../Standard_Guides.h"
#include "../Thread_Pool.h"
#include "../String_Parsing.h"
#include "../Dialogs.h"
#include "../Alignment_Rigid.h"
#include "../Documentation.h"
#include "../Dialogs/Tray_Notification.h"
#include "../Triple_Three.h"
#include "../Rotating_Cube.h"
#include "../STB_Shim.h"
#include "../Surface_Meshes.h"

#include "SDL_Viewer.h"

//extern const std::string DCMA_VERSION_STR;

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


// Draw a loading animation using ImGui primitives.
//
// Looks like a wave propagating through a line of squares.
static
void
CustomImGuiWidget_LoadingBar(const std::chrono::time_point<std::chrono::system_clock> &t_start){

    const std::chrono::time_point<std::chrono::system_clock> t_now = std::chrono::system_clock::now();
    const auto t = std::chrono::duration<float>(t_now - t_start).count();

    auto drawList = ImGui::GetWindowDrawList();
    const auto orig_screen_pos = ImGui::GetCursorScreenPos();
    const auto avail_space = ImGui::GetContentRegionAvail();
    const auto rect_width = std::clamp<float>(ImGui::GetFontSize(), 1.0f, 100.0f);
    const auto rect_height = std::clamp<float>(ImGui::GetTextLineHeight(), 1.0f, 100.0f);
    const auto rect_height_offset = (ImGui::GetTextLineHeightWithSpacing() / rect_height) * 0.5f;
    const auto rect_width_offset = rect_height_offset;
    const auto rect_space = rect_width * 0.25f;
    const auto num_rects_f = std::clamp<float>( (avail_space.x - ImGui::GetCursorPosX() * 2.0f + rect_space) / (rect_width + rect_space), 3.0f, 50.0f );
    const auto wave_speed = 125.0f;

    if( std::isfinite(rect_width)
    &&  std::isfinite(rect_height)
    &&  std::isfinite(rect_height_offset)
    &&  std::isfinite(rect_width_offset)
    &&  std::isfinite(rect_space)
    &&  std::isfinite(num_rects_f)  ){
        const int64_t num_rects = static_cast<int64_t>(std::floor(num_rects_f));
        const auto wave_period = rect_width + rect_space * 20.0f * 5.0f; // 'tuned' for 20 rectangles.
        const auto pi = std::acos(-1.0);

        for(int64_t i = 1; i <= num_rects; i += 1){
            const auto x_offset = (rect_width * i) + (rect_space * (i - 1));

            ImVec2 tl_pos;
            tl_pos.x = orig_screen_pos.x + rect_width_offset + x_offset;
            tl_pos.y = orig_screen_pos.y + rect_height_offset;

            ImVec2 br_pos;
            br_pos.x = tl_pos.x + rect_width;
            br_pos.y = tl_pos.y + rect_height;

            const auto intensity = std::cos(2.0f*pi*(wave_speed * t - x_offset)/wave_period);
            const auto clamped = std::clamp<double>(intensity, 0.2, 1.0);
            ImU32 col = ImGui::GetColorU32( ImVec4(clamped, clamped * 0.5f, clamped * 0.1f, 1.0f ) );

            drawList->AddRectFilled( tl_pos, br_pos, col );
        }
        ImVec2 placeholder_extent;
        placeholder_extent.x = avail_space.x;
        placeholder_extent.y = rect_height_offset * 2.0f + rect_height;
        ImGui::Dummy(placeholder_extent);
    }
    return;
}

// Compute an axis-aligned bounding box in pixel coordinates.
std::tuple<int64_t, int64_t, int64_t, int64_t>
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

    auto col_min = std::clamp<int64_t>(static_cast<int64_t>(std::floor(bbox_min.x/img.pxl_dx)), 0, img.columns-1);
    auto col_max = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(bbox_max.x/img.pxl_dx)), 0, img.columns-1);
    auto row_min = std::clamp<int64_t>(static_cast<int64_t>(std::floor(bbox_min.y/img.pxl_dy)), 0, img.rows-1);
    auto row_max = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(bbox_max.y/img.pxl_dy)), 0, img.rows-1);
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
        return l_custom_shader;
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
                      int64_t channel,
                      float intensity_min = std::numeric_limits<float>::infinity() * -1.0,
                      float intensity_max = std::numeric_limits<float>::infinity(),
                      bool is_additive = true){

    YLOGINFO("Implementing brush stroke");

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
        buffer_space = radius * 2.25;

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
            for(int64_t r = row_min; r <= row_max; ++r){
                for(int64_t c = col_min; c <= col_max; ++c){
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

                        //if(buffer_space < dR) continue;
                    }

                    cit->reference( r, c, channel ) = std::clamp<float>(f(pos, dR, cit->value(r, c, channel)), intensity_min, intensity_max);
                }
            }
        }
    };


    // Implement brushes.
    if( (brush == brush_t::rigid_circle)
    ||  (brush == brush_t::rigid_square) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [intensity,is_additive](const vec3<double> &, double, float) -> float {
                const float l_intensity = (is_additive) ? intensity : 0.0f;
                return l_intensity;
            });
        }

    }else if( (brush == brush_t::gaussian_2D)
          ||  (brush == brush_t::gaussian_3D) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [radius,intensity,is_additive](const vec3<double> &, double dR, float v) -> float {
                // Approach the desired intensity at a rate dependent on the location; proportional to a spatial
                // Gaussian.
                const float l_intensity = (is_additive) ? intensity : 0.0f;
                const float scale = 0.65f;
                const auto l_exp = std::exp( -std::pow(dR / (scale * radius), 2.0f) );
                return (l_intensity - v) * l_exp + v;
            });
        }

    }else if( (brush == brush_t::tanh_2D)
          ||  (brush == brush_t::tanh_3D) ){
        for(const auto &img_it : img_its){
            apply_to_inner_pixels({img_it}, [radius,intensity,is_additive](const vec3<double> &, double dR, float v) -> float {
                const float l_intensity = (is_additive) ? intensity : 0.0f;
                const float old_v = v;
                const float steepness = 1.5f;  // How steep the perimeter of the brush is. Also impacts contour detail.
                const float paint_flow_rate = 1.0f; // "Strength" of the brush stroke.

                // Find proposed brush intensity.
                auto l_tanh = 0.5 * (1.0 + std::tanh( steepness * (radius - dR)));
                l_tanh = is_additive ? l_tanh : (1.0f - l_tanh); // Flip distribution vertically if subtracting.
                l_tanh *= intensity; // Scale distribution to target intensity @ maximum.

                // Alter brush behaviour based on whether the current voxel's intensity is above or below the target,
                // whether in additive or subtractive mode, and whether the voxel is within the brush boundary.
                //
                // This system has weird behaviour for negative intensities and when in drawing mode and painting
                // multiple intensities. But it otherwise works intuitively and provides accurate contours (e.g.,
                // the contours produced have the correct dimensions). It is also economical, requiring lower mask
                // resolution to accomplish the same contour smoothness.
                const bool is_mode_aligned = (is_additive == (l_tanh >= old_v));
                const bool is_inside_brush = (dR <= radius);
                float new_v = old_v;
                if(is_mode_aligned){
                    // Free to increase or decrease in intensity. The boundary should stay reasonably accurate.
                    new_v = l_tanh;

                }else if(!is_mode_aligned && is_inside_brush){
                    // Pull the intensity to the target intensity somewhat quickly, i.e., the maximum intensity
                    // the brush can make. This allows the brush to honour the proposed intensity, but won't leave
                    // noticeable edges when performing a brush stroke.

                    //const bool is_beyond = (is_additive == (l_intensity < old_v));
                    new_v = (l_intensity - old_v) * 0.5f + old_v;

                }else if(!is_mode_aligned && !is_inside_brush){
                    // Do nothing.

                    // Note: pulling the intensity to the desired tanh shape *outside* the brush when not mode
                    // aligned produces counter-intuitive results. Performing a brush stroke results in a jagged and
                    // rough line, and sweeping results in a shape like an exclamation mark. It will also produce a
                    // 'moat' around the current brush location if held long enough.
                }

                // Perform final blend using brush stroke strength.
                return (new_v - old_v) * paint_flow_rate + old_v;
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
        apply_to_inner_pixels(img_its, [intensity,is_additive](const vec3<double> &, double, float) -> float {
            const float l_intensity = (is_additive) ? intensity : 0.0f;
            return l_intensity;
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

    out.args.emplace_back();
    out.args.back().name = "LexiconCustomizer";
    out.args.back().desc = "Controls whether the lexicon customizer interface is opened by default.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Contouring";
    out.args.back().desc = "Controls whether the contouring interface is opened by default.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };
    out.args.back().samples = OpArgSamples::Exhaustive;

    out.args.emplace_back();
    out.args.back().name = "Guide";
    out.args.back().desc = "A guide to display to the user. Usually used to walk the user through one or more actions.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "Step 1---Step 2---Step 3" };

    return out;
}

bool SDL_Viewer(Drover &DICOM_data,
                const OperationArgPkg& OptArgs,
                std::map<std::string, std::string>& InvocationMetadata,
                const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto DefaultLexiconCustomizerStr = OptArgs.getValueStr("LexiconCustomizer").value();
    const auto DefaultContouringStr = OptArgs.getValueStr("Contouring").value();
    const auto GuideOpt = OptArgs.getValueStr("Guide");

    //-----------------------------------------------------------------------------------------------------------------
    const auto TrueRegex = Compile_Regex("^tr?u?e?$");

    const auto DefaultLexiconCustomizer = std::regex_match(DefaultLexiconCustomizerStr, TrueRegex);
    const auto DefaultContouring = std::regex_match(DefaultContouringStr, TrueRegex);

    // Register a callback for capturing (all) logs for the duration of this operation.
    std::shared_timed_mutex ylogs_mutex;
    std::string ylogs;
    ygor::scoped_callback ylog_capture([&ylogs_mutex, &ylogs](ygor::log_message msg){
        const std::lock_guard<std::shared_timed_mutex> lock(ylogs_mutex);

        std::stringstream ss;
        const std::time_t t_conv = std::chrono::system_clock::to_time_t(msg.t);
        ss << "--(" << log_level_to_string(msg.ll) << ")";
        ss << " " << ygor::get_localtime_str(t_conv);
        ss << " thread 0x" << std::hex << msg.tid << std::dec;
        ss << " function '" << msg.fn << "'";
        ss << " file '" << msg.fl << "'";
        ss << " line " << msg.sl;
        ss << ": " << msg.msg << ".";
        ylogs += ss.str() + "\n";

        // Trim earlier messages if the log is holding 'lots' of data.
        const auto limit = 10UL * 1024UL * 1024UL; // MB.
        while(limit < ylogs.size()){
            auto c = ylogs.find('\n', static_cast<std::string::size_type>(limit / 10UL));
            if(c == std::string::npos){
                ylogs.clear();
            }else{
                ylogs.erase(0, c);
            }
        }
        return;
    });

    // Register a callback for displaying certain logs as tray notifications.
    std::atomic<bool> ylog_relay_enabled(true);
    std::shared_ptr<ygor::scoped_callback> ylog_relay;
    ylog_relay = std::make_shared<ygor::scoped_callback>( [&ylog_relay_enabled](ygor::log_message msg){
        if( ylog_relay_enabled.load()
        &&  (ygor::log_level::warn <= msg.ll)  ){
            notification_t n;
            n.urgency = ( ygor::log_level::info == msg.ll ) ? notification_urgency_t::low :
                        ( ygor::log_level::warn == msg.ll ) ? notification_urgency_t::medium :
                        ( ygor::log_level::err  == msg.ll ) ? notification_urgency_t::high : notification_urgency_t::medium;
            n.message = msg.msg;
            n.duration = static_cast<int32_t>(10000);
            if(!tray_notification(n)){
                ylog_relay_enabled = false;
                YLOGWARN("Unable to emit tray notification, disabling further tray notifications");
            }
        }
        return;
    });

    // --------------------------------------- Operational State ------------------------------------------
    std::shared_timed_mutex drover_mutex;
    const auto mutex_dt = std::chrono::microseconds(5);

    const std::chrono::time_point<std::chrono::system_clock> t_start = std::chrono::system_clock::now();

    Explicator X(FilenameLex);

    struct View_Toggles {
        bool set_about_popup = false;
        bool view_imgui_demo = false;
        bool view_implot_demo = false;
        bool view_documentation_enabled = false;
        bool view_metrics_window = false;

        bool view_images_enabled = true;
        bool view_image_metadata_enabled = false;
        bool view_contours_enabled = true;
        bool view_contouring_enabled = false; // Overridden below.
        bool view_contouring_debug = false;
        bool view_drawing_enabled = false;
        bool view_row_column_profiles = false;
        bool view_time_profiles = false;
        bool view_image_feature_extraction = false;
        bool save_time_profiles = false;
        bool save_row_column_profiles = false;

        bool view_meshes_enabled = true;
        bool view_mesh_metadata_enabled = false;

        bool view_plots_enabled = true;
        bool view_plots_metadata = true;

        bool view_parameter_table = false;
        bool view_lexicon_customizer = false; // Overridden below.

        bool view_ylogs = false;

        bool view_tables_enabled = true;
        bool view_table_metadata_enabled = false;

        bool view_rtplans_enabled = true;
        bool view_rtplan_metadata_enabled = false;

        bool view_psets_enabled = true;
        bool view_psets_metadata_enabled = false;

        bool view_tforms_enabled = true;
        bool view_tforms_metadata_enabled = false;

        bool view_script_editor_enabled = false;
        bool view_script_feedback = true;

        bool show_image_hover_tooltips = true;

        bool adjust_window_level_enabled = false;
        bool adjust_colour_map_enabled = false;

        bool view_shader_editor_enabled = false;

        bool view_polyominoes_enabled = false;
        bool view_triple_three_enabled = false;
        bool view_encompass_enabled = false;
        bool view_cube_enabled = false;

        bool view_guides_enabled = true;
    } view_toggles;
    view_toggles.view_contouring_enabled = DefaultContouring;
    view_toggles.view_lexicon_customizer = DefaultLexiconCustomizer;

    // Documentation state.
    std::string docs_str;
    std::map<std::string, std::string> docs_ops; // op_name, text documentation.

    // Plot viewer state.
    std::map<int64_t, bool> lsamps_visible;
    enum class plot_norm_t {
        none,
        max,
    } plot_norm = plot_norm_t::none;
    bool show_plot_legend = true;
    float plot_thickness = 1.0;

    // Image viewer state.
    int64_t img_array_num = -1; // The image array currently displayed.
    int64_t img_num = -1; // The image currently displayed.
    int64_t img_channel = -1; // Which channel to display.
    bool img_is_rgb = false; // Treat the image as though it were in RGB format already.
    bool use_texture_antialiasing = false; // Controls whether textures use mipmaps for antialiasing.
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
        std::make_pair("Composite_50_90_100_107_110", ColourMap_Composite_50_90_100_107_110),

        std::make_pair("DICOM_Hot_Iron", ColourMap_DICOM_Hot_Iron),
        std::make_pair("DICOM_PET", ColourMap_DICOM_PET),
        std::make_pair("DICOM_Hot_Metal_Blue", ColourMap_DICOM_Hot_Metal_Blue),
        std::make_pair("DICOM_PET_20_Step", ColourMap_DICOM_PET_20_Step),
    };
    size_t colour_map = 0;

    const auto nan_colour = std::array<std::byte, 3>{ std::byte{60}, std::byte{0}, std::byte{0} }; // 8-bit colour.

    auto pos_contour_colour = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    auto neg_contour_colour = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    auto editing_contour_colour = ImVec4(1.0f, 0.45f, 0.0f, 1.0f);
    auto adjacent_contour_colour = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);

    auto line_numbers_normal_colour = ImVec4(1.0f, 1.0f, 1.0f, 0.3f);
    auto line_numbers_debug_colour  = ImVec4(0.4f, 1.0f, 0.4f, 0.8f);
    auto line_numbers_info_colour   = ImVec4(0.4f, 0.4f, 1.0f, 0.7f);
    auto line_numbers_warn_colour   = ImVec4(0.7f, 0.5f, 0.1f, 0.8f);
    auto line_numbers_error_colour  = ImVec4(1.0f, 0.1f, 0.1f, 0.8f);
    
    const auto get_unique_colour = [](int64_t i){
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
    int64_t mesh_num = -1;
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

        double rot_y = 0.0; // Yaw.
        double rot_p = 0.0; // Pitch.
        double rot_r = 0.0; // Roll.

        double zoom = 1.0;
        double cam_distort = 0.0;

        // Transformations applied to all models.
        num_array<float> model = num_array<float>().identity(4);

        // Nominal colours.
        std::array<float, 4> colours = { 1.000, 0.588, 0.005, 0.8 };
    } mesh_display_transform;

    // Tables.
    using disp_table_it_t = decltype(DICOM_data.table_data.begin());
    struct table_display_t {
        int64_t table_num = -1;
        bool use_keyword_highlighting = true;
        std::map<std::string, ImVec4> colours = { { std::string("pass"),  ImVec4(0.175f, 0.500f, 0.000f, 1.00f) },
                                                  { std::string("true"),  ImVec4(0.175f, 0.500f, 0.000f, 1.00f) },
                                                  { std::string("fail"),  ImVec4(0.600f, 0.100f, 0.000f, 1.00f) },
                                                  { std::string("false"), ImVec4(0.600f, 0.100f, 0.000f, 1.00f) } };

        ImVec4 selected_colour = ImVec4(0.260f, 0.590f, 0.980f, 0.50f);
        //ImVec4 pass_colour = ImVec4(0.175f, 0.500f, 0.000f, 1.00f);
        //ImVec4 fail_colour = ImVec4(0.600f, 0.100f, 0.000f, 1.00f);
    } table_display;
    using table_cell_bounds_t = std::pair<tables::cell_coord_t, tables::cell_coord_t>; // row_bounds, col_bounds
    std::set< tables::cell_coord_t > table_selection;
    std::optional< tables::cell_coord_t > cell_selected;
    std::optional< tables::cell_coord_t > cell_being_edited;
    int64_t cell_being_edited_first_frame = 0;
    std::optional<tables::cell_coord_t> set_focus_on_cell;

    const auto get_table_selection_bounds = [](const std::set< tables::cell_coord_t >& table_selection) -> std::optional<table_cell_bounds_t> {
        std::optional<table_cell_bounds_t> out;
        if(!table_selection.empty()){
            const auto seed_coord = *(std::begin(table_selection));
            tables::cell_coord_t row_bounds = std::make_pair(seed_coord.first, seed_coord.first);
            tables::cell_coord_t col_bounds = std::make_pair(seed_coord.second, seed_coord.second);
            for(const auto& c : table_selection){ // r, c
                const auto row = c.first;
                const auto col = c.second;

                if(row < row_bounds.first) row_bounds.first = row;
                if(col < col_bounds.first) col_bounds.first = col;
                if(row_bounds.second < row) row_bounds.second = row;
                if(col_bounds.second < col) col_bounds.second = col;
            }
            out = std::make_pair(row_bounds, col_bounds);
        }
        return out;
    };

    // RT Plans.
    using disp_rtplan_it_t = decltype(DICOM_data.rtplan_data.begin());
    int64_t rtplan_num = -1;
    int64_t rtplan_dynstate_num = -1;
    int64_t rtplan_statstate_num = -1;

    // Point Sets / Point Clouds.
    using disp_pset_it_t = decltype(DICOM_data.point_data.begin());
    int64_t pset_num = -1;

    // Transforms.
    using disp_tform_it_t = decltype(DICOM_data.trans_data.begin());
    int64_t tform_num = -1;


    // Image feature extraction.
    struct img_features_t {
        point_set<double> features_A;  // Input features (A).
        point_set<double> features_B;  // Input features (B).
        point_set<double> features_C;  // Output features (transformed features from A onto B coordinate system).

        std::string metadata_key = "FrameOfReferenceUID";
        std::string description;
        std::array<char, 2048> buff;

        float snap_dist = 5.0f; // Distance between mouse click and feature to 'snap' click to existing feature.

        std::array<float, 4> o_col = { 1.0f, 1.0f, 1.0f, 1.0f }; // Override colour.
        bool use_override_colour = false;
    } img_features;
    size_t feature_transform_num = 0U;

    // ------------------------------------------ Viewer State --------------------------------------------
    auto background_colour = ImVec4(0.025f, 0.087f, 0.118f, 1.00f); // For dark mode.
    //auto background_colour = ImVec4(0.8f, 0.9f, 1.0f, 1.00f); // For light mode.

    struct image_mouse_pos_s {
        bool mouse_hovering_image;
        bool image_window_focused;
        bool image_window_hovered;

        float region_x;   // [0,1] clamped position of mouse on image.
        float region_y;

        int64_t r; // Row and column number of current mouse position.
        int64_t c;

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
    std::array<char, 2048> row_profile_text_entry;
    string_to_array(row_profile_text_entry, "");
    std::array<char, 2048> col_profile_text_entry;
    string_to_array(col_profile_text_entry, "");

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
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE); // Enable file/dir drag-and-drop.

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if(gl_context == nullptr){
        throw std::runtime_error("Unable to create an OpenGL context for SDL: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_MakeCurrent(window, gl_context)){
        throw std::runtime_error("Unable to associate OpenGL context with SDL window: "_s + SDL_GetError());
    }
    if(SDL_GL_SetSwapInterval(-1) != 0){ // Enable adaptive vsync to limit the frame rate.
        if(SDL_GL_SetSwapInterval(1) != 0){ // Enable vsync (non-adaptive).
            YLOGINFO("Unable to enable vsync. Continuing without it");
        }
    }

    glewExperimental = true; // Bug fix for glew v1.13.0 and earlier.
    if(glewInit() != GLEW_OK){
        throw std::runtime_error("Glew was unable to initialize OpenGL");
    }
    try{
        CHECK_FOR_GL_ERRORS(); // Clear any errors encountered during glewInit.
    }catch(const std::exception &e){
        YLOGINFO("Ignoring glew-related error: " << e.what());
    }

    // Create an ImGui context we can use and associate it with the OpenGL context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    [[maybe_unused]] ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // If the user has an existing imgui config file, then honour it.
    if(io.IniFilename != nullptr){
        bool use_imgui_config = false;
        try{
            const auto p = std::filesystem::current_path() / io.IniFilename;
            if(std::filesystem::is_regular_file(p)){
                use_imgui_config = true;
                YLOGINFO("Using existing ImGui configuration at '" << p.string() << "'");
            }
        }catch(const std::exception &){ }

        if(!use_imgui_config){
            io.IniFilename = nullptr;
        }
    }

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

        YLOGINFO("Initialized OpenGL '" << gl_version << "' with GLSL '" << glsl_version << "'");
    }catch(const std::exception &e){
        YLOGWARN("Unable to detect OpenGL/GLSL version");
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

    //YLOGINFO("Using default vertex shader source: '" << array_to_string(vert_shader_src) << "'");
    //YLOGINFO("Using default fragment shader source: '" << array_to_string(frag_shader_src) << "'");

    // Note: the following will throw if the default shader fails to compile and link.
    auto custom_shader = compile_shader_program(vert_shader_src, frag_shader_src, shader_log);

    // -------------------------------- Functors for various things ---------------------------------------

    // Create an OpenGL texture from an image.
    struct opengl_texture_handle_t {
        GLuint texture_number = 0;
        int64_t col_count = 0L;
        int64_t row_count = 0L;
        float aspect_ratio = 1.0; // In image pixel space: height / width.
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

    // Lexicon customizer state.
    std::map<std::string, std::string> lexicon_overrides;
    std::array<char, 2048> lexicon_override_buffer;
    string_to_array(lexicon_override_buffer, "");
    auto lexicon_exact_match_colour = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);

    // Scale bar for showing current colour map.
    vec3<double> zero3(0.0, 0.0, 0.0);
    planar_image<float,double> scale_bar_img;
    scale_bar_img.init_buffer(1L, 100L, 1L);
    scale_bar_img.init_spatial(1.0, 1.0, 1.0, zero3, zero3);
    scale_bar_img.init_orientation(vec3<double>(1.0, 0.0, 0.0), vec3<double>(0.0, 1.0, 0.0));
    for(int64_t c = 0; c < scale_bar_img.columns; ++c){
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
    bool contouring_show_adjacent = true;
    std::string contouring_method = "marching-squares";
    brush_t contouring_brush = brush_t::tanh_2D;
    float last_mouse_button_0_down = 1E30;
    float last_mouse_button_1_down = 1E30;
    std::optional<vec3<double>> last_mouse_button_pos;

    Drover_Cache contouring_drover_cache;
    std::shared_ptr<Drover> cdrover_ptr = contouring_drover_cache.create_drover().second;
    cdrover_ptr->Ensure_Contour_Data_Allocated();
    cdrover_ptr->image_data.push_back(std::make_unique<Image_Array>());
    cdrover_ptr->image_data.back()->imagecoll.images.emplace_back();

    std::array<char, 2048> new_contour_name;
    string_to_array(new_contour_name, "");
    bool overwrite_existing_contours = false;
    std::optional<decltype(std::begin(DICOM_data.contour_data->ccs))> edit_existing_contour_selection;
    std::vector<std::string> contour_overlap_styles = {
        "ignore",
        "honour_opposite_orientations",
        "overlapping_contours_cancel" };
    size_t contour_overlap_style = 0UL;

    std::shared_timed_mutex extracted_contours_mutex;
    std::optional<Drover> extracted_contours;
    std::atomic<int64_t> contour_extraction_underway = 0L;

    // Polyominoes state.
    opengl_texture_handle_t polyomino_texture;
    Drover polyomino_imgs;
    std::chrono::time_point<std::chrono::steady_clock> t_polyomino_updated;
    double dt_polyomino_update = 500.0; // milliseconds
    bool polyomino_paused = false;
    int polyomino_family = 0;

    // Triple-three state.
    tt_game_t tt_game;
    tt_game.reset();
    bool tt_hidden = false;
    std::chrono::time_point<std::chrono::steady_clock> t_tt_updated;
    double dt_tt_update = 3000.0; // milliseconds
    std::array<int8_t, 9> tt_cell_owner; // Used for card-flip animations.
    tt_cell_owner.fill(-1);
    std::array<decltype(t_tt_updated), 9> tt_cell_owner_time; // Used for card-flip animations.
    float tt_anim_dt = 1000.0f; // ms

    // Encompass state.
    struct en_game_obj_t {
        vec2<double> pos; // position.
        vec2<double> vel; // velocity.
        double rad;       // radius, which also implies mass since we assume constant mass density.
        bool player_controlled = false;
    };
    std::vector< en_game_obj_t > en_game_objs;
    std::chrono::time_point<std::chrono::steady_clock> t_en_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_en_started;

    struct en_game_t {
        int64_t N_objs = 250L;        // Initial configuration target number of objects.

        double min_radius = 3.0;      // Objects cannot be smaller than this.
        double max_radius = 60.0;     // Only used for initial configuration.

        double box_width  = 1000.0;   // World bounds.
        double box_height = 800.0;

        double max_speed = 25.0;      // The maximum speed an object can attain.
                                      // It's possible that objects might temporarily be faster, so consider
                                      // the upper limit to be slightly higher in practice.

        double mutiny_period = 300.0; // Relates to how often some mass 'leaks' spontaneously.
        double mutiny_slope = 75.0;   // Relates to likelihood of mutiny due to area (logistic slope).
        double mutiny_mid = 100.0;    // Relates to likelihood of mutiny due to area (logistic midpoint).

        std::mt19937 re;
    } en_game;
    en_game.re.seed( std::random_device()() );

    const auto reset_en_game = [&](){
        en_game_objs.clear();

        // First, generate radii according to some distribution.
        std::vector<double> radii(en_game.N_objs, 1.0);
        {
            double dof = 3.0;
            std::chi_squared_distribution<> rd(dof);
            for(auto &r : radii){
                r = rd(en_game.re);
            }
            std::sort( std::begin(radii), std::end(radii) );

            // Rescale so all are [min_radius, max_radius].
            const auto curr_min = radii.front();
            const auto curr_max = radii.back();
            for(auto &r : radii){
                const auto clamped = (r - curr_min) / (curr_max - curr_min);
                r = en_game.min_radius + (en_game.max_radius - en_game.min_radius) * clamped;
            }
        }

        // Then generate placements and momentums, starting with the largest objects.
        std::reverse( std::begin(radii), std::end(radii) );

        const auto intersects_existing = [&]( const vec2<double> &pos, double rad ) -> bool {
            bool intersection = false;
            for(const auto& obj : en_game_objs){
                const auto sep = pos.distance(obj.pos);
                const auto min = rad + obj.rad;
                if(sep <= min){
                    intersection = true;
                    break;
                }
            }
            return intersection;
        };

        const auto intersects_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
            return (pos.x <= (0.0 + rad))
                || ((en_game.box_width - rad) <= pos.x)
                || (pos.y <= (0.0 + rad))
                || ((en_game.box_height - rad) <= pos.y);
        };

        std::uniform_real_distribution<> rd_x(0.0, en_game.box_width);
        std::uniform_real_distribution<> rd_y(0.0, en_game.box_height);
        std::uniform_real_distribution<> rd_v(-0.05 * en_game.max_speed, 0.05 * en_game.max_speed);
        //auto f_x = std::bind(rd_x,re);
        //auto f_y = std::bind(rd_y,re);
        //auto f_v = std::bind(rd_v,re);
        for(const auto &r : radii){
            int64_t i = 100L;
            while(true){
                const vec2<double> pos( rd_x(en_game.re), rd_y(en_game.re) );
                const vec2<double> vel( rd_v(en_game.re), rd_v(en_game.re) );
                const auto rad = r;
//std::cout << "Attempting to place object with pos, vel, rad = " << pos << ", " << vel << ", " << rad << std::endl;
                if( !intersects_wall(pos, rad)
                &&  !intersects_existing(pos, rad) ){
                    en_game_objs.emplace_back();
//std::cout << "Successfully placed." << std::endl;
                    en_game_objs.back().pos = pos;
                    en_game_objs.back().vel = vel;
                    en_game_objs.back().rad = rad;
                    en_game_objs.back().player_controlled = false;
                    break;
                }
                if(--i < 0L){
//std::cout << "Unable to place." << std::endl;

                    YLOGWARN("Unable to place object after 100 attempts. Ignoring object");
                    break;
                }
            }
        }
        
        // Select one object to be under player control.
        {
            auto n = static_cast<int64_t>( std::round(static_cast<float>(en_game_objs.size()) * 0.75) );
            n = std::clamp<int64_t>(n, 0L, en_game_objs.size()-1L);
            en_game_objs.at(n).player_controlled = true;
        }


        // Reset the update time.
        const auto t_now = std::chrono::steady_clock::now();
        t_en_updated = t_now;
        t_en_started = t_now;
        return;
    };

    // Cube state.
    std::chrono::time_point<std::chrono::steady_clock> t_cube_updated;
    std::chrono::time_point<std::chrono::steady_clock> t_cube_started;

    int64_t rc_game_size = 4L;
    rc_game_t rc_game;

    const auto reset_cube_game = [&](){

        rc_game.reset(rc_game_size);

        // Reset the update time.
        const auto t_now = std::chrono::steady_clock::now();
        t_cube_updated = t_now;
        t_cube_started = t_now;
        return;
    };
    reset_cube_game();

    // Guide state.
    std::shared_timed_mutex guide_mutex;
    std::list<guide_stage_t> guide_stages;
    int64_t guide_stage_num = -1;
    std::list<opengl_texture_handle_t> guide_image_textures;



    // Save a copy of the current cdrover_ptr object's Drover in the cache.
    const auto create_cdrover_snapshot = [](Drover_Cache &l_contouring_drover_cache,
                                            std::shared_ptr<Drover> l_cdrover_ptr ){
        auto l_cdrover_c = l_cdrover_ptr->Deep_Copy();
        l_contouring_drover_cache.store_drover( std::move(l_cdrover_c) );
        return l_contouring_drover_cache.get();

        // Note: you will need to regenerate contouring image iterators after calling this!
    };

    // Resets the contouring image to match the display image characteristics.
    const auto reset_contouring_state = [&cdrover_ptr,
                                         &contouring_drover_cache,
                                         &contouring_img_row_col_count]( img_array_ptr_it_t  dimg_array_ptr_it ) -> void {
            contouring_img_row_col_count = std::clamp<int>(contouring_img_row_col_count, 5, 1024);

            // Clear all undo history except the current item.
            const auto l_v_opt = contouring_drover_cache.get_version(cdrover_ptr);
            contouring_drover_cache.trim_except(l_v_opt.value_or(0L));
            if(contouring_drover_cache.empty()){
                YLOGWARN("Contouring drover cache is empty; re-seeding the cache");
                contouring_drover_cache.create_drover();
                cdrover_ptr = contouring_drover_cache.get();
            }

            // Reset the contouring images.
            if(!cdrover_ptr->Has_Image_Data()){
                cdrover_ptr->image_data.push_back(std::make_unique<Image_Array>());
            }
            cdrover_ptr->image_data.back()->imagecoll.images.clear();

            const auto regex_linkage_tags = Compile_Regex("^Patient.*|^Study.*|^Series.*|.*UID$");

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
                //const auto encompassing_images = cdrover_ptr->image_data.back()->imagecoll.get_images_which_encompass_all_points(points);
                //if(!encompassing_images.empty()) continue;

                const auto centre = dimg.center();
                const auto A_corners = dimg.corners2D();
                auto encompassing_images = cdrover_ptr->image_data.back()->imagecoll.get_images_which_sandwich_point_within_top_bottom_planes( centre );
                encompassing_images.remove_if([&](const decltype(cdrover_ptr->image_data.back()->imagecoll.get_all_images().front()) &img_it){
                    const auto B_corners = img_it->corners2D();

                    //Fixed corner-to-corner distance.
                    double dist = 0.0;
                    for(auto A_it = std::begin(A_corners), B_it = std::begin(B_corners);
                        (A_it != std::end(A_corners)) && (B_it != std::end(B_corners)); ){
                        dist += A_it->sq_dist(*B_it);
                        ++A_it;
                        ++B_it;
                    }
                    return (std::min<double>(dimg.pxl_dx, dimg.pxl_dy) < dist);
                });
                if(!encompassing_images.empty()) continue;

                // Add this image to the list of spatially-distinct images.
                cdrover_ptr->image_data.back()->imagecoll.images.emplace_back();
                const auto cimg_ptr = &(cdrover_ptr->image_data.back()->imagecoll.images.back());

                // Make the contouring image spatial extent match the display image, except with a different number of
                // rows and columns. This will make it easy to translate contours back and forth.
                const auto cimg_pxl_dy = dimg.pxl_dy * static_cast<float>(dimg.rows)/static_cast<float>(contouring_img_row_col_count);
                const auto cimg_pxl_dx = dimg.pxl_dx * static_cast<float>(dimg.columns)/static_cast<float>(contouring_img_row_col_count);
                const auto cimg_offset = dimg.offset - dimg.row_unit * dimg.pxl_dx * 0.5
                                                     - dimg.col_unit * dimg.pxl_dy * 0.5
                                                     + dimg.row_unit * cimg_pxl_dx * 0.5
                                                     + dimg.col_unit * cimg_pxl_dy * 0.5;
                cimg_ptr->init_buffer(contouring_img_row_col_count, contouring_img_row_col_count, 1L);
                cimg_ptr->init_spatial(cimg_pxl_dx, cimg_pxl_dy, dimg.pxl_dz, dimg.anchor, cimg_offset);
                cimg_ptr->init_orientation(dimg.row_unit, dimg.col_unit);
                cimg_ptr->fill_pixels(-1.0f);

                // Inherit metadata selectively so contours can images can be properly linked together.
                cimg_ptr->metadata = filter_keys_retain_only(dimg.metadata, regex_linkage_tags);
            }

            // Reset any existing contours.
            cdrover_ptr->Ensure_Contour_Data_Allocated();
            cdrover_ptr->contour_data->ccs.clear();
            YLOGINFO("Reset contouring state with " << cdrover_ptr->image_data.back()->imagecoll.images.size() << " images");

            return;
    };

    const auto Free_OpenGL_Texture = [](opengl_texture_handle_t &tex){
        // Release the previous texture, iff needed.
        if( tex.texture_exists &&
            (tex.texture_number != 0) ){
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
                                      &nan_colour  ]( const planar_image<float,double>& img,
                                                      const int64_t img_channel,
                                                      const bool img_is_rgb,
                                                      const bool use_texture_antialiasing,
                                                      const std::optional<double>& custom_centre,
                                                      const std::optional<double>& custom_width ) -> opengl_texture_handle_t {

            const auto img_cols = img.columns;
            const auto img_rows = img.rows;
            const auto img_chns = img.channels;

            if(!isininc(1,img_rows,50000) || !isininc(1,img_cols,50000)){
                throw std::invalid_argument("Image dimensions are not reasonable. Refusing to continue");
            }
            if(!img_is_rgb && !isininc(1,img_channel+1,img_chns)){
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

            const auto UseAsRGB = img_is_rgb; // Ignore colour mapping and scaling and render image as-is.
            const auto UseCustomWL = (custom_win_c && custom_win_fw);
            const auto UseImgWL = (UseCustomWL) ? false 
                                                : (   (img_chns == 1) // Only honour for single-channel images.
                                                   && img_win_valid 
                                                   && img_desc
                                                   && img_win_c 
                                                   && img_win_fw 
                                                   && (img_win_valid.value() == img_desc.value()));

            if( UseAsRGB ){
                std::vector<int64_t> l_channels {
                    ( (0 < img_chns) ? 0 : -1 ), // This is to support missing or disordered channels.
                    ( (1 < img_chns) ? 1 : -1 ),
                    ( (2 < img_chns) ? 2 : -1 )
                };

                const auto destmin = static_cast<float>( std::numeric_limits<uint8_t>::lowest() );
                const auto destmax = static_cast<float>( std::numeric_limits<uint8_t>::max() );
                const float nan = std::numeric_limits<float>::quiet_NaN();

                for(auto j = 0; j < img_rows; ++j){ 
                    for(auto i = 0; i < img_cols; ++i){ 
                        for(const auto& chn : l_channels){
                            const auto val = (0 <= chn) ? img.value(j,i,chn) : nan;

                            if(std::isfinite(val)){
                                const auto clamped = std::clamp<float>(std::round(val), destmin, destmax);
                                const auto casted = static_cast<uint8_t>(clamped);
                                animage.push_back(std::byte{casted});
                            }else{
                                animage.push_back(std::byte{0});
                            }
                        }
                    }
                }

            }else if( UseCustomWL || UseImgWL ){
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
                const auto destmin = static_cast<double>( std::numeric_limits<uint8_t>::lowest() );
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
                img.apply_to_pixels([&rmm,&img_channel](int64_t /*row*/,
                                                        int64_t /*col*/,
                                                        int64_t chnl,
                                                        pixel_value_t val) -> void {
                    if( (img_channel < 0)
                    ||  (chnl == img_channel) ) rmm.Digest(val);
                    return;
                });
                const auto lowest = rmm.Current_Min();
                const auto highest = rmm.Current_Max();
                //const auto lowest = Stats::Percentile(img.data, 0.01);
                //const auto highest = Stats::Percentile(img.data, 0.99);

                // Rescale avoiding overflow if lowest and highest span the full range, avoiding division by zero if
                // lowest is zero, and using a null transformation if lowest and highest are equal. Also avoid 'trial'
                // division in case floats are not IEEE 754.
                //
                // We do this by setting the slope and intercept rescale parameters for each scenario.
                const auto zero = static_cast<pixel_value_t>(0);
                const auto one  = static_cast<pixel_value_t>(1);
                const bool lowest_is_zero = !std::isnormal(lowest);
                const bool lowest_is_highest = !std::isnormal(highest - lowest);

                auto rescale_m = zero;
                auto rescale_b = one;
                if( lowest_is_zero
                &&  lowest_is_highest ){
                    // There is no sensible scale, so set everything to zero.
                    rescale_m = zero;
                    rescale_b = zero;

                }else if( lowest_is_zero
                      &&  !lowest_is_highest ){
                    // Since lowest is not normal, highest is necessarily normal.
                    rescale_m = one / highest;
                    rescale_b = zero;

                }else{
                    // All numbers and inverses are finite, so just need to avoid overflow.
                    // Rescale like (val - low)/(high - low) = (val/low - 1)/(high/low - 1).
                    const auto inv_lowest = one / lowest;
                    const auto inv_denom = one / (highest * inv_lowest - one);
                    rescale_m = inv_lowest * inv_denom;
                    rescale_b = -inv_denom;
                }

                const auto dest_type_max = static_cast<double>(std::numeric_limits<uint8_t>::max());
                const auto dest_type_min = static_cast<double>(std::numeric_limits<uint8_t>::lowest());

                for(auto j = 0; j < img_rows; ++j){ 
                    for(auto i = 0; i < img_cols; ++i){ 
                        const auto val = img.value(j,i,img_channel);
                        if(!std::isfinite(val)){
                            animage.push_back( nan_colour[0] );
                            animage.push_back( nan_colour[1] );
                            animage.push_back( nan_colour[2] );
                        }else{
                            const auto rescaled_value = std::clamp<float>(val * rescale_m + rescale_b, 0.0f, 1.0f);
                            const auto res = colour_maps[colour_map].second(rescaled_value);
                            const double x_R = res.R;
                            const double x_G = res.G;
                            const double x_B = res.B;
                            const auto scaled_R = static_cast<uint8_t>(dest_type_min + x_R * dest_type_max);
                            const auto scaled_G = static_cast<uint8_t>(dest_type_min + x_G * dest_type_max);
                            const auto scaled_B = static_cast<uint8_t>(dest_type_min + x_B * dest_type_max);

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
            out.aspect_ratio = (img.pxl_dy * static_cast<float>(img_rows)) / (img.pxl_dx * static_cast<float>(img_cols));
            out.aspect_ratio = std::isfinite(out.aspect_ratio) ? out.aspect_ratio : (img.pxl_dy / img.pxl_dx);
            out.aspect_ratio = std::isfinite(out.aspect_ratio) ? out.aspect_ratio : (static_cast<float>(img_rows) / static_cast<float>(img_cols));

            CHECK_FOR_GL_ERRORS();

            glGenTextures(1, &out.texture_number);
            glBindTexture(GL_TEXTURE_2D, out.texture_number);
            if(out.texture_number == 0){
                throw std::runtime_error("Unable to assign OpenGL texture");
            }
            CHECK_FOR_GL_ERRORS();

            if(use_texture_antialiasing){
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); //MIRRORED_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); //MIRRORED_REPEAT);
            }else{
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }

            CHECK_FOR_GL_ERRORS();

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                         static_cast<int>(out.col_count), static_cast<int>(out.row_count),
                         0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<void*>(animage.data()));
            CHECK_FOR_GL_ERRORS();

            if(use_texture_antialiasing){
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            glBindTexture(GL_TEXTURE_2D, 0);
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
    const auto recompute_cimage_iters = [ &cdrover_ptr,
                                          &recompute_image_iters ](){
        std::tuple<bool, img_array_ptr_it_t, disp_img_it_t > out;
        std::get<bool>( out ) = false;
        const int64_t cimg_array_num = 0; // Currently only support one image array per Drover.

        // Set the current image array and image iters and load the texture.
        const auto has_cimages = cdrover_ptr->Has_Image_Data();
        auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
        do{ 
            if( !has_cimages ) break;
            if( !img_valid ) break;
            if( cdrover_ptr->image_data.size() != 1 ) throw std::logic_error("Multiple contouring image arrays not supported");
            if( !isininc(1, cimg_array_num+1, cdrover_ptr->image_data.size()) ) break;
            auto cimg_array_ptr_it = std::next(cdrover_ptr->image_data.begin(), cimg_array_num);
            if( cimg_array_ptr_it == std::end(cdrover_ptr->image_data) ) break;

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
                encompassing_images.remove_if(
                    [&,
                     disp_img_it = disp_img_it](const decltype((*cimg_array_ptr_it)->imagecoll.get_all_images().front()) &img_it){
                        const auto B_corners = img_it->corners2D();

                        //Fixed corner-to-corner distance.
                        double dist = 0.0;
                        for(auto A_it = std::begin(A_corners), B_it = std::begin(B_corners);
                            (A_it != std::end(A_corners)) && (B_it != std::end(B_corners)); ){
                            dist += A_it->sq_dist(*B_it);
                            ++A_it;
                            ++B_it;
                        }
                        return (std::min<double>(disp_img_it->pxl_dx, disp_img_it->pxl_dy) < dist);
                    }
                );
                if(encompassing_images.size() != 1) break;

                std::get<bool>( out ) = true;
                std::get<img_array_ptr_it_t>( out ) = cimg_array_ptr_it;
                std::get<disp_img_it_t>( out ) = encompassing_images.front();
            }catch(const std::exception &e){
                YLOGWARN("Contouring image not valid: '" << e.what() << "'");
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
                                         &img_is_rgb,
                                         &use_texture_antialiasing,
                                         &recompute_image_iters,
                                         &current_texture,
                                         &custom_centre,
                                         &custom_width,
                                         &need_to_reload_opengl_texture ](){

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
        do{
            // See if the current image numbers are valid.
            if( auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters(); img_valid ){
                break;
            }

            // Try reset to the first image.
            img_array_num = 0;
            img_num = 0;
            img_channel = 0;
            img_is_rgb = false;
            use_texture_antialiasing = false;
            if( auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters(); img_valid ){
                break;
            }

            // At this point, for whatever reason(s), the image data does not appear to be valid.
            // Set negative images numbers to disable showing anything.
            img_array_num = -1;
            img_num = -1;
            img_channel = -1;
            img_is_rgb = false;
            use_texture_antialiasing = false;
        }while(false);

        // Signal the need to reload the texture.
        need_to_reload_opengl_texture.store(true);
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
                                         &table_display ](){
        std::tuple<bool, disp_table_it_t > out;
        std::get<bool>( out ) = false;

        do{ 
            const auto has_tables = DICOM_data.Has_Table_Data();
            if( !has_tables ) break;
            if( !isininc(1, table_display.table_num+1, DICOM_data.table_data.size()) ) break;
            auto table_ptr_it = std::next(DICOM_data.table_data.begin(), table_display.table_num);
            if( table_ptr_it == std::end(DICOM_data.table_data) ) break;

            std::get<bool>( out ) = true;
            std::get<disp_table_it_t>( out ) = table_ptr_it;
        }while(false);
        return out;
    };

    // Recompute rtplan iterators for the current rtplan
    const auto recompute_rtplan_iters = [ &DICOM_data,
                                         &rtplan_num ](){
        std::tuple<bool, disp_rtplan_it_t > out;
        std::get<bool>( out ) = false;

        do{ 
            const auto has_rtplans = DICOM_data.Has_RTPlan_Data();
            if( !has_rtplans ) break;
            if( !isininc(1, rtplan_num+1, DICOM_data.rtplan_data.size()) ) break;
            auto rtplan_ptr_it = std::next(DICOM_data.rtplan_data.begin(), rtplan_num);
            if( rtplan_ptr_it == std::end(DICOM_data.rtplan_data) ) break;

            std::get<bool>( out ) = true;
            std::get<disp_rtplan_it_t>( out ) = rtplan_ptr_it;
        }while(false);
        return out;
    };

    // Recompute point set iterators for the current pset
    const auto recompute_pset_iters = [ &DICOM_data,
                                        &pset_num ](){
        std::tuple<bool, disp_pset_it_t > out;
        std::get<bool>( out ) = false;

        do{ 
            const auto has_psets = DICOM_data.Has_Point_Data();
            if( !has_psets ) break;
            if( !isininc(1, pset_num+1, DICOM_data.point_data.size()) ) break;
            auto pset_ptr_it = std::next(DICOM_data.point_data.begin(), pset_num);
            if( pset_ptr_it == std::end(DICOM_data.point_data) ) break;

            std::get<bool>( out ) = true;
            std::get<disp_pset_it_t>( out ) = pset_ptr_it;
        }while(false);
        return out;
    };

    // Recompute transform iterators for the current transform
    const auto recompute_tform_iters = [ &DICOM_data,
                                         &tform_num ](){
        std::tuple<bool, disp_tform_it_t > out;
        std::get<bool>( out ) = false;

        do{ 
            const auto has_tforms = DICOM_data.Has_Tran3_Data();
            if( !has_tforms ) break;
            if( !isininc(1, tform_num+1, DICOM_data.trans_data.size()) ) break;
            auto tform_ptr_it = std::next(DICOM_data.trans_data.begin(), tform_num);
            if( tform_ptr_it == std::end(DICOM_data.trans_data) ) break;

            std::get<bool>( out ) = true;
            std::get<disp_tform_it_t>( out ) = tform_ptr_it;
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
            const bool l_img_is_rgb = false;
            const bool l_use_texture_antialiasing = false;
            scale_bar_texture = Load_OpenGL_Texture( scale_bar_img, 0L, l_img_is_rgb, l_use_texture_antialiasing, {}, {} );
        }
        return;
    };

    const auto unregister_guide_textures = [ &guide_stages,
                                             &guide_stage_num,
                                             &guide_image_textures,
                                             &Free_OpenGL_Texture ]() -> void {
        for(auto& t : guide_image_textures){
            Free_OpenGL_Texture(t);
        }
        guide_image_textures.clear();
        return;
    };

    const auto register_guide_textures = [ &guide_stages,
                                           &guide_stage_num,
                                           &guide_image_textures,
                                           &unregister_guide_textures,
                                           &Load_OpenGL_Texture ]() -> void {
        unregister_guide_textures();

        if( guide_stages.empty() ) return;
        const auto first_stage_num = static_cast<int64_t>(0);
        const auto final_stage_num = static_cast<int64_t>(guide_stages.size() - 1);
        guide_stage_num = std::clamp(guide_stage_num, first_stage_num, final_stage_num);

        auto stage_it = std::next( std::begin(guide_stages), guide_stage_num );
        for(const auto& base64_img : stage_it->base64_images){
            try{
                const auto img_blob = Base64::Decode(base64_img);
                const auto pic = ReadImageUsingSTB(img_blob);
                for(const auto &img : pic.images){
                    const int64_t img_channel = 0L;
                    const bool l_img_is_rgb = true;
                    const bool l_use_texture_antialiasing = true;
                    guide_image_textures.emplace_back( Load_OpenGL_Texture(img, img_channel,
                                                                           l_img_is_rgb,
                                                                           l_use_texture_antialiasing, {}, {}) );
                }
            }catch(const std::exception &e){
                YLOGWARN("Unable to load embedded base64-encoded image: " << e.what());
            }
        }
        return;
    };

    const auto reset_guide = [ &guide_stages,
                               &guide_stage_num,
                               &guide_image_textures,
                               &unregister_guide_textures ]() -> void {
        guide_stages.clear();
        guide_stage_num = -1;

        unregister_guide_textures();
        return;
    };


    // Contour preprocessing. Expensive pre-processing steps are performed asynchronously in another thread.
    struct preprocessed_contour {
        int64_t epoch;
        ImU32 colour;
        std::string ROIName;
        std::string NormalizedROIName;
        contour_of_points<double> contour;

        preprocessed_contour() = default;
    };
    using preprocessed_contours_t = std::list<preprocessed_contour>;
    preprocessed_contours_t preprocessed_contours;
    std::map<std::string, ImVec4> contour_colours;
    std::atomic<int64_t> preprocessed_contour_epoch = 0L;
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
                                       &contour_colour_from_orientation ](int64_t epoch) -> void {
        preprocessed_contours_t out;

        decltype(contour_colours) contour_colours_l;
        bool contour_colour_from_orientation_l;
        {
            std::shared_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
            contour_colours_l = contour_colours;
            contour_colour_from_orientation_l = contour_colour_from_orientation;
        }
        std::set<std::string> encountered;

        int64_t n = contour_colours_l.size();

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
                        encountered.insert(ROIName);

                        if(contour_colours_l.count(ROIName) == 0){
                            contour_colours_l[ROIName] = get_unique_colour( n++ );
                        }
                    }
                }

                // Remove any irrelevant entries.
                {
                    auto it = std::begin(contour_colours_l);
                    const auto end = std::end(contour_colours_l);
                    while(it != end){
                        const bool was_encountered = (encountered.count(it->first) != 0UL);
                        if(was_encountered){
                            ++it;
                        }else{
                            it = contour_colours_l.erase(it);
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

                                       &cdrover_ptr,
                                       &recompute_image_iters,
                                       &X,
                                       &reset_contouring_state,
                                       &launch_contour_preprocessor,
                                       &overwrite_existing_contours ]( const std::string &roi_name ) -> bool {
            //std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            //auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_cimage_iters();
            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            try{
                if( !img_valid ) throw std::runtime_error("Contouring image not valid.");
                if(roi_name.empty()) throw std::runtime_error("Cannot save with an empty ROI name.");

                auto cm = (*img_array_ptr_it)->imagecoll.get_common_metadata({});
                cm = coalesce_metadata_for_rtstruct(cm);

                auto FrameOfReferenceUID_opt = get_as<std::string>(cm, "FrameOfReferenceUID");
                if(!FrameOfReferenceUID_opt){
                    throw std::runtime_error("Missing 'FrameOfReferenceUID' metadata element. Cannot continue.");
                }
                auto StudyInstanceUID_opt = get_as<std::string>(cm, "StudyInstanceUID");
                if(!StudyInstanceUID_opt){
                    throw std::runtime_error("Missing 'StudyInstanceUID' metadata element. Cannot continue.");
                }

                cdrover_ptr->Ensure_Contour_Data_Allocated();
                for(auto &cc : cdrover_ptr->contour_data->ccs){
                    //Trim empty contours from the shuttle.
                    cc.Purge_Contours_Below_Point_Count_Threshold(3);
                    if(cc.contours.empty()) throw std::runtime_error("Given empty contour collection. Contours need at least 3 vertices each.");
                }
                cdrover_ptr->Ensure_Contour_Data_Allocated();

                if(overwrite_existing_contours){
                    DICOM_data.Ensure_Contour_Data_Allocated();
                    DICOM_data.contour_data->ccs.remove_if( [roi_name](const contour_collection<double>& cc) -> bool {
                        const auto n_opt = cc.get_dominant_value_for_key("ROIName");
                        return n_opt && (n_opt.value() == roi_name);
                    });
                }

                // Inject metadata.
                for(auto &cc : cdrover_ptr->contour_data->ccs){
                    const double MinimumSeparation = disp_img_it->pxl_dz; // TODO: use more robust method here.
                    for(auto &cop : cc.contours){
                        coalesce(cop.metadata, cm);
                    }
                    cc.Insert_Metadata("ROIName", roi_name);
                    cc.Insert_Metadata("NormalizedROIName", X(roi_name));
                    cc.Insert_Metadata("ROINumber", "10000"); // TODO: find highest existing and ++ it.
                    cc.Insert_Metadata("MinimumSeparation", std::to_string(MinimumSeparation));
                }

                // Insert the contours into the Drover object.
                DICOM_data.Ensure_Contour_Data_Allocated();
                DICOM_data.contour_data->ccs.splice(std::end(DICOM_data.contour_data->ccs),cdrover_ptr->contour_data->ccs);
                YLOGINFO("Drover class imbued with new contour collection");

                cdrover_ptr->contour_data->ccs.clear();
                cdrover_ptr->Ensure_Contour_Data_Allocated();
                reset_contouring_state(img_array_ptr_it);
                launch_contour_preprocessor();

            }catch(const std::exception &e){
                YLOGWARN("Unable to save contour collection: '" << e.what() << "'");
                return false;
            }
            return true;
    };


    // Advance to the specified Image_Array. Also resets necessary display image iterators.
    const auto advance_to_image_array = [ &DICOM_data,
                                          &img_array_num,
                                          &img_num ](const int64_t n){
            const int64_t N_arrays = DICOM_data.image_data.size();
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
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();
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
                                    &img_num ](const int64_t n){
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();

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

    // General-purpose Drover processing offloading worker thread.
    work_queue<std::function<void(void)>> wq(1U);
    wq.submit_task([](){
        YLOGINFO("Worker thread ready");
        return;
    });

    // Open file dialog state.
    std::filesystem::path open_file_root = std::filesystem::current_path();
    std::array<char,2048> root_entry_text;

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
    std::list<std::shared_future<loaded_files_res>> loaded_files;

    // Load a list of files/directories.
    // Note: This is meant to be called asynchronously.
    const auto load_paths = [InvocationMetadata,
                             FilenameLex ](std::list<std::filesystem::path> paths) -> loaded_files_res {

        loaded_files_res lfs;
        lfs.res = false;
        lfs.InvocationMetadata = InvocationMetadata;
        std::list<OperationArgPkg> Operations;
        try{
            lfs.res = Load_Files(lfs.DICOM_data, lfs.InvocationMetadata, FilenameLex, Operations, paths);
            if(!Operations.empty()){
                 lfs.res = false;
                 YLOGWARN("Loaded file contains a script. Currently unable to handle script files here");
            }
        }catch(const std::exception &){};

        // Notify that the files can be inserted into the main Drover.
        return lfs;
    };

    // Launch an interactive dialog box for file selection.
    // Note: This is meant to be run asynchronously.
    const auto launch_file_open_dialog = [ &load_paths ](std::filesystem::path open_file_root) -> loaded_files_res {
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

        return load_paths(paths);
    };

    // Script files.
    struct script_file {
        std::filesystem::path path;
        bool altered = false;
        std::vector<char> content;
        std::list<script_feedback_t> feedback; // Compilation/validation messages.
    };
    std::vector<script_file> script_files;
    int64_t active_script_file = -1L;
    std::shared_mutex script_mutex;
    std::atomic<int64_t> script_epoch = 0L;
    const std::string new_script_content = "#!/usr/bin/env -S dicomautomaton_dispatcher -v\n\n";

    const auto append_to_script = [](std::vector<char> &content, const std::string &s) -> void {
        for(const auto &c : s) content.emplace_back(c);
        return;
    };

    struct loaded_scripts_res {
        bool res;
        std::vector<script_file> script_files;
    };
    std::shared_future<loaded_scripts_res> loaded_scripts;

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
                YLOGWARN("Unable to access script file '" << p.string() << "'");
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
                    YLOGINFO("Abandoning run due to potentially conflicting user activity");
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
                    YLOGWARN("Script execution failed");
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

    // Launch a thread to extract contours.
    // Note: This is meant to be called asynchronously. It should be provided with deep copies of all objects.
    const auto extract_contours = [InvocationMetadata,
                                   FilenameLex]( Drover contouring_drover,
                                                 std::string contouring_method ) -> Drover {

        contouring_drover.Ensure_Contour_Data_Allocated();
        contouring_drover.contour_data->ccs.clear();


        std::list<OperationArgPkg> Operations;
        const bool op_load_res = Load_Standard_Script( Operations, "plumbing", "extract contours from mask" );
        if(!op_load_res) throw std::runtime_error("Unable to load script");
        auto l_InvocationMetadata = InvocationMetadata;
        l_InvocationMetadata["method"] = contouring_method;

        const auto res = Operation_Dispatcher(contouring_drover, l_InvocationMetadata, FilenameLex, Operations);
        if(!res){
            YLOGWARN("Contour extraction failed");

            // Signal to NOT replace the Drover class to the receiving thread.
            throw std::runtime_error("Unable to extract contours");
        }
        return contouring_drover;
    };

    // Launch a thread to export interactively.
    const auto export_as_DICOM = [ &drover_mutex,
                                   &DICOM_data,
                                   &InvocationMetadata,
                                   &FilenameLex,

                                   &img_array_num,

                                   //&recompute_image_state,
                                   //&recompute_image_iters,

                                   &wq ]( std::string plumbing_script_name ) -> void {

        // Submit the work to the worker thread.
        const auto worker = [&, plumbing_script_name](){

            std::list<OperationArgPkg> Operations;
            const bool op_load_res = Load_Standard_Script( Operations, "plumbing", plumbing_script_name );
            if(!op_load_res) throw std::runtime_error("Unable to load script");

            // Probably don't need an exclusive lock here, but doing so just in case the loaded script modifies the
            // Drover state somehow.
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);

            const auto l_img_array_num = img_array_num;
            auto l_InvocationMetadata = InvocationMetadata;

            // Selector used only for images.
            l_InvocationMetadata["img_arr_selection"] = "#"_s + std::to_string(l_img_array_num);

            const auto res = Operation_Dispatcher(DICOM_data, l_InvocationMetadata, FilenameLex, Operations);
            if(!res){
                YLOGWARN("Export failed");
            }

            return;
        };
        wq.submit_task( worker ); // Offload to waiting worker thread.

        return;
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

    // Load a guide passed as an argument to this operation.
    if(GuideOpt){
        std::unique_lock<std::shared_timed_mutex> guide_lock(guide_mutex, mutex_dt);
        if(!guide_lock) return false;

        auto l_guide_stages = parse_guide(GuideOpt.value());
        if(!l_guide_stages.empty()){
            guide_stages = l_guide_stages;
            guide_stage_num = 0;
            register_guide_textures();
            view_toggles.view_guides_enabled = true;
        }
    }


    int64_t frame_count = 0;
    while(true){
        ++frame_count;
        image_mouse_pos_opt = {};

        // Poll for queued SDL events.
        {
            SDL_Event event;
            bool close_window = false;
            std::list<std::filesystem::path> paths; // Drag-and-dropped paths from OS/window manager.

            while(SDL_PollEvent(&event)){
                if(event.type == SDL_QUIT){
                    close_window = true;
                    break;

                }else if( (event.type == SDL_WINDOWEVENT)
                      &&  (event.window.event == SDL_WINDOWEVENT_CLOSE)
                      &&  (event.window.windowID == SDL_GetWindowID(window)) ){
                    close_window = true;
                    break;

                }else if( event.type == SDL_DROPFILE ){
                    if(nullptr != event.drop.file){
                        const std::string p(event.drop.file);
                        SDL_free(event.drop.file);
                        paths.emplace_back(p);
                    }
                }else{
                    ImGui_ImplSDL2_ProcessEvent(&event);
                }
            }

            if(close_window) break;

            if( !paths.empty() ){
                loaded_files.emplace_back(std::async(std::launch::async, load_paths, paths));
            }

        }

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

        const auto display_guide = [&guide_mutex,
                                    &mutex_dt,
                                    &guide_stages,
                                    &guide_stage_num,
                                    &guide_image_textures,
                                    &unregister_guide_textures,
                                    &register_guide_textures,
                                    &reset_guide,
                                    &view_toggles ]() -> void {
            if( !view_toggles.view_guides_enabled ) return;

            // Attempt to acquire an exclusive lock.
            std::unique_lock<std::shared_timed_mutex> guide_lock(guide_mutex, mutex_dt);
            if(!guide_lock) return;

            if( guide_stages.empty() ) return;

            const auto first_stage_num = static_cast<int64_t>(0);
            const auto final_stage_num = static_cast<int64_t>(guide_stages.size() - 1);

            guide_stage_num = std::clamp(guide_stage_num, first_stage_num, final_stage_num);
            const bool is_first_stage = (guide_stage_num == first_stage_num);
            const bool is_final_stage = (guide_stage_num == final_stage_num);

            ImGuiWindowFlags window_flags = 0;
            //window_flags |= ImGuiWindowFlags_NoTitleBar;
            window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
            ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
            const float padding = 32.0f;
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - padding, padding), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowSizeConstraints(ImVec2(400, 50), ImVec2(600, 600));

            ImGui::Begin("Guide", &view_toggles.view_guides_enabled, window_flags);

            const auto next_button_str = std::string(is_final_stage ? "Finish" : "Continue");
            ImGui::BeginDisabled(is_first_stage);
            const bool prev_button = ImGui::Button("Back");
            ImGui::EndDisabled();
            //const bool prev_button = is_first_stage ? false : ImGui::Button("Back");
            //if(!is_first_stage) ImGui::SameLine();
            ImGui::SameLine();
            const bool next_button = ImGui::Button(next_button_str.c_str());

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(20.0f, 0.0f));
            ImGui::SameLine();

            const std::string counter_str = "Step "_s + std::to_string(guide_stage_num + 1)
                                          + " / " + std::to_string(final_stage_num + 1);
            //ImGui::Text("%s", counter_str.c_str());
            ImGui::TextColored(ImVec4(0.0f, 0.5f, 1.0f, 1.0f), "%s", counter_str.c_str());

            ImGui::Text(" ");

            auto stage_it = std::next( std::begin(guide_stages), guide_stage_num );
            ImGui::TextWrapped("%s", stage_it->message.c_str());


            // Only draw images if the texture will still be available during the draw call.
            // This allows us to immediately de-register textures if we need to.
            if( !next_button
            &&  !prev_button
            &&  view_toggles.view_guides_enabled ){
                for(auto &t : guide_image_textures){
                    ImGui::Text(" ");
                    auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(t.texture_number));
                    ImGui::Image(gl_tex_ptr,
                                 ImVec2(static_cast<float>(t.col_count), static_cast<float>(t.row_count)),
                                 ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f) );
                }
            }

            ImGui::End();

            if(!view_toggles.view_guides_enabled){
                reset_guide();

            }else if( is_final_stage
                  &&  next_button ){
                reset_guide();

            }else if(next_button){
                ++guide_stage_num;
                unregister_guide_textures();
                register_guide_textures();

            }else if(prev_button){
                --guide_stage_num;
                unregister_guide_textures();
                register_guide_textures();

            }

            return;
        };
        try{
            display_guide();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_guide(): '" << e.what() << "'");
            throw;
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
            YLOGWARN("Exception in display_parameter_table(): '" << e.what() << "'");
            throw;
        }

        const auto display_lexicon_customizer = [&drover_mutex,
                                                 &mutex_dt,
                                                 &DICOM_data,
                                                 &lexicon_overrides,
                                                 &lexicon_override_buffer,
                                                 &lexicon_exact_match_colour,
                                                 &FilenameLex,
                                                 &X,
                                                 &wq,
                                                 &view_toggles ]() -> void {
            if( !view_toggles.view_lexicon_customizer ) return;

            // Attempt to acquire an exclusive lock.
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            // Extract contour labels.
            DICOM_data.Ensure_Contour_Data_Allocated();

            metadata_multimap_t roiname_translations;
            const auto cc_all = All_CCs( DICOM_data );
            for(const auto &cc_refw : cc_all){
                for(const auto &c : cc_refw.get().contours){
                    const auto n_opt  = get_as<std::string>(c.metadata, "ROIName");
                    const auto nn_opt = get_as<std::string>(c.metadata, "NormalizedROIName");

                    metadata_map_t l_m;
                    if(n_opt && nn_opt){
                        l_m[n_opt.value()] = nn_opt.value();
                    }
                    combine_distinct(roiname_translations, l_m);
                }
            }

            // Extract lexicon values.
            const auto lex_kv = X.lexicon;
            std::set<std::string> cleans;
            for(const auto &kv : lex_kv){
                cleans.insert(kv.second);
            }

            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
            ImGui::Begin("Lexicon Customizer", &view_toggles.view_lexicon_customizer);
            const float sep = 200.0f;

            ImGui::Text("Lexicon Filename:");
            ImGui::SameLine(sep*1.0f);
            ImGui::Text("%s", FilenameLex.c_str());

            ImGui::Text("Number of lexicon entries:");
            ImGui::SameLine(sep*1.0f);
            ImGui::Text("%s", std::to_string(cleans.size()).c_str());

            ImGui::Text("Number of contour ROIs:");
            ImGui::SameLine(sep*1.0f);
            ImGui::Text("%s", std::to_string(cc_all.size()).c_str());

            ImGui::Dummy(ImVec2(sep, 20));

            ImGui::Separator();
            ImGui::Text("ROI Name");
            ImGui::SameLine(sep*1.0f);
            ImGui::Text("Translation");
            ImGui::SameLine(sep*2.0f);
            ImGui::Text("Override");
            ImGui::Separator();

            bool an_exact_match_is_overridden = false;
            metadata_map_t current;
            metadata_map_t nonexact_and_overrides;
            int row_number = 0;
            for(const auto &kv : roiname_translations){
                const auto dirty = kv.first;
                const auto predicted = X(dirty);
                auto predictions = X.Get_Last_Results();
                if(predictions == nullptr){
                    continue;
                }
                current[dirty] = predicted;
                const bool is_an_exact_match = (predictions->size() == 1UL);
                if(!is_an_exact_match){
                    nonexact_and_overrides[dirty] = predicted;
                }

                ImGui::Text("%s", kv.first.c_str());
                if( ImGui::IsItemHovered()){
                    // std::unique_ptr<std::map<std::string, float>> Get_Last_Results(void); // Can only be called once per query!
                    using p_t = std::pair<float, std::string>;
                    std::list<p_t> l;
                    for(const auto& l_kv : *predictions){
                        l.emplace_back( l_kv.second, l_kv.first );
                    }
                    l.sort([](const p_t &a, const p_t &b){
                        return a.first > b.first;
                    });

                    ImGui::BeginTooltip();
                    ImGui::Text("Ranking:");
                    for(const auto &p : l){
                        ImGui::Text("%s", std::to_string(p.first).c_str());
                        ImGui::SameLine(sep*1.0f);
                        ImGui::Text("'%s'", p.second.c_str());
                    }
                    ImGui::EndTooltip();
                }

                ImGui::SameLine(sep*1.0f);
                if(is_an_exact_match){
                    ImGui::TextColored(lexicon_exact_match_colour, "%s", predicted.c_str());
                }else{
                    ImGui::Text("%s", predicted.c_str());
                }
                //for(const auto &v : kv.second){
                //    ImGui::SameLine();
                //    ImGui::Text("%s", v.c_str());
                //}

                ImGui::SameLine(sep*2.0f);
                string_to_array(lexicon_override_buffer, lexicon_overrides[dirty]);
                ImGui::PushID(row_number++);
                ImGui::SetNextItemWidth(sep);
                const auto edited = ImGui::InputText("##lexicon_override",
                                                     lexicon_override_buffer.data(),
                                                     lexicon_override_buffer.size() - 1UL);
                if(edited){
                    array_to_string(lexicon_overrides[dirty], lexicon_override_buffer);
                }
                if( !lexicon_overrides[dirty].empty()
                &&  (predicted != lexicon_overrides[dirty]) ){
                    ImGui::SameLine(sep*3.05f);
                    ImGui::Text("*");

                    current[dirty] = lexicon_overrides[dirty];
                    nonexact_and_overrides[dirty] = lexicon_overrides[dirty];

                    if(is_an_exact_match){
                        an_exact_match_is_overridden = true;
                    }
                }
                ImGui::PopID();
            }

            ImGui::Separator();


            ImGui::BeginDisabled(an_exact_match_is_overridden);
            const bool button_append_to_existing = ImGui::Button("Append inexact matches and overrides to existing lexicon");
            const bool button_append_to_existing_is_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
            ImGui::EndDisabled();
            if( an_exact_match_is_overridden
            &&  button_append_to_existing_is_hovered ){
                // Note:
                //  Overriding exact matches should be disallowed, otherwise there will be a duplicate entry in the
                //  lexicon. Currently, later entries override earlier entries, but sorting/shuffling the lexicon will
                //  potentially change the result. Also, support for later entries overriding earlier entries is not
                //  guaranteed by Explicator, and may cause issues when parsing the lexicon.
                ImGui::BeginTooltip();
                ImGui::Text("Appending would overriding an exact match, which will result in am ambiguous lexicon.");
                ImGui::EndTooltip();
            }
            if(button_append_to_existing){
                std::stringstream ss;
                ss << "# Altered " << time_mark().Dump_as_string() << std::endl;
                for(const auto& dc : nonexact_and_overrides){
                    ss << dc.second << " : " << dc.first << std::endl;
                }
                {
                    std::ofstream FO(FilenameLex, std::ios::app | std::ios::binary);
                    if(!FO){
                        YLOGWARN("Unable to append to lexicon file");
                    }else{
                        FO << ss.str();
                        FO.flush();
                    }
                }
                X.ReReadFile();
            }

            const bool button_create_new_lexicon = ImGui::Button("Export the present translation to a new lexicon");
            if( ImGui::IsItemHovered() ){
                ImGui::BeginTooltip();
                ImGui::Text("Note: the lexicon will comprise only the current translation, including exact, inexact matches, and overrides.");
                ImGui::EndTooltip();
            }
            if(button_create_new_lexicon){
                const auto worker = [current](){

                    // Prompt for filename.
                    std::optional<select_filename> selector_opt;
                    if(!selector_opt){
                        selector_opt.emplace("Select lexicon filename"_s);
                        //                   open_file_root.string(),
                        //                   std::vector<std::string>{ "DICOM Files"_s, "*.dcm *.DCM"_s,
                        //                                             "All Files"_s, "*"_s } );
                    }

                    // Wait for the user to provide input.
                    //
                    // Note: the following blocks by continuous polling.
                    const auto filename = selector_opt.value().get_selection();
                    selector_opt.reset();

                    // Write the new lexicon.
                    std::ofstream FO(filename, std::ios::app | std::ios::binary);
                    if(!FO){
                        YLOGWARN("Unable to open file for lexicon export");
                    }else{
                        FO << "# Created " << time_mark().Dump_as_string() << std::endl;
                        for(const auto& dc : current){
                            FO << dc.second << " : " << dc.first << std::endl;
                        }
                        FO.flush();
                        YLOGINFO("Exported lexicon to file '" << filename << "'");
                    }
                    return;
                };
                wq.submit_task( worker ); // Offload to waiting worker thread.
            }

            ImGui::End();

            ImGui::SetNextWindowSize(ImVec2(300, 600), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(750, 50), ImGuiCond_FirstUseEver);
            ImGui::Begin("Lexicon Names", &view_toggles.view_lexicon_customizer);
            for(const auto &s : cleans){
                ImGui::Text("%s", s.c_str());
            }
            ImGui::End();

            ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(1100, 50), ImGuiCond_FirstUseEver);
            ImGui::Begin("Lexicon Entries", &view_toggles.view_lexicon_customizer);
            for(const auto &kv : lex_kv){
                ImGui::Text("%s", kv.first.c_str());
                ImGui::SameLine(sep*1.0f);
                ImGui::Text("%s", kv.second.c_str());
            }
            ImGui::End();
            return;
        };
        try{
            display_lexicon_customizer();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_lexicon_customizer(): '" << e.what() << "'");
            throw;
        }


        const auto display_ylogs = [&drover_mutex,
                                    &mutex_dt,
                                    &ylogs_mutex,
                                    &ylogs,
                                    &InvocationMetadata,
                                    &display_metadata_table,
                                    &view_toggles ]() -> void {
            if( !view_toggles.view_ylogs ) return;

            const std::lock_guard<std::shared_timed_mutex> ylogs_lock(ylogs_mutex);

            ImGui::SetNextWindowSize(ImVec2(900, 300), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(400, 75), ImGuiCond_FirstUseEver);
            ImGui::Begin("Logs", &view_toggles.view_ylogs);

            const bool clear = ImGui::Button("Clear");
            ImGui::SameLine();
            const bool copy = ImGui::Button("Copy to clipboard");
            if(clear){
                ylogs.clear();
            }
            if(copy){
                ImGui::LogToClipboard();
            }

            ImGui::Separator();
            ImGui::BeginChild("Logs_scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(ylogs.data(), ylogs.data() + ylogs.size());

            if(ImGui::GetScrollMaxY() <= ImGui::GetScrollY()){
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::End();
            return;
        };
        try{
            display_ylogs();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_ylogs(): '" << e.what() << "'");
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
                                           &img_is_rgb,
                                           &use_texture_antialiasing,
                                           &Free_OpenGL_Texture,
                                           &Load_OpenGL_Texture ]() -> void {
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            if( view_toggles.view_images_enabled
            &&  img_valid ){
                img_channel = std::clamp<int64_t>(img_channel, 0, disp_img_it->channels-1);
                Free_OpenGL_Texture(current_texture);
                current_texture = Load_OpenGL_Texture(*disp_img_it, img_channel,
                                                      img_is_rgb, use_texture_antialiasing,
                                                      custom_centre, custom_width);
            }else{
                img_channel = -1;
                img_array_num = -1;
                img_num = -1;
                img_is_rgb = false;
                use_texture_antialiasing = false;
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
                const bool l_img_is_rgb = false;
                const bool l_use_texture_antialiasing = false;
                contouring_texture = Load_OpenGL_Texture( *cimg_it, 0L, l_img_is_rgb, l_use_texture_antialiasing, {}, {} );
                auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(contouring_texture.texture_number));
                ImGui::Image(gl_tex_ptr, ImVec2(600,600), uv_min, uv_max);
                ImGui::End();
            }
        }

        // Display the main menu bar, which should always be visible.
        const auto display_main_menu_bar = [&view_toggles,
                                            &docs_str,
                                            &docs_ops,
                                            &open_file_root,
                                            &loaded_files,
                                            &launch_file_open_dialog,
                                            &contour_enabled,
                                            &contour_hovered,
                                            &launch_contour_preprocessor,
                                            &contouring_img_altered,
                                            &tagged_pos,

                                            &export_as_DICOM,

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
                                            &time_profile,

                                            &guide_mutex,
                                            &mutex_dt,
                                            &guide_stages,
                                            &guide_stage_num,
                                            &guide_image_textures,
                                            &unregister_guide_textures,
                                            &register_guide_textures,
                                            &reset_guide,

                                            &img_features ](void) -> bool {

            // Check for hot keys.
            ImGuiIO &io = ImGui::GetIO();
            const bool hotkey_ctrl_o = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_O);
            const bool hotkey_ctrl_q = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_Q);
            const bool hotkey_ctrl_h = io.KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_H);

            const auto implement_file_open = [&]() -> void {
                loaded_files.emplace_back(std::async(std::launch::async, launch_file_open_dialog, open_file_root));
                return;
            };
            const auto implement_show_help = [&]() -> void {
                view_toggles.set_about_popup = true;
                return;
            };

            if( hotkey_ctrl_o ) implement_file_open();
            if( hotkey_ctrl_q ) return false;
            if( hotkey_ctrl_h ) implement_show_help();

            if(ImGui::BeginMainMenuBar()){
                if(ImGui::BeginMenu("File")){
                    if( ImGui::MenuItem("Open", "ctrl+o") ){
                        implement_file_open();
                    }
                    if( ImGui::IsItemHovered()){
                        ImGui::BeginTooltip();
                        ImGui::Text("Note: your system might support drag-and-drop for files and directories.");
                        ImGui::EndTooltip();
                    }
                    ImGui::Separator();
                    if(ImGui::BeginMenu("Export")){
                        if( ImGui::MenuItem("Images as DICOM CT") ){
                            export_as_DICOM("export images DICOM CT");
                        }
                        if( ImGui::IsItemHovered()){
                            ImGui::BeginTooltip();
                            ImGui::Text("Note: exports the images currently being displayed in the image viewer.");
                            ImGui::EndTooltip();
                        }
                        if( ImGui::MenuItem("Images as DICOM MR") ){
                            export_as_DICOM("export images DICOM MR");
                        }
                        if( ImGui::IsItemHovered()){
                            ImGui::BeginTooltip();
                            ImGui::Text("Note: exports the images currently being displayed in the image viewer.");
                            ImGui::EndTooltip();
                        }
                        if( ImGui::MenuItem("Images as DICOM RTDOSE") ){
                            export_as_DICOM("export images DICOM RTDOSE");
                        }
                        if( ImGui::IsItemHovered()){
                            ImGui::BeginTooltip();
                            ImGui::Text("Note: exports the images currently being displayed in the image viewer.");
                            ImGui::EndTooltip();
                        }
                        ImGui::Separator();
                        if( ImGui::MenuItem("Original images") ){
                            export_as_DICOM("export original images");
                        }
                        if( ImGui::IsItemHovered()){
                            ImGui::BeginTooltip();
                            ImGui::Text("Note: exports the images currently being displayed in the image viewer.");
                            ImGui::EndTooltip();
                        }
                        ImGui::Separator();
                        if( ImGui::MenuItem("Contours as DICOM RTSTRUCT") ){
                            export_as_DICOM("export contours DICOM RTSTRUCT");
                        }
                        if( ImGui::IsItemHovered()){
                            ImGui::BeginTooltip();
                            ImGui::Text("Note: exports all loaded and saved contours. Does not export unsaved contours in contouring mode!");
                            ImGui::EndTooltip();
                        }
                        ImGui::EndMenu();
                    }
                    if( ImGui::IsItemHovered()){
                        ImGui::BeginTooltip();
                        ImGui::Text("Note: file export can be customized using scripts.");
                        ImGui::EndTooltip();
                    }
                    ImGui::Separator();
                    if( ImGui::MenuItem("Exit", "ctrl+q") ){
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
                    if(ImGui::MenuItem("Image Feature Extractor", nullptr, &view_toggles.view_image_feature_extraction)){
                        view_toggles.view_contouring_enabled = false;
                        view_toggles.view_drawing_enabled = false;
                        view_toggles.view_row_column_profiles = false;
                        //view_toggles.view_image_feature_extraction = false;
                        view_toggles.view_time_profiles = false;

                        tagged_pos = {};

                    }
                    ImGui::Separator();
                    if(ImGui::MenuItem("Contouring", nullptr, &view_toggles.view_contouring_enabled)){
                        //view_toggles.view_contouring_enabled = false;
                        view_toggles.view_drawing_enabled = false;
                        view_toggles.view_row_column_profiles = false;
                        view_toggles.view_image_feature_extraction = false;
                        view_toggles.view_time_profiles = false;

                        contouring_img_altered = true;
                        tagged_pos = {};
                    }
                    if(ImGui::MenuItem("Drawing", nullptr, &view_toggles.view_drawing_enabled)){
                        view_toggles.view_contouring_enabled = false;
                        //view_toggles.view_drawing_enabled = false;
                        view_toggles.view_row_column_profiles = false;
                        view_toggles.view_image_feature_extraction = false;
                        view_toggles.view_time_profiles = false;

                        tagged_pos = {};
                    }
                    ImGui::Separator();
                    if(ImGui::MenuItem("Row and Column Profiles", nullptr, &view_toggles.view_row_column_profiles)){
                        view_toggles.view_contouring_enabled = false;
                        view_toggles.view_drawing_enabled = false;
                        //view_toggles.view_row_column_profiles = false;
                        view_toggles.view_image_feature_extraction = false;
                        view_toggles.view_time_profiles = false;

                        row_profile.samples.clear();
                        col_profile.samples.clear();
                        tagged_pos = {};
                    }
                    if(ImGui::MenuItem("Time Profiles", nullptr, &view_toggles.view_time_profiles)){
                        view_toggles.view_contouring_enabled = false;
                        view_toggles.view_drawing_enabled = false;
                        view_toggles.view_row_column_profiles = false;
                        view_toggles.view_image_feature_extraction = false;
                        //view_toggles.view_time_profiles = false;

                        time_profile.samples.clear();
                        tagged_pos = {};
                    }
                    ImGui::Separator();
                    ImGui::MenuItem("Meshes", nullptr, &view_toggles.view_meshes_enabled);
                    ImGui::MenuItem("Point Sets", nullptr, &view_toggles.view_psets_enabled);
                    ImGui::Separator();
                    if(ImGui::MenuItem("Plots", nullptr, &view_toggles.view_plots_enabled)){
                        lsamps_visible.clear();
                    }
                    ImGui::Separator();
                    ImGui::MenuItem("RT Plans", nullptr, &view_toggles.view_rtplans_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Tables", nullptr, &view_toggles.view_tables_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Transforms", nullptr, &view_toggles.view_tforms_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Script Editor", nullptr, &view_toggles.view_script_editor_enabled);
                    ImGui::MenuItem("Script Feedback", nullptr, &view_toggles.view_script_feedback);
                    ImGui::Separator();
                    ImGui::MenuItem("Parameter Table", nullptr, &view_toggles.view_parameter_table);
                    ImGui::MenuItem("Lexicon Customizer", nullptr, &view_toggles.view_lexicon_customizer);
                    ImGui::Separator();
                    ImGui::MenuItem("Guides", nullptr, &view_toggles.view_guides_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Shader Editor", nullptr, &view_toggles.view_shader_editor_enabled);
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Metadata")){
                    ImGui::MenuItem("Image Metadata", nullptr, &view_toggles.view_image_metadata_enabled);
                    ImGui::MenuItem("Image Hover Tooltips", nullptr, &view_toggles.show_image_hover_tooltips);
                    ImGui::Separator();
                    ImGui::MenuItem("Mesh Metadata", nullptr, &view_toggles.view_mesh_metadata_enabled);
                    ImGui::MenuItem("Point Set Metadata", nullptr, &view_toggles.view_psets_metadata_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Plot Hover Metadata", nullptr, &view_toggles.view_plots_metadata);
                    ImGui::Separator();
                    ImGui::MenuItem("RT Plan Metadata", nullptr, &view_toggles.view_rtplan_metadata_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Table Metadata", nullptr, &view_toggles.view_table_metadata_enabled);
                    ImGui::Separator();
                    ImGui::MenuItem("Transform Metadata", nullptr, &view_toggles.view_tforms_metadata_enabled);
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
                    if(ImGui::BeginMenu("Log/Notification Verbosity")){
                        const auto ll_callback = ygor::log_level_to_string(ygor::g_logger.get_callback_min_level());
                        const auto ll_terminal = ygor::log_level_to_string(ygor::g_logger.get_terminal_min_level());
                        const auto ll_callback_str = "Current Tray Notification Level: "_s + ll_callback;
                        const auto ll_terminal_str = "Current Terminal Log Level: "_s + ll_terminal;
                        ImGui::MenuItem(ll_terminal_str.c_str(), nullptr, false, false);
                        ImGui::MenuItem(ll_callback_str.c_str(), nullptr, false, false);
                        ImGui::Separator();
                        if(ImGui::BeginMenu("Increase")){
                            if(ImGui::MenuItem("All Logs/Notifications", nullptr, nullptr)){
                                ygor::g_logger.increase_verbosity();
                            }
                            if(ImGui::MenuItem("Terminal/Console Logs", nullptr, nullptr)){
                                ygor::g_logger.increase_terminal_verbosity();
                            }
                            if(ImGui::MenuItem("Tray Notifications", nullptr, nullptr)){
                                ygor::g_logger.increase_callback_verbosity();
                            }
                            ImGui::EndMenu();
                        }
                        if(ImGui::BeginMenu("Decrease")){
                            if(ImGui::MenuItem("All Logs/Notifications", nullptr, nullptr)){
                                ygor::g_logger.decrease_verbosity();
                            }
                            if(ImGui::MenuItem("Terminal/Console Logs", nullptr, nullptr)){
                                ygor::g_logger.decrease_terminal_verbosity();
                            }
                            if(ImGui::MenuItem("Tray Notifications", nullptr, nullptr)){
                                ygor::g_logger.decrease_callback_verbosity();
                            }
                            ImGui::EndMenu();
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
                        auto known_ops = Known_Operations_and_Aliases();
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

                                auto N_sfs = static_cast<int64_t>(script_files.size());
                                if( N_sfs == 0 ){
                                    YLOGINFO("No script to append to. Creating new script.");
                                    script_files.emplace_back();
                                    script_files.back().altered = true;
                                    append_to_script(script_files.back().content, new_script_content);
                                    script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
                                    active_script_file = N_sfs;
                                    N_sfs = static_cast<int64_t>(script_files.size());
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
                                        auto N_sfs = static_cast<int64_t>(script_files.size());
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
                                        YLOGWARN("Script execution failed");
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

                if( ImGui::BeginMenu("Help") ){
                    if(ImGui::MenuItem("About", "ctrl+h")){
                        implement_show_help();
                    }
                    ImGui::Separator();
                    ImGui::MenuItem("Logs", nullptr, &view_toggles.view_ylogs);
                    ImGui::MenuItem("Metrics", nullptr, &view_toggles.view_metrics_window);
                    ImGui::Separator();

                    if(ImGui::MenuItem("Reference Manual", nullptr, &view_toggles.view_documentation_enabled)){
                        docs_str.clear();
                        std::stringstream ss;
                        Emit_Documentation(ss);
                        docs_str = ss.str() + '\0';
                    }
                    if(ImGui::BeginMenu("Guides")){
                        const auto categories = standard_guide_categories();
                        for(const auto &cat : categories){
                            if(ImGui::BeginMenu(cat.c_str())){
                                for(const auto &sguide : standard_guides_with_category(cat)){
                                    if(ImGui::MenuItem(sguide.name.c_str())){
                                        std::unique_lock<std::shared_timed_mutex> guide_lock(guide_mutex, mutex_dt);
                                        if(!guide_lock) return false;

                                        auto l_guide_stages = parse_guide(sguide.text);
                                        if(!l_guide_stages.empty()){
                                            guide_stages = l_guide_stages;
                                            guide_stage_num = 0;
                                            register_guide_textures();
                                            view_toggles.view_guides_enabled = true;
                                        }
                                    }
                                    //if(ImGui::IsItemHovered()){
                                    //    ImGui::SetNextWindowSizeConstraints(ImVec2(600.0, -1), ImVec2(500.0, -1));
                                    //    ImGui::BeginTooltip();
                                    //    ImGui::TextWrapped("%s", sguide.text.c_str());
                                    //    ImGui::EndTooltip();
                                    //}
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndMenu();
                    }
                    if(ImGui::BeginMenu("Operation Documentation")){
                        auto known_ops = Known_Operations_and_Aliases();
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

                            if(ImGui::MenuItem(op_name.c_str())){
                                std::stringstream ss;
                                Emit_Op_Documentation(op_name, ss);
                                docs_ops[op_name] = ss.str() + '\0';
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

                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
            return true;
        };
        try{
            // Break from the main render loop if false is received.
            if(!display_main_menu_bar()) break;
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_main_menu_bar(): '" << e.what() << "'");
            throw;
        }

        if( view_toggles.view_metrics_window ){
            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::ShowMetricsWindow(&view_toggles.view_metrics_window);
        }

        // Display the full reference manual.
        if( view_toggles.view_documentation_enabled ){
            ImGui::SetNextWindowSize(ImVec2(875, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(150, 150), ImGuiCond_FirstUseEver);
            if(ImGui::Begin("Reference Manual", &view_toggles.view_documentation_enabled )){
                if( !docs_str.empty()
                &&  view_toggles.view_documentation_enabled ){
                    ImGui::TextUnformatted( &(docs_str.front()), &(docs_str.back()) );
                }
            }
            ImGui::End();
        }

        // Display operation-specific documentation windows.
        for(auto docs_ops_it = std::begin(docs_ops); docs_ops_it != std::end(docs_ops); ){
            auto& op_name = docs_ops_it->first;
            auto& op_docs = docs_ops_it->second;
            const auto window_title = "Operation Documentation: "_s + op_name;
            bool window_stays_open = true;


            ImGui::SetNextWindowSize(ImVec2(875, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(150, 150), ImGuiCond_FirstUseEver);
            if(ImGui::Begin(window_title.c_str(), &window_stays_open )){
                if( !op_docs.empty()
                &&  window_stays_open ){
                    ImGui::TextUnformatted( &(op_docs.front()), &(op_docs.back()) );
                }
            }
            ImGui::End();

            if(window_stays_open){
                ++docs_ops_it;
            }else{
                docs_ops_it = docs_ops.erase(docs_ops_it);
            }
        }

        if( view_toggles.view_polyominoes_enabled ){
            if(!polyomino_imgs.Has_Image_Data()){
                polyomino_imgs.Ensure_Contour_Data_Allocated();
                polyomino_imgs.image_data.push_back(std::make_unique<Image_Array>());
                polyomino_imgs.image_data.back()->imagecoll.images.emplace_back();
                auto *img_ptr = &(polyomino_imgs.image_data.back()->imagecoll.images.back());
                img_ptr->init_buffer(20L, 10L, 1L);
                img_ptr->init_spatial(1.0, 1.0, 1.0, zero3, zero3);
                img_ptr->init_orientation(vec3<double>(1.0, 0.0, 0.0), vec3<double>(0.0, 1.0, 0.0));

                img_ptr->metadata["Description"] = "Polyominoes";
                img_ptr->metadata["WindowValidFor"] = "Polyominoes";
                img_ptr->metadata["WindowCenter"] = "0.5";
                img_ptr->metadata["WindowWidth"] = "1.0";

                const bool l_img_is_rgb = false;
                const bool l_use_texture_antialiasing = false;
                polyomino_texture = Load_OpenGL_Texture( *img_ptr, 0L, l_img_is_rgb, l_use_texture_antialiasing, {}, {} );
                t_polyomino_updated = std::chrono::steady_clock::now();
            }
            const auto score = polyomino_imgs.image_data.back()->imagecoll.images.back().GetMetadataValueAs<double>("PolyominoesScore").value_or(0.0);
            const auto speed_multiplier = 50.0; // Speed will have doubled when the score equals this factor.
            const auto speed = (score + speed_multiplier) / speed_multiplier;


            ImGui::SetNextWindowSize(ImVec2(365, 820), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(1000, 50), ImGuiCond_FirstUseEver);
            ImGui::Begin("Polyominoes", &view_toggles.view_polyominoes_enabled, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar ); //| ImGuiWindowFlags_AlwaysAutoResize);
            ImVec2 window_extent = ImGui::GetContentRegionAvail();
            const auto f = ImGui::IsWindowFocused();

            std::string action = "none";
            if( ImGui::Button("Left", ImVec2(window_extent.x/7, 0))
            ||  (f && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_LeftArrow))) ) action = "translate-left";
            ImGui::SameLine();
            if( ImGui::Button("Right", ImVec2(window_extent.x/7, 0))
            ||  (f && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_RightArrow))) ) action = "translate-right";
            ImGui::SameLine();
            if( ImGui::Button("Rot L", ImVec2(window_extent.x/7, 0))
            ||  (f && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_UpArrow))) ) action = "rotate-counter-clockwise";
            ImGui::SameLine();
            if( ImGui::Button("Rot R", ImVec2(window_extent.x/7, 0))
            ||  (f && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Tab))) ) action = "rotate-clockwise";
            ImGui::SameLine();
            if( ImGui::Button("Down", ImVec2(window_extent.x/7, 0))
            ||  (f && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_DownArrow))) ) action = "translate-down";
            ImGui::SameLine();
            if( ImGui::Button("Drop", ImVec2(window_extent.x/7, 0))
            ||  (f && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Space))) ) action = "drop";

            ImGui::SliderInt("Polyomino Family", &polyomino_family, 0, 5);
            polyomino_family = std::clamp<int>(polyomino_family, 0, 5);
            if(ImGui::IsItemHovered()){
                ImGui::SetTooltip("Controls the family or order of new polyominoes, e.g., four for tetrominoes. Zero selects from all available families.");
            }

            ImGui::Checkbox("Pause", &polyomino_paused);
            ImGui::SameLine();
            const auto reset = ImGui::Button("Reset", ImVec2(window_extent.x/6, 0));
            
            ImGui::Text("Current Score: %s, Current Speed: %s%%", std::to_string(static_cast<int64_t>(score)).c_str(),
                                                                  std::to_string(static_cast<int64_t>(100.0 * speed)).c_str());

            // Run a simulation with the given action.
            const auto t_now = std::chrono::steady_clock::now();
            const auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_polyomino_updated).count();
            if(reset){
                Free_OpenGL_Texture(polyomino_texture);
                polyomino_imgs = Drover();
                t_polyomino_updated = t_now;
                polyomino_paused = false;

            }else if( polyomino_paused ){
                // Do not simulate or change the texture.
                t_polyomino_updated = t_now;

            }else if( (action != "none")
                  ||  ( (action == "none") && (dt_polyomino_update <= (t_diff * speed)) ) ){
                t_polyomino_updated = t_now;

                // Loading the script and parsing into an op_list could be cached. Might speed this up slightly...
                std::list<OperationArgPkg> Operations;
                const bool op_load_res = Load_Standard_Script( Operations, "plumbing", "iterate polyominoes" );
                if(!op_load_res){
                    throw std::runtime_error("Unable to load polyominoes script");
                }
                metadata_map_t l_InvocationMetadata;
                l_InvocationMetadata["poly_family"] = std::to_string(polyomino_family);
                l_InvocationMetadata["action"] = action;
                std::string l_FilenameLex;
                const bool res = Operation_Dispatcher(polyomino_imgs, l_InvocationMetadata, l_FilenameLex, Operations);
                if( res 
                &&  polyomino_imgs.Has_Image_Data()){
                    auto *img_ptr = &(polyomino_imgs.image_data.back()->imagecoll.images.back());

                    Free_OpenGL_Texture(polyomino_texture);
                    const bool l_img_is_rgb = false;
                    const bool l_use_texture_antialiasing = false;
                    polyomino_texture = Load_OpenGL_Texture( *img_ptr, 0L, l_img_is_rgb, l_use_texture_antialiasing, {}, {} );
                }else{
                    polyomino_paused = true;
                }
            }

            // Note: we have to render the image last so the texture number is available during rendering.
            ImVec2 image_extent = ImGui::GetContentRegionAvail();
            image_extent.y = std::min<float>(700.0f, image_extent.y - 5.0f);
            image_extent.x = image_extent.y / polyomino_texture.aspect_ratio;
            auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(polyomino_texture.texture_number));
            ImGui::Image(gl_tex_ptr, image_extent);
            ImGui::End();
        }

        if( view_toggles.view_triple_three_enabled ){
            ImGui::SetNextWindowSize(ImVec2(450, 650), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(150, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("Triple-Three", &view_toggles.view_triple_three_enabled, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar );

            ImGui::Checkbox("Hide cards", &tt_hidden);
            if( ImGui::IsItemHovered() ){
                ImGui::BeginTooltip();
                ImGui::Text("Note: the computer player never 'sees' your unused cards.");
                ImGui::EndTooltip();
            }

            const auto curr_score = tt_game.compute_score();
            const bool game_is_complete = tt_game.is_game_complete();

            const auto reset = ImGui::Button("Reset");
            if(reset){
                tt_game.reset();
                const auto t_now = std::chrono::steady_clock::now();
                t_tt_updated = t_now;
                tt_cell_owner.fill(-1);
            }
            ImGui::SameLine();
            
            ImGui::Text("Current score: %s.", std::to_string(curr_score).c_str());
            ImGui::SameLine();
            {
                std::stringstream ss;
                if(game_is_complete){
                    ss << "Game complete.";
                    if(0 < curr_score){
                        ss << " You win!";
                    }else{
                        ss << " Computer wins.";
                    }
                }else{
                    ss << (tt_game.first_players_turn ? "Computer's" : "Your") << " turn.";
                }
                ImGui::Text("%s", ss.str().c_str());
            }
            ImGui::Separator();

            const auto block_dims = ImVec2(80, 110);
            int button_id = 0;

            // Immediately move the next available computer card into the next available space, for now.
            const auto draw_empty_cell = [&](){
                ImGui::PushID(button_id++);
                ImGui::Dummy(block_dims);
                ImGui::PopID();
                return;
            };

            const auto draw_empty_card = [&](int cell_num, bool dark){
                ImGui::PushID(button_id++);
                int styles_overridden = 0;
                if(dark){
                    // Temporarily scale down the alpha component to make these appear darker.
                    const auto& styles = ImGui::GetStyle();

                    auto button_colour = styles.Colors[ImGuiCol_Button];
                    button_colour.w *= 0.3;
                    ImGui::PushStyleColor(ImGuiCol_Button, button_colour);
                    ++styles_overridden;

                    auto button_hovered_colour = styles.Colors[ImGuiCol_ButtonHovered];
                    button_hovered_colour.w *= 0.3;
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_hovered_colour);
                    ++styles_overridden;

                    auto button_active_colour = styles.Colors[ImGuiCol_ButtonActive];
                    button_active_colour.w *= 0.3;
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_active_colour);
                    ++styles_overridden;
                }
                ImGui::Button("", block_dims);

                // Accept a card dragged here.
                if( !tt_game.first_players_turn
                &&  tt_game.is_valid_cell_num(cell_num)
                &&  !tt_game.cell_holds_valid_card(cell_num)
                &&  ImGui::BeginDragDropTarget() ){

                    if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("tt_card_number"); payload != nullptr){
                        if(payload->DataSize != sizeof(int64_t)){
                            throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
                        }
                        const int64_t card_num = *static_cast<const int64_t*>(payload->Data);
                        tt_game.move_card(card_num, cell_num);

                        const auto t_now = std::chrono::steady_clock::now();
                        t_tt_updated = t_now;
                    }
                    ImGui::EndDragDropTarget();

                }

                if(dark){
                    ImGui::PopStyleColor(styles_overridden);
                }
                ImGui::PopID();
                return;
            };

            const auto draw_card = [&]( int64_t cell_num,
                                        int64_t card_index,
                                        bool obscure_stats ){
                const auto t_now = std::chrono::steady_clock::now();
                const tt_card_t &card = tt_game.get_card(card_index);

                // Determine the colour blend for animations.
                float colour_blend = 0.0; // From computer to user card colours.
                if(tt_game.is_valid_cell_num(cell_num)){
                    auto &card_owner = tt_cell_owner.at(cell_num);
                    auto &card_time = tt_cell_owner_time.at(cell_num);
                    if(card_owner == -1){
                        card_time = t_now - std::chrono::hours(1);
                    }else if( ( (card_owner == 0) && !card.owned_by_first_player )
                          ||  ( (card_owner == 1) && card.owned_by_first_player ) ){
                        card_time = t_now;
                    }
                    card_owner = (card.owned_by_first_player ? 0 : 1);

                    const auto t_diff = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(t_now - card_time).count());
                    const auto dt = std::clamp<float>(t_diff, 0.0f, tt_anim_dt) / tt_anim_dt;
                    colour_blend = ((card_owner == 0) ? 1.0 - dt : dt);
                }else{
                    colour_blend = ( card.owned_by_first_player ? 0.0f : 1.0f );
                }
                const auto user_colour = ImColor(0.1f, 0.4f, 0.8f, 1.0f).Value;
                const auto comp_colour = ImColor(0.8f, 0.4f, 0.1f, 1.0f).Value;
                ImVec4 card_colour;
                card_colour.x = std::clamp<float>(comp_colour.x + (user_colour.x - comp_colour.x) * colour_blend, 0.0f, 1.0f);
                card_colour.y = std::clamp<float>(comp_colour.y + (user_colour.y - comp_colour.y) * colour_blend, 0.0f, 1.0f);
                card_colour.z = std::clamp<float>(comp_colour.z + (user_colour.z - comp_colour.z) * colour_blend, 0.0f, 1.0f);
                card_colour.w = std::clamp<float>(comp_colour.w + (user_colour.w - comp_colour.w) * colour_blend, 0.0f, 1.0f);


                ImGui::PushID(button_id++);

                // Draw the card.
                const auto pos_prior = ImGui::GetCursorPos();

                ImGui::PushStyleColor(ImGuiCol_Button, card_colour);

                //ImGui::Dummy(block_dims);
                ImGui::Button("", block_dims);

                // Make the card draggable.
                if( !tt_game.first_players_turn
                &&  (!card.used)
                &&  (!card.owned_by_first_player)
                &&  ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)){
                    ImGui::SetDragDropPayload("tt_card_number", static_cast<void*>(&card_index), sizeof(int64_t));

                    // Show a preview of the card being dragged.
                    ImGui::Text("Card");

                    ImGui::EndDragDropSource();
                }
                ImGui::PopStyleColor(1);

                const auto pos_after = ImGui::GetCursorPos();

                // Draw a text overlay showing card information.
                ImGui::SetCursorPos(pos_prior);
                std::stringstream ss;
                if(obscure_stats){
                    ss << " ? \n"
                       << "? ?\n"
                       << " ? \n";
                }else{
                    ss << " " << card.stat_up << " " << "\n"
                       << card.stat_left << " " << card.stat_right << "\n"
                       << " " << card.stat_down << " " << "\n";
                }
                ss << "   " << (card.owned_by_first_player ? "C" : "U");
                ImGui::Text("%s", const_cast<char *>(ss.str().c_str()));
                ImGui::SetCursorPos(pos_prior);
                ImGui::Dummy(block_dims);

                ImGui::PopID();
                return;
            };

            
            // Perform the computer's move.
            const auto t_now = std::chrono::steady_clock::now();
            const auto t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_tt_updated).count();
            if( !game_is_complete
            &&  tt_game.first_players_turn
            &&  (dt_tt_update < t_diff) ){
                tt_game.auto_move_card();
                t_tt_updated = t_now;
            }

            // Display the cards on a 5x5 grid. The first column are the computer's hand, the middle 3x3 is the game
            // board, and the last column are the user's hand. The 3 middle cells along the top and bottom are not used.
            for(int64_t row = 0; row < 5; ++row){
                for(int64_t col = 0; col < 5; ++col){

                    // Cards held by the computer.
                    if(col == 0){
                        const auto card_index = row;
                        const auto &card = tt_game.get_card(card_index);
                        if(card.used){
                            const bool dark = true;
                            draw_empty_card(-1, dark);
                        }else{
                            const int64_t cell_num = -1;
                            draw_card(cell_num, card_index, tt_hidden);
                        }

                    // Cards held by the user.
                    }else if(col == 4){
                        const auto card_index = row + 5;
                        const auto &card = tt_game.get_card(card_index);
                        if(card.used){
                            const bool dark = true;
                            draw_empty_card(-1, dark);
                        }else{
                            const bool obscure_card = false;
                            const int64_t cell_num = -1;
                            draw_card(cell_num, card_index, obscure_card);
                        }

                    // Main board / cards already in-play.
                    }else{
                        if( (row == 0)
                        ||  (row == 4) ){
                            draw_empty_cell();
                        }else{
                            const auto cell_num = tt_game.get_cell_num(row - 1, col - 1);
                            const auto card_num = tt_game.board.at(cell_num);
                            if(tt_game.is_valid_card_num(card_num)){
                                const bool obscure_card = false;
                                draw_card(cell_num, card_num, obscure_card);
                            }else{
                                const bool dark = false;
                                draw_empty_card(cell_num, dark);
                            }
                        }
                    }

                    if(col != 4){
                        ImGui::SameLine();
                    }
                }
            }

            ImGui::End();
        }

        if( view_toggles.view_encompass_enabled ){
            // Reset the game before any game state is used.
            if( ImGui::IsKeyPressed(SDL_SCANCODE_R) ){
                reset_en_game();
            }

            const auto pi = std::acos(-1.0);

            const auto win_width  = static_cast<int>( std::ceil(en_game.box_width) ) + 15;
            const auto win_height = static_cast<int>( std::ceil(en_game.box_height) ) + 40;
            auto flags = ImGuiWindowFlags_AlwaysAutoResize
                       | ImGuiWindowFlags_NoScrollWithMouse
                       | ImGuiWindowFlags_NoNavInputs
                       | ImGuiWindowFlags_NoScrollbar ;
            ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
            ImGui::Begin("Encompass", &view_toggles.view_encompass_enabled, flags );

            //const auto intersects_horiz_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
            //    return (pos.y <= (0.0 + rad))
            //        || ((en_game.box_height - rad) <= pos.y);
            //};
            //const auto intersects_vert_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
            //    return (pos.x <= (0.0 + rad))
            //        || ((en_game.box_width - rad) <= pos.x);
            //};

            const auto rad_to_area = [&](double rad){
                return pi * std::pow(rad, 2.0);
            };

            const auto intersects_existing = [&]( const vec2<double> &pos, double rad ) -> bool {
                bool intersection = false;
                for(const auto& obj : en_game_objs){
                    const auto sep = pos.distance(obj.pos);
                    const auto min = rad + obj.rad;
                    if(sep <= min){
                        intersection = true;
                        break;
                    }
                }
                return intersection;
            };

            const auto intersects_wall = [&]( const vec2<double> &pos, double rad ) -> bool {
                return (pos.x <= (0.0 + rad))
                    || ((en_game.box_width - rad) <= pos.x)
                    || (pos.y <= (0.0 + rad))
                    || ((en_game.box_height - rad) <= pos.y);
            };

            const auto obj_intersections = [&](size_t j){
                std::vector<size_t> ints;
                const auto& obj_j = en_game_objs[j];
                for(size_t i = 0UL; i < j; ++i){
                    const auto& obj_i = en_game_objs[i];
                    const auto sep = obj_j.pos.distance( obj_i.pos );
                    const auto min = obj_j.rad + obj_i.rad;
                    if(sep <= min){
                        ints.emplace_back(i);
                    }
                }
                return ints;
            };
                                    
            const auto attempt_to_shed = [&]( en_game_obj_t &obj, 
                                              const vec2<double> &dir,
                                              double radius,
                                              decltype(en_game_objs) &l_en_game_objs ) -> bool {
                bool shed_successfully = false;
                do{
                    const auto l_dir = dir.unit();
                    const auto l_rad = radius;

                    const auto surplus_sq_rad = std::pow(obj.rad, 2.0) - std::pow(l_rad, 2.0);
                    if(surplus_sq_rad <= std::pow(en_game.min_radius, 2.0)) break;

                    const auto surplus_rad = std::sqrt( surplus_sq_rad );
                    if(surplus_rad < en_game.min_radius) break;
                    const auto l_obj_remaining_rad = surplus_rad;

                    // Should ideally do this, but then it will usually collide with the not-yet-shrunk 'obj'.
                    // So this will need support from the collision check.
                    //const auto l_pos = obj.pos + l_dir * (l_obj_remaining_rad + l_rad + 1.0);
                    // instead, we just use the existing not-yet-shrunk radius.
                    const auto l_pos = obj.pos + l_dir * (obj.rad + l_rad + 1.0);
                    const auto l_vel = l_dir * en_game.max_speed;

                    if( !intersects_wall(l_pos, l_rad)
                    &&  !intersects_existing(l_pos, l_rad) ){

                        l_en_game_objs.emplace_back();
                        l_en_game_objs.back().pos = l_pos;
                        l_en_game_objs.back().vel = l_vel;
                        l_en_game_objs.back().rad = l_rad;
                        l_en_game_objs.back().player_controlled = false;

                        const auto orig_area = pi * std::pow(obj.rad, 2.0);
                        const auto new_area  = pi * std::pow(l_obj_remaining_rad, 2.0);
                        const auto orig_area_shed = 0.0;
                        const auto new_area_shed  = pi * std::pow(l_rad, 2.0);

                        //const auto d_area_i = pi * (std::pow(new_i_rad, 2.0) - std::pow(obj_i.rad, 2.0));
                        //const auto d_area_j = pi * (std::pow(new_j_rad, 2.0) - std::pow(obj_j.rad, 2.0));
                        //std::cout << "Radius transfer: " << (new_i_rad - obj_i.rad) << " vs " << (new_j_rad - obj_j.rad) << std::endl;
                        //std::cout << "  (Transfer of mass: " << d_area_i << " vs " << d_area_j << ")" << std::endl;

                        obj.vel = (obj.vel * orig_area - l_vel * new_area_shed) / (orig_area - new_area_shed);
                        obj.rad = l_obj_remaining_rad;
                        shed_successfully = true;
                    }
                }while(false);
                return shed_successfully;
            };

            // Display.
            ImVec2 curr_pos = ImGui::GetCursorScreenPos();
            //ImVec2 window_extent = ImGui::GetContentRegionAvail();
            ImDrawList *window_draw_list = ImGui::GetWindowDrawList();
            const auto f = ImGui::IsWindowFocused();

            {
                const auto c = ImColor(0.7f, 0.7f, 0.8f, 1.0f);
                window_draw_list->AddRect(curr_pos, ImVec2( curr_pos.x + en_game.box_width, 
                                                            curr_pos.y + en_game.box_height ), c);
            }

            const auto t_now = std::chrono::steady_clock::now();
            const auto t_started_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_en_started).count();
            auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_en_updated).count();

            // Limit individual time steps to around 30 fps otherwise 'infinitesimal' updates to the system will no
            // longer be small, and the simulation will quickly break down.
            //
            // Note that this will cause the simulation to be choppy if the frame rate falls below 30 fps or so.
            if(30 < t_updated_diff) t_updated_diff = 30;

            decltype(en_game_objs) l_en_game_objs;
            for(auto &obj : en_game_objs){
                ImVec2 obj_pos = curr_pos;
                obj_pos.x = curr_pos.x + obj.pos.x;
                obj_pos.y = curr_pos.y + obj.pos.y;

                const auto rel_r = std::clamp<double>(obj.rad / 30.0, 0.0, 1.0);
                auto c = ImColor(rel_r * 1.0f, (1.0f - rel_r) * 1.0f, 0.5f, 1.0f);
                if(obj.player_controlled){
                    c = ImColor(1.0f, 1.0f, 0.1f, 1.0f);
                }
                window_draw_list->AddCircle(obj_pos, obj.rad, c);

                // Implement player controls.
                if( f && obj.player_controlled){
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) ){
                        obj.vel.x -= 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_RightArrow)) ){
                        obj.vel.x += 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_UpArrow)) ){
                        obj.vel.y -= 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_DownArrow)) ){
                        obj.vel.y += 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Space)) ){
                        // Jettison a small object in the direction opposite of travel.
                        auto l_rad = obj.rad * 0.05;
                        l_rad = (l_rad < en_game.min_radius) ? en_game.min_radius : l_rad;
                        auto l_dir = obj.vel * (-1.0);
                        l_dir = ( 0.0 < l_dir.length() ) ? l_dir : vec2<double>(1.0, 0.0);

                        attempt_to_shed( obj, l_dir, l_rad, l_en_game_objs);
                    }
                    if( ImGui::IsKeyPressed(SDL_SCANCODE_S) ){
                        // Attempt to split into two.
                        auto l_rad = std::sqrt(0.5) * obj.rad;
                        auto l_dir = obj.vel * (-1.0);
                        l_dir = ( 0.0 < l_dir.length() ) ? l_dir : vec2<double>(1.0, 0.0);

                        decltype(en_game_objs) ll_en_game_objs;
                        attempt_to_shed( obj, l_dir, l_rad, ll_en_game_objs);
                        for(auto& obj : ll_en_game_objs){
                            obj.player_controlled = true;
                        }
                        l_en_game_objs.insert( std::begin(l_en_game_objs),
                                               std::begin(ll_en_game_objs), std::end(ll_en_game_objs) );
                    }
                }

                // Limit the maximum speed.
                {
                    const auto speed = obj.vel.length();
                    if(en_game.max_speed < speed){
                        const auto dir = obj.vel.unit();
                        obj.vel = dir * en_game.max_speed;
                    }
                }
            }
            en_game_objs.insert( std::begin(en_game_objs),
                                 std::begin(l_en_game_objs), std::end(l_en_game_objs) );
            l_en_game_objs.clear();

            // Sort so larger objects are first.
            std::sort( std::begin(en_game_objs), std::end(en_game_objs),
                       [](const en_game_obj_t &l, const en_game_obj_t &r) -> bool {
                           return (l.rad > r.rad);
                       } );

            std::vector< vec2<double> > transfer_events;

            // Update the system.
            const auto N_objs = en_game_objs.size();
            for(size_t i = 0UL; i < N_objs; ++i){
                auto &obj_i = en_game_objs[i];
                bool should_move_to_cand_pos = true;
                const auto cand_pos = obj_i.pos + obj_i.vel * (static_cast<double>(t_updated_diff) / 1000.0);

                // Check for intersections with the wall.
                const bool cand_int_l_wall = (obj_i.pos.x <= (0.0 + obj_i.rad));
                const bool cand_int_r_wall = ((en_game.box_width - obj_i.rad) <= obj_i.pos.x);
                const bool cand_int_b_wall = (obj_i.pos.y <= (0.0 + obj_i.rad));
                const bool cand_int_t_wall = ((en_game.box_height - obj_i.rad) <= obj_i.pos.y);

                if(cand_int_l_wall) obj_i.vel.x =  std::abs(obj_i.vel.x);
                if(cand_int_r_wall) obj_i.vel.x = -std::abs(obj_i.vel.x);
                if(cand_int_b_wall) obj_i.vel.y =  std::abs(obj_i.vel.y);
                if(cand_int_t_wall) obj_i.vel.y = -std::abs(obj_i.vel.y);


                // Check for intersections with any of the other objects with updated positions.
                //
                // If none, then simulate spontaneous single-object events.
                const auto cand_int_objs  = obj_intersections(i);
                if(cand_int_objs.empty()){

                    // Make large objects slowly disintegrate, 'leaking' a small amount of area in a mutiny event.
                    //
                    // Leaking is a spontaneous event with an associated probabality. The occurrence and amount of mass
                    // lost are proportional to object's current area.
                    //
                    // Since this will be evaluated each frame, we need to scale the likelihood of each individual
                    // evaluation so that the joint likelihood is as expected.

                    //const auto period = 100.0; //milliseconds.
                    const auto period = en_game.mutiny_period;
                    const auto time_slice = static_cast<double>(t_updated_diff);
                    std::uniform_real_distribution<> rd_t(0.0, period);
                    const bool time_slice_selected = (rd_t(en_game.re) <= time_slice);

                    const auto x = rad_to_area(obj_i.rad);
                    //const auto mid = rad_to_area(20.0 * en_game.min_radius);
                    //const auto slope = 1.0 / rad_to_area(5.0 * en_game.min_radius);
                    const auto mid = rad_to_area(en_game.mutiny_mid);
                    const auto slope = 1.0 / rad_to_area(en_game.mutiny_slope);
                    const auto asympt_true = 1.0 / (1.0 + std::exp(-slope * (x - mid))); // logistic function = soft.
                    std::bernoulli_distribution bd(asympt_true);
                    const bool spontaneously_activated = bd(en_game.re);

                    const bool try_shed = time_slice_selected && spontaneously_activated;

                    if( ((5.0 * en_game.min_radius) < obj_i.rad)
                    &&  try_shed ){
                        const auto l_dir = vec2<double>(1.0, 0.0).rotate_around_z( rd_t(en_game.re) );
                        auto l_rad = obj_i.rad * 0.05;
                        l_rad = (l_rad < en_game.min_radius) ? en_game.min_radius : l_rad;

                        const bool shed_successfully = attempt_to_shed( obj_i, l_dir, l_rad, l_en_game_objs);
                        should_move_to_cand_pos = !shed_successfully;
                    }

                // If one or more interesctions are expected, implement mass transfer or scatter or something.
                //
                // Because larger objects are first, object intersections here cause the 'i'th object
                // to transfer mass to the larger object.
                }else{
                    for(auto &j : cand_int_objs){
                        auto& obj_j = en_game_objs[j];
                        const auto sep = obj_j.pos.distance( obj_i.pos );
                        const auto min = obj_j.rad + obj_i.rad;
                        if( (sep < min) 
                        &&  (obj_i.rad <= obj_j.rad) ){
                            // Attempt to consume enough radius so the objects are no longer overlapping.
                            double new_i_rad = obj_i.rad - (min - sep);
                            new_i_rad = std::clamp(new_i_rad, 0.0, 1.0E6);

                            // If the smaller would end up below the minimum radius, then consume the entire object.
                            if(new_i_rad < en_game.min_radius) new_i_rad = 0.0;

                            // Transfer the area to the larger object.
                            double new_j_rad = std::sqrt( std::pow(obj_j.rad, 2.0)
                                                        + std::pow(obj_i.rad, 2.0)
                                                        - std::pow(new_i_rad, 2.0) );
                            // If the larger object will grow beyond the bounds, reduce the amount transferred.
                            const auto max_new_j_rad_wall = std::max<double>( obj_j.rad, 
                                                               std::min<double>({ obj_j.pos.x,
                                                                                  obj_j.pos.y,
                                                                                  en_game.box_width - obj_j.pos.x,
                                                                                  en_game.box_height - obj_j.pos.y }) );
                            // Also determine whether expansion is limited by another nearby (larger) object.
                            auto max_new_j_rad_obj = new_j_rad;
                            for(size_t k = 0UL; k < j; ++k){
                                const auto& obj_k = en_game_objs[k];
                                const auto sep = obj_j.pos.distance( obj_k.pos );
                                const auto surplus = sep - obj_j.rad;
                                if(max_new_j_rad_obj < surplus) max_new_j_rad_obj = surplus;
                            }

                            const bool growth_constrained =    (max_new_j_rad_wall < new_j_rad)
                                                            || (max_new_j_rad_obj < new_j_rad);
                            if(growth_constrained){
                                should_move_to_cand_pos = false;

                                // Instead of kinematics, try 'shedding' the excess mass where it can be placed randomly.
                                // You can make relatively small objects for this to increase the likelihood of
                                // successful placement.
                                const auto can_shed = ((sqrt(2.0) * en_game.min_radius) < obj_j.rad);
                                if(can_shed){
                                    std::uniform_real_distribution<> rd_t(0.0, pi*2.0);
                                    {
                                        int64_t iter = 100L;
                                        while(true){
                                            const auto l_dir = vec2<double>(1.0, 0.0).rotate_around_z( rd_t(en_game.re) );
                                            const auto l_rad = en_game.min_radius;
                                            const bool shed_successfully = attempt_to_shed( obj_j, l_dir, l_rad, l_en_game_objs);

                                            if( shed_successfully ){
                                                break;
                                            }
                                            if(--iter < 0L){
                                                //YLOGWARN("Unable to place shed object after 100 attempts. Ignoring object");
                                                break;
                                            }
                                        }
                                    }
                                    
                                    // Make the object halt.
                                    obj_j.vel = vec2<double>(0.0, 0.0);
                                }



/*
                                // Make the objects bounce apart using 2D hard sphere elastic kinematics.
                                const auto rel_pos = obj_i.pos - obj_j.pos;
                                const auto rel_vel = obj_i.vel - obj_j.vel;
                                const auto inv_mass_i = 1.0; //(1.0 / (pi * std::pow(obj_i.rad, 2.0)));
                                const auto inv_mass_j = 1.0; //(1.0 / (pi * std::pow(obj_j.rad, 2.0)));
                                
                                const auto t_a = rel_pos.Dot(rel_vel);
                                const auto t_c = rel_vel.Dot(rel_vel);
                                const auto t_d = rel_pos.Dot(rel_pos);

                                //const auto t_e = t_b - (rel_pos.Dot(rel_pos) - std::pow(obj_i.rad + obj_j.rad, 2.0));
                                const auto t_e = std::max<double>(0.0, t_a * t_a - t_c * (t_d - std::pow(obj_i.rad + obj_j.rad, 2.0)));
                                //if(t_e < 0.0) continue;
                                auto t = -(t_a + std::sqrt(t_e))/t_c;
                                t = (std::isfinite(t) && (0.0 < t)) ? t : 0.0;
                                
                                const auto impulse_dir = (rel_pos + rel_vel * t).unit();
                                const auto impulse = impulse_dir * 2.0 * impulse_dir.Dot(rel_vel) / (inv_mass_i + inv_mass_j);
                                const auto new_obj_i_vel = obj_i.vel + impulse * inv_mass_i;
                                const auto new_obj_j_vel = obj_j.vel - impulse * inv_mass_j;

//std::cout << "Collision detected between " << obj_i.pos << " and " << obj_j.pos
//          << " with velocities changing from " << obj_i.vel << " to " << new_obj_vel
//          << " and " << obj_j.vel << " to " << new_obj_j_vel << std::endl;

                                obj_i.vel = new_obj_i_vel;
                                obj_j.vel = new_obj_j_vel;
*/
                            }else{
                                const auto dir = (obj_j.pos - obj_i.pos).unit();
                                transfer_events.emplace_back( obj_i.pos + dir * obj_i.rad );

                                const auto orig_area_i = pi * std::pow(obj_i.rad, 2.0);
                                const auto new_area_i  = pi * std::pow(new_i_rad, 2.0);
                                const auto orig_area_j = pi * std::pow(obj_j.rad, 2.0);
                                const auto new_area_j  = pi * std::pow(new_j_rad, 2.0);

                                const auto d_area_i = pi * (std::pow(new_i_rad, 2.0) - std::pow(obj_i.rad, 2.0));
                                const auto d_area_j = pi * (std::pow(new_j_rad, 2.0) - std::pow(obj_j.rad, 2.0));
                                //std::cout << "Radius transfer: " << (new_i_rad - obj_i.rad) << " vs " << (new_j_rad - obj_j.rad) << std::endl;
                                //std::cout << "  (Transfer of mass: " << d_area_i << " vs " << d_area_j << ")" << std::endl;

                                obj_i.rad = new_i_rad;
                                obj_j.rad = new_j_rad;

                                obj_j.vel = (obj_j.vel * orig_area_j + obj_i.vel * d_area_j) / (orig_area_j + d_area_j);
                            }
                        }
                    }
                }
                    

/*
                    // Move this object, and adjust the other object(s) velocities.
                    for(auto &j : cand_int_objs){
                        auto& obj_j = en_game_objs[j];

                        // 2D hard sphere elastic kinematics.
                        const auto rel_pos = obj.pos - obj_j.pos;
                        const auto rel_vel = obj.vel - obj_j.vel;
                        const auto inv_mass_a = (1.0 / (pi * std::pow(obj.rad, 2.0)));
                        const auto inv_mass_b = (1.0 / (pi * std::pow(obj_j.rad, 2.0)));
                        
                        const auto t_a = rel_pos.Dot(rel_vel);
                        const auto t_c = rel_vel.Dot(rel_vel);
                        const auto t_d = rel_pos.Dot(rel_pos);

                        //const auto t_e = t_b - (rel_pos.Dot(rel_pos) - std::pow(obj.rad + obj_j.rad, 2.0));
                        const auto t_e = t_a * t_a - t_c * (t_d - std::pow(obj.rad + obj_j.rad, 2.0));
                        if(t_e < 0.0) continue;
                        const auto t = -(t_a + std::sqrt(t_e))/t_c;
                        if( !std::isfinite(t)
                        ||  (t < 0.0) ) continue;
                        
                        const auto impulse_dir = (rel_pos + rel_vel * t).unit();
                        const auto impulse = impulse_dir * 2.0 * impulse_dir.Dot(rel_vel) / (inv_mass_a + inv_mass_b);
                        const auto new_obj_vel = obj.vel + impulse * inv_mass_a;
                        const auto new_obj_j_vel = obj_j.vel - impulse * inv_mass_b;

std::cout << "Collision detected between " << obj.pos << " and " << obj_j.pos
          << " with velocities changing from " << obj.vel << " to " << new_obj_vel
          << " and " << obj_j.vel << " to " << new_obj_j_vel << std::endl;

                        obj.vel = new_obj_vel;
                        obj_j.vel = new_obj_j_vel;
                    }
*/

                // Move to candidate position.
                if(should_move_to_cand_pos){
                    obj_i.pos = cand_pos;
                }

                // Slowly move toward smaller objects and away from larger objects.
                if(!obj_i.player_controlled){
                    const auto max_dist_between = std::hypot(en_game.box_width, en_game.box_height);
                    const auto time_scale = static_cast<double>(t_updated_diff) / 5000.0;
                    struct nudge_t {
                        double intensity = 0.0;
                        double repulsion_factor = 0.0;
                        vec2<double> dir = vec2<double>(0.0, 0.0);
                    };
                    size_t N_nudges = 3;
                    std::vector<nudge_t> nudges;

                    for(size_t j = 0UL; j < N_objs; ++j){
                        if(i == j) continue;
                        const auto &obj_j = en_game_objs[j];

                        const auto repulsion_factor = (obj_j.rad < obj_i.rad) ? 1.0 : -1.0;

                        const auto rel_pos = (obj_j.pos - obj_i.pos);
                        auto dir = rel_pos.unit();
                        const auto dist_between = rel_pos.length() - obj_i.rad - obj_j.rad;
                        const auto intensity_dist = std::pow( (max_dist_between - dist_between) / max_dist_between, 2.0 );

                        // If 'i' is larger, prefer larger 'prey' even if they are slightly further away.
                        // If 'j' is larger, run away from the closest object large enough to encompass you.
                        const bool is_prey = (obj_i.rad < obj_j.rad);
                        const auto intensity_mass = is_prey ? 1.0
                                                            : std::pow( obj_j.rad / obj_i.rad, 1.5 );
                        if(is_prey){
                            dir = dir.rotate_around_z( pi*0.15 );
                        }
                        const auto intensity = intensity_dist * intensity_mass;

                        nudges.emplace_back();
                        nudges.back().intensity = intensity;
                        nudges.back().repulsion_factor = repulsion_factor;
                        nudges.back().dir = dir;

                        std::sort( std::begin(nudges), std::end(nudges),
                                   [](const nudge_t &l, const nudge_t &r) -> bool {
                                       return std::abs(l.intensity) > std::abs(r.intensity);
                                   } );

                        if(N_nudges < nudges.size()) nudges.resize(N_nudges);
                    }
                    for(const auto &n : nudges){
                        obj_i.vel += n.dir * en_game.max_speed * n.repulsion_factor * n.intensity * time_scale;
                    }
                }

                // Limit the maximum speed.
                {
                    const auto speed = obj_i.vel.length();
                    if(en_game.max_speed < speed){
                        const auto dir = obj_i.vel.unit();
                        obj_i.vel = dir * en_game.max_speed;
                    }
                }
            }
            t_en_updated = t_now;

            // Draw the transfer events.
            for(auto &p : transfer_events){
                ImVec2 obj_pos = curr_pos;
                obj_pos.x = curr_pos.x + p.x;
                obj_pos.y = curr_pos.y + p.y;

                auto c = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
                window_draw_list->AddCircle(obj_pos, 1.0, c);
             }

            // Draw the velocity vectors.
            for(size_t i = 0UL; i < N_objs; ++i){
                auto &obj_i = en_game_objs[i];

                const auto obj_pos  = ImVec2(curr_pos.x + obj_i.pos.x, curr_pos.y + obj_i.pos.y);
                const auto vec_term = ImVec2(curr_pos.x + obj_i.pos.x + obj_i.vel.x, curr_pos.y + obj_i.pos.y + obj_i.vel.y);
                auto c = ImColor(1.0f, 0.0f, 0.0f, 1.0f);
                window_draw_list->AddLine(obj_pos, vec_term, c);
                //IMGUI_API void  AddTriangle(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness = 1.0f);
             }

            // Include the newly-created objects.
            en_game_objs.insert( std::begin(en_game_objs),
                                 std::begin(l_en_game_objs), std::end(l_en_game_objs) );
            l_en_game_objs.clear();

            // Remove objects with a small radius.
            en_game_objs.erase( std::remove_if( std::begin(en_game_objs),
                                                std::end(en_game_objs),
                                                [&](const en_game_obj_t &obj) -> bool {
                                                    return (obj.rad < en_game.min_radius);
                                                }),
                                std::end(en_game_objs) );


            ImGui::Dummy(ImVec2(en_game.box_width, en_game.box_height));
            ImGui::End();
        }


        if( view_toggles.view_cube_enabled ){

            const auto win_width  = static_cast<int>(700);
            const auto win_height = static_cast<int>(500);
            auto flags = ImGuiWindowFlags_AlwaysAutoResize
                       | ImGuiWindowFlags_NoScrollWithMouse
                       | ImGuiWindowFlags_NoNavInputs
                       | ImGuiWindowFlags_NoScrollbar ;
            ImGui::SetNextWindowSize(ImVec2(win_width, win_height), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
            ImGui::Begin("Cube", &view_toggles.view_cube_enabled, flags );

            const auto reset = ImGui::Button("Reset");
            if(reset){
                reset_cube_game();
            }
            ImGui::SameLine();
            {
                auto l_N = static_cast<int>(rc_game.get_N());
                ImGui::SliderInt("Size", &l_N, 2, 10);
                if(l_N != rc_game_size){
                    rc_game_size = l_N;
                    reset_cube_game();
                }
            }
            ImGui::Separator();

            const int64_t rc_game_box_width = 1200;
            const int64_t rc_game_box_height = 800;


            // Lay out the faces and cells on a grid.
            const auto rc_game_N = rc_game.get_N();
            
            const int64_t cell_count_height = rc_game_N * 3 + 2;
            const int64_t cell_count_width = rc_game_N * 4 + 2;

            const int64_t cell_height = static_cast<int64_t>(std::floor(1.0f * rc_game_box_height / cell_count_height));
            const int64_t cell_width  = static_cast<int64_t>(std::floor(1.0f * rc_game_box_width  / cell_count_width ));


            ImVec2 curr_screen_pos = ImGui::GetCursorScreenPos();
            ImVec2 curr_window_pos = ImGui::GetCursorPos();
            //ImVec2 window_extent = ImGui::GetContentRegionAvail();
            ImDrawList *window_draw_list = ImGui::GetWindowDrawList();
            const auto f = ImGui::IsWindowFocused();

            {
                const auto c = ImColor(0.7f, 0.7f, 0.8f, 1.0f);
                window_draw_list->AddRect(curr_screen_pos, ImVec2( curr_screen_pos.x + rc_game_box_width, 
                                                                   curr_screen_pos.y + rc_game_box_height ), c);
            }

            const auto t_now = std::chrono::steady_clock::now();
            const auto t_started_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_cube_started).count();
            auto t_updated_diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_en_updated).count();

            // Limit individual time steps to around 30 fps otherwise 'infinitesimal' updates to the system will no
            // longer be small, and the simulation will quickly break down.
            //
            // Note that this will cause the simulation to be choppy if the frame rate falls below 30 fps or so.
            if(30 < t_updated_diff) t_updated_diff = 30;

            const auto block_dims = ImVec2( cell_width, cell_height );

            // Use a placeholder object to determine which drag-and-drop payload is available, if any.
            ImGui::Dummy(ImVec2( rc_game_box_width, rc_game_box_height));
            std::optional<int64_t> drag_and_drop_index;
            if( ImGui::BeginDragDropTarget() ){
                int64_t l_index = -10;
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("rc_game_index", ImGuiDragDropFlags_AcceptPeekOnly); payload != nullptr){
                    if(payload->DataSize != sizeof(int64_t)){
                        throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
                    }
                    drag_and_drop_index = *static_cast<const int64_t*>(payload->Data);
                }

                ImGui::EndDragDropTarget();
            }
            ImGui::SetCursorPos(curr_window_pos);

            std::optional< std::tuple<int64_t, int64_t, rc_game_t::coords_t> > drag_and_drop_grid_coords;



            // Walk over the grid.
            for(int64_t i = 1; (i+1) < cell_count_width; ++i){
                for(int64_t j = 1; (j+1) < cell_count_height; ++j){
                    ImVec2 cell_pos_window( curr_window_pos.x + (cell_width  * i),
                                            curr_window_pos.y + (cell_height * j) );
                    ImVec2 cell_pos_screen( curr_screen_pos.x + (cell_width  * i),
                                            curr_screen_pos.y + (cell_height * j) );

                    rc_game_t::coords_t c { -1, -1, -1 };

                    if(false){
                    }else if( isininc( rc_game_N*0 + 1, i, rc_game_N*1) 
                          &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                        std::get<0>(c) = 0; // Face number.
                        std::get<1>(c) = i - 1 - (rc_game_N*0); // x coordinate.
                        std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

                    }else if( isininc( rc_game_N*1 + 1, i, rc_game_N*2) 
                          &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                        std::get<0>(c) = 1; // Face number.
                        std::get<1>(c) = i - 1 - (rc_game_N*1); // x coordinate.
                        std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

                    }else if( isininc( rc_game_N*2 + 1, i, rc_game_N*3) 
                          &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                        std::get<0>(c) = 2; // Face number.
                        std::get<1>(c) = i - 1 - (rc_game_N*2); // x coordinate.
                        std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

                    }else if( isininc( rc_game_N*3 + 1, i, rc_game_N*4) 
                          &&  isininc( rc_game_N*1 + 1, j, rc_game_N*2) ){
                        std::get<0>(c) = 3; // Face number.
                        std::get<1>(c) = i - 1 - (rc_game_N*3); // x coordinate.
                        std::get<2>(c) = j - 1 - (rc_game_N*1); // y coordinate.

                    }else if( isininc( rc_game_N*1 + 1, i, rc_game_N*2) 
                          &&  isininc( rc_game_N*0 + 1, j, rc_game_N*1) ){
                        std::get<0>(c) = 4; // Face number.
                        std::get<1>(c) = i - 1 - (rc_game_N*1); // x coordinate.
                        std::get<2>(c) = j - 1 - (rc_game_N*0); // y coordinate.

                    }else if( isininc( rc_game_N*2 + 1, i, rc_game_N*3) 
                          &&  isininc( rc_game_N*2 + 1, j, rc_game_N*3) ){
                        std::get<0>(c) = 5; // Face number.
                        std::get<1>(c) = i - 1 - (rc_game_N*2); // x coordinate.
                        std::get<2>(c) = j - 1 - (rc_game_N*2); // y coordinate.

                    }

                    // Invert the y coordinate (map between screen space and the cell layout).
                    std::get<2>(c) = (rc_game_N - 1) - std::get<2>(c);

                    const auto index = rc_game.index(c);

                    if(rc_game.confirm_index_valid(index)){
                        // If this is the cell being dragged, save the coordinates for later.
                        if( drag_and_drop_index
                        &&  (index == drag_and_drop_index.value()) ){
                            drag_and_drop_grid_coords = std::make_tuple(i, j, c);
                        }

                        const auto l_colour_num = rc_game.get_const_cell(index).colour;
                        const auto l_colour = rc_game.colour_to_rgba(l_colour_num);
                        const auto im_col = ImColor(l_colour.at(0), l_colour.at(1), l_colour.at(2), l_colour.at(3)).Value;

                        // Check if this cell can accept an in-progress drag-and-drop index.
                        bool drag_and_drop_active = !!drag_and_drop_index;

                        const auto & [ cell_face, cell_x, cell_y ] = c;

                        std::stringstream ss;
                        ss << "##";
                        ss << i << ", " << j << "\n"
                           << cell_face << ", "
                           << cell_x << ", "
                           << cell_y << "\n";

                        ImGui::SetCursorPos(cell_pos_window);

                        int styles_overridden = 0;
                        {
                            auto im_colour_button = im_col;
                            auto im_colour_hovered = im_col;
                            auto im_colour_active = im_col;

                            im_colour_button.w *= 0.9;
                            im_colour_hovered.w *= 0.8;
                            im_colour_active.w *= 0.6;

                            if(drag_and_drop_active){
                                im_colour_button.w *= 0.25;
                                im_colour_hovered.w *= 0.25;
                                im_colour_active.w *= 0.25;
                            }

                            // Temporarily alter the appearance of buttons.
                            const auto& styles = ImGui::GetStyle();

                            ImGui::PushStyleColor(ImGuiCol_Button, im_colour_button);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, im_colour_hovered);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, im_colour_active);
                            styles_overridden += 3;
                        }

                        //ImGui::PushStyleColor(ImGuiCol_Button, im_col);

                        // Draw the button.
                        //
                        // Note that if the text is not unique, then a unique ID needs to be provided.
                        ImGui::Button(ss.str().c_str(), block_dims);

                        //ImGui::PopStyleColor(1);
                        if(0L < styles_overridden){
                            ImGui::PopStyleColor(styles_overridden);
                        }

                        // Make the cell draggable.
                        if( ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)){
                            int64_t payload = index;
                            ImGui::SetDragDropPayload("rc_game_index", static_cast<void*>(&payload), sizeof(payload));

                            // Show a preview of the cube being dragged.
                            ImGui::Text("Cell");

                            ImGui::EndDragDropSource();
                        }

                        // Draw a border around the cell.
                        const auto c_border = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
                        window_draw_list->AddRect(cell_pos_screen, ImVec2( cell_pos_screen.x + block_dims.x, 
                                                                           cell_pos_screen.y + block_dims.y ), c_border);
                    }


                    //ImGui::SetCursorPos(cell_pos);
                    //ImGui::Text("%s", const_cast<char *>(text.c_str()));
                    //ImGui::Dummy(block_dims);

                }
            }
            ImGui::SetCursorPos(curr_window_pos);



            // Walk over the grid.
            if(drag_and_drop_grid_coords){
                auto [i, j, c] = drag_and_drop_grid_coords.value();

                using pack_t = std::vector< std::tuple< int64_t, int64_t, rc_direction, std::string > >;
                for( auto [di, dj, dir, desc] : pack_t { { -1,  0, rc_direction::left,         std::string("left") },
                                                         {  1,  0, rc_direction::right,        std::string("right") },
                                                         {  0, -1, rc_direction::up,           std::string("up") },
                                                         {  0,  1, rc_direction::down,         std::string("down") },
                                                         { -1, -1, rc_direction::rotate_left,  std::string("rotate\nleft") },
                                                         {  1, -1, rc_direction::rotate_right, std::string("rotate\nright") } } ){
                    ImVec2 cell_pos_screen( curr_screen_pos.x + (cell_width  * (i + di)),
                                            curr_screen_pos.y + (cell_height * (j + dj)) );
                    ImVec2 cell_pos_window( curr_window_pos.x + (cell_width  * (i + di)),
                                            curr_window_pos.y + (cell_height * (j + dj)) );

                    std::stringstream ss;
                    if( false ){
                    }else if( (dir == rc_direction::left)
                          ||  (dir == rc_direction::right)
                          ||  (dir == rc_direction::up)
                          ||  (dir == rc_direction::down) ){
                        const auto [adj_c, adj_dir] = rc_game.get_neighbour_cell({c, dir});
                        const auto [adj_f, adj_x, adj_y] = adj_c;
                        ss << "##" << desc << "\n" << adj_f << "," << adj_x << "," << adj_y;
                    }else{
                        ss << "##" << desc;
                    }

                    ImGui::SetCursorPos(cell_pos_window);
                    ImGui::Button(ss.str().c_str(), block_dims);

                    // Accept a cell dragged here.
                    if( ImGui::BeginDragDropTarget() ){
                        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("rc_game_index"); payload != nullptr){
                            if(payload->DataSize != sizeof(int64_t)){
                                throw std::logic_error("Drag-and-drop payload is not expected size, refusing to continue");
                            }
                            const int64_t payload_index = *static_cast<const int64_t*>(payload->Data);
                            const int64_t l_index = rc_game.index(c);
                            if(l_index != payload_index){
                                throw std::logic_error("Drag-and-drop inconsistency, unable to continue");
                            }

                            // Implement the move.
                            rc_game.move( std::make_tuple(c, dir) );
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Show an indicator of what the drop buttons will do.
                    const auto c_border = ImColor(0.8f, 0.8f, 0.8f, 1.0f);

                    std::vector<vec2<float>> verts;

                    if( (dir == rc_direction::left)
                    ||  (dir == rc_direction::right)
                    ||  (dir == rc_direction::up)
                    ||  (dir == rc_direction::down) ){
                        // Left arrow.        x       y
                        verts.emplace_back( -0.50,  0.00 );
                        verts.emplace_back( -0.10, -0.35 );
                        verts.emplace_back( -0.10, -0.20 );
                        verts.emplace_back(  0.50, -0.20 );
                        verts.emplace_back(  0.50,  0.20 );
                        verts.emplace_back( -0.10,  0.20 );
                        verts.emplace_back( -0.10,  0.35 );
                        verts.emplace_back( -0.50,  0.00 );

                    }else{
                        // Left rotation symbol.
                        verts.emplace_back( -0.40,  0.40 );
                        verts.emplace_back( -0.40, -0.05 );
                        verts.emplace_back( -0.25,  0.10 );
                        verts.emplace_back(  0.00, -0.05 );
                        verts.emplace_back( -0.25, -0.35 );
                        verts.emplace_back(  0.10, -0.45 );
                        verts.emplace_back(  0.40,  0.00 );
                        verts.emplace_back( -0.05,  0.30 );
                        verts.emplace_back(  0.05,  0.40 );
                        verts.emplace_back( -0.40,  0.40 );
                    }

                    const auto pi = std::acos(-1.0);
                    for(auto & v : verts){
                        if(false){
                        }else if(dir == rc_direction::left){
                            // Do nothing.
                        }else if(dir == rc_direction::right){
                            v = v.rotate_around_z(pi);
                        }else if(dir == rc_direction::up){
                            v = v.rotate_around_z(pi * 1.5);
                        }else if(dir == rc_direction::down){
                            v = v.rotate_around_z(pi * 0.5);

                        }else if(dir == rc_direction::rotate_left){
                            // Do nothing.
                        }else if(dir == rc_direction::rotate_right){
                            // Mirror.
                            v.x *= -1.0;
                        }
                    }

                    window_draw_list->PathClear();
                    for(auto & v : verts){
                        // Scale and rotate if needed.
                        ImVec2 im_v;
                        im_v.x = cell_pos_screen.x + block_dims.x * 0.5 + v.x * (block_dims.x * 0.45);
                        im_v.y = cell_pos_screen.y + block_dims.y * 0.5 - v.y * (block_dims.y * 0.45);
                        window_draw_list->PathLineTo( im_v );
                    }
                    float thickness = 1.5f;
                    bool closed = false;
                    window_draw_list->PathStroke(c_border, closed, thickness);
                }

            }
            ImGui::SetCursorPos(curr_window_pos);



/*
            decltype(en_game_objs) l_en_game_objs;
            for(auto &obj : en_game_objs){
                ImVec2 obj_pos = curr_pos;
                obj_pos.x = curr_pos.x + obj.pos.x;
                obj_pos.y = curr_pos.y + obj.pos.y;

                const auto rel_r = std::clamp<double>(obj.rad / 30.0, 0.0, 1.0);
                auto c = ImColor(rel_r * 1.0f, (1.0f - rel_r) * 1.0f, 0.5f, 1.0f);
                if(obj.player_controlled){
                    c = ImColor(1.0f, 1.0f, 0.1f, 1.0f);
                }
                window_draw_list->AddCircle(obj_pos, obj.rad, c);

                // Implement player controls.
                if( f && obj.player_controlled){
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) ){
                        obj.vel.x -= 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_RightArrow)) ){
                        obj.vel.x += 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_UpArrow)) ){
                        obj.vel.y -= 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_DownArrow)) ){
                        obj.vel.y += 1.0;
                    }
                    if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Space)) ){
                        // Jettison a small object in the direction opposite of travel.
                        auto l_rad = obj.rad * 0.05;
                        l_rad = (l_rad < en_game.min_radius) ? en_game.min_radius : l_rad;
                        auto l_dir = obj.vel * (-1.0);
                        l_dir = ( 0.0 < l_dir.length() ) ? l_dir : vec2<double>(1.0, 0.0);

                        attempt_to_shed( obj, l_dir, l_rad, l_en_game_objs);
                    }
                    if( ImGui::IsKeyPressed(SDL_SCANCODE_S) ){
                        // Attempt to split into two.
                        auto l_rad = std::sqrt(0.5) * obj.rad;
                        auto l_dir = obj.vel * (-1.0);
                        l_dir = ( 0.0 < l_dir.length() ) ? l_dir : vec2<double>(1.0, 0.0);

                        decltype(en_game_objs) ll_en_game_objs;
                        attempt_to_shed( obj, l_dir, l_rad, ll_en_game_objs);
                        for(auto& obj : ll_en_game_objs){
                            obj.player_controlled = true;
                        }
                        l_en_game_objs.insert( std::begin(l_en_game_objs),
                                               std::begin(ll_en_game_objs), std::end(ll_en_game_objs) );
                    }
                }
            }
*/

            t_cube_updated = t_now;

            ImGui::Dummy(ImVec2(rc_game_box_width, rc_game_box_height));
            ImGui::End();
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
                        YLOGWARN("Shader compilation failed: '" << e.what() << "'");
                    };
                }


                ImGui::Text("Vertex shader");
                ImVec2 edit_box_extent = ImGui::GetContentRegionAvail();
                edit_box_extent.y *= 3.0 / 7.0;

                ImGuiInputTextFlags flags = 0;
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
            YLOGWARN("Exception in display_shader_editor(): '" << e.what() << "'");
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

                    auto N_sfs = static_cast<int64_t>(script_files.size());
                    if(ImGui::Button("New", ImVec2(window_extent.x/5, 0))){ 
                        script_files.emplace_back();
                        script_files.back().altered = true;
                        append_to_script(script_files.back().content, new_script_content);
                        script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
                        active_script_file = N_sfs;
                        N_sfs = static_cast<int64_t>(script_files.size());
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Open", ImVec2(window_extent.x/5, 0))){ 
                        if(!loaded_scripts.valid()){
                            loaded_scripts = std::async(std::launch::async, launch_script_open_dialog, open_file_root);
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Save As", ImVec2(window_extent.x/5, 0))){ 
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
                                YLOGWARN("Unable to access current filesystem path");
                            }
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Close", ImVec2(window_extent.x/5, 0))){ 
                        if( (N_sfs != 0) 
                        &&  isininc(0, active_script_file, N_sfs-1)){
                            script_files.erase( std::next( std::begin( script_files ), active_script_file ) );
                            --active_script_file; // Default to script on left.
                            --N_sfs;
                        }
                    }

                    if(ImGui::Button("Validate", ImVec2(window_extent.x/5, 0))){ 
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
                    if(ImGui::Button("Run", ImVec2(window_extent.x/5, 0))){ 
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
                    for(int64_t i = 0; i < N_sfs; ++i){
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
                        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize
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
                            const auto text_ln_max = std::max<int>(0, text_ln + static_cast<int>(std::floor((vert_scroll + edit_box_extent.y) / text_vert_spacing)));
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
            YLOGWARN("Exception in display_script_editor(): '" << e.what() << "'");
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
                                           &contouring_show_adjacent,
                                           &new_contour_name,
                                           &save_contour_buffer,
                                           &overwrite_existing_contours,
                                           &edit_existing_contour_selection,
                                           &contour_overlap_styles,
                                           &contour_overlap_style,
                                           &extract_contours,
                                           &extracted_contours,
                                           &extracted_contours_mutex,
                                           &contour_extraction_underway,
                                           &wq,

                                           &recompute_cimage_iters,
                                           &contouring_drover_cache,
                                           &create_cdrover_snapshot,
                                           &cdrover_ptr,
                                           &contouring_img_altered,
                                           &last_mouse_button_0_down,
                                           &last_mouse_button_1_down,
                                           &last_mouse_button_pos,
                                           &reset_contouring_state,
                                           &need_to_reload_opengl_texture,
                                           &editing_contour_colour,
                                           &pos_contour_colour,
                                           &neg_contour_colour,
                                           &adjacent_contour_colour,

                                           &row_profile,
                                           &col_profile,
                                           &time_profile,
                                           &time_course_abscissa_key,
                                           &time_course_image_inclusivity,
                                           &time_course_abscissa_relative,

                                           &display_metadata_table,

                                           &frame_count,

                                           &img_features ]() -> void {

// Lock the image details mutex in shared mode.
// 
// ... TODO ...
//
            if( !view_toggles.view_images_enabled
            ||  !current_texture.texture_exists
            ||  need_to_reload_opengl_texture.load() ) return;

            ImGui::SetNextWindowSize(ImVec2(650, 670), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_FirstUseEver);
            ImGui::Begin("Images", &view_toggles.view_images_enabled, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar );
            ImGuiIO &io = ImGui::GetIO();

            // Note: unhappy with this. Can cause feedback loop and flicker/jumpiness when resizing. Works OK for now
            // though. TODO.
            ImVec2 image_extent = ImGui::GetContentRegionAvail();
            const auto window_padding = ImGui::GetStyle().WindowPadding;
            image_extent.x = std::max<float>(128.0f, image_extent.x); // - 0.5f * window_padding.x);
            // Ensure images have the same aspect ratio as the true image.
            image_extent.y = current_texture.aspect_ratio * image_extent.x;
            auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(current_texture.texture_number));

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::Image(gl_tex_ptr, image_extent, uv_min, uv_max);
            image_mouse_pos_s image_mouse_pos;
            image_mouse_pos.mouse_hovering_image = ImGui::IsItemHovered();
            image_mouse_pos.image_window_focused = ImGui::IsWindowFocused();
            image_mouse_pos.image_window_hovered = ImGui::IsWindowHovered();

            ImVec2 real_extent; // The true dimensions of the unclipped image, accounting for zoom and panning.
            real_extent.x = image_extent.x / (uv_max.x - uv_min.x);
            real_extent.y = image_extent.y / (uv_max.y - uv_min.y);
            ImVec2 real_pos; // The true position of the top-left corner, accounting for zoom and panning.
            real_pos.x = pos.x - (real_extent.x * uv_min.x);
            real_pos.y = pos.y - (real_extent.y * uv_min.y);
            ImGui::End(); // "Images".

            // Force focus if there is a mouse wheel scroll while hovering the image viewer.
            if( image_mouse_pos.image_window_hovered
            &&  !image_mouse_pos.image_window_focused
            &&  (io.MouseWheel != 0.0) ){
                ImGui::SetWindowFocus("Images");
                image_mouse_pos.image_window_focused = true;
            }

            // Attempt to acquire an exclusive lock.
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            if( !view_toggles.view_images_enabled
            ||  !img_valid ) return;

            //We have three distinct coordinate systems: DICOM, pixel coordinates and screen pixel coordinates,
            // and SDL 'world' coordinates. We need to map from the DICOM coordinates to screen pixel coords.
            //
            //Get a DICOM-coordinate bounding box for the image.
            const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->columns;
            const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->rows; 
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
                image_mouse_pos.region_x = std::clamp<float>((io.MousePos.x - real_pos.x) / real_extent.x, 0.0f, 1.0f);
                image_mouse_pos.region_y = std::clamp<float>((io.MousePos.y - real_pos.y) / real_extent.y, 0.0f, 1.0f);
                image_mouse_pos.r = std::clamp<int64_t>( static_cast<int64_t>( std::floor( image_mouse_pos.region_y * img_rows_f ) ), 0L, (img_rows-1) );
                image_mouse_pos.c = std::clamp<int64_t>( static_cast<int64_t>( std::floor( image_mouse_pos.region_x * img_cols_f ) ), 0L, (img_cols-1) );
                image_mouse_pos.zero_pos = disp_img_it->position(0L, 0L);
                image_mouse_pos.dicom_pos = image_mouse_pos.zero_pos 
                                            + (disp_img_it->row_unit * disp_img_it->pxl_dx * img_cols_f) * image_mouse_pos.region_x
                                            + (disp_img_it->col_unit * disp_img_it->pxl_dy * img_rows_f) * image_mouse_pos.region_y
                                            - (disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5)
                                            - (disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5);
                image_mouse_pos.voxel_pos = disp_img_it->position(image_mouse_pos.r, image_mouse_pos.c);
                image_mouse_pos.pixel_scale = static_cast<float>(real_extent.y) / (disp_img_it->pxl_dy * disp_img_it->rows);

            }
            image_mouse_pos.DICOM_to_pixels = [=,
                                               disp_img_it = disp_img_it](const vec3<double> &P) -> ImVec2 {
                // Convert from absolute DICOM coordinates to ImGui screen pixel coordinates for the image.
                // This routine basically just inverts the above transformation.
                const auto img_rows = current_texture.row_count;
                const auto img_cols = current_texture.col_count;
                const auto img_rows_f = static_cast<float>(img_rows);
                const auto img_cols_f = static_cast<float>(img_cols);
                const auto Z = disp_img_it->position(0L, 0L);
                const auto region_x = (disp_img_it->row_unit.Dot(P - Z) + 0.5 * disp_img_it->pxl_dx)/(disp_img_it->pxl_dx * img_cols_f);
                const auto region_y = (disp_img_it->col_unit.Dot(P - Z) + 0.5 * disp_img_it->pxl_dy)/(disp_img_it->pxl_dy * img_rows_f);

                const auto pixel_x = pos.x + (region_x - uv_min.x) * image_extent.x/(uv_max.x - uv_min.x);
                const auto pixel_y = pos.y + (region_y - uv_min.y) * image_extent.y/(uv_max.y - uv_min.y);

                return ImVec2(pixel_x, pixel_y);
            };

            // Display a visual cue of the tagged position.
            if( tagged_pos 
            &&  image_mouse_pos.DICOM_to_pixels ){
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
                if(ImGui::Button("Unique", ImVec2(window_extent.x/2.1, 0))){ 
                    std::unique_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                    contour_colour_from_orientation = false;
                    contour_colours.clear();
                    altered = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Orientation", ImVec2(window_extent.x/2.1, 0))){ 
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
                if(ImGui::Button("All", ImVec2(window_extent.x/3.1, 0))){ 
                    for(auto &p : contour_enabled) p.second = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("None", ImVec2(window_extent.x/3.1, 0))){ 
                    for(auto &p : contour_enabled) p.second = false;
                }
                ImGui::SameLine();
                if(ImGui::Button("Invert", ImVec2(window_extent.x/3.1, 0))){ 
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
                        ImGui::SetNextItemWidth( window_extent.x/3.1 );
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
                        const auto clamped_col = dR.Dot( disp_img_it->row_unit ) / img_dicom_width;
                        const auto clamped_row = dR.Dot( disp_img_it->col_unit ) / img_dicom_height;

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
                    if( image_mouse_pos.mouse_hovering_image
                    &&  pc.contour.closed ){
                        const auto within_poly = pc.contour.Is_Point_In_Polygon_Projected_Orthogonally(img_plane,image_mouse_pos.dicom_pos);
                        thickness *= ( within_poly ) ? 1.5f : 1.0f;
                        if(within_poly) contour_hovered[pc.ROIName] = true;
                    }
                    imgs_window_draw_list->PathStroke(pc.colour, pc.contour.closed, thickness);
                    //AddPolyline(const ImVec2* points, int num_points, ImU32 col, bool closed, float thickness);
                }
            }

            // Overlay features on the current image.
            if( view_toggles.view_image_feature_extraction
            &&  image_mouse_pos.DICOM_to_pixels ){
                const auto box_radius = 3.0f;

                const auto img_val_opt = get_as<std::string>(disp_img_it->metadata, img_features.metadata_key);
                for(const auto &pset : { &img_features.features_A,
                                         &img_features.features_B,
                                         &img_features.features_C }){
                    const auto pset_val_opt = get_as<std::string>(pset->metadata, img_features.metadata_key);
                    if(pset_val_opt != img_val_opt) continue;

                    int64_t feature_num = 0;
                    int32_t colour_num = 0;
                    for(const auto &p : pset->points){
                        const auto c_rgb = Colour_cycle_max_contrast_20(colour_num);
                        auto c = img_features.use_override_colour
                               ? ImColor(img_features.o_col[0], img_features.o_col[1], img_features.o_col[2], img_features.o_col[3])
                               : ImColor(c_rgb.R, c_rgb.G, c_rgb.B, 1.0f);

                        // Display out-of-plane features with low alpha.
                        if( !disp_img_it->sandwiches_point_within_top_bottom_planes(p) ){
                            c.Value.w *= 0.25;
                        }

                        ImVec2 p1 = image_mouse_pos.DICOM_to_pixels(p);
                        ImVec2 ul1( p1.x - box_radius, p1.y - box_radius );
                        ImVec2 lr1( p1.x + box_radius, p1.y + box_radius );
                        imgs_window_draw_list->AddRect(ul1, lr1, c);

                        const auto fn_str = std::to_string(feature_num);
                        imgs_window_draw_list->AddText(lr1, c, fn_str.c_str());

                        ++feature_num;
                    }
                }
            }

            // Contouring and drawing interface.
            if( view_toggles.view_contouring_enabled
            ||  view_toggles.view_drawing_enabled ){
                // Provide a visual cue for the contouring brush.
                if(image_mouse_pos.mouse_hovering_image){
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
                        contour_extraction_underway += 1;

                        // Launch a thread to extract contours.
                        const auto worker = [&extract_contours,
                                             &extracted_contours_mutex,
                                             &extracted_contours,
                                             &contour_extraction_underway,
                                             l_cdrover = cdrover_ptr->Deep_Copy(),
                                             contouring_method ](){
                            try{
                                YLOGINFO("Starting contour extraction");
                                auto out = extract_contours(l_cdrover, contouring_method);
                                YLOGINFO("Completed contour extraction; waiting on lock");

                                std::unique_lock<std::shared_timed_mutex> ec_lock(extracted_contours_mutex);
                                extracted_contours = out;
                            }catch(const std::exception &e){
                                YLOGWARN("Contour extraction failed: '" << e.what() << "'");

                                std::unique_lock<std::shared_timed_mutex> ec_lock(extracted_contours_mutex);
                                extracted_contours = {};
                            }

                            contour_extraction_underway -= 1;
                            return;
                        };
                        wq.submit_task( worker ); // Offload to waiting worker thread.
                    }
                    ImGui::SameLine();
                    if(ImGui::BeginPopupModal("Save Contours", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                        const std::string dots((frame_count / 15) % 4, '.'); // Simplistic animation.
                        ImGui::Text("Saving contours%s", dots.c_str());

                        ImGui::InputText("ROI Name", new_contour_name.data(), new_contour_name.size());
                        std::string entered_text;
                        array_to_string(entered_text, new_contour_name);

                        ImGui::Checkbox("Overwrite existing contours", &overwrite_existing_contours);

                        // Check if the contouring task is complete. We assume that the work is done and the results
                        // have replaced the original contouring Drover object if there are no available contours.
                        const bool work_is_done = (contour_extraction_underway.load() == 0L);
                        if(work_is_done){
                            std::unique_lock<std::shared_timed_mutex> lock(extracted_contours_mutex,
                                                                           std::chrono::microseconds(50));
                            if( lock
                            && extracted_contours){
                                *cdrover_ptr = extracted_contours.value();
                                extracted_contours = {};
                            }
                        }else{
                            ImGui::TextDisabled("Waiting for contour extraction%s", dots.c_str());
                        }

                        bool roiname_is_valid = !entered_text.empty();
                        if(!roiname_is_valid){
                            ImGui::TextDisabled("Please enter a name to proceed.");
                        }
                        if(roiname_is_valid && !overwrite_existing_contours){
                            DICOM_data.Ensure_Contour_Data_Allocated();
                            roiname_is_valid = ( std::find_if( std::begin(DICOM_data.contour_data->ccs),
                                                         std::end(DICOM_data.contour_data->ccs),
                                                         [entered_text](const contour_collection<double> &cc) -> bool {
                                                             const auto l_roi_name = cc.get_dominant_value_for_key("ROIName").value_or("unspecified");
                                                             return (l_roi_name == entered_text);
                                                         }) == std::end(DICOM_data.contour_data->ccs) );
                            if(!roiname_is_valid){
                                ImGui::TextDisabled("Found existing contour with the given name.");
                            }
                        }

                        const bool save_possible = roiname_is_valid && work_is_done;
                        ImGui::BeginDisabled(!save_possible);
                        const bool clicked_save = ImGui::Button("Save");
                        ImGui::EndDisabled();

                        if( clicked_save
                        &&  save_possible
                        &&  save_contour_buffer(entered_text) ){
                            string_to_array(new_contour_name, "");
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::SameLine();
                        if(ImGui::Button("Cancel")){
                            ImGui::CloseCurrentPopup();
                            // Detaching here would be nice, but not currently available for std::shared_future AFAICT.
                            // Otherwise, we have to wait for the task to complete. Maybe transfer to yet another thread
                            // and detach the thread?? TODO.
                            extracted_contours = decltype(extracted_contours)();
                            contouring_img_altered = true;
                        }
                        ImGui::EndPopup();
                    }

                    DICOM_data.Ensure_Contour_Data_Allocated();
                    if( ImGui::Button("Edit Existing")
                    &&  !DICOM_data.contour_data->ccs.empty() ){ 
                        edit_existing_contour_selection = std::begin(DICOM_data.contour_data->ccs);

                        ImGui::OpenPopup("Edit Existing Contours");
                    }
                    if(ImGui::BeginPopupModal("Edit Existing Contours", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                        DICOM_data.Ensure_Contour_Data_Allocated();

                        const auto beg = std::begin(DICOM_data.contour_data->ccs);
                        const auto end = std::end(DICOM_data.contour_data->ccs);
                        bool valid_roi_selected = false;
                        
                        if(ImGui::BeginListBox("Contours")){
                            for(auto it = beg; it != end; ++it){
                                const auto l_roi_name = it->get_dominant_value_for_key("ROIName").value_or("unspecified");
                                const bool is_selected =  edit_existing_contour_selection
                                                       && (edit_existing_contour_selection.value() == it);
                                if(is_selected) valid_roi_selected = true;
                                ImGui::PushID(static_cast<void*>(std::addressof(*it)));
                                if(ImGui::Selectable(l_roi_name.data(), is_selected)){
                                    edit_existing_contour_selection = it;
                                }
                                ImGui::PopID();
                                if(is_selected) ImGui::SetItemDefaultFocus(); // Used for keyboard navigation.
                            }
                            ImGui::EndListBox();
                        }

                        if(ImGui::BeginListBox("Overlap Handling")){
                            for(size_t i = 0UL; i < contour_overlap_styles.size(); ++i){
                                const bool is_selected = (i == contour_overlap_style);
                                ImGui::PushID(i);
                                if(ImGui::Selectable(contour_overlap_styles[i].data(), is_selected)){
                                    contour_overlap_style = i;
                                }
                                ImGui::PopID();
                                if(is_selected) ImGui::SetItemDefaultFocus(); // Used for keyboard navigation.
                            }
                            ImGui::EndListBox();
                        }

                        const bool clicked_copy = ImGui::Button("Copy");
                        if( clicked_copy
                        &&  valid_roi_selected
                        &&  edit_existing_contour_selection ){

                            // Copy the selected contours to a shuttle. We use double-buffering here in case there are
                            // any existing contours.
                            decltype(cdrover_ptr->contour_data->ccs) shtl;
                            shtl.emplace_back();
                            for(const auto& c : edit_existing_contour_selection.value()->contours){
                                shtl.back().contours.push_back( c );
                            }
                            auto cm = shtl.back().get_common_metadata({}, {});

                            // Make snapshot of the current state.
                            cdrover_ptr = create_cdrover_snapshot(contouring_drover_cache, cdrover_ptr);

                            std::list<OperationArgPkg> Operations;
                            const bool op_load_res = Load_Standard_Script( Operations, "plumbing", "copy existing contours" );
                            if(!op_load_res) throw std::runtime_error("Unable to load script");
                            metadata_map_t l_InvocationMetadata;
                            contour_overlap_style = std::clamp<size_t>(contour_overlap_style, static_cast<size_t>(0UL), contour_overlap_styles.size());
                            l_InvocationMetadata["contour_overlap_method"] = contour_overlap_styles[contour_overlap_style];

                            cdrover_ptr->Ensure_Contour_Data_Allocated();
                            cdrover_ptr->contour_data->ccs.swap(shtl);
                            const bool res = Operation_Dispatcher(*cdrover_ptr, l_InvocationMetadata, FilenameLex, Operations);
                            cdrover_ptr->Ensure_Contour_Data_Allocated();
                            cdrover_ptr->contour_data->ccs.swap(shtl);
                            
                            contouring_img_altered = true;
                            if(res){
                                edit_existing_contour_selection = {};
                                ImGui::CloseCurrentPopup();
                            }else{
                                YLOGWARN("Copying failed");
                            }
                        }

                        ImGui::SameLine();
                        if(ImGui::Button("Cancel")){
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                }

                // Undo and redo manual control.
                if( view_toggles.view_contouring_enabled ){
                    const auto v_list = contouring_drover_cache.get_versions();
                    const auto v_curr = contouring_drover_cache.get_version(cdrover_ptr);
                    int pos = 0;
                    for(const auto &v : v_list){
                        if(v == v_curr){
                            break;
                        }else{
                            ++pos;
                        }
                    }

                    const auto N_versions = static_cast<int>(v_list.size());
                    ImGui::SliderInt("History", &pos, 0, N_versions - 1);
                    if( ImGui::IsItemHovered() ){
                        ImGui::BeginTooltip();
                        ImGui::Text("Note: hot keys [ctrl]+[z] and [ctrl]+[y] are supported.");
                        ImGui::EndTooltip();
                    }
                    const auto new_pos = std::clamp<int>(pos, 0, N_versions - 1);
                    const auto v_new = *(std::next( std::begin(v_list), new_pos ));
                    if(v_new != v_curr){
                        auto l_cdrover_ptr = contouring_drover_cache.get(v_new);
                        cdrover_ptr = (l_cdrover_ptr == nullptr) ? cdrover_ptr : l_cdrover_ptr;

                        contouring_img_altered = true;
                    }

                    if(ImGui::Checkbox("Show contours from adjacent slices", &contouring_show_adjacent)){
                        contouring_img_altered = true;
                    }

                    ImGui::Separator();
                }

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
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A fast brush that is not smoothed.\n"
                                "Produces jagged brush strokes and contours.\n"
                                "This brush has no dynamics.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Circle")){
                    contouring_brush = brush_t::mean_circle;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that averages voxel intensities.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Circle")){
                    contouring_brush = brush_t::median_circle;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that applies a median filter to voxel intensities.");
                    ImGui::EndTooltip();
                }

                if(ImGui::Button("Rigid Square")){
                    contouring_brush = brush_t::rigid_square;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A fast brush that is not smoothed.\n"
                                "Produces jagged brush strokes and contours.\n"
                                "This brush has no dynamics.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Square")){
                    contouring_brush = brush_t::mean_square;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that averages voxel intensities.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Square")){
                    contouring_brush = brush_t::median_square;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that applies a median filter to voxel intensities.");
                    ImGui::EndTooltip();
                }

                if(ImGui::Button("2D Gaussian")){
                    contouring_brush = brush_t::gaussian_2D;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Provides a smooth brush with strong dynamic effect.\n"
                                "This brush is somewhat slow because the Gaussian distribution extends\n"
                                "relatively far outwards into adjacent voxels.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("2D Tanh")){
                    contouring_brush = brush_t::tanh_2D;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Provides geometrically-accurate contours and sweeps with minimal dynamics.\n"
                                "This brush is also economical compared to the rigid brush,\n"
                                "requiring lower mask resolution to achieve a smooth contour.");
                    ImGui::EndTooltip();
                }

                ImGui::Text("3D shapes");
                if(ImGui::Button("Rigid Sphere")){
                    contouring_brush = brush_t::rigid_sphere;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A fast brush that is not smoothed.\n"
                                "Produces jagged brush strokes and contours.\n"
                                "This brush has no dynamics.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Sphere")){
                    contouring_brush = brush_t::mean_sphere;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that averages voxel intensities.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Sphere")){
                    contouring_brush = brush_t::median_sphere;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that applies a median filter to voxel intensities.");
                    ImGui::EndTooltip();
                }

                if(ImGui::Button("Rigid Cube")){
                    contouring_brush = brush_t::rigid_cube;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A fast brush that is not smoothed.\n"
                                "Produces jagged brush strokes and contours.\n"
                                "This brush has no dynamics.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Mean Cube")){
                    contouring_brush = brush_t::mean_cube;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that averages voxel intensities.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("Median Cube")){
                    contouring_brush = brush_t::median_cube;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("A brush that applies a median filter to voxel intensities.");
                    ImGui::EndTooltip();
                }

                if(ImGui::Button("3D Gaussian")){
                    contouring_brush = brush_t::gaussian_3D;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Provides a smooth brush with strong dynamic effect.\n"
                                "This brush is somewhat slow because the Gaussian distribution extends\n"
                                "relatively far outwards into adjacent voxels.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                if(ImGui::Button("3D Tanh")){
                    contouring_brush = brush_t::tanh_3D;
                }
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Provides geometrically-accurate contours and sweeps with minimal dynamics.\n"
                                "This brush is also economical compared to the rigid brush,\n"
                                "requiring lower mask resolution to achieve a smooth contour.");
                    ImGui::EndTooltip();
                }

                ImGui::Separator();
                ImGui::Text("Dilation and Erosion");
                ImGui::DragFloat("Margin (mm)", &contouring_margin, 0.1f, -10.0f, 10.0f);
                if(ImGui::Button("Apply Margin")){

                    // Make snapshot of the current state.
                    if( view_toggles.view_contouring_enabled ){
                        cdrover_ptr = create_cdrover_snapshot(contouring_drover_cache, cdrover_ptr);
                    }

                    std::list<OperationArgPkg> Operations;
                    const bool op_load_res = Load_Standard_Script( Operations, "plumbing", "dilate erode margin" );
                    if(!op_load_res) throw std::runtime_error("Unable to load script");
                    metadata_map_t l_InvocationMetadata;
                    contour_overlap_style = std::clamp<size_t>(contour_overlap_style, static_cast<size_t>(0UL), contour_overlap_styles.size());
                    l_InvocationMetadata["reduction"] = (0.0 <= contouring_margin) ? "dilate" : "erode";
                    l_InvocationMetadata["max_distance"] = std::to_string( std::abs( contouring_margin ) );

                    Drover *d = (view_toggles.view_contouring_enabled) ? cdrover_ptr.get() : &DICOM_data;
                    if(!Operation_Dispatcher(*d, l_InvocationMetadata, FilenameLex, Operations)){
                        YLOGWARN("Dilation/Erosion failed");
                    }

                    if(view_toggles.view_contouring_enabled){
                        contouring_img_altered = true;
                    }else if(view_toggles.view_drawing_enabled){
                        need_to_reload_opengl_texture.store(true);
                    }
                }

                ImGui::Separator();
                ImGui::Text("Clearing");
                const bool button_clear_one_slice = ImGui::Button("Clear this slice");
                ImGui::SameLine();
                const bool button_clear_all_slices = ImGui::Button("Clear all slices");

                if( button_clear_one_slice
                ||  button_clear_all_slices ){

                    // Make snapshot of the current state.
                    if( view_toggles.view_contouring_enabled ){
                        cdrover_ptr = create_cdrover_snapshot(contouring_drover_cache, cdrover_ptr);
                    }
                    Drover *d = (view_toggles.view_contouring_enabled) ? cdrover_ptr.get() : &DICOM_data;

                    auto [img_valid, img_array_ptr_it, img_it] = recompute_image_iters();
                    if( view_toggles.view_contouring_enabled ){
                        std::tie(img_valid, img_array_ptr_it, img_it) = recompute_cimage_iters();
                    }
                    if( img_valid ){
                        if(false){
                        }else if(button_clear_one_slice){
                            img_it->fill_pixels(0.0f);
                        }else if(button_clear_all_slices){
                            for(auto & img : (*img_array_ptr_it)->imagecoll.images){
                                img.fill_pixels(0.0f);
                            }
                        }else{
                            throw std::logic_error("Unaccounted for button clicked");
                        }
                    }
                    if(d != nullptr ){
                        d->Ensure_Contour_Data_Allocated();
                        d->contour_data->ccs.clear();
                    }

                    if(view_toggles.view_contouring_enabled){
                        contouring_img_altered = true;
                        contouring_drover_cache.trim(10UL);
                    }else if(view_toggles.view_drawing_enabled){
                        need_to_reload_opengl_texture.store(true);
                    }
                    last_mouse_button_0_down = 1E30;
                    last_mouse_button_1_down = 1E30;
                    last_mouse_button_pos = {};
                }

                ImGui::Separator();
                ImGui::Text("Interpolation");
                const bool button_interpolation_axially = ImGui::Button("Interpolate Axially");
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Interpolate *cleared* slices."
                                "\n\n"
                                "Only cleared images will be filled in, and the"
                                "\n image being viewed must be within the range of cleared images."
                                "\n\n"
                                "The cleared images must be bounded by non-cleared images."
                                "\n\n"
                                "Note: use *only* if contours/shapes are convex and axially aligned"
                                "\n (e.g. the ends of a cylinder). Misaligned contours/shapes will"
                                "\n be improperly interpolated.");
                    ImGui::EndTooltip();
                }
                if(button_interpolation_axially){
                    // Make snapshot of the current state.
                    if( view_toggles.view_contouring_enabled ){
                        cdrover_ptr = create_cdrover_snapshot(contouring_drover_cache, cdrover_ptr);
                    }
                    Drover *d = (view_toggles.view_contouring_enabled) ? cdrover_ptr.get() : &DICOM_data;

                    auto [img_valid, img_array_ptr_it, img_it] = recompute_image_iters();
                    if( view_toggles.view_contouring_enabled ){
                        std::tie(img_valid, img_array_ptr_it, img_it) = recompute_cimage_iters();
                    }
                    if( img_valid ){

                        // Build a spatial index.
                        {
                            std::list<planar_image_adjacency<float,double>::img_refw_t> selected_imgs;
                            for(auto &img : (*img_array_ptr_it)->imagecoll.images){
                                selected_imgs.push_back( std::ref(img) );
                            }
                            if(selected_imgs.empty()){
                                throw std::invalid_argument("Unable to build spatial index: no images available. Cannot continue");
                            }
                            if(!Images_Form_Rectilinear_Grid(selected_imgs)){
                                throw std::invalid_argument("Unable to build spatial index: images do not form a rectilinear grid. Cannot continue");
                            }
                        }
                        const auto row_unit = (*img_array_ptr_it)->imagecoll.images.front().row_unit.unit();
                        const auto col_unit = (*img_array_ptr_it)->imagecoll.images.front().col_unit.unit();
                        const auto img_unit = (*img_array_ptr_it)->imagecoll.images.front().ortho_unit();

                        planar_image_adjacency<float,double> img_adj( {}, { { std::ref((*img_array_ptr_it)->imagecoll) } }, img_unit );
                        if(img_adj.int_to_img.empty()){
                            throw std::invalid_argument("Unable to build spatial index: mapping is empty. Cannot continue.");
                        }
                        //auto beg = std::begin((*img_array_ptr_it)->imagecoll.images);
                        //auto end = std::end((*img_array_ptr_it)->imagecoll.images);
                        auto img_refw = typename planar_image_adjacency<float,double>::img_refw_t( std::ref(*img_it) );
                        const auto img_N = img_adj.image_to_index(img_refw);

                        // Scan outwards to find bounding image slices that have been altered.
                        std::optional<int64_t> upper_img_N_opt;
                        std::optional<int64_t> lower_img_N_opt;
                        std::set<int64_t> gap_imgs;

                        const auto scan_imgs = [&](int64_t increment,
                                                   std::set<int64_t> &l_empty_imgs){

                            std::optional<int64_t> first_nonempty_img;
                            for(int64_t i = 0; ; i += increment){
                                const auto l_img_N = img_N + i;
                                const bool l_img_valid = img_adj.index_present(l_img_N);
                                if( !l_img_valid ) break;

                                const auto& img = img_adj.index_to_image(l_img_N);
                                const auto mm = img.get().minmax();
                                if( (mm.first < 0.5) &&  (mm.second < 0.5) ){
                                    l_empty_imgs.insert(l_img_N);
                                }else{
                                    first_nonempty_img = l_img_N;
                                    break;
                                }
                            }
                            return first_nonempty_img;
                        };
                        upper_img_N_opt = scan_imgs( 1, gap_imgs);
                        lower_img_N_opt = scan_imgs(-1, gap_imgs);

                        if(false){
                        }else if( !upper_img_N_opt || !lower_img_N_opt ){
                            YLOGWARN("Unable to interpolate: one or both bounding slices were not found");
                        }else if( (upper_img_N_opt.value() == lower_img_N_opt.value())
                              ||  gap_imgs.empty() ){
                            YLOGWARN("Unable to interpolate: no empty slices were found");
                        }else{
                            const auto &upper_img = img_adj.index_to_image( upper_img_N_opt.value() ).get();
                            const auto &lower_img = img_adj.index_to_image( lower_img_N_opt.value() ).get();
                            for(const auto& gap_img_N : gap_imgs){
                                auto& gap_img = img_adj.index_to_image( gap_img_N ).get();
                                const auto rows = gap_img.rows;
                                const auto cols = gap_img.columns;
                                const auto chns = gap_img.channels;

                                for(int64_t l_row = 0L; l_row < rows; ++l_row){
                                    for(int64_t l_col = 0L; l_col < cols; ++l_col){
                                        const auto upper_pos = upper_img.position(l_row, l_col);
                                        const auto lower_pos = lower_img.position(l_row, l_col);
                                        const auto gap_pos   = gap_img.position(l_row, l_col);
                                        const auto d_gl  = gap_pos.distance(lower_pos);
                                        const auto d_gu  = gap_pos.distance(upper_pos);
                                        const auto d_tot = d_gl + d_gu;
                                        for(int64_t l_chn = 0L; l_chn < chns; ++l_chn){
                                            const auto upper_val = upper_img.value(l_row, l_col, l_chn);
                                            const auto lower_val = lower_img.value(l_row, l_col, l_chn);

                                            const auto x = lower_val + (upper_val - lower_val) * (d_gl / d_tot);
                                            gap_img.reference(l_row, l_col, l_chn) = x;
                                        }
                                    }
                                }
                            }
                            YLOGINFO("Interpolated " << gap_imgs.size() << " images axially");
                        }
                    }
                    if(d != nullptr ){
                        d->Ensure_Contour_Data_Allocated();
                        d->contour_data->ccs.clear();
                    }

                    if(view_toggles.view_contouring_enabled){
                        contouring_img_altered = true;
                    }else if(view_toggles.view_drawing_enabled){
                        need_to_reload_opengl_texture.store(true);
                    }
                    last_mouse_button_0_down = 1E30;
                    last_mouse_button_1_down = 1E30;
                    last_mouse_button_pos = {};
                }

                if(view_toggles.view_contouring_enabled){
                    ImGui::Separator();
                    ImGui::Text("Contour Extraction");
                    if(ImGui::DragInt("Resolution", &contouring_img_row_col_count, 0.1f, 5, 1024)){
                        reset_contouring_state(img_array_ptr_it);
                        contouring_img_altered = true;
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
                    cdrover_ptr->Ensure_Contour_Data_Allocated();
                    if( auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();
                        cimg_valid
                    &&  contouring_img_altered
                    &&  (frame_count % 5 == 0) ){ // Terrible stop-gap until I can parallelize contour extraction. TODO

                        // Only bother extracting contours for the current and adjacent images.
                        const auto beg_it = std::begin((*cimg_array_ptr_it)->imagecoll.images);
                        const auto end_it = std::end((*cimg_array_ptr_it)->imagecoll.images);
                        Drover shtl;
                        shtl.Ensure_Contour_Data_Allocated();
                        shtl.image_data.push_back(std::make_unique<Image_Array>());
                        //shtl.image_data.back()->imagecoll.images.emplace_back();

                        if( contouring_show_adjacent
                        &&  (cimg_it != beg_it) ){
                            shtl.image_data.back()->imagecoll.images.emplace_back( *(std::prev(cimg_it)) );
                        }
                        shtl.image_data.back()->imagecoll.images.emplace_back( *cimg_it );
                        if( contouring_show_adjacent
                        &&  (std::next(cimg_it) != end_it) ){
                            shtl.image_data.back()->imagecoll.images.emplace_back( *(std::next(cimg_it)) );
                        }

                        std::list<OperationArgPkg> Operations;
                        const bool op_load_res = Load_Standard_Script( Operations, "plumbing", "extract contours from mask" );
                        if(!op_load_res) throw std::runtime_error("Unable to load script");
                        metadata_map_t l_InvocationMetadata;
                        l_InvocationMetadata["method"] = contouring_method;

                        if(!Operation_Dispatcher(shtl, l_InvocationMetadata, FilenameLex, Operations)){
                            YLOGWARN("ContourViaThreshold failed");
                        }

                        cdrover_ptr->contour_data->ccs.clear();
                        cdrover_ptr->Consume( shtl.contour_data );

                        contouring_img_altered = false;
                    }

                    // Draw the WIP contours.
                    cdrover_ptr->Ensure_Contour_Data_Allocated();
                    if( auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();
                        cimg_valid
                    &&  (cdrover_ptr->Has_Contour_Data()) ){
                        const auto cimg_dicom_width = cimg_it->pxl_dx * cimg_it->columns;
                        const auto cimg_dicom_height = cimg_it->pxl_dy * cimg_it->rows; 
                        //const auto cimg_top_left = cimg_it->anchor + cimg_it->offset
                        //                         - cimg_it->row_unit * cimg_it->pxl_dx * 0.5f
                        //                         - cimg_it->col_unit * cimg_it->pxl_dy * 0.5f;
                        //const auto cimg_top_right = cimg_top_left + cimg_it->row_unit * cimg_dicom_width;
                        //const auto cimg_bottom_left = cimg_top_left + cimg_it->col_unit * cimg_dicom_height;
                        //const auto cimg_plane = cimg_it->image_plane();

                        for(const auto &cc : cdrover_ptr->contour_data->ccs){
                            for(const auto &cop : cc.contours){
                                if( cop.points.empty() ) continue;
                                //if( !cimg_it->sandwiches_point_within_top_bottom_planes( cop.points.front() ) ) continue;
                                const bool is_in_plane = cimg_it->sandwiches_point_within_top_bottom_planes(cop.points.front());

                                if( !contouring_show_adjacent
                                &&  !is_in_plane) continue;

                                imgs_window_draw_list->PathClear();
                                for(auto & p : cop.points){

                                    //Clamp the point to the bounding box, using the top left as zero.
                                    const auto dR = p - img_top_left;
                                    const auto clamped_col = dR.Dot( cimg_it->row_unit ) / cimg_dicom_width;
                                    const auto clamped_row = dR.Dot( cimg_it->col_unit ) / cimg_dicom_height;

                                    //Convert to ImGui coordinates using the top-left position of the display image.
                                    const auto world_x = real_pos.x + real_extent.x * clamped_col;
                                    const auto world_y = real_pos.y + real_extent.y * clamped_row;

                                    ImVec2 v;
                                    v.x = world_x + (is_in_plane ? 0.0 : 0.1);
                                    v.y = world_y;
                                    imgs_window_draw_list->PathLineTo( v );
                                }

                                float thickness = contour_line_thickness;
                                ImU32 colour = ImGui::GetColorU32(editing_contour_colour);

                                if(is_in_plane){
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
                                }else{
                                    colour = ImGui::GetColorU32(adjacent_contour_colour);
                                    //thickness = contour_line_thickness;
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
                    for(int64_t chan = 0; chan < disp_img_it->channels; ++chan){
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

                auto common_metadata = coalesce_metadata_for_lsamp(disp_img_it->metadata);

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

                row_profile.metadata = common_metadata;
                row_profile.metadata["Abscissa"] = "ColumnNumber";
                row_profile.metadata["CurrentAbscissa"] = std::to_string(image_mouse_pos.c);

                col_profile.metadata = common_metadata;
                col_profile.metadata["Abscissa"] = "RowNumber";
                col_profile.metadata["CurrentAbscissa"] = std::to_string(image_mouse_pos.r);
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
                int64_t n_current_img = 0;
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
                time_profile.metadata["Abscissa"] = (meta_key) ? abscissa_key : "ImageNumber";
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
            YLOGWARN("Exception in display_image_viewer(): '" << e.what() << "'");
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

            // Process only one future every frame.
            // This keeps frame delays minimum, and also retains future creation order.
            if(loaded_files.empty()) return;

            // Prune invalid (default-constructed) futures.
            if(!loaded_files.front().valid()){
                loaded_files.pop_front();
                return;
            }

            ImGui::OpenPopup("Loading");
            if(ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                std::string str((frame_count / 15) % 4, '.'); // Simplistic animation.
                str.insert(str.size(), 4UL - str.size(), ' ');

                ImGui::Text("Loading files%s", str.c_str());

                if(ImGui::Button("Close")){
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if(std::future_status::ready == loaded_files.front().wait_for(std::chrono::microseconds(1))){
                std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
                auto f = loaded_files.front().get();

                if(f.res){
                    DICOM_data.Consume(f.DICOM_data);
                    f.InvocationMetadata.merge(InvocationMetadata);
                    InvocationMetadata = f.InvocationMetadata;
                }else{
                    YLOGWARN("Disregarding files");
                }

                recompute_image_state();
                need_to_reload_opengl_texture.store(true);
                loaded_files.pop_front();

                auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                if(img_valid){
                    if(view_toggles.view_contours_enabled) launch_contour_preprocessor();
                    reset_contouring_state(img_array_ptr_it);
                    tagged_pos = {};
                }
            }
            return;
        };
        try{
            handle_file_loading();
        }catch(const std::exception &e){
            YLOGWARN("Exception in handle_file_loading(): '" << e.what() << "'");
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
                        active_script_file = static_cast<int64_t>(script_files.size()) - 1;
                    }else{
                        YLOGWARN("Unable to load scripts");
                        // TODO ... warn about the issue.
                    }

                    loaded_scripts = decltype(loaded_scripts)();
                }
            }
        };
        try{
            handle_script_loading();
        }catch(const std::exception &e){
            YLOGWARN("Exception in handle_script_loading(): '" << e.what() << "'");
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
            YLOGWARN("Exception in adjust_window_level(): '" << e.what() << "'");
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

                // Draw buttons for each available colour map.
                for(size_t i = 0; i < colour_maps.size(); ++i){
                    if( ImGui::Button(colour_maps[i].first.c_str(), ImVec2(250, 0)) ){
                        colour_map = i;
                        reload_texture = true;
                    }
                }

                if(!reload_texture){
                    // Draw the scale bar.
                    auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(scale_bar_texture.texture_number));
                    ImGui::Image(gl_tex_ptr, ImVec2(250,25), ImVec2(0.0, 0.0), ImVec2(1.0, 1.0));
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
            YLOGWARN("Exception in adjust_colour_map(): '" << e.what() << "'");
            throw;
        }


        // Display plots.
        const auto display_plots = [&view_toggles,
                                    &drover_mutex,
                                    &mutex_dt,
                                    &DICOM_data,

                                    &lsamps_visible,
                                    &plot_norm,
                                    &plot_thickness,
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
                ImGui::DragFloat("Line thickness", &plot_thickness, 0.1f, 0.1f, 10.0f, "%.01f");
                ImGui::Checkbox("Show metadata on hover", &view_toggles.view_plots_metadata);
                ImGui::Checkbox("Show legend", &show_plot_legend);

                ImGui::Text("Normalization: ");
                ImGui::SameLine();
                if(ImGui::Button("None", ImVec2(window_extent.x/4, 0))){ 
                    plot_norm = plot_norm_t::none;
                }
                ImGui::SameLine();
                if(ImGui::Button("Max", ImVec2(window_extent.x/4, 0))){ 
                    plot_norm = plot_norm_t::max;
                }
            }

            const int N_lsamps = static_cast<int>(DICOM_data.lsamp_data.size());

            {
                ImVec2 window_extent = ImGui::GetContentRegionAvail();
                ImGui::Text("Display");
                if(ImGui::Button("All##plots_display", ImVec2(window_extent.x/4, 0))){ 
                    for(auto &p : lsamps_visible) p.second = true;
                }
                ImGui::SameLine();
                if(ImGui::Button("None##plots_display", ImVec2(window_extent.x/4, 0))){ 
                    for(auto &p : lsamps_visible) p.second = false;
                }
                ImGui::SameLine();
                if(ImGui::Button("Invert##plots_display", ImVec2(window_extent.x/4, 0))){ 
                    for(auto &p : lsamps_visible) p.second = !p.second;
                }
            }

            bool any_selected = false;
            std::optional<std::string> abscissa;
            std::optional<std::string> ordinate;
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
                if(is_visible){
                    any_selected = true;

                    const auto l_abscissa = (*lsamp_ptr_it)->line.GetMetadataValueAs<std::string>("Abscissa").value_or("unknown"_s);
                    if(!abscissa){
                        abscissa = l_abscissa;
                    }else if(abscissa.value() != l_abscissa){
                        abscissa = "(mixed)";
                    }
                    const auto l_ordinate = (*lsamp_ptr_it)->line.GetMetadataValueAs<std::string>("Ordinate").value_or("unknown"_s);
                    if(!ordinate){
                        ordinate = l_ordinate;
                    }else if(ordinate.value() != l_ordinate){
                        ordinate = "(mixed)";
                    }
                }
            }
            ImGui::End();

            if(plot_norm != plot_norm_t::none){
                ordinate = {};
            }

            if(any_selected){
                ImGui::SetNextWindowSize(ImVec2(620, 640), ImGuiCond_FirstUseEver);
                ImGui::Begin("Plots", &view_toggles.view_plots_enabled);
                ImVec2 window_extent = ImGui::GetContentRegionAvail();

                auto flags = ImPlotFlags_AntiAliased
                           | ImPlotFlags_NoLegend
                           | ImPlotFlags_Query;
                if(show_plot_legend) flags ^= ImPlotFlags_NoLegend;

                if(ImPlot::BeginPlot("Plots",
                                     abscissa ? abscissa.value().c_str() : nullptr,
                                     ordinate ? ordinate.value().c_str() : nullptr,
                                     window_extent,
                                     flags,
                                     ImPlotAxisFlags_AutoFit,
                                     ImPlotAxisFlags_AutoFit )) {

                    ImPlot::SetLegendLocation(ImPlotLocation_NorthEast, ImPlotOrientation_Vertical);

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

                        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, plot_thickness);
                        ImPlot::PlotLine<double>(title.c_str(),
                                                 &s_ptr->samples[0][0], 
                                                 &s_ptr->samples[0][2],
                                                 s_ptr->samples.size(),
                                                 offset, stride );
                        ImPlot::PopStyleVar();
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
            YLOGWARN("Exception in display_plots(): '" << e.what() << "'");
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
            YLOGWARN("Exception in display_row_column_profiles(): '" << e.what() << "'");
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
                        int64_t i = 0;
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
            YLOGWARN("Exception in display_time_profiles(): '" << e.what() << "'");
            throw;
        }

        // Display tables.
        const auto display_tables = [&view_toggles,
                                     &drover_mutex,
                                     &mutex_dt,
                                     &table_display,
                                     &recompute_table_iters,
                                     &display_metadata_table,
                                     &table_selection,
                                     &cell_being_edited,
                                     &cell_being_edited_first_frame,
                                     &cell_selected,
                                     &set_focus_on_cell,
                                     &get_table_selection_bounds,
                                     &DICOM_data ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_tables_enabled
            ||  !DICOM_data.Has_Table_Data() ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(750, 500), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 140), ImGuiCond_FirstUseEver);
            ImGui::Begin("Table Selection", &view_toggles.view_tables_enabled);

            // Measure user input.
            const bool window_is_focused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );

            const bool pressing_shift     = ImGui::GetIO().KeyShift;
            const bool pressing_ctrl      = ImGui::GetIO().KeyCtrl;
            const bool pressing_tab       = ImGui::IsKeyPressed(SDL_SCANCODE_TAB);
            const bool pressing_enter     = ImGui::IsKeyPressed(SDL_SCANCODE_RETURN);
            const bool pressing_delete    = ImGui::IsKeyPressed(SDL_SCANCODE_DELETE);
            const bool pressing_backspace = ImGui::IsKeyPressed(SDL_SCANCODE_BACKSPACE);
            const bool pressing_c         = ImGui::IsKeyPressed(SDL_SCANCODE_C);
            const bool pressing_x         = ImGui::IsKeyPressed(SDL_SCANCODE_X);
            const bool pressing_v         = ImGui::IsKeyPressed(SDL_SCANCODE_V);

            const bool pressing_up        = ImGui::IsKeyPressed(SDL_SCANCODE_UP);
            const bool pressing_down      = ImGui::IsKeyPressed(SDL_SCANCODE_DOWN);
            const bool pressing_left      = ImGui::IsKeyPressed(SDL_SCANCODE_LEFT);
            const bool pressing_right     = ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT);

            //const bool typed_text         = (ImGui::GetIO().InputQueueCharacters.Size != 0);
            std::string typed_text;
            {
                ImGuiIO& io = ImGui::GetIO();
                for(int i = 0; i < io.InputQueueCharacters.Size; ++i){
                    // Note: this conversion assumes the input is utf8. It will discard code points in other encodings.
                    // Imgui encodes chars, but does not (currently) provide a way to decode them in the public API.
                    // So we filter out non-utf8 codepoints.
                    const auto c_wchar = io.InputQueueCharacters[i];
                    const char c = (' ' <= c_wchar && c_wchar <= 255) ? static_cast<char>(c_wchar) : '?';
                    typed_text.push_back( c );
                }
            }

            bool resize_columns_to_default = false;
            bool resize_columns_to_fit = false;

            if(ImGui::Button("Create table")){
                DICOM_data.table_data.emplace_back( std::make_shared<Sparse_Table>() );
                table_display.table_num = DICOM_data.table_data.size() - 1;

                table_selection.clear();
                cell_being_edited = {};
                cell_being_edited_first_frame = 0L;
                cell_selected = {};
                resize_columns_to_default = true;
            }
            ImGui::SameLine();
            if(ImGui::Button("Remove table")){
                auto [table_is_valid, table_ptr_it] = recompute_table_iters();
                if(table_is_valid){
                    DICOM_data.table_data.erase( table_ptr_it );
                    table_display.table_num -= 1;

                    table_selection.clear();
                    cell_being_edited = {};
                    cell_being_edited_first_frame = 0L;
                    cell_selected = {};
                    resize_columns_to_default = true;
                }
            }

            // Scroll through tables.
            if(DICOM_data.Has_Table_Data()){
                int scroll_tables = table_display.table_num;
                const int N_tables = DICOM_data.table_data.size();
                ImGui::SliderInt("Table", &scroll_tables, 0, N_tables - 1);
                const int64_t new_table_num = std::clamp<int>(scroll_tables, 0, N_tables - 1);
                if(new_table_num != table_display.table_num){
                    table_display.table_num = new_table_num;

                    table_selection.clear();
                    cell_being_edited = {};
                    cell_being_edited_first_frame = 0L;
                    cell_selected = {};
                    resize_columns_to_default = true;
                }
            }

            ImGui::Separator();
            {
                const bool l_resize_columns_to_fit = ImGui::Button("Resize columns to fit contents");
                ImGui::SameLine();
                const bool l_resize_columns_to_default = ImGui::Button("Resize columns to default");

                resize_columns_to_fit = resize_columns_to_fit ? true : l_resize_columns_to_fit;
                resize_columns_to_default = resize_columns_to_default ? true : l_resize_columns_to_default;
            }

            ImGui::Checkbox("Keyword highlighting", &table_display.use_keyword_highlighting);
            if( ImGui::IsItemHovered() ){
                ImGui::BeginTooltip();
                std::stringstream ss;
                ss << "Keywords: ";
                for(const auto& kvp : table_display.colours){
                    ss << "'" << kvp.first << "' ";
                }
                ImGui::Text("%s", ss.str().c_str());
                ImGui::EndTooltip();
            }
            ImGui::Separator();

            // Display the table.
            if( auto [table_is_valid, table_ptr_it] = recompute_table_iters();  table_is_valid ){
                const auto [tbl_min_col, tbl_max_col] = (*table_ptr_it)->table.standard_min_max_col();
                const auto [tbl_min_row, tbl_max_row] = (*table_ptr_it)->table.standard_min_max_row();

                // ImGui currently has a 64-column limit, so truncate extra columns.
                // Play it safe with excess rows too.
                auto l_min_col = tbl_min_col;
                auto l_max_col = tbl_max_col;
                auto l_min_row = tbl_min_row;
                auto l_max_row = tbl_max_row;

                l_min_col = std::clamp<int64_t>(l_min_col, tbl_min_col, tbl_max_col);
                l_max_col = std::clamp<int64_t>(l_max_col, l_min_col, std::min<int64_t>(tbl_max_col, l_min_col + 63));

                l_min_row = std::clamp<int64_t>(l_min_row, tbl_min_row, tbl_max_row);
                l_max_row = std::clamp<int64_t>(l_max_row, l_min_row, std::min<int64_t>(tbl_max_row, l_min_row + 19'999));

                if( (l_min_col != tbl_min_col)
                ||  (l_max_col != tbl_max_col) 
                ||  (l_min_row != tbl_min_row)
                ||  (l_max_row != tbl_max_row) ){
                    ImGui::Text("Warning: this table is truncated for display purposes");
                    ImGui::Separator();
                }

                const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
                const float TEXT_BASE_HEIGHT = ImGui::CalcTextSize("A").y;
                if( (tbl_min_col < tbl_max_col)
                &&  (tbl_min_row < tbl_max_row)
                &&  (l_min_col < l_max_col)
                &&  (l_min_row < l_max_row)
                &&  ImGui::BeginTable("Table display",
                                     (l_max_col - l_min_col) + 1,
                                     ImGuiTableFlags_Borders
                                       | ImGuiTableFlags_NoSavedSettings
                                       //| ImGuiWindowFlags_NoNavInputs
                                       //| ImGuiWindowFlags_NoNav
                                       //| ImGuiTableFlags_ScrollX
                                       //| ImGuiTableFlags_ScrollY
                                       | ImGuiTableFlags_ScrollX
                                       | ImGuiTableFlags_ScrollY
                                       | ImGuiTableFlags_RowBg
                                       | ImGuiTableFlags_BordersV
                                       | ImGuiTableFlags_BordersInnerV
                                       | ImGuiTableFlags_BordersOuterV
                                       | ImGuiTableFlags_SizingFixedFit
                                       //| ImGuiTableFlags_SizingFixedSame
                                       //| ImGuiTableFlags_PadOuterX
                                       //| ImGuiTableFlags_NoHostExtendX
                                       //| ImGuiTableFlags_NoHostExtendY
                                       | ImGuiTableFlags_Hideable
                                       | ImGuiTableFlags_Reorderable
                                       | ImGuiTableFlags_Resizable )){
                                     //ImVec2(TEXT_BASE_WIDTH * 50, 100.0f) )){

                    // Number the columns.
                    const float default_col_width = 70.0f;
                    const float min_col_width = TEXT_BASE_WIDTH * 3.0f;
                    {
                        for(int64_t c = l_min_col; c <= l_max_col; ++c){
                            std::stringstream ss;
                            ss << std::setw(3) << std::to_string(c);
                            ImGui::TableSetupColumn(ss.str().c_str(),
                                                    ImGuiTableColumnFlags_WidthFixed,
                                                    default_col_width);
                        }
                    }
                    ImGui::TableSetupScrollFreeze(0, 1); // Lock the column numbers onto the top when scrolling.
                    ImGui::TableHeadersRow();

                    std::array<char, 2048> buf;
                    string_to_array(buf, "");

                    // Resize column widths.
                    if(false){
                    }else if(resize_columns_to_default){
                        for(int64_t col = l_min_col; col <= l_max_col; ++col){
                            ImGui::TableSetColumnWidth(col, default_col_width);
                        }

                    }else if(resize_columns_to_fit){
                        std::map<int64_t, float> col_width;
                        tables::visitor_func_t f_size = [&](int64_t row, int64_t col, std::string& v) -> tables::action {
                            if( (l_min_col <= col) && (col <= l_max_col)
                            &&  (l_min_row <= row) && (row <= l_max_row) ){
                                const auto truncated_v = v.substr(std::size_t{0}, std::min<size_t>(v.size(), buf.size()));
                                // Leave a bit of extra space, which makes it easier to locate the cursor when editing.
                                const auto w = ImGui::CalcTextSize(v.c_str()).x + TEXT_BASE_WIDTH * 2.0f;
                                col_width[col] = std::max<float>(col_width[col], min_col_width);
                                col_width[col] = std::max<float>(col_width[col], w);
                            }
                            return tables::action::automatic; // Retain only non-empty cells.
                        };
                        (*table_ptr_it)->table.visit_standard_block(f_size);

                        for(int64_t col = l_min_col; col <= l_max_col; ++col){
                            ImGui::TableSetColumnWidth(col, col_width[col]);
                        }
                    }

                    // Eliminate the gap between cells to eliminate dead zones in the grid where the mouse cannot click.
                    // (Some elements like ImGui::Selectable() account for this gap, but ImGui::InvisibleButton()
                    // currently does not.)
                    const auto cell_padding = ImGui::GetStyle().CellPadding;
                    const auto frame_padding = ImGui::GetStyle().FramePadding;

                    // Hide the default keyboard navigation.
                    // Ideally we would be able to disable it, but the functionality is not exposed to the public API.
                    const ImVec4 hidden_colour(1.0f, 1.0f, 1.0f, 0.0f);
                    ImGui::PushStyleColor(ImGuiCol_NavHighlight, hidden_colour);
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hidden_colour);
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, hidden_colour);

                    // Visit each cell and render the contents as an InputText widget.
                    tables::visitor_func_t f = [&](int64_t row, int64_t col, std::string& v) -> tables::action {
                        if( (l_min_col <= col) && (col <= l_max_col)
                        &&  (l_min_row <= row) && (row <= l_max_row) ){
                        //&&  ImGui::TableNextColumn() ){
                            ImGui::TableNextColumn();
                            string_to_array(buf, v);
                            const bool buf_holds_full_v = ((v.size() + 1U) < buf.size());

                            // This ID ensures the table can grow with cells retaining their ID's. It splits an int32_t into two
                            // ranges, allowing rows to span [0,100'000] and columns to span [0,max_int32_t/100'000] =
                            // [0,20'000].
                            int cell_ID = (row - l_min_row) + (col - l_min_col) * 100'000;
                            ImGui::PushID(cell_ID);

                            const auto available_space = ImGui::GetContentRegionAvail();
                            std::optional<ImVec2> cell_actual_pos_min;
                            std::optional<ImVec2> cell_actual_pos_max;

                            const auto cell_rc_coords = std::make_pair(row, col);
                            const bool is_selected = cell_selected && (cell_selected.value() == cell_rc_coords);
                            const bool is_group_selected = (table_selection.count( cell_rc_coords ) != 0UL);
                            const bool is_being_edited = cell_being_edited ? (cell_being_edited.value() == cell_rc_coords) : false;
                            bool key_changed = false;
                            if(is_being_edited){
                                // Draw editable text.
                                if( 0L < cell_being_edited_first_frame ){
                                    ImGui::SetKeyboardFocusHere();
                                }
                                ImGui::SetNextItemWidth( available_space.x );
                                key_changed = ImGui::InputText("##datum", buf.data(), buf.size() - 1);

                                // Check if still editing (focus + active). If not, stop editing in the next frame.
                                const bool still_editing = !ImGui::IsItemDeactivated();

                                if( is_selected
                                &&  ImGui::IsItemVisible() ){
                                    cell_actual_pos_min = ImGui::GetItemRectMin();
                                    cell_actual_pos_max = ImGui::GetItemRectMax();
                                }

                                if(false){
                                }else if( 0L < cell_being_edited_first_frame ){
                                    // Debounce, needed because these keypresses can cycle to the next cell.
                                    //
                                    // This is integer rather than Boolean because after tabbing between cells imgui
                                    // needs to move to and render the next cell. The flag indicates the newly focused
                                    // cell has just been opened for editing, but keyboard focus may have already been
                                    // stolen this frame. So we need to skip a frame.
                                    --cell_being_edited_first_frame;

                                }else if( pressing_tab && pressing_shift ){
                                    cell_being_edited = std::make_pair(row, col - 1L);
                                    cell_being_edited_first_frame += 2L;
                                    cell_selected = cell_being_edited;
                                    table_selection.erase(cell_rc_coords);

                                }else if( pressing_tab ){
                                    cell_being_edited = std::make_pair(row, col + 1L);
                                    cell_being_edited_first_frame += 2L;
                                    cell_selected = cell_being_edited;
                                    table_selection.erase(cell_rc_coords);

                                }else if( pressing_enter && pressing_shift ){
                                    cell_being_edited = std::make_pair(row - 1L, col);
                                    cell_being_edited_first_frame += 2L;
                                    cell_selected = cell_being_edited;
                                    table_selection.erase(cell_rc_coords);

                                }else if( pressing_enter ){
                                    cell_being_edited = std::make_pair(row + 1L, col);
                                    cell_being_edited_first_frame += 2L;
                                    cell_selected = cell_being_edited;
                                    table_selection.erase(cell_rc_coords);

                                }else if(!still_editing){
                                    cell_being_edited = {};

                                }

                            }else{
                                // Draw selectable text.
                                const ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_None;
                                //const auto col_width = ImGui::TableGetColumnWidth(col - l_min_col);
                                const ImVec2 selectable_size( 0.0f, TEXT_BASE_HEIGHT + cell_padding.y);
                                
                                if(ImGui::Selectable(buf.data(), is_selected, selectable_flags, selectable_size)){
                                    cell_selected = cell_rc_coords;

                                    if(false){
                                    }else if(pressing_shift){
                                        // Rectangular selection.
                                        table_selection.insert(cell_rc_coords);
                                        const auto [row_bounds, col_bounds] = get_table_selection_bounds(table_selection).value();
                                        for(int64_t r = row_bounds.first; r <= row_bounds.second; ++r){
                                            for(int64_t c = col_bounds.first; c <= col_bounds.second; ++c){
                                                table_selection.insert( std::make_pair(r, c) );
                                            }
                                        }

                                    }else if( pressing_ctrl ){
                                        // Toggle selection for one cell.
                                        if(is_group_selected){
                                            table_selection.erase(cell_rc_coords);
                                        }else{
                                            table_selection.insert(cell_rc_coords);
                                        }

                                    }else{
                                        // Exclusive selection of one cell.
                                        table_selection.clear();
                                        table_selection.insert(cell_rc_coords);
                                    }
                                }

                                // Move navigation focus to the highlighted cell iff directed.
                                if( set_focus_on_cell
                                &&  (set_focus_on_cell.value() == cell_rc_coords) ){
                                    ImGui::SetScrollHereX();
                                    ImGui::SetScrollHereY();

                                    set_focus_on_cell = {};
                                }

                                // Set bounding box coordinates for the cell.
                                if( is_selected
                                &&  ImGui::IsItemVisible() ){
                                    cell_actual_pos_min = ImGui::GetItemRectMin();
                                    ImVec2 rect_max = ImGui::GetItemRectMax();
                                    //cell_actual_pos_max = ImVec2(rect_max.x, rect_max.y + frame_padding.y);
                                    cell_actual_pos_max = ImVec2(rect_max.x, rect_max.y);
                                }

                                // Check if text is hovered, active, and the mouse was double-clicked.
                                // If so, next frame we will draw it in an editable (non-selectable) box instead.
                                bool is_double_clicked = false;
                                for(int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++){
                                    if(ImGui::IsMouseDoubleClicked(i)){
                                        is_double_clicked = true;
                                    }
                                }
                                const bool is_now_editing_mouse =   ImGui::IsItemActive()
                                                                 && ImGui::IsItemHovered()
                                                                 && ImGui::IsItemVisible()
                                                                 && ImGui::IsItemClicked()
                                                                 && is_double_clicked;
                                const bool is_now_editing_keybd =   window_is_focused
                                                                 && ImGui::IsItemVisible()
                                                                 && is_selected
                                                                 && ( !typed_text.empty()
                                                                      || pressing_enter );
                                if( is_now_editing_mouse
                                ||  is_now_editing_keybd ){
                                    cell_being_edited = cell_rc_coords;
                                    ++cell_being_edited_first_frame;
                                    table_selection.erase(cell_rc_coords);
                                }
                                //if(is_now_editing_keybd){
                                //    // Prime the cell with the typed character.
                                //    v = typed_text;
                                //}
                            }

                            // Colourize if keywords are present.
                            if(table_display.use_keyword_highlighting){
                                for(const auto &kv : table_display.colours){
                                    if(v == kv.first){
                                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(kv.second));
                                        break;
                                    }
                                }
                            }

                            // Colourize if selected.
                            if(is_group_selected){
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(table_display.selected_colour));
                            }
                            if( is_selected
                            &&  cell_actual_pos_min
                            &&  cell_actual_pos_max ){
                                auto drawlist = ImGui::GetWindowDrawList();
                                drawlist->AddRect(cell_actual_pos_min.value(),
                                                  cell_actual_pos_max.value(),
                                                  IM_COL32(255, 255, 0, 255));
                            }

                            ImGui::PopID();

                            if( key_changed 
                            &&  buf_holds_full_v ) array_to_string(v, buf);
                        }
                        return tables::action::automatic; // Retain only non-empty cells.
                    };
                    (*table_ptr_it)->table.visit_standard_block(f);

                    ImGui::PopStyleColor(); // ImGuiCol_NavHighlight
                    ImGui::PopStyleColor(); // ImGuiCol_HeaderHovered
                    ImGui::PopStyleColor(); // ImGuiCol_HeaderActive


                    // Helper function for jump navigation.
                    const auto insert_cells_between = [&](const tables::cell_coord_t &A,
                                                          const tables::cell_coord_t &B) -> void {
                        const auto min_row = std::min(A.first, B.first);
                        const auto max_row = std::max(A.first, B.first);
                        const auto min_col = std::min(A.second, B.second);
                        const auto max_col = std::max(A.second, B.second);

                        for(auto row = min_row; row <= max_row; ++row){
                            for(auto col = min_col; col <= max_col; ++col){
                                const auto p = std::make_pair(row, col);
                                table_selection.insert( p );
                            }
                        }
                        return;
                    };

                    // Check for keyboard actions.
                    if(  window_is_focused ){
                        // Delete the selection.
                        if(false){
                        }else if( (pressing_delete || pressing_backspace )
                              &&  !table_selection.empty() ){
                            for(const auto& c : table_selection){
                                 const auto [row, col] = c;
                                 (*table_ptr_it)->table.remove(row, col);
                            }

                        // Copy the selection.
                        }else if( pressing_ctrl
                              &&  pressing_c
                              &&  !table_selection.empty() ){
                            const auto [row_bounds, col_bounds] = get_table_selection_bounds(table_selection).value();
                            std::ostringstream os;
                            (*table_ptr_it)->table.write_csv(os, '\t', row_bounds, col_bounds);
                            const auto selection_csv = os.str();
                            ImGui::SetClipboardText(selection_csv.c_str());
                            YLOGINFO("Copied rectangular selection to clipboard");

                        // Paste the selection.
                        }else if( pressing_ctrl
                              &&  pressing_v
                              &&  cell_selected ){
                            char *c_txt = SDL_GetClipboardText();
                            const std::string txt(c_txt);
                            SDL_free(c_txt);
                            try{
                                tables::table2 t;
                                std::stringstream ss(txt);
                                t.read_csv( ss );
                                const auto mmr = t.min_max_row();
                                const auto mmc = t.min_max_col();
                                const auto [row_offset, col_offset] = cell_selected.value();
                                tables::visitor_func_t l_f = [&](int64_t row, int64_t col, std::string& v) -> tables::action {
                                    (*table_ptr_it)->table.inject(row - mmr.first + row_offset,
                                                                  col - mmc.first + col_offset, v);
                                    return tables::action::automatic;
                                };
                                //Visit all cells, so we overwrite even if the pasted cell is empty.
                                t.visit_block(mmr, mmc, l_f);
                                YLOGINFO("Pasted rectangular region to clipboard");

                            }catch(const std::exception &e){
                                YLOGWARN("Unable to parse tabular data from clipboard: " << e.what());
                            }

                        // Jump navigation over multiple cells, optionally adding to the selection.
                        }else if( pressing_ctrl
                              &&  cell_selected
                              &&  pressing_up ){
                            const auto inc = std::make_pair<int64_t,int64_t>(-1L, 0L);
                            const auto jump = (*table_ptr_it)->table.jump_navigate(cell_selected.value(), inc);
                            if( pressing_shift ) insert_cells_between(cell_selected.value(), jump);
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;
                        }else if( pressing_ctrl
                              &&  cell_selected
                              &&  pressing_down ){
                            const auto inc = std::make_pair<int64_t,int64_t>(1L, 0L);
                            const auto jump = (*table_ptr_it)->table.jump_navigate(cell_selected.value(), inc);
                            if( pressing_shift ) insert_cells_between(cell_selected.value(), jump);
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;
                        }else if( pressing_ctrl
                              &&  cell_selected
                              &&  pressing_left ){
                            const auto inc = std::make_pair<int64_t,int64_t>(0L, -1L);
                            const auto jump = (*table_ptr_it)->table.jump_navigate(cell_selected.value(), inc);
                            if( pressing_shift ) insert_cells_between(cell_selected.value(), jump);
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;
                        }else if( pressing_ctrl
                              &&  cell_selected
                              &&  pressing_right ){
                            const auto inc = std::make_pair<int64_t,int64_t>(0L, 1L);
                            const auto jump = (*table_ptr_it)->table.jump_navigate(cell_selected.value(), inc);
                            if( pressing_shift ) insert_cells_between(cell_selected.value(), jump);
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;

                        // Navigate the selected cell one cell over, optionally adding to the selection.
                        }else if( cell_selected
                              &&  pressing_up ){
                            const auto [row, col] = cell_selected.value();
                            const auto jump = std::make_pair(std::clamp(row - 1L, l_min_row, l_max_row), col);
                            if( pressing_shift ){
                                table_selection.insert(cell_selected.value());
                                table_selection.insert(jump);
                            }
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;
                        }else if( cell_selected
                              &&  pressing_down ){
                            const auto [row, col] = cell_selected.value();
                            const auto jump = std::make_pair(std::clamp(row + 1L, l_min_row, l_max_row), col);
                            if( pressing_shift ){
                                table_selection.insert(cell_selected.value());
                                table_selection.insert(jump);
                            }
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;
                        }else if( cell_selected
                              &&  pressing_left ){
                            const auto [row, col] = cell_selected.value();
                            const auto jump = std::make_pair(row, std::clamp(col - 1L, l_min_col, l_max_col));
                            if( pressing_shift ){
                                table_selection.insert(cell_selected.value());
                                table_selection.insert(jump);
                            }
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;
                        }else if( cell_selected
                              &&  pressing_right ){
                            const auto [row, col] = cell_selected.value();
                            const auto jump = std::make_pair(row, std::clamp(col + 1L, l_min_col, l_max_col));
                            if( pressing_shift ){
                                table_selection.insert(cell_selected.value());
                                table_selection.insert(jump);
                            }
                            cell_selected = jump;
                            set_focus_on_cell = cell_selected;

                        }
                    }

                    ImGui::EndTable();
                }

                // Display metadata.
                if( view_toggles.view_table_metadata_enabled ){
                    ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                    ImGui::Begin("Table Metadata", &view_toggles.view_table_metadata_enabled);

                    display_metadata_table( (*table_ptr_it)->table.metadata );

                    ImGui::End();
                }

                ImGui::End();
            }
            return;
        };
        try{
            display_tables();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_tables(): '" << e.what() << "'");
            throw;
        }

        // Display RT plans.
        const auto display_rtplans = [&view_toggles,
                                      &drover_mutex,
                                      &mutex_dt,
                                      &rtplan_num,
                                      &rtplan_dynstate_num,
                                      &rtplan_statstate_num,
                                      &recompute_rtplan_iters,
                                      &display_metadata_table,
                                      &DICOM_data ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_rtplans_enabled
            ||  !DICOM_data.Has_RTPlan_Data() ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 40), ImGuiCond_FirstUseEver);
            ImGui::Begin("RT Plans", &view_toggles.view_rtplans_enabled);

            // Scroll through RT plans.
            if(DICOM_data.Has_RTPlan_Data()){
                int scroll_rtplans = rtplan_num;
                const int N_rtplans = DICOM_data.rtplan_data.size();
                ImGui::SliderInt("Plan", &scroll_rtplans, 0, N_rtplans - 1);
                const int64_t new_rtplan_num = std::clamp<int>(scroll_rtplans, 0, N_rtplans - 1);
                if(new_rtplan_num != rtplan_num){
                    rtplan_num = new_rtplan_num;
                }
            }

            ImGui::Checkbox("View RT plan metadata", &view_toggles.view_rtplan_metadata_enabled);

            if(auto [rtplan_is_valid, rtplan_ptr_it] = recompute_rtplan_iters(); rtplan_is_valid){

                // Display the RT plan.
                //
                // Note: we currently only display the top-level metadata without any visual display.
                ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
                ImGui::Begin("RT Plan", &view_toggles.view_rtplans_enabled);
                display_metadata_table( (*rtplan_ptr_it)->metadata );
                ImGui::End();

                if( view_toggles.view_rtplan_metadata_enabled
                &&  !(*rtplan_ptr_it)->dynamic_states.empty() ){

                    // Scroll through dynamic states.
                    int scroll_dynstate = rtplan_dynstate_num;
                    const int N_dynstates = (*rtplan_ptr_it)->dynamic_states.size();
                    ImGui::SliderInt("Beam", &scroll_dynstate, 0, N_dynstates - 1);
                    const int64_t new_dynstate_num = std::clamp<int>(scroll_dynstate, 0, N_dynstates - 1);
                    if(new_dynstate_num != rtplan_dynstate_num){
                        rtplan_dynstate_num = new_dynstate_num;
                    }
                    auto *dynstate_ptr = &( (*rtplan_ptr_it)->dynamic_states.at(rtplan_dynstate_num) );

                    // Display the selected dynamic state (i.e., the beam).
                    {
                        std::stringstream ss;
                        ss << "Beam number: " << std::to_string(dynstate_ptr->BeamNumber) << std::endl;
                        ss << "Cumulative meterset: " << std::to_string(dynstate_ptr->FinalCumulativeMetersetWeight) << std::endl;
                        ss << "Number of control points: " << std::to_string(dynstate_ptr->static_states.size()) << std::endl;
                        ImGui::Text("%s", ss.str().c_str());
                    }
                    if(view_toggles.view_rtplan_metadata_enabled){
                        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
                        ImGui::SetNextWindowPos(ImVec2(80, 80), ImGuiCond_FirstUseEver);
                        ImGui::Begin("Beam view", &view_toggles.view_rtplan_metadata_enabled);
                        display_metadata_table( dynstate_ptr->metadata );
                        ImGui::End();
                    }

                    if( !dynstate_ptr->static_states.empty() ){
                        // Scroll through static states.
                        int scroll_statstate = rtplan_statstate_num;
                        const int N_statstates = dynstate_ptr->static_states.size();
                        ImGui::SliderInt("Control point", &scroll_statstate, 0, N_statstates - 1);
                        const int64_t new_statstate_num = std::clamp<int>(scroll_statstate, 0, N_statstates - 1);
                        if(new_statstate_num != rtplan_statstate_num){
                            rtplan_statstate_num = new_statstate_num;
                        }
                        auto *statstate_ptr = &( dynstate_ptr->static_states.at(rtplan_statstate_num) );

                        // Display the selected static state (i.e., the control point).
                        {
                            std::stringstream ss;
                            ss << "Control point index: " << std::to_string(statstate_ptr->ControlPointIndex) << std::endl
                               << "Cumulative meterset: " << std::to_string(statstate_ptr->CumulativeMetersetWeight) << std::endl
                               << "Gantry angle: " << std::to_string(statstate_ptr->GantryAngle) << std::endl;
                            ImGui::Text("%s", ss.str().c_str());
                        }
                        if(view_toggles.view_rtplan_metadata_enabled){
                            ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
                            ImGui::SetNextWindowPos(ImVec2(120, 120), ImGuiCond_FirstUseEver);
                            ImGui::Begin("Control point view", &view_toggles.view_rtplan_metadata_enabled);
                            display_metadata_table( statstate_ptr->metadata );
                            ImGui::End();
                        }
                    }
                }
            }

            ImGui::End();
            return;
        };
        try{
            display_rtplans();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_rtplans(): '" << e.what() << "'");
            throw;
        }

        // Display feature selection dialog.
        const auto display_feat_sel = [&view_toggles,
                                       &drover_mutex,
                                       &mutex_dt,
                                       &display_metadata_table,
                                       &DICOM_data,
                                       &img_features,
                                       &feature_transform_num ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_image_feature_extraction ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(450, 375), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 410), ImGuiCond_FirstUseEver);
            ImGui::Begin("Image Feature Selection", &view_toggles.view_image_feature_extraction);

            ImGui::DragFloat("Snap distance", &img_features.snap_dist, 0.01f, 0.0f, 50.0f, "%.01f");

            string_to_array(img_features.buff, img_features.metadata_key);
            ImGui::InputText("Metadata key", img_features.buff.data(), img_features.buff.size());
            array_to_string(img_features.metadata_key, img_features.buff);

            string_to_array(img_features.buff, img_features.description);
            ImGui::InputText("Description", img_features.buff.data(), img_features.buff.size());
            array_to_string(img_features.description, img_features.buff);
            
            ImGui::Checkbox("Use colour override", &img_features.use_override_colour);
            ImGui::ColorEdit4("Override colour", img_features.o_col.data());

            {
                int i = 0;
                for(auto &pset : { &img_features.features_A, &img_features.features_B } ){
                    ImGui::PushID(i);
                    const auto pset_val_opt = get_as<std::string>(pset->metadata, img_features.metadata_key);

                    std::stringstream ss;
                    ss << "Image array " << ++i << ":" << std::endl;
                    ss << "  Features: " << pset->points.size() << std::endl;
                    ss << "  Key value: " << pset_val_opt.value_or("N/A") << std::endl;
                    ImGui::Separator();
                    ImGui::Text("%s", ss.str().c_str());
                    if(ImGui::Button("Save feature snapshot")){
                        DICOM_data.point_data.emplace_back( std::make_shared<Point_Cloud>() );
                        DICOM_data.point_data.back()->pset = (*pset);
                        if(!img_features.description.empty()){
                            DICOM_data.point_data.back()->pset.metadata["Description"] = img_features.description;
                        }
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Delete features")){
                        (*pset) = point_set<double>();
                    }
                    ImGui::SameLine();
                    const std::string override_popup_name = "Edit Features (set " + std::to_string(i) + ")";
                    if(ImGui::Button("Edit features")){
                        ImGui::OpenPopup(override_popup_name.c_str());
                    }

                    if(ImGui::BeginPopupModal(override_popup_name.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)){
                        int j = 0;
                        if(ImGui::BeginChild("##feature list", ImVec2(800, 400))){
                            auto space = ImGui::GetContentRegionAvail();
                            ImGui::PushItemWidth(space.x*0.25 - 1.0);
                            for(auto& v : pset->points){
                                ImGui::PushID(++j);
                                std::string point_label = "feature " + std::to_string(j) + ":";
                                ImGui::Text("%s", point_label.c_str());
                                ImGui::SameLine();
                                ImGui::InputDouble("##x", &(v.x));
                                ImGui::SameLine();
                                ImGui::InputDouble("##y", &(v.y));
                                ImGui::SameLine();
                                ImGui::InputDouble("##z", &(v.z));
                                ImGui::PopID();
                            }
                            ImGui::PopItemWidth();
                        }
                        ImGui::EndChild(); // NOTE: BeginChild()/ EndChild() are unique. Must always call EndChild()!

                        if(ImGui::Button("Add feature")){
                            pset->points.push_back( vec3<double>(0.0, 0.0, 0.0) );
                        }
                        ImGui::SameLine();
                        if(ImGui::Button("Delete feature")){
                            if(!pset->points.empty()){
                                pset->points.pop_back();
                            }
                        }
                        ImGui::SameLine();
                        if(ImGui::Button("Done")){
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
            ImGui::Separator();

            if(ImGui::Button("Swap feature sets (1 <-> 2)")){
                std::swap(img_features.features_A, img_features.features_B);
            }
            ImGui::SameLine();
            if(ImGui::Button("Duplicate to empty set")){
                const bool feats_A_empty = img_features.features_A.points.empty();
                const bool feats_B_empty = img_features.features_B.points.empty();
                if( feats_A_empty && feats_B_empty ){
                    YLOGWARN("Both feature sets are empty");
                }else if( feats_A_empty ){
                    img_features.features_A = img_features.features_B;
                }else if( feats_B_empty ){
                    img_features.features_B = img_features.features_A;
                }else{
                    YLOGWARN("Neither feature set is empty");
                }
            }

            ImGui::Separator();

            std::vector<std::string> tform_names { 
#ifdef DCMA_USE_EIGEN
                "rigid (orthogonal procrustes with isotropic scaling)",
                "rigid (orthogonal procrustes without isotropic scaling)",
#endif // DCMA_USE_EIGEN
                "centroid translation",
                "PCA",
                "ICP", };
            const size_t N_tforms = tform_names.size();
            feature_transform_num = std::clamp<size_t>(feature_transform_num, 0U, N_tforms - 1U);

            ImGuiComboFlags flags = 0;
            if(ImGui::BeginCombo("Registration Method", tform_names.at(feature_transform_num).c_str(), flags)){
                for(size_t i = 0U; i < N_tforms; i++){
                    const bool is_selected = (i == feature_transform_num);
                    if(ImGui::Selectable(tform_names.at(i).c_str(), is_selected)){
                        feature_transform_num = i;
                    }
                    if(is_selected){
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const bool make_tform = ImGui::Button("Generate transform (1 -> 2)");
            if( ImGui::IsItemHovered() 
            &&  (img_features.features_A.points.size() != img_features.features_B.points.size()) ){
                ImGui::BeginTooltip();
                ImGui::Text("Not recommended -- features are currently mismatched");
                ImGui::EndTooltip();
            }
            if( make_tform ){
                try{
                    const std::string TransformName = "unspecified";
                    std::optional<affine_transform<double>> tform;
                    size_t i = 0U;
                    if(false){
#ifdef DCMA_USE_EIGEN
                    }else if(i++ == feature_transform_num){
                        AlignViaOrthogonalProcrustesParams params;
                        params.permit_mirroring = false;
                        params.permit_isotropic_scaling = true;

                        tform = AlignViaOrthogonalProcrustes(params, img_features.features_A, img_features.features_B);

                    }else if(i++ == feature_transform_num){
                        AlignViaOrthogonalProcrustesParams params;
                        params.permit_mirroring = false;
                        params.permit_isotropic_scaling = false;

                        tform = AlignViaOrthogonalProcrustes(params, img_features.features_A, img_features.features_B);
#endif // DCMA_USE_EIGEN
                    }else if(i++ == feature_transform_num){
                        tform = AlignViaCentroid(img_features.features_A, img_features.features_B);

                    }else if(i++ == feature_transform_num){
                        tform = AlignViaPCA(img_features.features_A, img_features.features_B);

                    }else if(i++ == feature_transform_num){
                        tform = AlignViaExhaustiveICP(img_features.features_A, img_features.features_B);

                    }else{
                        // This is just to help keep the tform list and implementation synchronized.
                        YLOGWARN("Registration technique not understood");
                    }

                    if(!tform){
                        throw std::runtime_error("(no explanation available)");
                    }

                    metadata_multimap_t cmm;
                    combine_distinct(cmm, img_features.features_A.metadata);
                    combine_distinct(cmm, img_features.features_B.metadata);
                    auto cm = singular_keys(cmm);
                    cm = coalesce_metadata_for_basic_def_reg(cm);

                    DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
                    DICOM_data.trans_data.back()->transform = tform.value();
                    DICOM_data.trans_data.back()->metadata = cm;
                    DICOM_data.trans_data.back()->metadata["TransformName"] = TransformName;

                    // Apply transform to features A to compare with features B for inspection.
                    img_features.features_C = img_features.features_A;
                    const auto B_val_opt = get_as<std::string>(img_features.features_B.metadata, img_features.metadata_key);
                    if(B_val_opt) img_features.features_C.metadata[img_features.metadata_key] = B_val_opt.value();
                    for(auto& p : img_features.features_C.points){
                        tform.value().apply_to(p);
                    }

                }catch(const std::exception &e){
                    YLOGWARN("Unable to create transformation: " << e.what());
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Delete transformed features")){
                img_features.features_C = point_set<double>();
            }


            ImGui::End();
            return;
        };
        try{
            display_feat_sel();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_feat_sel(): '" << e.what() << "'");
            throw;
        }

        // Display Point Sets.
        const auto display_psets = [&view_toggles,
                                    &drover_mutex,
                                    &mutex_dt,
                                    &pset_num,
                                    &recompute_pset_iters,
                                    &display_metadata_table,
                                    &DICOM_data ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_psets_enabled
            ||  !DICOM_data.Has_Point_Data() ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 140), ImGuiCond_FirstUseEver);
            ImGui::Begin("Point Sets", &view_toggles.view_psets_enabled);

            // Scroll through point sets.
            if(DICOM_data.Has_Point_Data()){
                int scroll_psets = pset_num;
                const int N_psets = DICOM_data.point_data.size();
                ImGui::SliderInt("Set", &scroll_psets, 0, N_psets - 1);
                const int64_t new_pset_num = std::clamp<int>(scroll_psets, 0, N_psets - 1);
                if(new_pset_num != pset_num){
                    pset_num = new_pset_num;
                }
            }

            ImGui::Checkbox("View point set metadata", &view_toggles.view_psets_metadata_enabled);

            if(auto [pset_is_valid, pset_ptr_it] = recompute_pset_iters();
                view_toggles.view_psets_metadata_enabled && pset_is_valid ){
                ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(40, 140), ImGuiCond_FirstUseEver);
                ImGui::Begin("Point Set Metadata", &view_toggles.view_psets_metadata_enabled);
                display_metadata_table( (*pset_ptr_it)->pset.metadata );
                ImGui::End();
            }

            ImGui::End();
            return;
        };
        try{
            display_psets();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_psets(): '" << e.what() << "'");
            throw;
        }


        // Display Transforms.
        const auto display_tforms = [&view_toggles,
                                     &drover_mutex,
                                     &mutex_dt,
                                     &tform_num,
                                     &recompute_tform_iters,
                                     &display_metadata_table,
                                     &DICOM_data ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.view_tforms_enabled
            ||  !DICOM_data.Has_Tran3_Data() ) return;

            // Display a selection and navigation window.
            ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(680, 240), ImGuiCond_FirstUseEver);
            ImGui::Begin("Transform", &view_toggles.view_tforms_enabled);

            // Scroll through point sets.
            if(DICOM_data.Has_Tran3_Data()){
                int scroll_tforms = tform_num;
                const int N_tforms = DICOM_data.trans_data.size();
                ImGui::SliderInt("Transform", &scroll_tforms, 0, N_tforms - 1);
                const int64_t new_tform_num = std::clamp<int>(scroll_tforms, 0, N_tforms - 1);
                if(new_tform_num != tform_num){
                    tform_num = new_tform_num;
                }
            }

            ImGui::Checkbox("View transform metadata", &view_toggles.view_tforms_metadata_enabled);

            if(auto [tform_is_valid, tform_ptr_it] = recompute_tform_iters();
                view_toggles.view_tforms_metadata_enabled && tform_is_valid ){
                ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(40, 240), ImGuiCond_FirstUseEver);
                ImGui::Begin("Transform Metadata", &view_toggles.view_tforms_metadata_enabled);
                display_metadata_table( (*tform_ptr_it)->metadata );
                ImGui::End();
            }

            ImGui::End();
            return;
        };
        try{
            display_tforms();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_tforms(): '" << e.what() << "'");
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
                                               &img_is_rgb,
                                               &use_texture_antialiasing,

                                               &zoom,
                                               &pan,
                                               &uv_min,
                                               &uv_max,
                                               &image_mouse_pos_opt,
                                               &io,

                                               &recompute_cimage_iters,
                                               &contouring_drover_cache,
                                               &cdrover_ptr,
                                               &create_cdrover_snapshot,
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

                                               &frame_count,

                                               &img_features ]() -> void {

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
            bool scroll_is_rgb = img_is_rgb;
            bool scroll_use_tex_aa = use_texture_antialiasing;
            {
                //ImVec2 window_extent = ImGui::GetContentRegionAvail();

                ImGui::Text("Image selection");
                const int N_arrays = DICOM_data.image_data.size();
                const int N_images = (*img_array_ptr_it)->imagecoll.images.size();
                //ImGui::SetNextItemWidth(window_extent.x);
                ImGui::SliderInt("Array", &scroll_arrays, 0, N_arrays - 1);
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Shortcut: shift + mouse wheel, (up)/(down), shift + n/p, or shift + home/end");
                    ImGui::EndTooltip();
                }
                //ImGui::SetNextItemWidth(window_extent.x);
                ImGui::SliderInt("Image", &scroll_images, 0, N_images - 1);
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Shortcut: mouse wheel, (left)/(right), n/p, page-up/page-down, or home/end");
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
                ImGui::DragFloat("Zoom level", &zoom, 0.01f, 1.0f, 100.0f, "%.03f");
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Note: use [ctrl] and mouse wheel to zoom.");
                    ImGui::EndTooltip();
                }
                zoom = std::clamp<float>(zoom, 1.0f, 1000.0f);
                const float uv_width = 1.0f / zoom;
                ImGui::DragFloat("Pan horizontal", &pan.x, 0.01f, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f, "%.03f");
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Note: click and hold the mouse wheel and drag to pan while zoomed.");
                    ImGui::EndTooltip();
                }
                ImGui::DragFloat("Pan vertical",   &pan.y, 0.01f, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f, "%.03f");
                if( ImGui::IsItemHovered() ){
                    ImGui::BeginTooltip();
                    ImGui::Text("Note: click and hold the mouse wheel and drag to pan while zoomed.");
                    ImGui::EndTooltip();
                }
                pan.x = std::clamp<float>(pan.x, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f);
                pan.y = std::clamp<float>(pan.y, 0.0f + uv_width * 0.5f, 1.0f - uv_width * 0.5f);
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
                const auto N_channels = static_cast<int>(disp_img_it->channels);
                ImGui::SliderInt("Channel", &scroll_channel, 0, N_channels - 1);
                ImGui::Checkbox("Use antialiasing", &scroll_use_tex_aa);
                if( 3 <= N_channels ){
                    ImGui::Checkbox("Image represents RGB colour", &scroll_is_rgb);
                }else{
                    scroll_is_rgb = false;
                }

                const bool pressing_ctrl_Z = (io.KeyCtrl && ImGui::IsKeyPressed( SDL_SCANCODE_Z ));
                const bool pressing_ctrl_Y = (io.KeyCtrl && ImGui::IsKeyPressed( SDL_SCANCODE_Y ));


                if( image_mouse_pos_opt.value().image_window_focused
                // Image navigation window focused, but image viewer window being hovered.
                ||  (ImGui::IsWindowFocused() && image_mouse_pos_opt.value().image_window_hovered)
                // Image viewer window being hovered and mouse being clicked.
                // (The window will not yet be focused, but this is needed for intercepting contouring
                // and drawing clicks, since the contouring window will be focused.)
                ||  (image_mouse_pos_opt.value().image_window_hovered && ImGui::IsAnyMouseDown()) ){

                    auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();

                    const int d_l = static_cast<int>( std::floor(io.MouseWheel) );
                    const int d_h = static_cast<int>( std::ceil(io.MouseWheel) );
                    if(false){
                    }else if(io.KeyCtrl && (0 < io.MouseWheel)){
                        zoom += std::log(zoom + 0.25f);
                        zoom = std::clamp<float>( zoom, 1.0f, 100.0f );
                    }else if(io.KeyCtrl && (io.MouseWheel < 0)){
                        zoom -= std::log(zoom + 0.25f);
                        zoom = std::clamp<float>( zoom, 1.0f, 100.0f );

                    }else if( (2 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (0.0f <= io.MouseDownDuration[2]) ){
                        pan.x -= static_cast<float>( io.MouseDelta.x ) / 600.0f;
                        pan.y -= static_cast<float>( io.MouseDelta.y ) / 600.0f;
                          
                    }else if( io.KeyShift && (0 < io.MouseWheel) ){
                        scroll_arrays = std::clamp<int>((scroll_arrays + N_arrays + d_h) % N_arrays, 0, N_arrays - 1);
                    }else if( io.KeyShift && (io.MouseWheel < 0) ){
                        scroll_arrays = std::clamp<int>((scroll_arrays + N_arrays + d_l) % N_arrays, 0, N_arrays - 1);

                    }else if( (io.KeyShift && ImGui::IsKeyPressed( SDL_SCANCODE_N ))
                          ||  ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_UpArrow )) ){
                        scroll_arrays = std::clamp<int>((scroll_arrays + 50 * N_arrays + 1) % N_arrays, 0, N_arrays - 1);
                    }else if( (io.KeyShift && ImGui::IsKeyPressed( SDL_SCANCODE_P ))
                          ||  ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_DownArrow )) ){
                        scroll_arrays = std::clamp<int>((scroll_arrays + 50 * N_arrays - 1) % N_arrays, 0, N_arrays - 1);

                    }else if( io.KeyShift && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Home)) ){
                        scroll_arrays = 0;
                    }else if( io.KeyShift && ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_End)) ){
                        scroll_arrays = N_arrays - 1;

                    }else if( (view_toggles.view_contouring_enabled && cimg_valid)
                          &&  (pressing_ctrl_Z || pressing_ctrl_Y) ){
                        // Contouring mode: undo and redo.
                        const auto v_list = contouring_drover_cache.get_versions();
                        const auto v_curr = contouring_drover_cache.get_version(cdrover_ptr);
                        const auto beg = std::begin(v_list);
                        const auto end = std::end(v_list);

                        auto it = std::find(beg, end, v_curr);
                        if(false){
                        }else if(pressing_ctrl_Z && (it != end) && (it != beg)){
                            it = std::next(it, -1);
                            cdrover_ptr = contouring_drover_cache.get(*it);
                        }else if(pressing_ctrl_Y && (it != end) && (std::next(it) != end)){
                            it = std::next(it, 1);
                            cdrover_ptr = contouring_drover_cache.get(*it);
                        }
                        std::tie(cimg_valid, cimg_array_ptr_it, cimg_it) = recompute_cimage_iters();
                        if(!cimg_valid){
                            throw std::runtime_error("Contouring undo operation failed, result is not valid");
                        }

                    }else if( (   (view_toggles.view_contouring_enabled && cimg_valid)
                               || (view_toggles.view_drawing_enabled && img_valid) )
                          &&  (0 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (1 < IM_ARRAYSIZE(io.MouseDown))
                          &&  ((0.0f <= io.MouseDownDuration[0]) || (0.0f <= io.MouseDownDuration[1]))
                          &&  image_mouse_pos_opt.value().mouse_hovering_image ){

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

                        const bool mouse_click_has_bounced =
                          (    ( (0.0f < io.MouseDownDuration[0]) && (io.MouseDownDuration[0] < 0.5f) )
                            || ( (0.0f < io.MouseDownDuration[1]) && (io.MouseDownDuration[1] < 0.5f) ) )
                          && last_mouse_button_pos
                          && image_mouse_pos_opt
                          && ((image_mouse_pos_opt.value().dicom_pos).distance( last_mouse_button_pos.value() ) < 1E-3);

                        do{
                            if(mouse_click_has_bounced){
                                YLOGINFO("Debouncing mouse click");
                                //last_mouse_button_pos = {};
                                break;
                            }

                            // If the mouse was just clicked down, then make a copy.
                            if( view_toggles.view_contouring_enabled
                            &&  ( (0.0f == io.MouseDownDuration[0]) || (0.0f == io.MouseDownDuration[1]) ) ){
                                YLOGINFO("Copying current contouring Drover state into undo buffer");
                                cdrover_ptr = create_cdrover_snapshot(contouring_drover_cache, cdrover_ptr);
                                std::tie(cimg_valid, cimg_array_ptr_it, cimg_it) = recompute_cimage_iters();
                                if(!cimg_valid){
                                    throw std::runtime_error("Copying contouring state into undo buffer failed, result is not valid");
                                }

                                // Trim old items in the undo buffer.
                                contouring_drover_cache.trim(10UL);
                            }

                            decltype(disp_img_it) l_img_it = (view_toggles.view_contouring_enabled) ? cimg_it : disp_img_it;
                            decltype(img_array_ptr_it) l_img_array_ptr_it = (view_toggles.view_contouring_enabled) ? cimg_array_ptr_it : img_array_ptr_it;

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
                            const float intensity = contouring_intensity;
                            const float intensity_min = (view_toggles.view_contouring_enabled) ?  0.0f : -inf;
                            const float intensity_max = (view_toggles.view_contouring_enabled) ?  1.0f :  inf;
                            const int64_t channel = 0;
                            const bool is_additive = mouse_button_0;
                            draw_with_brush( cimg_its, lss, contouring_brush, radius, intensity, channel, intensity_min,
                                             intensity_max, is_additive );

                            // Update mouse position for next time, if applicable.
                            if( mouse_button_0 ){
                                last_mouse_button_0_down = io.MouseDownDuration[0];
                                last_mouse_button_pos = image_mouse_pos_opt.value().dicom_pos;
                            }
                            if( mouse_button_1 ){
                                last_mouse_button_1_down = io.MouseDownDuration[1];
                                last_mouse_button_pos = image_mouse_pos_opt.value().dicom_pos;
                            }

                            if(view_toggles.view_contouring_enabled){
                                contouring_img_altered = true;
                            }else if(view_toggles.view_drawing_enabled){
                                need_to_reload_opengl_texture.store(true);
                            }
                        }while(false);

                    // Left-button mouse click on an image.
                    }else if( image_mouse_pos_opt.value().mouse_hovering_image
                          &&  img_valid
                          &&  image_mouse_pos_opt
                          &&  (0 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (0.0f == io.MouseDownDuration[0]) ){ // Debounced!

                        if(view_toggles.view_time_profiles){
                            view_toggles.save_time_profiles = true;

                        }else if(view_toggles.view_row_column_profiles){
                            view_toggles.save_row_column_profiles = true;

                        }else if(view_toggles.view_image_feature_extraction){
                            img_features.features_C = point_set<double>();

                            // Add a feature to the first matching or empty features point set.
                            const auto dicom_pos = image_mouse_pos_opt.value().dicom_pos;
                            const auto img_val_opt = get_as<std::string>(disp_img_it->metadata, img_features.metadata_key);

                            for(auto &pset : { &img_features.features_A, &img_features.features_B } ){
                                const auto pset_val_opt = get_as<std::string>(pset->metadata, img_features.metadata_key);
                                if( pset_val_opt
                                &&  (pset_val_opt != img_val_opt) ) continue;

                                auto existing = std::find_if( std::begin(pset->points),
                                                              std::end(pset->points),
                                                              [&](const vec3<double> &p) -> bool {
                                                                  return dicom_pos.distance(p) < img_features.snap_dist;
                                                              });
                                const auto is_near_existing = (existing != std::end(pset->points));
                                if(is_near_existing){
                                    // Do not add the point, and also do not move to next point set.
                                    break;
                                }

                                auto cm = (*img_array_ptr_it)->imagecoll.get_common_metadata({});
                                cm = coalesce_metadata_for_basic_pset(cm);

                                pset->points.emplace_back(dicom_pos);
                                pset->metadata = cm;
                                if(img_val_opt) pset->metadata[img_features.metadata_key] = img_val_opt.value();
                                break;
                            }

                        }else{
                            if(!tagged_pos){
                                tagged_pos = image_mouse_pos_opt.value().dicom_pos;
                            }else{
                                tagged_pos = {};
                            }
                        }

                    // Right-button mouse click on an image.
                    }else if( image_mouse_pos_opt.value().mouse_hovering_image
                        &&  img_valid
                        &&  image_mouse_pos_opt
                        &&  (1 < IM_ARRAYSIZE(io.MouseDown))
                        &&  (0.0f == io.MouseDownDuration[1]) ){ // Debounced!

                        if(view_toggles.view_image_feature_extraction){
                            img_features.features_C = point_set<double>();

                            // Remove nearby features.
                            const auto dicom_pos = image_mouse_pos_opt.value().dicom_pos;
                            const auto img_val_opt = get_as<std::string>(disp_img_it->metadata, img_features.metadata_key);

                            for(auto &pset : { &img_features.features_A, &img_features.features_B }){
                                const auto pset_val_opt = get_as<std::string>(pset->metadata, img_features.metadata_key);
                                if(pset->points.empty()) continue;
                                if( pset_val_opt
                                &&  (pset_val_opt != img_val_opt) ) continue;

                                auto existing = std::find_if( std::begin(pset->points),
                                                              std::end(pset->points),
                                                              [&](const vec3<double> &p) -> bool {
                                                                  return dicom_pos.distance(p) < img_features.snap_dist;
                                                              });
                                const auto is_near_existing = (existing != std::end(pset->points));
                                if(is_near_existing){
                                    pset->points.erase(existing);
                                    if(pset->points.empty()) pset->metadata.clear();

                                    break;
                                }
                            }

                        }

                    }else if( 0 < io.MouseWheel ){
                        scroll_images = std::clamp<int>((scroll_images + N_images + d_h) % N_images, 0, N_images - 1);
                    }else if( io.MouseWheel < 0 ){
                        scroll_images = std::clamp<int>((scroll_images + N_images + d_l) % N_images, 0, N_images - 1);

                    }else if( ImGui::IsKeyPressed( SDL_SCANCODE_N)
                          ||  ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_RightArrow)) ){
                        scroll_images = std::clamp<int>((scroll_images + N_images + 1) % N_images, 0, N_images - 1);
                    }else if( ImGui::IsKeyPressed( SDL_SCANCODE_P)
                          ||  ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) ){
                        scroll_images = std::clamp<int>((scroll_images + N_images - 1) % N_images, 0, N_images - 1);

                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_PageDown) ) ){
                        scroll_images = std::clamp<int>((scroll_images + 50 * N_images - 10) % N_images, 0, N_images - 1);
                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_PageUp) ) ){
                        scroll_images = std::clamp<int>((scroll_images + 50 * N_images + 10) % N_images, 0, N_images - 1);

                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_Home) ) ){
                        scroll_images = 0;
                    }else if( ImGui::IsKeyPressed( ImGui::GetKeyIndex(ImGuiKey_End) ) ){
                        scroll_images = N_images - 1;
                    }
                    //ImGui::Text("%.2f secs", io.KeysDownDuration[ (int)(ImGui::GetKeyIndex(ImGuiKey_PageUp)) ]);
                }
            }

            int64_t new_img_array_num = scroll_arrays;
            int64_t new_img_num = scroll_images;
            int64_t new_img_chnl = scroll_channel;
            bool new_img_is_rgb = scroll_is_rgb;
            bool new_use_tex_aa = scroll_use_tex_aa;

            // Update the image state.
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
                img_channel = std::clamp<int64_t>(new_img_chnl, 0L, disp_img_it->channels - 1L);
                recompute_image_state();
                auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                if( !img_valid ) throw std::runtime_error("Advanced to inaccessible image channel");

            }else if( (new_img_is_rgb != img_is_rgb)
                  ||  (new_use_tex_aa != use_texture_antialiasing) ){
                img_is_rgb = new_img_is_rgb;
                use_texture_antialiasing = new_use_tex_aa;
                recompute_image_state();
                auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
                if( !img_valid ) throw std::runtime_error("Unable to render image (RGB or antialiasing issue)");
            }

            ImGui::End();
            return;
        };
        try{
            display_image_navigation();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_image_navigation(): '" << e.what() << "'");
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
                ImGui::InputText("Name", time_course_text_entry.data(), time_course_text_entry.size() - 1);

                ImGui::Separator();
                if(ImGui::Button("Save")){
                    std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
                    if(!drover_lock) break;

                    std::string text;
                    array_to_string(text, time_course_text_entry);
                    time_profile.metadata["LineName"] = text;

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

        if(view_toggles.save_row_column_profiles){
            view_toggles.save_row_column_profiles = false;
            string_to_array(col_profile_text_entry, "unspecified");
            string_to_array(row_profile_text_entry, "unspecified");
            ImGui::OpenPopup("Save Row and Column Profiles");
        }
        if(ImGui::BeginPopupModal("Save Row and Column Profiles", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
            do{
                ImGui::InputText("Row Profile Name", row_profile_text_entry.data(), row_profile_text_entry.size() - 1);
                ImGui::InputText("Column Profile Name", col_profile_text_entry.data(), col_profile_text_entry.size() - 1);

                ImGui::Separator();
                if(ImGui::Button("Save")){
                    std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
                    if(!drover_lock) break;

                    std::string text;
                    array_to_string(text, row_profile_text_entry);
                    row_profile.metadata["LineName"] = text;
                    array_to_string(text, col_profile_text_entry);
                    col_profile.metadata["LineName"] = text;

                    // Save a copy of the current profiles.
                    DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
                    DICOM_data.lsamp_data.back()->line = row_profile;

                    DICOM_data.lsamp_data.emplace_back( std::make_shared<Line_Sample>() );
                    DICOM_data.lsamp_data.back()->line = col_profile;

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

            const auto N_meshes = static_cast<int64_t>(DICOM_data.smesh_data.size());
            const auto new_mesh_num = std::clamp<int64_t>(mesh_num, 0L, N_meshes - 1L);
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
                    if(static_cast<int64_t>(scroll_meshes) != mesh_num){
                        mesh_num = std::clamp<int64_t>(static_cast<int64_t>(scroll_meshes), 0L, N_meshes - 1L);
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
                    ImGui::DragScalar("Yaw",   ImGuiDataType_Double, &mesh_display_transform.rot_y, drag_speed, &clamp_l, &clamp_h, "%.1f");
                    ImGui::DragScalar("Pitch", ImGuiDataType_Double, &mesh_display_transform.rot_p, drag_speed, &clamp_l, &clamp_h, "%.1f");
                    ImGui::DragScalar("Roll",  ImGuiDataType_Double, &mesh_display_transform.rot_r, drag_speed, &clamp_l, &clamp_h, "%.1f");

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


        // Show a loading animation.
        const auto display_loading_animation = [&drover_mutex,
                                                &mutex_dt,
                                                &frame_count,
                                                &t_start]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(drover_lock) return; // Only draw when the lock IS held.

            auto flags = ImGuiWindowFlags_NoBackground
                       | ImGuiWindowFlags_NoInputs
                       | ImGuiWindowFlags_NoScrollWithMouse
                       | ImGuiWindowFlags_NoDecoration
                       | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoNav
                       | ImGuiWindowFlags_NoBackground
                       | ImGuiWindowFlags_NoBringToFrontOnFocus;
            bool placeholder = true;

            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowSize(ImVec2(250, 40), ImGuiCond_Always);

            // Position bottom-right corner of window at screen bottom-right.
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always, ImVec2(1.0f, 1.0f));

            ImGui::Begin("Script Loading Bar", &placeholder, flags);
            CustomImGuiWidget_LoadingBar(t_start);
            ImGui::End();
            return;
        };
        try{
            display_loading_animation();
        }catch(const std::exception &e){
            YLOGWARN("Exception in display_loading_animation(): '" << e.what() << "'");
            throw;
        }



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
                mesh_display_transform.rot_y += 0.0100 * mesh_display_transform.precess_rate;
                mesh_display_transform.rot_p -= 0.0029 * mesh_display_transform.precess_rate;
                mesh_display_transform.rot_r -= 0.0003 * mesh_display_transform.precess_rate;
            }
            mesh_display_transform.rot_y = std::fmod(mesh_display_transform.rot_y, 360.0);
            mesh_display_transform.rot_p = std::fmod(mesh_display_transform.rot_p, 360.0);
            mesh_display_transform.rot_r = std::fmod(mesh_display_transform.rot_r, 360.0);

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
            GLuint prior_gl_program = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&prior_gl_program));
            glUseProgram(custom_gl_program);

            // Account for viewport aspect ratio to make the render square.
            const auto w = static_cast<int>(io.DisplaySize.x);
            const auto h = static_cast<int>(io.DisplaySize.y);
            glViewport(0, 0, w, h);
            CHECK_FOR_GL_ERRORS();

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
                for(int64_t r = 0; r < 3; ++r){
                    for(int64_t c = 0; c < 3; ++c){
                        out.coeff(r,c) = mvp.read_coeff(r,c);
                    }
                }
                return out;
            };

            // Rotate camera according as per user's settings / precession.
            const auto pi = std::acos(-1.0);
            const auto y_rot = 0.0 + mesh_display_transform.rot_y * (2.0 * pi) / 360.0; // Yaw.
            const auto p_rot = 0.0 + mesh_display_transform.rot_p * (2.0 * pi) / 360.0; // Pitch.
            const auto r_rot = 0.0 - mesh_display_transform.rot_r * (2.0 * pi) / 360.0; // Roll.

            auto axis_1 = vec3<double>(0,0,1); // Represents camera position (looking inward, toward origin).
            auto axis_2 = vec3<double>(1,0,0); // Represents camera view's leftward direction.
            auto axis_3 = vec3<double>(0,1,0); // Represents camera views's upward direction.

            axis_1 = axis_1.rotate_around_unit(axis_2, p_rot);
            axis_3 = axis_3.rotate_around_unit(axis_2, p_rot);
            axis_2 = axis_2.rotate_around_unit(axis_2, p_rot);

            axis_3 = axis_3.rotate_around_unit(axis_1, r_rot);
            axis_2 = axis_2.rotate_around_unit(axis_1, r_rot);
            axis_1 = axis_1.rotate_around_unit(axis_1, r_rot);

            axis_1 = axis_1.rotate_around_unit(axis_3, y_rot);
            axis_2 = axis_2.rotate_around_unit(axis_3, y_rot);
            axis_3 = axis_3.rotate_around_unit(axis_3, y_rot);

            const auto target_pos = vec3<double>(0.0, 0.0, 0.0);
            const auto up_unit = axis_3.unit();
            const auto cam_pos = axis_1.unit() * std::exp(mesh_display_transform.cam_distort - 5.0);

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
            glUseProgram(prior_gl_program);
            CHECK_FOR_GL_ERRORS();
        }

        // Show a pop-up with information about DICOMautomaton.
        if(view_toggles.set_about_popup){
            view_toggles.set_about_popup = false;
            ImGui::OpenPopup("About");
        }
        if(ImGui::BeginPopupModal("About")){
            const std::string desc = "DICOMautomaton: a multipurpose tool for medical physics.";
            ImGui::Dummy(ImVec2(100, 15));
            ImGui::Text("%s", desc.c_str());

            const std::string version = "Version: "_s + DCMA_VERSION_STR;
            ImGui::Dummy(ImVec2(100, 15));
            ImGui::Text("%s", version.c_str());

            const std::string contact = "Please report issues at https://github.com/hdclark/DICOMautomaton"_s;
            ImGui::Dummy(ImVec2(100, 15));
            ImGui::Text("%s", contact.c_str());

            ImGui::Dummy(ImVec2(100, 25));
            if(ImGui::Button("Close")){
                ImGui::CloseCurrentPopup();
            }
            ImGui::Dummy(ImVec2(100, 15));

            if(ImGui::Button("View contouring debug window")){
                view_toggles.view_contouring_debug = true;
                ImGui::CloseCurrentPopup();
            }

            if(ImGui::Button("Imgui demo")){
                view_toggles.view_imgui_demo = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Implot demo")){
                view_toggles.view_implot_demo = true;
                ImGui::CloseCurrentPopup();
            }

            if(ImGui::Button("Polyominoes")){
                view_toggles.view_polyominoes_enabled = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Triple Three")){
                view_toggles.view_triple_three_enabled = true;
                const auto t_now = std::chrono::steady_clock::now();
                t_tt_updated = t_now;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Encompass")){
                view_toggles.view_encompass_enabled = true;
                reset_en_game();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cube")){
                view_toggles.view_cube_enabled = true;
                reset_cube_game();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }


        // Render the ImGui components and swap OpenGL buffers.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    std::unique_lock<std::shared_mutex> ppc_lock(preprocessed_contour_mutex);
    terminate_contour_preprocessors();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Hope that this is enough time for preprocessing threads to terminate.
                                                                 // TODO: use a work queue with condition variable to
                                                                 // signal termination!

    oglm_ptr = nullptr;  // Release OpenGL resources while context is valid.
    custom_shader = nullptr;
    Free_OpenGL_Texture(current_texture);
    Free_OpenGL_Texture(contouring_texture);
    Free_OpenGL_Texture(scale_bar_texture);
    unregister_guide_textures();

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
