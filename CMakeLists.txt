
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
option(WITH_SFML      "Compile assuming SFML is available."                     OFF)
option(WITH_SDL       "Compile assuming SDL2 and glew are available."           ON)
option(WITH_WT        "Compile assuming Wt is available."                       ON)
option(WITH_GNU_GSL   "Compile assuming the GNU GSL is available."              ON)
option(WITH_POSTGRES  "Compile assuming PostgreSQL libraries are available."    ON)
option(WITH_JANSSON   "Compile assuming Jansson is available."                  ON)
option(WITH_THRIFT    "Compile assuming Apache Thrift is available."            ON)
option(WITH_EXT_SYCL  "Compile assuming external SYCL support is available."    ON)

option(WITH_PYTHON           "Enable DICOMautomaton Python integration."         OFF)
option(WITH_PYTHON_EXTENSION "Build the CPython extension module."               OFF)
option(WITH_PYTHON_EMBED     "Enable embedded CPython support."                  OFF)

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
set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions (e.g., std=gnu++14). Enforced via target_compile_features.


set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For use with clang-tidy et al.
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

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

if(WITH_PYTHON)
    include(CTest)
endif()

if((WITH_PYTHON_EXTENSION OR WITH_PYTHON_EMBED) AND NOT WITH_PYTHON)
    message(FATAL_ERROR "WITH_PYTHON must be enabled when a Python subfeature is requested.")
endif()
if(WITH_PYTHON)
    if(CMAKE_VERSION VERSION_LESS 3.18)
        message(FATAL_ERROR
            "Python integration requires CMake 3.18 or newer for "
            "component-specific development artifact discovery. Python-disabled "
            "builds continue to support CMake 3.12.")
    endif()
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "Python integration is not configured for cross compilation. Disable "
            "WITH_PYTHON or provide a deliberate target-Python integration first.")
    endif()
    if(WITH_PYTHON_EMBED AND NOT BUILD_SHARED_LIBS)
        message(FATAL_ERROR
            "WITH_PYTHON_EMBED is not supported by the fully static executable "
            "configuration. Enable BUILD_SHARED_LIBS or disable embedding.")
    endif()
    if(NOT WITH_PYTHON_EXTENSION AND NOT WITH_PYTHON_EMBED)
        message(FATAL_ERROR
            "WITH_PYTHON is enabled, but neither WITH_PYTHON_EXTENSION nor "
            "WITH_PYTHON_EMBED is enabled.")
    endif()

    set(_dcma_python_components Interpreter)
    if(WITH_PYTHON_EXTENSION)
        list(APPEND _dcma_python_components Development.Module)
    endif()
    if(WITH_PYTHON_EMBED)
        list(APPEND _dcma_python_components Development.Embed)
    endif()
    find_package(Python3 REQUIRED COMPONENTS ${_dcma_python_components})
    if(Python3_VERSION VERSION_LESS 3.8 OR NOT Python3_VERSION VERSION_LESS 3.12)
        message(FATAL_ERROR
            "Python ${Python3_VERSION} is not in the supported CPython 3.8-3.11 window.")
    endif()

    # Older pybind11 packages use FindPythonInterp internally. Keep that
    # discovery aligned with the interpreter already selected by FindPython3.
    set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
    find_package(pybind11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        if(WITH_FETCHCONTENT_FALLBACK)
            message(STATUS "pybind11 not found via find_package; fetching via FetchContent.")
            FetchContent_Declare(
                pybind11
                GIT_REPOSITORY https://github.com/pybind/pybind11.git
                # Immutable commit for pybind11 v2.10.4.
                GIT_TAG        a2e59f0e7065404b44dfe92a28aca47ba1378dc4
            )
            FetchContent_MakeAvailable(pybind11)
            set(_dcma_pybind11_version 2.10.4)
        else()
            message(FATAL_ERROR
                "pybind11 not found. Install the pybind11 development package or "
                "set WITH_FETCHCONTENT_FALLBACK=ON.")
        endif()
    else()
        set(_dcma_pybind11_version "${pybind11_VERSION}")
    endif()
    if(_dcma_pybind11_version VERSION_LESS 2.6)
        message(FATAL_ERROR "Python integration requires pybind11 2.6 or newer.")
    endif()
    if((NOT Python3_VERSION VERSION_LESS 3.10) AND _dcma_pybind11_version VERSION_LESS 2.7)
        message(FATAL_ERROR "CPython ${Python3_VERSION} requires pybind11 2.7 or newer.")
    endif()
    if((NOT Python3_VERSION VERSION_LESS 3.11) AND _dcma_pybind11_version VERSION_LESS 2.10)
        message(FATAL_ERROR "CPython ${Python3_VERSION} requires pybind11 2.10 or newer.")
    endif()

    if(WITH_PYTHON_EXTENSION AND BUILD_TESTING)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c "import numpy; print(numpy.__version__)"
            RESULT_VARIABLE _dcma_numpy_result
            OUTPUT_VARIABLE _dcma_numpy_version
            ERROR_VARIABLE _dcma_numpy_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT _dcma_numpy_result EQUAL 0)
            message(FATAL_ERROR
                "NumPy is required by the Python extension data adapters and tests. "
                "Install the NumPy package matching ${Python3_EXECUTABLE}. "
                "Import failed with: ${_dcma_numpy_error}")
        endif()
    endif()

    if(WITH_PYTHON_EXTENSION)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c
                    "import sysconfig; print(sysconfig.get_path('platlib', vars={'base': '', 'platbase': ''}).lstrip('/'))"
            RESULT_VARIABLE _dcma_python_sitearch_result
            OUTPUT_VARIABLE DCMA_PYTHON_SITEARCH_RELATIVE
            ERROR_VARIABLE _dcma_python_sitearch_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if((NOT _dcma_python_sitearch_result EQUAL 0) OR (NOT DCMA_PYTHON_SITEARCH_RELATIVE))
            message(FATAL_ERROR
                "Unable to determine the selected interpreter's prefix-relative "
                "module installation path: ${_dcma_python_sitearch_error}")
        endif()
    endif()

    message(STATUS "Python integration:")
    message(STATUS "  executable: ${Python3_EXECUTABLE}")
    message(STATUS "  version: ${Python3_VERSION}")
    message(STATUS "  include directories: ${Python3_INCLUDE_DIRS}")
    message(STATUS "  pybind11 version: ${_dcma_pybind11_version}")
    if(TARGET Python3::Module)
        set(_dcma_python_module_target "available")
    else()
        set(_dcma_python_module_target "unavailable")
    endif()
    if(TARGET Python3::Python)
        set(_dcma_python_embed_target "available")
    else()
        set(_dcma_python_embed_target "unavailable")
    endif()
    message(STATUS "  extension enabled: ${WITH_PYTHON_EXTENSION} (Python3::Module ${_dcma_python_module_target})")
    if(WITH_PYTHON_EXTENSION AND DEFINED _dcma_numpy_version)
        message(STATUS "  NumPy version: ${_dcma_numpy_version}")
    endif()
    if(WITH_PYTHON_EXTENSION)
        message(STATUS "  prefix-relative module path: ${DCMA_PYTHON_SITEARCH_RELATIVE}")
    endif()
    message(STATUS "  embedding enabled: ${WITH_PYTHON_EMBED} (Python3::Python ${_dcma_python_embed_target})")
endif()

####################################################################################
#                                  Dependencies 
####################################################################################

# Author's upstream support libraries: Ygor, YgorClustering, Explicator.
# Uses find_package() with optional FetchContent fallback (cmake 3.12+).
include(cmake/UpstreamAuthorDeps.cmake)

find_package(Threads REQUIRED)

# Enable verbose Boost find_package output only for Debug builds.
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(Boost_DEBUG ON)
endif()
if(NOT BUILD_SHARED_LIBS)
    #set(Boost_USE_STATIC_RUNTIME ON)
    set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost COMPONENTS iostreams thread
                   OPTIONAL_COMPONENTS system)
if(NOT Boost_FOUND)
    message(STATUS "find_package(Boost CONFIG) not found, attempting non-config method instead")
    find_package(Boost REQUIRED COMPONENTS iostreams thread
                                 OPTIONAL_COMPONENTS system)
endif()

find_library(MPFR_LIBRARY mpfr)
find_library(GMP_LIBRARY gmp)

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
# cmake/PackageLists.cmake. Legacy packaging uses the build-dependency list,
# but Python header-only build dependencies are not runtime requirements.
#
# Note: none of the SYCL external dependencies seem to be packaged for Debian (yet).
set(_dcma_debian_package_deps ${DCMA_DEBIAN_BUILD_DEPS})
list(REMOVE_ITEM _dcma_debian_package_deps python3-dev pybind11-dev)
list(JOIN _dcma_debian_package_deps ", " CPACK_DEBIAN_PACKAGE_DEPENDS)
list(JOIN DCMA_DEBIAN_RECOMMENDED_DEPS ", " CPACK_DEBIAN_PACKAGE_RECOMMENDS)

include(CPack)
