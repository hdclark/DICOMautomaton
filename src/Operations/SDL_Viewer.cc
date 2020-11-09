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

#include "../Colour_Maps.h"
#include "../Common_Boost_Serialization.h"
#include "../Common_Plotting.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"

#include "../Font_DCMA_Minimal.h"

#include "SDL_Viewer.h"


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
                  const std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& FilenameLex){

    // ----------------------------------------- User Parameters ------------------------------------------
    const auto FPSLimit = std::stoul( OptArgs.getValueStr("FPSLimit").value() );

    // --------------------------------------- Operational State ------------------------------------------
    Explicator X(FilenameLex);

    // Image viewer state.
    long int img_array_num = -1; // The image array currently displayed.
    long int img_num = -1; // The image currently displayed.
    //Trim any empty image arrays.
    for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
        if((*it)->imagecoll.images.empty()){
            it = DICOM_data.image_data.erase(it);
        }else{
            ++it;
        }
    }
    if(DICOM_data.Has_Image_Data()){
        img_array_num = 0;
        img_num = 0;
    }

    //Real-time modifiable sticky window and level.
    std::optional<double> custom_width;
    std::optional<double> custom_centre;

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


            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            CHECK_FOR_GL_ERRORS();

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, out.col_count, out.row_count, 0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<void*>(animage.data()));
            CHECK_FOR_GL_ERRORS();

            return out;
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

    // Pre-load the first image.
    opengl_texture_handle_t current_texture;
    if( DICOM_data.Has_Image_Data()
    &&  (0 <= img_array_num)
    &&  (0 <= img_num) ){
        auto img_array_ptr_it = std::next(DICOM_data.image_data.begin(), img_array_num);
        auto disp_img_it = std::next((*img_array_ptr_it)->imagecoll.images.begin(), img_num);
        current_texture = Load_OpenGL_Texture(*disp_img_it);
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

        {
            if(ImGui::BeginMainMenuBar()){
                if(ImGui::BeginMenu("File")){
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Edit")){
                    if(ImGui::MenuItem("Undo", "CTRL+Z")){}
                    if(ImGui::MenuItem("Redo", "CTRL+Y", false, false)){}  // Disabled item
                    ImGui::Separator();
                    if(ImGui::MenuItem("Cut", "CTRL+X")){}
                    if(ImGui::MenuItem("Copy", "CTRL+C")){}
                    if(ImGui::MenuItem("Paste", "CTRL+V")){}
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if(ImGui::BeginMenu("Exit", "CTRL+Q")){
                    ImGui::EndMenu();
                    break;
                }
                ImGui::EndMainMenuBar();
            }
        }

        {
            ImGui::Begin("Main");

            ImGui::Text("%s", "This is the main menu window.");

            if(ImGui::Button("Exit Viewer")){
                FUNCINFO("Pressed the [exit] button");
                break;
            }

            ImGui::End();
        }

        if( DICOM_data.Has_Image_Data()
        &&  (0 <= img_array_num)
        &&  (0 <= img_num) ){
            ImGui::Begin("Images");

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
            ImVec2 window_size = ImGui::GetContentRegionAvail();
            window_size.x = std::max(512.0f, window_size.x);
            // Ensure images have the same aspect ratio as the true image.
            window_size.y = window_size.x * static_cast<float>(disp_img_it->rows) / static_cast<float>(disp_img_it->columns);
            window_size.y = std::isfinite(window_size.y) ? window_size.y : window_size.x;
            auto gl_tex_ptr = reinterpret_cast<void*>(static_cast<intptr_t>(current_texture.texture_number));
            ImGui::Image(gl_tex_ptr, window_size);

            ImGui::End();
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
if(DICOM_data.Has_Mesh_Data()){
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
        // Scale each of x, y, and z to [-1,+1], but shrink to [-1/sqrt(2),+1/sqrt(2)] to account for rotation.
        // Scaling down will ensure the corners are not clipped when the cube is rotated.
        vec3<double> w( 0.707 * (2.0 * (v.x - x_min) / (x_max - x_min) - 1.0),
                        0.707 * (2.0 * (v.y - y_min) / (y_max - y_min) - 1.0),
                        0.707 * (2.0 * (v.z - z_min) / (z_max - z_min) - 1.0) );

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
