#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that images can be combined.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  "${TEST_FILES_ROOT}"/MR_distorted_grid.dcm \
  "${TEST_FILES_ROOT}"/MR_distributed_squares.dcm \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  \
  -o GroupImages:KeysCommon='Modality' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 Image_Arrays loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .


# Test that images can be exploded.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  "${TEST_FILES_ROOT}"/MR_distorted_grid.dcm \
  "${TEST_FILES_ROOT}"/MR_distributed_squares.dcm \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  \
  -o GroupImages:KeysCommon='Filename' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "4 Image_Arrays loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .


# Test that images can be combined and exploded without loss.
# Also test the 'PartitionImages' alias.
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  "${TEST_FILES_ROOT}"/MR_distorted_grid.dcm \
  "${TEST_FILES_ROOT}"/MR_distributed_squares.dcm \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  \
  -o PartitionImages:KeysCommon='Filename' \
  -o PartitionImages:KeysCommon='Modality' \
  -o GroupImages:KeysCommon='Filename' \
  -o GroupImages:KeysCommon='Modality' \
  -o PartitionImages:KeysCommon='Filename' \
  -o PartitionImages:KeysCommon='Modality' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 Image_Arrays loaded" | 
  `# Ensure the output stream is not empty. ` \
  grep .

