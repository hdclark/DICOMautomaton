# DCMACompilerOptions.cmake
# Modern CMake module for compiler options and flags
#
# This module provides functions to set up compiler options in a target-based way.

include_guard(GLOBAL)

# Function to apply common compiler options to a target
function(dcma_apply_compiler_options target)
    # C++17 standard requirement
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # Position independent code
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

    # Per-compiler options
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE
            -fdiagnostics-color=always
            -fno-var-tracking-assignments
            -Wno-deprecated
            -Wall
            -Wextra
            -Wpedantic
            -Wdeprecated
            -Wno-unused-variable
            -Wno-unused-parameter
            -Wno-unused-but-set-variable
            -Wno-maybe-initialized
        )

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE
                -pg
                -fno-omit-frame-pointer
                -fstack-check
            )
        endif()

        if(DCMA_MEMORY_CONSTRAINED_BUILD)
            target_compile_options(${target} PRIVATE
                "SHELL:--param ggc-min-expand=10"
                "SHELL:--param ggc-min-heapsize=32768"
            )
        endif()

    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PRIVATE
            -fdiagnostics-color=always
            -Wall
            -Wextra
            -Wpedantic
            -Wdeprecated
            -Wno-unused-lambda-capture
            -Wno-unused-variable
            -Wno-unused-parameter
            -Wno-reserved-identifier
        )
    endif()
endfunction()

# Function to apply sanitizer options to a target
function(dcma_apply_sanitizers target)
    if(NOT (DCMA_WITH_ASAN OR DCMA_WITH_TSAN OR DCMA_WITH_MSAN))
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -O2
        -g
        -fno-omit-frame-pointer
        -fno-common
    )

    # Enable coverage instrumentation when sanitizers are active
    target_compile_options(${target} PRIVATE --coverage)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(${target} PRIVATE gcov)
    endif()

    target_compile_definitions(${target} PRIVATE -U_FORTIFY_SOURCE)

    if(DCMA_WITH_ASAN)
        target_compile_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize-address-use-after-scope
            -fsanitize=undefined
            -fno-sanitize-recover=undefined
        )
        target_link_options(${target} PRIVATE
            -fsanitize=address
            -fsanitize=undefined
        )

    elseif(DCMA_WITH_TSAN)
        message(WARNING "TSan may not support exceptions (depends on compiler version and platform).")
        target_compile_options(${target} PRIVATE -fsanitize=thread)
        target_link_options(${target} PRIVATE -fsanitize=thread)

    elseif(DCMA_WITH_MSAN)
        message(WARNING "MSan may not be available on your system.")
        target_compile_options(${target} PRIVATE
            -fsanitize=memory
            -fPIE
            -pie
            -fsanitize-memory-track-origins
        )
        target_link_options(${target} PRIVATE -fsanitize=memory)
    endif()
endfunction()

# Function to apply memory-constrained build options
function(dcma_apply_memory_constrained_options target)
    if(NOT DCMA_MEMORY_CONSTRAINED_BUILD)
        return()
    endif()

    target_compile_options(${target} PRIVATE -Os)
    target_compile_definitions(${target} PRIVATE NDEBUG)
endfunction()

# Set up global build type flags (called once at configuration time)
function(dcma_configure_build_types)
    # Override default CXX flags for each build type
    set(CMAKE_CXX_FLAGS_DEBUG           "-O2 -g" CACHE STRING "Debug flags" FORCE)
    set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG" CACHE STRING "MinSizeRel flags" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE         "-O3 -DNDEBUG" CACHE STRING "Release flags" FORCE)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-O3 -g" CACHE STRING "RelWithDebInfo flags" FORCE)

    if(DCMA_MEMORY_CONSTRAINED_BUILD)
        set(CMAKE_CXX_FLAGS_DEBUG           "-Os -DNDEBUG" CACHE STRING "Debug flags" FORCE)
        set(CMAKE_CXX_FLAGS_MINSIZEREL      "-Os -DNDEBUG" CACHE STRING "MinSizeRel flags" FORCE)
        set(CMAKE_CXX_FLAGS_RELEASE         "-Os -DNDEBUG" CACHE STRING "Release flags" FORCE)
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-Os -DNDEBUG" CACHE STRING "RelWithDebInfo flags" FORCE)
    endif()
endfunction()

# Set up IWYU if requested
function(dcma_configure_iwyu)
    if(NOT DCMA_WITH_IWYU)
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(WARNING "IWYU requires Clang compiler")
        return()
    endif()

    set(IWYU_INVOCATION iwyu)
    list(APPEND IWYU_INVOCATION "-Xiwyu;--no_comments")
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_INVOCATION} PARENT_SCOPE)
endfunction()
