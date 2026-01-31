# DCMAPlatformChecks.cmake
# Modern CMake module for platform and feature detection
#
# This module handles detection of platform-specific features and capabilities.

include_guard(GLOBAL)

include(CheckCXXSymbolExists)
include(CheckIncludeFileCXX)

# Detect and configure ARM architecture settings
function(dcma_detect_arm_architecture)
    check_cxx_symbol_exists(__arm__     "cstdio" DCMA_ARCH_IS_ARM)
    check_cxx_symbol_exists(__aarch64__ "cstdio" DCMA_ARCH_IS_ARM64)

    if(DCMA_ARCH_IS_ARM OR DCMA_ARCH_IS_ARM64)
        message(STATUS "Detected ARM architecture.")
        
        # Check if user already specified architecture flags
        if(CMAKE_CXX_FLAGS MATCHES "-march=|-mcpu=|-mtune=")
            message(STATUS "Architecture set by user.")
        else()
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                message(STATUS "No architecture set, adding march=native flag")
                add_compile_options(-march=native)
            elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
                message(STATUS "No architecture set, adding mcpu=generic flag")
                add_compile_options(-mcpu=generic)
            else()
                message(WARNING "Not able to set architecture, if compilation errors occur set architecture manually")
            endif()
        endif()
    endif()

    # Export detection results
    set(DCMA_ARCH_IS_ARM ${DCMA_ARCH_IS_ARM} PARENT_SCOPE)
    set(DCMA_ARCH_IS_ARM64 ${DCMA_ARCH_IS_ARM64} PARENT_SCOPE)
endfunction()

# Check for C++ standard library features
function(dcma_check_cppstdlib_features)
    # Check for regex multiline support
    check_cxx_symbol_exists(std::regex::multiline "regex" DCMA_HAS_REGEX_MULTILINE)
    if(DCMA_HAS_REGEX_MULTILINE)
        add_compile_definitions(DCMA_CPPSTDLIB_HAS_REGEX_MULTILINE=1)
    endif()

    # Check for quick_exit (missing from macos libc)
    check_cxx_symbol_exists(std::quick_exit "cstdlib" DCMA_HAS_QUICK_EXIT)
    if(DCMA_HAS_QUICK_EXIT)
        add_compile_definitions(DCMA_CPPSTDLIB_HAS_QUICK_EXIT=1)
    endif()
endfunction()

# Check for system header features (for DCMA_Definitions.h.in)
function(dcma_check_system_features)
    # sys/select.h
    check_cxx_symbol_exists(select    "sys/select.h" DCMA_HAS_SYS_SELECT)

    # unistd.h
    check_cxx_symbol_exists(isatty    "unistd.h"     DCMA_HAS_UNISTD_ISATTY)

    # cstdio
    check_cxx_symbol_exists(fileno    "cstdio"       DCMA_HAS_CSTDIO_FILENO)

    # termios.h
    check_cxx_symbol_exists(ICANON    "termios.h"    DCMA_HAS_TERMIOS_ICANON)
    check_cxx_symbol_exists(ECHO      "termios.h"    DCMA_HAS_TERMIOS_ECHO)
    check_cxx_symbol_exists(VMIN      "termios.h"    DCMA_HAS_TERMIOS_VMIN)
    check_cxx_symbol_exists(VTIME     "termios.h"    DCMA_HAS_TERMIOS_VTIME)
    check_cxx_symbol_exists(TCSANOW   "termios.h"    DCMA_HAS_TERMIOS_TCSANOW)
    check_cxx_symbol_exists(tcgetattr "termios.h"    DCMA_HAS_TERMIOS_TCGETADDR)

    # fcntl.h
    check_cxx_symbol_exists(fcntl      "fcntl.h"     DCMA_HAS_FCNTL_FCNTL)
    check_cxx_symbol_exists(F_GETFL    "fcntl.h"     DCMA_HAS_FCNTL_F_GETFL)
    check_cxx_symbol_exists(O_NONBLOCK "fcntl.h"     DCMA_HAS_FCNTL_O_NONBLOCK)

    # Export all results to parent scope
    set(DCMA_HAS_SYS_SELECT ${DCMA_HAS_SYS_SELECT} PARENT_SCOPE)
    set(DCMA_HAS_UNISTD_ISATTY ${DCMA_HAS_UNISTD_ISATTY} PARENT_SCOPE)
    set(DCMA_HAS_CSTDIO_FILENO ${DCMA_HAS_CSTDIO_FILENO} PARENT_SCOPE)
    set(DCMA_HAS_TERMIOS_ICANON ${DCMA_HAS_TERMIOS_ICANON} PARENT_SCOPE)
    set(DCMA_HAS_TERMIOS_ECHO ${DCMA_HAS_TERMIOS_ECHO} PARENT_SCOPE)
    set(DCMA_HAS_TERMIOS_VMIN ${DCMA_HAS_TERMIOS_VMIN} PARENT_SCOPE)
    set(DCMA_HAS_TERMIOS_VTIME ${DCMA_HAS_TERMIOS_VTIME} PARENT_SCOPE)
    set(DCMA_HAS_TERMIOS_TCSANOW ${DCMA_HAS_TERMIOS_TCSANOW} PARENT_SCOPE)
    set(DCMA_HAS_TERMIOS_TCGETADDR ${DCMA_HAS_TERMIOS_TCGETADDR} PARENT_SCOPE)
    set(DCMA_HAS_FCNTL_FCNTL ${DCMA_HAS_FCNTL_FCNTL} PARENT_SCOPE)
    set(DCMA_HAS_FCNTL_F_GETFL ${DCMA_HAS_FCNTL_F_GETFL} PARENT_SCOPE)
    set(DCMA_HAS_FCNTL_O_NONBLOCK ${DCMA_HAS_FCNTL_O_NONBLOCK} PARENT_SCOPE)
endfunction()

# CGAL version compatibility checks
function(dcma_check_cgal_compatibility)
    if(NOT DCMA_WITH_CGAL)
        return()
    endif()

    # Workaround for CGAL v4.13 class and file rename
    check_include_file_cxx("CGAL/Implicit_mesh_domain_3.h" DCMA_CGAL_HAS_IMPLICIT_MESH_DOMAIN_3_HEADER)
    if(NOT DCMA_CGAL_HAS_IMPLICIT_MESH_DOMAIN_3_HEADER)
        add_compile_definitions(DCMA_CGAL_HAS_LABELED_MESH_DOMAIN_3_HEADER=1)
    endif()
endfunction()

# Configure for MinGW/Windows builds
function(dcma_configure_windows_platform)
    if(NOT (MINGW OR WIN32))
        return()
    endif()

    # Target Windows 7 features (needed for ASIO and other libs)
    add_compile_definitions(_WIN32_WINNT=0x0601)

    # Create 'console' applications so a terminal appears when running
    add_link_options(-mconsole)

    # Override IFileDialogs check to disable
    # See https://github.com/samhocevar/portable-file-dialogs/issues/50
    add_compile_definitions(PFD_HAS_IFILEDIALOG=0)
endfunction()

# Run all platform checks
function(dcma_run_all_platform_checks)
    dcma_detect_arm_architecture()
    dcma_check_cppstdlib_features()
    dcma_check_system_features()
    dcma_configure_windows_platform()
endfunction()
