#!/bin/bash

# This script can be used to verify the syntax of all files without actually compiling anything.


# Check all files in the project.
#find ./src/ -type f -execdir \
#    g++ --std=c++14 -fsyntax-only '{}' \+


# Check only modified and untracked files.
git ls-files -z -o -m |
  grep -z -E '*[.]h|*[.]cc|*[.]cpp' |
  xargs -0 -I '{}' -P $(nproc || echo 2) -n 1 -r \
    g++ --std=c++14 -fsyntax-only '{}'

# Compile, but do not link:
#   g++ --std=c++14 -c in.cc -o /dev/null 

