
cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(DICOMautomaton LANGUAGES CXX)
#set(DICOMautomaton_VERSION_MAJOR 0)
#set(DICOMautomaton_VERSION_MINOR 0)
#set(DICOMautomaton_VERSION_PATCH 0)

####################################################################################
#                                  User Options
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile slowly, with minimal memory usage."    OFF)

option(WITH_EIGEN     "Compile assuming Eigen is available."                    ON)
option(WITH_CGAL      "Compile assuming CGAL is available."                     ON)
option(WITH_NLOPT     "Compile assuming nlopt is available."                    ON)
option(WITH_SFML      "Compile assuming SFML is available."                     ON)
option(WITH_WT        "Compile assuming Wt is available."                       ON)
option(WITH_POSTGRES  "Compile assuming PostgreSQL libraries are available."    ON)
option(WITH_GNU_GSL   "Compile assuming the GNU GSL is available."              ON)


####################################################################################
#                                  Dependencies 
####################################################################################
# Note: Dependencies are listed in CPACK list below.

find_package(Threads REQUIRED)

if(WITH_EIGEN)
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
endif()

if(WITH_CGAL)
    find_package(CGAL REQUIRED COMPONENTS Core)
    include(${CGAL_USE_FILE})
endif()


####################################################################################
#                                  Configuration
####################################################################################

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

# Set the release type. 
if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()


####################################################################################
#                                    Definitions
####################################################################################
add_definitions(-DUSTREAM_H) # -DUSE_ICU_STRINGS

if(WITH_EIGEN)
    message(STATUS "Assuming Eigen is available.")
    add_definitions(-DDCMA_USE_EIGEN=1)
else()
    message(STATUS "Assuming Eigen is not available.")
    add_definitions(-UDCMA_USE_EIGEN)
endif()

if(WITH_CGAL)
    message(STATUS "Assuming CGAL is available.")
    add_definitions(-DDCMA_USE_CGAL=1)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_definitions(-DCGAL_DISABLE_ROUNDING_MATH_CHECK=1) # May be needed for Valgrind debugging.
        message(WARNING "If profiling/debugging on Arch Linux, disable binary stripping in PKGBUILD.")
    endif()
    if(WITH_EIGEN)
        add_definitions(-DCGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.
    endif()
else()
    message(STATUS "Assuming CGAL is not available.")
    add_definitions(-UDCMA_USE_CGAL)
                                        message(FATAL_ERROR "This option is not yet supported.")
endif()

if(WITH_NLOPT)
    message(STATUS "Assuming NLOPT is available.")
    add_definitions(-DDCMA_USE_NLOPT=1)
else()
    message(STATUS "Assuming NLOPT is not available.")
    add_definitions(-UDCMA_USE_NLOPT)
endif()

if(WITH_SFML)
    message(STATUS "Assuming SFML is available.")
    add_definitions(-DDCMA_USE_SFML=1)
else()
    message(STATUS "Assuming SFML is not available.")
    add_definitions(-UDCMA_USE_SFML)
endif()

if(WITH_WT)
    message(STATUS "Assuming Wt is available.")
    add_definitions(-DDCMA_USE_WT=1)
else()
    message(STATUS "Assuming Wt is not available.")
    add_definitions(-UDCMA_USE_WT)
endif()

if(WITH_POSTGRES)
    message(STATUS "Assuming PostgreSQL client libraries are available.")
    add_definitions(-DDCMA_USE_POSTGRES=1)
else()
    message(STATUS "Assuming PostgreSQL client libraries are not available.")
    add_definitions(-UDCMA_USE_POSTGRES)
endif()

if(WITH_GNU_GSL)
    message(STATUS "Assuming the GNU GSL is available.")
    add_definitions(-DDCMA_USE_GNU_GSL=1)
else()
    message(STATUS "Assuming the GNU GSL is not available.")
    add_definitions(-UDCMA_USE_GNU_GSL)
endif()


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
set(BASH_COMPLETION_INSTALL_DIR "/usr/share/bash-completion/completions/") # Should match the system directory.


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)
add_subdirectory(config-files)
add_subdirectory(scripts)


####################################################################################
#                                    Packaging    
####################################################################################

set(CPACK_GENERATOR "DEB")
#set(CPACK_PACKAGE_NAME "dicomautomaton")

string(TIMESTAMP INVOCATION_TIMESTAMP "%Y%m%d.%H%M%S") # For a time-based version number.
set(CPACK_PACKAGE_VERSION "${INVOCATION_TIMESTAMP}")
set(CPACK_STRIP_FILES TRUE)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64") # i386, amd64, armel, armhf, ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Various tools for medical physics applications.")
set(CPACK_PACKAGE_CONTACT "hdeanclark@gmail.com")
set(CPACK_PACKAGE_MAINTAINER "Haley Clark <hdeanclark@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "Science")


# Ygor build dependencies, e.g., "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)"
set(BUILD_DEPENDENCY_PACKAGES "")
list(APPEND BUILD_DEPENDENCY_PACKAGES "ygor")
list(APPEND BUILD_DEPENDENCY_PACKAGES "explicator")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libz-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libjansson-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-filesystem-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-iostreams-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-program-options-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-thread-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libasio-dev")
if(WITH_EIGEN)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libeigen3-dev")
endif()
if(WITH_SFML)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libsfml-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "fonts-freefont-ttf")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "fonts-cmu")
endif()
if(WITH_CGAL)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libcgal-dev")
endif()
if(WITH_WT)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libwt-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libwthttp-dev")
endif()
if(WITH_NLOPT)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libnlopt-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libnlopt-cxx-dev")
endif()
if(WITH_GNU_GSL)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libgsl-dev")
endif()
if(WITH_POSTGRES)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libpqxx-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "postgresql-dev")
endif()
list(JOIN BUILD_DEPENDENCY_PACKAGES ", " CPACK_DEBIAN_PACKAGE_DEPENDS)

# Recommended or optional packages, e.g., "liboptional-dev (>= 1.2.3-1), libmaybe-dev (>= 1:1.3.2-10)"
set(RECOMMENDED_PACKAGES "")
list(APPEND RECOMMENDED_PACKAGES "gnuplot")
list(APPEND RECOMMENDED_PACKAGES "zenity")
list(APPEND RECOMMENDED_PACKAGES "ygorclustering")
list(APPEND RECOMMENDED_PACKAGES "libboost-all-dev")
list(APPEND RECOMMENDED_PACKAGES "patchelf")
list(JOIN RECOMMENDED_PACKAGES ", " CPACK_DEBIAN_PACKAGE_RECOMMENDS)

include(CPack)


