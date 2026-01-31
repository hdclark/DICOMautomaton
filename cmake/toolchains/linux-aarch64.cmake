# ARM Cross-Compilation Toolchain File
#
# This toolchain file sets up cross-compilation for ARM targets (armv7/aarch64).
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake ..
#
# Override TARGET_TRIPLE for different ARM variants.

# System configuration
set(CMAKE_SYSTEM_NAME Linux)

# Target architecture - can be overridden
if(NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH "aarch64")
endif()

if(TARGET_ARCH STREQUAL "aarch64")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(TARGET_TRIPLE "aarch64-linux-gnu")
elseif(TARGET_ARCH STREQUAL "armv7")
    set(CMAKE_SYSTEM_PROCESSOR arm)
    set(TARGET_TRIPLE "arm-linux-gnueabihf")
else()
    message(FATAL_ERROR "Unknown TARGET_ARCH: ${TARGET_ARCH}")
endif()

# Cross compiler paths
if(DEFINED ENV{CROSS_COMPILE})
    set(CROSS_COMPILE "$ENV{CROSS_COMPILE}")
else()
    set(CROSS_COMPILE "/usr/bin/${TARGET_TRIPLE}-")
endif()

# Compilers
set(CMAKE_C_COMPILER "${CROSS_COMPILE}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE}g++")
set(CMAKE_AR "${CROSS_COMPILE}ar")
set(CMAKE_RANLIB "${CROSS_COMPILE}ranlib")
set(CMAKE_STRIP "${CROSS_COMPILE}strip")

# Sysroot (if using a custom sysroot)
if(DEFINED ENV{ARM_SYSROOT})
    set(CMAKE_SYSROOT "$ENV{ARM_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
endif()

# Search path configurations
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Architecture-specific flags
if(TARGET_ARCH STREQUAL "aarch64")
    set(CMAKE_C_FLAGS_INIT "-mcpu=generic")
    set(CMAKE_CXX_FLAGS_INIT "-mcpu=generic")
elseif(TARGET_ARCH STREQUAL "armv7")
    set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
    set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
endif()
