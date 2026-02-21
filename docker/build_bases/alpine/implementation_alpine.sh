#!/usr/bin/env bash
# shellcheck disable=SC2086
# SC2086: Double quote to prevent globbing and word splitting - intentionally disabled for package lists.

# This script installs all dependencies needed to build DICOMautomaton on Alpine Linux.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

source /scratch_base/set_environment.sh

# Use the centralized package list script (copied to /dcma_scripts by Dockerfile).
GET_PACKAGES="/dcma_scripts/get_packages.sh"

# Get packages from the centralized script.
PKGS_BUILD_TOOLS="$("${GET_PACKAGES}" --os alpine --tier build_tools)"
PKGS_DEVELOPMENT="$("${GET_PACKAGES}" --os alpine --tier development)"
PKGS_YGOR_DEPS="$("${GET_PACKAGES}" --os alpine --tier ygor_deps)"
PKGS_DCMA_DEPS="$("${GET_PACKAGES}" --os alpine --tier dcma_deps)"

retry_count=0
retry_limit=5
until
    apk update && \
    `# Install build dependencies ` \
    apk add --no-cache ${PKGS_BUILD_TOOLS} && \
    `# Development tools ` \
    apk add --no-cache ${PKGS_DEVELOPMENT} && \
    `# Ygor dependencies` \
    apk add --no-cache ${PKGS_YGOR_DEPS} && \
    `# DCMA dependencies` \
    apk add --no-cache ${PKGS_DCMA_DEPS}
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

# Omitted (for now):
#    cgal-dev \
#    libpqxx-devel \
#    postgresql-dev \
#    nlopt-devel \
#    freefont-ttf \
#    gnuplot \
#    zenity  \
#    bash-completion \
#    xorg-video-drivers \
#    xorg-apps

# Intentionally omitted:
#    boost1.76-static boost1.76-dev \
#    asio-dev \
# Note: these libraries seem to be either shared, or have a shared runtime, which CMake dislikes when compiling static DCMA.

# Removed:
#    sdl-static      (Seems to have disappeared during SDL3 release.)

/scratch_base/build_install_customized_system_dependencies.sh

# Building the following are not necessary, since a later stage will re-build everything anyway. However, it's useful to
# do some computation up-front and possibly cache the results.
/scratch_base/build_install_other_dependencies.sh
#/scratch_base/build_install_dcma.sh

