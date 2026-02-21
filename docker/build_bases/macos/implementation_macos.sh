#!/usr/bin/env bash
# shellcheck disable=SC2086
# SC2086: Double quote to prevent globbing and word splitting - intentionally disabled for package lists.

# This script installs packages needed to compile DICOMautomaton on MacOSX 10.15.4 (Catalina).
#
# Warning: This script assumes a fresh install of the operating system and might overwrite/destroy an existing homebrew setup!

set -eux

printf 'WARNING: This script may overwrite or destroy an existing system.\n'
printf 'It is designed to be called on a freshly installed macos (or inside some sort of jail).\n'
printf 'Delaying 30 seconds before continuing...\n'
sleep 30

# Install homebrew.
#softwareupdate --install --all  # Will update the OS. Likely not needed.
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

export HOMEBREW_NO_ANALYTICS=1
brew analytics off

# Use the centralized package list script.
# For macOS, the script is invoked directly from the repository (not Docker).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GET_PACKAGES="${SCRIPT_DIR}/../../../scripts/get_packages.sh"

# Get packages from the centralized script.
PKGS_BUILD_TOOLS="$("${GET_PACKAGES}" --os macos --tier build_tools)"
PKGS_DEVELOPMENT="$("${GET_PACKAGES}" --os macos --tier development)"
PKGS_YGOR_DEPS="$("${GET_PACKAGES}" --os macos --tier ygor_deps)"
PKGS_DCMA_DEPS="$("${GET_PACKAGES}" --os macos --tier dcma_deps)"

# Install build tools first (git and svn needed for further package installs)
brew install ${PKGS_BUILD_TOOLS}

`# Development tools `
brew install ${PKGS_DEVELOPMENT}

`# Ygor and DCMA dependencies `
brew install ${PKGS_YGOR_DEPS} ${PKGS_DCMA_DEPS}

# Alter mesa formula before installing because it fails to build on catalina.
EDITOR=true brew edit mesa  # Add '-Wno-register' {C,CPP,CXX}FLAGS env variables.

patch -u --ignore-whitespace << 'EOF'
--- mesa.rb     2023-04-27 15:52:29.000000000 +0000
+++ mesa.rb     2023-04-25 19:34:02.000000000 +0000
@@ -118,6 +118,11 @@
       ]
     end
 
+    ENV["CFLAGS"] = "-Wno-register"
+    ENV["CCFLAGS"] = "-Wno-register"
+    ENV["CXXFLAGS"] = "-Wno-register"
+    ENV["LDFLAGS"] = "-Wno-register"
+
     system "meson", "build", *args, *std_meson_args
     system "meson", "compile", "-C", "build"
     system "meson", "install", "-C", "build"
EOF

#brew install --build-from-source /usr/local/Homebrew/Library/Taps/homebrew/homebrew-core/Formula/mesa.rb
brew install --build-from-source ./mesa.rb
#HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_UPGRADE=1 HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1 brew install freeglut

# There should now be a lot of compilers available.
gcc --version    # System-provided clang (not gcc!)
/Library/Developer/CommandLineTools/usr/bin/clang++ --version  # System-provided clang.
/usr/local/bin/gcc-12 --version   # Homebrew-install gcc.
clang++ --version   # Homebrew-installed clang.


