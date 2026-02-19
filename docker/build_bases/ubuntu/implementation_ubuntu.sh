#!/usr/bin/env bash
# shellcheck disable=SC2086
# SC2086: Double quote to prevent globbing and word splitting - intentionally disabled for package lists.

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Ubuntu system.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

export DEBIAN_FRONTEND="noninteractive"

# Use the centralized package list script (copied to /dcma_scripts by Dockerfile).
GET_PACKAGES="/dcma_scripts/get_packages.sh"

# Get packages from the centralized script.
PKGS_BUILD_TOOLS="$("${GET_PACKAGES}" --os ubuntu --tier build_tools)"
PKGS_YGOR_DEPS="$("${GET_PACKAGES}" --os ubuntu --tier ygor_deps)"
PKGS_DCMA_DEPS="$("${GET_PACKAGES}" --os ubuntu --tier dcma_deps)"
PKGS_HEADLESS="$("${GET_PACKAGES}" --os ubuntu --tier headless_rendering)"
PKGS_OPTIONAL="$("${GET_PACKAGES}" --os ubuntu --tier optional)"

retry_count=0
retry_limit=5
until \
    apt-get update --yes && \
    `# Install build dependencies ` \
    apt-get install --yes --no-install-recommends ${PKGS_BUILD_TOOLS} && \
    `# Ygor dependencies ` \
    apt-get install --yes --no-install-recommends ${PKGS_YGOR_DEPS} && \
    `# DCMA dependencies ` \
    apt-get install --yes --no-install-recommends ${PKGS_DCMA_DEPS} && \
    `# Additional dependencies for headless OpenGL rendering with SFML ` \
    apt-get install --yes --no-install-recommends ${PKGS_HEADLESS} && \
    `# Other optional dependencies ` \
    apt-get install --yes --no-install-recommends ${PKGS_OPTIONAL}
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done



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
make -j "$JOBS" VERBOSE=1
make install
make clean

#mkdir -pv /scratch_base
#cd /scratch_base
#apt-get install --yes \
#  -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb


# Install Ygor.
#
# Option 1: install a binary package.
#mkdir -pv /scratch
#cd /scratch
#apt-get install --yes -f ./Ygor*deb
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
#mkdir -pv /scratch
#cd /scratch
#apt-get install --yes -f ./Explicator*deb
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

