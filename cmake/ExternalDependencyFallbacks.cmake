# ExternalDependencyFallbacks.cmake
#
# FetchContent fallback helpers for optional third-party dependencies.  These
# helpers are only used after the normal system package discovery path fails and
# only when WITH_FETCHCONTENT_FALLBACK is enabled.

function(_dcma_fetchcontent_populate dep_name)
    FetchContent_GetProperties(${dep_name})
    string(TOLOWER "${dep_name}" _dcma_fc_lower)
    if(NOT ${_dcma_fc_lower}_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(SET CMP0169 OLD) # Permit use of FetchContent_Populate().
        endif()
        FetchContent_Populate(${dep_name})
    endif()
    set(${dep_name}_SOURCE_DIR "${${_dcma_fc_lower}_SOURCE_DIR}" PARENT_SCOPE)
    set(${dep_name}_BINARY_DIR "${${_dcma_fc_lower}_BINARY_DIR}" PARENT_SCOPE)
    unset(_dcma_fc_lower)
endfunction()

function(_dcma_fetchcontent_add_subdirectory dep_name)
    set(_dcma_source_subdir "${ARGV1}")
    _dcma_fetchcontent_populate(${dep_name})
    set(_dcma_source_dir "${${dep_name}_SOURCE_DIR}")
    set(_dcma_binary_dir "${${dep_name}_BINARY_DIR}")
    if(NOT "${_dcma_source_subdir}" STREQUAL "")
        set(_dcma_source_dir "${_dcma_source_dir}/${_dcma_source_subdir}")
        set(_dcma_binary_dir "${_dcma_binary_dir}/${_dcma_source_subdir}")
    endif()
    if(EXISTS "${_dcma_source_dir}/CMakeLists.txt")
        add_subdirectory("${_dcma_source_dir}"
                         "${_dcma_binary_dir}"
                         EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "${dep_name} was fetched, but no CMakeLists.txt was found under ${_dcma_source_dir}.")
    endif()
endfunction()

function(_dcma_require_or_fetch dep_name)
    if(NOT WITH_FETCHCONTENT_FALLBACK)
        message(FATAL_ERROR "${dep_name} not found. Install the system package or set WITH_FETCHCONTENT_FALLBACK=ON.")
    endif()
    message(STATUS "${dep_name} not found; fetching via FetchContent.")
endfunction()

function(dcma_fetch_eigen3)
    _dcma_require_or_fetch(Eigen3)
    FetchContent_Declare(
        Eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3147391d946bb4b6c68edd901f2add6ac1f31f8c # 3.4.0
    )
    _dcma_fetchcontent_add_subdirectory(Eigen3)
    set(Eigen3_FOUND TRUE PARENT_SCOPE)
endfunction()

function(dcma_fetch_cgal)
    _dcma_require_or_fetch(CGAL)
    FetchContent_Declare(
        CGAL
        GIT_REPOSITORY https://github.com/CGAL/cgal.git
        GIT_TAG        v5.6.1
    )
    _dcma_fetchcontent_add_subdirectory(CGAL)
    set(CGAL_FOUND TRUE PARENT_SCOPE)
endfunction()

function(dcma_fetch_nlopt)
    _dcma_require_or_fetch(nlopt)
    set(NLOPT_CXX ON CACHE BOOL "Build the nlopt C++ interface." FORCE)
    set(NLOPT_PYTHON OFF CACHE BOOL "Do not build nlopt Python bindings." FORCE)
    set(NLOPT_OCTAVE OFF CACHE BOOL "Do not build nlopt Octave bindings." FORCE)
    set(NLOPT_MATLAB OFF CACHE BOOL "Do not build nlopt Matlab bindings." FORCE)
    set(NLOPT_GUILE OFF CACHE BOOL "Do not build nlopt Guile bindings." FORCE)
    set(NLOPT_SWIG OFF CACHE BOOL "Do not build nlopt SWIG bindings." FORCE)
    set(NLOPT_TESTS OFF CACHE BOOL "Do not build nlopt tests." FORCE)
    FetchContent_Declare(
        nlopt
        GIT_REPOSITORY https://github.com/stevengj/nlopt.git
        GIT_TAG        9c7018f617dcf74a6e42f67e0a14869f10e5c957 # v2.7.1
    )
    _dcma_fetchcontent_add_subdirectory(nlopt)
    set(NLOPT_FOUND TRUE PARENT_SCOPE)
    set(NLOPT_INCLUDE_DIRS "${nlopt_SOURCE_DIR}/src/api" PARENT_SCOPE)
    if(TARGET nlopt)
        set(NLOPT_LIBRARIES nlopt PARENT_SCOPE)
    elseif(TARGET nlopt_cxx)
        set(NLOPT_LIBRARIES nlopt_cxx PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_sfml)
    _dcma_require_or_fetch(SFML)
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "Do not build SFML audio." FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "Do not build SFML network." FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "Do not build SFML examples." FORCE)
    set(SFML_BUILD_DOC OFF CACHE BOOL "Do not build SFML documentation." FORCE)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        2.6.1
    )
    _dcma_fetchcontent_add_subdirectory(SFML)
    set(SFML_FOUND TRUE PARENT_SCOPE)
    set(SFML_INCLUDE_DIRS "${SFML_SOURCE_DIR}/include" PARENT_SCOPE)
    set(SFML_LIBRARIES sfml-graphics sfml-window sfml-system PARENT_SCOPE)
endfunction()

function(dcma_fetch_sdl2)
    _dcma_require_or_fetch(SDL2)
    set(SDL_SHARED ${BUILD_SHARED_LIBS} CACHE BOOL "Build SDL shared library." FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL static library." FORCE)
    set(SDL_TEST OFF CACHE BOOL "Do not build SDL tests." FORCE)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.12
    )
    _dcma_fetchcontent_add_subdirectory(SDL2)
    set(SDL2_FOUND TRUE PARENT_SCOPE)
    set(SDL2_INCLUDE_DIRS "${SDL2_SOURCE_DIR}/include" PARENT_SCOPE)
    if(TARGET SDL2::SDL2)
        set(SDL2_LIBRARIES SDL2::SDL2 PARENT_SCOPE)
    elseif(TARGET SDL2)
        set(SDL2_LIBRARIES SDL2 PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_glew)
    _dcma_require_or_fetch(GLEW)
    FetchContent_Declare(
        glew
        GIT_REPOSITORY https://github.com/nigels-com/glew.git
        GIT_TAG        glew-2.2.0
    )
    _dcma_fetchcontent_add_subdirectory(glew build/cmake)
    set(GLEW_FOUND TRUE PARENT_SCOPE)
    set(GLEW_INCLUDE_DIRS "${glew_SOURCE_DIR}/include" PARENT_SCOPE)
    if(TARGET libglew_static)
        set(GLEW_LIBRARIES libglew_static PARENT_SCOPE)
    elseif(TARGET glew)
        set(GLEW_LIBRARIES glew PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_gsl)
    _dcma_require_or_fetch(GNU_GSL)
    FetchContent_Declare(
        gsl
        GIT_REPOSITORY https://github.com/ampl/gsl.git
        GIT_TAG        v2.7.1
    )
    _dcma_fetchcontent_add_subdirectory(gsl)
    set(GNU_GSL_FOUND TRUE PARENT_SCOPE)
    set(GNU_GSL_INCLUDE_DIRS "${gsl_SOURCE_DIR}" "${gsl_BINARY_DIR}" PARENT_SCOPE)
    if(TARGET gsl)
        set(GNU_GSL_LIBRARIES gsl PARENT_SCOPE)
    elseif(TARGET GSL::gsl)
        set(GNU_GSL_LIBRARIES GSL::gsl PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_jansson)
    _dcma_require_or_fetch(Jansson)
    set(JANSSON_BUILD_DOCS OFF CACHE BOOL "Do not build Jansson docs." FORCE)
    set(JANSSON_EXAMPLES OFF CACHE BOOL "Do not build Jansson examples." FORCE)
    set(JANSSON_WITHOUT_TESTS ON CACHE BOOL "Do not build Jansson tests." FORCE)
    FetchContent_Declare(
        jansson
        GIT_REPOSITORY https://github.com/akheron/jansson.git
        GIT_TAG        v2.14
    )
    _dcma_fetchcontent_add_subdirectory(jansson)
    set(JANSSON_FOUND TRUE PARENT_SCOPE)
    set(JANSSON_INCLUDE_DIRS "${jansson_SOURCE_DIR}/src" "${jansson_BINARY_DIR}/include" PARENT_SCOPE)
    if(TARGET jansson)
        set(JANSSON_LIBRARIES jansson PARENT_SCOPE)
    elseif(TARGET jansson::jansson)
        set(JANSSON_LIBRARIES jansson::jansson PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_libpqxx)
    _dcma_require_or_fetch(libpqxx)
    set(BUILD_TEST OFF CACHE BOOL "Do not build libpqxx tests." FORCE)
    set(SKIP_BUILD_TEST ON CACHE BOOL "Do not build libpqxx tests." FORCE)
    FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG        7.9.2
    )
    _dcma_fetchcontent_add_subdirectory(libpqxx)
    set(POSTGRES_FOUND TRUE PARENT_SCOPE)
    set(POSTGRES_INCLUDE_DIRS "${libpqxx_SOURCE_DIR}/include" PARENT_SCOPE)
    if(TARGET pqxx)
        set(POSTGRES_LIBRARIES pqxx PARENT_SCOPE)
    elseif(TARGET libpqxx::pqxx)
        set(POSTGRES_LIBRARIES libpqxx::pqxx PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_thrift)
    _dcma_require_or_fetch(Thrift)
    set(BUILD_COMPILER OFF CACHE BOOL "Do not build the thrift compiler." FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Do not build thrift tests." FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build thrift examples." FORCE)
    set(WITH_SHARED_LIB ${BUILD_SHARED_LIBS} CACHE BOOL "Build thrift shared library." FORCE)
    set(WITH_STATIC_LIB ON CACHE BOOL "Build thrift static library." FORCE)
    set(WITH_PYTHON OFF CACHE BOOL "Do not build thrift Python support." FORCE)
    set(WITH_JAVA OFF CACHE BOOL "Do not build thrift Java support." FORCE)
    FetchContent_Declare(
        thrift
        GIT_REPOSITORY https://github.com/apache/thrift.git
        GIT_TAG        v0.20.0
    )
    _dcma_fetchcontent_add_subdirectory(thrift)
    set(THRIFT_FOUND TRUE PARENT_SCOPE)
    set(THRIFT_INCLUDE_DIRS "${thrift_SOURCE_DIR}/lib/cpp/src" "${thrift_BINARY_DIR}" PARENT_SCOPE)
    if(TARGET thrift::thrift)
        set(THRIFT_LIBRARIES thrift::thrift PARENT_SCOPE)
    elseif(TARGET thrift)
        set(THRIFT_LIBRARIES thrift PARENT_SCOPE)
    endif()
endfunction()

function(dcma_fetch_wt)
    _dcma_require_or_fetch(Wt)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build Wt examples." FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "Do not build Wt tests." FORCE)
    set(ENABLE_HARU OFF CACHE BOOL "Do not build Wt Haru support." FORCE)
    set(ENABLE_POSTGRES OFF CACHE BOOL "Do not build Wt PostgreSQL support." FORCE)
    set(ENABLE_MYSQL OFF CACHE BOOL "Do not build Wt MySQL support." FORCE)
    set(ENABLE_FIREBIRD OFF CACHE BOOL "Do not build Wt Firebird support." FORCE)
    set(ENABLE_QT4 OFF CACHE BOOL "Do not build Wt Qt4 support." FORCE)
    set(ENABLE_QT5 OFF CACHE BOOL "Do not build Wt Qt5 support." FORCE)
    FetchContent_Declare(
        wt
        GIT_REPOSITORY https://github.com/emweb/wt.git
        GIT_TAG        4.10.4
    )
    _dcma_fetchcontent_add_subdirectory(wt)
    set(Wt_FOUND TRUE PARENT_SCOPE)
    set(WT_INCLUDE_DIRS "${wt_SOURCE_DIR}/src" "${wt_BINARY_DIR}" PARENT_SCOPE)
    if(TARGET wt AND TARGET wthttp)
        set(WT_LIBRARIES wt wthttp PARENT_SCOPE)
    elseif(TARGET Wt::Wt AND TARGET Wt::HTTP)
        set(WT_LIBRARIES Wt::Wt Wt::HTTP PARENT_SCOPE)
    endif()
endfunction()
