#!/bin/bash

# This script will clone the pwd and then remotely build.
# The build artifacts are retained, but not packaged or installed.
# This script is most useful for testing builds.

USER_AT_REMOTE="$@"  # e.g., user@machine
BUILD_DIR="/tmp/dcma_remote_build/" # Scratch directory on the remote, created if necessary.

rsync -avz --no-links --cvs-exclude ./ "${USER_AT_REMOTE}:${BUILD_DIR}"

set -e

ssh "${USER_AT_REMOTE}" " 
  cd '${BUILD_DIR}' && 
  mkdir -p build && 
  cd build &&
  cmake ../ && "'
  JOBS=$(nproc)
  JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) # Limit to reduce memory use.
  make -j "$JOBS"
  '

printf ' ======================================== \n'
printf ' ===    Compilation was successful.   === \n'
printf ' === Note that nothing was installed! === \n'
printf ' ======================================== \n'

#  ( rm -rf build || true ) &&

