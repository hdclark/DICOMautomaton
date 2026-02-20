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
#                       Values: build_tools, development, ygor_deps, dcma_deps,
#                               appimage_runtime
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

# Ensure mutually exclusive flags are not used together
if [[ "$REQUIRED_ONLY" == true && "$OPTIONAL_ONLY" == true ]]; then
    error_exit "Cannot use --required-only and --optional-only together"
fi
# --- Package definitions ---
# Each function outputs space-separated package names.
# The naming convention is: get_<os>_<tier>_<required|optional>

####################################################################################
#                               Alpine Linux
####################################################################################
get_alpine_build_tools_required() {
    printf "alpine-sdk "
    printf "bash "
    printf "git "
    printf "cmake "
    printf "rsync "
    printf "clang "
    printf "clang-headers "
    printf "clang-libs "
    printf "clang-extra-tools "
    printf "llvm "
    printf "libc++ "
    printf "lld "
    printf "musl-dev "
    printf "compiler-rt "
    printf '\n'
}

get_alpine_build_tools_optional() {
    printf '\n'
}

get_alpine_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_alpine_development_optional() {
    printf "vim "
    printf "openssh "
    printf "wget "
    printf "unzip "
    printf '\n'
}

get_alpine_ygor_deps_required() {
    printf "gsl-static "
    printf "gsl-dev "
    printf "eigen-dev "
    printf '\n'
}

get_alpine_ygor_deps_optional() {
    printf '\n'
}

get_alpine_dcma_deps_required() {
    printf "openssl-libs-static "
    printf "zlib-static "
    printf "zlib-dev "
    printf "sfml-dev "
    printf "sdl2-dev "
    printf "glew-dev "
    printf "jansson-dev "
    printf '\n'
}

get_alpine_dcma_deps_optional() {
    # Headless/graphical components.
    printf "libx11-dev "
    printf "libx11-static "
    printf "glu-dev "
    printf "glu "
    printf "mesa "
    printf "mesa-dev "
    printf "xorg-server-dev "
    printf "xf86-video-dummy "

    # Runtime components.
    printf "libnotify-dev "
    printf "dunst " # Or any other notification server compatible with libnotify.
    printf '\n'
}

get_alpine_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf '\n'
}

get_alpine_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Arch Linux
####################################################################################
get_arch_build_tools_required() {
    printf "base-devel "
    printf "git "
    printf "asio "
    printf "cmake "

    printf "rsync "
    printf '\n'
}

get_arch_build_tools_optional() {
    printf "adaptivecpp " # + other optional hw accel components for SYCL.
    printf '\n'
}

get_arch_development_required() {
    printf "gdb "
    printf "sudo "
    printf "patchelf "
    printf '\n'
}

get_arch_development_optional() {
    printf "vim "
    printf "screen "
    printf "sudo "
    printf "pyalpm "
    printf "wget "
    printf '\n'
}

get_arch_ygor_deps_required() {
    printf "gsl "
    printf "eigen "
    printf "boost-libs "
    printf '\n'
}

get_arch_ygor_deps_optional() {
    printf '\n'
}

get_arch_dcma_deps_required() {
    printf "gsl "
    printf "eigen "
    printf "boost-libs "
    printf "gcc-libs "
    printf "gnu-free-fonts "
    printf "zlib "
    printf '\n'
}

get_arch_dcma_deps_optional() {
    # Optional buildtime components.
    printf "jansson "
    printf "libpqxx "
    printf "postgresql "
    printf "cgal "
    printf "wt "
    printf "nlopt "
    printf "thrift "

    # Optional graphical components.
    printf "sfml "
    printf "sdl2 "
    printf "glew "
    printf "glu "
    printf "freeglut "
    printf "libxi "
    printf "libxmu "
    printf "xorg-server "
    printf "xorg-apps "
    printf "mesa "
    printf "xf86-video-dummy "

    # Runtime components.
    printf "bash-completion "
    printf "zenity "
    printf "libnotify "
    printf "dunst " # Or any other notification server compatible with libnotify.
    printf '\n'
}

get_arch_appimage_runtime_required() {
    printf 'coreutils '
    printf 'binutils '
    printf 'findutils '
    printf 'grep '
    printf 'sed '
    printf 'curl '
    printf 'wget '
    printf 'git '
    printf 'ca-certificates '
    printf '\n'
}


get_arch_appimage_runtime_optional() {
    printf '\n'
}



####################################################################################
#                           Arch Linux with SYCL
####################################################################################
get_arch_sycl_build_tools_required() {
    printf "base-devel "
    printf "git "
    printf "cmake "
    printf "gcc "
    printf "rsync "
    printf "which "
    printf "asio "
    printf '\n'
}

get_arch_sycl_build_tools_optional() {
    # SYCL components.
    printf "clang "
    printf "libclc "
    printf "ocl-icd "
    printf "opencl-mesa "
    printf "pocl "
    printf "clinfo "
    # AUR packages - sfml2 and adaptivecpp
    printf "sfml2 "
    printf "adaptivecpp "
    printf '\n'
}

get_arch_sycl_development_required() {
    printf "gdb "
    printf "sudo "
    printf "patchelf "
    printf '\n'
}

get_arch_sycl_development_optional() {
    printf "vim "
    printf "screen "
    printf "wget "
    printf "pyalpm "
    printf '\n'
}

get_arch_sycl_ygor_deps_required() {
    printf "gsl "
    printf "eigen "
    printf "boost-libs "
    printf '\n'
}

get_arch_sycl_ygor_deps_optional() {
    printf '\n'
}

get_arch_sycl_dcma_deps_required() {
    printf "gcc-libs "
    printf "gnu-free-fonts "
    printf "zlib "
    printf '\n'
}

get_arch_sycl_dcma_deps_optional() {
    # Optional buildtime components.
    printf "sdl2 "
    printf "glew "
    printf "glu "
    printf "jansson "
    printf "libpqxx "
    printf "postgresql "
    printf "cgal "
    printf "wt "
    printf "nlopt "
    printf "freeglut "
    printf "libxi "
    printf "libxmu "
    printf "thrift "

    # Optional graphical components.
    printf "xorg-server "
    printf "xorg-apps "
    printf "mesa "
    printf "xf86-video-dummy "

    # Runtime components.
    printf "bash-completion "
    printf "libnotify "
    printf "dunst "
    printf "zenity "
    printf '\n'
}

get_arch_sycl_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf '\n'
}

get_arch_sycl_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Debian Buster
####################################################################################
get_debian_buster_build_tools_required() {
    printf "git "
    printf "cmake "
    printf "make "
    printf "g++ "
    printf "rsync "
    printf '\n'
}

get_debian_buster_build_tools_optional() {
    printf "clang "
    printf "clang-format "
    printf "clang-tidy "
    printf "clang-tools "
    printf '\n'
}

get_debian_buster_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_debian_buster_development_optional() {
    printf "ncurses-term "
    printf "wget "
    printf "ca-certificates "
    printf "file "
    printf "openssh-client "
    # Additional packages prospectively added for future development
    printf "libsqlite3-dev "
    printf "sqlite3 "
    printf "liblua5.3-dev "
    printf "libpython3-dev "
    printf "libprotobuf-dev "
    printf "protobuf-compiler "
    printf '\n'
}

get_debian_buster_ygor_deps_required() {
    printf "libboost-dev "
    printf "libgsl-dev "
    printf "libeigen3-dev "
    printf '\n'
}

get_debian_buster_ygor_deps_optional() {
    printf '\n'
}

get_debian_buster_dcma_deps_required() {
    printf "libeigen3-dev "
    printf "libboost-dev "
    printf "libboost-filesystem-dev "
    printf "libboost-iostreams-dev "
    printf "libboost-program-options-dev "
    printf "libboost-thread-dev "
    printf "libz-dev "
    printf "libasio-dev "
    printf "fonts-freefont-ttf "
    printf "fonts-cmu "
    printf '\n'
}

get_debian_buster_dcma_deps_optional() {
    # Optional buildtime components.
    printf "libsfml-dev "
    printf "libsdl2-dev "
    printf "libglew-dev "
    printf "libjansson-dev "
    printf "libpqxx-dev "
    printf "postgresql-client "
    printf "libcgal-dev "
    printf "libnlopt-dev "
    printf "libnlopt-cxx-dev "
    printf "freeglut3 "
    printf "freeglut3-dev "
    printf "libxi-dev "
    printf "libxmu-dev "
    printf "libthrift-dev "
    printf "thrift-compiler "

    # Optional graphical components.
    printf "x-window-system "
    printf "mesa-utils "
    printf "xserver-xorg-video-dummy "
    printf "x11-apps "

    # Runtime components.
    printf "libnotify-dev "
    printf "dunst "
    printf "bash-completion "
    printf "gnuplot "
    printf "zenity "
    printf '\n'
}

get_debian_buster_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf "openssl "
    printf "libgpg-error0 "
    printf "mesa-utils "
    printf "libfreetype6 "
    printf "libsdl2-dev "
    printf "libice-dev "
    printf "libsm-dev "
    printf "libopengl0 "
    printf "g++ "
    printf '\n'
}

get_debian_buster_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Debian Bookworm
####################################################################################
get_debian_bookworm_build_tools_required() {
    printf "git "
    printf "cmake "
    printf "make "
    printf "g++ "
    printf "rsync "
    printf '\n'
}

get_debian_bookworm_build_tools_optional() {
    printf '\n'
}

get_debian_bookworm_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_debian_bookworm_development_optional() {
    printf "vim "
    printf "ncurses-term "
    printf "wget "
    printf "ca-certificates "
    printf '\n'
}

get_debian_bookworm_ygor_deps_required() {
    printf "libboost-dev "
    printf "libgsl-dev "
    printf "libeigen3-dev "
    printf '\n'
}

get_debian_bookworm_ygor_deps_optional() {
    printf '\n'
}

get_debian_bookworm_dcma_deps_required() {
    printf "libeigen3-dev "
    printf "libboost-dev "
    printf "libboost-filesystem-dev "
    printf "libboost-iostreams-dev "
    printf "libboost-program-options-dev "
    printf "libboost-thread-dev "
    printf "libz-dev "
    printf "libasio-dev "
    printf "fonts-freefont-ttf "
    printf "fonts-cmu "
    printf '\n'
}

get_debian_bookworm_dcma_deps_optional() {
    # Optional buildtime components.
    printf "libsfml-dev "
    printf "libsdl2-dev "
    printf "libglew-dev "
    printf "libjansson-dev "
    printf "libpqxx-dev "
    printf "postgresql-client "
    printf "libcgal-dev "
    printf "libnlopt-dev "
    printf "libnlopt-cxx-dev "
    printf "freeglut3-dev "
    printf "libxi-dev "
    printf "libxmu-dev "
    printf "libthrift-dev "
    printf "thrift-compiler "

    # Optional graphical components.
    printf "x-window-system "
    printf "mesa-utils "
    printf "xserver-xorg-video-dummy "
    printf "x11-apps "

    # Runtime components.
    printf "libnotify-dev "
    printf "dunst "
    printf "bash-completion "
    printf "gnuplot "
    printf "zenity "
    printf '\n'
}

get_debian_bookworm_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf "openssl "
    printf "libgpg-error0 "
    printf "mesa-utils "
    printf "libfreetype6 "
    printf "libsdl2-dev "
    printf "libice-dev "
    printf "libsm-dev "
    printf "libopengl0 "
    printf "g++ "
    printf '\n'
}

get_debian_bookworm_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Debian Bullseye
####################################################################################
get_debian_bullseye_build_tools_required() {
    printf "git "
    printf "cmake "
    printf "make "
    printf "g++ "
    printf "rsync "
    printf '\n'
}

get_debian_bullseye_build_tools_optional() {
    printf '\n'
}

get_debian_bullseye_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_debian_bullseye_development_optional() {
    printf "vim "
    printf "ncurses-term "
    printf "wget "
    printf "ca-certificates "
    printf '\n'
}

get_debian_bullseye_ygor_deps_required() {
    printf "libboost-dev "
    printf "libgsl-dev "
    printf "libeigen3-dev "
    printf '\n'
}

get_debian_bullseye_ygor_deps_optional() {
    printf '\n'
}

get_debian_bullseye_dcma_deps_required() {
    printf "libeigen3-dev "
    printf "libboost-dev "
    printf "libboost-filesystem-dev "
    printf "libboost-iostreams-dev "
    printf "libboost-program-options-dev "
    printf "libboost-thread-dev "
    printf "libz-dev "
    printf "libasio-dev "
    printf "fonts-freefont-ttf "
    printf "fonts-cmu "
    printf '\n'
}

get_debian_bullseye_dcma_deps_optional() {
    # Optional buildtime components.
    printf "libsfml-dev "
    printf "libsdl2-dev "
    printf "libglew-dev "
    printf "libjansson-dev "
    printf "libpqxx-dev "
    printf "postgresql-client "
    printf "libcgal-dev "
    printf "libnlopt-dev "
    printf "libnlopt-cxx-dev "
    printf "freeglut3 "
    printf "freeglut3-dev "
    printf "libxi-dev "
    printf "libxmu-dev "
    printf "libthrift-dev "
    printf "thrift-compiler "

    # Optional graphical components.
    printf "x-window-system "
    printf "mesa-utils "
    printf "xserver-xorg-video-dummy "
    printf "x11-apps "

    # Runtime components.
    printf "libnotify-dev "
    printf "dunst "
    printf "bash-completion "
    printf "gnuplot "
    printf "zenity "
    printf '\n'
}

get_debian_bullseye_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf "openssl "
    printf "libgpg-error0 "
    printf "mesa-utils "
    printf "libfreetype6 "
    printf "libsdl2-dev "
    printf "libice-dev "
    printf "libsm-dev "
    printf "libopengl0 "
    printf "g++ "
    printf '\n'
}

get_debian_bullseye_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Debian Stretch
####################################################################################
get_debian_stretch_build_tools_required() {
    printf "git "
    printf "cmake "
    printf "make "
    printf "g++ "
    printf "rsync "
    printf '\n'
}

get_debian_stretch_build_tools_optional() {
    printf '\n'
}

get_debian_stretch_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_debian_stretch_development_optional() {
    printf "vim "
    printf "ncurses-term "
    printf "wget "
    printf "ca-certificates "
    printf '\n'
}

get_debian_stretch_ygor_deps_required() {
    printf "libboost-dev "
    printf "libgsl-dev "
    printf "libeigen3-dev "
    printf '\n'
}

get_debian_stretch_ygor_deps_optional() {
    printf '\n'
}

get_debian_stretch_dcma_deps_required() {
    printf "libeigen3-dev "
    printf "libboost-dev "
    printf "libboost-filesystem-dev "
    printf "libboost-iostreams-dev "
    printf "libboost-program-options-dev "
    printf "libboost-thread-dev "
    printf "libz-dev "
    printf "libasio-dev "
    printf "fonts-freefont-ttf "
    printf "fonts-cmu "
    printf '\n'
}

get_debian_stretch_dcma_deps_optional() {
    # Optional buildtime components.
    printf "libsfml-dev "
    printf "libsdl2-dev "
    printf "libglew-dev "
    printf "libjansson-dev "
    printf "libpqxx-dev "
    printf "postgresql-client "
    printf "libcgal-dev "
    printf "libnlopt-dev "
    printf "freeglut3 "
    printf "freeglut3-dev "
    printf "libxi-dev "
    printf "libxmu-dev "
    printf "xz-utils "
    printf "libthrift-dev "
    printf "thrift-compiler "

    # Optional graphical components.
    printf "x-window-system "
    printf "mesa-utils "
    printf "xserver-xorg-video-dummy "
    printf "x11-apps "

    # Runtime components.
    printf "libnotify "
    printf "dunst "
    printf "bash-completion "
    printf "gnuplot "
    printf "zenity "
    printf '\n'
}

get_debian_stretch_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf '\n'
}

get_debian_stretch_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               macOS
####################################################################################
get_macos_build_tools_required() {
    printf "git "
    printf "svn "
    printf "coreutils "
    printf "cmake "
    printf "make "
    printf "rsync "
    printf '\n'
}

get_macos_build_tools_optional() {
    printf '\n'
}

get_macos_development_required() {
    printf '\n'
}

get_macos_development_optional() {
    printf "vim "
    printf "wget "
    printf '\n'
}

get_macos_ygor_deps_required() {
    printf "gsl "
    printf "eigen "
    printf "boost "
    printf '\n'
}

get_macos_ygor_deps_optional() {
    printf '\n'
}

get_macos_dcma_deps_required() {
    printf "zlib "
    printf "asio "
    printf "gcc "
    printf "llvm "
    printf '\n'
}

get_macos_dcma_deps_optional() {
    printf "sdl2 "
    printf "sfml "
    printf "glew "
    printf "jansson "
    printf "libpq "
    printf "nlopt "
    printf "thrift "
    printf "cgal "
    printf "libpqxx "

    # Optional graphical components.
    printf "mesa "
    printf "freeglut "

    # Runtime components.
    printf "libnotify "
    printf "zenity "
    printf '\n'
}

get_macos_appimage_runtime_required() {
    printf '\n'
}

get_macos_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               MXE (Cross-compile for Windows)
####################################################################################
get_mxe_build_tools_required() {
    printf "autoconf "
    printf "automake "
    printf "autopoint "
    printf "bash "
    printf "bison "
    printf "bzip2 "
    printf "flex "
    printf "g++ "
    printf "g++-multilib "
    printf "gettext "
    printf "git "
    printf "gperf "
    printf "intltool "
    printf "libc6-dev-i386 "
    printf "libgdk-pixbuf2.0-dev "
    printf "libltdl-dev "
    printf "libssl-dev "
    printf "libtool-bin "
    printf "libxml-parser-perl "
    printf "lzip "
    printf "make "
    printf "openssl "
    printf "p7zip-full "
    printf "patch "
    printf "perl "
    printf "python3 "
    printf "python3-mako "
    printf "python-is-python3 "
    printf "ruby "
    printf "sed "
    printf "unzip "
    printf "wget "
    printf "xz-utils "
    printf "ca-certificates "
    printf "rsync "
    printf "sudo "
    printf "gnupg "
    printf '\n'
}

get_mxe_build_tools_optional() {
    printf '\n'
}

get_mxe_development_required() {
    printf '\n'
}

get_mxe_development_optional() {
    printf '\n'
}

get_mxe_ygor_deps_required() {
    # These packages are built from source via MXE build system
    printf "gmp "
    printf "mpfr "
    printf "boost "
    printf "eigen "
    printf '\n'
}

get_mxe_ygor_deps_optional() {
    printf '\n'
}

get_mxe_dcma_deps_required() {
    printf '\n'
}

get_mxe_dcma_deps_optional() {
    # These packages are built from source via MXE build system
    printf "sfml "
    printf "sdl2 "
    printf "glew "
    printf "nlopt "
    printf "mesa "
    printf "cgal "
    printf "thrift "
    printf '\n'
}

get_mxe_appimage_runtime_required() {
    printf '\n'
}

get_mxe_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Ubuntu
####################################################################################
get_ubuntu_build_tools_required() {
    printf "bash "
    printf "git "
    printf "cmake "
    printf "make "
    printf "g++ "
    printf "rsync "
    printf '\n'
}

get_ubuntu_build_tools_optional() {
    printf '\n'
}

get_ubuntu_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_ubuntu_development_optional() {
    printf "vim "
    printf "ncurses-term "
    printf "wget "
    printf "ca-certificates "
    printf '\n'
}

get_ubuntu_ygor_deps_required() {
    printf "libboost-dev "
    printf "libgsl-dev "
    printf "libeigen3-dev "
    printf '\n'
}

get_ubuntu_ygor_deps_optional() {
    printf '\n'
}

get_ubuntu_dcma_deps_required() {
    printf "libeigen3-dev "
    printf "libboost-dev "
    printf "libboost-filesystem-dev "
    printf "libboost-iostreams-dev "
    printf "libboost-program-options-dev "
    printf "libboost-thread-dev "
    printf "libz-dev "
    printf "libasio-dev "
    printf "fonts-freefont-ttf "
    printf "fonts-cmu "
    printf '\n'
}

get_ubuntu_dcma_deps_optional() {
    # Optional buildtime components.
    printf "libsfml-dev "
    printf "libsdl2-dev "
    printf "libglew-dev "
    printf "libjansson-dev "
    printf "libpqxx-dev "
    printf "postgresql-client "
    printf "libcgal-dev "
    printf "libnlopt-dev "
    printf "libnlopt-cxx-dev "
    printf "freeglut3 "
    printf "freeglut3-dev "
    printf "libxi-dev "
    printf "libxmu-dev "
    printf "libthrift-dev "
    printf "thrift-compiler "

    # Optional graphical components.
    printf "x-window-system "
    printf "mesa-utils "
    printf "x11-apps "
    printf "libfreetype6 "
    printf "libice-dev "
    printf "libsm-dev "
    printf "libopengl0 "

    # Runtime components.
    printf "libnotify-dev "
    printf "dunst "
    printf "bash-completion "
    printf "gnuplot "
    printf "zenity "
    printf '\n'
}

get_ubuntu_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf "libssl-dev "
    printf "mesa-utils "
    printf "libfreetype6 "
    printf "libsdl2-dev "
    printf "libice-dev "
    printf "libsm-dev "
    printf "libopengl0 "
    printf "g++ "
    printf '\n'
}

get_ubuntu_appimage_runtime_optional() {
    printf '\n'
}

####################################################################################
#                               Void Linux
####################################################################################
get_void_build_tools_required() {
    printf "base-devel "
    printf "bash "
    printf "git "
    printf "cmake "
    printf "rsync "
    printf '\n'
}

get_void_build_tools_optional() {
    printf '\n'
}

get_void_development_required() {
    printf "gdb "
    printf "patchelf "
    printf '\n'
}

get_void_development_optional() {
    printf "vim "
    printf "ncurses-term "
    printf "wget "
    printf '\n'
}

get_void_ygor_deps_required() {
    printf "boost-devel "
    printf "gsl-devel "
    printf "eigen "
    printf '\n'
}

get_void_ygor_deps_optional() {
    printf '\n'
}

get_void_dcma_deps_required() {
    printf "eigen "
    printf "boost-devel "
    printf "zlib-devel "
    printf "asio "
    printf "freefont-ttf "
    printf '\n'
}

get_void_dcma_deps_optional() {
    # Optional buildtime components.
    printf "SFML-devel "
    printf "SDL2-devel "
    printf "glew-devel "
    printf "jansson-devel "
    printf "libpqxx-devel "
    printf "postgresql-client "
    printf "cgal-devel "
    printf "nlopt-devel "
    printf "thrift "
    printf "thrift-devel "

    # Optional graphical components.
    printf "xorg-minimal "
    printf "glu-devel "
    printf "xorg-video-drivers "
    printf "xf86-video-dummy "
    printf "xorg-apps "

    # Runtime components.
    printf "libnotify "
    printf "dunst "
    printf "bash-completion "
    printf "gnuplot "
    printf "zenity "
    printf '\n'
}

get_void_appimage_runtime_required() {
    printf "coreutils "
    printf "binutils "
    printf "findutils "
    printf "grep "
    printf "sed "
    printf "curl "
    printf "wget "
    printf "git "
    printf "ca-certificates "
    printf '\n'
}

get_void_appimage_runtime_optional() {
    printf '\n'
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
    echo "build_tools development ygor_deps dcma_deps appimage_runtime"
}

# Architecture-specific package adjustments
# This function modifies the package list based on architecture constraints.
# Most packages are architecture-agnostic, but some need adjustment for non-x86_64.
adjust_packages_for_arch() {
    local os="$1"
    local arch="$2"
    local packages="$3"

    # For x86_64, no adjustments needed
    if [[ "$arch" == "x86_64" ]]; then
        echo "$packages"
        return
    fi

    # For non-x86_64 architectures, remove/replace packages that are not available
    case "$os" in
        mxe)
            # MXE cross-compilation is x86_64 specific
            if [[ "$arch" != "x86_64" ]]; then
                # Remove i386-specific packages for non-x86 hosts
                packages="${packages//libc6-dev-i386 /}"
                packages="${packages//g++-multilib /}"
            fi
            ;;
        debian_*|ubuntu)
            # Some Debian/Ubuntu packages may differ by architecture
            case "$arch" in
                aarch64|arm64)
                    # ARM64 adjustments - remove any x86-specific packages
                    packages="${packages//libc6-dev-i386 /}"
                    ;;
                armv7l|armhf)
                    # ARM32 adjustments
                    packages="${packages//libc6-dev-i386 /}"
                    ;;
            esac
            ;;
        alpine)
            # Alpine uses different package naming for some arch-specific packages
            case "$arch" in
                aarch64|arm64)
                    # Most Alpine packages are arch-agnostic in naming
                    ;;
                armv7|armhf)
                    # ARM32 adjustments
                    ;;
            esac
            ;;
    esac

    echo "$packages"
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

    # Apply architecture-specific adjustments
    packages="$(adjust_packages_for_arch "$os" "$ARCH" "$packages")"

    echo "$packages"
}

# Main function
main() {
    local normalized_os
    normalized_os="$(normalize_os "$OS")"

    local all_packages=""

    # If no tiers specified, use all tiers
    if [[ ${#TIERS[@]} -eq 0 ]]; then
        # Default tiers for most platforms
        TIERS=("build_tools" "development" "ygor_deps" "dcma_deps" "appimage_runtime")
    fi

    # Validate requested tiers against the known allowlist
    local valid_tiers_str
    valid_tiers_str="$(get_valid_tiers "$normalized_os")"
    local -a valid_tiers
    # shellcheck disable=SC2206  # intentional word-splitting into array
    valid_tiers=($valid_tiers_str)

    local tier is_valid
    for tier in "${TIERS[@]}"; do
        is_valid=false
        for valid_tier in "${valid_tiers[@]}"; do
            if [[ "$tier" == "$valid_tier" ]]; then
                is_valid=true
                break
            fi
        done
        if [[ "$is_valid" != true ]]; then
            echo "Error: Unknown tier '${tier}'. Valid tiers are: ${valid_tiers[*]}" >&2
            exit 1
        fi
    done

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
