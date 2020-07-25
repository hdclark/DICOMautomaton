#!/bin/bash

set -eu

# The (base) name of the container being built. The base name will be augmented with additional tags (see below).
if [ ! -v image_name ] ; then # Allows environment variable override.
    image_name="dicomautomaton_webserver_debian_stable"
fi

# The image to use as the base system. It should have up-to-date dependencies installed.
#
# Note that this name can be prefixed with registry details and postfixed with tags, e.g., 'hdclark/some_name:latest'.
if [ ! -v base_image_name ] ; then # Allows environment variable override.
    base_image_name="dcma_build_base_debian_stable"
fi

commit_id="$(git rev-parse HEAD)"
build_datetime="$(date '+%Y%m%_0d-%_0H%_0M%_0S')"
clean_dirty="clean"
sstat="$(git diff --shortstat)"
if [ ! -z "${sstat}" ] ; then
    clean_dirty="dirty"
fi

repo_root="$(git rev-parse --show-toplevel)"
cd "${repo_root}"

time \
sudo \
docker build \
    --network=host \
    --no-cache=true \
    --build-arg "BASE_CONTAINER_NAME=${base_image_name}" \
    --tag "${image_name}":"built_${build_datetime}" \
    --tag "${image_name}":"commit_${commit_id}_${clean_dirty}" \
    --tag "${image_name}":latest \
    --file docker/builders/debian_stable/Dockerfile \
    .

