#!/usr/bin/env bash

set -eux
set -o pipefail

# Tests typical threshold contouring operations.
printf 'Test 1\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
 -o ContourViaThreshold:ROILabel='10,200 binary':Lower=10:Upper=200:Method=binary:SimplifyMergeAdjacent=true \
 -o ContourViaThreshold:ROILabel='10,200 marching squares':Lower=10:Upper=200:Method=marching-squares \
 -x ContourViaThreshold:ROILabel='10,200 marching cubes':Lower=10:Upper=200:Method=marching-cubes \
 \
 -o ContourViaThreshold:ROILabel='20tile,50tile binary':Lower=20tile:Upper=80tile:Method=binary:SimplifyMergeAdjacent=true \
 -o ContourViaThreshold:ROILabel='20tile,50tile marching squares':Lower=20tile:Upper=80tile:Method=marching-squares \
 -x ContourViaThreshold:ROILabel='20tile,50tile marching cubes':Lower=20tile:Upper=80tile:Method=marching-cubes \
 \
 -o ContourViaThreshold:ROILabel='-inf,200 binary':Lower=-inf:Upper=200:Method=binary:SimplifyMergeAdjacent=true \
 -o ContourViaThreshold:ROILabel='-inf,200 marching squares':Lower=-inf:Upper=200:Method=marching-squares \
 -x ContourViaThreshold:ROILabel='-inf,200 marching cubes':Lower=-inf:Upper=200:Method=marching-cubes \
 \
 -o ContourViaThreshold:ROILabel='20%,inf binary':Lower='20%':Upper=inf:Method=binary:SimplifyMergeAdjacent=true \
 -o ContourViaThreshold:ROILabel='20%,inf marching squares':Lower='20%':Upper=inf:Method=marching-squares \
 -x ContourViaThreshold:ROILabel='20%,inf marching cubes':Lower='20%':Upper=inf:Method=marching-cubes \
 \
 -o DroverDebug | 
  tee -a fullstdout |
  grep -i "8 contour_collections loaded" | 
  `# Note: ensures the output stream is not empty. ` \
  grep . 

