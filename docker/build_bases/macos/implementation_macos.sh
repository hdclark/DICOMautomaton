#!/usr/bin/env bash

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

brew install \
  git \
  svn

brew install \
  coreutils \
  cmake \
  make \
  vim \
  sdl2 \
  rsync \
  wget \
  gsl \
  eigen \
  boost \
  zlib \
  sfml \
  glew \
  jansson \
  libpq \
  nlopt \
  asio \
  thrift \
  gcc \
  llvm

brew install \
  cgal \
  libpqxx \
  libnotify \
  zenity \

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
brew install freeglut 

# There should now be a lot of compilers available.
gcc --version    # System-provided clang (not gcc!)
/Library/Developer/CommandLineTools/usr/bin/clang++ --version  # System-provided clang.
/usr/local/bin/gcc-12 --version   # Homebrew-install gcc.
clang++ --version   # Homebrew-installed clang.

