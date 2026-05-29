#!/usr/bin/env bash

# This script tests the CellularAutomata operation.

set -eux
set -o pipefail


# Simulate directly.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o ContourWholeImages \
  -o CellularAutomata \
    -p Method=conway \
    -p Low=0 \
    -p High=255 \
    -p Iterations=50 \
  -o ContourViaThreshold \
    -p ROILabel=remaining \
    -p Lower=128 \
    -p Method=marching-squares \
  -o TestConditions \
    -p ROILabelRegex='remaining' \
    -p Conditions='contour_count(314)'


# Simulate in smaller batches.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o ContourWholeImages \
  -o Repeat:N=5 \
  -\( \
      -o CellularAutomata \
        -p Method=conway \
        -p Low=0 \
        -p High=255 \
        -p Iterations=10 \
  -\) \
  -o ContourViaThreshold \
    -p ROILabel=remaining \
    -p Lower=128 \
    -p Method=marching-squares \
  -o TestConditions \
    -p ROILabelRegex='remaining' \
    -p Conditions='contour_count(314)'

