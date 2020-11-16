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
  \
  `# Pin nixpkgs version. See https://status.nixos.org for current build statuses. ` \
  --arg nixpkgs 'import (fetchTarball "https://github.com/NixOS/nixpkgs/archive/dd1b7e377f6d77ddee4ab84be11173d3566d6a18.tar.gz") { config = import ./config.nix; } ' \
  \
  \
  `# NOTE: None of the toolchains/options below correctly build DCMA. ` \
  `#       There seem to be underlying assumptions in some key dependencies ` \
  `#       that make it hard to compile with musl or cross-compile. ` \
  `#       Ygor and Explicator cross-compile fine, but DCMA is held hostage ` \
  `#       by upstream. Consider disabling DCMA functionality or adjusting ` \
  `#       upstream via config.nix if exotic toolchains are needed. ` \
  \
  \
  `# Build (some) static-objects even on a standard shared-object toolchain.` \
  --arg buildStatic       false \
  \
  `# Use modified toolchains.` \
  --arg toolchainStatic   false \
  --arg toolchainMingw64  false \
  --arg toolchainMusl     false \
  \
  `# Does not work due to dependencies (e.g., bzip)` \
  --arg toolchainWasi     false \


# Install the result into the current environment.
nix-env -i ./result

