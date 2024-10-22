#!/usr/bin/env bash
set -eu
shopt -s nocasematch

# This script will clone the current repository and then remotely build.
# The build artifacts are retained and portable binaries are returned to the local machine.
# They may or may not be installed remotely.
# This script is most useful for testing builds.

USER_AT_REMOTE=""  # e.g., user@machine
BUILD_DIR="/tmp/dcma_remote_build/" # Scratch directory on the remote, created if necessary.
PORTABLE_BIN_DIR="/tmp/dcma_portable_build/" # Portable binaries are dumped here on the remote.
L_PORTABLE_BIN_DIR="${HOME}/portable_dcma/" # Portable binaries are dumped here locally.

# System to build on. See below for options.
BUILDER="debian_buster"  # 'arch', 'debian_buster', 'mxe', or 'native'.

# Argument parsing:
OPTIND=1 # Reset in case getopts has been used previously in the shell.
while getopts "d:p:l:b:h" opt; do
    case "$opt" in
    d)  BUILD_DIR="$OPTARG"
        printf 'Proceeding with remote build root directory "%s".\n' "${BUILD_DIR}"
        ;;
    p)  PORTABLE_BIN_DIR="$OPTARG"
        printf 'Proceeding with remote portable binary root directory "%s".\n' "${PORTABLE_BIN_DIR}"
        ;;
    l)  L_PORTABLE_BIN_DIR="$OPTARG"
        printf 'Proceeding with local portable binary root directory "%s".\n' "${L_PORTABLE_BIN_DIR}"
        ;;
    b)  BUILDER="$OPTARG"
        printf 'Proceeding with builder "%s".\n' "${BUILDER}"
        ;;
    h|*)
        printf 'This script will clone the current repository and then remotely build using ssh.\n'
        printf 'Docker build images or chroots can be used on remotes.\n'
        printf 'The build artifacts are retained and portable binaries are returned to the local machine.\n'
        printf 'They may or may not be installed remotely.\n'
        printf 'This script is most useful for testing builds.\n'
        printf "\n"
        printf "Usage: \n\t $0 <args> user@remote\n"
        printf "\n"
        printf "Available arguments: \n"
        printf "\n"
        printf " -h       : Display usage information and terminate.\n"
        printf "\n"
        printf " -d <dir> : A directory on the remote machine in which to perform the build.\n"
        printf "          : It is created if necesary.\n"
        printf "          : Default: '%s'\n" "${BUILD_DIR}"
        printf "\n"
        printf " -p <arg> : A directory on the remote machine in which portable binaries will be dumped.\n"
        printf "          : It is created if necesary.\n"
        printf "          : Default: '%s'\n" "${PORTABLE_BIN_DIR}"
        printf "\n"
        printf " -l <arg> : A directory on the local machine in which portable binaries will be dumped.\n"
        printf "          : It is created if necesary.\n"
        printf "          : Default: '%s'\n" "${L_PORTABLE_BIN_DIR}"
        printf "\n"
        printf " -b <arg> : Specify which Docker build image or chroot to use, if any.\n"
        printf "          : This option controls how the build is performed, packaged, and installed.\n"
        printf "          : Options are 'arch', 'debian_buster', 'mxe', and 'native'.\n"
        printf "          : Default: '%s'\n" "${BUILDER}"
        printf "\n"
        exit 0
        ;;
    esac
done


# Determine the user@machine to use from the remaining arguments.
shift $((OPTIND-1))
[ "$1" = "--" ] && shift
USER_AT_REMOTE="$*"  # e.g., user@machine
if [ -z "${USER_AT_REMOTE}" ] ; then
    printf 'No user or remote machine specified. Cannot continue.\n' 1>&2
    exit 1
fi
printf 'Proceeding with remote user and remote machine "%s".\n' "${USER_AT_REMOTE}"


# Find the repository root, so we can sync the entire repo contents.
REPOROOT="$(git rev-parse --show-toplevel || true)"
if [ ! -d "${REPOROOT}" ] ; then

    # Fall-back on the source position of this script.
    SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
    if [ ! -d "${SCRIPT_DIR}" ] ; then
        printf "Cannot access repository root or root directory containing this script. Cannot continue.\n" 1>&2
        exit 1
    fi
    REPOROOT="${SCRIPT_DIR}"
fi
cd "${REPOROOT}"
rsync -rptPzzv --no-links --cvs-exclude --exclude="$(realpath --relative-to="${REPOROOT}/" "${BUILD_DIR}")" "${REPOROOT}"/ "${USER_AT_REMOTE}:${BUILD_DIR}"


# Initiate the remote build.
if [[ "${BUILDER}" =~ .*native.* ]] ; then
    ssh "${USER_AT_REMOTE}" " 
      cd '${BUILD_DIR}' && 
      mkdir -p build && 
      cd build &&
      cmake \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_SYSCONFDIR=/etc \
        -DCMAKE_BUILD_TYPE=Release \
        -DMEMORY_CONSTRAINED_BUILD=OFF \
        -DWITH_ASAN=OFF \
        -DWITH_TSAN=OFF \
        -DWITH_MSAN=OFF \
        -DWITH_EIGEN=ON \
        -DWITH_CGAL=ON \
        -DWITH_NLOPT=ON \
        -DWITH_SFML=ON \
        -DWITH_WT=ON \
        -DWITH_GNU_GSL=ON \
        -DWITH_POSTGRES=ON \
        -DWITH_JANSSON=ON \
        ../ && "'
      JOBS=$(nproc) &&
      JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) &&
      make -j "$JOBS" VERBOSE=1 &&
      cd '${BUILD_DIR}' &&
      ./scripts/dump_portable_dcma_bundle.sh '${PORTABLE_BIN_DIR}'
      '

elif [[ "${BUILDER}" =~ .*debian.*_buster.* ]] && [[ "$(uname -m)" =~ .*aarch64.* ]] ; then
    printf 'Building debian buster for aarch64 architecture.\n'
    HOST_CHROOT_DIR='/var/tmp/dcma_aarch64_chroot'

    ssh -t "${USER_AT_REMOTE}" -- "
        set -eux
        cd '${BUILD_DIR}'

        cat > docker_run.sh << 'EOF'
#!/usr/bin/env bash
        set -eux
        #sudo mount -o remount,exec,dev '${BUILD_DIR}'
        /chroot/dcma/docker/build_bases/debian_buster/create_foreign_chroot.sh \
            -d /chroot/ \
            -s /chroot/run_in_chroot.sh \
            -a arm64

        mkdir -p '${PORTABLE_BIN_DIR}'
        cp -R /chroot/dcma/docker/build_bases/debian_buster /chroot/scratch_base
        /chroot/run_in_chroot.sh 'cd /dcma && ./docker/build_bases/debian_buster/implementation_debian_buster.sh'
        /chroot/run_in_chroot.sh 'cd /dcma && ./compile_and_install.sh -b build' # Will auto-detect the Debian build process.

        /chroot/run_in_chroot.sh 'cd /dcma && ./scripts/extract_system_appimage.sh'
        /chroot/run_in_chroot.sh 'mkdir -p /pbin/'
        /chroot/run_in_chroot.sh 'cd /dcma && mv ./DICOMautomaton*AppImage /pbin/'
        /chroot/run_in_chroot.sh 'cd /dcma && ./scripts/dump_portable_dcma_bundle.sh /pbin/'
        rsync -rv /chroot/pbin/ /pbin/
EOF
        chmod 777 docker_run.sh

        mkdir -p '${PORTABLE_BIN_DIR}'
        mkdir -p '${HOST_CHROOT_DIR}'
        
        sudo docker run \
          -it --rm \
          --network=host \
          -v '${HOST_CHROOT_DIR}':/chroot/:rw \
          -v '${BUILD_DIR}':/chroot/dcma/:rw \
          -v '${PORTABLE_BIN_DIR}':/pbin/:rw \
          -w / \
          dcma_build_base_debian_buster:latest \
          /chroot/dcma/docker_run.sh
      "

elif [[ "${BUILDER}" =~ .*debian.*_buster.* ]] ; then
    printf 'Building debian buster for the remote machine architecture.\n'

    ssh -t "${USER_AT_REMOTE}" -- "
        set -eu
        cd '${BUILD_DIR}'
        
        cat > docker_run.sh << 'EOF'
#!/usr/bin/env bash
set -eu
./compile_and_install.sh  # Will auto-detect the Debian build process.
./scripts/extract_system_appimage.sh
cp DICOMautomaton*AppImage /pbin/
./scripts/dump_portable_dcma_bundle.sh /pbin/
EOF
        chmod 777 docker_run.sh

        mkdir -p '${PORTABLE_BIN_DIR}'
        mkdir -p build
        
        sudo docker run \
          -it --rm \
          --network=host \
          -v '${BUILD_DIR}':/start/:rw \
          -v '${PORTABLE_BIN_DIR}':/pbin/:rw \
          -w /start/ \
          dcma_build_base_debian_buster:latest \
          /start/docker_run.sh
      "

elif [[ "${BUILDER}" =~ .*arch.* ]] ; then

    ssh -t "${USER_AT_REMOTE}" -- "
        set -eu
        cd '${BUILD_DIR}'
        
        cat > docker_run.sh << 'EOF'
#!/usr/bin/env bash
set -eu
./compile_and_install.sh  # Will auto-detect the Arch Linux build process.
./scripts/extract_system_appimage.sh
cp DICOMautomaton*AppImage /pbin/
./scripts/dump_portable_dcma_bundle.sh /pbin/
EOF
        chmod 777 docker_run.sh

        mkdir -p '${PORTABLE_BIN_DIR}'
        mkdir -p build
        
        sudo docker run \
          -it --rm \
          --network=host \
          -v '${BUILD_DIR}':/start/:rw \
          -v '${PORTABLE_BIN_DIR}':/pbin/:rw \
          -w /start/ \
          dcma_build_base_arch:latest \
          /start/docker_run.sh
      "

elif [[ "${BUILDER}" =~ .*mxe.* ]] ; then

    ssh -t "${USER_AT_REMOTE}" -- "
        set -eu
        cd '${BUILD_DIR}'

        cat > docker_run.sh << 'EOF'
#!/usr/bin/env bash
set -eu
./compile_and_install.sh  # Will auto-detect the MXE build process.
rsync -avPzz /out/ /pbin/
EOF
        chmod 777 docker_run.sh

        mkdir -p '${PORTABLE_BIN_DIR}'
        mkdir -p build

        sudo docker run \
          -it --rm \
          --network=host \
          -v '${BUILD_DIR}':/start/:rw \
          -v '${PORTABLE_BIN_DIR}':/pbin/:rw \
          -w /start/ \
          dcma_build_base_mxe:latest \
          /start/docker_run.sh
      "

else
    printf "Builder '%s' not understood. Cannot continue.\n" "${BUILDER}" 1>&2
    exit 1

fi


# Sync portable binary artifacts back.
rsync -rptvPzz --no-links --delete "${USER_AT_REMOTE}:${PORTABLE_BIN_DIR}/" "${L_PORTABLE_BIN_DIR}/"


printf ' ====================================================================== \n'
printf ' ===                   Compilation was successful.                  === \n'
printf ' === Note that nothing was installed locally, but portable binaries === \n'
printf ' ===       have been dumped remotely and locally for testing.       === \n'
printf ' ====================================================================== \n'


