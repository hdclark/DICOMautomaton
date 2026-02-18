# CMakeLists.txt for the DICOMautomaton Android SDL Viewer native library.
#
# This file is processed by the Android Gradle plugin via the
# 'externalNativeBuild' block in app/build.gradle.
#
# The build compiles a shared library (dcma_sdl_viewer) from:
#   - The SDL Viewer entry point (dcma_sdl_viewer_main.cc)
#   - The minimal set of DICOMautomaton source files needed to support SDL_Viewer
#   - Pre-built SDL2 and dependent libraries cross-compiled for Android
#
# Key assumptions:
#   - DCMA_SRC_DIR points to the DICOMautomaton src/ directory.
#   - SDL2 has been cross-compiled for Android and its headers/libs are available
#     via the variables SDL2_INCLUDE_DIR and SDL2_LIBRARY.
#   - The Ygor, YgorClustering, and Explicator libraries have been similarly
#     cross-compiled and are available via YGOR_INCLUDE_DIR, YGOR_LIBRARY, etc.
#   - All paths are passed in from the build script as CMake cache variables.
#
# This is an MVP build: optional features (CGAL, nlopt, etc.) are disabled.

cmake_minimum_required(VERSION 3.22.1)
project(dcma_sdl_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Paths (overridable via -D on the cmake command line or from Gradle)
# ---------------------------------------------------------------------------

# Root of the DICOMautomaton repository (i.e., the directory containing src/).
if(NOT DEFINED DCMA_REPO_ROOT)
    # When invoked by Gradle the source dir is android/app/src/main/cpp,
    # so the repo root is five levels up.
    get_filename_component(DCMA_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
endif()
set(DCMA_SRC_DIR "${DCMA_REPO_ROOT}/src")

message(STATUS "DCMA_REPO_ROOT = ${DCMA_REPO_ROOT}")
message(STATUS "DCMA_SRC_DIR   = ${DCMA_SRC_DIR}")

# ---------------------------------------------------------------------------
# Per-ABI dependency prefix
# ---------------------------------------------------------------------------
# When cross-compiled via the Android NDK toolchain, CMAKE_ANDROID_ARCH_ABI
# is set automatically (e.g. "arm64-v8a").  We derive the dependency prefix
# from this variable so that a single cmake invocation can locate the correct
# pre-built libraries.

if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    set(_DEFAULT_DEPS_PREFIX "/android-deps/${CMAKE_ANDROID_ARCH_ABI}")
else()
    set(_DEFAULT_DEPS_PREFIX "/android-deps/arm64-v8a")
endif()

set(ANDROID_DEPS_PREFIX "${_DEFAULT_DEPS_PREFIX}" CACHE PATH
    "Root of pre-built Android dependencies for the current ABI.")

message(STATUS "CMAKE_ANDROID_ARCH_ABI = ${CMAKE_ANDROID_ARCH_ABI}")
message(STATUS "ANDROID_DEPS_PREFIX    = ${ANDROID_DEPS_PREFIX}")

# Set defaults derived from ANDROID_DEPS_PREFIX (can be overridden via -D).
if(NOT DEFINED SDL2_INCLUDE_DIR)
    set(SDL2_INCLUDE_DIR "${ANDROID_DEPS_PREFIX}/include/SDL2")
endif()
if(NOT DEFINED SDL2_LIBRARY)
    set(SDL2_LIBRARY "${ANDROID_DEPS_PREFIX}/lib/libSDL2.so")
endif()
if(NOT DEFINED YGOR_INCLUDE_DIR)
    set(YGOR_INCLUDE_DIR "${ANDROID_DEPS_PREFIX}/include")
endif()
if(NOT DEFINED YGOR_LIBRARY)
    set(YGOR_LIBRARY "${ANDROID_DEPS_PREFIX}/lib/libygor.a")
endif()
if(NOT DEFINED EXPLICATOR_INCLUDE_DIR)
    set(EXPLICATOR_INCLUDE_DIR "${ANDROID_DEPS_PREFIX}/include")
endif()
if(NOT DEFINED EXPLICATOR_LIBRARY)
    set(EXPLICATOR_LIBRARY "${ANDROID_DEPS_PREFIX}/lib/libexplicator.a")
endif()

# ---------------------------------------------------------------------------
# Find required external packages (pre-built for Android ABI).
# ---------------------------------------------------------------------------

# SDL2: use explicit paths (pre-compiled for Android, not system SDL2).
if(EXISTS "${SDL2_LIBRARY}" AND EXISTS "${SDL2_INCLUDE_DIR}")
    message(STATUS "Using pre-built SDL2: ${SDL2_LIBRARY}")
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
else()
    message(FATAL_ERROR
        "SDL2 not found at ${SDL2_LIBRARY}. "
        "Pre-compile SDL2 for Android and set ANDROID_DEPS_PREFIX or "
        "SDL2_INCLUDE_DIR / SDL2_LIBRARY.")
endif()

# OpenGL ES 3 (provided by the Android NDK).
find_library(GLES3_LIB GLESv3)
if(NOT GLES3_LIB)
    find_library(GLES3_LIB GLESv2)
endif()
find_library(EGL_LIB EGL)
find_library(ANDROID_LIB android)
find_library(LOG_LIB log)

# ---------------------------------------------------------------------------
# Ygor library
# ---------------------------------------------------------------------------
if(EXISTS "${YGOR_LIBRARY}" AND EXISTS "${YGOR_INCLUDE_DIR}")
    message(STATUS "Using pre-built Ygor: ${YGOR_LIBRARY}")
    add_library(Ygor STATIC IMPORTED)
    set_target_properties(Ygor PROPERTIES
        IMPORTED_LOCATION "${YGOR_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${YGOR_INCLUDE_DIR}"
    )
else()
    message(FATAL_ERROR
        "Ygor not found at ${YGOR_LIBRARY}. "
        "Pre-compile Ygor for Android and set ANDROID_DEPS_PREFIX or "
        "YGOR_INCLUDE_DIR / YGOR_LIBRARY.")
endif()

# ---------------------------------------------------------------------------
# Explicator library
# ---------------------------------------------------------------------------
if(EXISTS "${EXPLICATOR_LIBRARY}" AND EXISTS "${EXPLICATOR_INCLUDE_DIR}")
    message(STATUS "Using pre-built Explicator: ${EXPLICATOR_LIBRARY}")
    add_library(Explicator STATIC IMPORTED)
    set_target_properties(Explicator PROPERTIES
        IMPORTED_LOCATION "${EXPLICATOR_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${EXPLICATOR_INCLUDE_DIR}"
    )
else()
    message(FATAL_ERROR
        "Explicator not found at ${EXPLICATOR_LIBRARY}. "
        "Pre-compile Explicator for Android and set ANDROID_DEPS_PREFIX or "
        "EXPLICATOR_INCLUDE_DIR / EXPLICATOR_LIBRARY.")
endif()

# ---------------------------------------------------------------------------
# Boost (required by Common_Boost_Serialization.h and several other headers).
# On Android, Boost must be cross-compiled or supplied as pre-built headers+libs.
# ---------------------------------------------------------------------------
if(NOT DEFINED BOOST_ROOT AND DEFINED ANDROID_DEPS_PREFIX)
    set(BOOST_ROOT "${ANDROID_DEPS_PREFIX}")
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost QUIET COMPONENTS
    filesystem
    iostreams
    serialization
    program_options
    thread
)
if(Boost_FOUND)
    message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")
    include_directories(${Boost_INCLUDE_DIRS})
else()
    # If Boost is not found via find_package, try explicit prefix.
    if(EXISTS "${ANDROID_DEPS_PREFIX}/include/boost/version.hpp")
        message(STATUS "Using Boost headers at ${ANDROID_DEPS_PREFIX}/include")
        include_directories("${ANDROID_DEPS_PREFIX}/include")
        # Provide stub link targets for Boost component libraries.
        foreach(_boost_comp filesystem iostreams serialization program_options thread)
            set(_lib "${ANDROID_DEPS_PREFIX}/lib/libboost_${_boost_comp}.a")
            if(EXISTS "${_lib}")
                add_library("Boost::${_boost_comp}" STATIC IMPORTED)
                set_target_properties("Boost::${_boost_comp}" PROPERTIES
                    IMPORTED_LOCATION "${_lib}")
                list(APPEND BOOST_LIBS "${_lib}")
            endif()
        endforeach()
    else()
        message(WARNING
            "Boost not found. The build may fail if Boost headers/libraries "
            "are required. Set BOOST_ROOT or ANDROID_DEPS_PREFIX to a "
            "directory containing pre-built Boost for Android.")
    endif()
endif()


# ---------------------------------------------------------------------------
# Compile definitions matching Android/OpenGL ES environment
# ---------------------------------------------------------------------------
add_definitions(
    -DDCMA_USE_SDL=1
    -DANDROID
    -DIMGUI_IMPL_OPENGL_ES3
)

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------
# NOTE: The android_shims directory must come first so that the GLEW shim
# header (GL/glew.h) is found before any system GLEW installation.
# The shim replaces GLEW with OpenGL ES 3 stubs on Android.
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/android_shims"
    "${DCMA_SRC_DIR}"
    "${DCMA_SRC_DIR}/imgui20210904"
    "${DCMA_SRC_DIR}/implot20210904"
    "${DCMA_SRC_DIR}/stbnothings20230607"
    "${DCMA_SRC_DIR}/doctest20251212"
)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
# We compile a minimal subset of DICOMautomaton source files sufficient to
# support SDL_Viewer. The full dispatcher and all operations are not needed;
# only SDL_Viewer and its direct dependencies are included.

# ImGui / ImPlot sources (always required by SDL_Viewer)
set(IMGUI_SOURCES
    "${DCMA_SRC_DIR}/imgui20210904/imgui.cpp"
    "${DCMA_SRC_DIR}/imgui20210904/imgui_demo.cpp"
    "${DCMA_SRC_DIR}/imgui20210904/imgui_draw.cpp"
    "${DCMA_SRC_DIR}/imgui20210904/imgui_tables.cpp"
    "${DCMA_SRC_DIR}/imgui20210904/imgui_widgets.cpp"
    "${DCMA_SRC_DIR}/imgui20210904/imgui_impl_sdl.cpp"
    "${DCMA_SRC_DIR}/imgui20210904/imgui_impl_opengl3.cpp"
    "${DCMA_SRC_DIR}/implot20210904/implot.cpp"
    "${DCMA_SRC_DIR}/implot20210904/implot_items.cpp"
)

# Core DICOMautomaton sources needed by SDL_Viewer
set(DCMA_CORE_SOURCES
    "${DCMA_SRC_DIR}/Structs.cc"
    "${DCMA_SRC_DIR}/Metadata.cc"
    "${DCMA_SRC_DIR}/Regex_Selectors.cc"
    "${DCMA_SRC_DIR}/String_Parsing.cc"
    "${DCMA_SRC_DIR}/Operation_Dispatcher.cc"
    "${DCMA_SRC_DIR}/File_Loader.cc"
    "${DCMA_SRC_DIR}/Script_Loader.cc"
    "${DCMA_SRC_DIR}/Standard_Scripts.cc"
    "${DCMA_SRC_DIR}/Standard_Guides.cc"
    "${DCMA_SRC_DIR}/Colour_Maps.cc"
    "${DCMA_SRC_DIR}/Font_DCMA_Minimal.cc"
    "${DCMA_SRC_DIR}/DCMA_Version.cc"
    "${DCMA_SRC_DIR}/Alignment_Rigid.cc"
    "${DCMA_SRC_DIR}/Surface_Meshes.cc"
    "${DCMA_SRC_DIR}/Thread_Pool.cc"
    "${DCMA_SRC_DIR}/Dialogs.cc"
    "${DCMA_SRC_DIR}/Documentation.cc"
    "${DCMA_SRC_DIR}/STB_Shim.cc"
    "${DCMA_SRC_DIR}/Triple_Three.cc"
    "${DCMA_SRC_DIR}/Common_Boost_Serialization.cc"
    "${DCMA_SRC_DIR}/Common_Plotting.cc"
    "${DCMA_SRC_DIR}/Dialogs/Tray_Notification.cc"
)

# SDL_Viewer operation and its sub-module dependencies
set(SDL_VIEWER_SOURCES
    "${DCMA_SRC_DIR}/Operations/SDL_Viewer.cc"
    "${DCMA_SRC_DIR}/YgorImages_Functors/Compute/AccumulatePixelDistributions.cc"
)

# Challenge / mini-game sources included by SDL_Viewer
set(CHALLENGE_SOURCES
    "${DCMA_SRC_DIR}/Challenges/Encompass.cc"
    "${DCMA_SRC_DIR}/Challenges/FreeSki.cc"
    "${DCMA_SRC_DIR}/Challenges/MazeExplorer.cc"
    "${DCMA_SRC_DIR}/Challenges/GuitarFret.cc"
    "${DCMA_SRC_DIR}/Challenges/Lawn.cc"
    "${DCMA_SRC_DIR}/Challenges/TacticalStealth.cc"
    "${DCMA_SRC_DIR}/Challenges/TowerDefence.cc"
    "${DCMA_SRC_DIR}/Challenges/Tracks.cc"
    "${DCMA_SRC_DIR}/Challenges/RotatingCube.cc"
    "${DCMA_SRC_DIR}/Challenges/Werewolf.cc"
)

# ---------------------------------------------------------------------------
# Build the shared library
# ---------------------------------------------------------------------------
add_library(dcma_sdl_viewer SHARED
    dcma_sdl_viewer_main.cc
    ${IMGUI_SOURCES}
    ${DCMA_CORE_SOURCES}
    ${SDL_VIEWER_SOURCES}
    ${CHALLENGE_SOURCES}
)

target_link_libraries(dcma_sdl_viewer
    PRIVATE
        SDL2::SDL2
        Ygor
        Explicator
        "${GLES3_LIB}"
        "${EGL_LIB}"
        "${ANDROID_LIB}"
        "${LOG_LIB}"
)

# Link Boost component libraries if found.
if(Boost_FOUND)
    target_link_libraries(dcma_sdl_viewer PRIVATE
        Boost::filesystem
        Boost::iostreams
        Boost::serialization
        Boost::thread
    )
elseif(DEFINED BOOST_LIBS)
    target_link_libraries(dcma_sdl_viewer PRIVATE ${BOOST_LIBS})
endif()

# Strip debug symbols in release builds to reduce APK size.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET dcma_sdl_viewer POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded
            $<TARGET_FILE:dcma_sdl_viewer>)
endif()
