#!/usr/bin/env bash


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

# Use thread sanitizer.
g++ --std=c++17 -Wall -I. -I"${REPOROOT}/src" -g -O2 \
  Thread_Pool.cc \
  -fsanitize=thread \
  -o test_threads_tsan \
  -pthread \
  -lygor

./test_threads_tsan

# Use Valgrind/drd.
g++ --std=c++17 -Wall -I. -I"${REPOROOT}/src" -g -O2 \
  Thread_Pool.cc \
  -fsanitize=thread \
  -o test_threads \
  -pthread \
  -lygor

./test_threads

rm ./test_threads_tsan
rm ./test_threads


