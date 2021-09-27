#!/usr/bin/env bash

set -eux

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
    cd ..
    git reset --hard
    git clean -fxd 
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
    cd ..
    git reset --hard
    git clean -fxd 
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
    cd ..
    git reset --hard
    git clean -fxd  
    #cd / && rm -rf /ygor
    file /usr/local/bin/regex_tester
)

