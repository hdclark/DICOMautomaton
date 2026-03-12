#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  -v \
  \
  -o Repeat:N=4 \
  -\( \
      -o LoadFiles:FileName="${TEST_FILES_ROOT}"/icosahedron.obj \
      -o LoadFiles:FileName="${TEST_FILES_ROOT}"/icosahedron.obj \
  -\) \
  -o TestConditions \
    -p Conditions='surface_mesh_count(8)'

