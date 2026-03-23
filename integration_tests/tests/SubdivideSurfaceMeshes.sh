#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that SubdivideSurfaceMeshes subdivides a surface mesh.

printf 'Test 1: Subdivision with default parameters\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/80_face_sphere.obj \
  -o SubdivideSurfaceMeshes \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'Test 2: Subdivision with custom iterations\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/80_face_sphere.obj \
  -o SubdivideSurfaceMeshes:Iterations=1 \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
