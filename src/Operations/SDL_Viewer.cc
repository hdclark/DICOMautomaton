//SDL_Viewer.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>            //Needed for exit() calls.
#include <cstdio>
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
    //disp_img_it_t disp_img_end  = (*img_array_ptr_it)->imagecoll.images.end();  // Not used atm.
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

    //Accumulation-type storage.
    contour_collection<double> contour_coll_shtl; //Stores contours in the DICOM coordinate system.
    contour_coll_shtl.contours.emplace_back();    //Prime the shuttle with an empty contour.
    contour_coll_shtl.contours.back().closed = true;

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
    //SDL_GL_SetSwapInterval(1); // Enable vsync

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

    // ------------------------------------------- Main loop ----------------------------------------------
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

            if(ImGui::Button("Exit")){
                FUNCINFO("Pressed the [exit] button");
                break;
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Secondary");

            ImGui::Text("This is a secondary window.");

            if(ImGui::Button("Exit")){
                FUNCINFO("Pressed the [exit] button");
                break;
            }

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
