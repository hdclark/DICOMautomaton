#!/usr/bin/env bash

set -eux

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
    # If we put the build directory outside of the repo root then we can more easily cache build results.
    cmake -B /dcma_build \
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
    cd /dcma_build
    time make -j8 VERBOSE=1 install
    #cd /dcma
    #git reset --hard
    #git clean -fxd
    cd / && rm -rf /dcma
    file /usr/local/bin/dicomautomaton_dispatcher
    strings /usr/local/bin/dicomautomaton_dispatcher | grep GLIBC | sort | uniq || true
)
