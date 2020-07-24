#!/bin/bash

set -e 

base_name="dcma_build_base_arch"
image_name="dicomautomaton_webserver_arch"

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
    --build-arg "BASE_CONTAINER_NAME=${base_name}" \
    -t "${image_name}":"built_${build_datetime}" \
    -t "${image_name}":"commit_${commit_id}_${clean_dirty}" \
    -t "${image_name}":latest \
    -f docker/builders/arch/Dockerfile \
    .

