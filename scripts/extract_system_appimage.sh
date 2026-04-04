#!/usr/bin/env bash

# This script packages DCMA into an AppImage sourcing the pieces from the system installation. It then gathers all
# dependencies using linuxdeploy and creates an AppImage.
#
# Note that the end-user's glibc version must be equivalent or newer than the Docker image glibc.

set -eux

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

# Fallback for direct installations (e.g., via 'make install') where package managers do not track the files.
if [ ! -f ./AppDir/usr/bin/dicomautomaton_dispatcher ] ; then
    dispatcher_path="$(command -v dicomautomaton_dispatcher)"
    install_prefix="$(cd "$(dirname "${dispatcher_path}")/.." && pwd -P)"
    install_manifest_path="$(find "${REPOROOT}" -path '*/install_manifest.txt' -print | head -n 1 || true)"

    mkdir -pv ./AppDir/usr/bin ./AppDir/usr/lib ./AppDir/usr/lib64 ./AppDir/usr/share

    if [ -n "${install_manifest_path}" ] && [ -f "${install_manifest_path}" ] ; then
        while IFS= read -r installed_path ; do
            [ -f "${installed_path}" ] || continue
            rel_path="${installed_path#"${install_prefix}/"}"
            [ "${rel_path}" != "${installed_path}" ] || continue
            mkdir -pv "./AppDir/usr/$(dirname "${rel_path}")"
            cp -a "${installed_path}" "./AppDir/usr/${rel_path}"
        done < "${install_manifest_path}"
    else
        shopt -s nullglob
        for installed_path in \
            "${install_prefix}/bin/dicomautomaton_dispatcher" \
            "${install_prefix}/bin/imebrashim" \
            "${install_prefix}/bin/dialogshim" \
            "${install_prefix}/bin/commonimgshim" \
            "${install_prefix}/lib/"libdicomautomaton* \
            "${install_prefix}/lib/"libygor* \
            "${install_prefix}/lib/"libexplicator* \
            "${install_prefix}/lib64/"libdicomautomaton* \
            "${install_prefix}/lib64/"libygor* \
            "${install_prefix}/lib64/"libexplicator* ; do
            [ -e "${installed_path}" ] || continue
            rel_path="${installed_path#"${install_prefix}/"}"
            mkdir -pv "./AppDir/usr/$(dirname "${rel_path}")"
            cp -a "${installed_path}" "./AppDir/usr/${rel_path}"
        done

        for installed_dir in \
            "${install_prefix}/share/dicomautomaton" \
            "${install_prefix}/share/bash-completion" ; do
            [ -d "${installed_dir}" ] || continue
            rel_path="${installed_dir#"${install_prefix}/"}"
            mkdir -pv "./AppDir/usr/$(dirname "${rel_path}")"
            cp -a "${installed_dir}" "./AppDir/usr/${rel_path}"
        done
        shopt -u nullglob
    fi
fi

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

    # Awful workaround (step 1 of 2) for glibc v 2.36 changing format of ldd output.
    ldd_path="$(which ldd)"
    printf 'Wrapping system ldd...\n'
    mv "${ldd_path}"{,_orig}
    printf '#!/usr/bin/env bash\nldd_orig "$@" | grep -v linux-vdso | grep -v ld-linux\n' > "${ldd_path}"
    chmod 777 "${ldd_path}"

    # Awful workaround (step 2 of 2) restore original ldd when no longer needed.
    trap "printf 'Returning system ldd...\n' ; mv "${ldd_path}"{_orig,} ; exit ;" EXIT

    # Manually strip the files using the platform toolchain's strip command.
    find AppDir/usr/bin/ -type f -exec strip '{}' \; || true
    find AppDir/usr/lib/ -type f -exec strip '{}' \; || true

    # Use continuous artifacts.
    cp "/linuxdeploy_artifacts/linuxdeploy-${ARCH}.AppImage" ./ ||
      wget "https://halclark.ca/linuxdeploy-${ARCH}.AppImage" ||
      wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
    chmod 777 ./linuxdeploy-${ARCH}.AppImage
    ./linuxdeploy-${ARCH}.AppImage --appimage-extract # Unpack because FUSE cannot be used in Docker.
    NO_STRIP=true \
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
    #wget "https://github.com/AppImage/AppImageKit/releases/download/13/AppRun-${ARCH}" -O ./AppDir/AppRun

    cp "/appimagekit_artifacts/obsolete-AppRun-${ARCH}" ./AppDir/AppRun ||
      wget "https://halclark.ca/obsolete-AppRun-${ARCH}" -O ./AppDir/AppRun ||
      wget "https://github.com/AppImage/AppImageKit/releases/download/13/obsolete-AppRun-${ARCH}" -O ./AppDir/AppRun
    find ./AppDir -type d -exec chmod -R 755 '{}' \+
    find ./AppDir -type f -exec chmod 644 '{}' \+

    # Bundle required libraries, but exclude libraries known to be problematic.
    #wget 'https://raw.githubusercontent.com/AppImage/pkg2appimage/master/excludelist' -O - |
    cp "/linuxdeploy_artifacts/f2df956789f36204213876c96500c8b05595e43b_excludelist" excludelist_proto ||
      wget "https://halclark.ca/f2df956789f36204213876c96500c8b05595e43b_excludelist" -O excludelist_proto ||
      wget 'https://raw.githubusercontent.com/AppImage/pkg2appimage/f2df956789f36204213876c96500c8b05595e43b/excludelist' -O excludelist_proto 
    cat excludelist_proto |
      sed -e 's/[ ]*[#].*//' |
      sed -e 's/[.]/[.]/g' |
      grep -v '^$' |
      sed -e 's/^/.*/' -e 's/$/.*/' > excludelist
    printf -- '.*linux.*vdso.*\n' >> excludelist
    printf 'Excluding the following objects:\n'
    cat excludelist

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

    #wget "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-${ARCH}.AppImage"
    cp "/appimagekit_artifacts/obsolete-appimagetool-${ARCH}.AppImage" ./appimagetool-${ARCH}.AppImage ||
      wget "https://halclark.ca/obsolete-appimagetool-${ARCH}.AppImage" -O ./appimagetool-${ARCH}.AppImage ||
      wget "https://github.com/AppImage/AppImageKit/releases/download/13/obsolete-appimagetool-${ARCH}.AppImage" -O ./appimagetool-${ARCH}.AppImage 
    chmod 777 ./appimagetool-${ARCH}.AppImage
    ./appimagetool-${ARCH}.AppImage --appimage-extract # Unpack because FUSE cannot be used in Docker.
    ./squashfs-root/AppRun -v ./AppDir
    rm -rf ./squashfs-root/ ./appimagetool-${ARCH}.AppImage 

    mv DICOMautomaton*AppImage "DICOMautomaton-${commit_hash}-${ARCH}.AppImage"

fi

ls DICOMautomaton*AppImage

