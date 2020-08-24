#!/usr/bin/env bash

set -e 

sudo docker run -it --rm \
    --network=host \
    -w /start/ \
    dcma_ci:latest \
    dicomautomaton_dispatcher -h

