#!/bin/bash
set -e

BUILDDIR="/home/hal/Builds/DICOMautomaton/"
BUILTPKGSDIR="/home/hal/Builds/"

mkdir -p "${BUILDDIR}"
rsync -avz  --no-links --cvs-exclude --delete ./ "${BUILDDIR}"  # Removes CMake cache files, forcing a fresh rebuild.

pushd .
cd "${BUILDDIR}"
mkdir -p build && cd build/
cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
make -j $(nproc) && make package
mv *.deb "${BUILTPKGSDIR}/"
popd

