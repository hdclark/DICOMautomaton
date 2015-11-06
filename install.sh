#!/bin/bash

# This script will ensure everything is compiled (as per the makefile), strip the binaries, 
# install them (as per the Makefile) and make a backup copy to a given directory.
#
# Due to installation requirements, we need root privileges.

BACKUPDIR="/root/Backups_-_DICOMautomaton_bins/"

# Check if we are root.
if [[ $EUID -ne 0 ]]; then
    echo "This script requires root privileges" 1>&2
    exit 1
fi

if make -j 8 && make install ; then
    # Make a timestamped copy of the binaries to a storage directory.
    mkdir -p "${BACKUPDIR}"
    TIMESTAMP=`date +%Y-%m-%d-%H:%M:%S`

    # Copy the binaries.
    for i in automaton overlaydosedata libimebrashim.so petct_perfusion_analysis pacs_ingress pacs_refresh ; do
        cp "${i}" "${BACKUPDIR}${i}_${TIMESTAMP}"
    done

    # Dump some info about the build environment for future reference.
    GCCVER="$(gcc --version | head -1)"
    echo "${TIMESTAMP}: ${GCCVER}" >> "${BACKUPDIR}VERSIONS" 


else
    echo "****************** Build failure encountered **********************"
fi


# Always ensure the files are cleaned
make clean > /dev/null 2>&1

