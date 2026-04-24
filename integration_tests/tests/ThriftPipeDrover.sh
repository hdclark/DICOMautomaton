#!/usr/bin/env bash

set -eux
set -o pipefail

function skip_test {
    echo "Apache Thrift support is not available. Skipping test..."
    exit 0
}
"${DCMA_BIN}" -u | \
  grep 'ImportDrover' | \
  grep . || skip_test

printf 'Test 1: Round-trip Drover through stdout/stdin pipeline\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 |
  tee piped_drover.ts_dcma |
  "${DCMA_BIN}" \
    -o TestConditions \
      -p Conditions='point_cloud_count(1)'
test -s piped_drover.ts_dcma

printf 'Test 2: Load Apache Thrift Drover content through the normal file loader\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  > redirected_drover.ts_dcma
test -s redirected_drover.ts_dcma
"${DCMA_BIN}" \
  redirected_drover.ts_dcma \
  -o TestConditions \
    -p Conditions='point_cloud_count(1)'

printf 'All tests passed!\n'
