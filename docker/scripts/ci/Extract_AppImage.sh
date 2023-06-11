#!/usr/bin/env bash

# This script packages DCMA into an AppImage sourcing the pieces from the CI
# installation. It then gathers all dependencies using linuxdeploy and creates
# an AppImage. Note that the end-user's glibc version must be equivalent or
# newer than the Docker image glibc. At the moment, Debian:stretch is not new
# enough to compile C++17, so we are probably stuck with the glibc in
# Debian:buster. But this script should work for both Arch Linux and Debian.

set -eu

create_sh=$(mktemp '/tmp/dcma_create_appimage_internal_XXXXXXXXX.sh')

trap "{ rm '${create_sh}' ; }" EXIT # Clean-up the temp file when we exit.

printf \
'#!/bin/bash

set -eu
mkdir -pv /scratch/AppDir/

cd /scratch/
wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod 777 ./linuxdeploy-x86_64.AppImage
# Unpack because FUSE cannot be used in Docker.
./linuxdeploy-x86_64.AppImage --appimage-extract

# Gather core files.

# Debian:
#dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / /scratch/AppDir/

# Arch:
#pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / /scratch/AppDir/

cd /scratch/
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
  --appdir AppDir \
  --output appimage \
  --executable ./AppDir/usr/bin/dicomautomaton_dispatcher \
  --icon-file /dcma/artifacts/logos/DCMA_cycle_opti.svg \
  --desktop-file dcma.desktop

mv DICOMautomaton*AppImage /passage/

' > "$create_sh"
chmod 777 "$create_sh"

sudo docker run -it --rm \
    --network=host \
    -v "$(pwd)":/passage/:rw \
    -v "$create_sh":/start/create.sh:ro \
    -w /passage/ \
    dcma_ci:latest \
    /start/create.sh 

sudo chown $(id -u -n):$(id -g -n) *AppImage

