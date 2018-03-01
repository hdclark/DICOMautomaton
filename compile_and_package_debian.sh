#!/bin/bash
set -e

BUILDDIR="/home/hal/Builds/DICOMautomaton/"
BUILTPKGSDIR="/home/hal/Builds/"

mkdir -p "${BUILDDIR}"
rsync -avz --no-links --cvs-exclude ./ "${BUILDDIR}"

pushd .
cd "${BUILDDIR}"
[ -f CMakeCache.txt ] || cmake . -DCMAKE_INSTALL_PREFIX=/usr
make -j 4 && make package
mv *.deb "${BUILTPKGSDIR}/"
popd

