#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that GenerateVirtualDataLineSampleV1 creates a line sample.
#
# This test verifies that:
# 1. The operation can be invoked without errors
# 2. A line sample is created
# 3. The line sample contains the expected metadata

printf 'Test 1: Basic invocation\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataLineSampleV1 \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Line_Sample' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

# Test that the operation creates exactly one line sample.
printf 'Test 2: Line sample count\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataLineSampleV1 \
  -o CountObjects |
  tee -a fullstdout |
  grep 'line_sample' |
  grep '1' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

# Test that we can export the line sample.
printf 'Test 3: Export line sample\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataLineSampleV1 \
  -o ExportLineSamples -p FilenameBase='linesample' |
  tee -a fullstdout

# Verify the exported file exists and is not empty.
# ExportLineSamples creates files with pattern: FilenameBase_N.dat
if ! ls linesample_*.dat 1> /dev/null 2>&1; then
  printf 'Error: No linesample_*.dat files found\n' 2>&1
  exit 1
fi
if [ ! -s linesample_*.dat ]; then
  printf 'Error: linesample_*.dat file is empty\n' 2>&1
  exit 1
fi

printf 'All tests passed!\n'
