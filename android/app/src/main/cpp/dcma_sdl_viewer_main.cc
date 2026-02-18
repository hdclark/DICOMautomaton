// dcma_sdl_viewer_main.cc - DICOMautomaton Android SDL Viewer entry point.
//
// This file provides the SDL_main() function that SDL's Android glue calls
// after the Java GameActivity has set up the window and OpenGL ES context.
//
// The intent is:
//   1. Android opens the app -> GameActivity starts -> native lib is loaded.
//   2. SDL's android glue invokes SDL_main() on a dedicated thread.
//   3. SDL_main() initialises a minimal Drover and calls SDL_Viewer().
//   4. When SDL_Viewer() returns (user closes window / back button), the app exits.
//
// NOTE: This is an MVP / feasibility build.  Not all SDL_Viewer features will
// work on Android out-of-the-box (e.g. file dialogs, GLEW is replaced by
// OpenGL ES), but the goal is to establish a working build pipeline and
// demonstrate the basic app lifecycle.

#include <string>
#include <map>
#include <list>
#include <memory>

// SDL2 provides SDL_main on Android, which renames main() to SDL_main().
#include <SDL.h>

// Minimal DICOMautomaton headers needed to construct a Drover and call SDL_Viewer.
#include "Structs.h"
#include "Operations/SDL_Viewer.h"
#include "Operation_Dispatcher.h"

// Android log support.
#ifdef __ANDROID__
#include <android/log.h>
#define DCMA_ANDROID_LOG(msg) \
    __android_log_print(ANDROID_LOG_INFO, "DICOMautomaton", "%s", (msg))
#else
#define DCMA_ANDROID_LOG(msg) SDL_Log("%s", (msg))
#endif

// SDL_main is the C++ entry point invoked by SDL's Android Java glue after
// GameActivity has initialised the display surface.
extern "C" int SDL_main(int /*argc*/, char** /*argv*/) {
    DCMA_ANDROID_LOG("DICOMautomaton SDL Viewer starting on Android");

    try {
        // Construct a minimal, empty Drover (no DICOM data loaded).
        // The SDL_Viewer will display an empty workspace that the user can
        // interact with — consistent with launching the desktop viewer with no
        // files on the command line.
        Drover DICOM_data;

        // InvocationMetadata carries key-value pairs consumed by some operations.
        std::map<std::string, std::string> InvocationMetadata;

        // FilenameLex is the path to the lexicon file used by the Explicator
        // library.  Leave empty; SDL_Viewer handles a missing lexicon gracefully.
        std::string FilenameLex;

        // Build the OperationArgPkg with default parameters for SDL_Viewer.
        OperationArgPkg sdl_viewer_args("SDL_Viewer");

        // Populate default parameter values from the operation documentation so
        // that SDL_Viewer() receives a valid, fully-specified argument package.
        const auto op_doc = OpArgDocSDL_Viewer();
        for(const auto& arg : op_doc.args){
            if(!arg.default_val.empty()){
                sdl_viewer_args.insert(arg.name, arg.default_val);
            }
        }

        DCMA_ANDROID_LOG("Launching SDL_Viewer operation");

        // Run the SDL_Viewer. This function blocks until the viewer is closed.
        const bool ok = SDL_Viewer(DICOM_data, sdl_viewer_args, InvocationMetadata, FilenameLex);

        if(!ok){
            DCMA_ANDROID_LOG("SDL_Viewer returned failure status");
        }

    } catch(const std::exception& e) {
#ifdef __ANDROID__
        // Use __android_log_assert so that the exception appears as a fatal
        // native crash in logcat with a visible tag, rather than a silent exit.
        __android_log_assert("SDL_main", "DICOMautomaton",
                             "Fatal std::exception: %s", e.what());
#else
        DCMA_ANDROID_LOG(e.what());
        return 1;
#endif
    } catch(...) {
#ifdef __ANDROID__
        __android_log_assert("SDL_main", "DICOMautomaton",
                             "Fatal unknown exception in SDL_main");
#else
        DCMA_ANDROID_LOG("Unknown exception in SDL_main");
        return 1;
#endif
    }

    DCMA_ANDROID_LOG("DICOMautomaton SDL Viewer exiting");
    return 0;
}
