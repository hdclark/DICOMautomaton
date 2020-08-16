#!/bin/bash

# This script prepares a build environment using MXE and then cross-compiles DICOMautomaton dependencies.


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
  xz-utils \
  sudo \
  gnupg


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
#
# Note: ideally we would download pre-compiled binaries, but not everything we need is available (see below).
make -j"$(nproc)" --keep-going \
  MXE_TARGETS="${TOOLCHAIN}" \
  MXE_PLUGIN_DIRS=plugins/gcc9 \
  gmp mpfr boost eigen sfml nlopt #wt

## Download pre-compiled binaries to speed up toolchain prep.
##
## Note: sadly, gcc9 is not available this way -- only the default (gcc5.5) is available.
##
## Note: files are installed with root /usr/lib/mxe/usr/"${TOOLCHAIN}"/ so that
##   root@trip:/mxe# dpkg-query -L mxe-x86-64-w64-mingw32.static-sfml
##   ...
##   /usr/lib/mxe/usr/x86-64-w64-mingw32.static/lib/libsfml-window-s.a
##   /usr/lib/mxe/usr/x86-64-w64-mingw32.static/lib/pkgconfig
##   /usr/lib/mxe/usr/x86-64-w64-mingw32.static/lib/pkgconfig/sfml.pc
#echo "deb http://mirror.mxe.cc/repos/apt stretch main" \
#    | tee /etc/apt/sources.list.d/mxeapt.list
#apt-key adv \
#  --keyserver keyserver.ubuntu.com \
#  --recv-keys C6BF758A33A3A276
#apt-get -y update
#apt-get -y install \
#  mxe-"${TOOLCHAIN}"-boost \
#  mxe-"${TOOLCHAIN}"-cgal \
#  mxe-"${TOOLCHAIN}"-eigen \
#  mxe-"${TOOLCHAIN}"-gmp \
#  mxe-"${TOOLCHAIN}"-mpfr \
#  mxe-"${TOOLCHAIN}"-nlopt \
#  mxe-"${TOOLCHAIN}"-sfml \
#  mxe-"${TOOLCHAIN}"-wt
# Note: trying to mix these files will fail:
#rsync -avP /usr/lib/mxe/usr/"${TOOLCHAIN}"/ /mxe/usr/"${TOOLCHAIN}"/ # <-- Will fail!

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

# Workaround for MXE's SFML pkg-config files missing the standard modules.
cp "/mxe/usr/${TOOLCHAIN}/lib/pkgconfig/"sfml{,-graphics}.pc
cp "/mxe/usr/${TOOLCHAIN}/lib/pkgconfig/"sfml{,-window}.pc
cp "/mxe/usr/${TOOLCHAIN}/lib/pkgconfig/"sfml{,-system}.pc

# Compile the portable pieces.
for repo_dir in /ygor /ygorclustering /explicator ; do
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
      -DWITH_GNU_GSL=OFF \
      -DBUILD_SHARED_LIBS=OFF \
      ../
    make -j"$(nproc)" --ignore-errors VERBOSE=1 2>&1 | tee ../build_log 
    make install DESTDIR="/mxe/usr/${TOOLCHAIN}/"

    # Make the artifacts available elsewhere for easier access.
    make install DESTDIR="/out"
    rsync -avP /out/usr/ "/mxe/usr/${TOOLCHAIN}/"

    cd "$repo_dir"
    git reset --hard || true
    git clean -fxd :/ || true
done

# Ensure the toolchain environment variables are passed to future sessions.
env |
  grep -i 'MXE\|^PATH=\|PKG_CONFIG\|PROXY\|TOOLCHAIN=' | \
  sed -e 's/^/export /' \
  >> /etc/profile.d/mxe_toolchain.sh

