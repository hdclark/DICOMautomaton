#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that BED conversion is valid when irrelevant data is provided.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='3.0' \
     -p AlphaBetaRatioLate='1E6' \
     -p EarlyROILabelRegex='Body' \
     -p PriorNumberOfFractions=35 \
     -p PriorPrescriptionDose='5E6' \
     -p TargetDosePerFraction='3E6' \
     -p Model='bed-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,116[.]' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

# Test that BED conversion is valid when irrelevant parameters use default values.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='3.0' \
     -p EarlyROILabelRegex='Body' \
     -p PriorNumberOfFractions=35 \
     -p Model='bed-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,116[.]' |
  grep .


# Test that BED conversion honours ROI selections. 
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 3\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  `# Create a contour encircling no voxels. ` \
  -o ContourViaGeometry \
     -p Shapes='sphere(109.5, 109.5, 100.0,  0.1)' \
     -p ImageSelection='first' \
     -p ROILabel='small' \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='10.0' \
     -p AlphaBetaRatioLate='3.0' \
     -p EarlyROILabelRegex='small' \
     -p PriorNumberOfFractions=35 \
     -p Model='bed-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,116[.]' |
  grep .



# Test that EQD2 conversion is valid with 2 Gy/f inputs.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 4\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='10.0' \
     -p AlphaBetaRatioLate='3.0' \
     -p EarlyROILabelRegex='Body' \
     -p PriorNumberOfFractions=35 \
     -p TargetDosePerFraction=2.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,70' |
  grep .

# Test that EQD2 conversion transforms 70/35 to 70 Gy with any alpha/beta.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 5\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='100.0' \
     -p AlphaBetaRatioLate='35.0' \
     -p EarlyROILabelRegex='Body' \
     -p PriorNumberOfFractions=35 \
     -p TargetDosePerFraction=2.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,70' |
  grep .

printf 'Test 6\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o ContourViaGeometry \
     -p Shapes='sphere(109.5, 109.5, 100.0,  0.1)' \
     -p ImageSelection='first' \
     -p ROILabel='small' \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='100.0' \
     -p AlphaBetaRatioLate='35.0' \
     -p EarlyROILabelRegex='small' \
     -p PriorNumberOfFractions=35 \
     -p TargetDosePerFraction=2.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,70' |
  grep .


# Test that EQD2 conversion is valid with 3 Gy/f inputs.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 7\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='10.0' \
     -p AlphaBetaRatioLate='3.0' \
     -p EarlyROILabelRegex='Body' \
     -p PriorNumberOfFractions=23.333333333 \
     -p TargetDosePerFraction=2.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,75[.]8' |
  grep .


printf 'Test 8\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o ContourViaGeometry \
     -p Shapes='sphere(109.5, 109.5, 100.0,  0.1)' \
     -p ImageSelection='first' \
     -p ROILabel='small' \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='10.0' \
     -p AlphaBetaRatioLate='3.0' \
     -p EarlyROILabelRegex='small' \
     -p PriorNumberOfFractions=23.333333333 \
     -p TargetDosePerFraction=2.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,84' |
  grep .


# Test that EQD5 conversion is valid with 3 Gy/f inputs.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 9\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='10.0' \
     -p AlphaBetaRatioLate='6.0' \
     -p EarlyROILabelRegex='Body' \
     -p PriorNumberOfFractions=23.333333333 \
     -p TargetDosePerFraction=5.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,60[.]6' |
  grep .


printf 'Test 10\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o ContourViaGeometry \
     -p Shapes='sphere(109.5, 109.5, 100.0,  0.1)' \
     -p ImageSelection='first' \
     -p ROILabel='small' \
  -o BEDConvert \
     -p ImageSelection='first' \
     -p AlphaBetaRatioEarly='10.0' \
     -p AlphaBetaRatioLate='6.0' \
     -p EarlyROILabelRegex='small' \
     -p PriorNumberOfFractions=23.333333333 \
     -p TargetDosePerFraction=5.0 \
     -p Model='eqdx-lq-simple' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,57[.]2' |
  grep .


