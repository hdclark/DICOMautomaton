#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/mesh_cube_with_normals.ply \
  \
  -o ExportSurfaceMeshesPLY \
    -p Variant=ascii \
    -p Filename="test.ply" \
  \
  -o ExportSurfaceMeshesPLY \
    -p Variant=binary \
    -p Filename="test_binary.ply" \


# Ensure the files can be read.
"${DCMA_BIN}" \
  "test.ply" \
  "test_binary.ply" \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

