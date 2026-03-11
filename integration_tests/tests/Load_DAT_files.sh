#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/line_sample_cumulative_dvh_absolute_volume.dat \
  "${TEST_FILES_ROOT}"/line_sample_cumulative_dvh_relative_volume.dat \
  \
  -o TestConditions \
    -p Conditions='line_sample_count(2)'

