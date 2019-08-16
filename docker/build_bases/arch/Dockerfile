
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
      rsync \
      patchelf && \
    rm -f /var/cache/pacman/pkg/*


# Create a temporary user for building AUR packages.
RUN useradd -r -d /var/empty aurbuild  && \
    mkdir -p /var/empty/ && \
    chown -R aurbuild:aurbuild /var/empty/ && \
    printf '\n''aurbuild ALL=(ALL) NOPASSWD: ALL''\n' >> /etc/sudoers


# Install an AUR helper.
WORKDIR /yay
RUN git clone https://aur.archlinux.org/yay.git . && \
    chown -R aurbuild:aurbuild . && \
    su - aurbuild -c "cd /yay && makepkg --syncdeps --install --clean --needed --noconfirm --rmdeps" && \
    rm -rf /yay


# Install all AUR dependencies.
RUN su - aurbuild -c \
    "yay -S --noconfirm --needed --afterclean --aur htmlcxx ttf-computer-modern-fonts"


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
    freeglut libxi libxmu \
     ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    xorg-server xorg-apps mesa xf86-video-dummy && \
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
    chown -R aurbuild:aurbuild . && \
    su - aurbuild -c "cd /ygor && makepkg --syncdeps --install --clean --needed --noconfirm" && \
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
    chown -R aurbuild:aurbuild . && \
    su - aurbuild -c "cd /explicator && makepkg --syncdeps --install --clean --needed --noconfirm" && \
    git reset --hard && \
    git clean -fxd :/ 


# Install YgorClustering.
WORKDIR /ygorcluster
RUN git clone https://github.com/hdclark/YgorClustering . && \
    chown -R aurbuild:aurbuild . && \
    su - aurbuild -c "cd /ygorcluster && makepkg --syncdeps --install --clean --needed --noconfirm" && \
    git reset --hard && \
    git clean -fxd :/ 


