#!/usr/bin/env bash

# This script performs basic static checks of source code.

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


AMAL_FILE="all"
trap "rm '${AMAL_FILE}' ;" EXIT

find ./ \
  -type f \
  \( -iname '*.h' -o -iname '*.hpp' -o -iname '*cc' -o -iname '*cpp' \) \
  -exec sed -e 's@^@{}@g' -e 's@//.*@@g' '{}' \;  >> "${AMAL_FILE}"


(
  cat "${AMAL_FILE}" | grep --text -E '\<localtime\>[(]''|''\<asctime\>[(]''|''\<ctime\>[(]''|''\<gmtime\>[(]' || true
  cat "${AMAL_FILE}" | grep --text -E '\<strtok\>[(]' || true
  cat "${AMAL_FILE}" | grep --text -E '\<gamma\>[(]''|''\<lgamma\>[(]''|''\<lgammaf\>[(]''|''\<lgammal\>[(]' || true
  #cat "${AMAL_FILE}" | grep --text -E '\<exit\>[(]' || true
  #cat "${AMAL_FILE}" | grep --text -E '\<rand\>[(]''|''\<srand\>[(]' || true
  cat "${AMAL_FILE}" | grep --text -E '\<mbrlen\>[(]''|''\<mbsrtowcs\>[(]''|''\<mbrtowc\>[(]''|''\<wcrtomb\>[(]''|''\<wcsrtomb\>[(]' || true
  cat "${AMAL_FILE}" | grep --text -E '\<setlocale\>[(]''|''\<localeconv\>[(]' || true
  #cat "${AMAL_FILE}" | grep --text -E '\<A\>[(]'|'\<B\>[(]'|'\<C\>[(]'|'\<D\>[(]' || true
) | ( ! grep --text . ) # Checks that the input is empty.



