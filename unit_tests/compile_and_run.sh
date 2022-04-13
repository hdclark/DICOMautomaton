#!/usr/bin/env bash

set -eu

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

# Move to script location.
cd "unit_tests/"

# Ensure the doctest header is available.
if [ ! -f doctest/doctest.h ] ; then
    mkdir -p doctest/
    wget -q 'https://raw.githubusercontent.com/onqtam/doctest/master/doctest/doctest.h' -O doctest/doctest.h
fi

g++ -std=c++17 -Wall -I. -I"${REPOROOT}/src" \
  Main.cc \
  {,"${REPOROOT}/src/"}Alignment_TPSRPM.cc \
  -o run_tests \
  -pthread \
  -lboost_system \
  -lygor

./run_tests #--success

rm -rf doctest/
rm ./run_tests

