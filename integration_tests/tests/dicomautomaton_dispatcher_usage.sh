#!/usr/bin/env bash

set -eux

"${DCMA_BIN}" -u > /dev/null

"${DCMA_BIN}" --detailed-usage > /dev/null

