#!/bin/bash

set -eu

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/MR_continents.dcm \
  -o CopyImages:ImageSelection=first \
  -o ContourWholeImages:ImageSelection=last \
  -o ReduceNeighbourhood:ImageSelection=last \
     -p Reduction=percentile01 \
     -p MaxDistance=5 \


