#!/usr/bin/env bash

set -eux

# This file will be sourced by both build_base and builders to ensure consistency.
export JOBS=$(nproc)
export JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))

export CMAKE_BUILD_TYPE="Release" # musl doesn't support gcrt1.o, which is needed for gcc's "-pg" option.
export BUILD_SHARED_LIBS="OFF"

export CUSTOM_FLAGS=""
if [ "${BUILD_SHARED_LIBS}" != "ON" ] ; then
    export CUSTOM_FLAGS="${CUSTOM_FLAGS} -static -DBoost_USE_STATIC_LIBS=ON -fPIC" # -Wl,--whole-archive -Wl,--no-as-needed"
fi

unset CC CXX AR RANLIB CFLAGS CXXFLAGS LDFLAGS
export CC="clang"
export CXX="clang++"
export AR="ar"
export RANLIB="ranlib"
export CFLAGS="${CUSTOM_FLAGS}"
export CXXFLAGS="${CUSTOM_FLAGS}"
export CXXFLAGS="${CXXFLAGS} -Wno-enum-constexpr-conversion"  # Needed for a Boost MPL bug with clang.
export LDFLAGS=""

