#!/usr/bin/env bash

set -e 

sudo docker run -it --rm \
    --network=host \
    --memory 4G \
    --memory-swap 4G \
    --cpus=4 \
    -v "$(pwd)":/start/:rw \
    -w /start/ \
    dcma_fuzz_testing_debian_stable:latest \
    /bin/bash 

