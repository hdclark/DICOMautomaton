#!/usr/bin/env bash

# This script installs dependencies and then builds and installs DICOMautomaton.
# It can be used for continuous integration (CI), development, and deployment (CD).
#
# Note that Debian buster is no longer supported, and this script relies on historical snapshots
# to source packages. Packages should be cached if possible to reduce load on the upstream servers.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

# Install build dependencies.
export DEBIAN_FRONTEND="noninteractive"


# Workarounds using the Debian archive for buster release.
#( printf -- 'deb http://archive.debian.org/debian buster main contrib non-free' ;
#  printf -- 'deb http://archive.debian.org/debian-security buster/updates main contrib non-free' ;
#  printf -- 'deb http://archive.debian.org/debian buster-backports main contrib non-free' ; ) > /etc/apt/sources.list

# Downgrade specific packages.
# apt-get install --yes --allow-remove-essential --allow-downgrades libc6=2.28-10+deb10u1
# apt-get install --yes --allow-remove-essential --allow-downgrades zlib1g=1:1.2.11.dfsg-1+deb10u1
# apt-get install --yes --allow-remove-essential --allow-downgrades libtinfo6=6.1+20181013-2+deb10u2
# apt-get install --yes --allow-remove-essential --allow-downgrades libncurses6 libtinfo6=6.1+20181013-2+deb10u2
# apt-get install --yes --allow-remove-essential --allow-downgrades libelf1=0.176-1.1 libmount1=2.33.1-0.1 libblkid1=2.33.1-0.1 libuuid1=2.33.1-0.1 'libudev1=241-7~deb10u8'

# Use the last snapshot of Debian buster.
( printf -- 'deb http://snapshot.debian.org/archive/debian/20250712T143640Z buster unstable main contrib' ;
  printf -- 'deb http://snapshot.debian.org/archive/debian/20250712T143640Z buster-updates unstable main contrib' ;
  printf -- 'deb-src http://snapshot.debian.org/archive/debian/20250712T143640Z buster unstable main contrib' ;
  printf -- 'deb-src http://snapshot.debian.org/archive/debian/20250712T143640Z buster-updates unstable main contrib' ) > /etc/apt/sources.list

# 'Pin' packages from the snapshot source so they override the local packages, even if the snapshotted sources are
# downgrades. See https://vincent.bernat.ch/en/blog/2020-downgrade-debian .
( printf -- 'Package: *\n' ; 
  printf -- 'Pin: origin snapshot.debian.org\n' ; 
  printf -- 'Pin-Priority: 2000\n' ) > /etc/apt/preferences.d/override_buster.pref

# Make a wrapper for the apt-get program that includes additional flags.
l_orig_apt="$(which apt-get)"
l_moved_apt="/real_apt-get"
mv -n "${l_orig_apt}" "${l_moved_apt}"
printf -- '#!/usr/bin/env bash\n%s --allow-remove-essential --allow-downgrades --no-install-recommends "$@"\n' "${l_moved_apt}" > "${l_orig_apt}"
chmod 777 "${l_orig_apt}"

apt-get update --yes || true

retry_count=0
retry_limit=5
until
    apt-get install --yes --no-install-recommends \
      git \
      cmake \
      make \
      g++ \
      ncurses-term \
      gdb \
      rsync \
      wget \
      ca-certificates \
      file \
      coreutils \
      binutils \
      findutils \
      openssh-client \
    && apt-get install --yes --no-install-recommends \
      ` # Ygor dependencies ` \
      libboost-dev \
      libgsl-dev \
      libeigen3-dev \
      ` # DICOMautomaton dependencies ` \
      libeigen3-dev \
      libboost-dev \
      libboost-filesystem-dev \
      libboost-iostreams-dev \
      libboost-program-options-dev \
      libboost-thread-dev \
      libz-dev \
      libsfml-dev \
      libsdl2-dev \
      libglew-dev \
      libjansson-dev \
      libpqxx-dev \
      postgresql-client \
      libcgal-dev \
      libnlopt-dev \
      libnlopt-cxx-dev \
      libasio-dev \
      fonts-freefont-ttf \
      fonts-cmu \
      freeglut3 \
      freeglut3-dev \
      libxi-dev \
      libxmu-dev \
      libthrift-dev \
      thrift-compiler \
      patchelf \
    ` # Other optional dependencies ` \
    && apt-get install --yes --no-install-recommends \
      libnotify-dev \
      dunst \
      bash-completion \
      gnuplot \
      zenity \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    && apt-get install --yes --no-install-recommends \
      x-window-system \
      mesa-utils \
      xserver-xorg-video-dummy \
      x11-apps \
    ` # Additional packages prospectively added in case needed for future development ` \
    && apt-get install --yes --no-install-recommends \
      libsqlite3-dev \
      sqlite3 \
      liblua5.3-dev \
      libpython3-dev \
      libprotobuf-dev \
      protobuf-compiler \
      clang \
      clang-format \
      clang-tidy \
      clang-tools

do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf || true


# Install Wt from source to get around outdated and buggy Debian package.
#
# Note: Add additional dependencies if functionality is needed -- this is a basic install.
#
# Note: Could also install build-deps for the distribution packages, but the dependencies are not
#       guaranteed to be stable (e.g., major version bumps).
if [ ! -d /wt ] ; then
    mkdir -pv /wt
    cd /wt
    git clone https://github.com/emweb/wt.git .
    mkdir -p build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ../
    JOBS=$(nproc)
    JOBS=$(( JOBS < 8 ? JOBS : 8 ))
    make -j "$JOBS" VERBOSE=1
    make install
    make clean

    mkdir -pv /scratch_base
    cd /scratch_base
    apt-get install --yes \
      -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb
fi

# This function either freshly clones a git repository, or pulls from upstream remotes.
# The return value can be used to determine whether recompilation is required.
function clone_or_pull {
    if git clone "$@" . ; then
        return 0 # Requires compilation.
    fi

    if ! git fetch ; then
        return 2 # Failure.
    fi

    if [ "$(git rev-parse HEAD)" == "$(git rev-parse '@{u}')" ] ; then
        return 1 # Compilation not required.
    fi
    git merge
}
export -f clone_or_pull

# Install Ygor.
#
# Option 1: install a binary package.
#mkdir -pv /scratch
#cd /scratch
#apt-get install --yes -f ./Ygor*deb
#
# Option 2: clone the latest upstream commit.
mkdir -pv /ygor
cd /ygor
if clone_or_pull "https://github.com/hdclark/Ygor" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Ygor already up-to-date.\n'
fi

# Install Explicator.
#
# Option 1: install a binary package.
#mkdir -pv /scratch
#cd /scratch
#apt-get install --yes -f ./Explicator*deb
#
# Option 2: clone the latest upstream commit.
mkdir -pv /explicator
cd /explicator
if clone_or_pull "https://github.com/hdclark/explicator" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Explicator already up-to-date.\n'
fi

# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
if clone_or_pull "https://github.com/hdclark/YgorClustering" ; then
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'Ygor Clustering already up-to-date.\n'
fi

# Install DICOMautomaton.
#
# Option 1: install a binary package.
#mkdir -pv /dcma
#cd /dcma
#apt-get install --yes -f ./DICOMautomaton*deb 
#
# Option 2: clone the latest upstream commit.
#mkdir -pv /dcma
#cd /dcma
#git clone https://github.com/hdclark/DICOMautomaton .
#   ...
#
# Option 3: use the working directory.
mkdir -pv /dcma
cd /dcma
if clone_or_pull "https://github.com/hdclark/DICOMautomaton" ; then
    #sed -i -e 's@MEMORY_CONSTRAINED_BUILD=OFF@MEMORY_CONSTRAINED_BUILD=ON@' /dcma/compile_and_install.sh || true
    #sed -i -e 's@option.*WITH_WT.*ON.*@option(WITH_WT "Wt disabled" OFF)@' /dcma/CMakeLists.txt || true
    ./compile_and_install.sh -b build
    git reset --hard
    git clean -fxd :/ 
else
    printf 'DICOMautomaton already up-to-date.\n'
fi

