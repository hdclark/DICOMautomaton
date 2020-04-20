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
make -j $(nproc)
cp arch/x86/boot/bzImage ../
cd ../

##############################################
# Busybox.
##############################################
if [ ! -f busybox.tar.bz2 ] ; then
    wget 'https://busybox.net/downloads/busybox-1.31.1.tar.bz2' -O busybox.tar.bz2
    tar axf busybox.tar.bz2
fi
cd busybox*/

#make help
make clean || true
make defconfig
#make menuconfig

cp .config .config_defconfig
sed -i \
  -e 's@.*CONFIG_STATIC.*@CONFIG_STATIC=y@g' \
  -e 's@.*CONFIG_BASH_IS_NONE.*@# CONFIG_BASH_IS_NONE is not set@g' \
  -e 's@.*CONFIG_BASH_IS_HUSH.*@CONFIG_BASH_IS_HUSH=y@g' \
  -e 's@.*CONFIG_PREFIX.*@CONFIG_PREFIX="./_install"@g' \
  -e 's@.*CONFIG_DESKTOP=.*@CONFIG_DESKTOP=n@g' \
  -e 's@.*CONFIG_EXTRA_CFLAGS.*@CONFIG_EXTRA_CFLAGS="-m64"@g' \
  -e 's@.*CONFIG_EXTRA_LDFLAGS.*@CONFIG_EXTRA_LDFLAGS="-m64"@g' \
  .config

make -j $(nproc) CC=musl-gcc install # Note: installs to a local directory. See config above.
#cp busybox ../
cd ../

##############################################
# Root filesystem.
##############################################
rm -rf rootfs || true
mkdir rootfs
rsync -a ./busybox*/_install/ ./rootfs/
cp ./rootfs/bin/hush ./rootfs/bin/bash
#chmod 777 ./rootfs/bin/busybox
mkdir rootfs/dev
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
sudo mknod null c 1 3
sudo mknod zero c 1 5
sudo mknod kmsg c 1 11
sudo mknod console c 5 1
)

rsync -a /tmp/portable_dcma/ ./rootfs/portable_dcma/

(
cat <<'EOF'
#!/bin/sh
echo "Running simple init now..."
mkdir /proc /sys /tmp
mount -t proc  none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
/usr/bin/setsid /bin/cttyhack /bin/sh
exec /bin/sh #sbin/init
EOF
) > rootfs/init
chmod 777 rootfs/init


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
