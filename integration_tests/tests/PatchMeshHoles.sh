#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that PatchMeshHoles detects and fills holes in surface meshes.

printf 'Test 1: Hole patching on a closed mesh (should be a no-op)\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  -o PatchMeshHoles \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
