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

    printf '    Received code %s for %s\n' "${status_code}" "${u}"

    if ! [[ "${status_code}" -eq 200 ||
            "${status_code}" -eq 301 ||
            "${status_code}" -eq 302 ]] ; then
        printf -- '        ^^^ not reachable ^^^\n' 1>&2
        any_failed=1
    fi
done


if ! [[ "${any_failed}" -eq 0 ]] ; then
    printf '    !!! One or more URLs were not reachable. !!! \n' 1>&2
    exit 1
fi

