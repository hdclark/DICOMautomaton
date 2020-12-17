#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/mesh_cube_with_normals_binary.ply \
  "${TEST_FILES_ROOT}"/mesh_cube_with_normals.ply \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/pcloud_cube_with_normals.ply \
  "${TEST_FILES_ROOT}"/pcloud_cube_with_normals_binary.ply \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Point_Clouds loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

