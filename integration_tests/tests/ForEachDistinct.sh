#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/3x3x3_random_positive.3ddose \
  "${TEST_FILES_ROOT}"/MR_mosaic.dcm \
  "${TEST_FILES_ROOT}"/line_sample_cumulative_dvh_absolute_volume.dat \
  \
  -o ForEachDistinct:KeysCommon='Modality' \
  -\( \
      -o GenerateVirtualDataPointCloudV1 \
  -\) \
  -o TestConditions \
    -p Conditions='point_cloud_count(3)' |
  tee -a fullstdout
