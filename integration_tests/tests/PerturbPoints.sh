#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that PerturbPoints perturbs points in a point cloud.

printf 'Test 1: Perturb points with width 1.0 mm\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o 'PerturbPoints:Width=1.0:Seed=12345' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Point_Cloud' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 2: Perturb points with width 5.0 mm\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o 'PerturbPoints:Width=5.0:Seed=54321' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Point_Cloud' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 3: Perturb points with zero width (no perturbation)\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o 'PerturbPoints:Width=0.0:Seed=12345' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Point_Cloud' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 4: Perturb points with random seed (negative value)\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o 'PerturbPoints:Width=2.0:Seed=-1' \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Point_Cloud' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'All tests passed!\n'
