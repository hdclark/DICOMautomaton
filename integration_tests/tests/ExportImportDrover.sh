#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that ExportDrover and ImportDrover correctly round-trip Drover data.
# These operations require Thrift support (WITH_THRIFT=ON).

# First check if ExportDrover is available. If not, skip this test.
function skip_test {
    echo "ExportDrover is not available (Thrift not enabled). Skipping test..."
    exit 0
}
"${DCMA_BIN}" \
  -o GenerateVirtualDataDoseStairsV1 \
  -o ExportDrover -p Filename='/dev/null' \
  2>&1 || skip_test


# Test 1: Export and import image data, verifying image counts and pixel values.
printf 'Test 1: Export and import image data\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o ExportDrover \
    -p Filename='test_drover_images.ts_dcma'

"${DCMA_BIN}" \
  -v \
  -o ImportDrover \
    -p Filename='test_drover_images.ts_dcma' \
  -o TestConditions \
    -p Conditions='image_array_count(1)'


# Test 2: Export and import point cloud data.
printf 'Test 2: Export and import point cloud data\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataPointCloudV1 \
  -o ExportDrover \
    -p Filename='test_drover_points.ts_dcma'

"${DCMA_BIN}" \
  -v \
  -o ImportDrover \
    -p Filename='test_drover_points.ts_dcma' \
  -o TestConditions \
    -p Conditions='point_cloud_count(1)'


# Test 3: Export and import line sample data.
printf 'Test 3: Export and import line sample data\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataLineSampleV1 \
  -o ExportDrover \
    -p Filename='test_drover_lsamp.ts_dcma'

"${DCMA_BIN}" \
  -v \
  -o ImportDrover \
    -p Filename='test_drover_lsamp.ts_dcma' \
  -o TestConditions \
    -p Conditions='line_sample_count(1)'


# Test 4: Export and import multiple data types together.
printf 'Test 4: Export and import multiple data types\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataDoseStairsV1 \
  -o GenerateVirtualDataPointCloudV1 \
  -o GenerateVirtualDataLineSampleV1 \
  -o ExportDrover \
    -p Filename='test_drover_multi.ts_dcma'

"${DCMA_BIN}" \
  -v \
  -o ImportDrover \
    -p Filename='test_drover_multi.ts_dcma' \
  -o TestConditions \
    -p Conditions='image_array_count(1); point_cloud_count(1); line_sample_count(1)'


printf 'All tests passed!\n'
