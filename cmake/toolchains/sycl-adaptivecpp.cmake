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
#   - The 'acpp' driver wraps the host C++ compiler and handles SYCL offloading.
#   - This toolchain only sets the compiler; all other CMake settings are
#     inherited from the main CMakeLists.txt.
#   - For native (non-cross) SYCL builds, this toolchain file changes only the
#     compiler driver; the target architecture is the host architecture.

# Locate the acpp compiler driver.
find_program(ACPP_COMPILER acpp
    HINTS
        /usr/bin
        /usr/local/bin
        "$ENV{ACPP_ROOT}/bin"
        "$ENV{ADAPTIVECPP_ROOT}/bin"
    DOC "AdaptiveCpp (acpp) SYCL compiler driver"
)

if(NOT ACPP_COMPILER)
    message(FATAL_ERROR
        "AdaptiveCpp 'acpp' compiler driver not found. "
        "Please install AdaptiveCpp or set ACPP_ROOT to its install prefix, "
        "or specify CMAKE_CXX_COMPILER manually."
    )
endif()

set(CMAKE_CXX_COMPILER "${ACPP_COMPILER}")
# Optionally set the C compiler to the same driver if needed.
# set(CMAKE_C_COMPILER "${ACPP_COMPILER}")
