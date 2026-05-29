#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that MakeMeshesConvex computes the convex hull of a surface mesh.

printf 'Test 1: Convex hull with default parameters\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  -o MakeMeshesConvex \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
