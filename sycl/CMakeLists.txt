
cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)

# SYCL compiler selection (Issue 2: toolchain file support).
#
# The preferred approach is to use a CMake toolchain file to select the SYCL
# compiler driver rather than hardcoding CMAKE_CXX_COMPILER here. For example:
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/sycl-adaptivecpp.cmake \
#         -DWITH_EXT_SYCL=ON ..
#
# If CMAKE_CXX_COMPILER is not already set (e.g., via a toolchain file or the
# CXX environment variable), fall back to attempting to locate 'acpp'.
if(NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(_DCMA_ACPP_COMPILER acpp
        HINTS
            /usr/bin /usr/local/bin
            "$ENV{ACPP_ROOT}/bin"
            "$ENV{ADAPTIVECPP_ROOT}/bin"
        DOC "AdaptiveCpp (acpp) SYCL compiler driver"
    )
    if(_DCMA_ACPP_COMPILER)
        set(CMAKE_CXX_COMPILER "${_DCMA_ACPP_COMPILER}")
        message(STATUS "SYCL: auto-detected CXX compiler: ${CMAKE_CXX_COMPILER}")
    else()
        message(WARNING
            "SYCL build: 'acpp' compiler not found and CMAKE_CXX_COMPILER is not set. "
            "Use -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/sycl-adaptivecpp.cmake "
            "or set CXX=/path/to/acpp when invoking cmake.")
    endif()
endif()

project(DICOMautomaton_SYCL LANGUAGES CXX)
#set(DICOMautomaton_VERSION_MAJOR 0)
#set(DICOMautomaton_VERSION_MINOR 0)
#set(DICOMautomaton_VERSION_PATCH 0)


####################################################################################
#                                  User Options
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile slowly, with minimal memory usage."    OFF)

option(WITH_EIGEN     "Compile assuming Eigen is available."                    ON)
option(WITH_NLOPT     "Compile assuming nlopt is available."                    ON)


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


####################################################################################
#                                  Dependencies 
####################################################################################

find_package(Threads REQUIRED)

find_package(Boost REQUIRED COMPONENTS filesystem serialization iostreams thread system)

if(WITH_EIGEN)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EIGEN3 REQUIRED eigen3)
endif()

if(WITH_NLOPT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NLOPT REQUIRED nlopt)
endif()


####################################################################################
#                       Compile Settings INTERFACE Library (Issue 3)
####################################################################################

add_library(sycl_compile_settings INTERFACE)

target_compile_features(sycl_compile_settings INTERFACE cxx_std_17)

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
    target_compile_options(sycl_compile_settings INTERFACE
        -fdiagnostics-color=always
        -fno-var-tracking-assignments
        -Wno-deprecated
        -Wall -Wextra -Wpedantic)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        target_compile_options(sycl_compile_settings INTERFACE -pg -fno-omit-frame-pointer)
    endif()
    if(MEMORY_CONSTRAINED_BUILD)
        target_compile_options(sycl_compile_settings INTERFACE
            "SHELL:--param ggc-min-expand=10"
            "SHELL:--param ggc-min-heapsize=32768")
    endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(sycl_compile_settings INTERFACE
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

# Sanitizers (enabled for SYCL builds).
target_compile_options(sycl_compile_settings INTERFACE
    -O0 -g -fno-omit-frame-pointer -fno-common
    -fsanitize=address -fsanitize-address-use-after-scope
    -fsanitize=pointer-compare -fsanitize=pointer-subtract
    -fsanitize=undefined -fno-sanitize-recover=undefined )
target_compile_definitions(sycl_compile_settings INTERFACE -U_FORTIFY_SOURCE)

# Definitions (Issue 3).
if(WITH_EIGEN)
    message(STATUS "Assuming Eigen is available.")
    target_compile_definitions(sycl_compile_settings INTERFACE DCMA_USE_EIGEN=1)
else()
    message(STATUS "Assuming Eigen is not available.")
endif()

if(WITH_NLOPT)
    message(STATUS "Assuming NLOPT is available.")
    target_compile_definitions(sycl_compile_settings INTERFACE DCMA_USE_NLOPT=1)
else()
    message(STATUS "Assuming NLOPT is not available.")
endif()

# Include directories (Issue 3).
target_include_directories(sycl_compile_settings INTERFACE ${Boost_INCLUDE_DIRS})
if(WITH_EIGEN)
    target_include_directories(sycl_compile_settings INTERFACE ${EIGEN3_INCLUDE_DIRS})
endif()
if(WITH_NLOPT)
    target_include_directories(sycl_compile_settings INTERFACE ${NLOPT_INCLUDE_DIRS})
endif()

# Apply to all targets defined in subdirectories (Issue 3).
link_libraries(sycl_compile_settings)


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)


