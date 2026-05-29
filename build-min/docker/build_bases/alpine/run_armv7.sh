#!/usr/bin/env bash

set -eux

#sudo docker run -it --rm --network=host --platform=linux/arm/v7 arm32v7/alpine
sudo docker run -it --rm --network=host --platform=linux/arm/v7 dcma_build_base_alpine_armv7

