#!/usr/bin/env bash
set -eu
shopt -s nocasematch

# This script will attempt to install runtime dependencies needed to run AppImages.

DISTRIBUTION="auto" # the distribution to assume.

INSTALLVIASUDO="yes" # whether to use sudo during installation.

# Argument parsing:
OPTIND=1 # Reset in case getopts has been used previously in the shell.
while getopts "d:uh" opt; do
    case "$opt" in
    h)
        printf 'This script attempts to install runtime dependencies needed to run'
        printf ' DICOMautomaton AppImages on common Linux distributions.\n'
        printf "\n"
        printf "Usage: \n\t $0 <args> \n"
        printf "\n"
        printf "Available arguments: \n"
        printf "\n"
        printf " -h       : Display usage information and terminate.\n"
        printf "\n"
        printf " -d <arg> : The Linux distribution/environment to assume.\n"
        printf "          : Options are 'auto' (i.e., automatic detection), 'debian' (oldstable),\n"
        printf "          : 'ubuntu' (latest), 'arch', and 'fedora' (latest).\n"
        printf "          : Default: '%s'\n" "${DISTRIBUTION}"
        printf "\n"
        printf " -u       : Attempt unprivileged installation (i.e., without using sudo).\n"
        printf "          : Default: sudo will be used: '%s'\n" "${INSTALLVIASUDO}"
        printf "\n"
        exit 0
        ;;
    d)  DISTRIBUTION="$OPTARG"
        printf 'Proceeding with user-specified distribution "%s".\n' "${DISTRIBUTION}"
        ;;
    u)  INSTALLVIASUDO="no"
        printf 'Disabling use of sudo.\n' "${INSTALLVIASUDO}"
        ;;
    esac
done
#shift $((OPTIND-1))
#[ "$1" = "--" ] && shift
#echo "Leftover arguments: $@"

# Determine which distribution/environment to assume.
if [[ "${DISTRIBUTION}" =~ .*auto.* ]] &&
   [ -e /etc/os-release ] ; then
    DISTRIBUTION=$( bash -c '. /etc/os-release && printf "${NAME}"' )
fi

# Move to the repository root.
REPOROOT="$(git rev-parse --show-toplevel || true)"
if [ ! -d "${REPOROOT}" ] ; then

    # Fall-back on the source position of this script.
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}/../"
fi
cd "${REPOROOT}"

# Determine how we will escalate privileges.
SUDO="sudo"
if [[ $EUID -eq 0 ]] ; then
    SUDO="" # no privileges needed.
fi
if [ "${INSTALLVIASUDO}" != "yes" ] ; then
    SUDO=""
fi
#################

retry_count=0
retry_limit=5

if [[ "${DISTRIBUTION}" =~ .*[dD]ebian.* ]] ; then
    printf 'Installing dependencies for Debian...\n'
    export DEBIAN_FRONTEND='noninteractive'

    until $SUDO apt-get update --yes && \
          $SUDO apt-get install --yes --no-install-recommends \
            bash git rsync \
            wget ca-certificates openssl \
            mesa-utils libfreetype6 libsdl2-dev libice-dev libsm-dev libopengl0 g++
    do
        (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
        printf 'Waiting to retry.\n' && sleep 5
    done

elif [[ "${DISTRIBUTION}" =~ .*[uU]buntu.* ]] ; then
    printf 'Installing dependencies for Ubuntu...\n'
    export DEBIAN_FRONTEND='noninteractive'

    until $SUDO apt-get update --yes && \
          $SUDO apt-get install --yes --no-install-recommends \
            bash git rsync \
            wget ca-certificates libssl-dev \
            mesa-utils libfreetype6 libsdl2-dev libice-dev libsm-dev libopengl0 g++
    do
        (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
        printf 'Waiting to retry.\n' && sleep 5
    done

elif [[ "${DISTRIBUTION}" =~ .*[aA]rch.* ]] ; then
    printf 'Installing dependencies for Arch Linux...\n'

    # NOTE: may be required for older Arch Linux images.
    #- "curl -o /etc/pacman.d/mirrorlist 'https://archlinux.org/mirrorlist/?country=all&protocol=http&ip_version=4&use_mirror_status=on'"
    #- "sed -i 's/^#Server/Server/' /etc/pacman.d/mirrorlist"
    #- "sed -i -e 's/SigLevel[ ]*=.*/SigLevel = Never/g' -e 's/.*IgnorePkg[ ]*=.*/IgnorePkg = archlinux-keyring/g' /etc/pacman.conf"
    until $SUDO pacman -Syu --noconfirm --needed \
            bash git rsync \
            wget ca-certificates openssl \
            mesa freetype2 sdl2 alsa-lib libice libsm libglvnd gcc-libs
    do
        (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
        printf 'Waiting to retry.\n' && sleep 5
    done

elif [[ "${DISTRIBUTION}" =~ .*[fF]edora.* ]] ; then
    printf 'Installing dependencies for Fedora...\n'

    until $SUDO dnf -y upgrade && \
          $SUDO dnf -y install \
            bash git rsync \
            wget ca-certificates openssl \
            findutils mesa-libGL freetype SDL2 alsa-lib libICE libSM libgcc libglvnd-opengl libstdc++
    do
        (( retry_limit < retry_count++ )) && printf 'Exceeded retry limit\n' && exit 1
        printf 'Waiting to retry.\n' && sleep 5
    done

else
    printf 'Linux distribution not recognized. Cannot continue.\n'
    exit 1
fi

ret_val=$?
printf 'Done.\n'
exit $ret_val

