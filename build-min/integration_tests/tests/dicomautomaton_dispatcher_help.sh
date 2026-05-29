#!/usr/bin/env bash

set -eux
set -o pipefail

"${DCMA_BIN}" -h

"${DCMA_BIN}" --help

