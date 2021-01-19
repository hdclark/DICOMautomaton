#!/usr/bin/env bash

set -eu

# Find root of repository
export REPOROOT=$(git rev-parse --show-toplevel || true)
if [ ! -d "${REPOROOT}" ] ; then
    printf 'Unable to find git repo root. Refusing to continue.\n' 1>&2
    exit 1
fi

export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
if [ ! -d "${SCRIPT_DIR}" ] ; then
    printf 'Unable to find script directory. Refusing to continue.\n' 1>&2
    exit 1
fi
cd "${SCRIPT_DIR}"

# Ensure the doctest header is available.
if [ ! -f doctest/doctest.h ] ; then
    mkdir -p doctest/
    wget -q 'https://raw.githubusercontent.com/onqtam/doctest/master/doctest/doctest.h' -O doctest/doctest.h
fi

g++ -std=c++17 -Wall -I"/usr/include/eigen3" -I. -I"${REPOROOT}/registration/src" \
  Main.cc \
  {,"${REPOROOT}/registration/src/CPD_Affine.cc"} \
  {,"${REPOROOT}/registration/src/CPD_Rigid.cc"} \
  {,"${REPOROOT}/registration/src/CPD_Nonrigid.cc"} \
  {,"${REPOROOT}/registration/src/CPD_Shared.cc"} \
  {,"${REPOROOT}/registration/unit_tests/"}CPD_Tests.cc \
  -o run_tests \
  -pthread \
  -lboost_system \
  -lygor

./run_tests #--success

rm -rf doctest/
rm ./run_tests

