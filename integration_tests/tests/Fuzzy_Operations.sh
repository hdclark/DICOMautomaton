#!/usr/bin/env bash

# This script tests whether fuzzy operation name matching is enabled.

set -eux
set -o pipefail


# The following operation names are close to valid names, but vary slightly.
# They should be accepted, ideally with a warning.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o ContoursViaThreshold \
    -p Lower=75 \
    -p Upper=185 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=B \
  -o contourviathreshold \
    -p Lower=100 \
    -p Upper=125 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=C \
  -o contour_via_threshold \
    -p Lower=10 \
    -p Upper=225 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=D \
  -o TestConditions \
    -p Conditions='contour_collection_count(4)'


# The following tests operation aliases and fuzzy matching for aliases.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o ConvertImagesToContours \
    -p Lower=75 \
    -p Upper=185 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=B \
  -o ConvertImageToContour \
    -p Lower=100 \
    -p Upper=125 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=C \
  -o TestConditions \
    -p Conditions='contour_collection_count(3)'


# The following operation is not close in name to any legitimate operations.
# DCMA should reject it as being invalid.
printf 'Test 3\n' |
  tee -a fullstdout
! \
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o NonExistentOperationName


