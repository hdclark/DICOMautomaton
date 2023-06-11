#!/usr/bin/env bash

# This script installs dependencies and then builds and installs DICOMautomaton.
# It can be used for continuous integration (CI), development, and deployment (CD).

set -eux

# Install build dependencies.
export DEBIAN_FRONTEND="noninteractive"

retry_count=0
retry_limit=5
until
    apt-get update --yes && \
    apt-get install --yes --no-install-recommends \
      git \
      cmake \
      make \
      g++ \
      ncurses-term \
      gdb \
      rsync \
      wget \
      ca-certificates \
      file \
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
      libthrift-dev \
      thrift-compiler \
      patchelf #\
#     && \
#     apt-get install --yes --no-install-recommends \
#      ` # Additional dependencies for headless OpenGL rendering with SFML ` \
#      x-window-system \
#      mesa-utils \
#      xserver-xorg-video-dummy \
#      x11-apps
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done
rm -rf /var/lib/apt/lists/*

cp /scratch/xpra-xorg.conf /etc/X11/xorg.conf || true


## Install Wt from source to get around outdated and buggy Debian package.
##
## Note: Add additional dependencies if functionality is needed -- this is a basic install.
##
## Note: Could also install build-deps for the distribution packages, but the dependencies are not
##       guaranteed to be stable (e.g., major version bumps).
#mkdir -pv /wt
#cd /wt
#git clone https://github.com/emweb/wt.git .
#cmake \
#  -B build \
#  -DCMAKE_INSTALL_PREFIX=/usr \
#  -DCMAKE_BUILD_TYPE=QUICK \
#  -DCMAKE_C_FLAGS_QUICK="-O0" \
#  -DCMAKE_CXX_FLAGS_QUICK="-O0" \
#  -DBUILD_EXAMPLES=OFF \
#  -DBUILD_TESTS=OFF \
#  -DENABLE_HARU=OFF \
#  -DENABLE_PANGO=OFF \
#  -DENABLE_POSTGRES=OFF \
#  -DENABLE_FIREBIRD=OFF \
#  -DENABLE_MYSQL=OFF \
#  -DENABLE_MSSQLSERVER=OFF \
#  -DENABLE_QT4=OFF \
#  -DENABLE_QT5=OFF \
#  -DENABLE_LIBWTTEST=OFF \
#  ./
#JOBS=$(nproc)
#JOBS=$(( JOBS < 8 ? JOBS : 8 ))
#JOBS=$(( JOBS < 3 ? 3 : JOBS ))
#make -j "$JOBS" VERBOSE=1
#make install
#make clean
#
#cd /scratch
#apt-get install --yes \
#  -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb


# Install Ygor.
#
# Option 1: install a binary package.
#mkdir -pv /ygor
#cd /ygor
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
#mkdir -pv /explicator
#cd /explicator
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


# Install DICOMautomaton.
#
# Option 1: install a binary package.
#mkdir -pv /dcma
#cd /dcma
#apt-get install --yes -f ./DICOMautomaton*deb 
#
# Option 2: clone the latest upstream commit.
#mkdir -pv /dcma
#cd /dcma
#git clone https://github.com/hdclark/DICOMautomaton .
#   ...
#
# Option 3: use the working directory.
mkdir -pv /dcma
cd /dcma
sed -i -e 's@MEMORY_CONSTRAINED_BUILD=OFF@MEMORY_CONSTRAINED_BUILD=ON@' /dcma/compile_and_install.sh
sed -i -e 's@option.*WITH_WT.*ON.*@option(WITH_WT "Wt disabled" OFF)@' /dcma/CMakeLists.txt
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 

