#!/usr/bin/env bash

# This script packages DCMA into an AppImage sourcing the pieces from the system installation. It then gathers all
# dependencies using linuxdeploy and creates an AppImage.
#
# Note that the end-user's glibc version must be equivalent or newer than the Docker image glibc.

set -eux

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

sloccount $(
  find ./ \
    -type f \
    \(   -iname '*cc' \
      -o -iname '*.h' \
      -o -iname '*.sh' \
      -o -iname '*.docker' \
      -o -iname '*.dscr' \
      -o -iname '*CMake*.txt' \
      -o -iname '*.md' \
      -o -iname '*.nix' \
      -o -iname '*.py' \
    \) \
    \! -iwholename '*[.]git*' \
    \! -iwholename '*documentation/*' \
    \! -iwholename '*imebra20121219*' \
    \! -iwholename '*pfd20211102*' \
    \! -iwholename '*imgui20210904*' \
    \! -iwholename '*implot20210904*' \
)


