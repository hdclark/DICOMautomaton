#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that RemeshSurfaceMeshes remeshes a surface mesh.

printf 'Test 1: Basic remeshing with default parameters\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/80_face_sphere.obj \
  -o RemeshSurfaceMeshes \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'Test 2: Remeshing with custom iterations and target edge length\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  "${TEST_FILES_ROOT}"/80_face_sphere.obj \
  -o RemeshSurfaceMeshes:Iterations=3:TargetEdgeLength=2.0 \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'

printf 'All tests passed!\n'
