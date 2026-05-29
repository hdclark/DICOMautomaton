#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/500_vert_heart_binary.stl \
  "${TEST_FILES_ROOT}"/icosahedron_binary.stl \
  "${TEST_FILES_ROOT}"/icosahedron.stl \
  \
  -o TestConditions \
    -p Conditions='surface_mesh_count(3)'

