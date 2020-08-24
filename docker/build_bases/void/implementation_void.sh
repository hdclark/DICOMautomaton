#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Void Linux system.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

# Install build dependencies.
xbps-install -y -Su xbps
xbps-install -y -Su
xbps-install -y -S \
  base-devel \
  bash \
  git \
  cmake \
  vim \
  ncurses-term \
  gdb \
  rsync \
  wget 

xbps-install -y -S \
    ` # Ygor dependencies ` \
    boost-devel \
    gsl-devel \
    eigen \
    ` # DICOMautomaton dependencies ` \
    eigen \
    boost-devel \
    zlib-devel \
    SFML-devel \
    jansson-devel \
    libpqxx-devel \
    postgresql-client \
    cgal-devel \
    nlopt-devel \
    asio \
    freefont-ttf \
    gnuplot \
    zenity  \
    patchelf \
    bash-completion \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    xorg-minimal \
    glu-devel \
    xorg-video-drivers \
    xf86-video-dummy \
    xorg-apps

cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf


# Install Wt from source to get around outdated and buggy Debian package.
#
# Note: Add additional dependencies if functionality is needed -- this is a basic install.
#
# Note: Could also install build-deps for the distribution packages, but the dependencies are not
#       guaranteed to be stable (e.g., major version bumps).
mkdir -pv /wt
cd /wt
git clone https://github.com/emweb/wt.git .
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ../
JOBS=$(nproc)
JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))
make -j "$JOBS" VERBOSE=0
make install
make clean


# Install Ygor.
#
# Option 1: install a binary package.
# ...
#
# Option 2: clone the latest upstream commit.
mkdir -pv /ygor
cd /ygor
git clone https://github.com/hdclark/Ygor .
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 

# Install Explicator.
#
# Option 1: install a binary package.
# ...
#
# Option 2: clone the latest upstream commit.
mkdir -pv /explicator
cd /explicator
git clone https://github.com/hdclark/explicator .
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 


# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
git clone https://github.com/hdclark/YgorClustering .
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 

