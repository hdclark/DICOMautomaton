#!/bin/sh

set -eux

# Provide git binary so we can use builtin.fetchGit.
nix-env -i git

# Attempt to build all necessary packages.
#export NIXPKGS_ALLOW_UNSUPPORTED_SYSTEM=1
#export NIXPKGS_ALLOW_BROKEN=1
#export NIXPKGS_ALLOW_INSECURE=1
#export NIXPKGS_ALLOW_UNFREE=1

#mkdir -pv /etc/nixos/
#echo '{ nixpkgs.config.allowUnsupportedSystem = true; }' >> /etc/nixos/configuration.nix
#mkdir -pv ~/.config/nixpkgs/
#echo '{ allowUnsupportedSystem = true; }' >> ~/.config/nixpkgs/config.nix

# Attempt the build.
nix-build \
  --show-trace \
  -j 8 \
  --option sandbox true \
  \
  ./nixpkgs.nix \
  -A ygor \
  -A ygorclustering \
  -A explicator \
  -A dicomautomaton

#  ./nixpkgs.nix \

# Install the result(s) into the current environment.
nix-env -i ./result*

