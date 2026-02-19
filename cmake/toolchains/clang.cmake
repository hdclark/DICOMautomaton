# clang.cmake
#
# CMake toolchain file for building DICOMautomaton with the Clang/LLVM toolchain.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang.cmake \
#         /path/to/DICOMautomaton
#
# Prerequisites:
#   - Clang (clang++) must be installed and available in PATH or specified below.
#   - A recent version of Clang supporting C++17 is required (Clang 5+).
#
# Notes:
#   - Use CMAKE_CXX_FLAGS_INIT (not CMAKE_CXX_FLAGS) to set default compiler flags from
#     a toolchain file. This allows users to still append their own flags on the command
#     line without overwriting the toolchain defaults.
#   - This toolchain targets the host architecture (native build, not cross-compiling).

# Locate the clang++ compiler. The search includes PATH to adapt to various environments.
find_program(CLANG_CXX_COMPILER
    NAMES clang++ clang++-18 clang++-17 clang++-16 clang++-15 clang++-14 clang++-13 clang++-12 clang++-11 clang++-10 clang++-9 clang++-8 clang++-7 clang++-6.0 clang++-5.0
    HINTS
        /usr/local/bin
        /usr/bin
    DOC "Clang C++ compiler"
)

if(NOT CLANG_CXX_COMPILER)
    message(FATAL_ERROR
        "Clang C++ compiler (clang++) not found. "
        "Install Clang or specify CMAKE_CXX_COMPILER=/path/to/clang++ manually."
    )
endif()

# Derive the matching clang from the found clang++ to ensure version coherence.
# Remove '+' characters to transform clang++ -> clang (e.g., clang++-17 -> clang-17).
get_filename_component(_clangxx_dir "${CLANG_CXX_COMPILER}" DIRECTORY)
get_filename_component(_clangxx_name "${CLANG_CXX_COMPILER}" NAME)
string(REPLACE "++" "" _clang_name "${_clangxx_name}")
set(_derived_clang "${_clangxx_dir}/${_clang_name}")

# Verify the derived clang exists
if(EXISTS "${_derived_clang}")
    set(CLANG_C_COMPILER "${_derived_clang}")
else()
    message(FATAL_ERROR
        "Could not find matching Clang C compiler '${_derived_clang}' for clang++ '${CLANG_CXX_COMPILER}'. "
        "Install the matching Clang version or specify CMAKE_C_COMPILER=/path/to/clang manually."
    )
endif()

# Set the compilers to the absolute paths.
set(CMAKE_C_COMPILER "${CLANG_C_COMPILER}")
set(CMAKE_CXX_COMPILER "${CLANG_CXX_COMPILER}")

# Set the C++ standard (DICOMautomaton requires C++17).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default compiler flags for Clang.
# These can be appended to or overridden by user-specified flags.
set(CMAKE_CXX_FLAGS_INIT "")
set(CMAKE_C_FLAGS_INIT "")

# Control how CMake searches for packages relative to any configured root path.
# For native builds (no explicit CMAKE_FIND_ROOT_PATH), allow searching both the
# host system and any root paths so that system-installed dependencies are found.
if(CMAKE_FIND_ROOT_PATH)
    # When a sysroot/root path is explicitly configured, restrict searches to it.
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
else()
    # Native build without a sysroot: search both root paths (if any) and the host.
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
endif()
