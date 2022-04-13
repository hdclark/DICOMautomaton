#!/usr/bin/env bash

set -eu

VERBOSE_FAILURE_OUTPUT="0" # Whether to print all test results on failure.

###########################################################################################
# Argument parsing
###########################################################################################
OPTIND=1 # Reset in case getopts has been used previously in the shell.
while getopts "huv" opt; do
    case "$opt" in
    u)  true
        ;&
    h)
        printf -- '\n'
        printf -- '===============================================================================\n'
        printf -- 'This script performs integration tests for DICOMautomaton.\n'
        printf -- '\n'
        printf -- 'Usage: \n'
        printf -- '\t '"$0"' -h -u -v\n'
        printf -- '\n'
        printf -- 'Options: \n'
        printf -- '\t -h  Print this help/usage information and quit.\n'
        printf -- '\t -u  Print this help/usage information and quit.\n'
        printf -- '\t -v  Verbosely print reports if any tests fail.\n'
        printf -- '\n'
        printf -- 'Note: \n'
        printf -- ' - If the "-v" option is specified, all test results are verbosely reported if\n'
        printf -- '   any tests fail. This is useful in continuous integration, or for inspecting\n'
        printf -- '   logs, but is not recommended for interactive use.\n'
        printf -- '===============================================================================\n'
        exit 1
        ;;
    v)  VERBOSE_FAILURE_OUTPUT="1"
        ;;
    esac
done
shift $(( OPTIND - 1 ))  # Purge all consumed options to the left of the first non-option token.

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

# Common setup.
export DCMA_BIN="dicomautomaton_dispatcher"
#export TESTING_ROOT="/tmp/dcma_integration_testing"
export TESTING_ROOT="${REPOROOT}/dcma_integration_testing"
mkdir -v -p "${TESTING_ROOT}"
export DCMA_REPO_ROOT="${REPOROOT}"
export TEST_FILES_ROOT="${REPOROOT}/artifacts/test_files/"
export TEST_FAILURES="$(mktemp "${TESTING_ROOT}"/failures_XXXXXXXXXX)" 
export TEST_SUCCESSES="$(mktemp "${TESTING_ROOT}"/successes_XXXXXXXXXX)" 
export KEEP_ALL_OUTPUTS="0" # Successes are only purged when != "1".

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
    bash "${s_f_base}" 1>stdout 2>stderr
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

find "${REPOROOT}"/tests/ -type f -exec bash -c 'perform_test {}' \;
#find "${REPOROOT}"/tests/ -type f -print0 |
#  xargs -0 -I '{}' -P $(nproc || echo 2) -n 1 -r  \
#         bash -c '{}'

# Report a summary.
#cat "${TEST_SUCCESSES}" 
N_successes="$(wc -l < "${TEST_SUCCESSES}")"
N_failures="$(wc -l < "${TEST_FAILURES}")"

if [ -s "${TEST_FAILURES}" ] ; then
    printf '\n'
    printf 'The following tests failed:\n'
    cat "${TEST_FAILURES}"

    if [ "${VERBOSE_FAILURE_OUTPUT}" == "1" ] ; then
        find "${TESTING_ROOT}" -type f -printf '=== %p ===\n' -exec cat '{}' \;
    fi
else
    printf 'All tests passed.\n'
fi

printf '\n'
printf -- '---------------------------------------------------------------------------------------\n'
printf 'Number of tests performed: \t%s\n' "$((N_successes + N_failures))" 
printf '       ... that succeeded: \t%s\n' "$N_successes"
printf '          ... that failed: \t%s\n' "$N_failures" 
printf 'All outputs are in: %s\n' "${TESTING_ROOT}"
printf -- '---------------------------------------------------------------------------------------\n'

if [ "${KEEP_ALL_OUTPUTS}" != "1" ] ; then
    rm "${TEST_FAILURES}" 
    rm "${TEST_SUCCESSES}" 
    rmdir "${TESTING_ROOT}" &>/dev/null || true
fi

if [ "${N_failures}" == "0" ] ; then
    exit 0
else
    exit 1
fi

