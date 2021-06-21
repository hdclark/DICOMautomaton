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
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p AreaBelow='inf' \
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
  -o PurgeContours \
    -p ROILabelRegex='.*' \
    -p AreaBelow='inf' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests inverting the ROI selection criteria.
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
  -o PurgeContours \
    -p ROILabelRegex='C' \
    -p InvertSelection=true \
    -p AreaBelow='inf' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests inverting the purge criteria logic.
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
  -o PurgeContours \
    -p ROILabelRegex='B' \
    -p InvertLogic=true \
    -p AreaBelow='-inf' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests inverting both selection criteria and the purge criteria logic.
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
  -o PurgeContours \
    -p ROILabelRegex='B' \
    -p InvertLogic=true \
    -p InvertSelection=true \
    -p AreaBelow='-inf' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests the area criteria.
printf 'Test 5\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p AreaBelow='1.0' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests the area criteria.
printf 'Test 6\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p AreaBelow='1000000.0' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 



# Tests the perimeter criteria.
printf 'Test 7\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p PerimeterBelow='1.0' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests the perimeter criteria.
printf 'Test 8\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p PerimeterBelow='1000000.0' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests the vertexcount criteria.
printf 'Test 9\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p VertexCountBelow='10.0' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "1 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests the vertexcount criteria.
printf 'Test 10\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_fuzzy_noise.dcm \
  -o ContourViaThreshold \
    -p Lower=50 \
    -p Upper=200 \
    -p SimplifyMergeAdjacent=true \
    -p ROILabel=A \
  -o PurgeContours \
    -p ROILabelRegex='A' \
    -p VertexCountBelow='1000000.0' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 

