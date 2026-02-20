#!/usr/bin/env bash
# shellcheck disable=SC2086
# SC2086: Double quote to prevent globbing and word splitting - intentionally disabled for package lists.

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

# Use the centralized package list script.
# In CI context, the script is available from the repository.
GET_PACKAGES="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../../scripts/get_packages.sh"
if [ ! -f "${GET_PACKAGES}" ]; then
    GET_PACKAGES="/dcma_scripts/get_packages.sh"
fi

# Get packages from the centralized script.
PKGS_BUILD_TOOLS="$("${GET_PACKAGES}" --os arch --tier build_tools)"
PKGS_DEVELOPMENT="$("${GET_PACKAGES}" --os arch --tier development)"
PKGS_YGOR_DEPS="$("${GET_PACKAGES}" --os arch --tier ygor_deps)"
PKGS_DCMA_DEPS="$("${GET_PACKAGES}" --os arch --tier dcma_deps)"

retry_count=0
retry_limit=5
until
    `# Install build dependencies ` \
    pacman -Syu --noconfirm --needed ${PKGS_BUILD_TOOLS}
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done
rm -f /var/cache/pacman/pkg/*


# Create a temporary user for building AUR packages.
useradd -m -r -d /var/empty builduser
mkdir -p /var/empty/
chown -R builduser:builduser /var/empty/
printf '\n''builduser ALL=(ALL) NOPASSWD: ALL''\n' >> /etc/sudoers


retry_count=0
retry_limit=5
until
    `# Install development tools ` \
    pacman -S --noconfirm --needed ${PKGS_DEVELOPMENT} && \
    `# Install build dependencies ` \
    `# Note: sfml needs SFML2 but no compat pkg available yet, so handled separately below ` \
    pacman -S --noconfirm --needed ${PKGS_YGOR_DEPS} ${PKGS_DCMA_DEPS}
do
    (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
    printf 'Waiting to retry.\n' && sleep 5
done

# Use snapshot of SFML2 package until something more permanent is available.
wget 'https://www.halclark.ca/sfml-2.6.1-1-x86_64.pkg.tar.zst' -O sfml-2.6.1-1-x86_64.pkg.tar.zst
pacman -U --noconfirm --needed ./sfml-2.6.1-1-x86_64.pkg.tar.zst

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

