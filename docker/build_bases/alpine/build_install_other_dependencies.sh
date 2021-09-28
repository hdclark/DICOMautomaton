#!/usr/bin/env bash

set -eux

# Ygor Clustering.
(
    cd / && rm -rf /ygorclustering || true
    git clone --depth 1 https://github.com/hdclark/YgorClustering.git /ygorclustering
    cd /ygorclustering/
#        -DCMAKE_EXE_LINKER_FLAGS="-static" \
#        -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
#        -DCMAKE_INSTALL_PREFIX='/pot' \
    cmake -B /build_ygorclustering \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=ON \
        -DWITH_GNU_GSL=ON \
        -DWITH_BOOST=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        .
    cd /build_ygorclustering
    time make -j "$JOBS"  VERBOSE=1 install
    #cd ..
    #git reset --hard
    #git clean -fxd 
    cd / && rm -rf /ygorclustering || true
)

# Explicator.
(
    cd / && rm -rf /explicator || true
    git clone --depth 1 https://github.com/hdclark/Explicator.git /explicator
    cd /explicator/
        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        #-DCMAKE_INSTALL_PREFIX='/pot' \
    cmake -B /build_explicator \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=ON \
        -DWITH_GNU_GSL=ON \
        -DWITH_BOOST=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        .
    cd /build_explicator
    time make -j8 VERBOSE=1 install
    #cd ..
    #git reset --hard
    #git clean -fxd 
    cd / && rm -rf /explicator || true
    file /usr/local/bin/explicator_cross_verify
)

# Ygor.
(
    cd / && rm -rf /ygor || true
    git clone --depth 1 https://github.com/hdclark/Ygor.git /ygor
    cd /ygor
        #-DCMAKE_EXE_LINKER_FLAGS="-static" \
        #-DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
        #-DCMAKE_INSTALL_PREFIX='/pot' \
        #-DBOOST_INCLUDEDIR=/pot/usr/include/ \
    cmake -B /build_ygor \
        -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
        -DBUILD_SHARED_LIBS="${BUILD_SHARED_LIBS}" \
        -DWITH_LINUX_SYS=OFF \
        -DWITH_EIGEN=ON \
        -DWITH_GNU_GSL=ON \
        -DWITH_BOOST=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        .
    cd /build_ygor
    time make -j8 VERBOSE=1 install
    #cd ..
    #git reset --hard
    #git clean -fxd  
    cd / && rm -rf /ygor || true
    file /usr/local/bin/regex_tester
)

