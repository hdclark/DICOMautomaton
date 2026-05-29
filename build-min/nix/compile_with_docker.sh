#!/usr/bin/env bash

sudo docker run \
  -it --rm \
  --network=host \
  -v "$(pwd)":/scratch/:rw \
  -w /scratch/ \
  'nixos/nix':latest \
  /bin/sh -c "
    # Perform the build.
    ./compile_natively.sh

    # Debug interactively.
    /bin/sh
  "

