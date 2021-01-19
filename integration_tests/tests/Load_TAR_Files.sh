#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/tabular_dvh_plan_uncertainty.tgz \
  "${TEST_FILES_ROOT}"/tabular_dvh_purely_absolute.tgz \
  "${TEST_FILES_ROOT}"/tabular_dvh_purely_relative.tgz \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "37 Line_Samples loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/meshing_phantom_dalek.obj.tgz \
  "${TEST_FILES_ROOT}"/torus.obj.tgz \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Surface_Meshes loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .


"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_rectangular_anisovoxelvol_DCMA_logo.tgz \
  \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 Image_Arrays loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

