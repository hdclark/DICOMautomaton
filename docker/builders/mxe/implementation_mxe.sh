#!/bin/bash

# This script prepares a build environment using MXE and then cross-compiles DICOMautomaton and all dependencies.
# It can be used for continuous integration (CI), development, and deployment (CD).
# It also produces Windows binary artifacts.

set -eux

export DEBIAN_FRONTEND="noninteractive"

apt-get -y update
apt-get -y install \
  autoconf \
  automake \
  autopoint \
  bash \
  bison \
  bzip2 \
  flex \
  g++ \
  g++-multilib \
  gettext \
  git \
  gperf \
  intltool \
  libc6-dev-i386 \
  libgdk-pixbuf2.0-dev \
  libltdl-dev \
  libssl-dev \
  libtool-bin \
  libxml-parser-perl \
  lzip \
  make \
  openssl \
  p7zip-full \
  patch \
  perl \
  python \
  ruby \
  sed \
  unzip \
  wget \
  ca-certificates \
  rsync \
  xz-utils


# See <https://mxe.cc/#tutorial> and <https://mxe.cc/#requirements-debian> for more information. Perform the following
# instructions inside a recent `Debian` `Docker` container (or VM, instance, or bare metal installation). The following
# is essentially a lightly customized version of the MXE tutorial.

git clone https://github.com/mxe/mxe.git /mxe
cd /mxe

# Remove components we won't need to reduce setup time.
rm -rf src/qt* src/ocaml* src/sdl* || true

#export TOOLCHAIN="x86_64-w64-mingw32.shared"
export TOOLCHAIN="x86_64-w64-mingw32.static"

# Build the MXE environment. Note this could take hours.
# Builds a gcc5.5 compiler toolchain by default.
#make -j4 --keep-going || true
# Should build gcc9, but still builds gcc5.5 (default).
#make -j4 --keep-going MXE_TARGETS="${TOOLCHAIN}.gcc9" || true
# Works, but was purportedly deprecated in 2018.
#echo 'override MXE_PLUGIN_DIRS += plugins/gcc9' >> settings.mk  # Update compiler version.

# Make everything.
#make -j4 --keep-going MXE_TARGETS="${TOOLCHAIN}" MXE_PLUGIN_DIRS=plugins/gcc9 || true

# Make a specific subset.
make -j4 --keep-going \
  MXE_TARGETS="${TOOLCHAIN}" \
  MXE_PLUGIN_DIRS=plugins/gcc9 \
  gmp mpfr boost eigen sfml nlopt #wt

export PATH="/mxe/usr/bin:$PATH"

# Report the compiler version for debugging.
"${TOOLCHAIN}-g++" --version

unset `env | \
  grep -vi '^EDITOR=\|^HOME=\|^LANG=\|MXE\|^PATH=' | \
  grep -vi 'PKG_CONFIG\|PROXY\|^PS1=\|^TERM=\|TOOLCHAIN=' | \
  cut -d '=' -f1 | tr '\n' ' '`


# Cross-compiled libraries and upstream packages are installed with the `/mxe/usr/${TOOLCHAIN}/` prefix. Installation to
# a custom prefix will simplify extraction of final build artifacts. Note that the `MXE` `CMake` wrapper will properly
# source from the `MXE` prefix directory, so we only need to tell the toolchain where to look for custom dependencies.

git clone 'https://github.com/hdclark/Ygor' /ygor
git clone 'https://github.com/hdclark/YgorClustering' /ygorclustering
git clone 'https://github.com/hdclark/Explicator' /explicator
if [ ! -d /dcma ] ; then
    printf 'Source not provided at /dcma, pulling public repository.\n'
    git clone 'https://github.com/hdclark/DICOMautomaton' /dcma
fi

mkdir -pv /out/usr/{bin,lib,include,share}

# Provide harmless duplicates if the std::filesystem bug is present.
#
# Note: this can be removed when libstdc++fs.a no longer needs to be linked explicitly. TODO.
if [ ! -f /mxe/usr/"${TOOLCHAIN}"/lib/libstdc++fs.a ] ; then
    cp /mxe/usr/"${TOOLCHAIN}"/lib/{libm.a,libstdc++fs.a} || true
fi
if [ ! -f /mxe/usr/"${TOOLCHAIN}"/lib/libstdc++fs.so ] ; then
    cp /mxe/usr/"${TOOLCHAIN}"/lib/{libm.so,libstdc++fs.so} || true
fi

# Confirm the search locations reflect the toolchain prefix.
/mxe/usr/x86_64-pc-linux-gnu/bin/"${TOOLCHAIN}-g++" -print-search-dirs

# Install missing dependencies.
# Asio.
( mkdir -pv /asio &&
  cd /asio &&
  wget 'https://sourceforge.net/projects/asio/files/latest/download' -O asio.tgz &&
  ( tar -axf asio.tgz || unzip asio.tgz ) &&
  cd asio-*/ &&
  cp -v -R include/asio/ include/asio.hpp /mxe/usr/"${TOOLCHAIN}"/include/  )

# Wt. (TODO.)
#mkdir -pv /wt
#cd /wt
#git clone https://github.com/emweb/wt.git .
#mkdir -p build
#cd build
#cmake -DCMAKE_INSTALL_PREFIX=/usr ../
#JOBS=$(nproc)
#JOBS=$(( $JOBS < 8 ? $JOBS : 8 ))
#make -j "$JOBS" VERBOSE=1
#make install
#make clean

# Workaround for MXE's SFML pkg-config files missing the standard modules.
cp "/mxe/usr/${TOOLCHAIN}/lib/pkgconfig/"sfml{,-graphics}.pc
cp "/mxe/usr/${TOOLCHAIN}/lib/pkgconfig/"sfml{,-window}.pc
cp "/mxe/usr/${TOOLCHAIN}/lib/pkgconfig/"sfml{,-system}.pc

# Compile the portable pieces.
for repo_dir in /ygor /ygorclustering /explicator /dcma ; do
#for repo_dir in /ygor /ygorclustering /explicator ; do
#for repo_dir in /explicator ; do
#for repo_dir in /ygor ; do
#for repo_dir in /dcma ; do
    cd "$repo_dir"
    rm -rf build/
    mkdir build
    cd build 
    #"${TOOLCHAIN}-cmake" -E env CXXFLAGS='-I/out/include' \
    #"${TOOLCHAIN}-cmake" -E env LDFLAGS="-L/out/lib" \
    #"${TOOLCHAIN}-cmake" \
    #  -DCMAKE_CXX_FLAGS='-I/out/include' \

    "${TOOLCHAIN}-cmake" \
      -DCMAKE_INSTALL_PREFIX=/usr/ \
      -DCMAKE_BUILD_TYPE=Release \
      -DWITH_LINUX_SYS=OFF \
      -DWITH_EIGEN=ON \
      -DWITH_CGAL=OFF \
      -DWITH_NLOPT=ON \
      -DWITH_SFML=ON \
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


