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
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

