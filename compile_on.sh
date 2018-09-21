#!/bin/bash

# This script will clone the pwd and then remotely build.
# The build artifacts are retained, but not packaged or installed.
# This script is most useful for testing builds.

USER_AT_REMOTE="$@"  # e.g., user@machine
BUILD_DIR="/tmp/build/" # Scratch directory on the remote, created if necessary.

rsync -avz --no-links --cvs-exclude ./ "${USER_AT_REMOTE}:${BUILD_DIR}"

set -e

ssh "${USER_AT_REMOTE}" " 
  cd '${BUILD_DIR}' && 
  ( rm -rf build || true ) &&
  mkdir build && 
  cd build &&
  cmake ../ &&
  make -j 8 
  " 

