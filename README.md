
![DICOMautomaton logo](artifacts/logos/DCMA_cycle_opti.svg)

[![Build Status](https://travis-ci.com/hdclark/DICOMautomaton.svg?branch=master)](https://travis-ci.com/hdclark/DICOMautomaton)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![LOC](https://tokei.rs/b1/gitlab/hdeanclark/DICOMautomaton)](https://gitlab.com/hdeanclark/DICOMautomaton)
[![Language](https://img.shields.io/github/languages/top/hdclark/DICOMautomaton.svg)](https://gitlab.com/hdeanclark/DICOMautomaton)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/1ac93861be524c7f9f18324b64960f28)](https://www.codacy.com/app/hdclark/DICOMautomaton?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=hdclark/DICOMautomaton&amp;utm_campaign=Badge_Grade)

# About

`DICOMautomaton` is a collection of tools for analyzing *medical physics* data,
specifically dosimetric and medical imaging data in the DICOM format. It has
become something of a platform that provides a variety of functionality.
`DICOMautomaton` is designed for easily developing customized workflows.


The basic workflow is:

  1. Files are loaded (from a DB or various types of files).

  2. A list of operations are provided and sequentially performed, mutating the
     data state.

  3. Files of various kinds can be written or a viewer can be invoked. Both
     are implemented as operations that can be chained together sequentially.

Some operations are interactive. Others will run on their own (possibly for for
days or even weeks). Each operation provides a description of the parameters
that can be configured. To see this documentation, invoke:

    $>  dicomautomaton_dispatcher -u

and for general information invoke:

    $>  dicomautomaton_dispatcher -h

NOTE: `DICOMautomaton` should NOT be used for clinical purposes. It is suitable
for research or support tool purposes only.


# License and Copying

All materials herein which may be copywrited, where applicable, are. Copyright
2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019 Hal Clark.

See [LICENSE.txt] for details about the license. Informally, `DICOMautomaton` is
available under a GPLv3+ license. The Imebra library is bundled for convenience
and was not written by the author; consult the Imebra license file
[src/imebra/license.txt].

All liability is herefore disclaimed. The person(s) who use this source and/or
software do so strictly under their own volition. They assume all associated
liability for use and misuse, including but not limited to damages, harm,
injury, and death which may result, including but not limited to that arising
from unforeseen or unanticipated implementation defects.


# Dependencies

Dependencies are listed in [PKGBUILD], using Arch Linux package
naming conventions, and in [CMakeLists.txt] using Debian package naming
conventions.

Notably, `DICOMautomaton` depends on the author's "Ygor", "Explicator", 
and "YgorClustering" projects which are hosted at:

  - Ygor: [https://gitlab.com/hdeanclark/Ygor] and
    [https://github.com/hdclark/Ygor].

  - Explicator: [https://gitlab.com/hdeanclark/Explicator] and
    [https://github.com/hdclark/Explicator].

  - YgorClustering (needed only for compilation):
    [https://gitlab.com/hdeanclark/YgorClustering] and
    [https://github.com/hdclark/YgorClustering].

  
# Installation

This project uses CMake. Use the usual commands to compile on Linux:

     $>  git clone https://gitlab.com/hdeanclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  git clone https://github.com/hdclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  cd /path/to/source/directory
     $>  mkdir build && cd build/

Then, iff by-passing your package manager:

     $>  cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
     $>  make && sudo make install

Or, if building for Debian:

     $>  cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
     $>  make && make package
     $>  sudo apt install -f ./*.deb

Or, if building for Arch Linux:

     $>  rsync -aC --exclude build ../ ./
     $>  makepkg --syncdeps --noconfirm # Optionally also [--install].

Direct installation on non-Linux systems is not officially supported. However, a
portable Docker image can be built that is portable across non-Linux systems.
`DICOMautomaton` can be installed on Android inside a termux environment
(refer to guide in [documentation/]).


# Containerization

`DICOMautomaton` can be built as a Docker image. This method automatically
handles installation of all dependencies. The resulting image can be run
interactively, or accessed through a web server.

In order to build the Docker image, you will need `git`, `Docker`, and a `bash`
shell. On `Windows` systems the `git` shell should be used. To build the image:

     $>  git clone https://gitlab.com/hdeanclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  git clone https://github.com/hdclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  cd /path/to/source/directory
     $>  ./docker/build_bases/arch/build.sh
     $>  ./docker/builder/arch/build.sh

After building, the default webserver can be launched using the convenience
script:

     $>  ./docker/scripts/arch/Run_Container.sh

and a container can be run interactively with the convenience script:

     $>  ./docker/scripts/arch/Run_Container_Interactively.sh

[Continuous integration](https://travis-ci.com/hdclark/DICOMautomaton) is used
to build Docker images for all commits using `Travis-CI`. Build artifacts may be
available [here](https://travis-ci.com/hdclark/DICOMautomaton), but are
unofficial. Docker containers can be built using Arch Linux, Debian, or Void
Linux bases; Arch Linux and Void Linux provide the latest upstream packages,
whereas Debian provides greater portability since an older `glibc` is used. Arch
Linux builds use `glibc`, Void Linux builds use `musl`.


# Portable Binaries

The well-known `LD_PRELOAD` trick can be used to provide somewhat portable
`DICOMautomaton` binaries for Linux systems. Binaries from the system-installed
or locally-built `DICOMautomaton` will be automatically gathered by building and
then invoking:

     $>  ./scripts/dump_portable_dcma_bundle.sh /tmp/portable_dcma/

If successful, the portable outputs will be dumped to `/tmp/portable_dcma/`. A
convenience script that performs the preload trick and forwards all user
arguments is `portable_dcma`.

Note that this trick works *only* on Linux systems, and a similar Linux system
must be used to generate the binaries. The interactive Debian Docker container
will likely suffice. Additionally this technique only provides the
`dicomautomaton_dispatcher` binary. All shared libraries needed to run it are
bundled, including `glibc` and some other intrinsic libraries in case the host
and target `glibc` differ. If the `patchelf` program is available, the binary
can be patched to use the bundled `ld-linux.so` interpreter and `glibc` using
the included `adjusting_dcma` script, otherwise the system interpreter will be
used. If `patchelf` is not available it is best to remove `ld-linux`, `libm`,
and `libc` from the bundle and rely fully on the target `glibc`. Mixing and
matching bits of different `glibc` installations will almost certainly result in
segmentation faults or silent failures so it is not recommended in any
circumstances. Also note that compilation arguments and architecture-specific
tunings will likely ruin portability. 

Alternatively, a third wrapper script (`emulate_dcma`) uses `qemu-x86_64` to
emulate a 64 bit x86 system and preload bundled libraries. This script may work
when the native `LD_PRELOAD` trick fails, but emulation may be slow. The cpu can
be emulated, so it may be possible to support architecture-specific tunings this
way.

Portability, validity of the program, and full functionality are *NOT*
guaranteed using either script! They should all be considered experimental. The
preload trick is best run in a controlled environment, and targetting the same
controlled environment and architecture. This method of distributing
`DICOMautomaton` is not officially supported, but can simplify distributing
custom builds in some situations. 


# Known Issues

- The `SFML_Viewer` operation hangs on some systems after viewing a plot with
  Gnuplot. This stems from a known issue in Ygor. 

- Building with `musl` may cause character conversion to fail for some DICOM
  files in some circumstances.

# Project Home

The `DICOMautomaton` homepage can be found at [http://www.halclark.ca/]. Source
code is available at [https://gitlab.com/hdeanclark/DICOMautomaton/] and
[https://github.com/hdclark/DICOMautomaton/].


