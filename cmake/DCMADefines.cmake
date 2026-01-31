# DCMADefines.cmake
# Modern CMake module for project-wide definitions
#
# This module provides functions to set up compile definitions based on feature availability.

include_guard(GLOBAL)

# Set up feature compile definitions based on what's available
function(dcma_configure_feature_definitions)
    # Core definitions
    add_compile_definitions(USTREAM_H)  # -DUSE_ICU_STRINGS

    # Feature-specific definitions
    if(DCMA_WITH_EIGEN AND DCMA_EIGEN_AVAILABLE)
        message(STATUS "Eigen support enabled")
        add_compile_definitions(DCMA_USE_EIGEN=1)
    else()
        message(STATUS "Eigen support disabled")
    endif()

    if(DCMA_WITH_CGAL AND DCMA_CGAL_AVAILABLE)
        message(STATUS "CGAL support enabled")
        add_compile_definitions(DCMA_USE_CGAL=1)
    else()
        message(STATUS "CGAL support disabled")
    endif()

    if(DCMA_WITH_NLOPT AND DCMA_NLOPT_AVAILABLE)
        message(STATUS "NLopt support enabled")
        add_compile_definitions(DCMA_USE_NLOPT=1)
    else()
        message(STATUS "NLopt support disabled")
    endif()

    if(DCMA_WITH_SFML AND DCMA_SFML_AVAILABLE)
        message(STATUS "SFML support enabled")
        add_compile_definitions(DCMA_USE_SFML=1)
    else()
        message(STATUS "SFML support disabled")
    endif()

    if(DCMA_WITH_SDL AND DCMA_SDL_AVAILABLE)
        message(STATUS "SDL support enabled")
        add_compile_definitions(DCMA_USE_SDL=1)
        add_compile_definitions(DCMA_USE_GLEW=1)
    else()
        message(STATUS "SDL support disabled")
    endif()

    if(DCMA_WITH_WT AND DCMA_WT_AVAILABLE)
        message(STATUS "Wt support enabled")
        add_compile_definitions(DCMA_USE_WT=1)
    else()
        message(STATUS "Wt support disabled")
    endif()

    if(DCMA_WITH_POSTGRES AND DCMA_POSTGRES_AVAILABLE)
        message(STATUS "PostgreSQL support enabled")
        add_compile_definitions(DCMA_USE_POSTGRES=1)
    else()
        message(STATUS "PostgreSQL support disabled")
    endif()

    if(DCMA_WITH_JANSSON AND DCMA_JANSSON_AVAILABLE)
        message(STATUS "Jansson support enabled")
        add_compile_definitions(DCMA_USE_JANSSON=1)
    else()
        message(STATUS "Jansson support disabled")
    endif()

    if(DCMA_WITH_GNU_GSL AND DCMA_GSL_AVAILABLE)
        message(STATUS "GNU GSL support enabled")
        add_compile_definitions(DCMA_USE_GNU_GSL=1)
    else()
        message(STATUS "GNU GSL support disabled")
    endif()

    if(DCMA_WITH_THRIFT AND DCMA_THRIFT_AVAILABLE)
        message(STATUS "Thrift support enabled")
        add_compile_definitions(DCMA_USE_THRIFT=1)
    else()
        message(STATUS "Thrift support disabled")
    endif()
endfunction()

# Extract DCMA version from script or use provided value
function(dcma_configure_version)
    if(NOT DEFINED DCMA_VERSION OR "${DCMA_VERSION}" STREQUAL "")
        execute_process(
            COMMAND "${CMAKE_SOURCE_DIR}/scripts/extract_dcma_version.sh"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE DCMA_VERSION
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    if("${DCMA_VERSION}" STREQUAL "")
        message(WARNING "Please supply a meaningful DCMA_VERSION when invoking CMake.")
        set(DCMA_VERSION "unknown")
    endif()

    set(DCMA_VERSION "${DCMA_VERSION}" PARENT_SCOPE)
    message(STATUS "DCMA Version: ${DCMA_VERSION}")
endfunction()

# Configure the DCMA_Definitions.h header file
function(dcma_configure_definitions_header)
    configure_file(
        "${CMAKE_SOURCE_DIR}/src/DCMA_Definitions.h.in"
        "${CMAKE_BINARY_DIR}/src/DCMA_Definitions.h"
    )
endfunction()

# Create an interface library that holds all the common definitions
function(dcma_create_definitions_interface)
    if(NOT TARGET DCMA::Definitions)
        add_library(dcma_definitions INTERFACE)
        add_library(DCMA::Definitions ALIAS dcma_definitions)
        
        target_include_directories(dcma_definitions INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endfunction()
