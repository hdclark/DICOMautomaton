#!/bin/bash
set -e

BUILDDIR="/home/hal/Builds/DICOMautomaton/"
BUILTPKGSDIR="/home/hal/Builds/"

mkdir -p "${BUILDDIR}"
#rsync -avz --cvs-exclude --delete ./ "${BUILDDIR}"  # Removes CMake cache files, forcing a fresh rebuild.
rsync -avz --cvs-exclude ./ "${BUILDDIR}"

pushd .
cd "${BUILDDIR}/src"
cmake . -DCMAKE_INSTALL_PREFIX=/usr
make -j 4 && make package
mv *.deb "${BUILTPKGSDIR}/"
popd

