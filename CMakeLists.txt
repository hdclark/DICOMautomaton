
cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)
project(DICOMautomaton VERSION 0.0.1 LANGUAGES CXX)

# Add cmake/ to module search path so includes like include(PackageLists) work.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Centralized package-name lists for various package managers.
include(cmake/PackageLists.cmake)

####################################################################################
#                                  User Options
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile slowly, with minimal memory usage."    OFF)
option(WITH_IWYU                "Compile using clang include-what-you-use."     OFF)

option(WITH_ASAN                "Compile using ASan, LSan, & UBSan."            OFF)
option(WITH_TSAN                "Compile using ThreadSanitizer."                OFF)
option(WITH_MSAN                "Compile using MemorySanitizer."                OFF)

option(WITH_EIGEN     "Compile assuming Eigen is available."                    ON)
option(WITH_CGAL      "Compile assuming CGAL is available."                     ON)
option(WITH_NLOPT     "Compile assuming nlopt is available."                    ON)
option(WITH_SFML      "Compile assuming SFML is available."                     ON)
option(WITH_SDL       "Compile assuming SDL2 and glew are available."           ON)
option(WITH_WT        "Compile assuming Wt is available."                       ON)
option(WITH_GNU_GSL   "Compile assuming the GNU GSL is available."              ON)
option(WITH_POSTGRES  "Compile assuming PostgreSQL libraries are available."    ON)
option(WITH_JANSSON   "Compile assuming Jansson is available."                  ON)
option(WITH_THRIFT    "Compile assuming Apache Thrift is available."            ON)
option(WITH_EXT_SYCL  "Compile assuming external SYCL support is available."    ON)

option(WITH_LTO       "Use link-time optimization when available."              OFF)

option(BUILD_SHARED_LIBS "Build shared-object/dynamicly-loaded binaries."       ON)

# When ON, attempt to download missing dependencies via CMake FetchContent
# before raising a fatal error. Useful for environments without system packages
# but NOT recommended when integrating with OS/third-party package managers.
option(WITH_FETCHCONTENT_FALLBACK "Download missing dependencies if not found." OFF)


####################################################################################
#                                  Configuration
####################################################################################

# Set the C++ standard (DICOMautomaton requires C++17).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# High-level configuration.
if(NOT BUILD_SHARED_LIBS)
    #set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    link_libraries("-static")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions (e.g., std=gnu++14). Enforced via target_compile_features.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For use with clang-tidy et al.
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)

if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()

if(WITH_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_avail OUTPUT lto_msg)
    if(lto_avail)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO was requested, but is not supported: ${lto_msg}")
    endif()
endif()

# Bring in standard install directory variables (CMAKE_INSTALL_LIBDIR etc.)
# early so they can be referenced in FetchContent and dependency detection
# blocks. This is safe to call multiple times.
include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(BASH_COMPLETION_INSTALL_DIR    "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions/")
# Use the directory where CMakeLists.txt is for inclusions.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

# FetchContent is available in CMake 3.11+. We require 3.12+ so it is always available.
include(FetchContent)


####################################################################################
#                                  Dependencies 
####################################################################################

find_package(Threads REQUIRED)

# Enable verbose Boost find_package output only for Debug builds.
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(Boost_DEBUG ON)
endif()
if(NOT BUILD_SHARED_LIBS)
    #set(Boost_USE_STATIC_RUNTIME ON)
    set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost COMPONENTS serialization iostreams thread
                   OPTIONAL_COMPONENTS system)
if(NOT Boost_FOUND)
    message(STATUS "find_package(Boost CONFIG) not found, attempting non-config method instead")
    find_package(Boost REQUIRED COMPONENTS serialization iostreams thread
                                OPTIONAL_COMPONENTS system)
endif()

if(WITH_EIGEN)
    find_package(Eigen3 CONFIG QUIET)
    if(NOT Eigen3_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(EIGEN3 eigen3)
    endif()
    if(NOT Eigen3_FOUND AND NOT EIGEN3_FOUND)
        if(WITH_FETCHCONTENT_FALLBACK)
            message(STATUS "Eigen3 not found via find_package/pkg-config; fetching via FetchContent.")
            FetchContent_Declare(
                Eigen3
                GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
                # Pin to the immutable commit SHA for Eigen 3.4.0 to prevent supply-chain
                # attacks via tag mutation. Verify: git ls-remote --tags on the upstream repo.
                GIT_TAG        3147391d946bb4b6c68edd901f2add6ac1f31f8c
            )
            FetchContent_MakeAvailable(Eigen3)
            set(Eigen3_FOUND TRUE)
        else()
            message(FATAL_ERROR "Eigen3 not found. Install libeigen3-dev or set WITH_FETCHCONTENT_FALLBACK=ON.")
        endif()
    endif()
endif()

if(WITH_CGAL)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)
        message(WARNING "If profiling/debugging on Arch Linux, disable binary stripping in PKGBUILD.")
    endif()
    find_package(CGAL REQUIRED COMPONENTS Core)

    # This workaround is for a CGAL v4.13 class and file rename {Implicit,Labeled}_mesh_domain_3.h.
    include(CheckIncludeFileCXX)
    # Alternatively, prefer to use the older Implicit_mesh_domain_3.h when available.
    check_include_file_cxx("CGAL/Implicit_mesh_domain_3.h" DCMA_CGAL_HAS_IMPLICIT_MESH_DOMAIN_3_HEADER)
endif()

if(WITH_NLOPT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NLOPT nlopt)
    if(NOT NLOPT_FOUND)
        if(WITH_FETCHCONTENT_FALLBACK)
            message(STATUS "nlopt not found via pkg-config; fetching via FetchContent.")
            FetchContent_Declare(
                nlopt
                GIT_REPOSITORY https://github.com/stevengj/nlopt.git
                # Pin to the immutable commit SHA for nlopt v2.7.1 to prevent supply-chain
                # attacks via tag mutation. Verify: git ls-remote --tags on the upstream repo.
                GIT_TAG        9c7018f617dcf74a6e42f67e0a14869f10e5c957
            )
            FetchContent_MakeAvailable(nlopt)
            set(NLOPT_FOUND TRUE)
        else()
            message(FATAL_ERROR "nlopt not found. Install libnlopt-dev or set WITH_FETCHCONTENT_FALLBACK=ON.")
        endif()
    endif()
endif()

if(WITH_SFML)
    find_package(PkgConfig REQUIRED)
    if(NOT BUILD_SHARED_LIBS)
        set(SFML_STATIC_LIBRARIES TRUE)
    endif()
    pkg_check_modules(SFML REQUIRED sfml-graphics sfml-window sfml-system)
endif()

if(WITH_SDL)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED COMPONENTS OpenGL) # Will help pick-up opengl32.a with MXE toolchain.
    message(STATUS "Proceeding with OPENGL_LIBRARIES = '${OPENGL_LIBRARIES}'")
    message(STATUS "Proceeding with OPENGL_INCLUDE_DIR = '${OPENGL_INCLUDE_DIR}'")
    message(STATUS "Proceeding with OPENGL_CFLAGS = '${OPENGL_CFLAGS}'")

    find_package(PkgConfig REQUIRED)
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        message(STATUS "find_package(SDL2) not found, attempting pkg-config instead")
        pkg_check_modules(SDL2 REQUIRED sdl2)
    endif()
    message(STATUS "Proceeding with SDL2_LIBRARIES = '${SDL2_LIBRARIES}'")
    message(STATUS "Proceeding with SDL2_INCLUDE_DIRS = '${SDL2_INCLUDE_DIRS}'")
    message(STATUS "Proceeding with SDL2_CFLAGS = '${SDL2_CFLAGS}'")
    if(NOT TARGET SDL2::SDL2)
        message(STATUS "Simulating missing 'SDL2::SDL2' target")
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        target_link_libraries(SDL2::SDL2 INTERFACE "${SDL2_LIBRARIES}")
    endif()

    pkg_check_modules(GLEW REQUIRED glew)
endif()

if(WITH_GNU_GSL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GNU_GSL REQUIRED gsl)
endif()

if(WITH_POSTGRES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POSTGRES REQUIRED libpq libpqxx)
endif()

if(WITH_THRIFT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(THRIFT REQUIRED thrift)
endif()

set(DCMA_SYCL_FALLBACK "Fallback")
set(DCMA_SYCL_BACKEND "${DCMA_SYCL_FALLBACK}")
if(WITH_EXT_SYCL)
    # AdaptiveCPP
    # For best results use the provided toolchain file:
    #   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/sycl-adaptivecpp.cmake ...
    if("${DCMA_SYCL_BACKEND}" STREQUAL "${DCMA_SYCL_FALLBACK}")
        # SYCL support via AdaptiveCpp (formerly hipSYCL).
        # AdaptiveCpp provides the acpp compiler wrapper and CMake integration.
        # See https://github.com/AdaptiveCpp/AdaptiveCpp for installation instructions.
        
        # Set default ACPP_TARGETS if not already specified by user.
        if(NOT DEFINED ACPP_TARGETS)
            set(ACPP_TARGETS "generic" CACHE STRING "AdaptiveCpp backend target")
            message(STATUS "ACPP_TARGETS not specified; defaulting to 'generic' for maximum portability.")
        endif()

        find_package(AdaptiveCpp CONFIG)
        if(AdaptiveCpp_FOUND)
            get_filename_component(DCMA_CXX_COMPILER_BASENAME "${CMAKE_CXX_COMPILER}" NAME)
            message(STATUS "CXX compiler: ${DCMA_CXX_COMPILER_BASENAME} (Note: can be set via environment flag 'CXX')")
            set(DCMA_SYCL_BACKEND "AdaptiveCPP")
        endif()
    endif()

    # TriSYCL (option 1 of 2)
    if("${DCMA_SYCL_BACKEND}" STREQUAL "${DCMA_SYCL_FALLBACK}")
        find_package(trisycl CONFIG)
        if(trisycl_FOUND)
            set(DCMA_SYCL_BACKEND "TriSYCL")
        endif()
    endif()

    # TriSYCL (option 2 of 2)
    if("${DCMA_SYCL_BACKEND}" STREQUAL "${DCMA_SYCL_FALLBACK}")
        find_package(TriSYCL CONFIG)
        if(TriSYCL_FOUND)
            set(DCMA_SYCL_BACKEND "TriSYCL")
        endif()
    endif()
endif()

#if(NOT "${DCMA_SYCL_BACKEND}" STREQUAL "${DCMA_SYCL_FALLBACK}")
#    # Assume that OpenMP is needed if any external SYCL components are being used.
#    find_package(OpenMP REQUIRED)
#endif()

####################################################################################
#                         DCMA_VERSION detection
####################################################################################
if( (NOT DEFINED DCMA_VERSION) OR ("${DCMA_VERSION}" MATCHES "^$") )
    execute_process( COMMAND
        ./scripts/extract_dcma_version.sh
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE DCMA_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if( "${DCMA_VERSION}" MATCHES "^$" )
    message(WARNING "Please supply a meaningful DCMA_VERSION when invoking CMake.")
    set(DCMA_VERSION "unknown")
endif()

####################################################################################
#                    Platform Feature Detection (for DCMA_Definitions.h)
####################################################################################
# Detect whether specific functions/variables/macros are available.
# Note: this method does not support structs. Best to find a function that accepts the struct instead.
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(select    "sys/select.h" DCMA_HAS_SYS_SELECT)        # Function (nominally).

check_cxx_symbol_exists(isatty    "unistd.h"     DCMA_HAS_UNISTD_ISATTY)     # Function (nominally).

check_cxx_symbol_exists(fileno    "cstdio"       DCMA_HAS_CSTDIO_FILENO)     # Function (nominally).

check_cxx_symbol_exists(ICANON    "termios.h"    DCMA_HAS_TERMIOS_ICANON)    # Macro.
check_cxx_symbol_exists(ECHO      "termios.h"    DCMA_HAS_TERMIOS_ECHO)      # Macro.
check_cxx_symbol_exists(VMIN      "termios.h"    DCMA_HAS_TERMIOS_VMIN)      # Macro.
check_cxx_symbol_exists(VTIME     "termios.h"    DCMA_HAS_TERMIOS_VTIME)     # Macro.
check_cxx_symbol_exists(TCSANOW   "termios.h"    DCMA_HAS_TERMIOS_TCSANOW)   # Macro.
check_cxx_symbol_exists(tcgetattr "termios.h"    DCMA_HAS_TERMIOS_TCGETADDR) # Function.

check_cxx_symbol_exists(fcntl      "fcntl.h"     DCMA_HAS_FCNTL_FCNTL)       # Function (nominally).
check_cxx_symbol_exists(F_GETFL    "fcntl.h"     DCMA_HAS_FCNTL_F_GETFL)     # Macro.
check_cxx_symbol_exists(O_NONBLOCK "fcntl.h"     DCMA_HAS_FCNTL_O_NONBLOCK)  # Macro.

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/src/DCMA_Definitions.h.in
                ${CMAKE_CURRENT_BINARY_DIR}/src/DCMA_Definitions.h )

####################################################################################
#                     Global Options Target (dcma_compile_settings)
####################################################################################
# The 'dcma_compile_settings' INTERFACE library is the project-wide "Global Options
# Target". Every DICOMautomaton target links against it to inherit compiler flags,
# compile definitions, and include directories.
#
# The definition is in cmake/CompileSettings.cmake for clean separation of concerns.
#
# Downstream CMake consumer usage after install:
#   find_package(DICOMautomaton REQUIRED)
#   target_link_libraries(my_target PRIVATE DICOMautomaton::dcma_compile_settings)

include(cmake/CompileSettings.cmake)

# Apply dcma_compile_settings to all targets defined in subdirectories.
link_libraries(dcma_compile_settings)


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)
add_subdirectory(config-files)
add_subdirectory(scripts)


####################################################################################
#                    CMake Package Config Support
####################################################################################
# Install CMake package config files so downstream projects can use:
#   find_package(DICOMautomaton REQUIRED)
#   target_link_libraries(my_target PRIVATE DICOMautomaton::imebrashim)
#
# This makes DICOMautomaton compatible with CMake-based consumers, as well as
# third-party C++ package managers (Conan, vcpkg) that rely on CMake config files.

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DICOMautomatonConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DICOMautomatonConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DICOMautomaton"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DICOMautomatonConfigVersion.cmake"
    VERSION "${CMAKE_PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DICOMautomatonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DICOMautomatonConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DICOMautomaton"
)

# Export dcma_compile_settings INTERFACE library so downstream consumers can inherit
# compile settings simply by linking against it.
install(TARGETS dcma_compile_settings
    EXPORT DICOMautomatonTargets
)

# The installed DICOMautomatonTargets.cmake is generated by the src/CMakeLists.txt
# install(TARGETS ... EXPORT DICOMautomatonTargets ...) calls.
install(EXPORT DICOMautomatonTargets
    NAMESPACE   DICOMautomaton::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DICOMautomaton"
)


####################################################################################
#                                    Packaging    
####################################################################################

set(CPACK_GENERATOR "DEB")
#set(CPACK_PACKAGE_NAME "dicomautomaton")

string(TIMESTAMP INVOCATION_TIMESTAMP "%Y%m%d.%H%M%S") # For a time-based version number.
set(CPACK_PACKAGE_VERSION "${INVOCATION_TIMESTAMP}")
set(CPACK_STRIP_FILES TRUE)

#set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64") # i386, amd64, armel, armhf, ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Various tools for medical physics applications.")
set(CPACK_PACKAGE_CONTACT "hdeanclark@gmail.com")
set(CPACK_PACKAGE_MAINTAINER "Haley Clark <hdeanclark@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "Science")


# Build and runtime dependency package lists are defined centrally in
# cmake/PackageLists.cmake. The DCMA_DEBIAN_BUILD_DEPS variable
# is populated there and used here directly.
#
# Note: none of the SYCL external dependencies seem to be packaged for Debian (yet).
list(JOIN DCMA_DEBIAN_BUILD_DEPS ", " CPACK_DEBIAN_PACKAGE_DEPENDS)
list(JOIN DCMA_DEBIAN_RECOMMENDED_DEPS ", " CPACK_DEBIAN_PACKAGE_RECOMMENDS)

include(CPack)
