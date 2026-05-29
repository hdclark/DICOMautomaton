#!/usr/bin/env bash

# WIP script to make a Docker container with an external SYCL toolchain based on Arch Linux.

exit
exit
exit

pacman -Sy --noconfirm
pacman -S --noconfirm --needed  git wget which rsync vim sed base-devel sudo cmake zlib patchelf

# Install DICOMautomaton dependencies.
pacman -S --noconfirm --needed  clang libclc ocl-icd opencl-mesa pocl clinfo
#pacman -S --noconfirm --needed  opencl-clhpp opencl-headers

pacman -S --noconfirm --needed  boost asio sdl2 glu glew gsl eigen freeglut thrift

# Neuter makepkg so it can build packages as root.
sed -i -e 'g/.*exit.*E_ROOT.*/d' $(which makepkg)

# Download an AUR helper for dependency handling.
git clone --depth=1 https://aur.archlinux.org/trizen.git
( cd trizen &&
  makepkg -si --noconfirm --needed --noprogressbar --skipchecksums --skipinteg --skippgpcheck --force --nocolor
)
trizen --nocolors --quiet --noconfirm -S trizen

# Install AdaptiveCPP and verify it works.
export CC='clang'
export CXX='clang++'
trizen --nocolors --quiet --noconfirm -S adaptivecpp
acpp-info

# Install TriSYCL.
trizen --nocolors --quiet --noconfirm -S trisycl-git

# Set environment variables to select the relevant compiler/toolchain.
export CXX='acpp'
export CXXFLAGS='-fopenmp'
export ACPP_PLATFORM='omp;generic'

#export CC='clang'
#export CXX='clang++'

# Enable relevant flags/toggles in CMake invocation (via PKGBUILD)

# ...





git clone --depth=1 'https://github.com/hdclark/DICOMautomaton.git'
( cd DICOMautomaton/ &&
  ./compile_and_install.sh
)

( cd DICOMautomaton/ &&
  ./scripts/compile_sycl.sh || true
)



