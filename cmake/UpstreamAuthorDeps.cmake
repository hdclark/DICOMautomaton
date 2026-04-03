# UpstreamAuthorDeps.cmake
#
# Locate the author's upstream support libraries (Ygor, YgorClustering,
# Explicator) via find_package(). When a library is not found and the
# WITH_FETCHCONTENT_FALLBACK option is enabled, the library is downloaded
# and built as part of the DICOMautomaton build tree using FetchContent.
#
# This file is included from the top-level CMakeLists.txt after
# include(FetchContent) and after the user options have been defined.

# Helper macro: try find_package, fall back to FetchContent when allowed.
# Usage: _dcma_find_or_fetch(<PackageName> <git_url> <git_tag>)
macro(_dcma_find_or_fetch pkg_name git_url git_tag)
    find_package(${pkg_name} QUIET)

    if(${pkg_name}_FOUND)
        message(STATUS "Found ${pkg_name}.")
        message(STATUS "    ${pkg_name}_CONFIG = '${${pkg_name}_CONFIG}'")
        message(STATUS "    ${pkg_name}_VERSION = '${${pkg_name}_VERSION}'")

    else()
        message(WARNING "${pkg_name} not found via find_package")

        if(WITH_FETCHCONTENT_FALLBACK)
            message(STATUS "Attempting to fetch ${pkg_name} from ${git_url} (${git_tag}).")
            message(STATUS "=============================================================")
            FetchContent_Declare(
                ${pkg_name}
                GIT_REPOSITORY ${git_url}
                GIT_TAG        ${git_tag}
            )

            # FetchContent_Populate is used instead of FetchContent_MakeAvailable
            # because this project targets CMake 3.12 as its minimum version
            # and MakeAvailable was only introduced in CMake 3.14.
            FetchContent_GetProperties(${pkg_name})
            string(TOLOWER "${pkg_name}" _dcma_fc_lower)
            if(NOT ${_dcma_fc_lower}_POPULATED)
                if(POLICY CMP0169)
                    cmake_policy(SET CMP0169 OLD) # Permit use of FetchContent_Populate()
                endif()
                FetchContent_Populate(${pkg_name})
                message(STATUS "Fetched ${pkg_name}")
                add_subdirectory("${${_dcma_fc_lower}_SOURCE_DIR}"
                                 "${${_dcma_fc_lower}_BINARY_DIR}")

                # FetchContent_Populate adds a regular target (e.g., 'ygor') not an imported
                # target (with a namespace, e.g., 'Ygor::ygor').
                #
                # Manually add a namespaced alias target to workaround this shortcoming.
                if(NOT TARGET ${pkg_name}::${_dcma_fc_lower} AND TARGET ${_dcma_fc_lower})
                    add_library(${pkg_name}::${_dcma_fc_lower} ALIAS ${_dcma_fc_lower})
                    message(STATUS "Manually added namespaced alias for ${pkg_name}")
                endif()
            endif()
            unset(_dcma_fc_lower)
            message(STATUS "=============================================================")

        else()
            message(FATAL_ERROR "${pkg_name} not found and could not be fetched.")

        endif()
    endif()
endmacro()


_dcma_find_or_fetch(Ygor
    "https://github.com/hdclark/Ygor.git"
    master)

_dcma_find_or_fetch(YgorClustering
    "https://github.com/hdclark/YgorClustering.git"
    master)

_dcma_find_or_fetch(Explicator
    "https://github.com/hdclark/Explicator.git"
    master)

