#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that ConvertPointsToMeshes creates a surface mesh from a point cloud.

printf 'Test 1: Basic invocation with default cube width\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o ConvertPointsToMeshes \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Surface_Mesh' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 2: Invocation with custom cube width\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o ConvertPointsToMeshes:CubeWidth=2.0 \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Surface_Mesh' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 3: Multiple point clouds\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o GenerateVirtualDataPointCloudV1 \
  -o ConvertPointsToMeshes:PointSelection=all \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Surface_Mesh' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'All tests passed!\n'
