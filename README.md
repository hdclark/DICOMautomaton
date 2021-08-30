# ![DICOMautomaton logo](artifacts/logos/DCMA_cycle_opti.svg)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Language](https://img.shields.io/github/languages/top/hdclark/DICOMautomaton.svg)](https://gitlab.com/hdeanclark/DICOMautomaton)
[![LOC](https://tokei.rs/b1/gitlab/hdeanclark/DICOMautomaton)](https://gitlab.com/hdeanclark/DICOMautomaton)

[![Language Grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/hdclark/DICOMautomaton.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/hdclark/DICOMautomaton/context:cpp)
[![GitLab CI Pipeline Status](https://gitlab.com/hdeanclark/DICOMautomaton/badges/master/pipeline.svg)](https://gitlab.com/hdeanclark/DICOMautomaton/-/commits/master)

[![Latest Release DOI](https://zenodo.org/badge/89630691.svg)](https://zenodo.org/badge/latestdoi/89630691)

## About

`DICOMautomaton` is a multipurpose tool for analyzing *medical physics* data
with a focus on automation. It has first-class support for:

  - images (2D and 3D; CT, MRI, PET, RT dose),
  - surface meshes (2D surfaces embedded in 3D),
  - 2D planar contours embedded in 3D,
  - point clouds (3D),
  - registration and warp transformations (rigid and deformable in 3D),
  - radiotherapy plans, and
  - line samples (i.e., discretized scalar-valued functions over $`\mathbb{R}^1`$).

There are four ways of operating `DICOMautomaton`:

  - command-line interface
    - most flexible, best for automation
    - not interactive, not a repl
  - a minimal graphical interface
    - best for contouring and simple evaluations
    - can be mixed with the command-line interface
  - a terminal graphical interface
    - extremely simplisitic, meant for question-answer interactions
    - can be mixed with the command-line interface
  - web interface
    - supports modal workflow interactions
    - not all operations are supported (e.g., contouring is not currently
      supported)
    - server-client model for off-site/remote installation, can provide
      cross-platform and low-powered client access

`DICOMautomaton` provides a diverse array of functionality, including
implementations of the following well-known algorithms and analytical
techniques:

  - contouring
    - interactive contouring
    - threshold-based contouring (2D or 3D)
    - contour erosion/dilation (2D or 3D)
    - sub-segmentation (i.e., splitting contour collections into 2D/3D compartments)
    - confined region-of-interest (ROI) image processing/analysis (2D or 3D)
    - 2D boolean operations
  - surface reconstruction and processing
    - Marching Cubes
    - restricted Delauney reconstruction
    - contours-to-contours interpolation (i.e., keyhole surface meshing)
    - mesh subdivision
    - mesh simplification
    - 3D boolean operations, erosion, and dilation/margins (exact for small
      vertex counts, and via image representation for large vertex counts)
  - point cloud registration
    - Iterative Closest Point (ICP)
    - Procrustes algorithm
    - Affine registration
    - Principal Component Analysis (PCA) rigid registration
    - Thin Plate Spline (TPS) deformable registration
      - when correspondence is known a priori, e.g., fiducial or feature matching
    - Thin Plate Spline Robust Point Matching (TPS-RPM) deformable registration
      - for when correspondence is unknown and must also be estimated
      - includes soft-assign and simulated annealing algorithm implementations
      - extensions: double-sided outlier handling, hard constraints, and multiple
        solver methods for improved robustness in degenerate situations
    - warping (i.e., applying registration transformations to generic objects)
  - detection of shapes within point clouds
    - Random sample concensus (RANSAC) for primitive shapes
    - MR distortion quantification for lattice grids
  - rudimentary image processing techniques
    - morphological operations (e.g., open/close, erode/dilate)
    - normalization, standardization, and locally adaptive techniques (2D or 3D)
    - convolution (2D or 3D)
    - sub-image search (2D or 3D)
    - median filtering, min/max filtering, percentile transformation (2D or 3D)
    - Otsu thresholding, simplistic thresholding
    - image gradients (e.g., Sobel, Scharr; first-order, second-order)
    - edge detection, non-maximum edge suppression
  - quantitative medical image analysis
    - gamma analysis, distance to agreement analysis (2D/3D to 2D or 3D)
    - Response Evaluation Criteria in Solid Tumours (RECIST) features
    - perfusion imaging quantfication: pharmacokinetic modeling for Dynamic
      Contrast-Enhanced (DCE) imaging (CT or MR)
    - diffusion imaging quantification: Intravoxel Incoherent Motion (IVIM)
      modeling and Apparent Diffusion Coefficient (ADC) estimation
    - time series analysis (i.e., '4D' analysis)
  - radiobiology
    - Biologically Effective Dose (BED) transformations
    - Equivalent Dose in 2 Gy Fractions (EQD2) transformations
    - extraction of alpha/beta from EQD2 images
    - tissue recovery, including the Jones et al 2014 model
    - standard Tissue Control Probability (TCP) models
    - standard Normal Tissue Control Probability (NTCP) models
    - conformity and heterogeneity index estimation
  - radiotherapy planning
    - rudimentary optimization for fixed-field plans
    - Dose Volume Histogram (DVH) extraction
    - clinical protocol evaluation
    - rudimentary automated plan checking support
    - re-treatment dose sculpting/cropping/trimming for base-planning
  - routine quality assurance
    - fully nonparametric multileaf collimator (MLC) picket fence leaf
      displacement quantification
    - light-radiation correspondence using edge-finding
    - basic image quality measures (e.g., mean, standard deviation)
  - clustering
    - Density-Based Spatial Clustering of Applications with Noise (DBSCAN)
    - k-means
    - connected components
  - geometry
    - 3D geometric primitives (vectors, lines, planes, spheres)
    - distance, closest points, and intersection point estimation
    - Hausdorff distance for point clouds
    - 'point-in-polygon' routines based on winding number and ray intersection
    - ray tracing / path sampling through voxelated geometries or surface point
      sampling
  - numerics
    - Kahan summation routines (i.e., 'compensated' summation)
    - Chebyshev polynomial approximations
    - Wasserman's 'local linear nonparametric regression' (NPRLL) algorithms
    - numerical optimization (Nelder-Mead simplex and more modern algorithms)
    - robust weighted least squares
    - descriptive statistics (min/mean/median/max/percentiles, correlation
      coefficients, and simple test statistics)
  - miscellaneous
    - transformations between data types (e.g., images to surface meshes,
      surface meshes to point clouds, point clouds to images)
    - fuzzy string matching (including Levenshtein, Jaro-Winkler, n-grams,
      Soundex, and Metaphone algorithms)
    - radiomic signatures (a subset of the IBSI-defined biomarkers)
    - A\* pathfinding (partial)
    - R\*-trees
    - 2D contouring via mapping to a 'voxel world game' (not maintained)
    - many operations support concurrent computation via threading (currently
      CPU only)

Notably absent but (eventually) planned features:

  - radiotherapy dose calculation
  - direct image registration (compared with indirect registration via feature
    extraction and point-cloud registration)
  - SURF/SIFT/KAZE feature extraction

Interchange is important. `DICOMautomaton` supports the following standard file
formats:

  - images/dose
    - DICOM CT (read and write)
    - DICOM MR (read)
    - FITS (read and write)
    - DICOM RTDOSE (read and write)
    - 3ddose (read)
  - contours
    - DICOM RTSTRUCT (read and write)
  - surface meshes
    - PLY (read and write; ASCII and binary; partial)
    - OBJ (read and write; partial)
    - OFF (read and write; partial)
    - STL (read and write; ASCII and binary)
  - point clouds
    - PLY (read and write; ASCII and binary; partial)
    - OBJ (read and write; partial)
    - OFF (read and write; partial)
    - XYZ (read and write)
  - registration
    - 16 parameter Affine or rigid transformation text files
    - Thin Plate Spline transformation text files (read and write)
  - radiotherapy plans
    - DICOM RTPLAN (read)
  - line samples
    - CSV (read and write)
  - reporting
    - CSV (write)
    - TSV (write)
  - direct database query (e.g., to interface directly with a PACs)

Customized file formats are provided for snapshotting internal state.

The basic workflow is:

 1. Files are loaded (from a DB or files).

 2. A list of operations are sequentially performed, mutating the data state.

 3. Files of various kinds can be written or a viewer can be invoked. Both
    are implemented as operations that can be chained together sequentially.

Some operations are interactive. Others will run on their own (possibly for days
or even weeks). See [integration\_tests/tests/](integration_tests/tests/) for
specific examples.

Each operation provides a description of the parameters that can be configured.
To see the exact, up-to-date documentation, invoke:

    $>  dicomautomaton_dispatcher -u

and for general information invoke:

    $>  dicomautomaton_dispatcher -h

Alternatively, see [documentation/](documentation/) for documentation snapshots.

## Clinical Use

`DICOMautomaton` should **NOT** be used for clinical purposes. It is suitable
*only* for research purposes or in a non-critical supporting role where outputs
can be easily validated.

While efforts have been made to verify integrity and validity of the code, no
independent audit or review has been performed. The breadth of functionality
would make it difficult to test all operations combinations. We therefore rely
on static analysis, code quality metrics, and a limited amount of integration
testing for specific workflows.

## License and Copying

All materials herein which may be copywrited, where applicable, are. Copyright
2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Hal
Clark and other contributing authors.

See [LICENSE.txt](LICENSE.txt) for details about the license. Informally,
`DICOMautomaton` is available under a GPLv3+ license. The Imebra library is
bundled for convenience and was not written by the author; consult the Imebra
[license file](src/imebra20121219/license.txt) which is informally a simplified
BSD-like license.

All liability is herefore disclaimed. The person(s) who use this source and/or
software do so strictly under their own volition. They assume all associated
liability for use and misuse, including but not limited to damages, harm,
injury, and death which may result, including but not limited to that arising
from unforeseen or unanticipated implementation defects.

## Dependencies

Dependencies are listed in [PKGBUILD](PKGBUILD) using Arch Linux package naming
conventions, and in [CMakeLists.txt](CMakeLists.txt) using Debian package naming
conventions.

Notably, `DICOMautomaton` depends on the author's `Ygor`, `Explicator`, and
`YgorClustering` projects which are hosted at:

  - `Ygor`: <https://gitlab.com/hdeanclark/Ygor> and
    <https://github.com/hdclark/Ygor>.

  - `Explicator`: <https://gitlab.com/hdeanclark/Explicator> and
    <https://github.com/hdclark/Explicator>.

  - `YgorClustering` (needed only for compilation):
    <https://gitlab.com/hdeanclark/YgorClustering> and
    <https://github.com/hdclark/YgorClustering>.

## Installation

### Quick start (Linux)

**[Download the latest AppImage artifact from the continuous integration server
here](http://halclark.ca/ci/DICOMautomaton-latest-x86_64.AppImage)** or via:

     $>  curl http://halclark.ca/ci/DICOMautomaton-latest-x86_64.AppImage > dicomautomaton_dispatcher
     $>  chmod 777 dicomautomaton_dispatcher
     $>  ./dicomautomaton_dispatcher -h

This artifact corresponds to the latest successful build on
<http://halclark.ca/ci/>. Please confirm the
[checksum](http://halclark.ca/ci/DICOMautomaton-latest-x86_64.AppImage.sha256sum).
Installation is not necessary, but the file can be renamed and installed in a
standard location for convenience (e.g., `/usr/bin/`).

#### Notes and caveats:

  - This is **not** an official release. It may be lacking functionality, and is
    almost certainly *not* optimized.

  - The CI build environment is currently based on `Debian` oldstable. Attempting to
    run on systems with older `glibc`s will likely fail.

  - `AppImage`s require `FUSE` support, so running in Docker will not work.
    However, `AppImages` can be extracted and run *without* `FUSE` via:

        $> ./DICOMautomaton-x86_64.AppImage --appimage-extract
        $> ./squashfs-root/usr/bin/dicomautomaton_dispatcher -h

  - The CI `AppImage` currently expects graphical components to be available on
    the host system. It will fail if `libGL`, `freetype`, or `libstdc++` libraries
    are either incompatible or missing.

  - See <https://gitlab.com/hdeanclark/DICOMautomaton> or
    <https://github.com/hdclark/DICOMautomaton> for sources and build scripts.

### Compiling

Compile from source to get all functionality and ensure compatibility with your
system.

This project uses `CMake`. Use the usual commands to compile on `Linux`:

     $>  git clone https://gitlab.com/hdeanclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  git clone https://github.com/hdclark/DICOMautomaton/ && cd DICOMautomaton/
     $>  cd /path/to/source/directory
     $>  mkdir build && cd build/

Then, if building for `Debian`:

     $>  cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
     $>  make && make package
     $>  sudo apt install -f ./*.deb

Or, if building for `Arch Linux`:

     $>  rsync -aC --exclude build ../ ./
     $>  makepkg --syncdeps --noconfirm # Optionally also [--install].

Otherwise, if installing directly (i.e., by-passing your package manager):

     $>  cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
     $>  make && sudo make install

Direct installation on non-`Linux` systems is not officially supported. However,
`Docker` images can be built that are portable across non-`Linux` systems (see
below). 

## Containerization

`DICOMautomaton` can be built as a `Docker` image. This method automatically
handles installation of all dependencies. The resulting image can be run
interactively or accessed through a web server.

In order to build the `Docker` image, you will need `git`, `Docker`, and a
`bash` shell. On `Windows` systems the `git` shell should be used. To build the
image:

     $>  git clone https://gitlab.com/hdeanclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  git clone https://github.com/hdclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  cd /path/to/source/directory
     $>  ./docker/build_bases/arch/build.sh
     $>  ./docker/builder/arch/build.sh

After building, the default web server can be launched using the convenience
script:

     $>  ./docker/scripts/arch/Run_Container.sh

and a container can be run interactively with the convenience script:

     $>  ./docker/scripts/arch/Run_Container_Interactively.sh

### Pre-Built `Docker` Images

`Docker` containers are available in three variants: using `Arch Linux`, `Debian`,
or `Void Linux` base images. `Arch Linux` and `Void Linux` provide the latest
upstream packages, whereas `Debian` provides greater portability since an older
`glibc` is used. `Arch Linux` builds use `glibc` whereas `Void Linux` builds use
`musl`.

Build base images contain all dependencies and requirements necessary to compile
`DICOMautomaton`, but may not themselves contain `DICOMautomaton`. The latest
successfully-built base images are available from `Docker Hub`:

  - [![Arch Linux](https://img.shields.io/badge/Latest_Docker_Build_Base-Arch_Linux-brightgreen)](https://hub.docker.com/r/hdclark/dcma_build_base_arch)

  - [![Debian OldStable](https://img.shields.io/badge/Latest_Docker_Build_Base-Debian_oldstable-brightgreen)](https://hub.docker.com/r/hdclark/dcma_build_base_debian_oldstable)

  - [![Void Linux](https://img.shields.io/badge/Latest_Docker_Build_Base-Void_Linux-brightgreen)](https://hub.docker.com/r/hdclark/dcma_build_base_void)

  - [![MXE](https://img.shields.io/badge/Latest_Docker_Build_Base-MXE-brightgreen)](https://hub.docker.com/r/hdclark/dcma_build_base_mxe)

### Continuous Integration

Continuous integration is used to build `Docker` images, `AppImage`s,
cross-compile and perform tests for all commits. `Docker` build
artifacts may be available [here](http://halclark.ca/ci/).
Additional build environments and `AppImage` portability are tested with
`GitHub` and `GitLab` CI pipelines; build artifacts are available
[here](https://gitlab.com/hdeanclark/DICOMautomaton/-/pipelines).
Direct links for the latest build artifacts:

  - [`Arch Linux`](https://gitlab.com/hdeanclark/DICOMautomaton/builds/artifacts/master/download?job=build_ci_arch)
  - [`Debian OldStable`](https://gitlab.com/hdeanclark/DICOMautomaton/builds/artifacts/master/download?job=build_ci_debian_oldstable)
  - [`MXE` (i.e., `Windows` executables)](https://gitlab.com/hdeanclark/DICOMautomaton/builds/artifacts/master/download?job=cross_compile_mxe)

**Note that all CI artifacts are not optimized and core functionality may be missing.**

## Building Portable Binaries

The well-known `LD_PRELOAD` trick can be used to provide somewhat portable
`DICOMautomaton` binaries for `Linux` systems. Binaries from the
system-installed or locally-built `DICOMautomaton` will be automatically
gathered by building and then invoking:

     $>  ./scripts/dump_portable_dcma_bundle.sh /tmp/portable_dcma/

If successful, the portable outputs will be dumped to `/tmp/portable_dcma/`. A
convenience script that performs the preload trick and forwards all user
arguments is `portable_dcma`.

Note that this trick works *only* on `Linux` systems, and a similar `Linux`
system must be used to generate the binaries. The interactive `Debian` `Docker`
container will likely suffice. Additionally this technique only provides the
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

## Other Build Options

A portable `AppImage` can be generated using an existing `Docker` image. This
method supports graphical operations, but suffers from the same general `glibc`
incompatibility issues described above. However, it works well if your system
`glibc` is newer than (or equivalent to) that provided by `Debian` oldstable.
External, runtime support programs (e.g., `Zenity`, `Gnuplot`) may be
incompatible or missing altogether. At the moment no canonical `AppImage`s are
provided, though continuous integration artifacts *are* available (see above).
Refer to `docker/scripts/debian_oldstable/` for instructions showing how to
generate your own `AppImage`.

A dedicated `Linux` system can be bootstrapped using an up-to-date `Arch Linux`
system that will package the system-installed `DICOMautomaton` in a truly
portable virtual machine that can be emulated using `qemu`, including a
graphical display. External, runtime support programs *can* be bundled this way,
so this method provides the most reliable means of archiving a specific version.
See `linux/`. Note that this method is experimental.


`DICOMautomaton` can also be built using the `Nix` package manager. See `nix/`.
Note that this method is experimental.

`DICOMautomaton` can be installed on `Android` inside a `Termux` environment
(refer to guide in [documentation/](documentation/)).

## Citing

If you use `DICOMautomaton` in an academic work, we ask that you please cite the
most relevant publication for that work or the most relevant release DOI, if possible.

`DICOMautomaton` can be cited **as a whole** using
[doi:10.5281/zenodo.4088796](https://doi.org/10.5281/zenodo.4088796).

Individual releases are assigned a DOI too; the latest release DOI can be found via
[![DOI](https://zenodo.org/badge/89630691.svg)](https://zenodo.org/badge/latestdoi/89630691)
or by clicking [here](https://zenodo.org/badge/latestdoi/89630691).

Finally, several publications describe core functionality of `DICOMautomaton`
and may be more appropriate to cite.

## Known Issues

  - The `SFML_Viewer` operation hangs on some systems after viewing a plot with
    `Gnuplot`. This stems from a known issue in `Ygor`.

  - Building with `musl` may cause character conversion to fail for some DICOM
    files in some circumstances.

  - Some operations make use of threading and create filesystem mutexes to avoid
    race conditions. If execution is unexpectedly terminated a mutex may remain
    and stall/hang future operations. This can be resolved by manually removing
    the mutex.

  - If you are limited by an `OpenGL` version earlier than 3.0, for example in a
    `VirtualBox` virtual machine, the `SDL` viewer may fail to load. This can be
    worked around by switching to `Mesa`-based software rendering by configuring
    the entire system or defining the `LIBGL_ALWAYS_SOFTWARE=1` environment
    variable.

## Project Home

The `DICOMautomaton` homepage can be found at <http://www.halclark.ca/>. Source
code is available at <https://gitlab.com/hdeanclark/DICOMautomaton/> and
<https://github.com/hdclark/DICOMautomaton/>.
