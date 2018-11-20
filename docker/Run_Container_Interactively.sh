#!/bin/bash

set -e 

sudo docker run -it --rm \
    -v "$(pwd)":/start/:rw \
    -w /start/ \
    dicomautomaton_webserver:latest \
    /bin/bash 

