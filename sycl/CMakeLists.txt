
cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)

# Override the environmental CC and CXX flags with the SYCL toolchain wrapper (e.g., from hipSYCL).
#
# NOTE: This MUST preceed project().
set(CMAKE_C_COMPILER   "syclcc")
set(CMAKE_CXX_COMPILER "syclcc")

project(DICOMautomaton_SYCL LANGUAGES CXX)
#set(DICOMautomaton_VERSION_MAJOR 0)
#set(DICOMautomaton_VERSION_MINOR 0)
#set(DICOMautomaton_VERSION_PATCH 0)


####################################################################################
#                                  User Options
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile slowly, with minimal memory usage."    OFF)

option(WITH_EIGEN     "Compile assuming Eigen is available."                    ON)
option(WITH_NLOPT     "Compile assuming nlopt is available."                    ON)


####################################################################################
#                                  Configuration
####################################################################################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions (e.g., std=gnu++14).

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For use with clang-tidy et al.
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)

# Set the release type. 
if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()


####################################################################################
#                                  Dependencies 
####################################################################################

find_package(Threads REQUIRED)

find_package(Boost REQUIRED COMPONENTS filesystem serialization iostreams thread system)
include_directories(${Boost_INCLUDE_DIRS})

if(WITH_EIGEN)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EIGEN3 REQUIRED eigen3)
    include_directories( ${EIGEN3_INCLUDE_DIRS} )
endif()

if(WITH_NLOPT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NLOPT REQUIRED nlopt)
    include_directories( ${NLOPT_INCLUDE_DIRS} )
endif()


####################################################################################
#                                  Compiler Flags
####################################################################################

# Override the default CXX flags, which are controlled by the release type.
#
# Note: The '_DEBUG' flags are only applied when the release mode is 'Debug' -- likewise for the other flags.
#
# Note: If you want to fully override the CXX_FLAGS, then do not supply a build type and specify your CXX_FLAGS by
#       defining CMAKE_CXX_FLAGS when calling cmake.
set(CMAKE_CXX_FLAGS_DEBUG           "-O2 -g")
set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE         "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O3 -g")

if(MEMORY_CONSTRAINED_BUILD)
    # Do not overwrite user-provided flags, but do provide sane defaults.
    if(NOT CMAKE_CXX_FLAGS)
        set(CMAKE_CXX_FLAGS "-O0 -DNDEBUG")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG           "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL      "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE         "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O0 -DNDEBUG")
endif()


# Add other appropriate CXX flags.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-var-tracking-assignments")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic")

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        # Add gprof profiling flag.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
    endif()

    if(MEMORY_CONSTRAINED_BUILD)
        # Trigger garbage collection more frequently.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --param ggc-min-expand=10")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --param ggc-min-heapsize=32768")
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-optimization-argument")

    if(WITH_IWYU)
        set(IWYU_INVOCATION iwyu) # Location of the iwyu binary.
        list(APPEND IWYU_INVOCATION "-Xiwyu;--no_comments")
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_INVOCATION})
    endif()
endif()


include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__arm__     "cstdio" ARCH_IS_ARM)
check_cxx_symbol_exists(__aarch64__ "cstdio" ARCH_IS_ARM64)
if(ARCH_IS_ARM OR ARCH_IS_ARM64)
    message(STATUS "Detected ARM architecture.")

    # Enable to fix linking errors for toolchains that do not auto-detect atomic intrinsics (e.g., some ARM systems).
    # Note: Binaries built this way should not be distributed.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()


# Sanitizers.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common")

add_definitions(-U_FORTIFY_SOURCE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address") #  <-- includes LSan.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-address-use-after-scope")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=pointer-compare")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=pointer-subtract")

# Add UBSan checks too, since ASan, LSan, and UBSan are all compatible.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=undefined")

####################################################################################
#                                    Definitions
####################################################################################
if(WITH_EIGEN)
    message(STATUS "Assuming Eigen is available.")
    add_definitions(-DDCMA_USE_EIGEN=1)
else()
    message(STATUS "Assuming Eigen is not available.")
    add_definitions(-UDCMA_USE_EIGEN)
endif()

if(WITH_NLOPT)
    message(STATUS "Assuming NLOPT is available.")
    add_definitions(-DDCMA_USE_NLOPT=1)
else()
    message(STATUS "Assuming NLOPT is not available.")
    add_definitions(-UDCMA_USE_NLOPT)
endif()


# Use the directory where CMakeLists.txt is for inclusions.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)


