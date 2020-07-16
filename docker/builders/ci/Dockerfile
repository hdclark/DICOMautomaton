
#FROM debian:testing
FROM debian:stable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton minimal CI build."

WORKDIR /scratch
COPY docker/builders/ci /scratch
COPY . /dcma

# Install build dependencies.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update --yes && \
    apt-get install --yes --no-install-recommends \
      git \
      cmake \
      make \
      g++ \
      ncurses-term \
      gdb \
      rsync \
      wget \
      ca-certificates \
      file \
    ` # Ygor dependencies ` \
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
        patchelf && \
        rm -rf /var/lib/apt/lists/*
#    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
#        x-window-system mesa-utils \
#        xserver-xorg-video-dummy x11-apps && \

RUN cp /scratch/xpra-xorg.conf /etc/X11/xorg.conf


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
    JOBS=$(nproc) && \
    JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) && \
    make -j "$JOBS" VERBOSE=1 && \
    make install && \
    make clean

WORKDIR /scratch
RUN apt-get install --yes \
    -f ./libwt-dev_10.0_all.deb ./libwthttp-dev_10.0_all.deb


# Install Ygor.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN apt-get install --yes -f ./Ygor*deb
#
# Option 2: clone the latest upstream commit.
WORKDIR /ygor
RUN git clone https://github.com/hdclark/Ygor . && \
    ./compile_and_install.sh -b build && \
    git reset --hard && \
    git clean -fxd :/ 


# Install Explicator.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN apt-get install --yes -f ./Explicator*deb
#
# Option 2: clone the latest upstream commit.
WORKDIR /explicator
RUN git clone https://github.com/hdclark/explicator . && \
    ./compile_and_install.sh -b build && \
    git reset --hard && \
    git clean -fxd :/ 


# Install YgorClustering.
WORKDIR /ygorcluster
RUN git clone https://github.com/hdclark/YgorClustering . && \
    ./compile_and_install.sh -b build && \
    git reset --hard && \
    git clean -fxd :/ 


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
RUN sed -i -e 's@MEMORY_CONSTRAINED_BUILD=OFF@MEMORY_CONSTRAINED_BUILD=ON@' /dcma/compile_and_install.sh && \
    ./compile_and_install.sh -b build && \
    git reset --hard && \
    git clean -fxd :/ 

# Default to a no-op.
WORKDIR /scratch
CMD ["/bin/false"]

