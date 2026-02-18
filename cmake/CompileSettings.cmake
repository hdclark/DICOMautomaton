# CompileSettings.cmake
#
# Global Options Target for DICOMautomaton.
#
# This file defines the 'dcma_compile_settings' INTERFACE library, which acts as
# the project-wide "Global Options Target". Every other target in the project links
# against it to inherit compiler flags, compile definitions, and include directories.
#
# Using an INTERFACE library (rather than global add_compile_options /
# add_definitions / include_directories) provides:
#   - Proper per-target scoping — flags are not leaked to third-party targets.
#   - Correct propagation to downstream CMake consumers via the installed
#     DICOMautomatonTargets.cmake.
#   - Clean generator-expression-based include paths that work both in the build
#     tree and after installation.
#
# This file is included from the top-level CMakeLists.txt *after* all dependency
# detection and option variables have been set. Variables such as WITH_EIGEN,
# Boost_INCLUDE_DIRS, NLOPT_INCLUDE_DIRS, etc. must therefore be set before this
# file is included.
#
# Downstream CMake consumer usage after install:
#   find_package(DICOMautomaton REQUIRED)
#   target_link_libraries(my_target PRIVATE DICOMautomaton::dcma_compile_settings)

add_library(dcma_compile_settings INTERFACE)

# C++ standard via compile_features (preferred over CMAKE_CXX_STANDARD for
# per-target control and correct propagation to consumers).
target_compile_features(dcma_compile_settings INTERFACE cxx_std_17)


####################################################################################
#                              Build-type Flags
####################################################################################
# These set the global per-build-type flag strings. They affect ALL C++ targets
# (including third-party ones), but are intentionally kept as global variables
# rather than target properties because they are build-system-level configuration.

set(CMAKE_CXX_FLAGS_DEBUG           "-O2 -g")
set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE         "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O3 -g")

if(MEMORY_CONSTRAINED_BUILD)
    # Do not overwrite user-provided flags, but do provide sane defaults.
    if(NOT CMAKE_CXX_FLAGS)
        set(CMAKE_CXX_FLAGS "-Os -DNDEBUG")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG           "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE         "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-Os -DNDEBUG")
endif()


####################################################################################
#                          Compiler-Specific Warning Flags
####################################################################################

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(dcma_compile_settings INTERFACE
        -fdiagnostics-color=always
        -fno-var-tracking-assignments
        #-ftrivial-auto-var-init=pattern
        -Wno-deprecated
        -Wall
        -Wextra
        -Wpedantic
        -Wdeprecated
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
        -Wno-maybe-initialized )

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        # Add gprof profiling flag.
        target_compile_options(dcma_compile_settings INTERFACE
            -pg
            -fno-omit-frame-pointer
            -fstack-check )
    endif()

    if(MEMORY_CONSTRAINED_BUILD)
        # Trigger garbage collection more frequently.
        target_compile_options(dcma_compile_settings INTERFACE
            "SHELL:--param ggc-min-expand=10"
            "SHELL:--param ggc-min-heapsize=32768" )
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(dcma_compile_settings INTERFACE
        -fdiagnostics-color=always
        -Wall
        -Wextra
        -Wpedantic
        -Wdeprecated
        #-Wno-ignored-optimization-argument
        -Wno-unused-lambda-capture
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-reserved-identifier )

    if(WITH_IWYU)
        set(IWYU_INVOCATION iwyu) # Location of the iwyu binary.
        list(APPEND IWYU_INVOCATION "-Xiwyu;--no_comments")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_INVOCATION})
    endif()
endif()


####################################################################################
#                                   Sanitizers
####################################################################################

if(WITH_ASAN  OR  WITH_TSAN  OR  WITH_MSAN)
    target_compile_options(dcma_compile_settings INTERFACE
        -O2
        -g
        -fno-omit-frame-pointer
        -fno-common )

    # Also enable coverage instrumentation, since sanitizers will typically be used for testing.
    target_compile_options(dcma_compile_settings INTERFACE
        --coverage
        # Clang only? Need to confirm ... TODO
        #-fsanitize-coverage=trace-pc-guard
        #-fprofile-instr-generate
        #-fcoverage-mapping
    )
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        link_libraries(gcov)
    endif()

    # -U_FORTIFY_SOURCE is a compiler flag (undefine), not a #define, so use compile_options.
    target_compile_options(dcma_compile_settings INTERFACE -U_FORTIFY_SOURCE)
endif()
if(WITH_ASAN)
    target_compile_options(dcma_compile_settings INTERFACE
        -fsanitize=address
        -fsanitize-address-use-after-scope )
    add_link_options(-fsanitize=address)

    target_compile_options(dcma_compile_settings INTERFACE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined )
    add_link_options(-fsanitize=undefined)

elseif(WITH_TSAN)
    message(WARNING "TSan may not support exceptions (depends on the compiler version and platform).")
    target_compile_options(dcma_compile_settings INTERFACE -fsanitize=thread)
    add_link_options(-fsanitize=thread)
elseif(WITH_MSAN)
    message(WARNING "MSan may not be available on your system.")
    target_compile_options(dcma_compile_settings INTERFACE
        -fsanitize=memory
        -fPIE
        -pie
        -fsanitize-memory-track-origins )
    add_link_options(-fsanitize=memory)
endif()


####################################################################################
#                             Architecture Detection
####################################################################################

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__arm__     "cstdio" ARCH_IS_ARM)
check_cxx_symbol_exists(__aarch64__ "cstdio" ARCH_IS_ARM64)
if(ARCH_IS_ARM OR ARCH_IS_ARM64)
    message(STATUS "Detected ARM architecture.")
    if(CMAKE_CXX_FLAGS MATCHES "-march=|-mcpu=|-mtune=")
        message(STATUS "Architecture set by user.")
    else()
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            # Enable to fix linking errors for toolchains that do not auto-detect atomic intrinsics
            # (e.g., some ARM systems). Note: Binaries built this way should not be distributed.
            message(STATUS "No architecture set, adding march=native flag")
            target_compile_options(dcma_compile_settings INTERFACE -march=native)
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            message(STATUS "No architecture set, adding mcpu=generic flag")
            target_compile_options(dcma_compile_settings INTERFACE -mcpu=generic)
        else()
            message(WARNING "Not able to set architecture; set it manually if compilation errors occur.")
        endif()
    endif()
endif()


####################################################################################
#                             Compile Definitions
####################################################################################

check_cxx_symbol_exists(std::regex::multiline "regex" HAS_REGEX_MULTILINE)
check_cxx_symbol_exists(std::quick_exit "cstdlib" HAS_QUICK_EXIT)  # C++11 feature missing from macOS libc.

target_compile_definitions(dcma_compile_settings INTERFACE
    USTREAM_H   # -DUSE_ICU_STRINGS
    $<$<BOOL:${HAS_REGEX_MULTILINE}>:DCMA_CPPSTDLIB_HAS_REGEX_MULTILINE=1>
    $<$<BOOL:${HAS_QUICK_EXIT}>:DCMA_CPPSTDLIB_HAS_QUICK_EXIT=1>
)

if(WITH_EIGEN)
    message(STATUS "Assuming Eigen is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_EIGEN=1)
    if(WITH_CGAL)
        target_compile_definitions(dcma_compile_settings INTERFACE CGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.
    endif()
else()
    message(STATUS "Assuming Eigen is not available.")
endif()

if(WITH_CGAL)
    message(STATUS "Assuming CGAL is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_CGAL=1)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        target_compile_definitions(dcma_compile_settings INTERFACE CGAL_DISABLE_ROUNDING_MATH_CHECK=1) # May be needed for Valgrind debugging.
    endif()
    if(NOT DCMA_CGAL_HAS_IMPLICIT_MESH_DOMAIN_3_HEADER)
        target_compile_definitions(dcma_compile_settings INTERFACE DCMA_CGAL_HAS_LABELED_MESH_DOMAIN_3_HEADER=1)
    endif()
else()
    message(STATUS "Assuming CGAL is not available.")
endif()

if(WITH_NLOPT)
    message(STATUS "Assuming NLOPT is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_NLOPT=1)
else()
    message(STATUS "Assuming NLOPT is not available.")
endif()

if(WITH_SFML)
    message(STATUS "Assuming SFML is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_SFML=1)
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(dcma_compile_settings INTERFACE SFML_STATIC=1)
    endif()
else()
    message(STATUS "Assuming SFML is not available.")
endif()

if(WITH_SDL)
    message(STATUS "Assuming SDL is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_SDL=1 DCMA_USE_GLEW=1)
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(dcma_compile_settings INTERFACE GLEW_STATIC=1)
    endif()
else()
    message(STATUS "Assuming SDL is not available.")
endif()

if(WITH_WT)
    message(STATUS "Assuming Wt is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_WT=1)
else()
    message(STATUS "Assuming Wt is not available.")
endif()

if(WITH_POSTGRES)
    message(STATUS "Assuming PostgreSQL client libraries are available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_POSTGRES=1)
else()
    message(STATUS "Assuming PostgreSQL client libraries are not available.")
endif()

if(WITH_JANSSON)
    message(STATUS "Assuming Jansson is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_JANSSON=1)
else()
    message(STATUS "Assuming Jansson is not available.")
endif()

if(WITH_GNU_GSL)
    message(STATUS "Assuming the GNU GSL is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_GNU_GSL=1)
else()
    message(STATUS "Assuming the GNU GSL is not available.")
endif()

if(WITH_THRIFT)
    message(STATUS "Assuming Apache Thrift is available.")
    target_compile_definitions(dcma_compile_settings INTERFACE DCMA_USE_THRIFT=1)
else()
    message(STATUS "Assuming Apache Thrift is not available.")
endif()

if("${DCMA_SYCL_BACKEND}" STREQUAL "AdaptiveCPP")
    message(STATUS "Using SYCL backend: AdaptiveCPP")
    target_compile_definitions(dcma_compile_settings INTERFACE
        DCMA_WHICH_SYCL="AdaptiveCPP"
        DCMA_USE_SYCL_ADAPTIVECPP=1
        DCMA_USE_EXT_SYCL=1)
    set(WITH_SYCL_FALLBACK OFF)
elseif("${DCMA_SYCL_BACKEND}" STREQUAL "TriSYCL")
    message(STATUS "Using SYCL backend: TriSYCL")
    target_compile_definitions(dcma_compile_settings INTERFACE
        DCMA_WHICH_SYCL="TriSYCL"
        DCMA_USE_SYCL_TRISYCL=1
        DCMA_USE_EXT_SYCL=1)
    set(WITH_SYCL_FALLBACK OFF)
else()
    message(STATUS "Using SYCL backend: Fallback")
    target_compile_definitions(dcma_compile_settings INTERFACE
        DCMA_WHICH_SYCL="Fallback"
        DCMA_USE_SYCL_FALLBACK=1)
    set(WITH_SYCL_FALLBACK ON)
endif()

# mingw-w64 tweaks.
if(MINGW OR WIN32)
    # Target Windows 7 features (need to specify for ASIO, maybe other libs).
    target_compile_definitions(dcma_compile_settings INTERFACE _WIN32_WINNT=0x0601)
    # Create 'console' applications, so a terminal will appear when running.
    link_libraries("-mconsole")
    # Override IFileDialogs check to disable.
    # See https://github.com/samhocevar/portable-file-dialogs/issues/50
    target_compile_definitions(dcma_compile_settings INTERFACE PFD_HAS_IFILEDIALOG=0)
endif()


####################################################################################
#                              Include Directories
####################################################################################
# Wrap pkg-config/find_package include directories in $<BUILD_INTERFACE:> so they
# are not exported to downstream consumers who install DICOMautomaton on a
# different system with different paths.

target_include_directories(dcma_compile_settings INTERFACE
    $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>
    # The generated DCMA_Definitions.h lives here:
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src>
    # Allow source-relative includes:
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if(WITH_EIGEN)
    if(TARGET Eigen3::Eigen)
        target_link_libraries(dcma_compile_settings INTERFACE Eigen3::Eigen)
    else()
        target_include_directories(dcma_compile_settings INTERFACE
            $<BUILD_INTERFACE:${EIGEN3_INCLUDE_DIRS}>)
    endif()
endif()
if(WITH_NLOPT)
    target_include_directories(dcma_compile_settings INTERFACE
        $<BUILD_INTERFACE:${NLOPT_INCLUDE_DIRS}>)
endif()
if(WITH_SFML)
    target_include_directories(dcma_compile_settings INTERFACE
        $<BUILD_INTERFACE:${SFML_INCLUDE_DIRS}>)
endif()
if(WITH_SDL)
    target_include_directories(dcma_compile_settings INTERFACE
        $<BUILD_INTERFACE:${SDL2_INCLUDE_DIRS}>
        $<BUILD_INTERFACE:${OPENGL_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${GLEW_INCLUDE_DIRS}>
    )
endif()
if(WITH_GNU_GSL)
    target_include_directories(dcma_compile_settings INTERFACE
        $<BUILD_INTERFACE:${GNU_GSL_INCLUDE_DIRS}>)
endif()
if(WITH_POSTGRES)
    target_include_directories(dcma_compile_settings INTERFACE
        $<BUILD_INTERFACE:${POSTGRES_INCLUDE_DIRS}>)
endif()
if(WITH_THRIFT)
    target_include_directories(dcma_compile_settings INTERFACE
        $<BUILD_INTERFACE:${THRIFT_INCLUDE_DIRS}>)
endif()
