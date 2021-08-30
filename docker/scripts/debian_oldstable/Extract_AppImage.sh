#!/usr/bin/env bash

# This script rebuilds DCMA inside the Debian:oldstable-based DCMA Docker image.
# It then gathers all dependencies using linuxdeploy and creates an AppImage.
# Note that the end-user's glibc version must be equivalent or newer than the
# Docker image glibc. At the moment, Debian:oldoldstable is not new enough to
# compile C++17, so we are stuck with the glibc in Debian:oldstable.

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


cd /ygorcluster/
git clean -fxd
git checkout .
git pull "https://github.com/hdclark/ygorclustering"
mkdir build
cd build
cmake \
  ../ \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release 
make -j 8 VERBOSE=1
make install DESTDIR=/scratch/AppDir/
rm -rf build/


cd /explicator/
git clean -fxd
git checkout .
git pull "https://github.com/hdclark/explicator"
mkdir build
cd build
cmake \
  ../ \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release 
make -j 8 VERBOSE=1
make install DESTDIR=/scratch/AppDir/
rm -rf build/


cd /ygor/
git clean -fxd
git checkout .
git pull "https://github.com/hdclark/ygor"
mkdir build
cd build
cmake \
  ../ \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_LINUX_SYS=ON \
  -DWITH_EIGEN=ON \
  -DWITH_GNU_GSL=ON \
  -DWITH_BOOST=ON
make -j 8 VERBOSE=1
make install DESTDIR=/scratch/AppDir/
rm -rf build/


cd /dcma/
git clean -fxd
git checkout .
git pull "https://github.com/hdclark/DICOMautomaton"
mkdir build
cd build
cmake \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_SYSCONFDIR=/etc \
  -DCMAKE_BUILD_TYPE=Release \
  -DMEMORY_CONSTRAINED_BUILD=OFF \
  -DWITH_ASAN=OFF \
  -DWITH_TSAN=OFF \
  -DWITH_MSAN=OFF \
  -DWITH_EIGEN=ON \
  -DWITH_CGAL=ON \
  -DWITH_NLOPT=ON \
  -DWITH_SFML=ON \
  -DWITH_SDL=ON \
  -DWITH_WT=ON \
  -DWITH_GNU_GSL=ON \
  -DWITH_POSTGRES=ON \
  -DWITH_JANSSON=ON \
  ../
make -j 8 VERBOSE=1
make install DESTDIR=/scratch/AppDir/
rm -rf build/


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


#/bin/bash -i

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
    dicomautomaton_webserver_debian_oldstable:latest \
    /start/create.sh 

sudo chown $(id -u -n):$(id -g -n) *AppImage

