# MXE Cross-Compilation Toolchain File for Windows targets
#
# This toolchain file sets up cross-compilation for Windows using MXE.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mxe-x86_64-w64-mingw32.cmake ..
#
# This file should be used when MXE is installed at /mxe.
# The TOOLCHAIN environment variable should be set (e.g., x86_64-w64-mingw32.static)

# System configuration
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Get the toolchain prefix from environment or use default
if(DEFINED ENV{TOOLCHAIN})
    set(TOOLCHAIN_PREFIX "$ENV{TOOLCHAIN}")
else()
    set(TOOLCHAIN_PREFIX "x86_64-w64-mingw32.static")
endif()

# MXE root directory
if(DEFINED ENV{MXE_ROOT})
    set(MXE_ROOT "$ENV{MXE_ROOT}")
else()
    set(MXE_ROOT "/mxe")
endif()

set(MXE_TARGET_ROOT "${MXE_ROOT}/usr/${TOOLCHAIN_PREFIX}")

# Compilers
set(CMAKE_C_COMPILER "${MXE_ROOT}/usr/bin/${TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${MXE_ROOT}/usr/bin/${TOOLCHAIN_PREFIX}-g++")
set(CMAKE_RC_COMPILER "${MXE_ROOT}/usr/bin/${TOOLCHAIN_PREFIX}-windres")

# Search paths
set(CMAKE_FIND_ROOT_PATH "${MXE_TARGET_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Package configuration
set(CMAKE_PREFIX_PATH "${MXE_TARGET_ROOT}")
set(PKG_CONFIG_EXECUTABLE "${MXE_ROOT}/usr/bin/${TOOLCHAIN_PREFIX}-pkg-config")

# Static linking for MXE static builds
if(TOOLCHAIN_PREFIX MATCHES "static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    # Prefer static linking for executables built with this static MXE toolchain;
    # use add_link_options so that the flag propagates correctly to targets.
    add_link_options(-static)

    # Default to building static libraries, but allow users to override
    # BUILD_SHARED_LIBS from the command line or presets.
    if(NOT DEFINED BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
    endif()
endif()

# MinGW-specific settings
set(MINGW TRUE)
set(WIN32 TRUE)
