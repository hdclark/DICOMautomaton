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

# Locate the g++ compiler and record its absolute path.
find_program(GCC_CXX_COMPILER
    NAMES g++ g++-13 g++-12 g++-11 g++-10 g++-9 g++-8 g++-7
    HINTS
        /usr/local/bin
        /usr/bin
    DOC "GNU GCC C++ compiler"
)

# Locate the gcc compiler for C sources and record its absolute path.
find_program(GCC_C_COMPILER
    NAMES gcc gcc-13 gcc-12 gcc-11 gcc-10 gcc-9 gcc-8 gcc-7
    HINTS
        /usr/local/bin
        /usr/bin
    DOC "GNU GCC C compiler"
)

if(NOT GCC_CXX_COMPILER)
    message(FATAL_ERROR
        "GNU GCC C++ compiler (g++) not found. "
        "Install GCC or specify CMAKE_CXX_COMPILER=/path/to/g++ manually."
    )
endif()

if(NOT GCC_C_COMPILER)
    message(FATAL_ERROR
        "GNU GCC C compiler (gcc) not found. "
        "Install GCC or specify CMAKE_C_COMPILER=/path/to/gcc manually."
    )
endif()

# Set the compilers to the absolute paths returned by find_program().
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

# Restrict CMake package search to the target sysroot only.
# For native builds this prevents accidentally finding a mismatched library copy.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
