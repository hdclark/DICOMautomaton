#!/usr/bin/env bash

# This script installs DICOMautomaton. It assumes dependencies have been installed (i.e., the corresponding build_bases
# script was completed successfully).

set -eux

# This function either freshly clones a git repository, or pulls from upstream remotes.
# The return value can be used to determine whether recompilation is required.
function clone_or_pull {
    if git clone "$@" . ; then
        return 0 # Requires compilation.
    fi

    if ! git fetch ; then
        return 2 # Failure.
    fi

    if [ "$(git rev-parse HEAD)" == "$(git rev-parse '@{u}')" ] ; then
        return 1 # Compilation not required.
    fi
    git merge
}
export -f clone_or_pull

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
if clone_or_pull "https://github.com/hdclark/Ygor" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Ygor already up-to-date.\n'
fi

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
if clone_or_pull "https://github.com/hdclark/explicator" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Explicator already up-to-date.\n'
fi

# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
if clone_or_pull "https://github.com/hdclark/YgorClustering" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Ygor Clustering already up-to-date.\n'
fi

# Install DICOMautomaton.
#
# Option 1: install a binary package.
#mkdir -pv /dcma
#cd /dcma
#apt-get install --yes -f ./DICOMautomaton*deb 
#
# Option 2: clone the latest upstream commit.
#mkdir -pv /dcma
#cd /dcma
#git clone https://github.com/hdclark/DICOMautomaton .
#   ...
#
# Option 3: use the working directory.
mkdir -pv /dcma
cd /dcma
if clone_or_pull "https://github.com/hdclark/DICOMautomaton" ; then
    #sed -i -e 's@MEMORY_CONSTRAINED_BUILD=OFF@MEMORY_CONSTRAINED_BUILD=ON@' /dcma/compile_and_install.sh || true
    #sed -i -e 's@option.*WITH_WT.*ON.*@option(WITH_WT "Wt disabled" OFF)@' /dcma/CMakeLists.txt || true
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'DICOMautomaton already up-to-date.\n'
fi
