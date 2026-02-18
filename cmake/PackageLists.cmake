# PackageLists.cmake
#
# Centralized list of dependency package names for various package managers.
# This file is the single source of truth for dependency package names.
# It is included by the top-level CMakeLists.txt and also consulted by
# external tooling (packaging scripts, CI, etc.).
#
# Note: This file only lists packages; it does NOT call find_package() or
# otherwise modify the CMake build. It merely sets list variables that other
# parts of the build can use (e.g., CPack, packaging scripts).

####################################################################################
#                         Debian/Ubuntu Build Dependencies
####################################################################################

set(DCMA_DEBIAN_BUILD_DEPS
    # Core build tools
    cmake
    make
    g++
    git
    # Core runtime/link dependencies (always required)
    libz-dev
    libboost-dev
    libboost-iostreams-dev
    libboost-program-options-dev
    libboost-thread-dev
    libasio-dev
    # Author's support libraries (must be built/installed separately)
    ygor
    ygorclustering
    explicator
)

set(DCMA_DEBIAN_OPTIONAL_DEPS "")

if(WITH_EIGEN OR NOT DEFINED WITH_EIGEN)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libeigen3-dev)
endif()

if(WITH_CGAL OR NOT DEFINED WITH_CGAL)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libcgal-dev)
endif()

if(WITH_NLOPT OR NOT DEFINED WITH_NLOPT)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libnlopt-dev libnlopt-cxx-dev)
endif()

if(WITH_SFML OR NOT DEFINED WITH_SFML)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libsfml-dev)
    list(APPEND DCMA_DEBIAN_OPTIONAL_DEPS fonts-freefont-ttf fonts-cmu)
endif()

if(WITH_SDL OR NOT DEFINED WITH_SDL)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libsdl2-dev libglew-dev)
endif()

if(WITH_WT OR NOT DEFINED WITH_WT)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libwt-dev libwthttp-dev)
endif()

if(WITH_GNU_GSL OR NOT DEFINED WITH_GNU_GSL)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libgsl-dev)
endif()

if(WITH_POSTGRES OR NOT DEFINED WITH_POSTGRES)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libpqxx-dev libpq-dev postgresql-client)
endif()

if(WITH_JANSSON OR NOT DEFINED WITH_JANSSON)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libjansson-dev)
endif()

if(WITH_THRIFT OR NOT DEFINED WITH_THRIFT)
    list(APPEND DCMA_DEBIAN_BUILD_DEPS libthrift-dev)
endif()

set(DCMA_DEBIAN_RECOMMENDED_DEPS
    gnuplot
    zenity
    libboost-all-dev
    patchelf
    ${DCMA_DEBIAN_OPTIONAL_DEPS}
)


####################################################################################
#                         Arch Linux Build Dependencies
####################################################################################

set(DCMA_ARCH_BUILD_DEPS
    # Core build tools
    cmake
    make
    gcc
    git
    base-devel
    # Core runtime/link dependencies (always required)
    zlib
    boost-libs
    boost
    asio
    # Author's support libraries (must be built/installed separately or via AUR)
    ygor
    ygorclustering
    explicator
)

set(DCMA_ARCH_OPTIONAL_DEPS "")

if(WITH_EIGEN OR NOT DEFINED WITH_EIGEN)
    list(APPEND DCMA_ARCH_BUILD_DEPS eigen)
endif()

if(WITH_CGAL OR NOT DEFINED WITH_CGAL)
    list(APPEND DCMA_ARCH_BUILD_DEPS cgal)
endif()

if(WITH_NLOPT OR NOT DEFINED WITH_NLOPT)
    list(APPEND DCMA_ARCH_BUILD_DEPS nlopt)
endif()

if(WITH_SFML OR NOT DEFINED WITH_SFML)
    list(APPEND DCMA_ARCH_BUILD_DEPS sfml)
    list(APPEND DCMA_ARCH_OPTIONAL_DEPS gnu-free-fonts ttf-computer-modern-fonts)
endif()

if(WITH_SDL OR NOT DEFINED WITH_SDL)
    list(APPEND DCMA_ARCH_BUILD_DEPS sdl2 glew glu)
endif()

if(WITH_WT OR NOT DEFINED WITH_WT)
    list(APPEND DCMA_ARCH_BUILD_DEPS wt)
endif()

if(WITH_GNU_GSL OR NOT DEFINED WITH_GNU_GSL)
    list(APPEND DCMA_ARCH_BUILD_DEPS gsl)
endif()

if(WITH_POSTGRES OR NOT DEFINED WITH_POSTGRES)
    list(APPEND DCMA_ARCH_BUILD_DEPS libpqxx postgresql)
endif()

if(WITH_JANSSON OR NOT DEFINED WITH_JANSSON)
    list(APPEND DCMA_ARCH_BUILD_DEPS jansson)
endif()

if(WITH_THRIFT OR NOT DEFINED WITH_THRIFT)
    list(APPEND DCMA_ARCH_BUILD_DEPS thrift)
endif()

set(DCMA_ARCH_OPTIONAL_DEPS
    ${DCMA_ARCH_OPTIONAL_DEPS}
    gnuplot
    zenity
    patchelf
    bash-completion
    adaptivecpp
    libnotify
)

