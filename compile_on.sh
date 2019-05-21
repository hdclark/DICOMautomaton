#!/bin/bash

# This script will clone the pwd and then remotely build.
# The build artifacts are retained, but not packaged or installed.
# This script is most useful for testing builds.

USER_AT_REMOTE="$@"  # e.g., user@machine
BUILD_DIR="/tmp/dcma_remote_build/" # Scratch directory on the remote, created if necessary.
PORTABLE_BIN_DIR="/tmp/dcma_portable_build/" # Portable binaries are dumped here on the remote.
L_PORTABLE_BIN_DIR="${HOME}/portable_dcma/" # Portable binaries are dumped here locally.

rsync -avz --no-links --cvs-exclude ./ "${USER_AT_REMOTE}:${BUILD_DIR}"

set -eu

ssh "${USER_AT_REMOTE}" " 
  cd '${BUILD_DIR}' && 
  mkdir -p build && 
  cd build &&
  cmake ../ && "'
  JOBS=$(nproc)
  JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) # Limit to reduce memory use.
  make -j "$JOBS" &&
  cd '${BUILD_DIR}' &&
  ./scripts/dump_portable_dcma_bundle.sh '${PORTABLE_BIN_DIR}'
  '

rsync -avPz --no-links --cvs-exclude --delete "${USER_AT_REMOTE}:${PORTABLE_BIN_DIR}/" "${L_PORTABLE_BIN_DIR}/"

printf ' ============================================================== \n'
printf ' ===               Compilation was successful.              === \n'
printf ' === Note that nothing was installed, but portable binaries === \n'
printf ' ===   have been dumped remotely and locally for testing.   === \n'
printf ' ============================================================== \n'

#  ( rm -rf build || true ) &&

