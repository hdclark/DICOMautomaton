#!/bin/bash

# This script installs all dependencies needed to build DICOMautomaton on Debian's stable distribution.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

export DEBIAN_FRONTEND="noninteractive"


apt-get update --yes
apt-get install --yes --no-install-recommends \
  git \
  cmake \
  make \
  g++ \
  vim \
  ncurses-term \
  gdb \
  rsync \
  wget \
  ca-certificates

apt-get install --yes --no-install-recommends \
    ` # Ygor dependencies ` \
    libboost-dev \
    libgsl-dev \
    libeigen3-dev \
    ` # DICOMautomaton dependencies ` \
    libeigen3-dev \
    libboost-dev \
    libboost-filesystem-dev \
    libboost-iostreams-dev \
    libboost-program-options-dev \
    libboost-thread-dev \
    libz-dev \
    libsfml-dev \
    libjansson-dev \
    libpqxx-dev \
    postgresql-client \
    libcgal-dev \
    libnlopt-dev \
    libnlopt-cxx-dev \
    libasio-dev \
    fonts-freefont-ttf \
    fonts-cmu \
    freeglut3 \
    freeglut3-dev \
    libxi-dev \
    libxmu-dev \
    gnuplot \
    zenity \
    patchelf \
    bash-completion \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    x-window-system \
    mesa-utils \
    xserver-xorg-video-dummy \
    x11-apps

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
make -j "$JOBS" VERBOSE=1
make install
make clean

mkdir -pv /scratch_base
cd /scratch_base
apt-get install --yes \
  -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb


# Install Ygor.
#
# Option 1: install a binary package.
#mkdir -pv /scratch
#cd /scratch
#apt-get install --yes -f ./Ygor*deb
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
#mkdir -pv /scratch
#cd /scratch
#apt-get install --yes -f ./Explicator*deb
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

