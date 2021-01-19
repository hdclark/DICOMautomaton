#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/500_vert_heart_binary.stl \
  "${TEST_FILES_ROOT}"/icosahedron_binary.stl \
  "${TEST_FILES_ROOT}"/icosahedron.stl \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "3 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

