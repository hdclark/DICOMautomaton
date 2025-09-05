#!/usr/bin/env bash

# This script searches for all operations that match the provided regex for (compile-time) tags, and opens
# them in an editor.

set -eu

declare -a documents
documents=("$@")

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

if [[ "${#documents[*]}" -eq 0 ]] ; then
    printf 'No documents provided.'
    exit 0
fi

for d in "${documents[@]}" ; do
    printf 'Checking %s\n' "${d}"

    cat "${d}" |
      sed -e 's@(@(\n@' -e 's@)@)\n@' |
      sed -e 's@.*\(http[s]*://[^ \"\)\>]*\).*@\1@g' |
      grep '://' |
      grep -v '[$]' |
      sort -u |
      "${REPOROOT}"/scripts/check_url_reachability.sh
done

