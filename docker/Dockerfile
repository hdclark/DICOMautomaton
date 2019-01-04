
FROM base/archlinux

LABEL maintainer="http://www.halclark.ca" \
      description="DICOMautomaton Web Server."

EXPOSE 80
EXPOSE 443

WORKDIR /scratch
ADD ./docker /scratch
ADD . /dcma


# Prepare alternative mirrors.
RUN printf -- 'Server = http://mirror.cedille.club/archlinux/$repo/os/$arch\n' > /etc/pacman.d/mirrorlist \
    printf -- 'Server = http://mirrors.xmission.com/archlinux/$repo/os/$arch\n' >> /etc/pacman.d/mirrorlist \
    printf -- 'Server = http://archlinux.mirror.rafal.ca/$repo/os/$arch\n' >> /etc/pacman.d/mirrorlist

# Install build dependencies.
RUN pacman -Sy --noconfirm archlinux-keyring && \
    pacman -Syu --noconfirm --needed base-devel git cmake gcc vim gdb screen \
    ` # Needed for an AUR helper ` \
    sudo pyalpm wget rsync && \
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
    "yay -S --noconfirm --needed --afterclean --aur htmlcxx ttf-computer-modern-fonts nlopt" && \
    pacman -Rc --noconfirm octave


# Install known official dependencies.
RUN pacman -S --noconfirm --needed  \
    gcc-libs \
    ttf-freefont \
    sfml \
    jansson \
    libpqxx \
    postgresql \
    freeglut \
    gsl \
    boost-libs \
    zlib \
    cgal \
    wt \
    asio \
    freeglut libxi libxmu \
     ` # Additional dependencies for headless OpenGL rendering with SFML ` \
    xorg-server xorg-apps mesa xf86-video-dummy && \
    rm -f /var/cache/pacman/pkg/*

RUN mv /scratch/xpra-xorg.conf /etc/X11/xorg.conf


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
    su - aurbuild -c "cd /dcma && makepkg --syncdeps --install --clean --needed --noconfirm" && \
    git reset --hard && \
    git clean -fxd :/ 


# Default to launching the webserver.
WORKDIR /scratch
CMD ["/bin/bash", "/scratch/Run_WebServer.sh"]


