#!/bin/bash

set -eu

pacman -S --needed bc     # Needed for linux kernel.
pacman -S --needed musl kernel-headers-musl # Needed for statically-linked busybox.
pacman -S --needed cpio   # Needed to create initramfs.

##############################################
# Linux kernel.
##############################################
if [ ! -f linux.txz ] ; then
    wget 'https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/linux-5.6.5.tar.xz' -O linux.txz
    tar axf linux.txz
fi
cd linux*/

#make help
make clean || true
make x86_64_defconfig
#make menuconfig
make -j $(nproc)
cp arch/x86/boot/bzImage ../
cd ../

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
# Root filesystem.
##############################################
rm -rf rootfs || true
mkdir rootfs
mkdir rootfs/{bin,sbin,lib,dev,run,etc,mnt,proc,sbin,sys,tmp,usr}
#rsync -a ./busybox*/_install/ ./rootfs/
(
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
  sudo mkdir -pv usr/lib
)
(
  cd rootfs/
  ln -s lib lib64
  ln -s ../bin usr/bin
  #mkdir -p usr/lib
  ln -s ../lib usr/lib
  ln -s proc/self/fd dev/fd
  ln -s proc/self/fd/0 stdin
  ln -s proc/self/fd/1 stdout
  ln -s proc/self/fd/2 stderr
  ln -s proc/kcore kcore
)

(
  echo '#!/bin/bash'
  echo 'mkdir -p /proc /sys /tmp'
  echo 'mount -t proc  none /proc'
  echo 'mount -t sysfs none /sys'
  echo 'mount -t tmpfs none /tmp'
# Busybox fix:
#  echo '/sbin/mdev -s'
#  echo '/usr/bin/setsid /bin/cttyhack /bin/sh'
  echo 'exec /bin/bash'
) > rootfs/init
chmod 777 rootfs/init

# Copy the host's glibc.
#(
#  cd rootfs/
#  pacman -Ql bash glibc binutils |
#    sed -e 's/^glibc //g' |
#    sed -e 's/^bash //g' |
#    sort |
#    while read afile ; do
#        if [ -d "$afile" ] ; then
#            sudo mkdir -pv ."$afile"
#        else
#            sudo cp -av "$afile" ."$afile"
#            #rsync -av -n /"$afile" ./rootfs/"$afile"
#        fi
#    done
#)

# Copy binaries and all needed libraries.
# Note that non-library runtime components may be missing!
unsorted=$(mktemp /tmp/unsorted.XXXXXXXXXXXXX)
for f in \
  sh cat cp ls mkdir mknod mktemp mount sed \
  sleep ln rm uname readlink basename bash \
  dirname vi printenv free htop du df \
  dicomautomaton_dispatcher \
  dicomautomaton_webserver \
  dicomautomaton_dump \
  zenity \
  dialog ; do
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
      echo -- cp -aL "${d}${alib}" ./rootfs"${d}${alib}"
      sudo cp -aL "${d}${alib}" ./rootfs"${d}${alib}"
  done

rm $unsorted

## Test via chroot to ensure all requisite libraries are present.
#sudo chroot rootfs/ /bin/bash

## Copy the portable binary distribution into the image.
#rsync -a /tmp/portable_dcma/ ./rootfs/portable_dcma/

rm initramfs.cpio.gz || true
cd rootfs
find . -print0 | cpio -0 -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
cd ../


#qemu-system-x86_64 -kernel bzImage -initrd initramfs.cpio --append "root=/dev/ram init=/init"

# Note: [ctrl]+[a] to access qemu monitor, then type 'quit'<enter> to stop the emulation.
qemu-system-x86_64 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel bzImage \
  -initrd initramfs.cpio.gz \
  --append "root=/dev/ram0 rootfstype=ramfs init=/init console=ttyS0"

