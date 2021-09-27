#!/usr/bin/env bash

# Move to the repository root.
REPOROOT=$(git rev-parse --show-toplevel || true)
if [ ! -d "${REPOROOT}" ] ; then
    printf 'Unable to find git repo root. Refusing to continue.\n' 1>&2
    exit 1
fi
cd "${REPOROOT}"

( ./docker/build_bases/debian_oldstable/build.sh 2>&1 && 
  ./docker/builders/debian_oldstable/build.sh    2>&1 ) | tee /tmp/dcma_docker_debian_oldstable.log

( ./docker/build_bases/arch/build.sh          2>&1 &&
  ./docker/builders/arch/build.sh             2>&1 ) | tee /tmp/dcma_docker_arch.log

( ./docker/build_bases/void/build.sh          2>&1 &&
  ./docker/builders/void/build.sh             2>&1 ) | tee /tmp/dcma_docker_void.log

( ./docker/build_bases/mxe/build.sh           2>&1 &&
  ./docker/builders/mxe/build.sh              2>&1 ) | tee /tmp/dcma_docker_mxe.log

( ./docker/build_bases/alpine/build_armv7.sh  2>&1 &&
  ./docker/builders/alpine/build_armv7.sh     2>&1 ) | tee /tmp/dcma_docker_alpine_armv7.log

( ./docker/build_bases/alpine/build_aarch64.sh  2>&1 &&
  ./docker/builders/alpine/build_aarch64.sh     2>&1 ) | tee /tmp/dcma_docker_alpine_aarch64.log

( ./docker/build_bases/alpine/build_x86_64.sh  2>&1 &&
  ./docker/builders/alpine/build_x86_64.sh     2>&1 ) | tee /tmp/dcma_docker_alpine_x86_64.log

./docker/builders/ci/build.sh  2>&1 | tee /tmp/dcma_docker_ci.log

./docker/fuzz_testing/build.sh 2>&1 | tee /tmp/dcma_docker_fuzz_testing.log

grep '^Successfully built' \
  /tmp/dcma_docker_debian_oldstable.log \
  /tmp/dcma_docker_arch.log \
  /tmp/dcma_docker_void.log \
  /tmp/dcma_docker_mxe.log \
  /tmp/dcma_docker_alpine_armv7.log \
  /tmp/dcma_docker_alpine_aarch64.log \
  /tmp/dcma_docker_alpine_x86_64.log \
  /tmp/dcma_docker_ci.log \
  /tmp/dcma_docker_fuzz_testing.log

