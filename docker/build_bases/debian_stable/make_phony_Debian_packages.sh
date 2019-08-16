#!/bin/bash

# This script creates phony/placeholder Debian packages. They are installed in the Docker image so that apt won't
# overwrite a custom upstream package installation with the distribution version.

# sudo apt-get install equivs
# equivs-control libwt-dev.phony
# equivs-control libwthttp-dev.phony
#   <edit the files>
equivs-build libwt-dev.phony 
equivs-build libwthttp-dev.phony 

