#!/bin/sh

set -eux

# Provide git binary so we can use builtin.fetchGit.
nix-env -i git

# Attempt to build all necessary packages.
export NIXPKGS_ALLOW_UNSUPPORTED_SYSTEM=1
export NIXPKGS_ALLOW_BROKEN=1
export NIXPKGS_ALLOW_INSECURE=1
export NIXPKGS_ALLOW_UNFREE=1

#mkdir -pv /etc/nixos/
#echo '{ nixpkgs.config.allowUnsupportedSystem = true; }' >> /etc/nixos/configuration.nix
#mkdir -pv ~/.config/nixpkgs/
#echo '{ allowUnsupportedSystem = true; }' >> ~/.config/nixpkgs/config.nix

# Attempt the build.
nix build \
  -L \
  --show-trace \
  -j 8 \
  -f default.nix \
  --arg toolchainStatic   false \
  --arg toolchainMusl     false \
  --arg toolchainMingw64  false \
  --arg toolchainWasi     false \
#  --arg toolchainClang    true

# Install the result into the current environment.
nix-env -i ./result

