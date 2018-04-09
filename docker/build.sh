#!/bin/bash

commit_id=$(git rev-parse HEAD)

clean_dirty="clean"
sstat=$(git diff --shortstat)
if [ ! -z "${sstat}" ] ; then
    clean_dirty="dirty"
fi

build_datetime=$(date '+%Y%m%_0d-%_0H%_0M%_0S')

time sudo docker build \
    --no-cache=true \
    -t dicomautomaton_webserver:"built_${build_datetime}" \
    -t dicomautomaton_webserver:"commit_${commit_id}_${clean_dirty}" \
    -t dicomautomaton_webserver:latest \
    .

