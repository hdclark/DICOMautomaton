#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that GenerateVirtualDataLineSampleV1 creates a line sample.

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

printf 'All tests passed!\n'
