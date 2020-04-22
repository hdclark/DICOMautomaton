#!/bin/bash

set -eu

sudo pacman -S --needed bc || true    # Needed for linux kernel.
#sudo pacman -S --needed musl kernel-headers-musl || true # Needed for statically-linked busybox.
sudo pacman -S --needed cpio || true  # Needed to create initramfs.

##############################################
# Linux kernel.
##############################################
if [ ! -d linux*/ ] || [ ! -f bzImage ] ; then
    wget 'https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/linux-5.6.5.tar.xz' -O linux.txz
    tar axf linux.txz
    (
      cd linux*/

      #make help
      make clean || true
      make x86_64_defconfig
      #make menuconfig
      make -j $(nproc)
      cp arch/x86/boot/bzImage ../
    )
fi

###############################################
## Busybox.
###############################################
#if [ ! -f busybox.tar.bz2 ] ; then
#    wget 'https://busybox.net/downloads/busybox-1.31.1.tar.bz2' -O busybox.tar.bz2
#    tar axf busybox.tar.bz2
#fi
#cd busybox*/
#
##make help
#make clean || true
#make defconfig
##make menuconfig
#
#cp .config .config_defconfig
#sed -i \
#  -e 's@.*CONFIG_STATIC.*@CONFIG_STATIC=y@g' \
#  -e 's@.*CONFIG_BASH_IS_NONE.*@# CONFIG_BASH_IS_NONE is not set@g' \
#  -e 's@.*CONFIG_BASH_IS_HUSH.*@CONFIG_BASH_IS_HUSH=y@g' \
#  -e 's@.*CONFIG_PREFIX.*@CONFIG_PREFIX="./_install"@g' \
#  -e 's@.*CONFIG_DESKTOP=.*@CONFIG_DESKTOP=n@g' \
#  -e 's@.*CONFIG_EXTRA_CFLAGS.*@CONFIG_EXTRA_CFLAGS="-m64"@g' \
#  -e 's@.*CONFIG_EXTRA_LDFLAGS.*@CONFIG_EXTRA_LDFLAGS="-m64"@g' \
#  .config
#
#make -j $(nproc) CC=musl-gcc install # Note: installs to a local directory. See config above.
##make -j $(nproc) install # Note: installs to a local directory. See config above.
##cp busybox ../
#cd ../

##############################################
# Root filesystem: install packages.
##############################################
sudo rm -rf rootfs || true
#rsync -a ./busybox*/_install/ ./rootfs/

# Rely on system package manager to install a usable base system.
(
  mkdir -p ./rootfs/var/lib/pacman/sync/
  mkdir -p ./rootfs/var/cache/pacman/pkg/
  mkdir -p ./rootfs/var/log/
  sudo rsync -avP /var/lib/pacman/sync/ ./rootfs/var/lib/pacman/sync/
  sudo pacman --root=./rootfs/ --cachedir=./rootfs/var/cache/pacman/pkg/ -S --noconfirm --overwrite='*' filesystem
)
(
  mkdir -pv rootfs/dev
  cd rootfs/dev
  sudo mknod ram0 b 1 0
  sudo mknod ram1 b 1 1
  sudo mknod random c 1 8
  sudo mknod urandom c 1 9
  sudo mknod tty0 c 4 0
  sudo mknod ttyS0 c 4 64
  sudo mknod ttyS1 c 4 65
  sudo mknod tty c 5 0
  sudo mknod -m 664 null c 1 3
  sudo mknod zero c 1 5
  sudo mknod kmsg c 1 11
  sudo mknod -m 640 console c 5 1
)
(
  sudo pacman --root=./rootfs/ --cachedir=./rootfs/var/cache/pacman/pkg/ -S --noconfirm --overwrite='*' \
    util-linux \
    coreutils \
    psmisc \
    bash \
    grep \
    sed \
    vim \
    xorg-server \
    xorg-xinit \
    xf86-video-fbdev \
    xf86-video-vesa \
    mesa \
    awesome \
    noto-fonts \
    screen \
    xterm \
    zenity \
    dialog \
    pacman \
    pacman-mirrorlist \
    wget

#    lightdm \

    #xorg-apps \
    # The following causes installation to hang due to gnupg/dirmngr (?)
    # It also install a lot of unnecessary stuff.
    #base \

  #sudo pacman --root=./rootfs/ -U --noconfirm \
  #  /tmp/dicomautomaton-...-x86_64.pkg.tar.xz \
  #  /tmp/explicator-...-x86_64.pkg.tar.xz \
  #  /tmp/ygorclustering-...-x86_64.pkg.tar.xz

#  sudo rm -rf ./rootfs/var/lib/pacman || true
)
(
  cd rootfs/
  #sudo mkdir -pv usr/lib
  #[ ! -e lib64      ] && sudo ln -s lib lib64
  #[ ! -e usr/bin    ] && sudo ln -s ../bin usr/bin
  #[ ! -e usr/lib    ] && sudo ln -s ../lib usr/lib
  [ ! -e dev/fd     ] && sudo ln -s proc/self/fd dev/fd
  [ ! -e dev/stdin  ] && sudo ln -s proc/self/fd/0 dev/stdin
  [ ! -e dev/stdout ] && sudo ln -s proc/self/fd/1 dev/stdout
  [ ! -e dev/stderr ] && sudo ln -s proc/self/fd/2 dev/stderr
  [ ! -e dev/kcore  ] && sudo ln -s proc/kcore dev/kcore
)

#mkdir -pv rootfs/{bin,sbin,lib,dev,run,etc,mnt,proc,sbin,sys,tmp,usr}

# Make a minimal init process.
(
  echo '#!/bin/bash'
  echo 'mkdir -p /proc /sys /tmp'
  echo 'mount -t proc  none /proc'
  echo 'mount -t sysfs none /sys'
  echo 'mount -t tmpfs none /tmp'
# Busybox fix:
#  echo '/sbin/mdev -s'
#  echo '/usr/bin/setsid /bin/cttyhack /bin/sh'

  # Launch into a shell.
  #echo 'exec /bin/bash'

  # Defer system init to systemd. This simplifies, e.g., allocating terminals.
  echo 'exec /usr/lib/systemd/systemd'
) > rootfs/init
chmod 777 rootfs/init

# Use ldd to ensure individual binaries are included with all necessary libraries.
#
# Note that non-library runtime components may be missing!
unsorted=$(mktemp /tmp/unsorted.XXXXXXXXXXXXX)
for f in \
  `# sh cat cp ls mkdir mknod mktemp mount sed` \
  `# sleep ln rm uname readlink basename bash` \
  `# dirname vi printenv free htop du df` \
  dicomautomaton_dispatcher \
  dicomautomaton_webserver \
  dicomautomaton_dump \
  poweroff ; do

    d="/bin"
    if   [ -e "/bin/$f" ] ; then   d="/bin"
    elif [ -e "/sbin/$f" ] ; then  d="/sbin"
    else
        printf 'Unable to locate host binary. Cannot continue.\n' 1>&2
        exit 1
    fi
    ldd "$d/$f" | sed -e 's/\t//g' | cut -d' ' -f1 >> $unsorted
    sudo cp -aL "$d/$f" ./rootfs/bin/
done

sort $unsorted |
  uniq |
  while read alib ; do
      if [[ "$alib" == linux-vdso.so.1 ]] ||
         [[ "$alib" == linux-gate.so.1 ]] ||
         [[ "$alib" == libsystemd-shared* ]] ; then
          continue
      fi
      d="/lib/"
      if   [ -e "$alib" ] ; then            d="" # Library already contains a prefix.
      elif [ -e "/lib/$alib" ] ; then       d="/lib/"
      elif [ -e "/lib64/$alib" ] ; then     d="/lib64/"
      elif [ -e "/usr/lib/$alib" ] ; then   d="/usr/lib/"
      else
          printf 'Unable to locate library "%s". Cannot continue.\n' "$alib" 1>&2
          exit 1
      fi
      sudo mkdir -p ./rootfs"${d}"
      sudo cp -avL "${d}${alib}" ./rootfs"${d}${alib}"
  done
rm $unsorted

# Query the package database for non-library runtime components that may be missing.
for pkg in \
  ygorclustering \
  explicator \
  ygor \
  dicomautomaton ; do

    pacman -Ql "$pkg" |
      cut -d' ' -f2- |
      grep -v '.*[/]$' |
      sudo rsync -avP --no-r -R --files-from=- / ./rootfs/
done

## Copy the portable binary distribution into the image.
#rsync -a /tmp/portable_dcma/ ./rootfs/portable_dcma/

##############################################
# Configure system.
##############################################
## Launch an xterm when X launches.
#sudo mkdir -pv ./rootfs/root/
#(
#  printf -- '#!/bin/bash\n'
#  printf -- 'while true ; do xterm ; sleep 1 ; done\n'
#) | sudo tee ./rootfs/root/.xinitrc

#  Autologin.
sudo mkdir -pv ./rootfs/etc/systemd/system/getty@tty1.service.d/
#sudo touch ./rootfs/etc/systemd/system/getty@tty1.service.d/override.conf
(
  printf -- '[Service]\n'
  printf -- 'ExecStart=\n'
  printf -- 'ExecStart=-/usr/bin/agetty --autologin root --noclear %%I $TERM\n'
) | sudo tee ./rootfs/etc/systemd/system/getty@tty1.service.d/override.conf

# Networking.
sudo mkdir -pv ./rootfs/etc/systemd/network/
(
  printf -- '[Match]\n'
  printf -- 'Name=en*\n'
  printf -- '\n'
  printf -- '[Network]\n'
  printf -- 'DHCP=ipv4\n'
) | sudo tee ./rootfs/etc/systemd/network/20-wired.network

## Display management.
#sudo sed -i -e 's/[#]autologin-user.*/autologin-user=root/g' \
#            -e 's/[#]user-session.*/user-session=awesome.desktop/g' \
#  ./rootfs/usr/lightdm/lightdm.conf

# Window management.
sudo rm ./rootfs/etc/X11/xinit/xinitrc
(
  printf -- '#!/bin/bash\n'
  printf -- 'while true ; do awesome ; sleep 1 ; done\n'
) | sudo tee ./rootfs/etc/X11/xinit/xinitrc
#(
#  cd rootfs
#  sudo mkdir -pv etc/systemd/system/
#  sudo ln -s /usr/lib/systemd/system/graphical.target \
#             etc/systemd/system/default.target
#)
sudo mkdir -pv ./rootfs/root/
(
  printf -- 'if [ ! $DISPLAY ] ; then startx ; fi\n'
) | sudo tee ./rootfs/etc/skel/.bash_profile
sudo cp ./rootfs/etc/skel/.bash_profile ./rootfs/root/

sudo sed -i -e 's/^theme[.]wallpaper.*//g' \
  ./rootfs/usr/share/awesome/themes/default/theme.lua

##############################################
# Jettison unneeded components.
##############################################
#sudo rm -rf ./rootfs/usr/share/* || true


##############################################
# Create disk image.
##############################################

## Test via chroot to ensure all requisite libraries are present.
#sudo chroot rootfs/ /bin/bash

# Prepare the cpio archive.
sudo rm initramfs.cpio* || true
cd rootfs
#find . -print0 | sudo cpio -0 -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
find . -print0 | sudo cpio -0 -ov --format=newc > ../initramfs.cpio
cd ../

##############################################
# Emulate system.
##############################################
#qemu-system-x86_64 -kernel bzImage -initrd initramfs.cpio --append "root=/dev/ram init=/init"

# Note: [ctrl]+[a] to access qemu monitor, then type 'quit'<enter> to stop the emulation.
#qemu-system-x86_64 \
#  -m 4G \
#  -smp 2 \
#  -nographic \
#  -kernel bzImage \
#  -initrd initramfs.cpio \
#  --append "root=/dev/ram0 rootfstype=ramfs init=/init console=ttyS0"

qemu-system-x86_64 \
  -m 4G \
  -smp 2 \
  -kernel bzImage \
  -initrd initramfs.cpio \
  --append "root=/dev/ram0 rootfstype=ramfs init=/init"

