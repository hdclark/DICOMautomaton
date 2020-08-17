#!/bin/bash

# This script rebuilds DCMA inside the Debian:stable-based DCMA Docker image.
# It then gathers all dependencies using linuxdeploy and creates an AppImage.
# Note that the end-user's glibc version must be equivalent or newer than the
# Docker image glibc. At the moment, Debian:oldstable is not new enough to
# compile C++17, so we are stuck with the glibc in Debian:stable.

set -eu

repo_root="$(git rev-parse --show-toplevel)"

create_sh=$(mktemp '/tmp/dcma_extract_executable_internal_XXXXXXXXX.sh')

trap "{ rm '${create_sh}' ; }" EXIT # Clean-up the temp file when we exit.

printf \
'#!/bin/bash

set -eu

/implementation_mxe.sh

rsync -avP /out/ /passage/Windows/

' > "${create_sh}"
chmod 777 "${create_sh}"

sudo docker run -it --rm \
    --network=host \
    -v "${repo_root}/docker/builders/mxe/implementation_mxe.sh":/implementation_mxe.sh \
    -v "$(pwd)":/passage/:rw \
    -v "${create_sh}":/start/create.sh:ro \
    -w /passage/ \
    dicomautomaton_mxe_cross_compiler:latest \
    /start/create.sh 

sudo chown -R $(id -u -n):$(id -g -n) ./Windows

