#!/bin/bash

# This script installs DICOMautomaton. It assumes dependencies have been installed (i.e., the corresponding build_bases
# script was completed successfully).

set -eux

# Install DICOMautomaton.

# Option 1: install a binary package.
#mkdir -pv /dcma
#cd /dcma
#apt-get install --yes -f ./DICOMautomaton*deb 

# Option 2: clone the latest upstream commit.
#mkdir -pv /dcma
#cd /dcma
#git clone https://github.com/hdclark/DICOMautomaton .
#...

# Option 3: assume a pristine repo is waiting at /dcma.
cd /dcma
./compile_and_install.sh -b build
git reset --hard
git clean -fxd :/ 

