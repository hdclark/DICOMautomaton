#!/bin/bash

# This script packages DCMA into an AppImage sourcing the pieces from the system installation. It then gathers all
# dependencies using linuxdeploy and creates an AppImage.
#
# Note that the end-user's glibc version must be equivalent or newer than the Docker image glibc.

set -eu

reporoot=$(git rev-parse --show-toplevel)
cd "${reporoot}"

wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod 777 ./linuxdeploy-x86_64.AppImage
# Unpack because FUSE cannot be used in Docker.
./linuxdeploy-x86_64.AppImage --appimage-extract

# Gather core files.
mkdir -pv AppDir

# Debian:
#dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / ./AppDir/

# Arch:
#pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / ./AppDir/

printf \
"[Desktop Entry]
Type=Application
# Note: version below is version of the *desktop* file, not DCMA. (...)
Version=1.0
Name=DICOMautomaton
Comment=Tools for working with medical physics data
Path=/usr/bin
Exec=dicomautomaton_dispatcher
Icon=DCMA_cycle_opti
Terminal=true
Categories=Science;
" > dcma.desktop

./squashfs-root/AppRun \
  --appdir ./AppDir \
  --output appimage \
  --executable ./AppDir/usr/bin/dicomautomaton_dispatcher \
  --icon-file ./artifacts/logos/DCMA_cycle_opti.svg \
  --desktop-file ./dcma.desktop

ls -lash DICOMautomaton*AppImage

