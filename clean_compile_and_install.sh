#!/bin/bash
set -e

BUILDDIR="/home/hal/Builds/DICOMautomaton/"
BUILTPKGSDIR="/home/hal/Builds/"

mkdir -p "${BUILDDIR}"
rsync -avz --no-links --cvs-exclude --delete ./ "${BUILDDIR}"  # Removes CMake cache files, forcing a fresh rebuild.

pushd .
cd "${BUILDDIR}"
makepkg --syncdeps --install --noconfirm
mv "${BUILDDIR}/"*.pkg.tar.* "${BUILTPKGSDIR}/"
popd

