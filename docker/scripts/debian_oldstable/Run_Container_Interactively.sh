#!/usr/bin/env bash

set -e 

sudo docker run -it --rm \
    --network=host \
    -v "$(pwd)":/start/:rw \
    -w /start/ \
    dicomautomaton_webserver_debian_oldstable:latest \
    /bin/bash 

