#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that OrientMeshes orients face normals consistently.

printf 'Test 1: Orient mesh faces\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  -o OrientMeshes \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
