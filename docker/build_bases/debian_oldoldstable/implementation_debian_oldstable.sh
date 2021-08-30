#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Debian oldoldstable system.

#
# NOTE (20201031): The approach shown below does not work. Specifically, linking so's created with the native toolchain
# (i.e., installed packages) fails when using the new toolchain. Clang provides enough libraries to successfully
# compile, but cannot overcome the incompatibilites in linking. It might be possible to use one of the alternative
# approaches below (e.g., using a backported newer clang toolchain, making source-level changes to compile with earlier
# C++ standards, or modifying 'experimental' features to hoist them from experimental/ and the experimental namespace).
# 

exit 1
exit 1
exit 1

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
    libasio-dev \
    fonts-freefont-ttf \
    fonts-cmu \
    freeglut3 \
    freeglut3-dev \
    libxi-dev \
    libxmu-dev \
    xz-utils \
    `# 'libc++-dev' ` \
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

## Use backported packages.
#echo "deb http://deb.debian.org/debian stretch-backports main" > /etc/apt/sources.list.d/backports.list
#apt-get update --yes
#apt-get -t stretch-backports install --yes --no-install-recommends \
#    clang-6.0 \
#    clang-7
#    #cmake \
#    #libarchive13 \

# Provide a more recent CMake binary.
apt-get remove --yes cmake
wget "https://github.com/Kitware/CMake/releases/download/v3.17.5/cmake-3.17.5-Linux-x86_64.sh" -O /tmp/cmake.sh
bash /tmp/cmake.sh --skip-license --prefix=/usr/
rm -rf /tmp/cmake.sh

# Provide a newer (precompiled) clang toolchain.
#wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz' -O /tmp/clang.tar.xz
wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/clang+llvm-10.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz' -O /tmp/clang.tar.xz
tar -C $HOME -axf /tmp/clang.tar.xz
rsync -a "$HOME/clang+llvm-10.0.1-x86_64-linux-gnu-ubuntu-16.04/" /usr/
rm -rf /tmp/clang.tar.xz "$HOME/clang+llvm-10.0.1-x86_64-linux-gnu-ubuntu-16.04/"
#export PATH="$HOME/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04/bin:$PATH"
#export PATH="$HOME/clang+llvm-10.0.1-x86_64-linux-gnu-ubuntu-16.04/bin:$PATH"

export CC="clang" 
export CXX="clang++"
#export CXXFLAGS="-v --rtlib=compiler-rt -stdlib=libc++ --static-libstdc++ --static-libgcc"
export CXXFLAGS="-stdlib=libc++"
#export LDFLAGS="-lc++abi" 

## Provide a custom libcxx that is linked against the current glibc.
#wget 'https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.1/libcxx-10.0.1.src.tar.xz' -O /tmp/libcxx.tar.xz
#tar -C $HOME -axf /tmp/libcxx.tar.xz
#rm -rf /tmp/libcxx.tar.xz
#cd $HOME/libcxx*
#mkdir build
#cd build
#CC="clang" \
#CXX="clang++" \
#cmake ../
#make -j2 install

 
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

