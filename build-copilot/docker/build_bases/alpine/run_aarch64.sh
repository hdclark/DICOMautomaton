#!/usr/bin/env bash

set -eux

#sudo docker run -it --rm --network=host --platform=linux/arm64 arm64v8/alpine
sudo docker run -it --rm --network=host --platform=linux/arm64 dcma_build_base_alpine_aarch64

