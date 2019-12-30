#!/bin/bash

# This script dumps the system-installed dicomautomaton_dispatcher and all shared libraries needed to run it.
# It can be used to provide a minimally (somewhat) portable binary for linux machines.
#
# Note that compilation arguments and architecture-specific tunings can ruin portability.
# Portability, validity of the program, and full functionality are *NOT* guaranteed!
# This script is best run in a virtualized (i.e., controlled) environment.
#

out_dir="$@"
if [ -z "${out_dir}" ] ; then
    out_dir="/tmp/portable_dcma/"
fi

set -e 
mkdir -p "${out_dir}"


# Display which libraries will be copied.
#ldd $(which dicomautomaton_dispatcher) | grep '=>' | sed -e 's@.*=> @@' -e 's@ (.*@@'

# Figure out which binary to use. If there is a build directory then favour the build version.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
if [ -f "${SCRIPT_DIR}/../build/bin/dicomautomaton_dispatcher" ] ; then
    printf 'Using the dcma in build/ ...\n'
    which_dcma=$(which "${SCRIPT_DIR}/../build/bin/dicomautomaton_dispatcher")
else
    printf 'Using the system dcma installation ...\n'
    which_dcma=$(which dicomautomaton_dispatcher)
fi
if [ ! -f "${which_dcma}" ] ; then
    printf 'Unable to locate a suitable version of dicomautomaton_dispatcher to use. Cannot continue.\n' 
    exit 1
fi

# Gather the necessary libraries.
#
# Note: if glibc versions match on host and client (or the host is older than the client),
#       then the client's system glibc distribution can be used. Add the following rsync excludes:
#           --exclude='ld-linux*' \
#           --exclude='libc.*' \
#           --exclude='libm.*' \
#       Otherwise, bundle the host's glibc distribution and hope that patching the binary will suffice!
rsync -L -r --delete \
  $( ldd "${which_dcma}" | 
     grep '=>' | 
     sed -e 's@.*=> @@' -e 's@ (.*@@' 
  ) \
  "${out_dir}/"

# Also grab the dcma binary.
rsync -L -r --delete "${which_dcma}" "${out_dir}/"

# Also provide a lexicon since Explicator may not be installed.
# The most recent lexicon will be selected.
lex_file="$( find /usr/share/explicator/lexicons/ -type f -iname '*SGF*' | LC_ALL=C  sort --numeric-sort | tail -n 1 )"
if [ -f "${lex_file}" ] ; then
    rsync -L -r "${lex_file}" "${out_dir}/"portable_default.lex
else
    printf 'Unable to find system lexicon. Creating a placeholder.\n'
    echo "null : null" > "${out_dir}/"portable_default.lex
fi

# Create a native wrapper script for the portable binary.
cat > "${out_dir}/portable_dcma" <<'PORTABLE_EOF'
#!/bin/bash

set -eu

# Identify the location of this script.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"

# Assume that this script, the libraries, and the binary are all bundled together.
LD_LIBRARY_PATH="${SCRIPT_DIR}" exec "${SCRIPT_DIR}/"dicomautomaton_dispatcher "$@"

PORTABLE_EOF
chmod 777 "${out_dir}/portable_dcma"


# Create a native wrapper script for the self-adjusting binary.
cat > "${out_dir}/adjusting_dcma" <<'ADJUSTING_EOF'
#!/bin/bash

set -eu

# Identify the location of this script.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"

# Patch the binary to use the bundled ld-linux.so
function command_exists () {
    type "$1" &> /dev/null ;
}
if command_exists patchelf ; then
    printf 'Relying on bundled glibc distribution. Execution may fail in bizarre ways...\n'

    current_rpath=$( patchelf --print-rpath "${SCRIPT_DIR}/"dicomautomaton_dispatcher )
    if [ "$(readlink -m "${current_rpath}")" != "$(readlink -m "${SCRIPT_DIR}")" ] ; then
        # Note: this step should only trigger when this script is first run after moving 
        #       the binary to a new location.
        printf 'Patching ELF binary...\n'
        patchelf \
          --set-interpreter "${SCRIPT_DIR}/"ld-linux*so* \
          --set-rpath "${SCRIPT_DIR}/" \
          "${SCRIPT_DIR}/"dicomautomaton_dispatcher
    fi

else
    printf 'Relying on hardcoded system glibc distribution. Symbol resolution may fail...\n'
fi

# Assume that this script, the libraries, and the binary are all bundled together.
LD_LIBRARY_PATH="${SCRIPT_DIR}" exec "${SCRIPT_DIR}/"dicomautomaton_dispatcher -l portable_default.lex "$@"

ADJUSTING_EOF
chmod 777 "${out_dir}/adjusting_dcma"


# Create an emulation wrapper script for the portable binary.
cat > "${out_dir}/emulate_dcma" <<'EMULATE_EOF'
#!/bin/bash

set -e

# This script requires `qemu` and `qemu-user` packages to emulate an x86_64 runtime.

# Identify the location of this script.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"

# Patch the binary to use the bundled ld-linux.so
#
# Note: it would be better to provide a custom runtime rather than patching the binary. TODO.
#      
function command_exists () {
    type "$1" &> /dev/null ;
}
if command_exists patchelf ; then
    printf 'Relying on bundled glibc distribution. Execution may fail in bizarre ways...\n'

    current_rpath=$( patchelf --print-rpath "${SCRIPT_DIR}/"dicomautomaton_dispatcher )
    if [ "$(readlink -m "${current_rpath}")" != "$(readlink -m "${SCRIPT_DIR}")" ] ; then
        # Note: this step should only trigger when this script is first run after moving 
        #       the binary to a new location.
        printf 'Patching ELF binary...\n'
        patchelf \
          --set-interpreter "${SCRIPT_DIR}/"ld-linux*so* \
          --set-rpath "${SCRIPT_DIR}/" \
          "${SCRIPT_DIR}/"dicomautomaton_dispatcher
    fi

else
    printf 'Relying on hardcoded system glibc distribution. Symbol resolution may fail...\n'
fi

# Assume that this script, the libraries, and the binary are all bundled together.
qemu-x86_64 -E LD_LIBRARY_PATH="${SCRIPT_DIR}" -L "${SCRIPT_DIR}" dicomautomaton_dispatcher -l portable_default.lex "$@"

EMULATE_EOF
chmod 777 "${out_dir}/emulate_dcma"


# Attempt to run the native binary.
set +e
"${out_dir}/dicomautomaton_dispatcher" -h >/dev/null
ret=$?
if [ "$ret" -eq 0 ] ; then
    printf '\nSuccess.\n'
    exit 0
else
    printf '\nFailed.\n'
    exit 1
fi


