
cmake_minimum_required (VERSION 3.1.0 FATAL_ERROR)
project (DICOMautomaton)
set (DICOMautomaton_VERSION_MAJOR 3)
set (DICOMautomaton_VERSION_MINOR 0)
set (DICOMautomaton_VERSION_PATCH 0)

# Set the release type. 
set(CMAKE_BUILD_TYPE Release) # Comment this line to use fallback default.
if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always -fno-var-tracking-assignments")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSTREAM_H") # -DUSE_ICU_STRINGS
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -frounding-math -fno-finite-math-only")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(BUILD_SHARED_LIBS TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For clang-tidy et al.


# Use the directory where CMakeLists.txt is for inclusions.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src)
add_subdirectory(config-files)


set(CPACK_GENERATOR "DEB")
#set(CPACK_PACKAGE_NAME "dicomautomaton")

string(TIMESTAMP INVOCATION_TIMESTAMP "%Y%m%d.%H%M%S") # For a time-based version number.
set(CPACK_PACKAGE_VERSION "${INVOCATION_TIMESTAMP}")

# Dependencies, e.g., "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)"
set(CPACK_DEBIAN_PACKAGE_DEPENDS 
   "ygor,
    explicator,
    libboost-dev,
    libboost-filesystem-dev,
    libboost-iostreams-dev,
    libcgal-dev,
    zlibc,
    libsfml-dev,
    libjansson-dev,
    libpqxx-dev,
    postgresql-client,
    freeglut3,
    libcgal-dev,
    libnlopt-dev,
    libasio-dev,
    libwt-dev,
    libwthttp-dev
    fonts-freefont-ttf,
    fonts-cmu")

# Recommended or optional packages, e.g., "liboptional-dev (>= 1.2.3-1), libmaybe-dev (>= 1:1.3.2-10)"
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS 
   "zenity, 
    libeigen3-dev,
    gnuplot,
    ygorclustering")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64") # i386, amd64, armel, armhf, ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Various tools for medical physics applications.")
set(CPACK_PACKAGE_CONTACT "hdeanclark@gmail.com")
set(CPACK_PACKAGE_MAINTAINER "Haley Clark <hdeanclark@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "Science")

include(CPack)

