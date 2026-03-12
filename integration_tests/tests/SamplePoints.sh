#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that SamplePoints samples points from a point cloud.

printf 'Test 1: Sample 50%% of points from generated point cloud\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o 'SamplePoints:Fraction=0.5:Seed=12345' \
  -o TestConditions \
    -p Conditions='point_cloud_count(1)'

printf 'Test 2: Sample 10%% of points\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o 'SamplePoints:Fraction=0.1:Seed=54321' \
  -o TestConditions \
    -p Conditions='point_cloud_count(1)'

printf 'All tests passed!\n'
