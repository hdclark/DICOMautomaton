#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that SimplifySurfaceMeshes simplifies a surface mesh.

printf 'Test 1: Simplification using flat method\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  -o SimplifySurfaceMeshes:Method=flat \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'Test 2: Simplification using remesh method\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  -o SimplifySurfaceMeshes:Method=remesh:TargetEdgeLength=5.0 \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
