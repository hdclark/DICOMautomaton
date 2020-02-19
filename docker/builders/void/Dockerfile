
FROM dcma_build_base_void:latest

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Web Server built from Void Linux."

EXPOSE 80
EXPOSE 443

WORKDIR /scratch
ADD ./docker/builders/void /scratch
ADD . /dcma



# Install DICOMautomaton.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN apt-get install --yes -f ./DICOMautomaton*deb 
#
# Option 2: clone the latest upstream commit.
#WORKDIR /dcma
#RUN git clone https://github.com/hdclark/DICOMautomaton . && \
#   ...
#
# Option 3: use the working directory.
WORKDIR /dcma
RUN mkdir -p build && \
    cd build && \
    cmake \
      -DCMAKE_INSTALL_PREFIX=/usr \
      ../ && \
    make -j$(( $(nproc) < 8 ? $(nproc) : 8 )) VERBOSE=1 && \
    make install && \
    make clean


# Default to launching the webserver.
WORKDIR /scratch
CMD ["/bin/bash", "/scratch/Run_WebServer.sh"]

