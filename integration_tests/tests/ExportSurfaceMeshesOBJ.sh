#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  \
  -o ExportSurfaceMeshesOBJ \
    -p Variant=ascii \
    -p Filename="test.obj"


# Ensure the files can be read.
"${DCMA_BIN}" \
  "test.obj" \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

