#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  \
  -o ExportSurfaceMeshesOFF \
    -p Variant=ascii \
    -p Filename="test.off"

test -s "test.off" || { echo "Error: OFF export did not produce a non-empty test.off file." >&2; exit 1; }
head -n 1 "test.off" | grep -Eq '^OFF$' || { echo "Error: test.off is missing OFF header." >&2; exit 1; }


# Ensure the files can be read.
"${DCMA_BIN}" \
  "test.off" \
  \
  -o TestConditions \
    -p Conditions='surface_mesh_count(1)'
