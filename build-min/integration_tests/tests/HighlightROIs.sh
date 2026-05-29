#!/usr/bin/env bash

# This script tests the HighlightROIs operation.

set -eux
set -o pipefail


# Ensure the ContourViaThreshold baseline hasn't changed.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o CropImages \
    -p ImageSelection=first \
    -p RowsL='10%' \
    -p RowsH='80%' \
    -p ColumnsL='10%' \
    -p ColumnsH='80%' \
    -p DICOMMargin=0.0 \
  -o ContourViaThreshold \
    -p Lower=20 \
    -p Upper=200 \
    -p Method=marching-squares \
    -p ROILabel=roi \
  -o TestConditions \
    -p ROILabelRegex='roi' \
    -p Conditions='contour_count(7)'


# Ensure we can convert from contour -> image -> contour without significant loss of detail.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o CropImages \
    -p ImageSelection=first \
    -p RowsL='10%' \
    -p RowsH='80%' \
    -p ColumnsL='10%' \
    -p ColumnsH='80%' \
    -p DICOMMargin=0.0 \
  -o ContourViaThreshold \
    -p Lower=20 \
    -p Upper=200 \
    -p Method=marching-squares \
    -p ROILabel=roi \
  -o CopyImages:ImageSelection=first \
  -o HighlightROIs \
    -p ImageSelection=last \
    -p ContourOverlap=overlap_cancel \
    -p Method=receding-squares \
    -p ROILabelRegex='roi' \
    -p ExteriorOverwrite=true \
  -o ContourViaThreshold \
    -p Lower=0.5 \
    -p ImageSelection=last \
    -p ROILabel='overlap cancel (receding)' \
    -p Method=marching-squares \
  -o DeleteContours \
    -p ROILabelRegex='roi' \
  -o TestConditions \
    -p Conditions='contour_count(7)'


printf 'Test 3\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o CropImages \
    -p ImageSelection=first \
    -p RowsL='10%' \
    -p RowsH='80%' \
    -p ColumnsL='10%' \
    -p ColumnsH='80%' \
    -p DICOMMargin=0.0 \
  -o ContourViaThreshold \
    -p Lower=20 \
    -p Upper=200 \
    -p Method=marching-squares \
    -p ROILabel=roi \
  -o CopyImages:ImageSelection=first \
  -o HighlightROIs \
    -p ImageSelection=last \
    -p ContourOverlap=honour_opps \
    -p Method=receding-squares \
    -p ROILabelRegex='roi' \
    -p ExteriorOverwrite=true \
  -o ContourViaThreshold \
    -p Lower=0.5 \
    -p ImageSelection=last \
    -p ROILabel='honour opps (receding)' \
    -p Method=marching-squares \
  -o DeleteContours \
    -p ROILabelRegex='roi' \
  -o TestConditions \
    -p Conditions='contour_count(7)'


printf 'Test 4\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o CropImages \
    -p ImageSelection=first \
    -p RowsL='10%' \
    -p RowsH='80%' \
    -p ColumnsL='10%' \
    -p ColumnsH='80%' \
    -p DICOMMargin=0.0 \
  -o ContourViaThreshold \
    -p Lower=20 \
    -p Upper=200 \
    -p Method=marching-squares \
    -p ROILabel=roi \
  -o CopyImages:ImageSelection=first \
  -o HighlightROIs \
    -p ImageSelection=last \
    -p ContourOverlap=honour_opps \
    -p Method=binary \
    -p SimplifyMergeAdjacent=true \
    -p ROILabelRegex='roi' \
    -p ExteriorOverwrite=true \
  -o ContourViaThreshold \
    -p Lower=0.5 \
    -p ImageSelection=last \
    -p ROILabel='honour opps (binary)' \
    -p Method=marching-squares \
  -o DeleteContours \
    -p ROILabelRegex='roi' \
  -o TestConditions \
    -p Conditions='contour_count(6)'


