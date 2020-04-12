
FROM archlinux/base

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton build base for Arch Linux."


WORKDIR /scratch_base
ADD docker/build_bases/arch /scratch_base


# Prepare alternative mirrors.
RUN curl -o /etc/pacman.d/mirrorlist "https://www.archlinux.org/mirrorlist/?country=all&protocol=http&ip_version=4&use_mirror_status=on" && \
    sed -i 's/^#Server/Server/' /etc/pacman.d/mirrorlist


# Install build dependencies.
RUN pacman -Sy --noconfirm archlinux-keyring && \
    pacman -Syu --noconfirm --needed \
      base-devel \
      git \
      cmake \
      gcc \
      vim \
      gdb \
      screen \
    ` # Needed for an AUR helper ` \
      sudo \
      pyalpm \
      wget \
      rsync && \
    rm -f /var/cache/pacman/pkg/*


# Create an unprivileged user for building packages.
RUN useradd -r -d /var/empty builduser  && \
    mkdir -p /var/empty/ && \
    chown -R builduser:builduser /var/empty/ && \
    printf '\n''builduser ALL=(ALL) NOPASSWD: ALL''\n' >> /etc/sudoers


# Install known official dependencies.
RUN pacman -S --noconfirm --needed  \
    gcc-libs \
    gnu-free-fonts \
    sfml \
    jansson \
    libpqxx \
    postgresql \
    gsl \
    boost-libs \
    zlib \
    cgal \
    wt \
    asio \
    nlopt \
    bash-completion \
    patchelf \
    freeglut \
    libxi \
    libxmu \
     ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    xorg-server \
    xorg-apps \
    mesa \
    xf86-video-dummy && \
    rm -f /var/cache/pacman/pkg/*


RUN cp /scratch_base/xpra-xorg.conf /etc/X11/xorg.conf


# Install Ygor.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN pacman -U ./Ygor*deb
#
# Option 2: clone the latest upstream commit.
WORKDIR /ygor
RUN git clone https://github.com/hdclark/Ygor . && \
    chown -R builduser:builduser . && \
    su - builduser -c "cd /ygor && ./compile_and_install.sh -b build" && \
    git reset --hard && \
    git clean -fxd :/ 


# Install Explicator.
#
# Option 1: install a binary package.
#WORKDIR /scratch
#RUN pacman -U ./Explicator*deb
#
# Option 2: clone the latest upstream commit.
WORKDIR /explicator
RUN git clone https://github.com/hdclark/explicator . && \
    chown -R builduser:builduser . && \
    su - builduser -c "cd /explicator && ./compile_and_install.sh -b build" && \
    git reset --hard && \
    git clean -fxd :/ 


# Install YgorClustering.
WORKDIR /ygorcluster
RUN git clone https://github.com/hdclark/YgorClustering . && \
    chown -R builduser:builduser . && \
    su - builduser -c "cd /ygorcluster && ./compile_and_install.sh -b build" && \
    git reset --hard && \
    git clean -fxd :/ 


