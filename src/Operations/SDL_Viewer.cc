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

    //Trim any empty image sets.
    for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
        if((*it)->imagecoll.images.empty()){
            it = DICOM_data.image_data.erase(it);
        }else{
            ++it;
        }
    }
    if(DICOM_data.image_data.empty()) throw std::invalid_argument("No image data available to view. Cannot continue");

    //If, for some reason, several image arrays are available for viewing, we need to provide a means for stepping
    // through the arrays. 
    //
    // NOTE: The reasoning for having several image arrays is not clear cut. If the timestamps are known exactly, it
    //       might make sense to split in this way. In general, it is up to the user to make this call. 
    using img_data_ptr_it_t = decltype(DICOM_data.image_data.begin());
    auto img_array_ptr_beg  = DICOM_data.image_data.begin();
    auto img_array_ptr_end  = DICOM_data.image_data.end();
    auto img_array_ptr_last = std::prev(DICOM_data.image_data.end());
    auto img_array_ptr_it   = img_array_ptr_beg;

    //At the moment, we keep a single 'display' image active at a time. To help walk through the neighbouring images
    // (and the rest of the images, for that matter) we keep a container iterator to the image.
    using disp_img_it_t = decltype(DICOM_data.image_data.front()->imagecoll.images.begin());
    auto disp_img_beg  = (*img_array_ptr_it)->imagecoll.images.begin();
    auto disp_img_end  = (*img_array_ptr_it)->imagecoll.images.end();
    auto disp_img_last = std::prev((*img_array_ptr_it)->imagecoll.images.end());
    auto disp_img_it   = disp_img_beg;

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
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0){
        throw std::invalid_argument("Unable to initialize SDL: "_s + SDL_GetError());
    }

    // Configure the desired OpenGL version (v3.0).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create an SDL window and provide a context we can refer to.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_Window* window = SDL_CreateWindow("DICOMautomaton Interactive Workspace",
                                           SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           1024, 768,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(window == nullptr){
        throw std::runtime_error("Unable to create an SDL window: "_s + SDL_GetError());
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync to limit the frame rate.

    if(glewInit() != GLEW_OK){
        throw std::invalid_argument("Glew was unable to initialize OpenGL");
    }

    // Create an ImGui context we can use and associate it with the OpenGL context.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    [[maybe_unused]] ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    const std::string gl_shader_version = "#version 130";
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(gl_shader_version.c_str());

    // -------------------------------- Functors for various things ---------------------------------------

    const auto Check_For_OpenGL_Errors = [](void){
        GLenum err = glGetError();
        if(err != GL_NO_ERROR){
            throw std::runtime_error("Unexpected OpenGL error ("_s + std::to_string(err) + "). Cannot continue");
        }
    };
    Check_For_OpenGL_Errors();


    // Create an OpenGL texture from an image.
    struct opengl_texture_handle_t {
        GLuint texture_number = 0;
        int col_count = 0;
        int row_count = 0;
    };
    const auto Load_OpenGL_Texture = [&Check_For_OpenGL_Errors,
                                      &custom_centre,
                                      &custom_width,
                                      &colour_maps,
                                      &colour_map,
                                      &nan_colour]( const disp_img_it_t &img_it ) -> opengl_texture_handle_t {
            const auto img_cols = img_it->columns;
            const auto img_rows = img_it->rows;

            if(!isininc(1,img_rows,10000) || !isininc(1,img_cols,10000)){
                FUNCERR("Image dimensions are not reasonable. Is this a mistake? Refusing to continue");
            }

            std::vector<std::byte> animage;
            animage.reserve(img_cols * img_rows * 3);

            //------------------------------------------------------------------------------------------------
            //Apply a window to the data if it seems like the WindowCenter or WindowWidth specified in the image metadata
            // are applicable. Note that it is likely that pixels will be clipped or truncated. This is intentional.
            
            auto img_win_valid = img_it->GetMetadataValueAs<std::string>("WindowValidFor");
            auto img_desc      = img_it->GetMetadataValueAs<std::string>("Description");
            auto img_win_c     = img_it->GetMetadataValueAs<double>("WindowCenter");
            auto img_win_fw    = img_it->GetMetadataValueAs<double>("WindowWidth"); //Full width or range. (Diameter, not radius.)

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
                        const auto val = static_cast<double>( img_it->value(j,i,0) ); //The first (R or gray) channel.
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
                using pixel_value_t = decltype(img_it->value(0, 0, 0));
                const auto pixel_minmax_allchnls = img_it->minmax();
                const auto lowest = std::get<0>(pixel_minmax_allchnls);
                const auto highest = std::get<1>(pixel_minmax_allchnls);

                //const auto lowest = Stats::Percentile(img_it->data, 0.01);
                //const auto highest = Stats::Percentile(img_it->data, 0.99);

                const auto pixel_type_max = static_cast<double>(std::numeric_limits<pixel_value_t>::max());
                const auto pixel_type_min = static_cast<double>(std::numeric_limits<pixel_value_t>::min());
                const auto dest_type_max = static_cast<double>(std::numeric_limits<uint8_t>::max()); //Min is implicitly 0.

                const double clamped_low  = static_cast<double>(lowest )/pixel_type_max;
                const double clamped_high = static_cast<double>(highest)/pixel_type_max;

                for(auto j = 0; j < img_rows; ++j){ 
                    for(auto i = 0; i < img_cols; ++i){ 
                        const auto val = img_it->value(j,i,0);
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
        
            Check_For_OpenGL_Errors();

            opengl_texture_handle_t out;
            out.col_count = img_cols;
            out.row_count = img_rows;
            glGenTextures(1, &out.texture_number);
            glBindTexture(GL_TEXTURE_2D, out.texture_number);

            if(out.texture_number == 0){
                throw std::runtime_error("Unable to assign OpenGL texture");
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, out.col_count, out.row_count, 0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<void*>(animage.data()));

            Check_For_OpenGL_Errors();

            return out;
    };

    // Advance to the specified Image_Array. Also resets necessary display image iterators.
    const auto advance_to_image_array = [&](const int64_t n){
            const int64_t N_arrays = DICOM_data.image_data.size();
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();

            const int64_t index_arrays = std::distance(img_array_ptr_beg, img_array_ptr_it);
            const int64_t index_images = std::distance(disp_img_beg, disp_img_it);

            if(n == index_arrays){
                return; // Already at desired position.
            }
            if((n < 0) || (N_arrays <= n)){
                throw std::logic_error("Unwilling to move to specified Image_Array. It does not exist.");
            }

            custom_width  = std::optional<double>();
            custom_centre = std::optional<double>();

            img_array_ptr_it = std::next(img_array_ptr_beg, n);
            FUNCINFO("There are " << (*img_array_ptr_it)->imagecoll.images.size() << " images in this Image_Array");

            disp_img_beg  = (*img_array_ptr_it)->imagecoll.images.begin();
            disp_img_last = std::prev((*img_array_ptr_it)->imagecoll.images.end());
            disp_img_it   = disp_img_beg;

            // Attempt to move to the Nth image, like we previously had.
            //
            // TODO: It's debatable whether this is even useful. Better to move to same DICOM position, I think.
            if(index_images < (*img_array_ptr_it)->imagecoll.images.size()){
                std::advance(disp_img_it, index_images);
            }

            return;
    };

    // Advance to the specified image in the current Image_Array.
    const auto advance_to_image = [&](const int64_t n){
            const int64_t N_arrays = DICOM_data.image_data.size();
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();

            const int64_t index_arrays = std::distance(img_array_ptr_beg, img_array_ptr_it);
            const int64_t index_images = std::distance(disp_img_beg, disp_img_it);

            if(n == index_images){
                return; // Already at desired position.
            }
            if((n < 0) || (N_images <= n)){
                throw std::logic_error("Unwilling to move to specified image. It does not exist.");
            }

            disp_img_it = std::next(disp_img_beg, n);

            return;
    };

    // ------------------------------------------- Main loop ----------------------------------------------

    // Pre-load the first image.
    opengl_texture_handle_t current_texture;
    current_texture = Load_OpenGL_Texture(disp_img_it);

    while(true){
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
            ImGui::Begin("Main");

            ImGui::Text("This is the main menu window.");

            if(ImGui::Button("Exit Viewer")){
                FUNCINFO("Pressed the [exit] button");
                break;
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Images");

            const int64_t N_arrays = DICOM_data.image_data.size();
            const int64_t N_images = (*img_array_ptr_it)->imagecoll.images.size();

            const int64_t index_arrays = std::distance(img_array_ptr_beg, img_array_ptr_it);
            const int64_t index_images = std::distance(disp_img_beg, disp_img_it);

            int scroll_arrays = index_arrays;
            int scroll_images = index_images;
            ImGui::SliderInt("Array", &scroll_arrays, 0, N_arrays - 1);
            ImGui::SliderInt("Image", &scroll_images, 0, N_images - 1);

            if( static_cast<int64_t>(scroll_arrays) != index_arrays ){
                advance_to_image_array(static_cast<int64_t>(scroll_arrays));
                current_texture = Load_OpenGL_Texture(disp_img_it);
                FUNCINFO("Loaded Image_Array " << std::distance(img_array_ptr_beg,img_array_ptr_it) << ". "
                         "There are " << (*img_array_ptr_it)->imagecoll.images.size() << " images in this Image_Array");

            }else if( static_cast<int64_t>(scroll_images) != index_images ){
                advance_to_image(static_cast<int64_t>(scroll_images));
                current_texture = Load_OpenGL_Texture(disp_img_it);
            }

            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(current_texture.texture_number)), ImVec2(1.5f*current_texture.col_count, 1.5f*current_texture.row_count));

            ImGui::End();
        }

        // Clear the current OpenGL frame.
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(background_colour.x,
                     background_colour.y,
                     background_colour.z,
                     background_colour.w);
        glClear(GL_COLOR_BUFFER_BIT);

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
