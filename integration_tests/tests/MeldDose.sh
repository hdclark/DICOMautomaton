#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that a single dose image will be unaltered.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o MeldDose \
  -o TestConditions \
    -p Conditions='pixel_min(0, 0.5); pixel_max(70, 0.5)'


# Test that exactly-aligned inputs sum perfectly.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o MeldDose \
  -o TestConditions \
    -p Conditions='pixel_min(0, 0.5); pixel_max(140, 0.5)'


# Test that when inputs are slightly shifted they will still more-or-less sum the same as if they were exactly aligned.
#
# Note: assumes GenerateVirtualDataDoseStairsV1 output range is [0,70] Gy.
printf 'Test 3\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateWarp \
     -p Transforms='translate(0.001, 0.001, 0.001)' \
  -o RigidWarpImages \
     -p ImageSelection=last \
     -p TransformSelection=last \
  -o MeldDose \
  -o TestConditions \
    -p Conditions='pixel_min(0, 0.5); pixel_max(140, 0.5)'


