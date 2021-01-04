#!/usr/bin/env bash

set -eux
set -o pipefail


# Test that null-transformed inputs remain unmodified.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='translate(0.0, 0.0, 0.0) ; rotate(109.5, 109.5, 100.0,  0.0, 0.0, 1.0,  0.0) ; scale(109.5, 109.5, 100.0,  1.0)' \
  -o WarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,140' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Test that multiple translational transforms will round-trip.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='translate( 10.0, 0.0, 0.0 ); translate( 5.0, 10.0, 0.0 ); translate( 0.0, 5.0, 10.0 ); translate( 0.0, 0.0, 5.0 ); translate( -15.0, -15.0, -15.0 )' \
  -o WarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,140' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Test that rotational transforms are accurate by flipping 180 deg so the inputs interlock and compensate each other.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
# Note: assumes GenerateVirtualDataDoseStairsV1 output image is centred at (109.5, 109.5, 100.0).
printf 'Test 3\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='rotate(109.5, 109.5, 100.0,  0.0, 0.0, 1.0,  3.14159265)' \
  -o WarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '70,70' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Test that rotational transforms are accurate by flipping 360 deg so the inputs sum as if no transform were performed.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
# Note: assumes GenerateVirtualDataDoseStairsV1 output image is centred at (109.5, 109.5, 100.0).
printf 'Test 4\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='rotate(109.5, 109.5, 100.0,  0.0, 0.0, 1.0,  6.2831853)' \
  -o WarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,140' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 5\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='rotate(109.5, 109.5, 100.0,  0.0, 0.0, 1.0,  3.14159265); rotate(109.5, 109.5, 100.0,  0.0, 0.0, -1.0,  -3.14159265)' \
  -o WarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,140' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Test that scale transforms round-trip.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
# Note: assumes GenerateVirtualDataDoseStairsV1 output image is centred at (109.5, 109.5, 100.0).
printf 'Test 6\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='scale(0.0, 0.0, 0.0, 2.0) ; scale(0.0, 0.0, 0.0, 0.5)' \
  -o WarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'pixel value range' |
  grep '0,140' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 


