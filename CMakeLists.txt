
cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)
project(DICOMautomaton LANGUAGES CXX)
#set(DICOMautomaton_VERSION_MAJOR 0)
#set(DICOMautomaton_VERSION_MINOR 0)
#set(DICOMautomaton_VERSION_PATCH 0)

####################################################################################
#                                  User Options
####################################################################################

option(MEMORY_CONSTRAINED_BUILD "Compile slowly, with minimal memory usage."    OFF)
option(WITH_IWYU                "Compile using clang include-what-you-use."     OFF)

option(WITH_ASAN                "Compile using ASan, LSan, & UBSan."            OFF)
option(WITH_TSAN                "Compile using ThreadSanitizer."                OFF)
option(WITH_MSAN                "Compile using MemorySanitizer."                OFF)

option(WITH_EIGEN     "Compile assuming Eigen is available."                    ON)
option(WITH_CGAL      "Compile assuming CGAL is available."                     ON)
option(WITH_NLOPT     "Compile assuming nlopt is available."                    ON)
option(WITH_SFML      "Compile assuming SFML is available."                     ON)
option(WITH_SDL       "Compile assuming SDL2 and glew are available."           ON)
option(WITH_WT        "Compile assuming Wt is available."                       ON)
option(WITH_GNU_GSL   "Compile assuming the GNU GSL is available."              ON)
option(WITH_POSTGRES  "Compile assuming PostgreSQL libraries are available."    ON)
option(WITH_JANSSON   "Compile assuming Jansson is available."                  ON)

option(BUILD_SHARED_LIBS "Build shared-object/dynamicly-loaded binaries."       ON)


####################################################################################
#                                  Configuration
####################################################################################

# High-level configuration.
if(NOT BUILD_SHARED_LIBS)
    #set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    link_libraries("-static")
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

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
# Note: Dependencies are listed in CPACK list below.

find_package(Threads REQUIRED)

if(NOT BUILD_SHARED_LIBS)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost REQUIRED COMPONENTS filesystem serialization iostreams thread system)
include_directories(${Boost_INCLUDE_DIRS})

if(WITH_EIGEN)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EIGEN3 REQUIRED eigen3)
    include_directories( ${EIGEN3_INCLUDE_DIRS} )
endif()

if(WITH_CGAL)
    find_package(CGAL REQUIRED COMPONENTS Core)
    include(${CGAL_USE_FILE})
endif()

if(WITH_NLOPT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NLOPT REQUIRED nlopt)
    include_directories( ${NLOPT_INCLUDE_DIRS} )
endif()

if(WITH_SFML)
    #find_package(SFML COMPONENTS graphics window system main REQUIRED)
    find_package(PkgConfig REQUIRED)
    if(NOT BUILD_SHARED_LIBS)
        set(SFML_STATIC_LIBRARIES TRUE)
        add_definitions(-DSFML_STATIC=1)
    endif()
    pkg_check_modules(SFML REQUIRED sfml-graphics sfml-window sfml-system)
    include_directories( ${SFML_INCLUDE_DIRS} )
endif()

if(WITH_SDL)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED COMPONENTS OpenGL) # Will help pick-up opengl32.a with MXE toolchain.
    message(STATUS "Proceeding with OPENGL_LIBRARIES = ${OPENGL_LIBRARIES}")
    message(STATUS "Proceeding with OPENGL_INCLUDE_DIR = ${OPENGL_INCLUDE_DIR}")
    include_directories( ${OPENGL_INCLUDE_DIR} )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    include_directories( ${SDL2_INCLUDE_DIRS} )

    if(NOT BUILD_SHARED_LIBS)
        add_definitions(-DGLEW_STATIC=1)
    endif()
    pkg_check_modules(GLEW REQUIRED glew)
    include_directories( ${GLEW_INCLUDE_DIRS} )
endif()

if(WITH_GNU_GSL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GNU_GSL REQUIRED gsl)
    include_directories( ${GNU_GSL_INCLUDE_DIRS} )
endif()

if(WITH_POSTGRES)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POSTGRES REQUIRED libpq libpqxx)
    include_directories( ${POSTGRES_INCLUDE_DIRS} )
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


# Sanitizers.
if(WITH_ASAN  OR  WITH_TSAN  OR  WITH_MSAN)
    #set(CMAKE_CXX_FLAGS " ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common")

    # Also enable coverage instrumentation, since sanitizers will typically be used for testing.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    # Clang only? Need to confirm ... TODO
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-coverage=trace-pc-guard")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoverage-mapping")

    add_definitions(-U_FORTIFY_SOURCE)
endif()
if(WITH_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address") #  <-- includes LSan.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-address-use-after-scope")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=pointer-compare")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=pointer-subtract")

    # Add UBSan checks too, since ASan, LSan, and UBSan are all compatible.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=undefined")
    # Clang only? Need to confirm ... TODO
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=implicit-conversion")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=local-bounds")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=nullability-arg")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=nullability-assign")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=nullability-return")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=unsigned-integer-overflow")
elseif(WITH_TSAN)
    message(WARNING "TSan may not support exceptions (depends on the compiler version and platform).")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
elseif(WITH_MSAN)
    message(WARNING "MSan may not be available on your system.")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pie")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-memory-track-origins")
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


####################################################################################
#                                    Definitions
####################################################################################
add_definitions(-DUSTREAM_H) # -DUSE_ICU_STRINGS

if( (NOT DEFINED DCMA_VERSION) OR ("${DCMA_VERSION}" MATCHES "^$") )
    execute_process( COMMAND
        ./scripts/extract_dcma_version.sh
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE DCMA_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if( "${DCMA_VERSION}" MATCHES "^$" )
    message(WARNING "Please supply a meaningful DCMA_VERSION when invoking CMake.")
    set(DCMA_VERSION "unknown")
endif()

if(WITH_EIGEN)
    message(STATUS "Assuming Eigen is available.")
    add_definitions(-DDCMA_USE_EIGEN=1)
else()
    message(STATUS "Assuming Eigen is not available.")
    add_definitions(-UDCMA_USE_EIGEN)
endif()

if(WITH_CGAL)
    message(STATUS "Assuming CGAL is available.")
    add_definitions(-DDCMA_USE_CGAL=1)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_definitions(-DCGAL_DISABLE_ROUNDING_MATH_CHECK=1) # May be needed for Valgrind debugging.
        message(WARNING "If profiling/debugging on Arch Linux, disable binary stripping in PKGBUILD.")
    endif()
    if(WITH_EIGEN)
        add_definitions(-DCGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.
    endif()
else()
    message(STATUS "Assuming CGAL is not available.")
    add_definitions(-UDCMA_USE_CGAL)
endif()

if(WITH_NLOPT)
    message(STATUS "Assuming NLOPT is available.")
    add_definitions(-DDCMA_USE_NLOPT=1)
else()
    message(STATUS "Assuming NLOPT is not available.")
    add_definitions(-UDCMA_USE_NLOPT)
endif()

if(WITH_SFML)
    message(STATUS "Assuming SFML is available.")
    add_definitions(-DDCMA_USE_SFML=1)
else()
    message(STATUS "Assuming SFML is not available.")
    add_definitions(-UDCMA_USE_SFML)
endif()

if(WITH_SDL)
    message(STATUS "Assuming SDL is available.")
    add_definitions(-DDCMA_USE_SDL=1)
    add_definitions(-DDCMA_USE_GLEW=1)
else()
    message(STATUS "Assuming SDL is not available.")
    add_definitions(-UDCMA_USE_SDL)
    add_definitions(-UDCMA_USE_GLEW)
endif()

if(WITH_WT)
    message(STATUS "Assuming Wt is available.")
    add_definitions(-DDCMA_USE_WT=1)
else()
    message(STATUS "Assuming Wt is not available.")
    add_definitions(-UDCMA_USE_WT)
endif()

if(WITH_POSTGRES)
    message(STATUS "Assuming PostgreSQL client libraries are available.")
    add_definitions(-DDCMA_USE_POSTGRES=1)
else()
    message(STATUS "Assuming PostgreSQL client libraries are not available.")
    add_definitions(-UDCMA_USE_POSTGRES)
endif()

if(WITH_JANSSON)
    message(STATUS "Assuming Jansson is available.")
    add_definitions(-DDCMA_USE_JANSSON=1)
else()
    message(STATUS "Assuming Jansson is not available.")
    add_definitions(-UDCMA_USE_JANSSON)
endif()

if(WITH_GNU_GSL)
    message(STATUS "Assuming the GNU GSL is available.")
    add_definitions(-DDCMA_USE_GNU_GSL=1)
else()
    message(STATUS "Assuming the GNU GSL is not available.")
    add_definitions(-UDCMA_USE_GNU_GSL)
endif()

# ASIO: target Windows 7 features.
add_definitions(-D_WIN32_WINNT=0x0601)

# Use the directory where CMakeLists.txt is for inclusions.
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
# The following should match the system directory:
set(BASH_COMPLETION_INSTALL_DIR   "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions/")


####################################################################################
#                                 Subdirectories 
####################################################################################

add_subdirectory(src)
add_subdirectory(config-files)
add_subdirectory(scripts)


####################################################################################
#                                    Packaging    
####################################################################################

set(CPACK_GENERATOR "DEB")
#set(CPACK_PACKAGE_NAME "dicomautomaton")

string(TIMESTAMP INVOCATION_TIMESTAMP "%Y%m%d.%H%M%S") # For a time-based version number.
set(CPACK_PACKAGE_VERSION "${INVOCATION_TIMESTAMP}")
set(CPACK_STRIP_FILES TRUE)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64") # i386, amd64, armel, armhf, ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Various tools for medical physics applications.")
set(CPACK_PACKAGE_CONTACT "hdeanclark@gmail.com")
set(CPACK_PACKAGE_MAINTAINER "Haley Clark <hdeanclark@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "Science")


# Ygor build dependencies, e.g., "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)"
set(BUILD_DEPENDENCY_PACKAGES "")
list(APPEND BUILD_DEPENDENCY_PACKAGES "explicator")
list(APPEND BUILD_DEPENDENCY_PACKAGES "ygorclustering")
list(APPEND BUILD_DEPENDENCY_PACKAGES "ygor")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libz-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-filesystem-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-iostreams-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-program-options-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libboost-thread-dev")
list(APPEND BUILD_DEPENDENCY_PACKAGES "libasio-dev")
if(WITH_EIGEN)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libeigen3-dev")
endif()
if(WITH_SFML)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libsfml-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "fonts-freefont-ttf")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "fonts-cmu")
endif()
if(WITH_SDL)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libsdl2-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libglew-dev")
endif()
if(WITH_CGAL)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libcgal-dev")
endif()
if(WITH_WT)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libwt-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libwthttp-dev")
endif()
if(WITH_NLOPT)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libnlopt-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libnlopt-cxx-dev")
endif()
if(WITH_GNU_GSL)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libgsl-dev")
endif()
if(WITH_POSTGRES)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libpqxx-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libpq-dev")
    list(APPEND BUILD_DEPENDENCY_PACKAGES "postgresql-client")
endif()
if(WITH_JANSSON)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libjansson-dev")
endif()
list(JOIN BUILD_DEPENDENCY_PACKAGES ", " CPACK_DEBIAN_PACKAGE_DEPENDS)

# Recommended or optional packages, e.g., "liboptional-dev (>= 1.2.3-1), libmaybe-dev (>= 1:1.3.2-10)"
set(RECOMMENDED_PACKAGES "")
list(APPEND RECOMMENDED_PACKAGES "gnuplot")
list(APPEND RECOMMENDED_PACKAGES "zenity")
list(APPEND RECOMMENDED_PACKAGES "libboost-all-dev")
list(APPEND RECOMMENDED_PACKAGES "patchelf")
list(JOIN RECOMMENDED_PACKAGES ", " CPACK_DEBIAN_PACKAGE_RECOMMENDS)

include(CPack)


