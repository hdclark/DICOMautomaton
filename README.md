
# ![DICOMautomaton logo](artifacts/logos/DCMA_cycle_opti.svg)

[![Build Status](https://travis-ci.com/hdclark/DICOMautomaton.svg?branch=master)](https://travis-ci.com/hdclark/DICOMautomaton)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![LOC](https://tokei.rs/b1/gitlab/hdeanclark/DICOMautomaton)](https://gitlab.com/hdeanclark/DICOMautomaton)
[![Language](https://img.shields.io/github/languages/top/hdclark/DICOMautomaton.svg)](https://gitlab.com/hdeanclark/DICOMautomaton)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/1ac93861be524c7f9f18324b64960f28)](https://www.codacy.com/app/hdclark/DICOMautomaton?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=hdclark/DICOMautomaton&amp;utm_campaign=Badge_Grade)

## About

`DICOMautomaton` is a multipurpose tool for analyzing *medical physics* data
with a focus on automation. It runs on Linux. It has first-class support for:

  - images (2D and 3D; CT, MRI, PET, RT dose),
  - surface meshes (2D surfaces embedded in 3D),
  - 2D planar contours embedded in 3D,
  - point clouds (3D),
  - registration and warp transformations (rigid and deformable in 3D),
  - radiotherapy plans, and
  - line samples (i.e., discretized 1D to 1D mappings).

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
    - server-client model for off-site installation, can provide access to
      non-Linux systems and low-powered client computers

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
    - This Plate Spline (TPS) deformable registration
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
    - OFF (read and write; partial)
    - OBJ (read and write; partial)
    - STL (read and write)
  - point clouds
    - OFF (read and write; partial)
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
2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Hal Clark.

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

Dependencies are listed in [PKGBUILD](PKGBUILD), using Arch Linux package naming
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

### Quick solution

[Download the latest AppImage artifact from the continuous integration server
here](https://hdclark.github.io/DICOMautomaton-x86_64.AppImage). Installation is
not necessary, but functionality may be missing and optimization may be limited.

     $>  curl https://hdclark.github.io/DICOMautomaton-x86_64.AppImage > dicomautomaton_dispatcher
     $>  chmod 777 dicomautomaton_dispatcher
     $>  ./dicomautomaton_dispatcher -h

### Comprehensive solution

Compile from source to get all functionality and ensure compatibility with your
system.

This project uses CMake. Use the usual commands to compile on Linux:

     $>  git clone https://gitlab.com/hdeanclark/DICOMautomaton/ && cd DICOMautomaton/ # or
     $>  git clone https://github.com/hdclark/DICOMautomaton/ && cd DICOMautomaton/
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
(refer to guide in [documentation/](documentation/)).

## Containerization

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

## Building Portable Binaries

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

## Other Build Options

A portable `AppImage` can be generated using an existing `Docker` image. This
method supports graphical operations, but suffers from the same general `glibc`
incompatibility issues described above. However, it works well if your system
`glibc` is newer than (or equivalent to) that provided by `Debian` stable.
External, runtime support programs (e.g., `Zenity`, `Gnuplot`) may be
incompatible or missing altogether. At the moment no canonical `AppImage`s are
provided, though [continuous integration artifacts *are* available
here](https://hdclark.github.io/DICOMautomaton-x86_64.AppImage). See
`docker/scripts/debian_stable/` for instructions showing how to generate your
own `AppImage`.

A dedicated `Linux` system can be bootstrapped using an up-to-date `Arch Linux`
system that will package the system-installed `DICOMautomaton` in a truly
portable virtual machine that can be emulated using `qemu`, including a
graphical display. External, runtime support programs *can* be bundled this way,
so this method provides the most reliable means of archiving a specific version.
See `linux/`. Note that this method is experimental.

`DICOMautomaton` can also be built using the `Nix` package manager. See `nix/`.
Note that this method is experimental.

## Known Issues

  - The `SFML_Viewer` operation hangs on some systems after viewing a plot with
    Gnuplot. This stems from a known issue in Ygor.

  - Building with `musl` may cause character conversion to fail for some DICOM
    files in some circumstances.

  - Some operations make use of threading and create filesystem mutexes to avoid
    race conditions. If execution is unexpectedly terminated a mutex may remain
    and stall/hang future operations. This can be resolved by manually removing
    the mutex.

## Project Home

The `DICOMautomaton` homepage can be found at <http://www.halclark.ca/>. Source
code is available at <https://gitlab.com/hdeanclark/DICOMautomaton/> and
<https://github.com/hdclark/DICOMautomaton/>.
