#!/usr/bin/env bash

set -eux
set -o pipefail

# Hard to test this. Ideally would load lots of different files and diff verbose
# debug dumps, but I don't want to rely on an external diff.
#
# This test will at least ensure that NoOp works in the absence of all data.

"${DCMA_BIN}" \
  -v \
  \
  -o NoOp \
  -o Repeat:N=100 \
  -\( \
      -o NoOp \
  -\)

