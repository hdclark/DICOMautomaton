
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
option(WITH_THRIFT    "Compile assuming Apache Thrift is available."            ON)

option(WITH_EXT_SYCL  "Compile with external SYCL support via AdaptiveCpp."     OFF)

option(WITH_LTO       "Use link-time optimization when available."              OFF)

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

if(NOT CMAKE_BUILD_TYPE)
    # Default to debug builds.
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "default to debug" FORCE)
endif()

if(WITH_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_avail OUTPUT lto_msg)
    if(lto_avail)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO was requested, but is not supported: ${lto_msg}")
    endif()
endif()


####################################################################################
#                                  Dependencies 
####################################################################################
# Note: Dependencies are listed in CPACK list below.

find_package(Threads REQUIRED)

set(Boost_DEBUG ON)
if(NOT BUILD_SHARED_LIBS)
    #set(Boost_USE_STATIC_RUNTIME ON)
    set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost COMPONENTS serialization iostreams thread
                   OPTIONAL_COMPONENTS system)
if(NOT Boost_FOUND)
    message(STATUS "find_package(Boost CONFIG) not found, attempting non-config method instead")
    find_package(Boost REQUIRED COMPONENTS serialization iostreams thread
                                OPTIONAL_COMPONENTS system)
endif()
include_directories(${Boost_INCLUDE_DIRS})

if(WITH_EIGEN)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EIGEN3 REQUIRED eigen3)
    include_directories( ${EIGEN3_INCLUDE_DIRS} )
endif()

if(WITH_CGAL)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(CGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE TRUE)
        add_definitions(-DCGAL_DISABLE_ROUNDING_MATH_CHECK=1) # May be needed for Valgrind debugging.
        message(WARNING "If profiling/debugging on Arch Linux, disable binary stripping in PKGBUILD.")
    endif()
    if(WITH_EIGEN)
        add_definitions(-DCGAL_EIGEN3_ENABLED=1) # Explicitly instruct CGAL to use Eigen.
    endif()
    find_package(CGAL REQUIRED COMPONENTS Core)

    # This workaround is for a CGAL v4.13 class and file rename {Implicit,Labeled}_mesh_domain_3.h.
    include(CheckIncludeFileCXX)
    # Opt into the newer Labeled_mesh_domain_3.h when available.
    #check_include_file_cxx("CGAL/Labeled_mesh_domain_3.h" DCMA_CGAL_HAS_LABELED_MESH_DOMAIN_3_HEADER)
    #if(DCMA_CGAL_HAS_LABELED_MESH_DOMAIN_3_HEADER)
    #    add_definitions(-DDCMA_CGAL_HAS_LABELED_MESH_DOMAIN_3_HEADER=1)
    #endif()
    #
    # Alternatively, prefer to use the older Implicit_mesh_domain_3.h when available.
    check_include_file_cxx("CGAL/Implicit_mesh_domain_3.h" DCMA_CGAL_HAS_IMPLICIT_MESH_DOMAIN_3_HEADER)
    if(NOT DCMA_CGAL_HAS_IMPLICIT_MESH_DOMAIN_3_HEADER)
        add_definitions(-DDCMA_CGAL_HAS_LABELED_MESH_DOMAIN_3_HEADER=1)
    endif()
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
    include_directories( ${OPENGL_INCLUDE_DIR} )
    message(STATUS "Proceeding with OPENGL_LIBRARIES = '${OPENGL_LIBRARIES}'")
    message(STATUS "Proceeding with OPENGL_INCLUDE_DIR = '${OPENGL_INCLUDE_DIR}'")
    message(STATUS "Proceeding with OPENGL_CFLAGS = '${OPENGL_CFLAGS}'")

    find_package(PkgConfig REQUIRED)
    find_package(SDL2)
    if(NOT SDL2_FOUND)
        message(STATUS "find_package(SDL2) not found, attempting pkg-config instead")
        pkg_check_modules(SDL2 REQUIRED sdl2)
    endif()
    include_directories( ${SDL2_INCLUDE_DIRS} )
    message(STATUS "Proceeding with SDL2_LIBRARIES = '${SDL2_LIBRARIES}'")
    message(STATUS "Proceeding with SDL2_INCLUDE_DIRS = '${SDL2_INCLUDE_DIRS}'")
    message(STATUS "Proceeding with SDL2_CFLAGS = '${SDL2_CFLAGS}'")
    if(NOT TARGET SDL2::SDL2)
        message(STATUS "Simulating missing 'SDL2::SDL2' target")
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        target_link_libraries(SDL2::SDL2 INTERFACE "${SDL2_LIBRARIES}")
    endif()

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

if(WITH_THRIFT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(THRIFT REQUIRED thrift)
    include_directories( ${THRIFT_INCLUDE_DIRS} )
endif()

if(WITH_EXT_SYCL)
    # SYCL support via AdaptiveCpp (formerly hipSYCL).
    # AdaptiveCpp provides the acpp compiler wrapper and CMake integration.
    # See https://github.com/AdaptiveCpp/AdaptiveCpp for installation instructions.
    
    # Set default ACPP_TARGETS if not already specified by user.
    if(NOT DEFINED ACPP_TARGETS)
        set(ACPP_TARGETS "omp.library-only" CACHE STRING "AdaptiveCpp backend target")
        message(STATUS "ACPP_TARGETS not specified; defaulting to 'omp.library-only' for maximum portability.")
    endif()
    
    find_package(AdaptiveCpp CONFIG)
    if(NOT AdaptiveCpp_FOUND)
        # Fall back to checking for hipSYCL (older name)
        find_package(hipSYCL CONFIG)
        if(NOT hipSYCL_FOUND)
            message(WARNING "WITH_EXT_SYCL enabled but AdaptiveCpp/hipSYCL not found. Disabling external SYCL support.")
            set(WITH_EXT_SYCL OFF)
        endif()
    endif()
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
        set(CMAKE_CXX_FLAGS "-Os -DNDEBUG")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG           "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE         "-Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-Os -DNDEBUG")
endif()


# Add other appropriate CXX flags.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options( -fdiagnostics-color=always
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
        add_compile_options( -pg
                             -fno-omit-frame-pointer
                             -fstack-check )
    endif()

    if(MEMORY_CONSTRAINED_BUILD)
        # Trigger garbage collection more frequently.
        add_compile_options( "SHELL:--param ggc-min-expand=10"
                             "SHELL:--param ggc-min-heapsize=32768" )
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options( -fdiagnostics-color=always
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


# Sanitizers.
if(WITH_ASAN  OR  WITH_TSAN  OR  WITH_MSAN)
    add_compile_options( -O2
                         -g
                         -fno-omit-frame-pointer
                         -fno-common )

    # Also enable coverage instrumentation, since sanitizers will typically be used for testing.
    add_compile_options( --coverage
                         # Clang only? Need to confirm ... TODO
                         #-fsanitize-coverage=trace-pc-guard
                         #-fprofile-instr-generate
                         #-fcoverage-mapping
    )
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        link_libraries(gcov)
    endif()

    add_definitions(-U_FORTIFY_SOURCE)
endif()
if(WITH_ASAN)
    add_compile_options( -fsanitize=address
                         -fsanitize-address-use-after-scope )
    add_link_options(-fsanitize=address)

    add_compile_options( -fsanitize=undefined
                         -fno-sanitize-recover=undefined )
    add_link_options(-fsanitize=undefined)

elseif(WITH_TSAN)
    message(WARNING "TSan may not support exceptions (depends on the compiler version and platform).")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
elseif(WITH_MSAN)
    message(WARNING "MSan may not be available on your system.")
    add_compile_options( -fsanitize=memory
                         -fPIE
                         -pie
                         -fsanitize-memory-track-origins )
    add_link_options(-fsanitize=memory)
endif()



include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__arm__     "cstdio" ARCH_IS_ARM)
check_cxx_symbol_exists(__aarch64__ "cstdio" ARCH_IS_ARM64)
if(ARCH_IS_ARM OR ARCH_IS_ARM64)
    message(STATUS "Detected ARM architecture.")
    if(CMAKE_CXX_FLAGS MATCHES "-march=|-mcpu=|-mtune=")
        message(STATUS "Architecture set by user.")
    else()
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            # Enable to fix linking errors for toolchains that do not auto-detect atomic intrinsics (e.g., some ARM systems).
            # Note: Binaries built this way should not be distributed.
            message(STATUS "No architecture set, adding march=native flag")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
        elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            message(STATUS "No architecture set, adding mcpu=generic flag")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=generic")
        else()
            message(WARNING "Not able to set architecture, if compilation errors occur set architecture manually")
        endif()
    endif()
endif()

check_cxx_symbol_exists(std::regex::multiline "regex" HAS_REGEX_MULTILINE)
if(HAS_REGEX_MULTILINE)
    add_definitions(-DDCMA_CPPSTDLIB_HAS_REGEX_MULTILINE=1)
endif()

# C++11 feature currently missing from macos libc.
check_cxx_symbol_exists(std::quick_exit "cstdlib" HAS_QUICK_EXIT)
if(HAS_QUICK_EXIT)
    add_definitions(-DDCMA_CPPSTDLIB_HAS_QUICK_EXIT=1)
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

if(WITH_THRIFT)
    message(STATUS "Assuming Apache Thrift is available.")
    add_definitions(-DDCMA_USE_THRIFT=1)
else()
    message(STATUS "Assuming Apache Thrift is not available.")
    add_definitions(-UDCMA_USE_THRIFT)
endif()

if(WITH_EXT_SYCL)
    message(STATUS "External SYCL support enabled via AdaptiveCpp.")
    add_definitions(-DDCMA_USE_EXT_SYCL=1)
else()
    message(STATUS "External SYCL support is not available.")
    add_definitions(-UDCMA_USE_EXT_SYCL)
endif()

# Note: DCMA_USE_SYCL is always defined when Mock_SYCL.h is available as a fallback.
# This enables SYCL code paths to work even without external SYCL support.
add_definitions(-DDCMA_USE_SYCL=1)

# Detect whether specific functions/variable/macros are available.
# Note: this method does not support structs. Best to find a function that accepts the struct instead.
check_cxx_symbol_exists(select    "sys/select.h" DCMA_HAS_SYS_SELECT)        # Function (nominally).

check_cxx_symbol_exists(isatty    "unistd.h"     DCMA_HAS_UNISTD_ISATTY)     # Function (nominally).

check_cxx_symbol_exists(fileno    "cstdio"       DCMA_HAS_CSTDIO_FILENO)     # Function (nominally).

check_cxx_symbol_exists(ICANON    "termios.h"    DCMA_HAS_TERMIOS_ICANON)    # Macro.
check_cxx_symbol_exists(ECHO      "termios.h"    DCMA_HAS_TERMIOS_ECHO)      # Macro.
check_cxx_symbol_exists(VMIN      "termios.h"    DCMA_HAS_TERMIOS_VMIN)      # Macro.
check_cxx_symbol_exists(VTIME     "termios.h"    DCMA_HAS_TERMIOS_VTIME)     # Macro.
check_cxx_symbol_exists(TCSANOW   "termios.h"    DCMA_HAS_TERMIOS_TCSANOW)   # Macro.
check_cxx_symbol_exists(tcgetattr "termios.h"    DCMA_HAS_TERMIOS_TCGETADDR) # Function.

check_cxx_symbol_exists(fcntl      "fcntl.h"     DCMA_HAS_FCNTL_FCNTL)       # Function (nominally).
check_cxx_symbol_exists(F_GETFL    "fcntl.h"     DCMA_HAS_FCNTL_F_GETFL)     # Macro.
check_cxx_symbol_exists(O_NONBLOCK "fcntl.h"     DCMA_HAS_FCNTL_O_NONBLOCK)  # Macro.

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/src/DCMA_Definitions.h.in
                ${CMAKE_CURRENT_BINARY_DIR}/src/DCMA_Definitions.h )

# mingw-w64 tweaks.
if(MINGW OR WIN32)
    # Target Windows 7 features (need to specify for ASIO, maybe other libs).
    add_definitions(-D_WIN32_WINNT=0x0601)
    # Create 'console' applications, so a terminal will appear when running.
    link_libraries("-mconsole")
    # Override IFileDialogs check to disable.
    # See https://github.com/samhocevar/portable-file-dialogs/issues/50
    add_definitions(-DPFD_HAS_IFILEDIALOG=0)
endif()

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

#set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64") # i386, amd64, armel, armhf, ...
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
if(WITH_THRIFT)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "libthrift-dev")
endif()
if(WITH_EXT_SYCL)
    list(APPEND BUILD_DEPENDENCY_PACKAGES "adaptivecpp")
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


