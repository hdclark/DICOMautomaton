#!/usr/bin/env bash

# This script can be used to compile a limited subset of DICOMautomaton code, which is useful for quick turnaround
# debugging.
#
# Requirements:
#  - cmake
#  - g++
#  - git
#  - gsl

set -eux

# Move to the repository root.
REPOROOT="$(git rev-parse --show-toplevel || true)"
if [ ! -d "${REPOROOT}" ] ; then

    # Fall-back on the source position of this script.
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}/../"
fi
cd "${REPOROOT}"

mkdir -p build

# Generate a minimal file to run the relevant unit tests.
cat <<EOF > build/sycl.cc

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

//int main(int argc, char **argv){
//    doctest::Context context;
//    auto r = context.run();
//    std::exit(r);
//    return 0;
//}

EOF

# Compile (only) the relevant sources.
g++ \
  --std=c++17 \
  -I"${REPOROOT}" \
  src/SYCL_Fallback_Tests.cc \
  build/sycl.cc \
  -o build/sycl

# Run the tests.
./build/sycl --success


