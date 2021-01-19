#!/usr/bin/env bash

set -eux
set -o pipefail


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/320_face_sphere.obj \
  "${TEST_FILES_ROOT}"/80_face_sphere.obj \
  "${TEST_FILES_ROOT}"/icosahedron.obj \
  "${TEST_FILES_ROOT}"/5_nested_spheres.obj \
  "${TEST_FILES_ROOT}"/dodecahedron.obj \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "5 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/square.obj \
  "${TEST_FILES_ROOT}"/twisted_rhombus.obj \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Point_Clouds loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

