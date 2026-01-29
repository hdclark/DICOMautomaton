#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that ConvertPointsToMeshes creates a surface mesh from a point cloud.

printf 'Test 1: Basic invocation with default cube width (expand method)\n' |
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

printf 'Test 2: Invocation with custom cube width (expand method)\n' |
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

printf 'Test 3: Explicit expand method invocation\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o ConvertPointsToMeshes:Method=expand \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Surface_Mesh' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 4: Convex hull method invocation\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o ConvertPointsToMeshes:Method=convexhull \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Surface_Mesh' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'Test 5: Convex hull with abbreviated method name\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o ConvertPointsToMeshes:Method=convex \
  -o DroverDebug |
  tee -a fullstdout |
  grep 'Surface_Mesh' |
  `# Note: ensures the output stream is not empty. ` \
  grep . 

printf 'All tests passed!\n'
