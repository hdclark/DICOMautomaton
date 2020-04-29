#!/bin/bash

# This script rebuilds DCMA inside the Debian:stable-based DCMA Docker image.
# It then gathers all dependencies using linuxdeploy and creates an AppImage.
# Note that the end-user's glibc version must be equivalent or newer than the
# Docker image glibc. At the moment, Debian:oldstable is not new enough to
# compile C++17, so we are stuck with the glibc in Debian:stable.

set -eu

create_sh=$(mktemp '/tmp/dcma_create_appimage_internal_XXXXXXXXX.sh')

trap "{ rm '${create_sh}' ; }" EXIT # Clean-up the temp file when we exit.

printf \
'#!/bin/bash

set -eu
mkdir -p /scratch/AppDir/

cd /scratch/
wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod 777 ./linuxdeploy-x86_64.AppImage
# Unpack because FUSE cannot be used in Docker.
./linuxdeploy-x86_64.AppImage --appimage-extract


cd /ygorcluster/
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
mkdir build
cd build
cmake \
  -DMEMORY_CONSTRAINED_BUILD=OFF \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_EIGEN=ON \
  -DWITH_CGAL=ON \
  -DWITH_NLOPT=ON \
  -DWITH_SFML=ON \
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

' > "$create_sh"
chmod 777 "$create_sh"

sudo docker run -it --rm \
    --network=host \
    -v "$(pwd)":/scratch/:rw \
    -v "$create_sh":/start/create.sh:ro \
    -w /scratch/ \
    dicomautomaton_webserver_debian_stable:latest \
    /start/create.sh 

