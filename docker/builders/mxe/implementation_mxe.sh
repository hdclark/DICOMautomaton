#!/usr/bin/env bash

# This script cross-compiles DICOMautomaton. It assumes dependencies have been pre-compiled (i.e., the corresponding build_bases
# script was completed successfully).
# The cross-compilation produces Windows binary artifacts.

set -eux

# Ensure the environment has been set correctly.
#export TOOLCHAIN="x86_64-w64-mingw32.static"
#export PATH="/mxe/usr/bin:$PATH"
source /etc/profile.d/mxe_toolchain.sh

# Report the compiler version for debugging.
"${TOOLCHAIN}-g++" --version
# Confirm the search locations reflect the toolchain prefix.
"${TOOLCHAIN}-g++" -print-search-dirs

if [ ! -d /dcma ] ; then
    printf 'Source not provided at /dcma, pulling public repository.\n'
    git clone 'https://github.com/hdclark/DICOMautomaton' /dcma
fi

# Re-build dependencies to improve re-use of the base container.
#
# The full MXE toolchain is a heavyweight in terms of size and computation time, which makes it hard to integrate into
# CI. Prospectively re-compiling the following dependencies before each DICOMautomaton re-compile will *significantly*
# reduce the need to fully regenerate the MXE toolchain.
rm -rf /ygor /ygorclustering /explicator || true
git clone 'https://github.com/hdclark/Ygor' /ygor
git clone 'https://github.com/hdclark/YgorClustering' /ygorclustering
git clone 'https://github.com/hdclark/Explicator' /explicator

# Perform the compilation.
for repo_dir in /ygor /ygorclustering /explicator /dcma ; do
#for repo_dir in /dcma ; do
    cd "$repo_dir"
    rm -rf build/
    mkdir build
    cd build 

    "${TOOLCHAIN}-cmake" \
      -DCMAKE_INSTALL_PREFIX=/usr/ \
      -DCMAKE_INSTALL_SYSCONFDIR=/etc \
      -DCMAKE_BUILD_TYPE=Release \
      -DWITH_LINUX_SYS=OFF \
      -DWITH_EIGEN=ON \
      -DWITH_CGAL=ON \
      -DWITH_NLOPT=ON \
      -DWITH_SFML=ON \
      -DWITH_SDL=ON \
      -DWITH_WT=OFF \
      -DWITH_GNU_GSL=OFF \
      -DWITH_POSTGRES=OFF \
      -DWITH_JANSSON=OFF \
      -DBUILD_SHARED_LIBS=OFF \
      ../

    make -j2 --ignore-errors VERBOSE=1 2>&1 | tee ../build_log 
    make install DESTDIR="/mxe/usr/${TOOLCHAIN}/"

    # Make the artifacts available elsewhere for easier access.
    make install DESTDIR="/out"
    rsync -avP /out/usr/ "/mxe/usr/${TOOLCHAIN}/"
done

# Note: imbuing the executables with icons:
#    cd /out/usr/bin/
#    cp /dcma/artifacts/logos/DCMA_cycle_opti.svg ./
#    apt-get -y install imagemagick
#    convert DCMA_cycle_opti.svg -scale 256x256 dcma.ico
#    printf 'id ICON dcma.ico\n' > win_res.rc
#    "${TOOLCHAIN}-windres" win_res.rc -O coff -o win_res.o
#    "${TOOLCHAIN}-g++" -o dicomautomaton_dispatcher.exe ... win_res.o ...
#
# However, this needs to be added to CMake so the object file can be linked into binaries.

