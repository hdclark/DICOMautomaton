#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  \
  -o ExportSurfaceMeshesOFF \
    -p Variant=ascii \
    -p Filename="test.off"


# Ensure the files can be read.
"${DCMA_BIN}" \
  "test.off" \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

