#!/usr/bin/env bash

# This script can be used to verify the syntax of all files without actually compiling anything.

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
    local f="$*"
    [ -f "$f" ] && {
      g++ --std=c++17 -fsyntax-only \
        -I'src/imebra20121219/library/imebra/include/' \
        $(pkg-config --cflags --libs sdl2 glew sfml-window sfml-graphics sfml-system libpqxx libpq nlopt gsl) \
        "$f"
    }
}
export -f check_cpp_syntax

check_sh_syntax () {
    local f="$*"
    [ -f "$f" ] && {
      shellcheck \
        -e SC1117,SC2059 \
        "$f"
    }
}
export -f check_sh_syntax

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
  grep -z -E '*[.]h|*[.]cc|*[.]cpp' |
    `# Ignore imebra headers. Useful for spelunking and testing... ` \
    `# grep -z -i -v '.*imebra.*' |   ` \
  xargs -0 -I '{}' -P "$(nproc || echo 2)" -n 1 -r \
    bash -c "check_cpp_syntax '{}'"

git ls-files -z -o -m "$@" |
  grep -z -E '*[.]sh' |
  xargs -0 -I '{}' -P 1 -n 1 -r \
    bash -c "check_sh_syntax '{}'"

