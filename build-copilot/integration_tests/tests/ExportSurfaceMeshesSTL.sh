#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/500_vert_heart_binary.stl \
  \
  -o ExportSurfaceMeshesSTL \
    -p Variant=ascii \
    -p Filename="test.stl" \
  \
  -o ExportSurfaceMeshesSTL \
    -p Variant=binary \
    -p Filename="test_binary.stl" \


# Ensure the files can be read.
"${DCMA_BIN}" \
  "test.stl" \
  "test_binary.stl" \
  \
  -o TestConditions \
    -p Conditions='surface_mesh_count(2)'

