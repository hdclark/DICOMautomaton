
#FROM debian:testing
FROM debian:oldstable

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Debian fuzz tester."


WORKDIR /scratch_base
COPY docker/build_bases/debian_oldstable /scratch_base
COPY . /dcma



# Install build dependencies.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update --yes && \
    apt-get install --yes --no-install-recommends \
      git \
      cmake \
      make \
      g++ \
      vim \
      ncurses-term \
      gdb \
      rsync \
      wget \
      ca-certificates

RUN apt-get install --yes --no-install-recommends \
    ` # Ygor dependencies ` \
        libboost-dev \
        libgsl-dev \
        libeigen3-dev \
    ` # DICOMautomaton dependencies ` \
        libeigen3-dev \
        libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-program-options-dev libboost-thread-dev \
        libz-dev libsfml-dev \
        libsdl2-dev libglew-dev \
        libjansson-dev \
        libpqxx-dev postgresql-client \
        libcgal-dev \
        libnlopt-dev \
        libnlopt-cxx-dev \
        libasio-dev \
        fonts-freefont-ttf fonts-cmu \
        freeglut3 freeglut3-dev libxi-dev libxmu-dev \
        gnuplot zenity \
        patchelf bash-completion \
    ` # Additional dependencies for headless OpenGL rendering with SFML ` \
        x-window-system mesa-utils \
        xserver-xorg-video-dummy x11-apps \
    ` # Fuzzing tools ` \
        afl-clang

RUN cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf

# Tweak the fuzzer settings.
ENV AFL_HARDEN 1
#ENV AFL_USE_ASAN 1
#ENV AFL_USE_MSAN 1
ENV AFL_QUIET 1
ENV AFL_SKIP_CPUFREQ 1

# Install Wt from source to get around outdated and buggy Debian package.
#
# Note: Add additional dependencies if functionality is needed -- this is a basic install.
#
# Note: Could also install build-deps for the distribution packages, but the dependencies are not
#       guaranteed to be stable (e.g., major version bumps).
WORKDIR /wt
RUN git clone https://github.com/emweb/wt.git . && \
    mkdir -p build && cd build && \
    cmake -E env CC=afl-clang CXX=afl-clang++ \
      cmake -DCMAKE_INSTALL_PREFIX=/usr ../ && \
    JOBS=$(nproc) && \
    JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) && \
    make -j "$JOBS" VERBOSE=1 && \
    make install && \
    make clean

WORKDIR /scratch_base
RUN apt-get install --yes --no-install-recommends \
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
    mkdir -p build && cd build && \
    cmake -E env CC=afl-clang CXX=afl-clang++ \
      cmake -DCMAKE_INSTALL_PREFIX=/usr ../ && \
    JOBS=$(nproc) && \
    JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) && \
    make -j "$JOBS" VERBOSE=1 && \
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
    cmake -E env CC=afl-clang CXX=afl-clang++ \
      cmake -DCMAKE_INSTALL_PREFIX=/usr ../ && \
    JOBS=$(nproc) && \
    JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) && \
    make -j "$JOBS" VERBOSE=1 && \
    make package && \
    apt-get install --yes -f ./*deb && \
    make clean


# Install YgorClustering.
WORKDIR /ygorcluster
RUN git clone https://github.com/hdclark/YgorClustering . && \
    ./compile_and_install.sh -b build


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
    cmake -E env CC=afl-clang CXX=afl-clang++ CXXFLAGS=" -DDCMA_FUZZ_TESTING " \
      cmake \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_SYSCONFDIR=/etc \
        ../ && \
    JOBS=$(nproc) && \
    JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) && \
    make -j "$JOBS" VERBOSE=1 dicomautomaton_dispatcher && \
    make package dicomautomaton_dispatcher && \
    apt-get install --yes -f ./*deb && \
    make clean


