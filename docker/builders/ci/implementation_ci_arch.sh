#!/usr/bin/env bash

# This script installs dependencies and then builds and installs DICOMautomaton.
# It can be used for continuous integration (CI), development, and deployment (CD).

set -eux

# Prepare alternative mirrors.
curl -o /etc/pacman.d/mirrorlist "https://archlinux.org/mirrorlist/?country=all&protocol=http&ip_version=4&use_mirror_status=on"
sed -i 's/^#Server/Server/' /etc/pacman.d/mirrorlist

# Disable signature checking.
#
# Note: This may not be needed -- it is only sometimes needed for very old base images.
sed -i -e 's/SigLevel[ ]*=.*/SigLevel = Never/g' \
       -e 's/.*IgnorePkg[ ]*=.*/IgnorePkg = archlinux-keyring/g' /etc/pacman.conf

# Install build dependencies.
#pacman -Sy --noconfirm archlinux-keyring
pacman -Syu --noconfirm --needed \
  base-devel \
  git \
  cmake \
  gcc \
  ` # Needed for an AUR helper ` \
  sudo \
  pyalpm \
  wget \
  rsync \
  patchelf
rm -f /var/cache/pacman/pkg/*


# Create a temporary user for building AUR packages.
useradd -m -r -d /var/empty builduser
mkdir -p /var/empty/
chown -R builduser:builduser /var/empty/
printf '\n''builduser ALL=(ALL) NOPASSWD: ALL''\n' >> /etc/sudoers


# Install hard build dependencies.
pacman -S --noconfirm --needed \
  gcc-libs \
  gnu-free-fonts \
  sfml \
  sdl2 \
  glew \
  jansson \
  libpqxx \
  postgresql \
  gsl \
  boost-libs \
  zlib \
  cgal \
  wt \
  asio \
  nlopt
rm -f /var/cache/pacman/pkg/*


# Install Ygor.
#
# Option 1: install a binary package.
#mkdir -pv /ygor
#cd /ygor
#pacman -U ./Ygor*deb
#
# Option 2: clone the latest upstream commit.
mkdir -pv /ygor
cd /ygor
git clone https://github.com/hdclark/Ygor .
chown -R builduser:builduser .
git config --global --add safe.directory /ygor
su - builduser -c "cd /ygor && ./compile_and_install.sh -b build"
git reset --hard
git clean -fxd :/ 


# Install Explicator.
#
# Option 1: install a binary package.
#mkdir -pv /explicator
#cd /explicator
#pacman -U ./Explicator*deb
#
# Option 2: clone the latest upstream commit.
mkdir -pv /explicator
cd /explicator
git clone https://github.com/hdclark/explicator .
chown -R builduser:builduser .
git config --global --add safe.directory /explicator
su - builduser -c "cd /explicator && ./compile_and_install.sh -b build"
git reset --hard
git clean -fxd :/ 


# Install YgorClustering.
mkdir -pv /ygorcluster
cd /ygorcluster
git clone https://github.com/hdclark/YgorClustering .
chown -R builduser:builduser .
git config --global --add safe.directory /ygorcluster
su - builduser -c "cd /ygorcluster && ./compile_and_install.sh -b build"
git reset --hard
git clean -fxd :/ 


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
chown -R builduser:builduser .
git config --global --add safe.directory /dcma
sed -i -e 's@MEMORY_CONSTRAINED_BUILD=OFF@MEMORY_CONSTRAINED_BUILD=ON@' /dcma/PKGBUILD
su - builduser -c "cd /dcma && makepkg --syncdeps --install --clean --needed --noconfirm"
git reset --hard
git clean -fxd :/ 

