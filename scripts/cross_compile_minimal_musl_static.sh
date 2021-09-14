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

apt-get update -y
apt-get upgrade -y
apt-get install -y clang g\+\+ wget xz-utils cmake git file vim rsync openssh-client \
                   python3 unzip #python3-pip pkg-config autoconf

## Zig-based toolchain.
#cd
#wget 'https://ziglang.org/builds/zig-linux-x86_64-0.9.0-dev.952+0c091feb5.tar.xz'
#tar -axf zig-linux-x86_64-0.9.0-dev.952+0c091feb5.tar.xz 
#rm zig-linux-x86_64-0.9.0-dev.952+0c091feb5.tar.xz 
#cd zig-linux-x86_64-0.9.0-dev.952+0c091feb5/
#
#export CC="$(pwd)/zig cc --target=x86_64-linux-musl -static -fno-use-cxa-atexit"
#export CXX="$(pwd)/zig c++ --target=x86_64-linux-musl -static -fno-use-cxa-atexit"
#export CXXFLAGS='-I/musl/include/ -I/musl/usr/include/ -L/musl/lib/ -L/musl/usr/lib/ -static -fno-use-cxa-atexit'
#
#
## Musl gcc wrapper toolchain.
#cd
#rm -rf musl-*
#wget 'http://musl.libc.org/releases/musl-1.2.2.tar.gz'
#tar -axf 'musl-1.2.2.tar.gz'
#cd musl-1.2.2/
#unset CC CXX CXXFLAGS
#CC="clang" CXX="clang++" ./configure --prefix=/musl --disable-shared --enable-wrapper
#time make -j8 VERBOSE=1 install
##export CC="/musl/bin/musl-gcc -static"
##export CXX="/musl/bin/musl-gcc -static"
#export CC="/musl/bin/musl-clang -static"
#export CXX="/musl/bin/musl-clang -static"
#export CXXFLAGS="-I/musl/include/ -I/musl/usr/include/ -L/musl/lib/ -L/musl/usr/lib/ -static"
#rm -rf musl-*


# Musl cross-compiler.
git clone 'https://github.com/richfelker/musl-cross-make' /mcm
cd /mcm
unset CC CXX CXXFLAGS LDFLAGS
export CC="clang"
export CXX="clang++"
export LDFLAGS=""
printf '\n%s\n' 'TARGET=x86_64-linux-musl' >> config.mak
printf '\n%s\n' 'OUTPUT=/musl' >> config.mak
#printf '\n%s\n' 'GCC_VER = 10.2.0' >> config.mak
time make -j8 VERBOSE=1 install

unset CC CXX CXXFLAGS LDFLAGS
export CC="/musl/bin/x86_64-linux-musl-gcc"
export CXX="/musl/bin/x86_64-linux-musl-g++"
export CXXFLAGS="-I/musl/include/ -I/musl/usr/include/ -L/musl/lib/ -L/musl/usr/lib/ -static -fno-use-cxa-atexit"
export LDFLAGS=""


# zlib.
(
    cd / && rm -rf /zlib*
    wget 'https://zlib.net/zlib-1.2.11.tar.gz'
    tar -axf 'zlib-1.2.11.tar.gz'
    rm 'zlib-1.2.11.tar.gz'
    cd zlib-1.2.11
    ./configure --static --prefix=/musl
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
    printf '%s\n' "using zlib : 1.2.11 : <search>/musl/lib <name>zlib <include>/musl/include ;" >> /root/user-config.jam
    ./build.sh --cxx="clang++"
    cd ../../
    ./bootstrap.sh --cxx="clang++"
    ./b2 --prefix=/musl install
    cd ../../
    ./tools/build/src/engine/b2 -j8 \
        --user-config=/root/user-config.jam \
        --prefix=/musl \
        release \
        toolset=gcc \
        debug-symbols=off \
        threading=multi \
        runtime-link=static \
        link=static \
        cflags=-fno-strict-aliasing \
        --includedir=/musl/usr/include \
        --libdir=/musl/usr/lib \
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
        -sZLIB_INCLUDE=/musl/include/ \
        -sZLIB_LIBRARY_PATH=/musl/lib/ \
        install
    ls -lash /musl/usr/lib
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
    cp -v -R include/asio/ include/asio.hpp /musl/include/
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
#        --prefix=/musl \
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
#        --with-sysroot=/musl \
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
        -DCMAKE_INSTALL_PREFIX='/musl' \
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
        -DCMAKE_INSTALL_PREFIX='/musl' \
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
    file /musl/bin/explicator_cross_verify
)


# Ygor.
(
    cd / && rm -rf /ygor
    git clone --depth 1 https://github.com/hdclark/Ygor.git /ygor
    cd /ygor
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/musl' \
        -DCMAKE_BUILD_TYPE=Release \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DBOOST_INCLUDEDIR=/musl/usr/include/ \
        .
    cd build
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /ygor
    file /musl/bin/regex_tester
)


# DICOMautomaton.
(
    cd / && rm -rf /dcma
    git clone --depth 1 https://github.com/hdclark/DICOMautomaton.git /dcma
    cd /dcma
    # Drop mpfr and gmp (not needed for musl toolchains).
    sed -i -e 's/.*mpfr.*//g' -e 's/.*gmp.*//g' src/CMakeLists.txt 
    # Implement a bogus thread dtor routine, just to make compilation succeed (runtime be damned!)
    #printf '\n\n%s\n\n' 'extern "C" int __cxa_thread_atexit_impl(void (*)(), void *, void *){ return 0; }' >> src/DICOMautomaton_Dispatcher.cc
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/musl' \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DDCMA_VERSION=dcma_manual_test_version \
        -DCMAKE_INSTALL_SYSCONFDIR=/musl/etc \
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
        -DBOOST_INCLUDEDIR=/musl/usr/include/ \
        -DBOOST_LIBRARYDIR=/musl/usr/lib/ \
        -DBOOST_ROOT=/musl/ \
        .
    cd build
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /dcma
    file /musl/bin/dicomautomaton_dispatcher
)

# ldd for musl ...
#apt-get install -y musl-dev
#/lib/ld-musl-x86_64.so.1 --list /musl/bin/dicomautomaton_dispatcher

