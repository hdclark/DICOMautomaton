#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Debian stable system.
set -eux
shopt -s nocasematch

if [ ! "$EUID" -eq 0 ] ; then
    printf "This script should be run as root. Refusing to continue.\n" 1>&2
    exit 1
fi

DEBIAN_ARCHITECTURE="arm64"
OUT_DIRECTORY="/debian_chroot"
OUT_SCRIPT="/run_within_debian_chroot.sh"

# Argument parsing:
OPTIND=1 # Reset in case getopts has been used previously in the shell.
while getopts "ha:d:s:" opt; do
    case "$opt" in
    h)
        printf 'This script creates a Debian chroot with the given architecture.'
        printf ' It can be used to cross-compile for foreign architectures.\n'
        printf "\n"
        printf "Usage: \n\t $0 <args> \n"
        printf "\n"
        printf "Available arguments: \n"
        printf "\n"
        printf " -h       : Display usage information and terminate.\n"
        printf "\n"
        printf " -d <dir> : The directory that will contain the chroot.\n"
        printf "          : Default: '%s'\n" "${OUT_DIRECTORY}"
        printf "\n"
        printf " -s <arg> : The name of a helper script that will be created to run commands within the chroot.\n"
        printf "          : Default: '%s'\n" "${OUT_SCRIPT}"
        printf "\n"
        printf " -a <arg> : The system architecture to use. Uses Debian nomenclature.\n"
        printf "          : For example, 'amd64', 'arm64', 'armhf'. Note that Debian refers to 'aarch64' as 'arm64'.\n"
        printf "          : Default: '%s'\n" "${DEBIAN_ARCHITECTURE}"
        printf "\n"
        exit 0
        ;;
    s)  OUT_SCRIPT="$OPTARG"
        printf 'Proceeding with user-specified script "%s".\n' "${OUT_SCRIPT}"
        ;;
    d)  OUT_DIRECTORY="$OPTARG"
        printf 'Proceeding with user-specified directory "%s".\n' "${OUT_DIRECTORY}"
        ;;
    a)  DEBIAN_ARCHITECTURE="$OPTARG"
        printf 'Proceeding with user-specified architecture "%s".\n' "${DEBIAN_ARCHITECTURE}"
        ;;
    esac
done
#shift $((OPTIND-1))
#[ "$1" = "--" ] && shift
#echo "Leftover arguments: $@"

if [ -z "${DEBIAN_ARCHITECTURE}" ] ; then
    printf 'Empty architecture. Refusing to continue.\n' 1>&2
    exit 1
elif [ "$(readlink -f "${OUT_DIRECTORY}")" == "/" ] ; then
    # Proceeding with build root at '/' will destroy the filesystem if the 'clean' option is used.
    printf 'Chroot resolves to "/". Refusing to continue.\n' 1>&2
    exit 1
#elif [ -d "${OUT_DIRECTORY}" ] ; then
#    printf 'Chroot directory "%s" exists. Refusing to overwrite it.\n' 1>&2
#    exit 1
#elif [ -f "${OUT_SCRIPT}" ] ; then
#    printf 'Script file "%s" exists. Refusing to overwrite it.\n' 1>&2
#    exit 1
fi

ARCHITECTURE=""
if [[ "${DEBIAN_ARCHITECTURE}" =~ .*amd64.* ]] ; then
    ARCHITECTURE="x86_64"
elif [[ "${DEBIAN_ARCHITECTURE}" =~ .*arm64.* ]] ; then
    ARCHITECTURE="aarch64"
elif [[ "${DEBIAN_ARCHITECTURE}" =~ .*armhf.* ]] ; then
    ARCHITECTURE="arm"
else
    printf 'Debian architecture name not recognized. Assuming qemu will recognize it...\n' 1>&2
    ARCHITECTURE="${DEBIAN_ARCHITECTURE}"
fi

DISTRIBUTION="debian"
if [ -e /etc/os-release ] ; then
    DISTRIBUTION=$( sh -c '. /etc/os-release && printf "${NAME}"' )
fi

function command_exists () {
    type "$1" &> /dev/null ;
}
if [[ "${DISTRIBUTION}" =~ .*debian.* ]] ; then
    export DEBIAN_FRONTEND="noninteractive"
    command_exists debootstrap                    || apt-get install --yes debootstrap 
    command_exists "qemu-${ARCHITECTURE}-static"  || apt-get install --yes qemu qemu-user-static
    command_exists proot                          || apt-get install --yes proot

elif [[ "${DISTRIBUTION}" =~ .*arch.* ]] ; then
    command_exists debootstrap                    || pacman -S --needed --noconfirm debootstrap 
    command_exists "qemu-${ARCHITECTURE}-static"  || yay    -S --needed --noconfirm qemu qemu-arch-extra qemu-user-static-bin
    command_exists proot                          || yay    -S --needed --noconfirm proot

else
    printf 'Unsupported host system. Cannot continue.\n' 1>&2
    exit 1
fi

cat << EOF > "${OUT_SCRIPT}"
#!/usr/bin/env sh
export PROOT_NO_SECCOMP=1 
exec \\
proot \\
  -q "qemu-${ARCHITECTURE}-static" \\
  -0 \\
  -r "${OUT_DIRECTORY}" \\
  --pwd=/ \\
  -b /dev/ \\
  -b /sys/ \\
  -b /proc/ \\
  /usr/bin/env \\
    -i \\
    HOME=/ \\
    TERM="xterm-256color" \\
    PATH=/bin:/usr/bin:/sbin:/usr/sbin \\
    /bin/sh -c "\$@"
EOF
chmod 777 "${OUT_SCRIPT}"

# Only create the chroot if it does not already exist. This allows for better re-use of the chroot.
if [ ! -d "${OUT_DIRECTORY}" ] ; then
    debootstrap --arch="${DEBIAN_ARCHITECTURE}" --foreign stable "${OUT_DIRECTORY}" 'http://deb.debian.org/debian/'
    "${OUT_SCRIPT}" '/debootstrap/debootstrap --second-stage'
fi
"${OUT_SCRIPT}" 'DEBIAN_FRONTEND="noninteractive" apt-get update --yes'
"${OUT_SCRIPT}" 'DEBIAN_FRONTEND="noninteractive" apt-get install --yes --no-install-recommends coreutils binutils findutils rsync openssh-client patchelf'

printf 'Outside the chroot:\n'
uname -a
printf 'Inside the chroot:\n'
"${OUT_SCRIPT}" 'uname -a'

printf "Chroot '%s' can be accessed via the script '%s'.\n" "${OUT_DIRECTORY}" "${OUT_SCRIPT}"

