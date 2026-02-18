#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Void Linux system.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

# Source the centralized package list script.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GET_PACKAGES="${SCRIPT_DIR}/../../../scripts/get_packages.sh"

# Get packages from the centralized script.
PKGS_BUILD_TOOLS="$("${GET_PACKAGES}" --os void --tier build_tools)"
PKGS_YGOR_DEPS="$("${GET_PACKAGES}" --os void --tier ygor_deps)"
PKGS_DCMA_DEPS="$("${GET_PACKAGES}" --os void --tier dcma_deps)"
PKGS_HEADLESS="$("${GET_PACKAGES}" --os void --tier headless_rendering)"
PKGS_OPTIONAL="$("${GET_PACKAGES}" --os void --tier optional)"

retry_count=0
retry_limit=5
until 
    # Install build dependencies.
    xbps-install -y -Su xbps && \
    xbps-install -y -Su && \
    # shellcheck disable=SC2086
    xbps-install -y -S ${PKGS_BUILD_TOOLS} && \
    # shellcheck disable=SC2086
    xbps-install -y -S ${PKGS_YGOR_DEPS} ${PKGS_DCMA_DEPS} ${PKGS_HEADLESS} ${PKGS_OPTIONAL}
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf


# Install Wt from source to get around outdated and buggy Debian package.
#
# Note: Add additional dependencies if functionality is needed -- this is a basic install.
#
# Note: Could also install build-deps for the distribution packages, but the dependencies are not
#       guaranteed to be stable (e.g., major version bumps).
mkdir -pv /wt
cd /wt
git clone https://github.com/emweb/wt.git .
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ../
JOBS=$(nproc)
JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))
make -j "$JOBS" VERBOSE=0
make install
make clean


# Install Ygor.
#
# Option 1: install a binary package.
# ...
#
# Option 2: clone the latest upstream commit.
mkdir -pv /ygor
cd /ygor
git clone https://github.com/hdclark/Ygor .
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 

# Install Explicator.
#
# Option 1: install a binary package.
# ...
#
# Option 2: clone the latest upstream commit.
mkdir -pv /explicator
cd /explicator
git clone https://github.com/hdclark/explicator .
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 


# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
git clone https://github.com/hdclark/YgorClustering .
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 

