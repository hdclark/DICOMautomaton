#!/usr/bin/env bash

set -e 

base_name="dcma_build_base_arch"

commit_id=$(git rev-parse HEAD)

clean_dirty="clean"
sstat=$(git diff --shortstat)
if [ ! -z "${sstat}" ] ; then
    clean_dirty="dirty"
fi

build_datetime=$(date '+%Y%m%_0d-%_0H%_0M%_0S')

reporoot=$(git rev-parse --show-toplevel)
cd "${reporoot}"

time sudo docker build \
    --network=host \
    --no-cache=true \
    -t "${base_name}":"built_${build_datetime}" \
    -t "${base_name}":"commit_${commit_id}_${clean_dirty}" \
    -t "${base_name}":latest \
    -f docker/build_bases/arch/Dockerfile \
    .

