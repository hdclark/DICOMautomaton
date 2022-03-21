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
        g++ --std=c++17 -fsyntax-only \
          -I'src/imebra20121219/library/imebra/include/' \
          -I"${temp_dir}/" \
          -I"${temp_dir}/src/" \
          `# Check if Eigen is available. If so, enable via preprocessor directive.` \
          ` [ ! -z "$(pkg-config --cflags --libs eigen3)" ] && printf -- '-DDCMA_USE_EIGEN=1' ` \
          $(pkg-config --cflags --libs sdl2 glew sfml-window sfml-graphics sfml-system libpqxx libpq nlopt gsl eigen3) \
          -lygor -lboost_serialization -lboost_iostreams -lboost_thread -lboost_system \
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

# Check all files in the project.
#find ./src/ -type f -print0 |
#    grep -z -E '*[.]cc|*[.]cpp' |
#    grep -z -i -v '.*imebra.*' |
#    xargs -0 -I '{}' -P $(nproc || echo 2) -n 1 -r \
#      g++ --std=c++17 -fsyntax-only '{}'

# Compile, but do not link:
#      g++ --std=c++17 -c '{}' -o /dev/null 

# Check only modified and untracked files.
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

