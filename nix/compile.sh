#!/bin/sh

## Permit packages that are labelled as being broken to (attempt to) be built.
#mkdir -p ~/.config/nixpkgs/
#echo "{ allowBroken = true; }" >> ~/.config/nixpkgs/config.nix

nix build -L --show-trace -j 8 -f default.nix

