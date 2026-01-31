# DCMADependencies.cmake
# Modern CMake module for dependency management with FetchContent fallbacks
#
# This module provides functions to find dependencies with graceful fallback
# to FetchContent when system packages are not available.

include_guard(GLOBAL)

include(FetchContent)

# Configure FetchContent defaults
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

#------------------------------------------------------------------------------
# Interface library for common dependencies
#------------------------------------------------------------------------------

# Create an interface library to collect common dependency includes/definitions
function(dcma_create_common_interface)
    if(NOT TARGET DCMA::CommonInterface)
        add_library(dcma_common_interface INTERFACE)
        add_library(DCMA::CommonInterface ALIAS dcma_common_interface)
    endif()
endfunction()

#------------------------------------------------------------------------------
# Threads
#------------------------------------------------------------------------------
function(dcma_find_threads)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    message(STATUS "Found Threads library")
endfunction()

#------------------------------------------------------------------------------
# Boost
#------------------------------------------------------------------------------
function(dcma_find_boost)
    set(Boost_DEBUG ON)
    if(NOT BUILD_SHARED_LIBS)
        set(Boost_USE_STATIC_LIBS ON)
    endif()

    find_package(Boost COMPONENTS serialization iostreams thread
                       OPTIONAL_COMPONENTS system)
    
    if(NOT Boost_FOUND)
        message(STATUS "find_package(Boost CONFIG) not found, attempting non-config method")
        find_package(Boost REQUIRED COMPONENTS serialization iostreams thread
                                    OPTIONAL_COMPONENTS system)
    endif()

    if(Boost_FOUND)
        message(STATUS "Found Boost version ${Boost_VERSION}")
    else()
        message(FATAL_ERROR "Boost not found and is required")
    endif()
endfunction()

#------------------------------------------------------------------------------
# Eigen
#------------------------------------------------------------------------------
function(dcma_find_eigen)
    if(NOT DCMA_WITH_EIGEN)
        message(STATUS "Eigen support disabled")
        return()
    endif()

    # Try find_package first (modern Eigen has CMake config)
    find_package(Eigen3 3.3 QUIET CONFIG)
    
    if(NOT Eigen3_FOUND)
        # Fall back to pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(EIGEN3 QUIET eigen3)
        endif()
    endif()

    if(Eigen3_FOUND OR EIGEN3_FOUND)
        message(STATUS "Found Eigen3")
        set(DCMA_EIGEN_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target if pkg-config was used
        if(NOT TARGET Eigen3::Eigen AND EIGEN3_INCLUDE_DIRS)
            add_library(Eigen3::Eigen INTERFACE IMPORTED)
            target_include_directories(Eigen3::Eigen INTERFACE ${EIGEN3_INCLUDE_DIRS})
        endif()
    else()
        # Try FetchContent as fallback
        message(STATUS "Eigen3 not found, attempting FetchContent fallback")
        dcma_fetch_eigen()
    endif()
endfunction()

function(dcma_fetch_eigen)
    FetchContent_Declare(
        Eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3.4.0
        GIT_SHALLOW TRUE
    )
    
    # Eigen has its own CMake setup, we just need headers
    FetchContent_GetProperties(Eigen3)
    if(NOT eigen3_POPULATED)
        FetchContent_Populate(Eigen3)
        add_library(Eigen3::Eigen INTERFACE IMPORTED)
        target_include_directories(Eigen3::Eigen INTERFACE ${eigen3_SOURCE_DIR})
        set(DCMA_EIGEN_AVAILABLE TRUE PARENT_SCOPE)
        message(STATUS "Eigen3 fetched successfully")
    endif()
endfunction()

#------------------------------------------------------------------------------
# CGAL
#------------------------------------------------------------------------------
function(dcma_find_cgal)
    if(NOT DCMA_WITH_CGAL)
        message(STATUS "CGAL support disabled")
        return()
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)
        add_compile_definitions(CGAL_DISABLE_ROUNDING_MATH_CHECK=1)
        message(WARNING "If profiling/debugging on Arch Linux, disable binary stripping in PKGBUILD.")
    endif()

    if(DCMA_WITH_EIGEN)
        add_compile_definitions(CGAL_EIGEN3_ENABLED=1)
    endif()

    find_package(CGAL QUIET COMPONENTS Core)
    
    if(CGAL_FOUND)
        message(STATUS "Found CGAL version ${CGAL_VERSION}")
        set(DCMA_CGAL_AVAILABLE TRUE PARENT_SCOPE)
    else()
        message(STATUS "CGAL not found - disabling CGAL-dependent features")
        set(DCMA_CGAL_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_CGAL OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# NLopt
#------------------------------------------------------------------------------
function(dcma_find_nlopt)
    if(NOT DCMA_WITH_NLOPT)
        message(STATUS "NLopt support disabled")
        return()
    endif()

    # Try CMake config first
    find_package(NLopt QUIET CONFIG)
    
    if(NOT NLopt_FOUND)
        # Fall back to pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(NLOPT QUIET nlopt)
        endif()
    endif()

    if(NLopt_FOUND OR NLOPT_FOUND)
        message(STATUS "Found NLopt")
        set(DCMA_NLOPT_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target if pkg-config was used
        if(NOT TARGET NLopt::nlopt AND NLOPT_INCLUDE_DIRS)
            add_library(NLopt::nlopt INTERFACE IMPORTED)
            target_include_directories(NLopt::nlopt INTERFACE ${NLOPT_INCLUDE_DIRS})
            target_link_libraries(NLopt::nlopt INTERFACE ${NLOPT_LIBRARIES})
        endif()
    else()
        # Try FetchContent as fallback
        message(STATUS "NLopt not found, attempting FetchContent fallback")
        dcma_fetch_nlopt()
    endif()
endfunction()

function(dcma_fetch_nlopt)
    FetchContent_Declare(
        NLopt
        GIT_REPOSITORY https://github.com/stevengj/nlopt.git
        GIT_TAG v2.7.1
        GIT_SHALLOW TRUE
    )
    
    # Configure NLopt build options
    set(NLOPT_PYTHON OFF CACHE BOOL "" FORCE)
    set(NLOPT_OCTAVE OFF CACHE BOOL "" FORCE)
    set(NLOPT_MATLAB OFF CACHE BOOL "" FORCE)
    set(NLOPT_GUILE OFF CACHE BOOL "" FORCE)
    set(NLOPT_SWIG OFF CACHE BOOL "" FORCE)
    set(NLOPT_TESTS OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(NLopt)
    
    if(TARGET nlopt)
        set(DCMA_NLOPT_AVAILABLE TRUE PARENT_SCOPE)
        message(STATUS "NLopt fetched successfully")
    else()
        message(STATUS "NLopt fetch failed - disabling NLopt-dependent features")
        set(DCMA_NLOPT_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_NLOPT OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# SFML
#------------------------------------------------------------------------------
function(dcma_find_sfml)
    if(NOT DCMA_WITH_SFML)
        message(STATUS "SFML support disabled")
        return()
    endif()

    if(NOT BUILD_SHARED_LIBS)
        set(SFML_STATIC_LIBRARIES TRUE)
        add_compile_definitions(SFML_STATIC=1)
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SFML QUIET sfml-graphics sfml-window sfml-system)
    endif()

    if(SFML_FOUND)
        message(STATUS "Found SFML")
        set(DCMA_SFML_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target
        if(NOT TARGET SFML::All)
            add_library(SFML::All INTERFACE IMPORTED)
            target_include_directories(SFML::All INTERFACE ${SFML_INCLUDE_DIRS})
            target_link_libraries(SFML::All INTERFACE ${SFML_LIBRARIES})
        endif()
    else()
        message(STATUS "SFML not found - disabling SFML-dependent features")
        set(DCMA_SFML_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_SFML OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# SDL2 and OpenGL
#------------------------------------------------------------------------------
function(dcma_find_sdl)
    if(NOT DCMA_WITH_SDL)
        message(STATUS "SDL support disabled")
        return()
    endif()

    # Find OpenGL
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL QUIET COMPONENTS OpenGL)
    
    if(NOT OpenGL_FOUND)
        message(STATUS "OpenGL not found - disabling SDL-dependent features")
        set(DCMA_SDL_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_SDL OFF PARENT_SCOPE)
        return()
    endif()
    
    message(STATUS "Found OpenGL: ${OPENGL_LIBRARIES}")

    # Find SDL2
    find_package(SDL2 QUIET CONFIG)
    if(NOT SDL2_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET sdl2)
        endif()
    endif()

    if(NOT SDL2_FOUND)
        message(STATUS "SDL2 not found - disabling SDL-dependent features")
        set(DCMA_SDL_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_SDL OFF PARENT_SCOPE)
        return()
    endif()
    
    message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")

    # Create SDL2::SDL2 target if it doesn't exist
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        target_link_libraries(SDL2::SDL2 INTERFACE "${SDL2_LIBRARIES}")
        target_include_directories(SDL2::SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
    endif()

    # Find GLEW
    if(NOT BUILD_SHARED_LIBS)
        add_compile_definitions(GLEW_STATIC=1)
    endif()
    
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLEW QUIET glew)
    endif()

    if(NOT GLEW_FOUND)
        message(STATUS "GLEW not found - disabling SDL-dependent features")
        set(DCMA_SDL_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_SDL OFF PARENT_SCOPE)
        return()
    endif()
    
    message(STATUS "Found GLEW: ${GLEW_LIBRARIES}")

    # Create GLEW target
    if(NOT TARGET GLEW::GLEW)
        add_library(GLEW::GLEW INTERFACE IMPORTED)
        target_include_directories(GLEW::GLEW INTERFACE ${GLEW_INCLUDE_DIRS})
        target_link_libraries(GLEW::GLEW INTERFACE ${GLEW_LIBRARIES})
    endif()

    set(DCMA_SDL_AVAILABLE TRUE PARENT_SCOPE)
endfunction()

#------------------------------------------------------------------------------
# Wt (Web Toolkit)
#------------------------------------------------------------------------------
function(dcma_find_wt)
    if(NOT DCMA_WITH_WT)
        message(STATUS "Wt support disabled")
        return()
    endif()

    find_library(WT_LIB wt)
    find_library(WTHTTP_LIB wthttp)
    
    if(WT_LIB AND WTHTTP_LIB)
        message(STATUS "Found Wt: ${WT_LIB}")
        set(DCMA_WT_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported targets
        if(NOT TARGET Wt::Wt)
            add_library(Wt::Wt UNKNOWN IMPORTED)
            set_target_properties(Wt::Wt PROPERTIES IMPORTED_LOCATION ${WT_LIB})
        endif()
        if(NOT TARGET Wt::HTTP)
            add_library(Wt::HTTP UNKNOWN IMPORTED)
            set_target_properties(Wt::HTTP PROPERTIES IMPORTED_LOCATION ${WTHTTP_LIB})
        endif()
    else()
        message(STATUS "Wt not found - disabling Wt-dependent features")
        set(DCMA_WT_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_WT OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# GNU GSL
#------------------------------------------------------------------------------
function(dcma_find_gsl)
    if(NOT DCMA_WITH_GNU_GSL)
        message(STATUS "GNU GSL support disabled")
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GNU_GSL QUIET gsl)
    endif()

    if(GNU_GSL_FOUND)
        message(STATUS "Found GNU GSL: ${GNU_GSL_LIBRARIES}")
        set(DCMA_GSL_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target
        if(NOT TARGET GSL::gsl)
            add_library(GSL::gsl INTERFACE IMPORTED)
            target_include_directories(GSL::gsl INTERFACE ${GNU_GSL_INCLUDE_DIRS})
            target_link_libraries(GSL::gsl INTERFACE ${GNU_GSL_LIBRARIES})
        endif()
    else()
        message(STATUS "GNU GSL not found - disabling GSL-dependent features")
        set(DCMA_GSL_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_GNU_GSL OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# PostgreSQL
#------------------------------------------------------------------------------
function(dcma_find_postgres)
    if(NOT DCMA_WITH_POSTGRES)
        message(STATUS "PostgreSQL support disabled")
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(POSTGRES QUIET libpq libpqxx)
    endif()

    if(POSTGRES_FOUND)
        message(STATUS "Found PostgreSQL: ${POSTGRES_LIBRARIES}")
        set(DCMA_POSTGRES_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target
        if(NOT TARGET PostgreSQL::All)
            add_library(PostgreSQL::All INTERFACE IMPORTED)
            target_include_directories(PostgreSQL::All INTERFACE ${POSTGRES_INCLUDE_DIRS})
            target_link_libraries(PostgreSQL::All INTERFACE ${POSTGRES_LIBRARIES})
        endif()
    else()
        message(STATUS "PostgreSQL not found - disabling PostgreSQL-dependent features")
        set(DCMA_POSTGRES_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_POSTGRES OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# Jansson
#------------------------------------------------------------------------------
function(dcma_find_jansson)
    if(NOT DCMA_WITH_JANSSON)
        message(STATUS "Jansson support disabled")
        return()
    endif()

    find_library(JANSSON_LIB jansson)
    find_path(JANSSON_INCLUDE_DIR jansson.h)

    if(JANSSON_LIB AND JANSSON_INCLUDE_DIR)
        message(STATUS "Found Jansson: ${JANSSON_LIB}")
        set(DCMA_JANSSON_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target
        if(NOT TARGET Jansson::Jansson)
            add_library(Jansson::Jansson UNKNOWN IMPORTED)
            set_target_properties(Jansson::Jansson PROPERTIES
                IMPORTED_LOCATION ${JANSSON_LIB}
                INTERFACE_INCLUDE_DIRECTORIES ${JANSSON_INCLUDE_DIR}
            )
        endif()
    else()
        message(STATUS "Jansson not found - disabling Jansson-dependent features")
        set(DCMA_JANSSON_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_JANSSON OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# Apache Thrift
#------------------------------------------------------------------------------
function(dcma_find_thrift)
    if(NOT DCMA_WITH_THRIFT)
        message(STATUS "Thrift support disabled")
        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(THRIFT QUIET thrift)
    endif()

    if(THRIFT_FOUND)
        message(STATUS "Found Thrift: ${THRIFT_LIBRARIES}")
        set(DCMA_THRIFT_AVAILABLE TRUE PARENT_SCOPE)
        
        # Create imported target
        if(NOT TARGET Thrift::Thrift)
            add_library(Thrift::Thrift INTERFACE IMPORTED)
            target_include_directories(Thrift::Thrift INTERFACE ${THRIFT_INCLUDE_DIRS})
            target_link_libraries(Thrift::Thrift INTERFACE ${THRIFT_LIBRARIES})
        endif()
    else()
        message(STATUS "Thrift not found - disabling Thrift-dependent features")
        set(DCMA_THRIFT_AVAILABLE FALSE PARENT_SCOPE)
        set(DCMA_WITH_THRIFT OFF PARENT_SCOPE)
    endif()
endfunction()

#------------------------------------------------------------------------------
# Find all dependencies
#------------------------------------------------------------------------------
function(dcma_find_all_dependencies)
    dcma_create_common_interface()
    dcma_find_threads()
    dcma_find_boost()
    dcma_find_eigen()
    dcma_find_cgal()
    dcma_find_nlopt()
    dcma_find_sfml()
    dcma_find_sdl()
    dcma_find_wt()
    dcma_find_gsl()
    dcma_find_postgres()
    dcma_find_jansson()
    dcma_find_thrift()
endfunction()
