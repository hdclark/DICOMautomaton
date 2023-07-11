#!/usr/bin/env bash

GIT_HASH="$(git describe --match=should_never_match --always --abbrev=1000 --dirty)"
if [ -z "${GIT_HASH}" ] ; then
    GIT_HASH="unknown"
fi

TIMESTAMP="$(date '+%Y%m%d')"
if [ "${#TIMESTAMP}" != "8" ] ; then
    TIMESTAMP="$(date '+%Y%m%_0d')"
fi
if [ -z "${TIMESTAMP}" ] ; then
    TIMESTAMP="unknown"
fi

printf 'git_%s_built_%s' "${GIT_HASH}" "${TIMESTAMP}"

