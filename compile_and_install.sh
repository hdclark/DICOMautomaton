#!/bin/sh
set -e

BUILDDIR="/home/hal/Builds/DICOMautomaton/"
BUILTPKGSDIR="/home/hal/Builds/"

mkdir -p "${BUILDDIR}"
#rsync -avz --delete ./ "${BUILDDIR}"  # Removes CMake cache files, forcing a fresh rebuild.
rsync -avz ./ "${BUILDDIR}"

pushd .
cd "${BUILDDIR}"
makepkg --syncdeps --install
mv "${BUILDDIR}/"*.pkg.tar.* "${BUILTPKGSDIR}/"
popd

