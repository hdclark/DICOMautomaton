#!/bin/bash

set -eu

# Move to a standard location.
export REPO_ROOT=$(git rev-parse --show-toplevel || true)
if [ ! -d "${REPO_ROOT}" ] ; then
    printf 'Unable to find git repo root. Refusing to continue.\n' 1>&2
    exit 1
fi

export SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}" )" )"
if [ ! -d "${SCRIPT_DIR}" ] ; then
    printf 'Unable to find script directory. Refusing to continue.\n' 1>&2
    exit 1
fi

# The file that will be edited.
declare -a edit_files
#edit_files+=( "src/Structs.h" )
#edit_files+=( "src/Structs.cc" )
edit_files+=( src/Operations/*h )

# The files that will be compiled.
comp_file="/dev/null"

set +e

function compile_func {
    cd "${REPO_ROOT}"
    #g++ \
    #  -DBOOST_ALL_NO_LIB \
    #  -DBOOST_SYSTEM_DYN_LINK \
    #  -DBOOST_THREAD_DYN_LINK \
    #  -DCGAL_EIGEN3_ENABLED=1 \
    #  -DCGAL_USE_CORE=1 \
    #  -DCGAL_USE_GMP \
    #  -DCGAL_USE_MPFR \
    #  -DDCMA_USE_CGAL=1 \
    #  -DDCMA_USE_EIGEN=1 \
    #  -DDCMA_USE_GNU_GSL=1 \
    #  -DDCMA_USE_JANSSON=1 \
    #  -DDCMA_USE_NLOPT=1 \
    #  -DDCMA_USE_POSTGRES=1 \
    #  -DDCMA_USE_SFML=1 \
    #  -DDCMA_USE_WT=1 \
    #  -DUSTREAM_H \
    #  -I./src/ \
    #  -I/usr/include/eigen3 \
    #  -pipe \
    #  -fno-plt \
    #  -fno-var-tracking-assignments \
    #  -Wall \
    #  -Wextra \
    #  -Wpedantic \
    #  -Wfatal-errors \
    #  -O0 \
    #  -std=c++17 \
    #  -o /dev/null \
    #  -c $@ &>/dev/null  && \
    CC=gcc CXX=g++ ./compile_and_install.sh -c </dev/null &>/dev/null && \
    dicomautomaton_dispatcher -h </dev/null &>/dev/null

    #CC=clang CXX=clang++ ./compile_and_install.sh -c </dev/null &>/dev/null && \
}
export -f compile_func


for edit_file in ${edit_files[@]}; do
    printf "=== '%s' ===\n" "${edit_file}"

    # Identify all header include statements in the file.
    mapfile -t includes < <( 
        cat "${edit_file}" |
          grep -- '^#include ' |
          grep -i 'cgal\|boost\|eigen\|nlopt\|_functors' |
          sort -u )

    # Remove each to see if the compilation succeeds.
    for I in "${includes[@]}" ; do
        # Escape the include.
        e="${I//\"/\\\"}"

        # Remove the include by merely commenting it out.
        gawk -i inplace '{ if( $0 == "'"${e}"'" ){ print "//" $0 }else{ print $0 } }' "${edit_file}"

        # Report progress.
        printf "Testing '%s' ... " "${I}"
        #git diff "${edit_file}"

        # Attempt the compilation.
        compile_func ${comp_file}
        ret=$?

        # If compilation succeeds, the inclusion is assumed to be extraneous. Leave it out.
        printf '\033[10000D''\033[110C'' ' # Move to a specific column for easier viewing.
        if [ "${ret}" == "0" ] ; then
            printf "superfluous.\n"
            # Note: here we could remove the line altogether, but for testing purposes we'll leave it commented out.

        # Otherwise, the inclusion was necessary, Add it back.
        else
            printf "necessary.\n"
            gawk -i inplace '{ if( $0 == ("//" "'"${e}"'") ){ print "'"${e}"'" }else{ print $0 } }' "${edit_file}"
        fi
    done
done

