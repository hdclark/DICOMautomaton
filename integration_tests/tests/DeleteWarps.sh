#!/usr/bin/env bash

set -eux
set -o pipefail


# Tests deleting one of many.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" -v \
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


