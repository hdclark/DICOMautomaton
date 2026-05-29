#!/usr/bin/env bash

set -eux

mkdir -pv /scratch_base /build_base

source /build_base/set_environment.sh

/build_base/build_install_other_dependencies.sh

/build_base/build_install_dcma.sh

