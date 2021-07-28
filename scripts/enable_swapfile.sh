#!/usr/bin/env bash
set -eux

swapfilename="/swapfile_custom"

if [ "$EUID" -ne 0 ] ; then
    printf "This script must be run as root. Cannot continue.\n" 1>&2
    exit 1
fi

dd if=/dev/zero of="${swapfilename}" bs=1M count=4096
chmod 600 "${swapfilename}"
chattr +c "${swapfilename}" || true

mkswap "${swapfilename}"

# Make the swapfile a device before enabling, rather than enabling it directly.
# This improves support on exotic filesystems.
swapon --fixpgsz --output-all "$(losetup --find "${swapfilename}" --show)" || true

swapon -s || true
free -m -h || true

