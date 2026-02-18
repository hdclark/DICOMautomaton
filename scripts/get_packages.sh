#!/usr/bin/env bash

# get_packages.sh
#
# Centralized script for enumerating external packages needed to build DICOMautomaton
# across different platforms and distributions.
#
# This script is the single source of truth for dependency package names. It is designed
# to be sourced or invoked by Docker implementation files and build scripts.
#
# Usage:
#   ./get_packages.sh [options]
#
# Options:
#   --os <os>           Operating system/distribution (required)
#                       Values: alpine, arch, arch_sycl, debian_buster, debian_bookworm,
#                               debian_bullseye, debian_stretch, macos, mxe, ubuntu, void
#   --arch <arch>       CPU architecture (optional, default: x86_64)
#                       Values: x86_64, aarch64, armv7l, etc.
#   --tier <tier>       Installation tier (optional, can be repeated)
#                       Values: build_tools, extra_toolchains, ygor_deps, dcma_deps,
#                               headless_rendering, optional, external_third_party
#   --required-only     Only list required packages (no optional dependencies)
#   --optional-only     Only list optional packages
#   --list              Output packages as a space-separated list (default)
#   --newline           Output packages as newline-separated list
#   --help              Show this help message
#
# Examples:
#   # Get all packages for Debian Bookworm
#   ./get_packages.sh --os debian_bookworm
#
#   # Get only build tools for Alpine
#   ./get_packages.sh --os alpine --tier build_tools
#
#   # Get multiple tiers
#   ./get_packages.sh --os arch --tier build_tools --tier ygor_deps
#
#   # Get only required packages for Ubuntu
#   ./get_packages.sh --os ubuntu --required-only
#

set -euo pipefail

# --- Default values ---
OS=""
ARCH="x86_64"
TIERS=()
REQUIRED_ONLY=false
OPTIONAL_ONLY=false
OUTPUT_FORMAT="list"  # "list" or "newline"

# --- Helper functions ---
show_help() {
    head -n 50 "$0" | grep -E '^#' | sed 's/^# \?//'
    exit 0
}

error_exit() {
    echo "Error: $1" >&2
    exit 1
}

# --- Parse arguments ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --os)
            if [[ $# -lt 2 ]]; then
                error_exit "--os requires an argument. See --help for usage."
            fi
            OS="$2"
            shift 2
            ;;
        --arch)
            if [[ $# -lt 2 ]]; then
                error_exit "--arch requires an argument. See --help for usage."
            fi
            ARCH="$2"
            shift 2
            ;;
        --tier)
            if [[ $# -lt 2 ]]; then
                error_exit "--tier requires an argument. See --help for usage."
            fi
            TIERS+=("$2")
            shift 2
            ;;
        --required-only)
            REQUIRED_ONLY=true
            shift
            ;;
        --optional-only)
            OPTIONAL_ONLY=true
            shift
            ;;
        --list)
            OUTPUT_FORMAT="list"
            shift
            ;;
        --newline)
            OUTPUT_FORMAT="newline"
            shift
            ;;
        --help|-h)
            show_help
            ;;
        *)
            error_exit "Unknown option: $1"
            ;;
    esac
done

# --- Validate required arguments ---
if [[ -z "$OS" ]]; then
    error_exit "Missing required argument: --os"
fi

# --- Package definitions ---
# Each function outputs space-separated package names.
# The naming convention is: get_<os>_<tier>_<required|optional>

####################################################################################
#                               Alpine Linux
####################################################################################
get_alpine_build_tools_required() {
    echo "alpine-sdk bash git cmake vim gdb rsync openssh wget unzip"
}

get_alpine_extra_toolchains_required() {
    echo "clang clang-headers clang-libs clang-extra-tools llvm libc++ lld musl-dev compiler-rt"
}

get_alpine_ygor_deps_required() {
    echo "gsl-static gsl-dev eigen-dev"
}

get_alpine_dcma_deps_required() {
    echo "openssl-libs-static zlib-static zlib-dev sfml-dev sdl2-dev glew-dev jansson-dev patchelf"
}

get_alpine_headless_rendering_required() {
    echo "libx11-dev libx11-static glu-dev glu mesa mesa-dev xorg-server-dev xf86-video-dummy"
}

get_alpine_optional_required() {
    echo "libnotify-dev dunst"
}

####################################################################################
#                               Arch Linux
####################################################################################
get_arch_build_tools_required() {
    echo "base-devel git cmake gcc vim gdb screen sudo pyalpm wget rsync"
}

get_arch_ygor_deps_required() {
    echo "gsl eigen boost-libs"
}

get_arch_dcma_deps_required() {
    echo "gcc-libs gnu-free-fonts sfml sdl2 glew glu jansson libpqxx postgresql zlib cgal wt asio nlopt patchelf freeglut libxi libxmu thrift"
}

get_arch_headless_rendering_required() {
    echo "xorg-server xorg-apps mesa xf86-video-dummy"
}

get_arch_optional_required() {
    echo "bash-completion libnotify dunst"
}

####################################################################################
#                           Arch Linux with SYCL
####################################################################################
get_arch_sycl_build_tools_required() {
    echo "base-devel git cmake gcc vim gdb screen wget rsync which sudo pyalpm"
}

get_arch_sycl_ygor_deps_required() {
    echo "gsl eigen boost-libs"
}

get_arch_sycl_dcma_deps_required() {
    echo "gcc-libs gnu-free-fonts sdl2 glew glu jansson libpqxx postgresql zlib cgal wt asio nlopt patchelf freeglut libxi libxmu thrift"
}

get_arch_sycl_headless_rendering_required() {
    echo "xorg-server xorg-apps mesa xf86-video-dummy"
}

get_arch_sycl_optional_required() {
    echo "bash-completion libnotify dunst zenity"
}

get_arch_sycl_extra_toolchains_required() {
    echo "clang libclc ocl-icd opencl-mesa pocl clinfo"
}

get_arch_sycl_external_third_party_required() {
    # AUR packages - sfml2 and adaptivecpp
    echo "sfml2 adaptivecpp"
}

####################################################################################
#                               Debian Buster
####################################################################################
get_debian_buster_build_tools_required() {
    echo "git cmake make g++ ncurses-term gdb rsync wget ca-certificates file coreutils binutils findutils openssh-client"
}

get_debian_buster_ygor_deps_required() {
    echo "libboost-dev libgsl-dev libeigen3-dev"
}

get_debian_buster_dcma_deps_required() {
    echo "libeigen3-dev libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev libz-dev libsfml-dev libsdl2-dev libglew-dev libjansson-dev libpqxx-dev postgresql-client libcgal-dev libnlopt-dev libnlopt-cxx-dev libasio-dev fonts-freefont-ttf fonts-cmu freeglut3 freeglut3-dev libxi-dev libxmu-dev libthrift-dev thrift-compiler patchelf"
}

get_debian_buster_headless_rendering_required() {
    echo "x-window-system mesa-utils xserver-xorg-video-dummy x11-apps"
}

get_debian_buster_optional_required() {
    echo "libnotify-dev dunst bash-completion gnuplot zenity"
}

get_debian_buster_extra_toolchains_optional() {
    echo "clang clang-format clang-tidy clang-tools"
}

get_debian_buster_optional_optional() {
    # Additional packages prospectively added for future development
    echo "libsqlite3-dev sqlite3 liblua5.3-dev libpython3-dev libprotobuf-dev protobuf-compiler"
}

####################################################################################
#                               Debian Bookworm
####################################################################################
get_debian_bookworm_build_tools_required() {
    echo "git cmake make g++ vim ncurses-term gdb rsync wget ca-certificates"
}

get_debian_bookworm_ygor_deps_required() {
    echo "libboost-dev libgsl-dev libeigen3-dev"
}

get_debian_bookworm_dcma_deps_required() {
    echo "libeigen3-dev libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev libz-dev libsfml-dev libsdl2-dev libglew-dev libjansson-dev libpqxx-dev postgresql-client libcgal-dev libnlopt-dev libnlopt-cxx-dev libasio-dev fonts-freefont-ttf fonts-cmu freeglut3-dev libxi-dev libxmu-dev libthrift-dev thrift-compiler patchelf"
}

get_debian_bookworm_headless_rendering_required() {
    echo "x-window-system mesa-utils xserver-xorg-video-dummy x11-apps"
}

get_debian_bookworm_optional_required() {
    echo "libnotify-dev dunst bash-completion gnuplot zenity"
}

####################################################################################
#                               Debian Bullseye
####################################################################################
get_debian_bullseye_build_tools_required() {
    echo "git cmake make g++ vim ncurses-term gdb rsync wget ca-certificates"
}

get_debian_bullseye_ygor_deps_required() {
    echo "libboost-dev libgsl-dev libeigen3-dev"
}

get_debian_bullseye_dcma_deps_required() {
    echo "libeigen3-dev libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev libz-dev libsfml-dev libsdl2-dev libglew-dev libjansson-dev libpqxx-dev postgresql-client libcgal-dev libnlopt-dev libnlopt-cxx-dev libasio-dev fonts-freefont-ttf fonts-cmu freeglut3 freeglut3-dev libxi-dev libxmu-dev libthrift-dev thrift-compiler patchelf"
}

get_debian_bullseye_headless_rendering_required() {
    echo "x-window-system mesa-utils xserver-xorg-video-dummy x11-apps"
}

get_debian_bullseye_optional_required() {
    echo "libnotify-dev dunst bash-completion gnuplot zenity"
}

####################################################################################
#                               Debian Stretch
####################################################################################
get_debian_stretch_build_tools_required() {
    echo "git cmake make g++ vim ncurses-term gdb rsync wget ca-certificates"
}

get_debian_stretch_ygor_deps_required() {
    echo "libboost-dev libgsl-dev libeigen3-dev"
}

get_debian_stretch_dcma_deps_required() {
    echo "libeigen3-dev libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev libz-dev libsfml-dev libsdl2-dev libglew-dev libjansson-dev libpqxx-dev postgresql-client libcgal-dev libnlopt-dev libasio-dev fonts-freefont-ttf fonts-cmu freeglut3 freeglut3-dev libxi-dev libxmu-dev xz-utils libthrift-dev thrift-compiler patchelf"
}

get_debian_stretch_headless_rendering_required() {
    echo "x-window-system mesa-utils xserver-xorg-video-dummy x11-apps"
}

get_debian_stretch_optional_required() {
    echo "libnotify dunst bash-completion gnuplot zenity"
}

####################################################################################
#                               macOS
####################################################################################
get_macos_build_tools_required() {
    echo "git svn coreutils cmake make vim rsync wget"
}

get_macos_ygor_deps_required() {
    echo "gsl eigen boost"
}

get_macos_dcma_deps_required() {
    echo "sdl2 zlib sfml glew jansson libpq nlopt asio thrift gcc llvm"
}

get_macos_optional_required() {
    echo "cgal libpqxx libnotify zenity"
}

get_macos_headless_rendering_required() {
    echo "mesa freeglut"
}

####################################################################################
#                               MXE (Cross-compile for Windows)
####################################################################################
get_mxe_build_tools_required() {
    echo "autoconf automake autopoint bash bison bzip2 flex g++ g++-multilib gettext git gperf intltool libc6-dev-i386 libgdk-pixbuf2.0-dev libltdl-dev libssl-dev libtool-bin libxml-parser-perl lzip make openssl p7zip-full patch perl python3 python3-mako python-is-python3 ruby sed unzip wget xz-utils ca-certificates rsync sudo gnupg"
}

get_mxe_external_third_party_required() {
    # These packages are built from source via MXE build system
    echo "gmp mpfr boost eigen sfml sdl2 glew nlopt mesa cgal thrift"
}

####################################################################################
#                               Ubuntu
####################################################################################
get_ubuntu_build_tools_required() {
    echo "bash git cmake make g++ vim ncurses-term gdb rsync wget ca-certificates"
}

get_ubuntu_ygor_deps_required() {
    echo "libboost-dev libgsl-dev libeigen3-dev"
}

get_ubuntu_dcma_deps_required() {
    echo "libeigen3-dev libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev libz-dev libsfml-dev libsdl2-dev libglew-dev libjansson-dev libpqxx-dev postgresql-client libcgal-dev libnlopt-dev libnlopt-cxx-dev libasio-dev fonts-freefont-ttf fonts-cmu freeglut3 freeglut3-dev libxi-dev libxmu-dev patchelf libthrift-dev thrift-compiler"
}

get_ubuntu_headless_rendering_required() {
    echo "x-window-system mesa-utils x11-apps libfreetype6 libsdl2-dev libice-dev libsm-dev libopengl0"
}

get_ubuntu_optional_required() {
    echo "libnotify-dev dunst bash-completion gnuplot zenity"
}

####################################################################################
#                               Void Linux
####################################################################################
get_void_build_tools_required() {
    echo "base-devel bash git cmake vim ncurses-term gdb rsync wget"
}

get_void_ygor_deps_required() {
    echo "boost-devel gsl-devel eigen"
}

get_void_dcma_deps_required() {
    echo "eigen boost-devel zlib-devel SFML-devel SDL2-devel glew-devel jansson-devel libpqxx-devel postgresql-client cgal-devel nlopt-devel asio freefont-ttf patchelf thrift thrift-devel"
}

get_void_headless_rendering_required() {
    echo "xorg-minimal glu-devel xorg-video-drivers xf86-video-dummy xorg-apps"
}

get_void_optional_required() {
    echo "libnotify dunst bash-completion gnuplot zenity"
}

####################################################################################
#                           Main logic
####################################################################################

# Normalize OS name (handle variations)
normalize_os() {
    local os="$1"
    case "$os" in
        alpine|alpine_linux)
            echo "alpine"
            ;;
        arch|archlinux|arch_linux)
            echo "arch"
            ;;
        arch_sycl|archlinux_sycl|arch_linux_sycl)
            echo "arch_sycl"
            ;;
        debian_buster|buster)
            echo "debian_buster"
            ;;
        debian_bookworm|bookworm)
            echo "debian_bookworm"
            ;;
        debian_bullseye|bullseye)
            echo "debian_bullseye"
            ;;
        debian_stretch|stretch)
            echo "debian_stretch"
            ;;
        macos|darwin|osx)
            echo "macos"
            ;;
        mxe|mingw|windows_cross)
            echo "mxe"
            ;;
        ubuntu)
            echo "ubuntu"
            ;;
        void|void_linux|voidlinux)
            echo "void"
            ;;
        *)
            error_exit "Unknown OS: $os. Valid options: alpine, arch, arch_sycl, debian_buster, debian_bookworm, debian_bullseye, debian_stretch, macos, mxe, ubuntu, void"
            ;;
    esac
}

# Get all valid tiers for an OS
get_valid_tiers() {
    local os="$1"
    echo "build_tools extra_toolchains ygor_deps dcma_deps headless_rendering optional external_third_party"
}

# Collect packages for a given OS and tier
get_packages_for_tier() {
    local os="$1"
    local tier="$2"
    local packages=""

    # Function name pattern: get_<os>_<tier>_required and get_<os>_<tier>_optional
    local func_required="get_${os}_${tier}_required"
    local func_optional="get_${os}_${tier}_optional"

    if ! $OPTIONAL_ONLY; then
        if declare -f "$func_required" > /dev/null; then
            packages+="$(${func_required}) "
        fi
    fi

    if ! $REQUIRED_ONLY; then
        if declare -f "$func_optional" > /dev/null; then
            packages+="$(${func_optional}) "
        fi
    fi

    echo "$packages"
}

# Main function
main() {
    local normalized_os
    normalized_os="$(normalize_os "$OS")"

    local all_packages=""

    # If no tiers specified, use all tiers
    if [[ ${#TIERS[@]} -eq 0 ]]; then
        # Default tiers for most platforms (excluding external_third_party by default)
        TIERS=("build_tools" "extra_toolchains" "ygor_deps" "dcma_deps" "headless_rendering" "optional")
    fi

    for tier in "${TIERS[@]}"; do
        all_packages+="$(get_packages_for_tier "$normalized_os" "$tier") "
    done

    # Remove duplicate packages and extra whitespace
    local unique_packages
    unique_packages=$(echo "$all_packages" | tr ' ' '\n' | sort -u | grep -v '^$' | tr '\n' ' ')

    # Output based on format
    if [[ "$OUTPUT_FORMAT" == "newline" ]]; then
        echo "$unique_packages" | tr ' ' '\n' | grep -v '^$'
    else
        echo "$unique_packages" | xargs
    fi
}

main
