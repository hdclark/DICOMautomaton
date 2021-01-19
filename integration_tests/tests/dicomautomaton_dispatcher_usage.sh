#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" -u > /dev/null

"${DCMA_BIN}" --detailed-usage > /dev/null

