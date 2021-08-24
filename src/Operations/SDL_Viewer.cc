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

#include "../imgui20201021/imgui.h"
#include "../imgui20201021/imgui_impl_sdl.h"
#include "../imgui20201021/imgui_impl_opengl3.h"
#include "../implot20210525/implot.h"

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
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"

#include "../Font_DCMA_Minimal.h"
#include "../DCMA_Version.h"
#include "../File_Loader.h"
#include "../Script_Loader.h"
#include "../Thread_Pool.h"

#ifdef DCMA_USE_CGAL
    #include "../Surface_Meshes.h"
#endif // DCMA_USE_CGAL

#include "SDL_Viewer.h"

//extern const std::string DCMA_VERSION_STR;

OperationDoc OpArgDocSDL_Viewer(){
    OperationDoc out;
    out.name = "SDL_Viewer";
    out.desc = 
        "Launch an interactive viewer based on SDL.";

    return out;
}

bool SDL_Viewer(Drover &DICOM_data,
                  const OperationArgPkg& /*OptArgs*/,
                  const std::map<std::string, std::string>& InvocationMetadata,
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
        bool view_metrics_window = false;
        bool open_files_enabled = false;
        bool view_images_enabled = true;
        bool view_image_metadata_enabled = false;
        bool view_meshes_enabled = true;
        bool view_plots_enabled = true;
        bool view_plots_metadata = true;
        bool view_contours_enabled = true;
        bool view_contouring_enabled = false;
        bool view_row_column_profiles = false;
        bool view_script_editor_enabled = false;
        bool view_script_feedback = true;
        bool show_image_hover_tooltips = true;
        bool adjust_window_level_enabled = false;
        bool adjust_colour_map_enabled = false;
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
    } image_mouse_pos;

    samples_1D<double> row_profile;
    samples_1D<double> col_profile;

    // --------------------------------------------- Setup ------------------------------------------------
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

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0){
        throw std::runtime_error("Unable to initialize SDL: "_s + SDL_GetError());
    }

    // Configure the desired OpenGL version (v3.0).
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_FLAGS: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_PROFILE_MASK: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)){
        throw std::runtime_error("Unable to set SDL_GL_CONTEXT_MAJOR_VERSION: "_s + SDL_GetError());
    }
    if(0 != SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0)){
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
        throw std::runtime_error("ImGui unable to initialize OpenGL with given shader.");
    }
    CHECK_FOR_GL_ERRORS();
    try{
        auto gl_version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        auto glsl_version = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        if(gl_version == nullptr) throw std::runtime_error("OpenGL version not accessible.");
        if(glsl_version == nullptr) throw std::runtime_error("GLSL version not accessible.");

        FUNCINFO("Initialized OpenGL '" << std::string(gl_version) << "' with GLSL '" << std::string(glsl_version) << "'");
    }catch(const std::exception &e){
        FUNCWARN("Unable to detect OpenGL/GLSL version");
    }

    // -------------------------------- Functors for various things ---------------------------------------

    // Create an OpenGL texture from an image.
    struct opengl_texture_handle_t {
        GLuint texture_number = 0;
        long int col_count = 0L;
        long int row_count = 0L;
        float aspect_ratio = 1.0; // In image pixel space.
        bool texture_exists = false;
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
    int contouring_img_row_col_count = 256;
    bool contouring_img_altered = false;
    float contouring_reach = 10.0;
    float contouring_margin = 1.0;
    std::string contouring_method = "marching-squares";
    enum class brushes {
        // 2D brushes.
        rigid_circle,
        rigid_square,
        gaussian,

        // 3D brushes.
        rigid_sphere,
    };
    brushes contouring_brush = brushes::rigid_circle;
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
                                          &img_num ](){
        std::tuple<bool, img_array_ptr_it_t, disp_img_it_t > out;
        std::get<bool>( out ) = false;
        const long int img_array_num = 0;

        // Set the current image array and image iters and load the texture.
        const auto has_images = contouring_imgs.Has_Image_Data();
        do{ 
            if( !has_images ) break;
            if( !isininc(1, img_array_num+1, contouring_imgs.image_data.size()) ) break;
            auto img_array_ptr_it = std::next(contouring_imgs.image_data.begin(), img_array_num);
            if( img_array_ptr_it == std::end(contouring_imgs.image_data) ) break;

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

    const auto recompute_scale_bar_image_state = [ &scale_bar_img,
                                                   &scale_bar_texture,
                                                   &recompute_cimage_iters,
                                                   &Load_OpenGL_Texture ](){
        auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_cimage_iters();
        if( img_valid ){ 
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

    // ------------------------------------------- Main loop ----------------------------------------------


    // Open file dialog state.
    std::filesystem::path open_file_root = std::filesystem::current_path();
    std::array<char,1024> root_entry_text;
    struct file_selection {
        std::filesystem::path path;
        bool is_dir;
        std::uintmax_t file_size;
        bool selected;
    };
    std::vector<file_selection> open_files_selection;
    const auto query_files = [&]( std::filesystem::path root ){
        open_files_selection.clear();
        try{
            if( !root.empty()
            &&  std::filesystem::exists(root)
            &&  std::filesystem::is_directory(root) ){
                for(const auto &d : std::filesystem::directory_iterator( root )){
                    const auto p = d.path();
                    const std::uintmax_t file_size = ( !std::filesystem::is_directory(p) && std::filesystem::exists(p) )
                                                   ? std::filesystem::file_size(p) : 0;
                    open_files_selection.push_back( { p,
                                                      std::filesystem::is_directory(p),
                                                      file_size,
                                                      false } );
                }
                std::sort( std::begin(open_files_selection), std::end(open_files_selection), 
                           [](const file_selection &L, const file_selection &R){
                                return L.path < R.path;
                           } );
            }
        }catch(const std::exception &e){
            FUNCINFO("Unable to query files: '" << e.what() << "'");
            open_files_selection.clear();
        }
        return;
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
    };
    std::future<loaded_files_res> loaded_files;

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
    const auto append_to_script = [](std::vector<char> &content, const std::string &s) -> void {
        for(const auto &c : s) content.emplace_back(c);
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


    long int frame_count = 0;
    while(true){
        ++frame_count;

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
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

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
                                           &Load_OpenGL_Texture ]() -> void {
            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex);
            auto [img_valid, img_array_ptr_it, disp_img_it] = recompute_image_iters();
            if( view_toggles.view_images_enabled
            &&  img_valid ){
                img_channel = std::clamp<long int>(img_channel, 0, disp_img_it->channels-1);
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
if(false){
            auto [cimg_valid, cimg_array_ptr_it, cimg_it] = recompute_cimage_iters();
            if(cimg_valid){
                ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(700, 40), ImGuiCond_FirstUseEver);
                ImGui::Begin("Images2", &view_toggles.view_images_enabled, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar ); //| ImGuiWindowFlags_AlwaysAutoResize);
                auto contouring_texture = Load_OpenGL_Texture( *cimg_it, {}, {} );
                auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(contouring_texture.texture_number));
                ImGui::Image(gl_tex_ptr, ImVec2(600,600), uv_min, uv_max);
                ImGui::End();
            }
}

        // Display the main menu bar, which should always be visible.
        const auto display_main_menu_bar = [&view_toggles,
                                            &query_files,
                                            &open_file_root,
                                            &contour_enabled,
                                            &contour_hovered,
                                            &launch_contour_preprocessor,
                                            &contouring_img_altered,
                                            &tagged_pos,

                                            &script_mutex,
                                            &script_files,
                                            &active_script_file,
                                            &script_epoch,
                                            &append_to_script,

                                            &lsamps_visible,
                                            &row_profile,
                                            &col_profile ](void) -> bool {
            if(ImGui::BeginMainMenuBar()){
                if(ImGui::BeginMenu("File")){
                    if(ImGui::MenuItem("Open", "ctrl+o", &view_toggles.open_files_enabled)){
                        query_files(open_file_root);
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
                        contouring_img_altered = true;
                        tagged_pos = {};
                    }
                    ImGui::MenuItem("Image Metadata", nullptr, &view_toggles.view_image_metadata_enabled);
                    ImGui::MenuItem("Image Hover Tooltips", nullptr, &view_toggles.show_image_hover_tooltips);
                    ImGui::MenuItem("Meshes", nullptr, &view_toggles.view_meshes_enabled);
                    if(ImGui::MenuItem("Plots", nullptr, &view_toggles.view_plots_enabled)){
                        lsamps_visible.clear();
                    }
                    ImGui::MenuItem("Plot Hover Metadata", nullptr, &view_toggles.view_plots_metadata);
                    if(ImGui::MenuItem("Row and Column Profiles", nullptr, &view_toggles.view_row_column_profiles)){
                        row_profile.samples.clear();
                        col_profile.samples.clear();
                    };
                    ImGui::MenuItem("Script Editor", nullptr, &view_toggles.view_script_editor_enabled);
                    ImGui::MenuItem("Script Feedback", nullptr, &view_toggles.view_script_feedback);
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Adjust")){
                    ImGui::MenuItem("Window and Level", nullptr, &view_toggles.adjust_window_level_enabled);
                    ImGui::MenuItem("Image Colour Map", nullptr, &view_toggles.adjust_colour_map_enabled);
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if(ImGui::BeginMenu("Script")){
                    if(ImGui::BeginMenu("Append Operation")){
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

                            if(ImGui::MenuItem(op_name.c_str())){
                                std::unique_lock<std::shared_mutex> script_lock(script_mutex);

                                auto N_sfs = static_cast<long int>(script_files.size());
                                if( N_sfs == 0 ){
                                    FUNCINFO("No script to append to. Creating new script.");
                                    script_files.emplace_back();
                                    script_files.back().altered = true;
                                    script_files.back().content.emplace_back('\0');
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
                                    bool first = true;
                                    std::set<std::string> args;
                                    for(const auto & a : op_docs.args){
                                        // Avoid duplicate arguments (from composite operations).
                                        const auto name = a.name;
                                        if(args.count(name) != 0) continue;
                                        args.insert(name);

                                        // Escape any quotes in the default value, which will generally be parsed
                                        // fuzzily via regex and should be OK.
                                        std::string val = a.default_val;
                                        val.erase( std::remove_if( std::begin(val),
                                                                   std::end(val),
                                                                   [](char c) -> bool { return (c == '\''); }),
                                                   std::end(val) );
                                        if(a.expected){
                                            if(!first) sc << ", ";
                                            sc << std::endl << "    " << name << " = '" << val << "'";
                                            first = false;
                                        }else{
                                            oc << "    # " <<  name << " = '" << val << "',";
                                        }
                                    }
                                    if(!oc.str().empty()){
                                        sc << std::endl << oc.str() << std::endl;
                                    }
                                    sc << " ){};" << std::endl;

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
        if(!display_main_menu_bar()) break;

        if( view_toggles.view_metrics_window ){
            ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
            ImGui::ShowMetricsWindow(&view_toggles.view_metrics_window);
        }

        // Display the script editor dialog.
        const auto display_script_editor = [&view_toggles,
                                            &custom_centre,
                                            &custom_width,
                                            &open_file_root,
                                            &root_entry_text,

                                            &script_mutex,
                                            &script_files,
                                            &active_script_file,
                                            &script_epoch,
                                            &append_to_script,

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
                ImGui::Begin("Script Editor", &view_toggles.view_script_editor_enabled );
                ImVec2 window_extent = ImGui::GetContentRegionAvail();

                auto N_sfs = static_cast<long int>(script_files.size());
                if(ImGui::Button("New", ImVec2(window_extent.x/4, 0))){ 
                    script_files.emplace_back();
                    script_files.back().altered = true;
                    script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
////////////////
////////////////
////////////////
const std::string testing_content = R"***(#!/usr/bin/env -S dicomautomaton_dispatcher -v

variable = "something";
variable = "something else";

variable_op = "operation2";

operation1(a = 123,
          b = "234",
          c = 3\45,
          d = "45\6",
          e = '56\7',
          f = variable );

# This is a comment. It should be ignored, including syntax errors like ;'"(]'\"".
# This is another comment.
variable_op(x = "123",
          # This is another comment.
          y = 456){

    # This is a nested statement. Recursive statements are supported.
    operation3(z = 789){
        variable = "something new";
        operation4(w = variable);
    };

};

DeleteImages(
    ImageSelection = 'all'
);
DeleteContours(
    ROILabelRegex = '.*'
);

roiall = "everything";
roisphere = 'sphere';

Repeat( N = 2 ){
    Repeat( N = 1 ){
        GenerateVirtualDataImageSphereV1();
    };
};
DeleteImages( ImageSelection = 'last' );

ContourWholeImages(
    ROILabel = roiname
);

ContourViaThreshold(
    ROILabel = roisphere,
    Method = "marching-squares",
    Lower = 0.5,
    SimplifyMergeAdjacent = true
);

)***";
script_files.back().content.clear();
append_to_script(script_files.back().content, testing_content);
script_files.back().content.emplace_back('\0');
////////////////
////////////////

                    active_script_file = N_sfs;
                    ++N_sfs;
                }
                ImGui::SameLine();
                if(ImGui::Button("Open", ImVec2(window_extent.x/4, 0))){ 
                    // TODO

                    // Note: ensure there is at least a null character. TODO.

                    // Mimic the 'new' button for testing.
                    script_files.emplace_back();
                    script_files.back().altered = true;
                    script_files.back().content.emplace_back('\0'); // Ensure there is at least a null character.
                    active_script_file = N_sfs;
                    ++N_sfs;
                }
                ImGui::SameLine();
                if(ImGui::Button("Save", ImVec2(window_extent.x/4, 0))){ 
                    if( (N_sfs != 0) 
                    &&  isininc(0, active_script_file, N_sfs-1)){
                        if( script_files.at(active_script_file).path.empty() ){
                            try{
                                const auto open_file_root_str = std::filesystem::absolute(open_file_root / "script.txt").string();
                                for(size_t i = 0; (i < open_file_root_str.size()) && ((i+1) < root_entry_text.size()); ++i){
                                    root_entry_text[i] = open_file_root_str[i];
                                    root_entry_text[i+1] = '\0';
                                }

                                ImGui::OpenPopup("Save Script Filename Picker");
                            }catch(const std::exception &e){ };
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
                        std::stringstream ss( std::string( std::begin(script_files.at(active_script_file).content),
                                                           std::end(script_files.at(active_script_file).content) ) );
                        script_files.at(active_script_file).feedback.clear();
                        std::list<OperationArgPkg> op_list;
                        const auto res = Load_DCMA_Script( ss, script_files.at(active_script_file).feedback, op_list );
                        if(!res){
                            //script_files.at(active_script_file).feedback.emplace_back();
                            script_files.at(active_script_file).feedback.back().message = "Compilation failed";
                            view_toggles.view_script_feedback = true;

                        }else{
                            // Submit the work to the worker thread.
                            const auto l_script_epoch = script_epoch.fetch_add(1) + 1;
                            const auto worker = [&,l_script_epoch,op_list](){
                                // Check if this task should be abandoned.
                                if(script_epoch.load() != l_script_epoch){
                                    FUNCINFO("Abandoning run due to user activity");
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
                                    if(!Operation_Dispatcher(DICOM_data,
                                                             InvocationMetadata,
                                                             FilenameLex,
                                                             {op})){
                                        success = false;
                                        break;
                                    }
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
                                    tagged_pos = {};
                                }
                                return;
                            };
                            wq.submit_task( worker ); // Offload to waiting worker thread.
                        }
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
                        script_files.at(active_script_file).path.replace_extension(".txt");

                        // Write the file contents to the given path.
                        std::ofstream FO(script_files.at(active_script_file).path.string());
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

                ImGui::End();
            }
            return;
        };
        display_script_editor();


        // Display the image dialog.
        const auto display_image_viewer = [&view_toggles,
                                           //&custom_centre,
                                           //&custom_width,
                                           &current_texture,
                                           &uv_min,
                                           &uv_max,
                                           &image_mouse_pos,
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
                                           &editing_contour_colour,
                                           &pos_contour_colour,
                                           &neg_contour_colour,

                                           &row_profile,
                                           &col_profile,

                                           &frame_count ]() -> void {

// Lock the image details mutex in shared mode.
// 
// ... TODO ...
//
            if( !view_toggles.view_images_enabled
            ||  !current_texture.texture_exists ) return;

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

            // Continue now that we have an exclusive lock.
            ImGui::Begin("Images", &view_toggles.view_images_enabled);

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
                ImDrawList *drawList = ImGui::GetWindowDrawList();

                const auto box_radius = 3.0f;
                const auto c = ImColor(1.0f, 0.2f, 0.2f, 1.0f);

                ImVec2 p1 = image_mouse_pos.DICOM_to_pixels(tagged_pos.value());
                ImVec2 ul1( p1.x - box_radius, p1.y - box_radius );
                ImVec2 lr1( p1.x + box_radius, p1.y + box_radius );
                drawList->AddRect(ul1, lr1, c);

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
                    drawList->AddRect(ul2, lr2, c);

                    // Connect the boxes with a line if both are contained within the same image volume.
                    if( disp_img_it->sandwiches_point_within_top_bottom_planes(tagged_pos.value())
                    &&  disp_img_it->sandwiches_point_within_top_bottom_planes(image_mouse_pos.dicom_pos) ){
                        drawList->AddLine(p1, p2, c);
                    }
                }
            }

            // Display a contour legend.
            if( view_toggles.view_contours_enabled
            &&  (DICOM_data.contour_data != nullptr) ){
                ImGui::SetNextWindowSize(ImVec2(510, 650), ImGuiCond_FirstUseEver);
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
                ImDrawList *drawList = ImGui::GetWindowDrawList();

                //We have three distinct coordinate systems: DICOM, pixel coordinates and screen pixel coordinates,
                // and SFML 'world' coordinates. We need to map from the DICOM coordinates to screen pixel coords.

                //Get a DICOM-coordinate bounding box for the image.
                const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->rows;
                const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->columns; 
                const auto img_top_left = disp_img_it->anchor + disp_img_it->offset
                                        - disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5f
                                        - disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5f;
                //const auto img_top_right = img_top_left + disp_img_it->row_unit * img_dicom_width;
                //const auto img_bottom_left = img_top_left + disp_img_it->col_unit * img_dicom_height;
                const auto img_plane = disp_img_it->image_plane();

                for(auto &p : contour_hovered) p.second = false;

                std::shared_lock<std::shared_mutex> lock(preprocessed_contour_mutex);
                const auto current_epoch = preprocessed_contour_epoch.load();
                for(const auto &pc : preprocessed_contours){
                    if( pc.epoch != current_epoch ) continue;
                    if( !contour_enabled[pc.ROIName] ) continue;

                    drawList->PathClear();
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
                        drawList->PathLineTo( v );
                    }

                    // Check if the mouse if within the contour.
                    float thickness = contour_line_thickness;
                    if(image_mouse_pos.mouse_hovering_image){
                        const auto within_poly = pc.contour.Is_Point_In_Polygon_Projected_Orthogonally(img_plane,image_mouse_pos.dicom_pos);
                        thickness *= ( within_poly ) ? 1.5f : 1.0f;
                        if(within_poly) contour_hovered[pc.ROIName] = true;
                    }
                    const bool closed = true;
                    drawList->PathStroke(pc.colour, closed, thickness);
                    //AddPolyline(const ImVec2* points, int num_points, ImU32 col, bool closed, float thickness);
                }

                // Contouring interface.
                if( view_toggles.view_contouring_enabled ){
                    // Provide a visual cue for the contouring brush.
                    {
                        const auto pixel_radius = static_cast<float>(contouring_reach) * image_mouse_pos.pixel_scale;
                        const auto c = ImColor(0.0f, 1.0f, 0.8f, 1.0f);

                        if( (contouring_brush == brushes::rigid_circle)
                        ||  (contouring_brush == brushes::rigid_sphere)
                        ||  (contouring_brush == brushes::gaussian) ){
                            drawList->AddCircle(io.MousePos, pixel_radius, c);

                        }else if(contouring_brush == brushes::rigid_square){
                            ImVec2 ul( io.MousePos.x - pixel_radius,
                                       io.MousePos.y - pixel_radius );
                            ImVec2 lr( io.MousePos.x + pixel_radius,
                                       io.MousePos.y + pixel_radius );
                            drawList->AddRect(ul, lr, c);
                        }
                    }

                    ImGui::SetNextWindowSize(ImVec2(510, 650), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowPos(ImVec2(680, 200), ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
                    ImGui::Begin("Contouring", &view_toggles.view_contouring_enabled);

                    ImGui::Text("Note: this functionality is still under active development.");
                    if(ImGui::Button("Save")){ 
                        ImGui::OpenPopup("Save Contours");
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
                                    cimg.fill_pixels(-1.0f);
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

                    ImGui::Separator();
                    ImGui::Text("Brush");
                    ImGui::DragFloat("Radius (mm)", &contouring_reach, 0.1f, 0.5f, 50.0f);
                    if(ImGui::Button("Rigid Circle")){
                        contouring_brush = brushes::rigid_circle;
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Rigid Square")){
                        contouring_brush = brushes::rigid_square;
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Soft")){
                        contouring_brush = brushes::gaussian;
                    }

                    if(ImGui::Button("Rigid Sphere")){
                        contouring_brush = brushes::rigid_sphere;
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
                        if(!Operation_Dispatcher(contouring_imgs, InvocationMetadata, FilenameLex, Operations)){
                            FUNCWARN("Dilation/Erosion failed");
                        }

                        contouring_img_altered = true;
                    }

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

                                drawList->PathClear();
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
                                    drawList->PathLineTo( v );
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
                                drawList->PathStroke( colour, closed, thickness);
                                //AddPolyline(const ImVec2* points, int num_points, ImU32 col, bool closed, float thickness);
                            }
                        }
                    }
                    ImGui::End();
                }
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
            && view_toggles.view_row_column_profiles ){
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

            // Metadata window.
            if( view_toggles.view_image_metadata_enabled ){
                ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::Begin("Image Metadata", &view_toggles.view_image_metadata_enabled);

                ImGui::Text("Image Metadata");
                ImGui::Columns(2);
                ImGui::Separator();
                ImGui::Text("Key"); ImGui::NextColumn();
                ImGui::Text("Value"); ImGui::NextColumn();
                ImGui::Separator();

                for(const auto &apair : disp_img_it->metadata){
                    ImGui::Text("%s",  apair.first.c_str());  ImGui::NextColumn();
                    ImGui::Text("%s",  apair.second.c_str()); ImGui::NextColumn();
                }

                ImGui::Columns(1);
                ImGui::Separator();

                ImGui::End();
            }
            return;
        };
        display_image_viewer();


        // Open files dialog.
        const auto open_files_viewer = [&view_toggles,
                                        
                                        &drover_mutex,
                                        &mutex_dt,
                                        &InvocationMetadata,
                                        &FilenameLex,

                                        &open_file_root,
                                        &root_entry_text,
                                        &open_files_selection,
                                        &loaded_files,
                                        &query_files ]() -> void {
            std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

            if( !view_toggles.open_files_enabled
            ||  loaded_files.valid()) return;

            ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_FirstUseEver);
            ImGui::Begin("Open File", &view_toggles.open_files_enabled);

        // Always center this window when appearing
//        ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
//        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            ImGui::Text("%s", "Select one or more files to load.");
            ImGui::Separator();
            std::string open_file_root_str;
            try{
                open_file_root_str = std::filesystem::absolute(open_file_root).string();
            }catch(const std::exception &){ };
            for(size_t i = 0; (i < open_file_root_str.size()) && ((i+1) < root_entry_text.size()); ++i){
                root_entry_text[i] = open_file_root_str[i];
                root_entry_text[i+1] = '\0';
            }

            ImGui::Text("Current directory:");
            ImGui::SameLine();
            ImGui::InputText("", root_entry_text.data(), root_entry_text.size());
            std::string entered_text;
            for(size_t i = 0; i < root_entry_text.size(); ++i){
                if(root_entry_text[i] == '\0') break;
                if(!std::isprint( static_cast<unsigned char>(root_entry_text[i]) )) break;
                entered_text.push_back(root_entry_text[i]);
            }
            if(entered_text != open_file_root_str){
                open_files_selection.clear();
                open_file_root = entered_text;
                if( !entered_text.empty() 
                &&  std::filesystem::exists(open_file_root)
                &&  std::filesystem::is_directory(open_file_root) ){
                    query_files(open_file_root);
                }
            }
            ImGui::Separator();


            if(ImGui::Button("Parent directory", ImVec2(120, 0))){ 
                if(!open_file_root.empty()){
                    open_file_root = open_file_root.parent_path();
                    query_files(open_file_root);
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Select all", ImVec2(120, 0))){ 
                for(auto &ofs : open_files_selection){
                    if(!ofs.is_dir){
                        ofs.selected = true;
                    }
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Select none", ImVec2(120, 0))){ 
                for(auto &ofs : open_files_selection){
                    ofs.selected = false;
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Invert selection", ImVec2(120, 0))){ 
                for(auto &ofs : open_files_selection){
                    if(ofs.is_dir){
                        ofs.selected = false;
                    }else{
                        ofs.selected = !ofs.selected;
                    }
                }
            }
            ImGui::Separator();

            for(auto &ofs : open_files_selection){
                const auto is_selected = ImGui::Selectable(ofs.path.lexically_relative(open_file_root).string().c_str(), &(ofs.selected), ImGuiSelectableFlags_AllowDoubleClick);
                const auto is_doubleclicked = is_selected && ImGui::IsMouseDoubleClicked(0);
                ImGui::SameLine(500);
                if(ofs.is_dir){
                    ImGui::Text("(dir)");
                }else if(ofs.file_size != 0){
                    const float file_size_kB = ofs.file_size / 1000.0;
                    if(file_size_kB < 500.0){
                        ImGui::Text("%.1f kB", file_size_kB );
                    }else{
                        ImGui::Text("%.2f MB", file_size_kB / 1000.0 );
                    }
                }

                if(is_doubleclicked){
                    if(ofs.is_dir){
                        open_file_root = ofs.path;
                        query_files(open_file_root);
                        break;
                    }
                }
            }

            ImGui::Separator();
            ImGui::SetItemDefaultFocus();
            if(ImGui::Button("Load selection", ImVec2(120, 0))){ 
                // Extract all files from the selection.
                std::list<std::filesystem::path> paths;
                for(auto &ofs : open_files_selection){
                    if(ofs.selected){
                        // Resolve all files within a directory.
                        if(ofs.is_dir){
                            for(const auto &d : std::filesystem::directory_iterator( ofs.path )){
                                paths.push_back( d.path().string() );
                            }

                        // Add a single file to the collection.
                        }else{
                            paths.push_back( ofs.path.string() );
                        }
                    }
                }

                // Load into to a separate Drover and only merge if all loads are successful.
                loaded_files = std::async(std::launch::async, [InvocationMetadata,
                                                               FilenameLex,
                                                               paths]() -> loaded_files_res {
                    loaded_files_res lfs;
                    auto paths_l = paths;
                    std::list<OperationArgPkg> Operations;
                    lfs.res = Load_Files(lfs.DICOM_data, InvocationMetadata, FilenameLex, Operations, paths_l);
                    if(!Operations.empty()){
                         lfs.res = false;
                         FUNCWARN("Loaded file contains a script. Currently unable to handle script files here");
                    }
                    return lfs;
                });

            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(120, 0))){ 
                view_toggles.open_files_enabled = false;
                open_files_selection.clear();
                // Reset the root directory.
                open_file_root = std::filesystem::current_path();
            }
            ImGui::End();
            return;
        };
        open_files_viewer();


        // Handle file loading future.
        const auto handle_file_loading = [&view_toggles,

                                          &drover_mutex,
                                          &DICOM_data,
                                          &recompute_image_state,

                                          &loaded_files,
                                          &open_files_selection,

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
                        view_toggles.open_files_enabled = false;
                        open_files_selection.clear();
                        DICOM_data.Consume(f.DICOM_data);
                    }else{
                        FUNCWARN("Unable to load files");
                        // TODO ... warn about the issue.
                    }
                    recompute_image_state();
                    loaded_files = decltype(loaded_files)();
                }
            }
        };
        handle_file_loading();


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
        adjust_window_level();


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
        adjust_colour_map();


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
        display_plots();


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
        display_row_column_profiles();


        // Display the image navigation dialog.
        const auto display_image_navigation = [&view_toggles,
                                               &drover_mutex,
                                               &mutex_dt,
                                               &DICOM_data,
                                               &recompute_image_iters,

                                               &img_array_num,
                                               &img_num,
                                               &img_channel,

                                               &zoom,
                                               &pan,
                                               &uv_min,
                                               &uv_max,
                                               &image_mouse_pos,
                                               &io,

                                               &recompute_cimage_iters,
                                               &contouring_img_altered,
                                               &contouring_reach,
                                               &last_mouse_button_0_down,
                                               &last_mouse_button_1_down,
                                               &last_mouse_button_pos,
                                               &largest_projection,
                                               &contouring_brush,

                                               &tagged_pos,

                                               &advance_to_image_array,
                                               &recompute_image_state,
                                               &launch_contour_preprocessor,
                                               &reset_contouring_state,
                                               &advance_to_image,

                                               &frame_count ]() -> void {

            std::unique_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;

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

                if(ImGui::IsWindowFocused() || image_mouse_pos.image_window_focused){
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

                    }else if( view_toggles.view_contouring_enabled
                          &&  cimg_valid
                          &&  (0 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (1 < IM_ARRAYSIZE(io.MouseDown))
                          &&  ((0.0f <= io.MouseDownDuration[0]) || (0.0f <= io.MouseDownDuration[1]))
                          &&  image_mouse_pos.mouse_hovering_image ){
                        contouring_img_altered = true;
                        long int channel = 0;

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
                            const auto pA = image_mouse_pos.dicom_pos; // Current position.
                            const auto pB = last_mouse_button_pos.value(); // Previous position.
                            // Project along image axes to create a taxi-cab metric corner vertex.
                            const auto corner = largest_projection(pA, pB, {cimg_it->row_unit, cimg_it->col_unit});
                            lss.emplace_back(pA,corner);
                            lss.emplace_back(corner,pB);

                        }else if( any_mouse_button_sticky
                              &&  last_mouse_button_pos ){
                            const auto pA = image_mouse_pos.dicom_pos; // Current position.
                            const auto pB = last_mouse_button_pos.value(); // Previous position.
                            lss.emplace_back(pA,pB);

                        }else{
                            auto pA = image_mouse_pos.dicom_pos; // Current position.
                            auto pB = pA;
                            pB.z += cimg_it->pxl_dz * 0.01; // Default offset to avoid degenerate line segment.
                            lss.emplace_back(pA,pB);
                        }

                        std::vector<disp_img_it_t> cimg_its;
                        if( (contouring_brush == brushes::rigid_circle)
                        ||  (contouring_brush == brushes::rigid_square)
                        ||  (contouring_brush == brushes::gaussian) ){
                            // Filter out irrelevant images.
                            cimg_its.emplace_back( cimg_it );
                        }else if(contouring_brush == brushes::rigid_sphere){
                            for(auto cit = std::begin((*cimg_array_ptr_it)->imagecoll.images);
                                     cit != std::end((*cimg_array_ptr_it)->imagecoll.images); ++cit){
                                // Pre-filter images that are not within range.
                                for(const auto& l : lss){
                                    // This is a dilated line_segment-plane intersection test.
                                    const auto plane_dist_R0 = cit->image_plane().Get_Signed_Distance_To_Point(l.Get_R0());
                                    const auto plane_dist_R1 = cit->image_plane().Get_Signed_Distance_To_Point(l.Get_R1());
                                    if( (std::signbit(plane_dist_R0) != std::signbit(plane_dist_R1))
                                    ||  (std::abs(plane_dist_R0) <= radius)
                                    ||  (std::abs(plane_dist_R1) <= radius) ){
                                        cimg_its.emplace_back( cit );
                                        break;
                                    }
                                }
                            }
                        }

                        for(auto &cit : cimg_its){
                            for(long int r = 0; r < cit->rows; ++r){
                                for(long int c = 0; c < cit->columns; ++c){
                                    const auto pos = cit->position(r,c);
                                    vec3<double> closest;
                                    {
                                        double closest_dist = 1E99;
                                        for(const auto &l : lss){
                                            const auto closest_l = l.Closest_Point_To(pos);
                                            const auto dist = closest_l.distance(pos);
                                            if(dist < closest_dist){
                                                closest = closest_l;
                                                closest_dist = dist;
                                            }
                                        }
                                    }

                                    const auto dR = closest.distance(pos);
                                    if( radius * 5.0 < dR ) continue;

                                    float dval = 0.0;
                                    if( (contouring_brush == brushes::rigid_circle)
                                    ||  (contouring_brush == brushes::rigid_sphere) ){
                                        dval = (dR <= radius) ? 2.0 : 0.0;

                                    }else if(contouring_brush == brushes::rigid_square){
                                        if( (std::abs((closest - pos).Dot(cit->row_unit)) < radius)
                                        &&  (std::abs((closest - pos).Dot(cit->col_unit)) < radius) ){
                                            dval = 2.0;
                                        }
                                    }else if(contouring_brush == brushes::gaussian){
                                        // Note: arbitrary scaling constant used here. Should give ~ same as rigid when
                                        // dragged across the image at a typical pace.
                                        dval = 2.0 * std::exp( -std::pow(dR / (0.5 * radius), 2.0f) );
                                    }

                                    if(mouse_button_0){
                                        // Do nothing.
                                    }else if(mouse_button_1){
                                        dval *= -1.0;
                                    }

                                    float val = cit->value(r, c, channel);
                                    val = std::clamp(val + dval, -1.0f, 2.0f);
                                    cit->reference( r, c, channel ) = val;
                                }
                            }
                        }

                        // Update mouse position for next time, if applicable.
                        if( mouse_button_0 ){
                            last_mouse_button_0_down = io.MouseDownDuration[0];
                            last_mouse_button_pos = image_mouse_pos.dicom_pos;
                        }
                        if( mouse_button_1 ){
                            last_mouse_button_1_down = io.MouseDownDuration[1];
                            last_mouse_button_pos = image_mouse_pos.dicom_pos;
                        }

                    }else if( image_mouse_pos.mouse_hovering_image
                          &&  (0 < IM_ARRAYSIZE(io.MouseDown))
                          &&  (0.0f == io.MouseDownDuration[0]) ){ // Debounced!
                          if(!tagged_pos){
                              tagged_pos = image_mouse_pos.dicom_pos;
                          }else{
                              tagged_pos = {};
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
        display_image_navigation();


        // Clear the current OpenGL frame.
        CHECK_FOR_GL_ERRORS();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(background_colour.x,
                     background_colour.y,
                     background_colour.z,
                     background_colour.w);
        glClear(GL_COLOR_BUFFER_BIT);
        CHECK_FOR_GL_ERRORS();


        // Tinkering with rendering surface meshes.
        const auto draw_surface_meshes = [&view_toggles,
                                          &drover_mutex,
                                          &mutex_dt,
                                          &DICOM_data,
                                          &frame_count ]() -> void {

            std::shared_lock<std::shared_timed_mutex> drover_lock(drover_mutex, mutex_dt);
            if(!drover_lock) return;
            if( !view_toggles.view_meshes_enabled
            ||  !DICOM_data.Has_Mesh_Data() ) return;

            auto smesh_ptr = DICOM_data.smesh_data.front();

            const auto N_verts = smesh_ptr->meshes.vertices.size();

            long int N_triangles = 0;
            for(const auto& f : smesh_ptr->meshes.faces){
                long int l_N_indices = f.size();
                if(l_N_indices < 3) continue; // Ignore faces that cannot be broken into triangles.
                N_triangles += (l_N_indices - 2);
            }

            const auto inf = std::numeric_limits<double>::infinity();
            auto x_min = inf;
            auto y_min = inf;
            auto z_min = inf;
            auto x_max = -inf;
            auto y_max = -inf;
            auto z_max = -inf;
            for(const auto& v : smesh_ptr->meshes.vertices){
                if(v.x < x_min) x_min = v.x;
                if(v.y < y_min) y_min = v.y;
                if(v.z < z_min) z_min = v.z;
                if(x_max < v.x) x_max = v.x;
                if(y_max < v.y) y_max = v.y;
                if(z_max < v.z) z_max = v.z;
            }

            {
                ImGui::Begin("Meshes");
                std::string msg = "Drawing "_s + std::to_string(N_verts) + " verts and "_s + std::to_string(N_triangles) + " triangles.";
                ImGui::Text("%s", msg.c_str());
                ImGui::End();
            }

            std::vector<float> vertices;
            vertices.reserve(3 * N_verts);
            for(const auto& v : smesh_ptr->meshes.vertices){
                // Scale each of x, y, and z to [-1,+1], but shrink to [-1/sqrt(3),+1/sqrt(3)] to account for rotation.
                // Scaling down will ensure the corners are not clipped when the cube is rotated.
                vec3<double> w( 0.577 * (2.0 * (v.x - x_min) / (x_max - x_min) - 1.0),
                                0.577 * (2.0 * (v.y - y_min) / (y_max - y_min) - 1.0),
                                0.577 * (2.0 * (v.z - z_min) / (z_max - z_min) - 1.0) );

                w = w.rotate_around_z(3.14159265 * static_cast<double>(frame_count / 59900.0));
                w = w.rotate_around_y(3.14159265 * static_cast<double>(frame_count / 11000.0));
                w = w.rotate_around_x(3.14159265 * static_cast<double>(frame_count / 26000.0));
                vertices.push_back(static_cast<float>(w.x));
                vertices.push_back(static_cast<float>(w.y));
                vertices.push_back(static_cast<float>(w.z));
            }
            
            std::vector<unsigned int> indices;
            indices.reserve(3 * N_triangles);
            for(const auto& f : smesh_ptr->meshes.faces){
                long int l_N_indices = f.size();
                if(l_N_indices < 3) continue; // Ignore faces that cannot be broken into triangles.

                const auto it_1 = std::cbegin(f);
                const auto it_2 = std::next(it_1);
                const auto end = std::end(f);
                for(auto it_3 = std::next(it_2); it_3 != end; ++it_3){
                    indices.push_back(static_cast<unsigned int>(*it_1));
                    indices.push_back(static_cast<unsigned int>(*it_2));
                    indices.push_back(static_cast<unsigned int>(*it_3));
                }
            }

            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;

            CHECK_FOR_GL_ERRORS();

            glGenVertexArrays(1, &vao); // Create a VAO inside the OpenGL context.
            glGenBuffers(1, &vbo); // Create a VBO inside the OpenGL context.
            glGenBuffers(1, &ebo); // Create a EBO inside the OpenGL context.

            glBindVertexArray(vao); // Bind = make it the currently-used object.
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            CHECK_FOR_GL_ERRORS();

            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), static_cast<void*>(vertices.data()), GL_STATIC_DRAW); // Copy vertex data.

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), static_cast<void*>(indices.data()), GL_STATIC_DRAW); // Copy index data.

            CHECK_FOR_GL_ERRORS();

            glEnableVertexAttribArray(0); // enable attribute with index 0.
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); // Vertex positions, 3 floats per vertex, attrib index 0.

            glBindVertexArray(0);
            CHECK_FOR_GL_ERRORS();


            // Draw the mesh.
            CHECK_FOR_GL_ERRORS();
            glBindVertexArray(vao); // Bind = make it the currently-used object.

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Enable wireframe mode.
            CHECK_FOR_GL_ERRORS();
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0); // Draw using the current shader setup.
            CHECK_FOR_GL_ERRORS();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Disable wireframe mode.

            glBindVertexArray(0);
            CHECK_FOR_GL_ERRORS();

            glDisableVertexAttribArray(0); // Free OpenGL resources.
            glDisableVertexAttribArray(1);
            glDeleteBuffers(1, &ebo);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
            CHECK_FOR_GL_ERRORS();
            return;
        };
        draw_surface_meshes();


        // Show a pop-up with information about DICOMautomaton.
        if(view_toggles.set_about_popup){
            view_toggles.set_about_popup = false;
            ImGui::OpenPopup("AboutPopup");
        }
        if(ImGui::BeginPopupModal("AboutPopup")){
            const std::string version = "DICOMautomaton SDL_Viewer version "_s + DCMA_VERSION_STR;
            ImGui::Text("%s", version.c_str());

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
