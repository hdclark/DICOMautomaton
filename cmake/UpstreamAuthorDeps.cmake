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
    if(NOT ${pkg_name}_FOUND)
        if(WITH_FETCHCONTENT_FALLBACK)
            message(WARNING "${pkg_name} not found via find_package; fetching from ${git_url} (${git_tag}).")
            FetchContent_Declare(
                ${pkg_name}
                GIT_REPOSITORY ${git_url}
                GIT_TAG        ${git_tag}
            )
            # Use FetchContent_Populate (available in CMake 3.11+) instead of
            # FetchContent_MakeAvailable (requires 3.14+) to stay compatible
            # with CMake 3.12.
            FetchContent_GetProperties(${pkg_name})
            string(TOLOWER "${pkg_name}" _dcma_fc_lower)
            if(NOT ${_dcma_fc_lower}_POPULATED)
                FetchContent_Populate(${pkg_name})
                add_subdirectory(${${_dcma_fc_lower}_SOURCE_DIR}
                                 ${${_dcma_fc_lower}_BINARY_DIR})
            endif()
            unset(_dcma_fc_lower)
        else()
            message(FATAL_ERROR
                "${pkg_name} not found. Install ${pkg_name} or "
                "re-run CMake with -DWITH_FETCHCONTENT_FALLBACK=ON.")
        endif()
    endif()
endmacro()

# Ygor is the foundational library (Explicator may depend on it), so find
# it first.
_dcma_find_or_fetch(Ygor
    "https://github.com/hdclark/Ygor.git"
    master)

_dcma_find_or_fetch(YgorClustering
    "https://github.com/hdclark/YgorClustering.git"
    master)

_dcma_find_or_fetch(Explicator
    "https://github.com/hdclark/Explicator.git"
    master)
