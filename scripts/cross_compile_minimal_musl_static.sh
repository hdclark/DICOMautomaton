#!/usr/bin/env bash

# This script:
#  1. generates a musl-based cross-compiler,
#  2. compiles a few minimal dependencies for DICOMautomaton, and
#  3. compiles statically-linked DICOMautomaton binaries.
#
# Ideally, this script would be able to compile all dependencies.
# At the moment only a minimum set of dependecies are supported.
# Ideas and suggestions are welcome!

# sudo docker run --rm -it debian:buster

set -eux

export CMAKE_BUILD_TYPE="Release"
export BUILD_SHARED_LIBS="OFF"
export CUSTOM_FLAGS=""

####################### Musl-cross-make targets ######################
# See https://github.com/richfelker/musl-cross-make README (musl cross-compiler).
#export TRIPLET='arm-linux-musleabi'

##################### Zig cross-compiler targets #####################
# See `zig target` (libc section) or 'https://ziglang.org/documentation/master/#Targets' 
# for other supported triplets using the zig toolchain.

#### Working ####

# Debian buster as of 20210914.
#export TRIPLET='x86_64-linux-gnu.2.28'

# Ancient glibc... Seems to have __cxa_thread_atexit_impl@GLIBC_2.18 whereas 2.17 does not have __cxa_thread_atexit_impl
export TRIPLET='x86_64-linux-gnu.2.18'

#### Not working ####

# ld.lld: error: undefined symbol: __cxa_thread_atexit_impl (see workaround below, but not a great workaround)
# Ensure CMake build type is Release because gcrt0 (i.e., compiler '-g' option) not available for musl.
#export TRIPLET='x86_64-linux-musl'
#export CMAKE_BUILD_TYPE="Release"
#export TRIPLET='aarch64-linux-musl'
#export TRIPLET="arm-linux-musleabi -mcpu=arm710t -D__libcpp_signed_lock_free=__cxx_contention_t -D__libcpp_unsigned_lock_free=__cxx_contention_t"

#export TRIPLET="arm-linux-musleabihf"
#export CUSTOM_FLAGS="-mcpu=cortex-a53" # -DATOMIC_CHAR_LOCK_FREE=2 -m32" # _LIBCPP_ATOMIC=1

# Foesn't actually static link libc. (Why?)
#export TRIPLET='aarch64-linux-musl.1.1.24'
#export CUSTOM_FLAGS="-mcpu=cortex_a53 -fno-use-cxa-atexit" # -D__cxa_thread_atexit_impl=int8_t"

# Boost iostreams compilation error:
# In file included from libs/serialization/src/xml_wgrammar.cpp:19:
# ...
# /zig*/lib/libcxx/include/atomic:2796:16: error: use of undeclared identifier '__libcpp_signed_lock_free'
# This is a bug. See https://github.com/ziglang/zig/issues/6573 for discussion.
#export TRIPLET='arm-linux-musleabi'
#export CMAKE_BUILD_TYPE="Release"

# zlib: "Failed to find a pointer-size integer type."
# (Need to confirm that this target is supported locally, because this seems like a toolchain issue...)
#export TRIPLET='arm-linux-android'

# Threads not supported, which boost.thread dislikes. Workaround by disabling boost.thread. Ygor OK without threads
# (surprisingly!) Stuck on wasm.ld rejecting non-relocatable libraries. According to
# 'https://emscripten.org/docs/compiling/WebAssembly.html?highlight=wasm' this might be due to gnu-strip removing the
# indices. (Not sure how to inhibit -- change to Debug releases?)
# But might possibly work with additional effort?
#export TRIPLET='wasm32-wasi-musl'
#export CMAKE_BUILD_TYPE="Debug"
## Webassembly extras (needed??)
#export CUSTOM_FLAGS="${CUSTOM_FLAGS} -Wl,--relocatable -Wl,--export-all -Wl,--allow-undefined" # -sUSE_PTHREADS=0" # Doesn't seem to help...
#export CFLAGS="${CUSTOM_FLAGS} -Wl,--relocatable -Wl,--export-all -Wl,--allow-undefined" # -sUSE_PTHREADS=0" # Doesn't seem to help...

# zlib: "Failed to find a pointer-size integer type."
# Ygor: fstream, array, dirent.h, iostream, stdlib.h, ... not found (because freestanding).
# Seems unlikely to work without significant porting effort...
#export TRIPLET='wasm32-freestanding-musl'

# Have to remove Threads from all CMakeLists.txt. Does not provide utf-8 wrappers.
#export TRIPLET='x86_64-windows-gnu'

# DCMA linking issues. Linker can't seem to resolve any symbols from Structs.o or Imebra_Shim.o when building
# libimebrashim.so (dynamic libs).
# 
# Static linking:
#   make[1]: *** [CMakeFiles/Makefile2:211: examples/CMakeFiles/explicator_cross_verify.dir/all] Error 2
#   error: InvalidCharacter
# not sure exactly what this means. Maybe it refers to in-source literals??
#export TRIPLET='x86_64-macos-gnu'

######################################################################

# Make static binaries.
if [ "${BUILD_SHARED_LIBS}" != "ON" ] ; then
    export CUSTOM_FLAGS="${CUSTOM_FLAGS} -static" # -Wl,--whole-archive -Wl,--no-as-needed"
fi

: | apt-get update -y
: | apt-get upgrade -y 
: | apt-get install -y clang wget xz-utils cmake git file vim rsync openssh-client \
                       python3 unzip #python3-pip pkg-config autoconf

# Zig-based toolchain.
cd / && rm -rf /zig*
#wget 'https://ziglang.org/builds/zig-linux-x86_64-0.9.0-dev.952+0c091feb5.tar.xz'
wget 'https://ziglang.org/builds/zig-linux-x86_64-0.9.0-dev.1083+9fa723ee5.tar.xz'
tar -axf zig*.tar.xz 
cd /zig*/
unset CC CXX AR RANLIB CFLAGS CXXFLAGS LDFLAGS
export CC="$(pwd)/zig cc --verbose --target=${TRIPLET}"
export CXX="$(pwd)/zig c++ --verbose --target=${TRIPLET}"
export AR="$(pwd)/zig ar"
export RANLIB="$(pwd)/zig ranlib"
export CFLAGS="-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ -fno-use-cxa-atexit ${CUSTOM_FLAGS}"
export CXXFLAGS="-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ -fno-use-cxa-atexit ${CUSTOM_FLAGS}"
export LDFLAGS=""
cd /
/zig*/zig targets

## Musl (linux-only) cross-compiler.
#cd / && rm -rf /musl* /mcm
#git clone 'https://github.com/richfelker/musl-cross-make' /mcm
#cd /mcm
#unset CC CXX AR RANLIB CFLAGS CXXFLAGS LDFLAGS
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
#export CFLAGS="-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ ${CUSTOM_FLAGS}"
#export CXXFLAGS="-I/pot/include/ -I/pot/usr/include/ -L/pot/lib/ -L/pot/usr/lib/ ${CUSTOM_FLAGS}"
#export LDFLAGS=""

# Destination for all build artifacts.
#
# Everything goes in the pot and out comes binaries -- we're doing alchemy here! :)
mkdir -pv /pot/{lib,include,usr/include,usr/lib}/


## musl time64 compatibility for 32-bit systems.
#(
#    cd / && rm -rf /musl*
#    wget 'http://musl.libc.org/releases/musl-1.2.2.tar.gz'
#    tar -axf 'musl-1.2.2.tar.gz'
#    rm 'musl-1.2.2.tar.gz'
#    cd /musl-1.2.2/compat/time32/
#    for f in ./*.c ; do
#        # Note: see 'https://github.com/richfelker/libcompat_time32'
#        $CC $CFLAGS -fPIC -D'weak_alias(a,b)=' -c $f
#        #$CC $CFLAGS -fPIC -c $f
#    done
#    $AR rc libcompat_time32.a ./*.o
#    $RANLIB libcompat_time32.a
#    mv libcompat_time32.a /pot/lib/
#    file /pot/lib/libcompat_time32.a
#
#
#    #cd / && rm -rf /libcompat*
#    #git clone --depth=1 'https://github.com/richfelker/libcompat_time32'
#    #cd /libcompat*/
#    #make -j8 VERBOSE=1 CCsrcdir=/musl-1.2.2
#    #mv libcompat_time32.a /pot/lib/
#
#    cd / && rm -rf /musl*
#    #cd / && rm -rf /libcompat*
#)
#export CC="${CC} -Wl,--whole-archive -lcompat_time32 -Wl,--no-whole-archive"
#export CXX="${CXX} -Wl,--whole-archive -lcompat_time32 -Wl,--no-whole-archive"
#export CXXFLAGS="${CXXFLAGS} -Wl,--whole-archive -lcompat_time32 -Wl,--no-whole-archive"
#export LDFLAGS="${LDFLAGS} -Wl,--whole-archive -lcompat_time32 -Wl,--no-whole-archive"


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
    wget 'https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.gz' 
    tar -axf 'boost_1_77_0.tar.gz' 
    rm 'boost_1_77_0.tar.gz' 
    cd /boost_1_77_0/tools/build/src/engine/
    printf '%s\n' "using gcc : : ${CXX} ${CXXFLAGS} : <link>static <runtime-link>static <cflags>${CFLAGS} <cxxflags>${CXXFLAGS} <linkflags>${LDFLAGS} ;" > /root/user-config.jam
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
#    ... TODO ...
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

## Un-hide 64-bit time functions for musl.
## Note: musl uses
##   #define __REDIR(x,y) __typeof__(x) x __asm__(#y)
#(
#    cd / && rm -rf /wrap64
#    mkdir -pv /wrap64 && cd /wrap64
#    wget 'https://musl.libc.org/releases/musl-1.2.2.tar.gz'
#    tar -axf musl-1.2.2.tar.gz
#
#rm -rf wrap64*
#    f=0
#    while read l ; do
#        printf '%s\n' "$l" |
#        sed -e 's/.*include[/]\([^:]*\):__REDIR[(]\([^,]*\),\([^)]*\)[)];/#if defined(__has_include)\n#if __has_include(<\1>)\n#include <\1>\n inline decltype(\2)\* \3 = \&\2;\n#endif\n#endif\n/g' \
#          > "wrap64_${f}.cc"
#        f=$((1+f))
#    done < <( find ./musl-*/ -type f -exec grep '^__REDIR' '{}' \+ | sort -u )
#
#    for f in wrap64_*.cc ; do
#        $CXX --std=c++17 "$f" -fPIC -c -o "${f}.o" -pthread || true
#    done
#    $AR rc libwrap64.a wrap64_*.o
#    $RANLIB libwrap64.a
#    cp libwrap64.a /pot/lib/
#
#    cd / && rm -rf /wrap64
#)
#export CC="${CC} -Wl,--whole-archive -lwrap64 -Wl,--no-whole-archive"
#export CXX="${CXX} -Wl,--whole-archive -lwrap64 -Wl,--no-whole-archive"
#export CXXFLAGS="${CXXFLAGS} -Wl,--whole-archive -lwrap64 -Wl,--no-whole-archive"
#export LDFLAGS="${LDFLAGS} -Wl,--whole-archive -lwrap64 -Wl,--no-whole-archive"

# Ygor Clustering.
(
    cd / && rm -rf /ygorclustering
    git clone --depth 1 https://github.com/hdclark/YgorClustering.git /ygorclustering
    cd /ygorclustering/
#        -DCMAKE_EXE_LINKER_FLAGS="-static" \
#        -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
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

        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
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

    # Changes (currently) necessary to compile with wasm (note: linking fails after changing).
    if false ; then
        printf '\n%s\n' '
            int pclose(const void *);
            int pclose(const void *, const void *);
            FILE* popen(const void *);
            FILE* popen(const void *, const void *);
            int mknod(const void *, const int, const void *); ' >> src/YgorMisc.h
        
        printf '\n%s\n' '
            int pclose(const void *){}
            int pclose(const void *, const void *){}
            FILE* popen(const void *){}
            FILE* popen(const void *, const void *){}
            int mknod(const void *, const int, const void *){} ' >> src/YgorMath.cc
        
        printf '#if 0\n%s\n#endif\n' "$(cat src/YgorNetworking.cc)" > src/YgorNetworking.cc
        printf '#if 0\n%s\n#endif\n' "$(cat src/YgorNetworking.h )" > src/YgorNetworking.h
    
        # If freestanding, also have to remove threads support.
    fi

        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
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
#
#    # arm (32bit) workarounds. Not exactly sure these are needed.
#    # Note: not recommended, unless absolutely required, for obvious reasons.
#
#printf '%s\n' '
##include <time.h>
##include <pthread.h>
#
#extern "C" {
#
#time_t __mktime64(struct tm *time){ return mktime(time); }
#struct tm *__localtime64_r( const time_t *timer, struct tm *buf ){ return localtime_r(timer, buf); }
#time_t __time64( time_t *arg ){ return time(arg); }
#int __clock_gettime64(clockid_t clock_id, struct timespec *tp){ return clock_gettime(clock_id, tp); }
#int __pthread_cond_timedwait_time64(pthread_cond_t * cond,
#                                    pthread_mutex_t * mutex,
#                                    const struct timespec * abstime){
#    return pthread_cond_timedwait(cond, mutex, abstime);
#};
#int __nanosleep_time64(const struct timespec *req, struct timespec *rem){ return nanosleep(req, rem); }
#
##include <sys/types.h>
##include <sys/stat.h>
##include <unistd.h>
#int __stat_time64(const char *path, struct stat *buf){ return stat(path, buf); }
#int __fstat_time64(int fd, struct stat *buf){ return fstat(fd, buf); }
#int __lstat_time64(const char *path, struct stat *buf){ return lstat(path, buf); }
#
##include <fcntl.h>
##include <sys/stat.h>
#int __utimensat_time64(int dirfd, const char *pathname,
#                       const struct timespec times[2], int flags){ return utimensat(dirfd, pathname, times, flags); }
#
#
#int __cxa_thread_atexit_impl(void (*)(), void *, void *){ return 0; }
#
#}
#' >> src/Structs.cc

    cmake -B build \
        -DCMAKE_INSTALL_PREFIX='/pot' \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
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
        -DWITH_THRIFT=OFF \
        -DBOOST_INCLUDEDIR=/pot/usr/include/ \
        -DBOOST_LIBRARYDIR=/pot/usr/lib/ \
        -DBOOST_ROOT=/pot/ \
        -DCMAKE_EXE_LINKER_FLAGS="-static" \
        -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        .
    cd build
    time make -j8 VERBOSE=1 install
    cd / && rm -rf /dcma
    file /pot/bin/dicomautomaton_dispatcher
    strings /pot/bin/dicomautomaton_dispatcher | grep GLIBC | sort --version-sort | uniq || true
)

# Note: ldd-equivalent for musl libc:
#apt-get install -y musl-dev
#/lib/ld-musl-x86_64.so.1 --list /pot/bin/dicomautomaton_dispatcher

