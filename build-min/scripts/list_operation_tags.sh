#!/usr/bin/env bash

# This script lists all (compile-time) tags listed by operations.

set -eu

# Move to the repository root.
REPOROOT="$(git rev-parse --show-toplevel || true)"
if [ ! -d "${REPOROOT}" ] ; then

    # Fall-back on the source position of this script.
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}/../"
fi
cd "${REPOROOT}"

#########################

grep 'tags[.]emplace' src/Operations/*cc |
  sed -e 's/.*[.]cc://' |
  sed -e 's/[^"]*["]\([^"]*\)["].*/\1/' |
  sort |
  uniq -c |
  sort --human-numeric-sort --reverse

