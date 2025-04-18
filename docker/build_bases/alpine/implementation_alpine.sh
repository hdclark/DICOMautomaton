#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton on Alpine Linux.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

source /scratch_base/set_environment.sh


retry_count=0
retry_limit=5
until
    # Install build dependencies.
    apk add --no-cache --update \
        alpine-sdk \
        bash \
        git \
        cmake \
        vim \
        gdb \
        rsync \
        openssh \
        wget \
        unzip \
    && \
    apk add --no-cache --update \
        ` # Additional toolchain ` \
        clang \
        clang-headers \
        clang-libs \
        clang-extra-tools \
        llvm \
        libc++ \
        lld \
        musl-dev \
        compiler-rt \
    && \
    apk add --no-cache \
        ` # Ygor dependencies ` \
        gsl-static gsl-dev \
        eigen-dev \
        ` # DICOMautomaton dependencies ` \
        openssl-libs-static \
        zlib-static zlib-dev \
        sfml-dev \
        sdl2-dev \
        glew-dev \
        jansson-dev \
        patchelf \
        ` # Additional dependencies for headless OpenGL rendering with SFML ` \
        libx11-dev libx11-static \
        glu-dev glu \
        mesa mesa-dev \
        xorg-server-dev \
        xf86-video-dummy \
        ` # Other optional dependencies ` \
        libnotify-dev dunst
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

# Omitted (for now):
#    cgal-dev \
#    libpqxx-devel \
#    postgresql-dev \
#    nlopt-devel \
#    freefont-ttf \
#    gnuplot \
#    zenity  \
#    bash-completion \
#    xorg-video-drivers \
#    xorg-apps

# Intentionally omitted:
#    boost1.76-static boost1.76-dev \
#    asio-dev \
# Note: these libraries seem to be either shared, or have a shared runtime, which CMake dislikes when compiling static DCMA.

# Removed:
#    sdl-static      (Seems to have disappeared during SDL3 release.)

/scratch_base/build_install_customized_system_dependencies.sh

# Building the following are not necessary, since a later stage will re-build everything anyway. However, it's useful to
# do some computation up-front and possibly cache the results.
/scratch_base/build_install_other_dependencies.sh
#/scratch_base/build_install_dcma.sh

