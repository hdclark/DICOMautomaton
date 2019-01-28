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

# Create a wrapper script for the portable binary.
cat > "${out_dir}/portable_dcma" <<'HEREDOC_EOF'
#!/bin/bash

set -e

# Identify the location of this script.
export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"

# Assume that this script, the libraries, and the binary are all bundled together.
LD_LIBRARY_PATH="${SCRIPT_DIR}" exec "${SCRIPT_DIR}/"dicomautomaton_dispatcher "$@"

HEREDOC_EOF

chmod 777 "${out_dir}/portable_dcma"

# Attempt to run the portable binary.
"${out_dir}/portable_dcma" -h

printf '\n\nSuccess!\n\n'


