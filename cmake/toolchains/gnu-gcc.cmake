# gnu-gcc.cmake
#
# CMake toolchain file for building DICOMautomaton with the GNU GCC toolchain.
# This is the default, recommended toolchain for most Linux systems.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/gnu-gcc.cmake \
#         /path/to/DICOMautomaton
#
# Prerequisites:
#   - GCC (g++) must be installed and available in PATH or specified below.
#   - A recent version of GCC supporting C++17 is required (GCC 7+).
#
# Notes:
#   - Use CMAKE_CXX_FLAGS_INIT (not CMAKE_CXX_FLAGS) to set default compiler flags from
#     a toolchain file. This allows users to still append their own flags on the command
#     line without overwriting the toolchain defaults.
#   - This toolchain targets the host architecture (native build, not cross-compiling).

# Locate the g++ compiler. The recorded path will be absolute.
find_program(GCC_CXX_COMPILER
    NAMES g++ g++-13 g++-12 g++-11 g++-10 g++-9 g++-8 g++-7
    HINTS
        /usr/local/bin
        /usr/bin
    DOC "GNU GCC C++ compiler"
)

if(NOT GCC_CXX_COMPILER)
    message(FATAL_ERROR
        "GNU GCC C++ compiler (g++) not found. "
        "Install GCC or specify CMAKE_CXX_COMPILER=/path/to/g++ manually."
    )
endif()

# Derive the matching gcc from the found g++ to ensure version coherence.
# Replace '+' characters with 'c' to transform g++ -> gcc (e.g., g++-13 -> gcc-13).
get_filename_component(_gxx_dir "${GCC_CXX_COMPILER}" DIRECTORY)
get_filename_component(_gxx_name "${GCC_CXX_COMPILER}" NAME)
string(REPLACE "+" "c" _gcc_name "${_gxx_name}")
set(_derived_gcc "${_gxx_dir}/${_gcc_name}")

# Verify the derived gcc exists
if(EXISTS "${_derived_gcc}")
    set(GCC_C_COMPILER "${_derived_gcc}")
else()
    message(FATAL_ERROR
        "Could not find matching GCC C compiler '${_derived_gcc}' for g++ '${GCC_CXX_COMPILER}'. "
        "Install the matching GCC version or specify CMAKE_C_COMPILER=/path/to/gcc manually."
    )
endif()

# Set the compilers to the absolute paths.
set(CMAKE_C_COMPILER "${GCC_C_COMPILER}")
set(CMAKE_CXX_COMPILER "${GCC_CXX_COMPILER}")

# Set the C++ standard (DICOMautomaton requires C++17).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default compiler flags for GCC.
# These can be appended to or overridden by user-specified flags.
set(CMAKE_CXX_FLAGS_INIT "")
set(CMAKE_C_FLAGS_INIT "")

# Restrict CMake package search to an explicitly configured sysroot/root path only.
# For native builds without a sysroot, rely on CMake defaults so system packages remain discoverable.
if(CMAKE_FIND_ROOT_PATH OR CMAKE_SYSROOT)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()
