#!/bin/bash

set -eu

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  \
  -o ContourWholeImages:ImageSelection=last \
  \
  -o CopyImages:ImageSelection=first \
  -o ReduceNeighbourhood:ImageSelection=last \
     -p Reduction=percentile01 \
     -p MaxDistance=5 \
  \
  -o CopyImages:ImageSelection=first \
  -o ReduceNeighbourhood:ImageSelection=last \
     -p Reduction=standardize \
     -p MaxDistance=5 \

