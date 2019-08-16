
#FROM debian:testing
FROM debian:stable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Debian build base."


WORKDIR /scratch_base
ADD docker/build_bases/debian_stable /scratch_base


# Install build dependencies.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get --yes update && \
    apt-get --yes install \
      git \
      cmake \
      g++ \
      vim \
      gdb \
      rsync \
      wget \
      patchelf

RUN apt-get install --yes \
    ` # Ygor dependencies ` \
        libhtmlcxx-dev \
        libboost-dev \
        libgsl-dev \
        libeigen3-dev \
    ` # DICOMautomaton dependencies ` \
        libeigen3-dev \
        libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev \
        libz-dev libsfml-dev libjansson-dev \
        libpqxx-dev postgresql-client \
        libcgal-dev \
        libnlopt-dev \
        libnlopt-cxx-dev \
        libasio-dev \
        fonts-freefont-ttf fonts-cmu \
        freeglut3 freeglut3-dev libxi-dev libxmu-dev \
        gnuplot zenity \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
        x-window-system mesa-utils \
        xserver-xorg-video-dummy x11-apps

RUN cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf


# Install Wt from source to get around outdated and buggy Debian package.
#
# Note: Add additional dependencies if functionality is needed -- this is a basic install.
#
# Note: Could also install build-deps for the distribution packages, but the dependencies are not
#       guaranteed to be stable (e.g., major version bumps).
WORKDIR /wt
RUN git clone https://github.com/emweb/wt.git . && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr ../ && \
    make -j$(nproc) && \
    make install && \
    make clean

WORKDIR /scratch_base
RUN apt-get install --yes -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb


# Install Ygor.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN apt-get install --yes -f ./Ygor*deb
#
# Option 2: clone the latest upstream commit.
WORKDIR /ygor
RUN git clone https://github.com/hdclark/Ygor . && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr ../ && \
    make -j$(nproc) && \
    make package && \
    apt-get install --yes -f ./*deb && \
    make clean


# Install Explicator.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN apt-get install --yes -f ./Explicator*deb
#
# Option 2: clone the latest upstream commit.
WORKDIR /explicator
RUN git clone https://github.com/hdclark/explicator . && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=/usr ../ && \
    make -j$(nproc) && \
    make package && \
    apt-get install --yes -f ./*deb && \
    make clean


# Install YgorClustering.
WORKDIR /ygorcluster
RUN git clone https://github.com/hdclark/YgorClustering . && \
    ./install_directly.sh


