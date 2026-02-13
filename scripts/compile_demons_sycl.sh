#!/usr/bin/env bash

# This script can be used to compile and run the SYCL-related unit tests for the Demons algorithm.
# Uses the mock SYCL implementation for CPU-only execution.
#
# Requirements:
#  - g++ with C++17 support
#  - git

set -eux

# Move to the repository root.
REPOROOT="$(git rev-parse --show-toplevel || true)"
if [ ! -d "${REPOROOT}" ] ; then
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}/../"
fi
cd "${REPOROOT}"

mkdir -p build

# Download Ygor (needed for image types).
if [ ! -d build/ygor ]; then
    git clone --depth=1 'https://github.com/hdclark/Ygor.git' build/ygor
    touch build/ygor/src/YgorDefinitions.h
fi

# Generate a minimal file to run the relevant unit tests.
cat <<EOF > build/demons_sycl.cc

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>
#include <thread>
#include <chrono>
#include <tuple>

#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <utility>
#include <csignal>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "src/doctest20251212/doctest.h"

EOF

# Compile (only) the relevant sources.
# Note: This uses Mock SYCL (no real SYCL compiler needed).
g++ \
  --std=c++17 \
  -U YGOR_USE_LINUX_SYS \
  -U YGOR_USE_EIGEN \
  -U YGOR_USE_GNU_GSL \
  -U YGOR_USE_BOOST \
  -U DCMA_USE_SYCL \
  -I"${REPOROOT}" \
  -I"${REPOROOT}"/build/ygor/src/ \
  src/Mock_SYCL_Tests.cc \
  src/Alignment_Demons_SYCL.cc \
  src/Alignment_Demons_SYCL_Tests.cc \
  build/ygor/src/Ygor*.cc \
  build/ygor/src/External/MD5/md5.cc \
  build/ygor/src/External/SpookyHash/SpookyV2.cpp \
  build/demons_sycl.cc \
  -o build/demons_sycl

# Run the tests.
./build/demons_sycl --success

