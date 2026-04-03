#!/usr/bin/env bash
set -eu
shopt -s nocasematch

# This script will attempt to build DICOMautomaton as quickly as possible, disabling optional external dependencies and
# compiler optimizations to reduce compilation time and minimize the number of external dependencies needed.
#
# It is intended to help new collaborators get started quickly.
#
# This script assumes a recent Debian-based distribution (e.g., Debian stable or Ubuntu LTS). The author's upstream
# support libraries (Ygor, YgorClustering, and Explicator) are fetched automatically via CMake FetchContent.
#
# Currently supported distributions: Debian, Ubuntu, and generic (i.e., if the necessary dependencies are pre-installed).


# The temporary location in which to build.
BUILDROOT="/tmp/dcma_build"

ALSOINSTALL="yes" # install the built binaries.

CLEANBUILD="no" # purge existing (cached) build artifacts before building.

INSTALLPREFIX="/usr"
INSTALLVIASUDO="yes" # whether to use sudo during installation.

# Argument parsing:
OPTIND=1 # Reset in case getopts has been used previously in the shell.
while getopts "b:i:unch" opt; do
    case "$opt" in
    b)  BUILDROOT=$(realpath "$OPTARG")
        printf 'Proceeding with user-specified build root "%s".\n' "${BUILDROOT}"
        ;;
    i)  INSTALLPREFIX="$OPTARG"
        printf 'Proceeding with user-specified installation prefix directory "%s".\n' "${INSTALLPREFIX}"
        ;;
    u)  INSTALLVIASUDO="no"
        printf 'Disabling use of sudo.\n'
        ;;
    n)  ALSOINSTALL="no"
        printf 'Disabling installation; building only.\n'
        ;;
    c)  CLEANBUILD="yes"
        printf 'Purging cached build artifacts for clean build.\n'
        ;;
    h|*)
        printf 'This script attempts to build DICOMautomaton as quickly as possible, disabling'
        printf ' optional external dependencies and compiler optimizations.\n'
        printf "\n"
        printf "Usage: \n\t $0 <args> \n"
        printf "\n"
        printf "Available arguments: \n"
        printf "\n"
        printf " -h       : Display usage information and terminate.\n"
        printf "\n"
        printf " -b <dir> : The location to use as the build root; build artifacts are cached here.\n"
        printf "          : Default: '%s'\n" "${BUILDROOT}"
        printf "\n"
        printf " -i <dir> : The location to use as the installation prefix.\n"
        printf "          : Default: '%s'\n" "${INSTALLPREFIX}"
        printf "\n"
        printf " -u       : Attempt unprivileged installation (i.e., without using sudo).\n"
        printf "          : Default: sudo will be used: '%s'\n" "${INSTALLVIASUDO}"
        printf "\n"
        printf " -n       : No-install; build, but do not install.\n"
        printf "          : Default: binaries will be installed: '%s'\n" "${ALSOINSTALL}"
        printf "\n"
        printf " -c       : Clean-build; purge any existing build artifacts before building.\n"
        printf "          : Default: '%s'\n" "${CLEANBUILD}"
        printf "\n"
        exit 0
        ;;
    esac
done

if [ -z "${BUILDROOT}" ] ; then
    # Proceeding with an empty build root can be dangerous, so refuse to do so.
    printf 'Empty build root directory. Refusing to continue.\n' 1>&2
    exit 1
elif [ "$(readlink -f "${BUILDROOT}")" == "/" ] ; then
    # Proceeding with build root at '/' will destroy the filesystem if the 'clean' option is used.
    printf 'Build root resolves to "/". Refusing to continue.\n' 1>&2
    exit 1
fi


# Move to the repository root.
REPOROOT="$(git rev-parse --show-toplevel || true)"
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
export DCMA_VERSION="$(./scripts/extract_dcma_version.sh)"

# Determine how we will escalate privileges.
SUDO="sudo"
if [[ $EUID -eq 0 ]] ; then
    SUDO="" # no privileges needed.
fi
if [ "${INSTALLVIASUDO}" != "yes" ] ; then
    SUDO=""
fi
#################

# Copy the repository to the build location to eliminate possibility of destroying the repo.
mkdir -p "${BUILDROOT}"
if [ -d "${BUILDROOT}"/.git ] ; then
    printf 'Build root directory contains a .git subdirectory. Refusing to overwrite it.\n' 1>&2
    exit 1
fi
if [[ "${CLEANBUILD}" =~ ^y.* ]] ; then
    $SUDO find "${BUILDROOT}/" -type f -exec chmod 644 '{}' \+
    $SUDO find "${BUILDROOT}/" -type d -exec chmod 755 '{}' \+
    $SUDO find "${BUILDROOT}/" -exec chown "$( id -n -u ):$( id -n -g )" '{}' \+
    rsync -rptv --delete --no-links --exclude="$(realpath --relative-to=. "$BUILDROOT")" --cvs-exclude ./ "${BUILDROOT}/"
else
    rsync -rptv          --no-links --exclude="$(realpath --relative-to=. "$BUILDROOT")" --cvs-exclude ./ "${BUILDROOT}/"
fi
cd "${BUILDROOT}"

printf 'Building with minimal dependencies for fastest compilation...\n'

mkdir -p build
cd build
cmake \
  -DDCMA_VERSION="${DCMA_VERSION}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALLPREFIX}" \
  -DCMAKE_INSTALL_SYSCONFDIR="/etc" \
  -DCMAKE_BUILD_TYPE=Quick \
  -DCMAKE_CXX_FLAGS="-O0 -DNDEBUG" \
  -DMEMORY_CONSTRAINED_BUILD=OFF \
  -DWITH_ASAN=OFF \
  -DWITH_TSAN=OFF \
  -DWITH_MSAN=OFF \
  -DWITH_EIGEN=OFF \
  -DWITH_CGAL=OFF \
  -DWITH_NLOPT=OFF \
  -DWITH_SFML=OFF \
  -DWITH_SDL=OFF \
  -DWITH_WT=OFF \
  -DWITH_GNU_GSL=OFF \
  -DWITH_POSTGRES=OFF \
  -DWITH_JANSSON=OFF \
  -DWITH_THRIFT=OFF \
  -DWITH_EXT_SYCL=OFF \
  -DWITH_FETCHCONTENT_FALLBACK=ON \
  ../
JOBS=$(nproc)
JOBS=$(( JOBS < 8 ? JOBS : 8 )) # Limit to reduce memory use.
make -j "$JOBS" VERBOSE=1

if [[ "${ALSOINSTALL}" =~ ^y.* ]] ; then
    printf 'Warning! Bypassing system package management and installing directly!\n'
    $SUDO make install
fi

compile_ret_val=$?
printf 'Done.\n'
exit $compile_ret_val
