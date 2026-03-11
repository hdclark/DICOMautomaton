#!/usr/bin/env bash

set -eux
set -o pipefail

# Test that GenerateVirtualDataLineSampleV1 creates a line sample.

printf 'Test 1: Basic invocation\n' |
  tee -a fullstdout
"${DCMA_BIN}" \
  -v \
  -o GenerateVirtualDataLineSampleV1 \
  -o TestConditions \
    -p Conditions='line_sample_count(1)'

printf 'All tests passed!\n'
