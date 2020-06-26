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
export TESTING_ROOT="/tmp/dcma_integration_testing"
mkdir -v -p "${TESTING_ROOT}"
export DCMA_REPO_ROOT="${REPO_ROOT}"
export TEST_FAILURES="$(mktemp "${TESTING_ROOT}"/failures_XXXXXXXXXX)" 
export TEST_SUCCESSES="$(mktemp "${TESTING_ROOT}"/successes_XXXXXXXXXX)" 
export KEEP_ALL_OUTPUTS="1" # Successes are only purged when != "1".

function perform_test {
    local s_f_name="$@"
    local s_f_base="$(basename "${s_f_name}")"
    if [ ! -f "${s_f_name}" ] ; then
        printf 'Unable to access script..\n' 2>&1
        return 1
    fi

    # Create a working space for the test.
    local tmp_dir="$(mktemp -d "${TESTING_ROOT}"/test_XXXXXXXXXX)" || true
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
    export ASAN_OPTIONS='verbosity=0 coverage=1 coverage_dir="." html_cov_report=1 detect_leaks=1 detect_invalid_pointer_pairs=2'
    export TSAN_OPTIONS='verbosity=0 coverage=1 coverage_dir="." html_cov_report=1 history_size=5'
    export MSAN_OPTIONS='verbosity=0 coverage=1 coverage_dir="." html_cov_report=1'
    export UBSAN_OPTIONS='verbosity=0 coverage=1 coverage_dir="." html_cov_report=1'
    bash "${s_f_base}" >stdout 2>stderr
    local ret_val=$?

    # Notify results of the test.
    popd &>/dev/null
    if [ "${ret_val}" != "0" ] ; then
        # Notify the user about the failure and leave the directory for user inspection.
        printf -- "'%s' --> see '%s'.\n" "${s_f_base}" "${tmp_dir}" >> "${TEST_FAILURES}"
        printf "Test '%s' failed. See '%s'.\n" "${s_f_base}" "${tmp_dir}" 1>&2
        return 1

    else
        printf -- "'%s' --> see '%s'.\n" "${s_f_base}" "${tmp_dir}" >> "${TEST_SUCCESSES}"

        # Clean up.
        if [ "${KEEP_ALL_OUTPUTS}" != "1" ] ; then
            rm -rf "${tmp_dir}"
        fi
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
printf 'All outputs are in: %s\n' "${TESTING_ROOT}"
printf -- '---------------------------------------------------------------------------------------\n'

if [ -s "${TEST_FAILURES}" ] ; then
    printf 'The following tests failed:\n'
    cat "${TEST_FAILURES}"
else
    printf 'All tests passed.\n'
fi

if [ "${KEEP_ALL_OUTPUTS}" != "1" ] ; then
    rm "${TEST_FAILURES}" 
    rm "${TEST_SUCCESSES}" 
fi

if [ "${N_failures}" == "0" ] ; then
    exit 0
else
    exit 1
fi

