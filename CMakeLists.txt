
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(DICOMautomaton LANGUAGES CXX)
#set(DICOMautomaton_VERSION_MAJOR 0)
#set(DICOMautomaton_VERSION_MINOR 0)
#set(DICOMautomaton_VERSION_PATCH 0)


####################################################################################
#                                  Dependencies 
####################################################################################
# Note: See CPACK list below too.

find_package(Threads REQUIRED)

#find_package(Eigen3 3.1.0)
#include(${EIGEN3_USE_FILE})
#
# or via pkg-config method:
find_package(PkgConfig REQUIRED)
pkg_check_modules(EIGEN3 REQUIRED eigen3)
include_directories( ${EIGEN3_INCLUDE_DIRS} )
#
# Fallback if the above methods fail:
#include_directories("/usr/include/eigen3/")

find_package(CGAL REQUIRED COMPONENTS Core)
include(${CGAL_USE_FILE})


####################################################################################
#                                  Configuration
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile with minimal memory usage." OFF)


# Provide a default build type if none were specified.
#
# Note: This is not necessary -- a 
#if(NOT CMAKE_BUILD_TYPE)
#    set(CMAKE_BUILD_TYPE Release CACHE STRING "default to release" FORCE)
#endif()


# Override the default CXX flags, which are controlled by the release type.
#
# Note: The '_DEBUG' flags are only applied when the release mode is 'Debug' -- likewise for the other flags.
#
# Note: If you want to fully override the CXX_FLAGS, then do not supply a build type and specify your CXX_FLAGS by
#       defining CMAKE_CXX_FLAGS when calling cmake.
set(CMAKE_CXX_FLAGS_DEBUG           "-O2 -g")
set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE         "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O3 -g")

if(MEMORY_CONSTRAINED_BUILD)
    # Do not overwrite user-provided flags, but do provide sane defaults.
    if(NOT CMAKE_CXX_FLAGS)
        set(CMAKE_CXX_FLAGS "-O0 -DNDEBUG")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG           "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL      "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE         "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O0 -DNDEBUG")
endif()


# Add other appropriate CXX flags.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-var-tracking-assignments")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        # Add gprof profiling flag.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    endif()

    if(MEMORY_CONSTRAINED_BUILD)
        # Trigger garbage collection more frequently.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --param ggc-min-expand=10")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --param ggc-min-heapsize=32768")
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-var-tracking-assignments")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
endif()

    
# Add definitions.
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-DCGAL_DISABLE_ROUNDING_MATH_CHECK=1) # May be needed for Valgrind debugging.
    message(WARNING "If profiling/debugging on Arch Linux, disable binary stripping in PKGBUILD.")
endif()
add_definitions(-DUSTREAM_H) # -DUSE_ICU_STRINGS
add_definitions(-DCGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.


# Add other compiler options.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(BUILD_SHARED_LIBS TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions (e.g., std=gnu++14).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For use with clang-tidy et al.


# Use the directory where CMakeLists.txt is for inclusions.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)
add_subdirectory(config-files)


####################################################################################
#                                    Packaging    
####################################################################################

set(CPACK_GENERATOR "DEB")
#set(CPACK_PACKAGE_NAME "dicomautomaton")

string(TIMESTAMP INVOCATION_TIMESTAMP "%Y%m%d.%H%M%S") # For a time-based version number.
set(CPACK_PACKAGE_VERSION "${INVOCATION_TIMESTAMP}")
set(CPACK_STRIP_FILES TRUE)

# Dependencies, e.g., "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)"
set(CPACK_DEBIAN_PACKAGE_DEPENDS 
   "ygor,
    explicator,
    libboost-dev,
    libboost-filesystem-dev,
    libboost-iostreams-dev,
    libboost-program-options-dev,
    libboost-thread-dev,
    libz-dev,
    libsfml-dev,
    libjansson-dev,
    libpqxx-dev,
    postgresql-client,
    libeigen3-dev,
    libcgal-dev,
    libnlopt-dev,
    libnlopt-cxx-dev,
    libasio-dev,
    libwt-dev,
    libwthttp-dev,
    fonts-freefont-ttf,
    fonts-cmu")

# Recommended or optional packages, e.g., "liboptional-dev (>= 1.2.3-1), libmaybe-dev (>= 1:1.3.2-10)"
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS 
   "zenity, 
    gnuplot,
    ygorclustering,
    libboost-all-dev,
    patchelf")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64") # i386, amd64, armel, armhf, ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Various tools for medical physics applications.")
set(CPACK_PACKAGE_CONTACT "hdeanclark@gmail.com")
set(CPACK_PACKAGE_MAINTAINER "Haley Clark <hdeanclark@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "Science")

include(CPack)

