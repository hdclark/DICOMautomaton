#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Debian stable system.

set -eux

export DEBIAN_FRONTEND="noninteractive"

apt-get update --yes
apt-get install --yes \
  debootstrap \
  qemu \
  proot \
  qemu-user-static 
#  binfmt-support \

# Note: debian refers to 'aarch64' as 'arm64'.
debootstrap --arch=arm64 --foreign stable /debian_stable_aarch64 'http://deb.debian.org/debian/'

cat << 'EOF' > /run_within_aarch64_chroot.sh
proot \
  -q qemu-aarch64-static \
  -0 \
  -r /debian_stable_aarch64 \
  -b /dev/ \
  -b /sys/ \
  -b /proc/ \
  /usr/bin/env \
    -i \
    HOME=/root \
    TERM="xterm-256color" \
    PATH=/bin:/usr/bin:/sbin:/usr/sbin \
    /bin/bash -c "$@"
EOF
chmod 777 /run_within_aarch64_chroot.sh

/run_within_aarch64_chroot.sh '/debootstrap/debootstrap --second-stage'

printf 'Outside the chroot:\n'
uname -a
printf 'Inside the chroot:\n'
/run_within_aarch64_chroot.sh 'uname -a'

# Chroot now ready to be used to build aarch64 artifacts.

