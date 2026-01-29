#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that GenerateSyntheticDataPointCloudV1 creates a point cloud.

printf 'Test 1: Basic invocation\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateSyntheticDataPointCloudV1 \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Point_Cloud' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'All tests passed!\n'
