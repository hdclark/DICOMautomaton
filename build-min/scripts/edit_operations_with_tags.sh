#!/usr/bin/env bash

# This script searches for all operations that match the provided regex for (compile-time) tags, and opens
# them in an editor.

set -eux

tags_regex="$*"

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

declare -a matching_ops

set +eux
for f in src/Operations/*cc ; do
    x=$( grep 'tags[.]emplace' "$f" | sed -e 's/.*[.]cc://' | grep -iE "${tags_regex}" )
    if [ ! -z "${x}" ] ; then
        matching_ops+=("${f}")
    fi
done

printf 'Found %s matching files:\n'  "${#matching_ops[*]}"
for f in "${matching_ops[@]}" ; do
    printf '    %s\n' "${f}"
done

printf '\nPress enter to edit...\n'
read

vim "${matching_ops[@]}"

