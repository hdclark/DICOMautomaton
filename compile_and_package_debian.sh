#!/bin/bash
set -e

BUILDDIR="/home/hal/Builds/DICOMautomaton/"
BUILTPKGSDIR="/home/hal/Builds/"

mkdir -p "${BUILDDIR}"
rsync -avz --no-links --cvs-exclude ./ "${BUILDDIR}"

pushd .
cd "${BUILDDIR}"
if [ -f CMakeCache.txt ] ; then 
    touch CMakeCache.txt  # To bump CMake-defined compilation time.
else
    cmake . -DCMAKE_INSTALL_PREFIX=/usr
fi
make -j 4 && make package
mv *.deb "${BUILTPKGSDIR}/"
popd

