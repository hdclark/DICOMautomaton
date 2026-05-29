#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that GenerateVirtualDataPointCloudV1 creates a point cloud.

printf 'Test 1: Basic invocation\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o TestConditions \
    -p Conditions='point_cloud_count(1)'

printf 'All tests passed!\n'
