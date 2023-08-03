#!/usr/bin/env bash

# This script can be used to verify the syntax of all files without actually compiling anything.
set -eux

#locale="en_US.UTF-8"
locale="C"
export LANG="$locale"
export LANGUAGE=""
export LC_CTYPE="$locale"
export LC_NUMERIC="$locale"
export LC_TIME="$locale"
export LC_COLLATE="$locale"
export LC_MONETARY="$locale"
export LC_MESSAGES="$locale"
export LC_PAPER="$locale"
export LC_NAME="$locale"
export LC_ADDRESS="$locale"
export LC_TELEPHONE="$locale"
export LC_MEASUREMENT="$locale"
export LC_IDENTIFICATION="$locale"
export LC_ALL=""


check_cpp_syntax () {
    # Provide an empty file for any build-system-generated config files.
    temp_dir="$(mktemp -d)"
    mkdir -pv "${temp_dir}/src/"
    touch "${temp_dir}/DCMA_Definitions.h"
    touch "${temp_dir}/src/DCMA_Definitions.h"

    local f="$*"
    [ -f "$f" ] && {
        set -eux
        "${CXX:-g++}" --std=c++17 -fsyntax-only \
          -I'src/imebra20121219/library/imebra/include/' \
          -I"${temp_dir}/" \
          -I"${temp_dir}/src/" \
          `# Check if dependencies are available. If so, enable them via preprocessor directives.` \
          ` pkg-config eigen3  &>/dev/null && printf -- "-DDCMA_USE_EIGEN=1   $(pkg-config --cflags eigen3)" ` \
          ` pkg-config nlopt   &>/dev/null && printf -- "-DDCMA_USE_NLOPT=1   $(pkg-config --cflags nlopt )" ` \
          ` pkg-config gsl     &>/dev/null && printf -- "-DDCMA_USE_GNU_GSL=1 $(pkg-config --cflags gsl   )" ` \
          ` pkg-config thrift  &>/dev/null && printf -- "-DDCMA_USE_THRIFT=1  $(pkg-config --cflags thrift)" ` \
          ` pkg-config glew sdl2 &>/dev/null && printf -- "-DDCMA_USE_SDL=1   $(pkg-config --cflags glew sdl2  )" ` \
          ` pkg-config libpq libpqxx &>/dev/null && printf -- "-DDCMA_USE_POSTGRES=1 $(pkg-config --cflags libpq libpqxx )" ` \
          ` pkg-config glew sfml-window sfml-graphics sfml-system &>/dev/null && printf -- "-DDCMA_USE_SFML=1 $(pkg-config --cflags glew sfml-window sfml-graphics sfml-system)" ` \
          `# Check for 'helper' programs that indicate the presence of a package.` \
          ` command -v cgal_create_CMakeLists &>/dev/null && printf -- "-DDCMA_USE_CGAL=1" ` \
          `# Some dependencies don't use pkg-config, so include them and hope for the best.` \
          -DDCMA_USE_BOOST=1 \
          -DDCMA_USE_WT=1 \
          -DDCMA_USE_JANSSON=1 \
          "$f"
    }
    rm "${temp_dir}/"*h "${temp_dir}"/src/*h
    rmdir "${temp_dir}/src"
    rmdir "${temp_dir}"
}
export -f check_cpp_syntax

check_sh_syntax () {
    local f="$*"
    if [ -f "$f" ] && type "shellcheck" &> /dev/null ; then
        set -eux
        shellcheck \
          -S error \
          -e SC1117,SC2059 \
          "$f"
    else
        printf "'shellcheck' not available, so skipping shell script check for '%s'\n" "$f"
    fi
}
export -f check_sh_syntax

check_yml_syntax () {
    local f="$*"
    if [ -f "$f" ] && type "yamllint" &> /dev/null ; then
        set -eux
        yamllint \
          -d "{extends: relaxed, rules: {line-length: {max: 200}}}" \
          "$f"
    else
        printf "'yamllint' not available, so skipping YAML file check for '%s'\n" "$f"
    fi
}
export -f check_yml_syntax


# Move to the repository root.
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

# Check all files in the project.
#find ./src/ -type f -print0 |
# 
# Or, check only modified and untracked files.
git ls-files -z -o -m "$@" |
  grep -z -E '.*[.]h|.*[.]cc|.*[.]cpp' |
    `# Ignore imebra headers. Useful for spelunking and testing... ` \
    `# grep -z -i -v '.*imebra.*' |   ` \
  xargs -0 -I '{}' -P "$(nproc || echo 2)" -n 1 -r \
    bash -c "check_cpp_syntax '{}'"

git ls-files -z -o -m "$@" |
  grep -z -E '.*[.]sh' |
  xargs -0 -I '{}' -P 1 -n 1 -r \
    bash -c "check_sh_syntax '{}'"

git ls-files -z -o -m "$@" |
  grep -z -E '.*[.]yml|.*[.]yaml' |
  xargs -0 -I '{}' -P 1 -n 1 -r \
    bash -c "check_yml_syntax '{}'"

