# Musl libc Toolchain File
#
# This toolchain file sets up compilation with musl libc for static linking.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-musl.cmake ..
#
# Commonly used with Alpine Linux or musl-cross-make toolchains.

# System configuration
set(CMAKE_SYSTEM_NAME Linux)

# Target architecture - can be overridden
if(NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH "x86_64")
endif()

if(TARGET_ARCH STREQUAL "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
    set(TARGET_TRIPLE "x86_64-linux-musl")
elseif(TARGET_ARCH STREQUAL "aarch64")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(TARGET_TRIPLE "aarch64-linux-musl")
elseif(TARGET_ARCH STREQUAL "armv7")
    set(CMAKE_SYSTEM_PROCESSOR arm)
    set(TARGET_TRIPLE "arm-linux-musleabihf")
else()
    message(FATAL_ERROR "Unknown TARGET_ARCH: ${TARGET_ARCH}")
endif()

# Musl toolchain paths
if(DEFINED ENV{MUSL_CROSS_MAKE})
    set(MUSL_ROOT "$ENV{MUSL_CROSS_MAKE}")
    set(CROSS_COMPILE "${MUSL_ROOT}/output/bin/${TARGET_TRIPLE}-")
elseif(DEFINED ENV{CROSS_COMPILE})
    set(CROSS_COMPILE "$ENV{CROSS_COMPILE}")
else()
    # Assume musl-gcc is in PATH (common on Alpine)
    set(CROSS_COMPILE "")
    set(CMAKE_C_COMPILER "musl-gcc")
    set(CMAKE_CXX_COMPILER "musl-g++")
endif()

# Set compilers if using cross-compile prefix
if(CROSS_COMPILE)
    set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
    set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")
    set(CMAKE_AR "${CROSS_COMPILE}ar")
    set(CMAKE_RANLIB "${CROSS_COMPILE}ranlib")
    set(CMAKE_STRIP "${CROSS_COMPILE}strip")
endif()

# Sysroot for cross-compilation
if(DEFINED ENV{MUSL_SYSROOT})
    set(CMAKE_SYSROOT "$ENV{MUSL_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
endif()

# Search path configurations
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Musl-specific: Enable static linking by default
# Default to building static libraries, but allow users to override
if(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
endif()

# Flags for musl compatibility - use add_link_options for proper propagation
add_link_options(-static)
