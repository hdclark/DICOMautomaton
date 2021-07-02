#!/usr/bin/env bash

# This script installs all dependencies needed to build DICOMautomaton starting with a minimal Debian stable system.
set -eu #x
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
elif [ -d "${OUT_DIRECTORY}" ] ; then
    printf 'Chroot directory "%s" exists. Refusing to overwrite it.\n' 1>&2
    exit 1
elif [ -f "${OUT_SCRIPT}" ] ; then
    printf 'Script file "%s" exists. Refusing to overwrite it.\n' 1>&2
    exit 1
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

export DEBIAN_FRONTEND="noninteractive"
apt-get update --yes
apt-get install --yes \
  debootstrap \
  qemu \
  proot \
  qemu-user-static 
#  binfmt-support \

debootstrap --arch="${DEBIAN_ARCHITECTURE}" --foreign stable "${OUT_DIRECTORY}" 'http://deb.debian.org/debian/'

cat << EOF > "${OUT_SCRIPT}"
#!/usr/bin/env bash
proot \\
  -q "qemu-${ARCHITECTURE}-static" \\
  -0 \\
  -r "${OUT_DIRECTORY}" \\
  -b /dev/ \\
  -b /sys/ \\
  -b /proc/ \\
  /usr/bin/env \\
    -i \\
    HOME=/root \\
    TERM="xterm-256color" \\
    PATH=/bin:/usr/bin:/sbin:/usr/sbin \\
    /bin/bash -c "\$@"
EOF
chmod 777 "${OUT_SCRIPT}"

"${OUT_SCRIPT}" '/debootstrap/debootstrap --second-stage'

printf 'Outside the chroot:\n'
uname -a
printf 'Inside the chroot:\n'
"${OUT_SCRIPT}" 'uname -a'

printf "Chroot '%s' can be accessed via the script '%s'.\n" "${OUT_DIRECTORY}" "${OUT_SCRIPT}"

