#!/usr/bin/env bash

# This script checks all the provided URLs for reachbility.

set -eu

declare -a urls
urls=("$@")

if [[ "${#urls[*]}" -eq 0 ]] ; then
    printf 'No urls provided as arguments, reading from stdin...\n'
    while read u ; do
        urls+=("${u}")
    done
fi

set +eu

any_failed=0

for u in "${urls[@]}" ; do
    status_code="$( curl --output '/dev/null' --head --silent --retry 3 --retry-delay 15 --retry-connrefused --write-out "%{http_code}" "${u}" )"

    # Attempt to use a fallback, which sometimes seems to work when the above fails.
    if ! [[ "${status_code}" -eq 200 ||
            "${status_code}" -eq 301 ||
            "${status_code}" -eq 302 ]] ; then
        printf '    Received code %s for %s -- retrying...\n' "${status_code}" "${u}"
        status_code="$( wget -O '/dev/null' --tries 3 --timeout 15  --server-response --max-redirect=0 "${u}" 2>&1 | grep 'HTTP/' | awk '{print $2}' )"
    fi

    printf '    Received code %s for %s\n' "${status_code}" "${u}"

    if ! [[ "${status_code}" -eq 200 ||
            "${status_code}" -eq 301 ||
            "${status_code}" -eq 302 ]] ; then
        printf -- '        ^^^ not reachable ^^^\n'
        any_failed=1
    fi
done


if ! [[ "${any_failed}" -eq 0 ]] ; then
    printf '    !!! One or more URLs were not reachable. !!! \n'
    exit 1
fi

