
cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)


project(DICOMautomaton_Registration LANGUAGES CXX)


####################################################################################
#                                  User Options
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile slowly, with minimal memory usage."    OFF)

option(WITH_EIGEN     "Compile assuming Eigen is available."                    ON)
option(WITH_NLOPT     "Compile assuming nlopt is available."                    ON)

# When ON, attempt to download missing dependencies via CMake FetchContent (Issue 4).
option(WITH_FETCHCONTENT_FALLBACK "Download missing dependencies if not found." OFF)


####################################################################################
#                                  Configuration
####################################################################################

set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions (e.g., std=gnu++14).

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For use with clang-tidy et al.
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)

# Set the release type. 
if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

include(FetchContent)


####################################################################################
#                                  Dependencies 
####################################################################################

find_package(Threads REQUIRED)

find_package(Boost REQUIRED COMPONENTS filesystem serialization iostreams thread system)

if(WITH_EIGEN)
    find_package(Eigen3 CONFIG QUIET)
    if(NOT Eigen3_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(EIGEN3 eigen3)
    endif()
    if(NOT Eigen3_FOUND AND NOT EIGEN3_FOUND)
        if(WITH_FETCHCONTENT_FALLBACK)
            message(STATUS "Eigen3 not found; fetching via FetchContent (Issue 4).")
            FetchContent_Declare(
                Eigen3
                GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
                GIT_TAG        3.4.0
                GIT_SHALLOW    TRUE
            )
            FetchContent_MakeAvailable(Eigen3)
            set(Eigen3_FOUND TRUE)
        else()
            message(FATAL_ERROR "Eigen3 not found. Install libeigen3-dev or set WITH_FETCHCONTENT_FALLBACK=ON.")
        endif()
    endif()
endif()

if(WITH_NLOPT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NLOPT nlopt)
    if(NOT NLOPT_FOUND)
        if(WITH_FETCHCONTENT_FALLBACK)
            message(STATUS "nlopt not found; fetching via FetchContent (Issue 4).")
            FetchContent_Declare(
                nlopt
                GIT_REPOSITORY https://github.com/stevengj/nlopt.git
                GIT_TAG        v2.7.1
                GIT_SHALLOW    TRUE
            )
            FetchContent_MakeAvailable(nlopt)
        else()
            message(FATAL_ERROR "nlopt not found. Install libnlopt-dev or set WITH_FETCHCONTENT_FALLBACK=ON.")
        endif()
    endif()
endif()


####################################################################################
#                       Compile Settings INTERFACE Library (Issue 3)
####################################################################################
# Bundle all compile options, definitions, and include directories in an
# INTERFACE library so they are properly scoped to targets rather than applied
# globally via add_compile_options() / add_definitions() / include_directories().

add_library(reg_compile_settings INTERFACE)

target_compile_features(reg_compile_settings INTERFACE cxx_std_17)

# Build-type flags.
set(CMAKE_CXX_FLAGS_DEBUG           "-O2 -g")
set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE         "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O3 -g")

if(MEMORY_CONSTRAINED_BUILD)
    if(NOT CMAKE_CXX_FLAGS)
        set(CMAKE_CXX_FLAGS "-O0 -DNDEBUG")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG           "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL      "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE         "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O0 -DNDEBUG")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(reg_compile_settings INTERFACE
        -fdiagnostics-color=always
        -fno-var-tracking-assignments
        -Wno-deprecated
        -Wall -Wextra -Wpedantic)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        target_compile_options(reg_compile_settings INTERFACE
            -pg -fno-omit-frame-pointer)
    endif()
    if(MEMORY_CONSTRAINED_BUILD)
        target_compile_options(reg_compile_settings INTERFACE
            "SHELL:--param ggc-min-expand=10"
            "SHELL:--param ggc-min-heapsize=32768")
    endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(reg_compile_settings INTERFACE
        -fdiagnostics-color=always
        -Wno-deprecated
        -Wall -Wextra -Wpedantic)
    if(WITH_IWYU)
        set(IWYU_INVOCATION iwyu)
        list(APPEND IWYU_INVOCATION "-Xiwyu;--no_comments")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_INVOCATION})
    endif()
endif()

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__arm__     "cstdio" ARCH_IS_ARM)
check_cxx_symbol_exists(__aarch64__ "cstdio" ARCH_IS_ARM64)
if(ARCH_IS_ARM OR ARCH_IS_ARM64)
    message(STATUS "Detected ARM architecture.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

# Compile definitions (Issue 3).
if(WITH_EIGEN)
    message(STATUS "Assuming Eigen is available.")
    target_compile_definitions(reg_compile_settings INTERFACE DCMA_USE_EIGEN=1)
else()
    message(STATUS "Assuming Eigen is not available.")
endif()

if(WITH_NLOPT)
    message(STATUS "Assuming NLOPT is available.")
    target_compile_definitions(reg_compile_settings INTERFACE DCMA_USE_NLOPT=1)
else()
    message(STATUS "Assuming NLOPT is not available.")
endif()

# Include directories (Issue 3).
target_include_directories(reg_compile_settings INTERFACE ${Boost_INCLUDE_DIRS})
if(WITH_EIGEN)
    if(TARGET Eigen3::Eigen)
        target_link_libraries(reg_compile_settings INTERFACE Eigen3::Eigen)
    else()
        target_include_directories(reg_compile_settings INTERFACE ${EIGEN3_INCLUDE_DIRS})
    endif()
endif()
if(WITH_NLOPT)
    target_include_directories(reg_compile_settings INTERFACE ${NLOPT_INCLUDE_DIRS})
endif()

# Apply to all targets defined in subdirectories (Issue 3).
link_libraries(reg_compile_settings)


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)


