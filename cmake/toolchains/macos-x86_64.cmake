# macOS Cross-Compilation Toolchain File
#
# This toolchain file sets up cross-compilation for macOS targets.
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-x86_64.cmake ..
#
# This is intended for use with osxcross or similar toolchains.

# System configuration
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# OSXCross toolchain paths
if(DEFINED ENV{OSXCROSS_TARGET_DIR})
    set(OSXCROSS_TARGET_DIR "$ENV{OSXCROSS_TARGET_DIR}")
else()
    set(OSXCROSS_TARGET_DIR "/opt/osxcross")
endif()

set(OSXCROSS_SDK_DIR "${OSXCROSS_TARGET_DIR}/SDK")
set(OSXCROSS_BIN_DIR "${OSXCROSS_TARGET_DIR}/bin")

# Toolchain triple
set(TARGET_TRIPLE "x86_64-apple-darwin21")

# Compilers
set(CMAKE_C_COMPILER "${OSXCROSS_BIN_DIR}/o64-clang")
set(CMAKE_CXX_COMPILER "${OSXCROSS_BIN_DIR}/o64-clang++")

# Search paths
set(CMAKE_FIND_ROOT_PATH "${OSXCROSS_SDK_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# macOS deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum macOS version")

# Additional flags for macOS
set(CMAKE_C_FLAGS_INIT "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
set(CMAKE_CXX_FLAGS_INIT "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")

# macOS-specific settings
set(APPLE TRUE)
