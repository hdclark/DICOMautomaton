#!/usr/bin/env bash

# This script can be used to validate or monitor DCMA scripts.
#
# It is useful for debugging script parsing, validation, and platform differences. This script is most effective when
# you can run it twice -- before and after making a change -- and then examining how the output differs.

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

fname="script_debugging_$(date '+%Y%m%_0d-%_0H%_0M%_0S').log"

find ./artifacts/dcma_scripts/ -type f -iname '*dscr' -print0 |
  sort -z |
   xargs -0 -I '{}' -P 1 -n 1 -r \
     bash -c "
         export YLOG_TERMINAL='function+source'
         printf '=== %s ===\n'  '{}'
         /home/hal/portable_dcma/DICOMautomaton-x86_64.AppImage \
           -v \
           -o CompileScript:Action=validate:Filename='{}' || true " bash '{}' > "${fname}" 2>&1

printf 'Outputs written to "%s".\n' "$(pwd)/${fname}"

