#!/usr/bin/env bash

set -eux
set -o pipefail


# Tests deleting one of many.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o ContourViaThreshold \
    -p Lower=75 \
    -p Upper=185 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=B \
  -o ContourViaThreshold \
    -p Lower=100 \
    -p Upper=125 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=C \
  -o DeleteContours \
    -p ROILabelRegex='A' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests deleting many.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o ContourViaThreshold \
    -p Lower=75 \
    -p Upper=185 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=B \
  -o ContourViaThreshold \
    -p Lower=100 \
    -p Upper=125 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=C \
  -o DeleteContours \
    -p ROILabelRegex='.*' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests deleting a single contour (one-at-a-time), but then using the contour storage afterward.
printf 'Test 3\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o ContourViaThreshold \
    -p Lower=75 \
    -p Upper=185 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=B \
  -o ContourViaThreshold \
    -p Lower=100 \
    -p Upper=125 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=C \
  -o ForEachDistinct:KeysCommon='ROIName' \
  -\( \
      -o DeleteContours \
        -p ROILabelRegex='.*' \
  -\) \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests selectivity of the delete operation. Should not delete contours that are not selected.
printf 'Test 4\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o ContourViaThreshold \
    -p Lower=75 \
    -p Upper=185 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=B \
  -o ContourViaThreshold \
    -p Lower=100 \
    -p Upper=125 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=C \
  -o ForEachDistinct:KeysCommon='ROIName' \
  -\( \
      -o DeleteContours \
        -p ROILabelRegex='A' \
  -\) \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


