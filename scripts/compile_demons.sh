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

# Download Ygor.
rm -rf build/ygor
git clone --depth=1 'https://github.com/hdclark/Ygor.git' build/ygor
touch build/ygor/src/YgorDefinitions.h

# Generate a minimal file to run the relevant unit tests.
cat <<EOF > build/demons.cc

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
  -U YGOR_USE_LINUX_SYS \
  -U YGOR_USE_EIGEN \
  -U YGOR_USE_GNU_GSL \
  -U YGOR_USE_BOOST \
  -I"${REPOROOT}" \
  -I"${REPOROOT}"/build/ygor/src/ \
  src/Alignment*cc \
  src/Structs.cc \
  src/Tables.cc \
  src/Regex_Selectors.cc \
  src/Dose_Meld.cc \
  build/ygor/src//Ygor*.cc \
  build/ygor/src/External/{MD5/md5.cc,SpookyHash/SpookyV2.cpp} \
  build/demons.cc \
  -lgsl \
  -o build/demons

# Run the tests.
./build/demons --success


