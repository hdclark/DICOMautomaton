#!/usr/bin/env bash

set -eux

# Register binfmt-misc qemu emulation so we can enter Docker images with virtual architecture.
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

