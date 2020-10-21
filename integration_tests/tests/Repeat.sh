#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  -v \
  \
  -o Repeat:N=4 \
  -\( \
      -o LoadFiles:FileName="${TEST_FILES_ROOT}"/icosahedron.obj \
      -o LoadFiles:FileName="${TEST_FILES_ROOT}"/icosahedron.obj \
  -\) \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'There are 8 Surface_Meshes loaded' |
  `# Ensure the output stream is not empty. ` \
  grep .

