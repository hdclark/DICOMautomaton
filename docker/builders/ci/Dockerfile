
FROM archlinux/base

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton minimal CI build."


WORKDIR /scratch
ADD docker/builders/ci /scratch
ADD . /dcma


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
    ` # Needed for an AUR helper ` \
      sudo \
      pyalpm \
      wget \
      rsync && \
    rm -f /var/cache/pacman/pkg/*


# Create a temporary user for building AUR packages.
RUN useradd -m -r -d /var/empty aurbuild  && \
    mkdir -p /var/empty/ && \
    chown -R aurbuild:aurbuild /var/empty/ && \
    printf '\n''aurbuild ALL=(ALL) NOPASSWD: ALL''\n' >> /etc/sudoers


# Install hard build dependencies.
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
    nlopt && \
    rm -f /var/cache/pacman/pkg/*


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
RUN chown -R aurbuild:aurbuild . && \
    sed -i -e 's@MEMORY_CONSTRAINED_BUILD=OFF@MEMORY_CONSTRAINED_BUILD=ON@' /dcma/PKGBUILD && \
    su - aurbuild -c "cd /dcma && makepkg --syncdeps --install --clean --needed --noconfirm" && \
    git reset --hard && \
    git clean -fxd :/ 


# Default to launching the webserver.
WORKDIR /scratch
CMD ["/bin/false"]


