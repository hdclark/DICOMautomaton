#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton on Alpine Linux.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

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
  wget 

apk add --no-cache \
    ` # Ygor dependencies ` \
    gsl-static gsl-dev \
    eigen-dev \
    ` # DICOMautomaton dependencies ` \
    openssl-libs-static \
    zlib-static zlib-dev \
    sfml-dev \
    sdl2-dev sdl-static \
    glew-dev \
    jansson-dev \
    patchelf \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    xorg-server-dev \
    glu-dev \
    xf86-video-dummy

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


# Remove these to trim custom boost build deps.
apk del --no-cache bzip2 xz icu 


export JOBS=$(nproc)
export JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))

export CMAKE_BUILD_TYPE="Release" # musl doesn't support gcrt1.o, which is needed for gcc's "-pg" option.
export BUILD_SHARED_LIBS="OFF"

export CUSTOM_FLAGS=""
if [ "${BUILD_SHARED_LIBS}" != "ON" ] ; then
    export CUSTOM_FLAGS="${CUSTOM_FLAGS} -static -DBoost_USE_STATIC_LIBS=ON -fPIC" # -Wl,--whole-archive -Wl,--no-as-needed"
fi

unset CC CXX AR RANLIB CFLAGS CXXFLAGS LDFLAGS
export CC="gcc"
export CXX="g++"
export AR="ar"
export RANLIB="ranlib"
export CFLAGS="${CUSTOM_FLAGS}"
export CXXFLAGS="${CUSTOM_FLAGS}"
export LDFLAGS=""

# Note: it might be possible to add this, but it wasn't specifically needed and didn't work on the first try...
#
## Install Wt from source to get around outdated and buggy Debian package.
##
## Note: Add additional dependencies if functionality is needed -- this is a basic install.
##
## Note: Could also install build-deps for the distribution packages, but the dependencies are not
##       guaranteed to be stable (e.g., major version bumps).
#(
#    cd / && rm -rf /wt*
#    git clone https://github.com/emweb/wt.git /wt
#    cd /wt
#    mkdir -p build && cd build
#    CXXFLAGS="-static -fPIC" \
#    cmake \
#      -DCMAKE_INSTALL_PREFIX=/usr \
#      -DBUILD_SHARED=OFF \
#      -DBoost_USE_STATIC_LIBS=ON \
#      ../
#    make -j "$JOBS" VERBOSE=0
#    make install
#    make clean
#)

# boost.
(
    cd / && rm -rf /boost*
    wget 'https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.gz' 
    tar -axf 'boost_1_77_0.tar.gz' 
    rm 'boost_1_77_0.tar.gz' 
    cd /boost_1_77_0/tools/build/src/engine/
    printf '%s\n' "using gcc : : ${CXX} ${CXXFLAGS} : <link>static <runtime-link>static <cflags>${CFLAGS} <cxxflags>${CXXFLAGS} <linkflags>${LDFLAGS} ;" > /user-config.jam
    printf '%s\n' "using zlib : : <search>/usr/lib <name>zlib <include>/use/include ;" >> /user-config.jam
    ./build.sh --cxx="g++"
    cd ../../
    ./bootstrap.sh --cxx="g++"
    ./b2 --prefix=/ install
    cd ../../
    ./tools/build/src/engine/b2 -j8 \
        --user-config=/user-config.jam \
        --prefix=/ \
        release \
        toolset=gcc \
        debug-symbols=off \
        threading=multi \
        runtime-link=static \
        link=static \
        cflags='-fno-use-cxa-atexit -fno-strict-aliasing' \
        --includedir=/usr/include \
        --libdir=/usr/lib \
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
        -sZLIB_INCLUDE=/usr/include/ \
        -sZLIB_LIBRARY_PATH=/usr/lib/ \
        install
    ls -lash /usr/lib
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
    cp -v -R include/asio/ include/asio.hpp /usr/include/
    cd / && rm -rf /asio
)

# Ygor Clustering.
(
    cd / && rm -rf /ygorclustering
    git clone --depth 1 https://github.com/hdclark/YgorClustering.git /ygorclustering
    cd /ygorclustering/
#        -DCMAKE_EXE_LINKER_FLAGS="-static" \
#        -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
#        -DCMAKE_INSTALL_PREFIX='/pot' \
    cmake -B build \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        .
    cd build/
    time make -j "$JOBS"  VERBOSE=1 install
    git reset --hard
    git clean -fxd :/ 
    #rm -rf /ygorclustering
)

# Explicator.
(
    cd / && rm -rf /explicator
    git clone --depth 1 https://github.com/hdclark/Explicator.git /explicator
    cd /explicator/

        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        #-DCMAKE_INSTALL_PREFIX='/pot' \
    cmake -B build \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        .
    cd build/
    time make -j8 VERBOSE=1 install
    git reset --hard
    git clean -fxd :/ 
    #cd / && rm -rf /explicator
    file /usr/local/bin/explicator_cross_verify
)

# Ygor.
(
    cd / && rm -rf /ygor
    git clone --depth 1 https://github.com/hdclark/Ygor.git /ygor
    cd /ygor

        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        #-DCMAKE_INSTALL_PREFIX='/pot' \
        #-DBOOST_INCLUDEDIR=/pot/usr/include/ \
    cmake -B build \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_GNU_GSL=OFF \
        -DWITH_BOOST=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        .
    cd build
    time make -j8 VERBOSE=1 install
    git reset --hard
    git clean -fxd :/ 
    #cd / && rm -rf /ygor
    file /usr/local/bin/regex_tester
)

# DICOMautomaton.
(
    cd / && rm -rf /dcma
    git clone --depth 1 https://github.com/hdclark/DICOMautomaton.git /dcma
    cd /dcma

    # Drop mpfr and gmp (not needed for musl toolchains, seemingly not needed for static compilation either).
    sed -i -e 's/.*mpfr.*//g' -e 's/.*gmp.*//g' src/CMakeLists.txt 

        #-DDCMA_VERSION=dcma_manual_test_version \
        #-DCMAKE_INSTALL_PREFIX='/pot' \
        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        #-DBOOST_INCLUDEDIR=/pot/usr/include/ \
        #-DBOOST_LIBRARYDIR=/pot/usr/lib/ \
        #-DBOOST_ROOT=/pot/ \
    cmake -B build \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DCMAKE_INSTALL_SYSCONFDIR=/etc \
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
        -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        -DBoost_USE_STATIC_LIBS=ON \
        -DBoost_USE_STATIC_RUNTIME=ON \
        .
    cd build
    time make -j8 VERBOSE=1 install
    git reset --hard
    git clean -fxd :/ 
    #cd / && rm -rf /dcma
    file /usr/local/bin/dicomautomaton_dispatcher
    strings /usr/local/bin/dicomautomaton_dispatcher | grep GLIBC | sort --version-sort | uniq || true
)

