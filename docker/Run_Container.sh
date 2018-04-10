#!/bin/bash

mkdir -p /home/hal/client_uploads/

sudo docker run -it --rm -p 8080:80 \
    -v /home/hal/client_uploads/:/client_uploads:rw \
    dicomautomaton_webserver:latest

#    --entrypoint /bin/bash \
