#!/usr/bin/env bash

set -eux

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/3x3x3_random_positive.3ddose \
  "${TEST_FILES_ROOT}"/MR_mosaic.dcm \
  "${TEST_FILES_ROOT}"/line_sample_cumulative_dvh_absolute_volume.dat \
  \
  -o ForEachDistinct:KeysCommon='Modality' \
  -\( \
      -o DroverDebug \
  -\) |
  grep "Performing operation 'DroverDebug' now" | 
  wc -l |
  grep 3 | 
  `# Ensure the output stream is not empty. ` \
  grep .

