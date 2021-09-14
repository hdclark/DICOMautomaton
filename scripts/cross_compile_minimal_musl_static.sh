#!/usr/bin/env bash

# This script:
#  1. generates a musl-based cross-compiler,
#  2. compiles a few minimal dependencies for DICOMautomaton, and
#  3. compiles statically-linked DICOMautomaton binaries.
#
# Ideally, this script would be able to compile all dependencies.
# At the moment only a minimum set of dependecies are supported.
# Ideas and suggestions are welcome!

# sudo docker run --rm -it debian:oldstable

set -eux

# See https://github.com/richfelker/musl-cross-make README (musl cross-compiler).
#export TRIPLET='arm-linux-musleabi'
# See `zig target` (libc section) for other supported triplets using zig toolchain.
#export TRIPLET='x86_64-linux-musl'
#export TRIPLET='x86_64-linux-gnu.2.28' # Debian oldstable as of 20210914.
export TRIPLET='x86_64-linux-gnu.2.18' # Ancient glibc... Seems to have __cxa_thread_atexit_impl@GLIBC_2.18 whereas 2.17 does not have __cxa_thread_atexit_impl

apt-get update -y
apt-get upgrade -y
apt-get install -y clang wget xz-utils cmake git file vim rsync openssh-client \
                   python3 unzip #python3-pip pkg-config autoconf

# Zig-based toolchain.
cd / && rm -rf /zig*
wget 'https://ziglang.org/builds/zig-linux-x86_64-0.9.0-dev.952+0c091feb5.tar.xz'
tar -axf zig-linux-x86_64-0.9.0-dev.952+0c091feb5.tar.xz 
cd /zig*/
unset CC CXX CFLAGS CXXFLAGS LDFLAGS
export CC="$(pwd)/zig cc --verbose --target=${TRIPLET}"
export CXX="$(pwd)/zig c++ --verbose --target=${TRIPLET}"
export CFLAGS='-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ -static'
export CXXFLAGS='-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ -static -fno-use-cxa-atexit'
export LDFLAGS=""
cd /


## Musl (linux-only) cross-compiler.
#git clone 'https://github.com/richfelker/musl-cross-make' /mcm
#cd /mcm
#unset CC CXX CFLAGS CXXFLAGS LDFLAGS
#export CC="clang"
#export CXX="clang++"
#export CFLAGS=""
#export CXXFLAGS=""
#export LDFLAGS=""
#printf '\nTARGET=%s\n' "$TRIPLET" >> config.mak
#printf '\n%s\n' 'OUTPUT=/musl' >> config.mak
##printf '\n%s\n' 'GCC_VER = 10.2.0' >> config.mak
#time make -j8 VERBOSE=1 install
#unset CC CXX CFLAGS CXXFLAGS LDFLAGS
#export CC="/musl/bin/${TRIPLET}-gcc"
#export CXX="/musl/bin/${TRIPLET}-g++"
#export CXXFLAGS="-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ -static -fno-use-cxa-atexit"
#export CFLAGS="-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ -static"
#export LDFLAGS=""


# zlib.
(
    cd / && rm -rf /zlib*
    wget 'https://zlib.net/zlib-1.2.11.tar.gz'
    tar -axf 'zlib-1.2.11.tar.gz'
    rm 'zlib-1.2.11.tar.gz'
    cd /zlib-1.2.11
    ./configure --static --prefix=/pot
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /zlib*
)

# boost.
(
    cd / && rm -rf /boost*
    wget 'https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.gz' 
    tar -axf 'boost_1_76_0.tar.gz' 
    rm 'boost_1_76_0.tar.gz' 
    cd /boost_1_76_0/tools/build/src/engine/
    printf '%s\n' "using gcc : : ${CXX} : <cxxflags>${CXXFLAGS} <linkflags>${LDFLAGS} ;" > /root/user-config.jam
    printf '%s\n' "using zlib : 1.2.11 : <search>/pot/lib <name>zlib <include>/pot/include ;" >> /root/user-config.jam
    ./build.sh --cxx="clang++"
    cd ../../
    ./bootstrap.sh --cxx="clang++"
    ./b2 --prefix=/pot install
    cd ../../
    ./tools/build/src/engine/b2 -j8 \
        --user-config=/root/user-config.jam \
        --prefix=/pot \
        release \
        toolset=gcc \
        debug-symbols=off \
        threading=multi \
        runtime-link=static \
        link=static \
        cflags='-fno-use-cxa-atexit -fno-strict-aliasing' \
        --includedir=/pot/usr/include \
        --libdir=/pot/usr/lib \
        --with-iostreams \
        --with-serialization \
        --with-thread \
        --with-system \
        --with-filesystem \
        --with-regex \
        --with-chrono \
        --with-date_time \
        --with-atomic \
        -sZLIB_NAME=libz \
        -sZLIB_INCLUDE=/pot/include/ \
        -sZLIB_LIBRARY_PATH=/pot/lib/ \
        install
    ls -lash /pot/usr/lib
    cd / && rm -rf /boost*
)

# Asio.
(
    cd / && rm -rf /asio
    mkdir -pv /asio
    cd /asio
    wget 'https://sourceforge.net/projects/asio/files/latest/download' -O asio.tgz
    tar -axf asio.tgz || unzip asio.tgz
    cd asio-*/
    cp -v -R include/asio/ include/asio.hpp /pot/include/
    cd / && rm -rf /asio
)

# These are WIP. They are kind of a pain to build...
#
## Pkg-config.
#(
#    cd / && rm -rf /pkgconfig
#    mkdir /pkgconfig && cd /pkgconfig
#    git clone --depth=1 'https://gitlab.freedesktop.org/pkg-config/pkg-config' 
#    cd pkg-config*/
#    ./autogen.sh
#
#    cd / && rm -rf /pkgconfig
#)
#
## Mesa.
#(
#    cd / && rm -rf /mesa
#    mkdir /mesa && cd /mesa
#    wget 'https://mesa.freedesktop.org/archive/mesa-21.2.1.tar.xz' -O mesa.txz
#    tar -axf mesa.txz
#    cd mesa*/
#    pip3 install meson mako
#    export PATH="/usr/local/bin/:${PATH}"
#    #/usr/local/bin/meson setup . output
#    /usr/local/bin/meson setup --cross-file /dev/null . output
#
#
#
#
#    cd / && rm -rf /mesa
#)
#
## SDL.
#(
#    cd / && rm -rf /sdl
#    mkdir -pv /sdl
#    cd /sdl
#    wget 'https://www.libsdl.org/release/SDL2-2.0.16.tar.gz' -O SDL.tgz
#    tar -axf SDL.tgz
#    cd SDL*/
#    ./configure \
#        --prefix=/pot \
#        --enable-static \
#        --disable-shared \
#        \
#        --enable-video-x11 \
#        --enable-video-x11-xinput \
#        --disable-video \
#        --disable-haptic \
#        --disable-sensor \
#        --disable-power \
#        --disable-filesystem \
#        --disable-threads \
#        --disable-timers \
#        --disable-file \
#        --disable-loadso \
#        --disable-cpuinfo \
#        --disable-assembly \
#        --disable-video-wayland \
#        --disable-video-wayland-qt-touch \
#        --disable-wayland-shared \
#        --disable-libdecor \
#        --disable-libdecor-shared \
#        --disable-video-rpi \
#        --disable-x11-shared \
#        --disable-video-x11-xcursor \
#        --disable-video-x11-xinerama \
#        --disable-video-x11-xrandr \
#        --disable-video-x11-scrnsaver \
#        --with-sysroot=/pot \
#        --with-x
#    time make -j8 VERBOSE=1 install
#    cd / && rm -rf /sdl
#)


# Ygor Clustering.
(
    cd /
    git clone --depth 1 https://github.com/hdclark/YgorClustering.git /ygorclustering
    cd /ygorclustering/
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBUILD_SHARED_LIBS=OFF \
        .
    cd build/
    time make -j8 VERBOSE=1 install
    rm -rf /ygorclustering
)


# Explicator.
(
    cd / && rm -rf /explicator
    git clone --depth 1 https://github.com/hdclark/Explicator.git /explicator
    cd /explicator/
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBUILD_SHARED_LIBS=OFF \
        .
    cd build/
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /explicator
    file /pot/bin/explicator_cross_verify
)


# Ygor.
(
    cd / && rm -rf /ygor
    git clone --depth 1 https://github.com/hdclark/Ygor.git /ygor
    cd /ygor
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DBOOST_INCLUDEDIR=/pot/usr/include/ \
        .
    cd build
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /ygor
    file /pot/bin/regex_tester
)


# DICOMautomaton.
(
    cd / && rm -rf /dcma
    git clone --depth 1 https://github.com/hdclark/DICOMautomaton.git /dcma
    cd /dcma

    # Drop mpfr and gmp (not needed for musl toolchains, seemingly not needed for static compilation either).
    sed -i -e 's/.*mpfr.*//g' -e 's/.*gmp.*//g' src/CMakeLists.txt 

    # Implement a bogus thread dtor routine, just to make compilation succeed (runtime be damned!)
    #
    # Note: not recommended, unless absolutely required, for obvious reasons.
    #printf '\n\n%s\n\n' 'extern "C" int __cxa_thread_atexit_impl(void (*)(), void *, void *){ return 0; }' >> src/DICOMautomaton_Dispatcher.cc

    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DDCMA_VERSION=dcma_manual_test_version \
        -DCMAKE_INSTALL_SYSCONFDIR=/pot/etc \
        -DMEMORY_CONSTRAINED_BUILD=OFF \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_ASAN=OFF \
        -DWITH_TSAN=OFF \
        -DWITH_MSAN=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_CGAL=OFF \
        -DWITH_NLOPT=OFF \
        -DWITH_SFML=OFF \
        -DWITH_WT=OFF \
        -DWITH_BOOST=ON \
        -DWITH_SDL=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_POSTGRES=OFF \
        -DWITH_JANSSON=OFF \
        -DBOOST_INCLUDEDIR=/pot/usr/include/ \
        -DBOOST_LIBRARYDIR=/pot/usr/lib/ \
        -DBOOST_ROOT=/pot/ \
        .
    cd build
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /dcma
    file /pot/bin/dicomautomaton_dispatcher
    strings /pot/bin/dicomautomaton_dispatcher | grep GLIBC | sort --version-sort | uniq || true
)

# ldd for pot ...
#apt-get install -y musl-dev
#/lib/ld-musl-x86_64.so.1 --list /pot/bin/dicomautomaton_dispatcher

