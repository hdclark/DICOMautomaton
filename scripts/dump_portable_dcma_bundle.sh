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

# Gather the necessary libraries.
rsync -L -r --delete \
  --exclude='ld-linux*' \
  --exclude='libc.*' \
  --exclude='libm.*' \
  $( ldd $(which dicomautomaton_dispatcher) | 
     grep '=>' | 
     sed -e 's@.*=> @@' -e 's@ (.*@@' 
  ) \
  "${out_dir}/"

# Also grab the dcma binary.
rsync -L -r --delete $( which dicomautomaton_dispatcher ) "${out_dir}/"


# Create a native wrapper script for the portable binary.
cat > "${out_dir}/portable_dcma" <<'PORTABLE_EOF'
#!/bin/bash

set -e

# Identify the location of this script.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"

# Assume that this script, the libraries, and the binary are all bundled together.
LD_LIBRARY_PATH="${SCRIPT_DIR}" exec "${SCRIPT_DIR}/"dicomautomaton_dispatcher "$@"

PORTABLE_EOF
chmod 777 "${out_dir}/portable_dcma"


# Create an emulation wrapper script for the portable binary.
cat > "${out_dir}/emulate_dcma" <<'EMULATE_EOF'
#!/bin/bash

set -e

# This script requires `qemu` and `qemu-user` packages to emulate an x86_64 runtime.

# Identify the location of this script.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"

# Assume that this script, the libraries, and the binary are all bundled together.
qemu-x86_64 -E LD_LIBRARY_PATH="${SCRIPT_DIR}" -L "${SCRIPT_DIR}" dicomautomaton_dispatcher "$@"

EMULATE_EOF
chmod 777 "${out_dir}/emulate_dcma"


# Attempt to run the native binary.
"${out_dir}/portable_dcma" -h


printf '\n\nSuccess!\n\n'


