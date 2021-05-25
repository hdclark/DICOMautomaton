//SDL_Viewer.cc - A part of DICOMautomaton 2020. Written by hal clark.

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

#include <boost/filesystem.hpp>

#include "../imgui20201021/imgui.h"
#include "../imgui20201021/imgui_impl_sdl.h"
#include "../imgui20201021/imgui_impl_opengl3.h"

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

#include "SDL_Viewer.h"

//extern const std::string DCMA_VERSION_STR;

OperationDoc OpArgDocSDL_Viewer(){
    OperationDoc out;
    out.name = "SDL_Viewer";
    out.desc = 
        "Launch an interactive viewer based on SDL.";

    out.args.emplace_back();
    out.args.back().name = "FPSLimit";
    out.args.back().desc = "The upper limit on the frame rate, in seconds as an unsigned integer."
                           " Note that this value may be treated as a suggestion.";
    out.args.back().default_val = "60";
    out.args.back().expected = true;
    out.args.back().examples = { "60", "30", "10", "1" };

    return out;
}

Drover SDL_Viewer(Drover DICOM_data,
                  const OperationArgPkg& OptArgs,
                  const std::map<std::string, std::string>& InvocationMetadata,
                  const std::string& FilenameLex){

    // ----------------------------------------- User Parameters ------------------------------------------
    const auto FPSLimit = std::stoul( OptArgs.getValueStr("FPSLimit").value() );

    // --------------------------------------- Operational State ------------------------------------------
    Explicator X(FilenameLex);

    // Image viewer state.
    long int img_array_num = -1; // The image array currently displayed.
    long int img_num = -1; // The image currently displayed.

    //Real-time modifiable sticky window and level.
    std::optional<double> custom_width;
    std::optional<double> custom_centre;
    std::optional<double> custom_low;
    std::optional<double> custom_high;

    //A tagged point for measuring distance.
    std::optional<vec3<double>> tagged_pos;

    //Flags for various things.
    bool OnlyShowTagsDifferentToNeighbours = true;
    bool ShowExistingContours = true; //Can be disabled/toggled for, e.g., blind re-contouring.

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

    //Toggle whether existing contours should be displayed.
    const auto toggle_showing_existing_contours = [&](){
            ShowExistingContours = !ShowExistingContours;
            return;
    };

    // ------------------------------------------ Viewer State --------------------------------------------
    bool show_another_window = false;
    auto background_colour = ImVec4(0.025f, 0.087f, 0.118f, 1.00f);


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
                                           1024, 768,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
        int col_count = 0;
        int row_count = 0;
    };
    opengl_texture_handle_t current_texture;

    const auto Load_OpenGL_Texture = [&custom_centre,
                                      &custom_width,
                                      &colour_maps,
                                      &colour_map,
                                      &nan_colour]( const planar_image<float,double>& img ) -> opengl_texture_handle_t {
            const auto img_cols = img.columns;
            const auto img_rows = img.rows;

            if(!isininc(1,img_rows,10000) || !isininc(1,img_cols,10000)){
                FUNCERR("Image dimensions are not reasonable. Is this a mistake? Refusing to continue");
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
                                                : (  img_win_valid && img_desc && img_win_c 
                                                  && img_win_fw && (img_win_valid.value() == img_desc.value()));

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
                        const auto val = static_cast<double>( img.value(j,i,0) ); //The first (R or gray) channel.
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
                const auto pixel_minmax_allchnls = img.minmax();
                const auto lowest = std::get<0>(pixel_minmax_allchnls);
                const auto highest = std::get<1>(pixel_minmax_allchnls);

                //const auto lowest = Stats::Percentile(img.data, 0.01);
                //const auto highest = Stats::Percentile(img.data, 0.99);

                const auto pixel_type_max = static_cast<double>(std::numeric_limits<pixel_value_t>::max());
                const auto pixel_type_min = static_cast<double>(std::numeric_limits<pixel_value_t>::min());
                const auto dest_type_max = static_cast<double>(std::numeric_limits<uint8_t>::max()); //Min is implicitly 0.

                const double clamped_low  = static_cast<double>(lowest )/pixel_type_max;
                const double clamped_high = static_cast<double>(highest)/pixel_type_max;

                for(auto j = 0; j < img_rows; ++j){ 
                    for(auto i = 0; i < img_cols; ++i){ 
                        const auto val = img.value(j,i,0);
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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, out.col_count, out.row_count, 0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<void*>(animage.data()));
            CHECK_FOR_GL_ERRORS();

            return out;
    };

    // Recompute the image viewer state, e.g., after the image data is altered by another operation.
    const auto recompute_image_state = [&](){

        //Trim any empty image arrays.
        for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
            if((*it)->imagecoll.images.empty()){
                it = DICOM_data.image_data.erase(it);
            }else{
                ++it;
            }
        }

        // Assess whether there is image data.
        const auto has_images = DICOM_data.Has_Image_Data();
        if( has_images
        &&  (0 <= img_array_num)
        &&  (0 <= img_num) ){
            // Do nothing, but validate (below) that the images are accessible.
        }else if( has_images
              &&  ((img_array_num < 0) || (img_num < 0)) ){
            img_array_num = 0;
            img_num = 0;
        }else{
            img_array_num = -1;
            img_num = -1;
        }

        // Set the current image array and image iters and load the texture.
        if( has_images
        &&  (0 <= img_array_num)
        &&  (0 <= img_num) ){
            if( !isininc(1, img_array_num+1, DICOM_data.image_data.size()) ) return;
            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);

            if( !isininc(1, img_num+1, (*img_array_ptr_it)->imagecoll.images.size()) ) return;
            auto disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);

            current_texture = Load_OpenGL_Texture(*disp_img_it);
        }
        return;
    };

    // Advance to the specified Image_Array. Also resets necessary display image iterators.
    const auto advance_to_image_array = [&](const long int n){
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

            // Invalidate any custom window and level.
            custom_width  = std::optional<double>();
            custom_centre = std::optional<double>();

            return;
    };

    // Advance to the specified image in the current Image_Array.
    const auto advance_to_image = [&](const long int n){
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

    // ------------------------------------------- Main loop ----------------------------------------------

    bool set_about_popup = false;
    bool open_files_enabled = false;
    bool view_images_enabled = true;
    bool view_image_metadata_enabled = false;
    bool view_meshes_enabled = true;
    bool show_image_hover_tooltips = true;
    bool adjust_window_level_enabled = false;
    bool adjust_colour_map_enabled = false;


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


    struct loaded_files_res {
        bool res;
        Drover DICOM_data;
    };
    std::future<loaded_files_res> loaded_files;


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

        if(ImGui::BeginMainMenuBar()){
            if(ImGui::BeginMenu("File")){
                if(ImGui::MenuItem("Open", "ctrl+o", &open_files_enabled)){
                    query_files(open_file_root);
                }
                //if(ImGui::MenuItem("Open", "ctrl+o")){
                //    ImGui::OpenPopup("OpenFileSelector");
                //}
                ImGui::Separator();
                if(ImGui::MenuItem("Exit", "ctrl+q")){
                    ImGui::EndMenu();
                    break;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if(ImGui::BeginMenu("View")){
                ImGui::MenuItem("Images", nullptr, &view_images_enabled);
                ImGui::MenuItem("Image Metadata", nullptr, &view_image_metadata_enabled);
                ImGui::MenuItem("Image Hover Tooltips", nullptr, &show_image_hover_tooltips);
                ImGui::MenuItem("Meshes", nullptr, &view_meshes_enabled);
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Adjust")){
                ImGui::MenuItem("Window and Level", nullptr, &adjust_window_level_enabled);
                ImGui::MenuItem("Image Colour Map", nullptr, &adjust_colour_map_enabled);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if(ImGui::BeginMenu("Help", "ctrl+h")){
                if(ImGui::MenuItem("About")){
                    set_about_popup = true;
                }
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

        if( view_images_enabled
        &&  DICOM_data.Has_Image_Data()
        &&  (0 <= img_array_num)
        &&  (0 <= img_num) ){
            ImGui::Begin("Images", &view_images_enabled);

            auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
            auto disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);

            int scroll_arrays = img_array_num;
            int scroll_images = img_num;
            {
                const int N_arrays = DICOM_data.image_data.size();
                const int N_images = (*img_array_ptr_it)->imagecoll.images.size();
                ImGui::SliderInt("Array", &scroll_arrays, 0, N_arrays - 1);
                ImGui::SliderInt("Image", &scroll_images, 0, N_images - 1);
            }
            long int new_img_array_num = scroll_arrays;
            long int new_img_num = scroll_images;

            if( new_img_array_num != img_array_num ){
                advance_to_image_array(new_img_array_num);
                img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
                disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);
                current_texture = Load_OpenGL_Texture(*disp_img_it);

            }else if( new_img_num != img_num ){
                advance_to_image(new_img_num);
                disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);
                current_texture = Load_OpenGL_Texture(*disp_img_it);
            }

            // Note: unhappy with this. Can cause feedback loop and flicker/jumpiness when resizing. Works OK for now
            // though. TODO.
            const auto img_rows = disp_img_it->rows;
            const auto img_cols = disp_img_it->columns;
            const auto img_rows_f = static_cast<float>(disp_img_it->rows);
            const auto img_cols_f = static_cast<float>(disp_img_it->columns);
            ImVec2 window_size = ImGui::GetContentRegionAvail();
            window_size.x = std::max(512.0f, window_size.x - 5.0f);
            // Ensure images have the same aspect ratio as the true image.
            window_size.y = (disp_img_it->pxl_dx / disp_img_it->pxl_dy) * window_size.x * img_rows_f / img_cols_f;
            window_size.y = std::isfinite(window_size.y) ? window_size.y : (disp_img_it->pxl_dx / disp_img_it->pxl_dy) * window_size.x;
            auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(current_texture.texture_number));
{
            ImGuiIO &io = ImGui::GetIO();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::Image(gl_tex_ptr, window_size);
            if( ImGui::IsItemHovered() 
            &&  show_image_hover_tooltips ){
                ImGui::BeginTooltip();
                const auto region_x = std::clamp((io.MousePos.x - pos.x) / window_size.x, 0.0f, 1.0f);
                const auto region_y = std::clamp((io.MousePos.y - pos.y) / window_size.y, 0.0f, 1.0f);
                const auto r = std::clamp( static_cast<long int>( std::floor( region_y * img_rows_f ) ), 0L, (img_rows-1) );
                const auto c = std::clamp( static_cast<long int>( std::floor( region_x * img_cols_f ) ), 0L, (img_cols-1) );
                const auto zero_pos = disp_img_it->position(0L, 0L);
                const auto dicom_pos = zero_pos + disp_img_it->row_unit * region_y * disp_img_it->pxl_dx * img_rows_f
                                                + disp_img_it->col_unit * region_x * disp_img_it->pxl_dy * img_cols_f
                                                - disp_img_it->row_unit * 0.5 * disp_img_it->pxl_dx
                                                - disp_img_it->col_unit * 0.5 * disp_img_it->pxl_dy;
                ImGui::Text("Image coordinates: %.4f, %.4f", region_y, region_x);
                ImGui::Text("Pixel coordinates: (r, c) = %ld, %ld", r, c);
                ImGui::Text("Mouse coordinates: (x, y, z) = %.4f, %.4f, %.4f", dicom_pos.x, dicom_pos.y, dicom_pos.z);
                const auto voxel_pos = disp_img_it->position(r,c);
                ImGui::Text("Voxel coordinates: (x, y, z) = %.4f, %.4f, %.4f", voxel_pos.x, voxel_pos.y, voxel_pos.z);
                if(disp_img_it->channels == 1){
                    ImGui::Text("Voxel intensity:   %.4f", disp_img_it->value(r, c, 0L));
                }else{
                    std::stringstream ss;
                    for(long int chan = 0; chan < disp_img_it->channels; ++chan){
                        ss << disp_img_it->value(r, c, chan);
                    }
                    ImGui::Text("Voxel intensities: %s", ss.str().c_str());
                }
                ImGui::EndTooltip();
            }
}

            ImGui::End();

            // Metadata window.
            if( view_image_metadata_enabled ){
                ImGui::SetNextWindowSize(ImVec2(650, 650), ImGuiCond_FirstUseEver);
                ImGui::Begin("Image Metadata", &view_image_metadata_enabled);

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
        }

        // Open files dialog.
        if( open_files_enabled
        &&  !loaded_files.valid()){

            ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_FirstUseEver);
            ImGui::Begin("Open File", &open_files_enabled);

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
                std::list<boost::filesystem::path> paths;
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
                    lfs.res = Load_Files(lfs.DICOM_data, InvocationMetadata, FilenameLex, paths_l);
                    return lfs;
                });

            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(120, 0))){ 
                open_files_enabled = false;
                open_files_selection.clear();
                // Reset the root directory.
                open_file_root = std::filesystem::current_path();
            }
            ImGui::End();
        }

        // Handle file loading future.
        if(loaded_files.valid()){
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
                auto f = loaded_files.get();

                if(f.res){
                    open_files_enabled = false;
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


        // Adjust the window and level.
        if(adjust_window_level_enabled){
            ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin("Adjust Window and Level", &adjust_window_level_enabled);
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
                sync_custom_wllh();
                recompute_image_state();
            }
        }


        // Adjust the colour map.
        if(adjust_colour_map_enabled){
            ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_FirstUseEver);
            ImGui::Begin("Adjust Colour Map", &adjust_colour_map_enabled);
            bool reload_texture = false;

            for(size_t i = 0; i < colour_maps.size(); ++i){
                if( ImGui::Button(colour_maps[i].first.c_str(), ImVec2(250, 0)) ){
                    colour_map = i;
                    reload_texture = true;
                }
            }
            ImGui::End();

            if(reload_texture){
                recompute_image_state();
            }
        }

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
        if( view_meshes_enabled
        && DICOM_data.Has_Mesh_Data() ){
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
}

        // Show a pop-up with information about DICOMautomaton.
        if(set_about_popup){
            set_about_popup = false;
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

    // OpenGL and SDL cleanup.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return DICOM_data;
}
