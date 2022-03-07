#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Debian oldstable system.

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
    libsdl2-dev \
    libglew-dev \
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
    patchelf \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    x-window-system \
    mesa-utils \
    xserver-xorg-video-dummy \
    x11-apps \
    ` # Other optional dependencies ` \
    libnotify \
    dunst \
    bash-completion \
    gnuplot \
    zenity 

cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf


# Install Wt from source to get around outdated and buggy Debian package.
#
# Note: Add additional dependencies if functionality is needed -- this is a basic install.
#
# Note: Could also install build-deps for the distribution packages, but the dependencies are not
#       guaranteed to be stable (e.g., major version bumps).
if [ ! -d /wt ] ; then
    mkdir -pv /wt
    cd /wt
    git clone https://github.com/emweb/wt.git .
    mkdir -p build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ../
    JOBS=$(nproc)
    JOBS=$(( JOBS < 8 ? JOBS : 8 ))
    make -j "$JOBS" VERBOSE=1
    make install
    make clean

    mkdir -pv /scratch_base
    cd /scratch_base
    apt-get install --yes \
      -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb
fi

# This function either freshly clones a git repository, or pulls from upstream remotes.
# The return value can be used to determine whether recompilation is required.
function clone_or_pull {
    if git clone "$@" . ; then
        return 0 # Requires compilation.
    fi

    if ! git fetch ; then
        return 2 # Failure.
    fi

    if [ "$(git rev-parse HEAD)" == "$(git rev-parse '@{u}')" ] ; then
        return 1 # Compilation not required.
    fi
    git merge
}
export -f clone_or_pull

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
if clone_or_pull "https://github.com/hdclark/Ygor" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Ygor already up-to-date.\n'
fi

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
if clone_or_pull "https://github.com/hdclark/explicator" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Explicator already up-to-date.\n'
fi

# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
if clone_or_pull "https://github.com/hdclark/YgorClustering" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Ygor Clustering already up-to-date.\n'
fi

