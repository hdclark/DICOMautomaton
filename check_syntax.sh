#!/bin/bash

# This script can be used to verify the syntax of all files without actually compiling anything.


# Check all files in the project.
#find ./src/ -type f -execdir \
#    g++ --std=c++14 -fsyntax-only '{}' \+


# Check only modified and untracked files.
git ls-files -o -m | 
  grep -E '*[.]h|*[.]cc|*[.]cpp' |
  xargs g++ --std=c++14 -fsyntax-only


