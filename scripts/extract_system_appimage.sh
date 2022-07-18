#!/usr/bin/env bash

# This script packages DCMA into an AppImage sourcing the pieces from the system installation. It then gathers all
# dependencies using linuxdeploy and creates an AppImage.
#
# Note that the end-user's glibc version must be equivalent or newer than the Docker image glibc.

set -eu

# Move to the repository root.
REPOROOT="$(git rev-parse --show-toplevel || true)"
if [ ! -d "${REPOROOT}" ] ; then

    # Fall-back on the source position of this script.
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}/../"
fi
cd "${REPOROOT}"

#########################

commit_hash="$(git rev-parse --short HEAD || printf 'unknown')"

# Gather core files.
mkdir -pv AppDir

# Debian:
#dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / ./AppDir/

# Arch:
#pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / ./AppDir/

# Sometimes the AppImage script fails to modify the rpath of some binaries. Attempt to do this ourselves.
patchelf --set-rpath '$ORIGIN/../lib' ./AppDir/usr/bin/*

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


#########################
# Identify which method will be used to create the AppImage.
ARCH="$(uname -m)" # x86_64, aarch64, ...
if [[ "${ARCH}" =~ .*armv7.* ]] ; then
    ARCH="armhf"
fi

# If a helper is available, use it.
if [ "$ARCH" == "x86_64" ] || [ "$ARCH" == "i686" ] ; then

    # Use continuous artifacts.
    wget "https://halclark.ca/linuxdeploy-${ARCH}.AppImage" ||
    wget "https://artifacts.assassinate-you.net/linuxdeploy/travis-456/linuxdeploy-${ARCH}.AppImage" ||
      wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
    chmod 777 ./linuxdeploy-${ARCH}.AppImage
    ./linuxdeploy-${ARCH}.AppImage --appimage-extract # Unpack because FUSE cannot be used in Docker.
    ./squashfs-root/AppRun \
      --appdir ./AppDir \
      --output appimage \
      --executable ./AppDir/usr/bin/dicomautomaton_dispatcher \
      --icon-file ./artifacts/logos/DCMA_cycle_opti.svg \
      --desktop-file ./dcma.desktop
    rm -rf ./squashfs-root/ ./linuxdeploy-${ARCH}.AppImage 


# Otherwise, try making the AppImage directly.
# Note that this method is less robust!
elif [ "$ARCH" == "aarch64" ] || [ "$ARCH" == "armhf" ] ; then

    # appimagetool appears to require these files be at the top-level.
    cp ./dcma.desktop ./AppDir/dcma.desktop
    cp ./artifacts/logos/DCMA_cycle_opti.svg ./AppDir/

    # Default AppRun program. Note that a more sophisticated approach could be taken here, but I can't find cross-platform
    # tooling that will build an AppRun for both x86_64 and aarch64.
#    cat <<'EOF' > ./AppDir/AppRun
##!/bin/sh
#SELF=$(readlink -f "$0")
#HERE=${SELF%/*}
#export PATH="${HERE}/usr/bin/:${HERE}/usr/sbin/:${HERE}/bin/:${HERE}/sbin/${PATH:+:$PATH}"
#export LD_LIBRARY_PATH="${HERE}/usr/lib/:${HERE}/usr/lib/aarch64-linux-gnu/:${HERE}/usr/lib/armhf-linux-gnu/:${HERE}/usr/lib/i386-linux-gnu/:${HERE}/usr/lib/x86_64-linux-gnu/:${HERE}/usr/lib32/:${HERE}/usr/lib64/:${HERE}/lib/:${HERE}/lib/i386-linux-gnu/:${HERE}/lib/x86_64-linux-gnu/:${HERE}/lib32/:${HERE}/lib64/${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
#export XDG_DATA_DIRS="${HERE}/usr/share/${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
#EXEC=$(grep -e '^Exec=.*' "${HERE}"/*.desktop | head -n 1 | cut -d "=" -f 2 | cut -d " " -f 1)
#exec "${EXEC}" "$@"
#EOF
#    chmod 777 ./AppDir/AppRun
#    ./AppDir/AppRun -h # Test the script is functional.

    # Use the provided AppRun program, which is more sophisticated than a shell script.
    wget "https://github.com/AppImage/AppImageKit/releases/download/13/AppRun-${ARCH}" -O ./AppDir/AppRun
    chmod 777 ./AppDir/AppRun

    # Bundle required libraries, but exclude libraries known to be problematic.
    wget 'https://raw.githubusercontent.com/AppImage/pkg2appimage/master/excludelist' -O - |
      sed -e 's/[ ]*[#].*//' |
      sed -e 's/[.]/[.]/g' |
      grep -v '^$' |
      sed -e 's/^/.*/' -e 's/$/.*/' > excludelist

    rsync -L -v -r \
      $( ldd ./AppDir/usr/bin/dicomautomaton_dispatcher | 
         grep '=>' | 
         sed -e 's@.*=> @@' -e 's@ (.*@@' |
         grep -v -f excludelist
      ) \
      ./AppDir/usr/lib/${ARCH}-linux-gnu/

    find AppDir/usr/bin/ -type f -exec strip '{}' \; \
                                 -exec patchelf --set-rpath '$ORIGIN/../lib/'"${ARCH}-linux-gnu/" '{}' \; || true
    find AppDir/usr/lib/ -type f -exec strip '{}' \; \
                                 -exec patchelf --set-rpath '$ORIGIN/../lib/' '{}' \; || true

    wget "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-${ARCH}.AppImage"
    chmod 777 ./appimagetool-${ARCH}.AppImage
    ./appimagetool-${ARCH}.AppImage --appimage-extract # Unpack because FUSE cannot be used in Docker.
    ./squashfs-root/AppRun -v ./AppDir
    rm -rf ./squashfs-root/ ./appimagetool-${ARCH}.AppImage 

    mv DICOMautomaton*AppImage "DICOMautomaton-${commit_hash}-${ARCH}.AppImage"

fi

ls DICOMautomaton*AppImage

