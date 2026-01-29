#!/usr/bin/env bash

set -eux
set -o pipefail


# Tests deleting one of many.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -o GenerateWarp \
    -p Transforms='translate(1.0, 0.0, 0.0)' \
  -o GenerateWarp \
    -p Transforms='translate(0.0, 1.0, 0.0)' \
  -o GenerateWarp \
    -p Transforms='translate(0.0, 0.0, 1.0)' \
  -o DeleteWarps \
    -p TransformSelection='first' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Transform3s loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests deleting many.
printf 'Test 2\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -o GenerateWarp \
    -p Transforms='translate(1.0, 0.0, 0.0)' \
  -o GenerateWarp \
    -p Transforms='translate(0.0, 1.0, 0.0)' \
  -o GenerateWarp \
    -p Transforms='translate(0.0, 0.0, 1.0)' \
  -o DeleteWarps \
    -p TransformSelection='.*' \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "0 Transform3s loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


# Tests deleting the last warp (default).
printf 'Test 3\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -o GenerateWarp \
    -p Transforms='translate(1.0, 0.0, 0.0)' \
  -o GenerateWarp \
    -p Transforms='translate(0.0, 1.0, 0.0)' \
  -o GenerateWarp \
    -p Transforms='translate(0.0, 0.0, 1.0)' \
  -o DeleteWarps \
  -o DroverDebug |
  tee -a fullstdout |
  grep -i "2 Transform3s loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 


