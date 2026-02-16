#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Arch Linux system.

set -eux

mkdir -pv /scratch_base
cd /scratch_base

# Prepare alternative mirrors.
curl -o /etc/pacman.d/mirrorlist "https://archlinux.org/mirrorlist/?country=all&protocol=http&ip_version=4&use_mirror_status=on"
sed -i 's/^#Server/Server/' /etc/pacman.d/mirrorlist

# Disable signature checking.
#
# Note: This may not be needed -- it is only sometimes needed for very old base images.
sed -i -e 's/SigLevel[ ]*=.*/SigLevel = Never/g' \
       -e 's/.*IgnorePkg[ ]*=.*/IgnorePkg = archlinux-keyring/g' /etc/pacman.conf


# Install core packages.
retry_count=0
retry_limit=5
until
    pacman -Syu --noconfirm --needed \
      base-devel \
      git \
      cmake \
      gcc \
      vim \
      gdb \
      screen \
      wget \
      rsync \
      which \
      ` # Needed for 'yay' AUR helper ` \
      sudo \
      pyalpm \
    && \
       \
    ` # Install known official dependencies. ` \
    pacman -S --noconfirm --needed  \
      gcc-libs \
      gsl \
      eigen \
      boost-libs \
      gnu-free-fonts \
      sfml \
      sdl2 \
      glew \
      glu \
      jansson \
      libpqxx \
      postgresql \
      zlib \
      cgal \
      wt \
      asio \
      nlopt \
      patchelf \
      freeglut \
      libxi \
      libxmu \
      thrift \
      ` # Additional dependencies for headless OpenGL rendering with SFML ` \
      xorg-server \
      xorg-apps \
      mesa \
      xf86-video-dummy \
      ` # Other optional dependencies ` \
      bash-completion \
      libnotify \
      dunst \
    && \
       \
    ` # Install SYCL components ` \
    pacman -S --noconfirm --needed  \
      clang \
      libclc \
      ocl-icd \
      opencl-mesa \
      pocl \
      clinfo
      # These conflict with later adaptivecpp pkg: opencl-clhpp opencl-headers

do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

rm -f /var/cache/pacman/pkg/*

cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf

# Neuter makepkg so it can build packages as root (note: still emits a futile error though).
sed -i -e 's/.*exit.*E_ROOT.*//g' $(which makepkg)

# Create an unprivileged user for building packages.
# 
# Note: The 'archlinux' Docker container currently contains user 'aurbuild' and has yay installed already.
#       It won't hurt to add a new build user in case it is missing.
useradd -r -d /var/empty builduser
mkdir -p /var/empty/.config/yay
chown -R builduser:builduser /var/empty
printf '\n''builduser ALL=(ALL) NOPASSWD: ALL''\n' >> /etc/sudoers


# Annoyance to work around eager directory ownership failures in git.
git config --global --add safe.directory "*"


## Download an AUR helper in case it is needed later.
##
## Usage: `su - builduser -c "yay -S --noconfirm packageA packageB ..."`
##
## Note: later versions of yay seem to require systemd components, so harder to run inside unprivileged docker :(.
#if ! command -v yay &>/dev/null ; then
#    cd /tmp
#    yay_version='12.4.2'
#    yay_arch="$(uname -m)"
#    wget "https://github.com/Jguer/yay/releases/download/v${yay_version}/yay_${yay_version}_${yay_arch}.tar.gz"
#    tar -axf yay_*tar.gz
#    mv yay_*/yay /tmp/
#    rm -rf yay_*
#    chmod 777 yay
#    su - builduser -c "cd /tmp && ./yay -S --mflags --skipinteg --noconfirm yay-bin"
#    rm -rf /tmp/yay
#fi

# Install AUR helper.
git clone --depth=1 'https://aur.archlinux.org/trizen.git'
( cd trizen &&
  makepkg -si --noconfirm --needed --noprogressbar --skipchecksums --skipinteg --skippgpcheck --force --nocolor &&
  cd .. &&
  rm -rf trizen

  trizen --nocolors --quiet --noconfirm -S trizen
)

# Examples of building something from the AUR:
#su - builduser -c "cd /tmp && yay -S --mflags --skipinteg --nopgpfetch --noconfirm example-git"
#trizen --nocolors --quiet --noconfirm -S example-git


# Install AdaptiveCpp for optional SYCL toolchain.
retry_count=0
retry_limit=5
until
    su - builduser -c "cd /tmp && trizen --nocolors --quiet --noconfirm -S adaptivecpp"
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

## Install TriSYCL for optional SYCL toolchain.
#retry_count=0
#retry_limit=5
#until
#    su - builduser -c "cd /tmp && trizen --nocolors --quiet --noconfirm -S trisycl-git"
#do
#    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
#    printf 'Waiting to retry.\n' && sleep 5
#done


# Install Ygor.
#
# Option 1: install a binary package.
#mkdir -pv /scratch
#cd /scratch
#pacman -U ./Ygor*deb
#
# Option 2: clone the latest upstream commit.
mkdir -pv /ygor
cd /ygor
git clone https://github.com/hdclark/Ygor .
chown -R builduser:builduser .
su - builduser -c "cd /ygor && ./compile_and_install.sh -b build"
git reset --hard
git clean -fxd :/ 


# Install Explicator.
#
# Option 1: install a binary package.
#mkdir -pv /scratch
#cd /scratch
#pacman -U ./Explicator*deb
#
# Option 2: clone the latest upstream commit.
mkdir -pv /explicator
cd /explicator
git clone https://github.com/hdclark/explicator .
chown -R builduser:builduser .
su - builduser -c "cd /explicator && ./compile_and_install.sh -b build"
git reset --hard
git clean -fxd :/ 


# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
git clone https://github.com/hdclark/YgorClustering .
chown -R builduser:builduser .
su - builduser -c "cd /ygorcluster && ./compile_and_install.sh -b build"
git reset --hard
git clean -fxd :/ 

