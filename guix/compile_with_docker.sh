#!/usr/bin/env bash
set -euo pipefail

reporoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
output_dir="${DCMA_GUIX_OUTPUT_DIR:-${reporoot}/guix/out}"
image_name="${DCMA_GUIX_DOCKER_IMAGE:-metacall/guix:latest}"

mkdir -p "${output_dir}"

docker run \
    --rm \
    --network=host \
    --entrypoint /bin/bash \
    -e DCMA_GUIX_CHANNELS_FILE=/dcma/guix/channels.scm \
    -e DCMA_GUIX_OUTPUT_DIR=/out \
    -e DCMA_GUIX_PACKAGE="${DCMA_GUIX_PACKAGE:-dicomautomaton}" \
    -e DCMA_GUIX_LINKAGE="${DCMA_GUIX_LINKAGE:-shared}" \
    -e DCMA_GUIX_C_TOOLCHAIN="${DCMA_GUIX_C_TOOLCHAIN:-gcc-toolchain@8}" \
    -v "${reporoot}":/dcma:rw \
    -v "${output_dir}":/out:rw \
    "${image_name}" \
    /dcma/guix/build_in_container.sh "$@"
