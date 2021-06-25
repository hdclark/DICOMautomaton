#!/usr/bin/env bash

# This script packages DCMA into an AppImage sourcing the pieces from the system installation. It then gathers all
# dependencies using linuxdeploy and creates an AppImage.
#
# Note that the end-user's glibc version must be equivalent or newer than the Docker image glibc.

reporoot=$(git rev-parse --show-toplevel)

set -eu

if [ -d "${reporoot}" ] ; then
    cd "${reporoot}"
fi

# Gather core files.
mkdir -pv AppDir

# Debian:
#dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(dpkg-query -L ygor ygorclustering explicator dicomautomaton | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / ./AppDir/

# Arch:
#pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done
rsync -avp --files-from=<(pacman -Ql ygor ygorclustering explicator dicomautomaton | cut -d" " -f 2- | while read a ; do [ -f "$a" ] && echo "${a#/}" ; done) / ./AppDir/


# Desktop file, including default exec statement.
printf \
"[Desktop Entry]
Type=Application
# Note: version below is version of the *desktop* file, not DCMA. (...)
Version=1.0
Name=DICOMautomaton
Comment=Tools for working with medical physics data
Path=/usr/bin
Exec=dicomautomaton_dispatcher
Icon=dicomautomaton_dispatcher
Terminal=true
Categories=Science;
" > dcma.desktop

#mkdir -pv ./AppDir/usr/share/applications/
cp ./dcma.desktop ./AppDir/dicomautomaton_dispatcher.desktop
#mkdir -pv ./AppDir/usr/share/icons/default/scalable/apps/
cp ./artifacts/logos/DCMA_cycle_opti.svg ./AppDir/dicomautomaton_dispatcher.svg

# Default AppRun program. Note that a more sophisticated approach could be taken here, but I can't find cross-platform
# tooling that will build an AppRun for x86_64 and aarch64.
cat <<'EOF' > ./AppDir/AppRun
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin/:${HERE}/usr/sbin/:${HERE}/bin/:${HERE}/sbin/${PATH:+:$PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib/:${HERE}/usr/lib/aarch64-linux-gnu/:${HERE}/usr/lib/i386-linux-gnu/:${HERE}/usr/lib/x86_64-linux-gnu/:${HERE}/usr/lib32/:${HERE}/usr/lib64/:${HERE}/lib/:${HERE}/lib/i386-linux-gnu/:${HERE}/lib/x86_64-linux-gnu/:${HERE}/lib32/:${HERE}/lib64/${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share/${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
EXEC=$(grep -e '^Exec=.*' "${HERE}"/*.desktop | head -n 1 | cut -d "=" -f 2 | cut -d " " -f 1)
exec "${EXEC}" "$@"
EOF
chmod 777 ./AppDir/AppRun

# Test the script is functional.
./AppDir/AppRun -h

# Build the AppImage using appimagetool directly.
ARCH="$(uname -m)" # x86_64, aarch64, ...
wget "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-${ARCH}.AppImage"
chmod 777 ./appimagetool-aarch64.AppImage
./appimagetool-${ARCH}.AppImage --appimage-extract
./squashfs-root/AppRun -v ./AppDir

ls DICOMautomaton*AppImage

