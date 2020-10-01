#!/usr/bin/env bash
set -eu
shopt -s nocasematch

ALSOINSTALL="yes" # install the built binaries.
CLEANBUILD="yes" # purge existing (cached) build artifacts before building.
INSTALLPREFIX="/usr"
INSTALLVIASUDO="yes" # whether to use sudo during installation.

# Move to the repository root.
REPOROOT=$(git rev-parse --show-toplevel || true) 
if [ ! -d "${REPOROOT}" ] ; then

    # Fall-back on the source position of this script.
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}"
fi
cd "${REPOROOT}"

# Move to the SYCL directory.
cd sycl/

# Determine how we will escalate privileges.
SUDO="sudo"
if [[ $EUID -eq 0 ]] ; then
    SUDO="" # no privileges needed.
fi
if [ "${INSTALLVIASUDO}" != "yes" ] ; then
    SUDO=""
fi

# Perform the build.
rm -rf build
mkdir -p build
cd build
cmake \
  -DMEMORY_CONSTRAINED_BUILD=OFF \
  -DWITH_EIGEN=ON \
  -DWITH_NLOPT=ON \
  -DCMAKE_INSTALL_PREFIX="${INSTALLPREFIX}" \
  -DCMAKE_BUILD_TYPE=Release \
  ../
JOBS=$(nproc)
JOBS=$(( $JOBS < 4 ? $JOBS : 4 )) # Limit to reduce memory use.
make -j "$JOBS" #VERBOSE=1

if [[ "${ALSOINSTALL}" =~ ^y.* ]] ; then
    printf 'Warning! Bypassing system package management and installing directly!\n'
    $SUDO make install
fi

