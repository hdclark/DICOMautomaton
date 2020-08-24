#!/usr/bin/env bash

set -eu

"${DCMA_BIN}" -u > /dev/null

"${DCMA_BIN}" --detailed-usage > /dev/null

