#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" \
  "${TEST_FILES_ROOT}"/opposing_short_cylinders_with_outlier.xyz \
  "${TEST_FILES_ROOT}"/short_cylinder_with_outlier.xyz \
  "${TEST_FILES_ROOT}"/twisted_rhombus.xyz \
  \
  -o TestConditions \
    -p Conditions='point_cloud_count(3)'

