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

# Common setup.
export DCMA_BIN="dicomautomaton_dispatcher"
export DCMA_REPO_ROOT="${REPO_ROOT}"
export TEST_FAILURES="$(mktemp /tmp/dcma_integration_test_XXXXXXXXXX)" 
export TEST_SUCCESSES="$(mktemp /tmp/dcma_integration_test_XXXXXXXXXX)" 

function perform_test {
    local s_f_name="$@"
    local s_f_base="$(basename "${s_f_name}")"
    if [ ! -f "${s_f_name}" ] ; then
        printf 'Unable to access script..\n' 2>&1
        return 1
    fi

    # Create a working space for the test.
    local tmp_dir="$(mktemp -d /tmp/dcma_integration_test_XXXXXXXXXX)" || true
    local found_dir="$(find "${tmp_dir}" -maxdepth 0 -type d -empty)"
    if [ ! -d "${tmp_dir}" ] || [ "${tmp_dir}" != "${found_dir}" ] ; then
        printf 'Unable to create temporary directory.\n' 2>&1
        return 1
    fi

    # Copy the script to the temporary directory and prepare to run the script from the dir.
    cp "${s_f_name}" "${tmp_dir}"
    pushd "${tmp_dir}" &>/dev/null
    if [ ! -f "${s_f_base}" ] ; then
        printf 'Unable to access script within temporary directory.\n' 2>&1
        return 1
    fi

    # Execute the script.
    bash "${s_f_base}" >stdout 2>stderr
    local ret_val=$?

    # Notify results of the test.
    popd &>/dev/null
    if [ "${ret_val}" != "0" ] ; then
        # Notify the user about the failure and leave the directory for user inspection.
        printf -- "  '%s' --> see '%s'.\n" "${s_f_base}" "${tmp_dir}" >> "${TEST_FAILURES}"
        printf "Test '%s' failed. See '%s'.\n" "${s_f_base}" "${tmp_dir}" 1>&2
        return 1

    else
        # Clean up.
        rm -rf "${tmp_dir}"
        printf "  Test '%s' succeeded.\n" "${s_f_base}" >> "${TEST_SUCCESSES}"
        return 0

    fi
}
export -f perform_test


# Run all scripts, reporting only failures.
set +e

find "${SCRIPT_DIR}"/tests/ -type f -exec bash -c 'perform_test {}' \;
#find "${SCRIPT_DIR}"/tests/ -type f -print0 |
#  xargs -0 -I '{}' -P $(nproc || echo 2) -n 1 -r  \
#         bash -c '{}'

# Report a summary.
#cat "${TEST_SUCCESSES}" 
N_successes="$(wc -l < "${TEST_SUCCESSES}")"
N_failures="$(wc -l < "${TEST_FAILURES}")"

printf '\n'
printf -- '---------------------------------------------------------------------------------------\n'
printf 'Number of tests performed: \t%s\n' "$(($N_successes + $N_failures))" 
printf '       ... that succeeded: \t%s\n' "$N_successes"
printf '          ... that failed: \t%s\n' "$N_failures" 
printf -- '---------------------------------------------------------------------------------------\n'

if [ -s "${TEST_FAILURES}" ] ; then
    printf 'The following tests failed:\n'
    cat "${TEST_FAILURES}"
else
    printf 'All tests passed.\n'
fi

rm "${TEST_FAILURES}" 
rm "${TEST_SUCCESSES}" 

if [ "${N_failures}" == "0" ] ; then
    exit 0
else
    exit 1
fi

