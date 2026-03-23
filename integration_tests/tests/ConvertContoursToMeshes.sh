#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that ConvertContoursToMeshes creates a surface mesh from contours
# using the ygor-convex-hull method.

printf 'Test 1: Ygor convex hull method\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o ContourViaThreshold:Lower=10:Upper=200:Method=binary:SimplifyMergeAdjacent=true \
  -o ConvertContoursToMeshes:Method=ygor-convex-hull \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
