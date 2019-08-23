#!/bin/bash
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
BUILDER="debian_stable"  # 'debian_stable', 'arch', or 'native'.

# Argument parsing:
OPTIND=1 # Reset in case getopts has been used previously in the shell.
while getopts "d:p:l:b:h" opt; do
    case "$opt" in
    h)
        printf 'This script will clone the current repository and then remotely build using ssh.\n'
        printf 'Docker build images can be used on remotes.\n'
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
        printf " -b <arg> : Specify which Docker build image to use, if any.\n"
        printf "          : This option controls how the build is performed, packaged, and installed.\n"
        printf "          : Options are 'debian_stable', 'arch', and 'native'.\n"
        printf "          : Default: '%s'\n" "${BUILDER}"
        printf "\n"
        exit 0
        ;;
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
        printf 'Proceeding with Docker build image "%s".\n' "${BUILDER}"
        ;;
    esac
done


# Determine the user@machine to use from the remaining arguments.
shift $((OPTIND-1))
[ "$1" = "--" ] && shift
USER_AT_REMOTE="$@"  # e.g., user@machine
if [ -z "${USER_AT_REMOTE}" ] ; then
    printf 'No user or remote machine specified. Cannot continue.\n' 1>&2
    exit 1
fi
printf 'Proceeding with remote user and remote machine "%s".\n' "${USER_AT_REMOTE}"


# Find the repository root, so we can sync the entire repo contents.
reporoot=$(git rev-parse --show-toplevel)
if [ ! -d "${reporoot}" ] ; then
    printf "Cannot access repository root '%s'. Cannot continue.\n" "${reporoot}" 1>&2
    exit 1
fi
cd "${reporoot}"
rsync -rptPz --no-links --cvs-exclude "${reporoot}"/ "${USER_AT_REMOTE}:${BUILD_DIR}"


# Initiate the remote build.
if [[ "${BUILDER}" =~ .*native.* ]] ; then
    ssh "${USER_AT_REMOTE}" " 
      cd '${BUILD_DIR}' && 
      mkdir -p build && 
      cd build &&
      cmake \
        -DMEMORY_CONSTRAINED_BUILD=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release \
        ../ && "'
      JOBS=$(nproc)
      JOBS=$(( $JOBS < 8 ? $JOBS : 8 )) # Limit to reduce memory use.
      make -j "$JOBS" VERBOSE=1 &&
      cd '${BUILD_DIR}' &&
      ./scripts/dump_portable_dcma_bundle.sh '${PORTABLE_BIN_DIR}'
      '

elif [[ "${BUILDER}" =~ .*debian.*stable.* ]] ; then

    ssh -t "${USER_AT_REMOTE}" -- "
        set -eu
        cd '${BUILD_DIR}'
        
        cat > docker_run.sh << 'EOF'
#!/bin/bash
set -eu
./compile_and_install.sh  # Will auto-detect the Debian build process.
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
          dicomautomaton_webserver_debian_stable:latest \
          /start/docker_run.sh
      "

elif [[ "${BUILDER}" =~ .*arch.* ]] ; then

    ssh -t "${USER_AT_REMOTE}" -- "
        set -eu
        cd '${BUILD_DIR}'
        
        cat > docker_run.sh << 'EOF'
#!/bin/bash
set -eu
./compile_and_install.sh  # Will auto-detect the Arch Linux build process.
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
          dicomautomaton_webserver_arch:latest \
          /start/docker_run.sh
      "

else
    printf "Builder '%s' not understood. Cannot continue.\n" "${BUILDER}" 1>&2
    exit 1

fi


# Sync portable binary artifacts back.
rsync -rptvPz --no-links --delete "${USER_AT_REMOTE}:${PORTABLE_BIN_DIR}/" "${L_PORTABLE_BIN_DIR}/"


printf ' ====================================================================== \n'
printf ' ===                   Compilation was successful.                  === \n'
printf ' === Note that nothing was installed locally, but portable binaries === \n'
printf ' ===       have been dumped remotely and locally for testing.       === \n'
printf ' ====================================================================== \n'


