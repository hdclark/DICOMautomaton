#!/usr/bin/env bash

set -eux

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

# Remove these to trim custom boost build deps.
apk del --no-cache bzip2 xz icu 

# boost.
(
    cd / && rm -rf /boost* || true
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
    ./tools/build/src/engine/b2 -j"$JOBS" \
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
    cd / && rm -rf /boost* || true
)

apk add --no-cache bzip2 xz unzip

# Asio.
(
    cd / && rm -rf /asio || true
    mkdir -pv /asio
    cd /asio
    wget 'https://sourceforge.net/projects/asio/files/latest/download' -O asio.tgz
    tar -axf asio.tgz || unzip asio.tgz
    cd asio-*/
    cp -v -R include/asio/ include/asio.hpp /usr/include/
    cd / && rm -rf /asio || true
)

