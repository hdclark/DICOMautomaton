# sycl-adaptivecpp.cmake
#
# CMake toolchain file for building DICOMautomaton with AdaptiveCpp (formerly hipSYCL) SYCL support.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/sycl-adaptivecpp.cmake \
#         -DWITH_EXT_SYCL=ON \
#         -DACPP_TARGETS="generic" \
#         /path/to/DICOMautomaton
#
# Prerequisites:
#   - AdaptiveCpp must be installed and 'acpp' must be in PATH (or specify full path below).
#   - See https://github.com/AdaptiveCpp/AdaptiveCpp for installation instructions.
#
# Notes:
#   - Use CMAKE_CXX_FLAGS_INIT (not CMAKE_CXX_FLAGS) to set default compiler flags from
#     a toolchain file. This allows users to still append their own flags on the command
#     line without overwriting the toolchain defaults.
#   - The 'acpp' driver wraps the host C++ compiler and handles SYCL offloading.
#   - This toolchain targets the host architecture (native SYCL build, not cross-compiling).

# Locate the acpp compiler driver using an absolute path to avoid PATH poisoning.
# Prefer the location specified by ACPP_ROOT or ADAPTIVECPP_ROOT environment variables.
# find_program() always returns an absolute path when it succeeds.
find_program(ACPP_COMPILER
    NAMES acpp
    HINTS
        "$ENV{ACPP_ROOT}/bin"
        "$ENV{ADAPTIVECPP_ROOT}/bin"
        /usr/local/bin
        /usr/bin
    NO_DEFAULT_PATH   # Search only the explicit HINTS; do not use $PATH implicitly.
    DOC "AdaptiveCpp (acpp) SYCL compiler driver (absolute path)"
)
# Fall back to a PATH search if the hints didn't find it.
if(NOT ACPP_COMPILER)
    find_program(ACPP_COMPILER
        NAMES acpp
        DOC "AdaptiveCpp (acpp) SYCL compiler driver"
    )
endif()

if(NOT ACPP_COMPILER)
    message(FATAL_ERROR
        "AdaptiveCpp 'acpp' compiler driver not found. "
        "Set ACPP_ROOT (or ADAPTIVECPP_ROOT) to the AdaptiveCpp install prefix, "
        "or specify CMAKE_CXX_COMPILER=/path/to/acpp manually."
    )
endif()

# Set the C++ compiler to the absolute path returned by find_program().
set(CMAKE_CXX_COMPILER "${ACPP_COMPILER}")

# Restrict CMake package search to the target sysroot only.
# For native SYCL builds this prevents accidentally finding a mismatched host
# library copy; for future cross-compilation scenarios it is essential.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
