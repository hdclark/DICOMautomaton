#!/usr/bin/env bash

set -eux

printf 'Overwrite existing files, including existing implementation file?\n'
read
sleep 3

rm -rf gen-*/

printf 'Compiling IDL now...\n'
thrift \
  -strict \
  -recurse \
  -verbose \
  --gen cpp \
  --gen py \
  --gen lua \
  --gen json \
  --gen gv \
  --gen html \
  --gen markdown \
  --gen xml \
  DCMA.thrift

# Rename file extensions for consistency.
for fn in gen-cpp/*.cpp ; do
    mv "$fn" "${fn%.cpp}.cc"
done

# Ensure all possibly-generated files were generated.
touch gen-cpp/DCMA_constants.{h,cc}
touch gen-cpp/DCMA_types.{h,cc}


