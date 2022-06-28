
# Timeline

Below is a simplified timeline of `DICOMautomaton` development, broken down by month.

## 202206

- CI: added custom gitlab runner tags.
- CI: add wait-and-retry when dealing with (most) package managers.
- ExportFITSImages: notify user how many images are being exported.
- FITS: support full metadata encoding. FITS images are now an option for lossless image transfer.
- FITS: upgrade support from single-image to multi-image for both reader/writer.
- WIP documentation for forthcoming network protocol.

## 202205

- Add operation for decomposing images using SVD (i.e., 'eigenfaces').
- Add TabulateImageMetadata operation.
- SDL\_Viewer: contours: garbage-collect entries for removed ROIs.
- SDL\_Viewer: support hot-keys (open, quit, help).

## 202204

- Add grid-like test meshes to test simplification.
- Address operation parameter compiler warning (unused parameters).
- Always expose SimplifySurfaceMeshes and QueryUserInteractively operations.
- AppImage: proceed if git commit hash not available.
- CMakeLists: enable optimization for memory-constrained builds.
- ConvertContoursToMeshes: add convex hull method.
- QueryUserInteractively: include header.
- ReduceNeighbourhood: add geometric mean reduction method.
- SDL\_Viewer: address minor compiler warnings.
- SDL\_Viewer: add tooltip 'advertising' drag-and-drop.
- SDL\_Viewer: support file drag-and-drop loading.
- SimplifySurfaceMeshes: add Ygor 'flat' mesh simplification method.
- SimplifySurfaceMeshes: disregard option for disengaged functionality.
- SimplifySurfaceMeshes: expose Ygor MinAlignAngle threshold criteria.
- SimplifySurfaceMeshes: fix documentation layout typo.
- Workaround for git CVE-2022-24765 on Arch.

## 202203

- Added Time and Sleep operations.
- Added file selector helper script.
- Add libnotify as optional (runtime) dependency.
- Add transform file reader and expand writer.
- AnalyzePicketFence: consolidate CSV output files.
- Bug fix: replace numeric\_limits::min --> lowest.
- check\_syntax: auto-enable Eigen, if available.
- DICOM image loading: provide better message if rows/columns data missing.
- DICOM metadata: add optional pixel extrema.
- DICOM reader: add optional metadata.
- DICOM: support multi-frame images.
- Ensure alignment classes can be in-place constructed from a stream.
- ExportWarps: ensure file has consistent extension.
- Expose Orthogonal Procrustes (SVD) point set alignment.
- ModelIVIM: replace placeholder image metadata.
- OrderImages: ensure the default variant is provided.
- OrderImages: support stable image array sorting.
- OrthogonalProcrustes: confirm solution validity.
- OrthogonalProcrustes: disable use of SVDBase.
- OrthogonalProcrustes: parametrize mirroring and isotropic scaling.
- Pixel mapping: clamp rescale for good measure.
- Pixel mapping: protect against degenerate cases.
- ReduceNeighbourhood: label neighbourhood arg examples as exhaustive.
- Re-introduce previous resample-less WarpImages.
- Rename 'TPlan' to 'RTPlan' for clarity.
- RTIMAGE: explicitly apply top-level LUT.
- Script: add img sort after partitioning.
- SDL\_Viewer: image feature matching: default to SVD-based solver.
- SDL\_Viewer: image feature matching: visualize transformed points.
- SDL\_Viewer: implement manual image feature selector.
- SDL\_Viewer: loading bar: decouple wave speed from frame rate.
- SDL\_Viewer: meshes: expose yaw, pitch, and roll angles.
- SDL\_Viewer: plot viewer: display ordinate and abscissa, if available.
- SDL\_Viewer: show loading animation.
- SDL\_Viewer: shrink script editor button width.
- SDL\_Viewer: speed up loading bar wave speed.
- Surface\_Meshes: add missing CGAL header.
- Time: explicitly include stringstream header.
- WarpImages: support resampling warped images.

## 202202

- Add AnyOf/FirstOf/AllOf/NoneOf operations or aliases.
- Added Eigen support to syntax checking script.
- Add Ignore operation.
- Add NotifyUser operation.
- Add ValidateTPlan example script.
- Dispatcher: lengthen pause time for error messages on Windows.
- Documentation: remove duplicated DICOM RT warnings.
- Harmonize control flow meta-operations.
- ModelIVIM: add example script.
- ModelIVIM: fix bi-exp model matrix coeff access bug.
- ModelIVIM: rewrite script filtering steps.
- Notifications: powershell: switch to blocking notifications.
- NotifyUser: assume powershell is always available.
- PFD: increase result buffer.
- README: update Zenodo DOI for previous release
- SDL\_Viewer: add rudimentary RTPLAN viewer.
- SDL\_Viewer: avoid drawing scale bar when changing colour map.
- SDL\_Viewer: avoid RTPLAN metadata window overlap.
- SDL\_Viewer: contouring: standardize contour metadata.
- SDL\_Viewer: default to wider table view.
- SDL\_Viewer: explicitly check coordinate map validity.
- Tray\_Notification: re-order powershell commands.
- Valgrind and TSAN bug fixes.
- ValidateTPlan: add is\_VMAT and collimator angle degeneracy checks.
- ValidateTPlan: add minimum jaw-defined field size check.

## 202201

- Add support for RTPLAN-based grouping.
- Add WIP TPlan checking operation.
- DroverDebug: provide more granular control over verbosity.
- ForEachDistinct: make N/A partition accessible.
- Partitioning: support partitioning tables.
- SDL\_Viewer: account for viewer aspect ratio.
- SDL\_Viewer: add keyword-based cell highlighting for tables.
- SDL\_Viewer: add minimal keyboard positioning for meshes.
- SDL\_Viewer: add toggleable face smoothing.
- SDL\_Viewer: flip vertical screen axis.
- SDL\_Viewer: force deprecation of pre-3.0 OpenGL functionality.
- SDL\_Viewer: more aggressively strip chars from GLSL version string.
- SDL\_Viewer: OpenGL: switch to shader-based rendering.

## 202112

- Add basic table operations (Delete, Copy, Generate).
- Added constructive solid geometry functionality using signed distance functions (CSG-SDF).
- Add ExportTables operation.
- Add MakeMeshes example script.
- Add minimal 'table' class.
- AnalyzeHistograms: use OS-dependent tmp dir.
- CSG: add poly chain shape.
- CSG-SDF: add 2D->3D extrusion operation.
- CSG-SDF: add extrusion example.
- CSG: SDF: add rotations and more Boolean ops.
- CSG-SDF: add text primitive.
- CSG: SDFs: add chamfer Boolean variants.
- CSG-SDF: support asymmetric SDFs.
- Expand CSG-SDF operation examples.
- Move README content to wiki.
- parse\_functions: added nested function support.
- Remove non-critical FUNCERRs in operations.
- SDL\_Viewer: always ensure mesh number is valid.
- SDL\_Viewer: enable mesh metadata viewer.
- SDL\_Viewer: fix table expansion ID issue.
- SDL\_Viewer: force reload of OpenGL meshes after script execution
- SDL\_Viewer: integrate table and table metadata editor
- Surface\_Meshes: support marching cubes with oracle functor.
- Tables: implement CSV write.

## 202111

- Added a position-based image sorter.
- Added a position-based image sorter.
- Added de-EQD2 standard script.
- Added ModifyParameters operation.
- Add LoadFilesInteractively, which uses recently-added dialog functionality.
- Add QueryUserInteractively operation.
- Add read support for (proprietary) XIM format images.
- Always prospectively rebuild Ygor on Debian.
- Avoid std::filesystem::absolute on Windows.
- CMake: replace large macro (stringified standard scripts) with generated file.
- ConvertContoursToMeshes: expose marching-cubes extractor.
- Dialogs: disable prospective pfd::kill().
- Dialogs: try to accept more Windows-like systems.
- ExtractRadiomicFeatures: use DCMA marching cubes for speed-up.
- FITS images: imbue minimal DICOM metadata in images.
- OrderImages: add spatial ordering option.
- OrderImages: add spatial ordering option.
- PFD: remove OS version check, which seemingly fails for no discernable reason?
- Replace large macro with generated file.
- Scripts: support quotation escapes.
- SDL\_Viewer: add native dialog support via PFD.
- SDL\_Viewer: disable optional parameters in stringified scripts.
- SDL\_Viewer: handle empty metadata tags.
- SDL\_Viewer: isolate dialogs into separate library.
- SDL\_Viewer: limit default script open dialog to script extensions.
- SDL\_Viewer: rely on select\_files defaults.
- SDL\_Viewer: simplify file loading.
- SDL\_Viewer: simplify script optional parameters.
- Standard\_Scripts: generator: disable verbose mode
- Windows: delay for longer when terminating abnormally.
- Windows: provide a new icon.

## 202110

- Add ConvertWarpToMeshes operation.
- Add MingW-specific winsock libs for Asio/Boost.
- Added ConvertImageToWarp to pair with ConvertWarpToImage.
- CI: GitHub: add aarch64 and x86\_64 static builds.
- ConvertWarpToMeshes: provide means to disregard rigid component.
- ConvertWarpTo\*: inject user-provided metadata.
- De-CGALify marching cubes implementation.
- Explicitly link imebrashim.
- Fallback to previous wstring offload when needed.
- Imbue converted warps with DICOM-style metadata.
- Imebra\_Shim: def reg reading: swap spatial dimensions.
- Img Reg: add an example of accessing images and voxels.
- Img Reg: added placeholder write\_to and apply\_to functions.
- Img Reg: support reading whole directories of FITS files.
- Musl: Fallback to an identity conversion if iconv does not support required conversion.
- Properly differentiate fv\_surface\_mesh and Polyhedron meshes.
- Documented current state of GUI intefaces via screenshots in GitHub issue.
- Roughed in field-based deformations.
- SDL\_Viewer: add 3D Gaussian brush.
- SDL\_Viewer: add custom mesh colouring with alpha support.
- SDL\_Viewer: add image auto-advance.
- SDL\_Viewer: add rudimentary lighting for meshes.
- SDL\_Viewer: cleanup OpenGL textures when re-assigning.
- SDL\_Viewer: encapsulate mesh loading and rendering in a class.
- SDL\_Viewer: fix clipping bug.
- SDL\_Viewer: honour minimizing the script window.
- SDL\_Viewer: maintain aspect ratio in model space.
- SDL\_Viewer: normalize soft brushes and add tanh{2D,3D}.
- SDL\_Viewer: release OpenGL resources while context valid.
- SDL\_Viewer: simplify placeholder script.
- SDL\_Viewer: support light mode.
- SDL\_Viewer: support mesh selection, rotation, and precession.
- SDL\_Viewer: tighten range for tanh brushes.
- SDL\_Viewer: embed raw opengl mesh in imgui window.
- Script: list exhaustive arguments in debug feedback.
- Scripts: map parent directory to category.
- Split point cloud and image registration working directories.
- Surface\_Meshes: accelerate marching cubes.
- Surface\_Meshes: face re-orientation: allow unorientable faces.
- Surface\_Meshes: implement orientation normalizer.
- Surface\_Meshes: marching cubes: track per-component orientation.
- Surface\_Meshes: use a faster vertex de-duplication method.
- Swap long int --> int64\_t to appease mingw64 ygor linking issue (and some arm archs).
- Support reading and visualizing deformation fields.
- Add minimal Affine registration DICOM read support.
- deformation\_field: conceal a spatial index.
- deformation\_field: implement basic apply\_to().

## 202109

- Add 'standard' scripts.
- Add an operation to add noise to an image array.
- Add script to build statically-linked DCMA binaries.
- Added extremely minimal terminal-based image viewer.
- Address upstream Boost bind placeholder de-escalation.
- AppImage: manually set rpath for binaries.
- Arm: also avoid clobbering user-provided mcpu and mtune.
- Arm: avoid clobbering user-provided march flag.
- CI: GitHub: WIP: add armv7 static build + test.
- CI: github: be more forceful with artifact sync.
- Common\_Plotting: when forking, use std::exit() rather than std::quick\_exit().
- Cross compiling: more clearly state which configurations work.
- Cross compiling: remove BUILD\_SHARED\_LIBS="ON" override.
- Cross-compiling: more tinkering to get armv8 to cross-compile with zig-cc or musl toolchain.
- DCMA: check that CWD is valid, else make it valid.
- DICOM loader: warn when modality cannot be assessed.
- Dispatcher: load command-line script operations before command-line operations.
- Docker: Alpine: add X and OpenGL-related packages.
- Docker: Alpine: add aarch64 and x86\_64 build\_bases.
- Docker: Alpine: add builders to complement build\_bases.
- Docker: Alpine: add note wrt mesa/OpenGL support.
- Docker: Alpine: avoid failing if unable to delete directory.
- Docker: Alpine: fix arm64 --> amd64 typo.
- Docker: Alpine: follow MXE build\_base and defer building DCMA.
- Docker: Alpine: make use of GSL and Eigen.
- Docker: CI: ensure patchelf is available.
- Docker: add Alpine armv7 container build\_base.
- GenerateSyntheticImages: provide UIDs that parse correctly.
- Metadata: bug fix: to\_seconds() string replacement.
- Metadata: generalize metadata evaluation and macro replacement.
- Metadata: implement WIP DICOM module-based metadata coalescers.
- Metadata: provide helper for key-value modifying operations.
- NormalizePixels: add PET SUVbw normalization.
- Remove vestigial getopts.h include.
- Scripts: bundle scripts during builds.
- SDL\_Viewer: WIP: support modifying image pixels.
- SDL\_Viewer: add 2D median filter brushes.
- SDL\_Viewer: add 3D median brushes.
- SDL\_Viewer: contouring: handle case with multiple overlapping images.
- SDL\_Viewer: display contour metadata as a tooltip.
- SDL\_Viewer: drawing: added full complement of 2D and 3D rigid/mean/median brushes.
- SDL\_Viewer: hoist image drawing from contouring.
- SDL\_Viewer: improve metadata handling for line samples.
- SDL\_Viewer: proactively set LIBGL\_ALWAYS\_SOFTWARE=1 within AppImages.
- SDL\_Viewer: provide a way to directly run scripts as actions.
- SDL\_Viewer: use bounding box to speed drawing.
- SDL\_Viewer: use imgui table for image metadata viewer.
- Terminal\_Viewer: add bare-bones shade glyphs display option.
- Terminal\_Viewer: add displays for non-ansi, non-unicode terminals.
- Terminal\_Viewer: allow user to override display settings.
- Terminal\_Viewer: disable use of terminal control codes for completely portable display option.

## 202108

- ModifyImageMetadata: support macro expansion and time extraction.
- GroupImages: add note about de-grouping.
- Address LGTM static analysis concerns.
- SYCL: switch to triSYCL.
- Operations: separate Drover updates from return/exit.
- Support loading scripts from files.
- Add control flow meta-operations for programming scripts.
- Provide conditional (if-then-else) control flow meta-operation.
- Debian: continue with buster (now oldstable) for now.
- Documentation: do not clobber inline paragraphs.
- Regex\_selectors: add compliment to 'numerous': 'fewest'.
- Regex\_Selectors: add undocumented image selector (numerous).
- GroupImages: provide automated partition key selection.
- ModelIVIM: support Siemens CSA b-values
- Imebra\_Shim: switch pixel extraction to metadata.
- Imebra\_Shim: added ad-hoc support Siemens private DICOM tag 'CSA2' binary blob decoding.
- Dispatcher: default to SDL\_Viewer.
- Dispatcher: added workaround for AppImageKit chdir() call.
- Replace boost::filesystem with std::filesystem.
- check\_syntax: document libraries needed for linkage.
- File\_Loader: verbosely report unloadable files.
- File\_Loader: attempt to resolve relative paths.
- File\_Loader: add a few more short-circuit extensions.
- File\_Loader: only FUNCINFO notice when loader is invoked.
- Load\_Files: bug fix: let user override extension filtering.
- Dispatcher: disable file checking logic.
- Scripts: check for shebang and provide compilation feedback to stdout.
- Script\_Loader: merely suggest non-expected arguments.
- SDL\_Viewer: expose abscissa for time courses.
- SDL\_Viewer: default to 'dt' metadata element for time courses.
- SDL\_Viewer: WIP time course viewing.
- SDL\_Viewer: display aliases in add-to=script menu.
- SDL\_Viewer: support saving generated contours.
- SDL\_Viewer: plots: give buttons unique IDs.
- SDL\_Viewer: plots: allow legend to be disabled.
- SDL\_Viewer: plots: more tooltip weidth tweaking.
- SDL\_Viewer: Plots: adjust column widths.
- SDL\_Viewer: attempt to widen plot hover metadata.
- SDL\_Viewer: Plots: view metadata and add on-the-fly normalization.
- SDL\_Viewer: plot selection bug fix.

## 202107

- Dispatcher: permit directory file command line arguments.
- Testing: report verbose results before summary.
- CI: provide verbose feedback for integration test failures.
- TAR loader: attempt to use file extensions from the archive.
- DICOM: add (ad-hoc) proprietary CSA header tag parser.
- check\_syntax: optionally check YAML files.
- DCMA-Dump: support printing beyond null bytes.
- File\_Loader: recursively find files when directories provided.
- File\_Loader: disable reprioritization when extension missing.
- File\_Loader: switch from constexpr lambda to regular function.
- File\_Loader: switch to priority-based file loading via file extension.
- Load\_Files: provide more insight into which loaders are being used.
- ContourWholeImages: only allocate space when creating contours.
- Scripts: accept up to 500 feedback notifications.
- Add single-thread worker FIFO queue class.
- ModelIVIM: use fuzzy key-value lookup for b-values.
- Docker: avoid unnecessary costly rebuilds (Debian stable).
- Add support for shellcheck-based checks.
- Provide better support for remote aarch64 compilation.
- CI: attempt to add armhf CI builds.
- CI: GitLab: use a loopback swapfile for all builders.
- Script\_Editor: make variable replacement exhaustion an error.
- Script\_Loader: support variable replacements.
- Script\_Editor: recursively parse operations and variables.
- Script\_Editor: imbue characters with basic parsing metadata.
- Script\_Editor: expand script validation feedback.
- Script\_Editor: validate parentheses and quotations.
- SDL\_Viewer: bug-fix: honour user image view setting.
- SDL\_Viewer: Script\_Editor: provide line numbers.
- SDL\_Viewer: added rudimentary script parser.
- SDL\_Viewer: make auto-scaler aware of multi-channel images.
- SDL\_Viewer: script editor: emphasize active script file better.
- SDL\_Viewer: continue to display previous image texture without locking.
- SDL\_Viewer: used timed mutex to skip locked data access.
- SDL\_Viewer: compartmentalize sub-operations.
- SDL\_Viewer: roughed-in worker offload thread.
- SDL\_Viewer: roughed-in script support.

## 202106

- CI: add binutils explicitly.
- CI: adjust rpath for aarch64 dynamic linked binaries and libraries.
- CI: mitigate AppImage naming differences on aarch64.
- CI: GitHub: account for chroot-nested AppImage.
- CI: altogether disable Wt for CI builds.
- CI: switch to CMake '-B' option.
- CI: more Wt compilation wrangling.
- CI: adjust Wt optimization level.
- CI: speed up Wt compilation.
- CI: GitHub: lengthen timeout for aarch64 AppImahe building.
- CI: AppImage: fallback to linuxdeploy for x86\_64.
- CI: permit longer aarch64 builds.
- CI: Debian: add script to make aarch64 chroot.
- AppImage: switch to appimagetool directly.
- CellularAutomata: added simplistic 'gravity' mode for voxel values.
- Added 2D cellular automata operation.
- ModelIVIM: Fixed bugs for Kurtosis model fitting
- ModelIVIM: utilize multiple model params as separate channels
- WIP: ModelIVIM: add bounds checks for debugging.
- WIP: ModelIVIM: Bayesian kurtosis model with noise floor (CSample)
- HighlightROIs: use r\*-tree for faster spatial lookups.
- HighlightROIs: rely on modified upstream Mutate\_Voxels() for contour overlap
- HighlightROIs: added integration test.
- HighlightROIs: expose receding-squares method
- HighlightROIs: add WIP marching-squares-inverse algorithm
- ContourViaThreshold: consistently handle contour orientation
- ContourViaThreshold: support marching-sqaure lower and upper thresholds
- ContourViaThreshold: make marching-squares handle boundaries
- ContourViaThreshold: add marching squares option.
- SDL\_Viewer: support 3D contour brushes.
- SDL\_Viewer: add snapping distance measuring tool.
- SDL\_Viewer: provide colour map scale bar.
- SDL\_Viewer: provide visual cue of contouring brush
- SDL\_Viewer: avoid double-counting overlapping line segments.
- SDL\_Viewer: support gapless contouring.
- SDL\_Viewer: added square rigid brush.
- SDL\_Viewer: add channel selector.
- SDL\_Viewer: support erosion and dilation (margins)
- SDL\_Viewer: contouring respects image aspect ratio
- SDL\_Viewer: use public ContourViaThreshold interface
- SDL\_Viewer: support multi-plotting
- SDL\_Viewer: contouring: notify user of contour deletion
- SDL\_Viewer: marching squares: switch to grid-based representation.
- SDL\_Viewer: contouring: implemented marching squares
- SDL\_Viewer: provide contouring fallback method.
- SDL\_Viewer: give user control of contouring resolution.

## 202105

- Added diffusion MR DICOM attributes.
- CI: GitLab: Fedora: workaround LD\_PRELOAD issues, explicitly preload libs, add gmp-c++ dependency.
- CI: remove Travis.ci.
- CI: upload artifacts to GitHub.
- Docker: added Ubuntu build\_base.
- ModelIVIM: account for extra voxel value.
- ModelIVIM: bug fix: log-transform MR intensities
- ModelIVIM: expose model selection flag.
- ModelIVIM: merge pull request from samplecm (biexponential model)
- Regex\_Selectors: add simplistic regex inversion mechanism.
- Regex\_Selectors: provide basic missing key selector.
- SDL\_Viewer: WIP asynchronous contours overlay
- SDL\_Viewer: WIP contours overlay
- SDL\_Viewer: WIP mask-based contouring.
- SDL\_Viewer: add image coordinates tooltip.
- SDL\_Viewer: add realtime row and column profile viewer
- SDL\_Viewer: add realtime-updating image metadata window.
- SDL\_Viewer: add simple linesample plots.
- SDL\_Viewer: add voxel intensity probe.
- SDL\_Viewer: allow contour line thickness adjustment, provide per-contour display toggle
- SDL\_Viewer: cache filesystem queries.
- SDL\_Viewer: disable default OpenGL word-alignment.
- SDL\_Viewer: expose window/level and colour maps.
- SDL\_Viewer: generate mipmaps and use nearest pixel sampling.
- SDL\_Viewer: improve file opener edge cases.
- SDL\_Viewer: load files asynchronously.
- SDL\_Viewer: protect against empty root paths.
- SDL\_Viewer: provide rudimentary file loader.
- SDL\_Viewer: report DICOM position.
- SDL\_Viewer: retain custom window and level
- SDL\_Viewer: revert mipmaps, keep nearest pixel sampling.
- SDL\_Viewer: support contour colour adjustment
- SDL\_Viewer: support mouse zoom and pan, zooming and panning images, keyboard image navigation
- SDL\_Viewer: support multiple file selections.
- SDL\_Viewer: support synchronized low-high and window-level.
- SDL\_Viewer: use shared\_mutexes for contour threads
- SDL\_Viewer: workaround wstring/wchar.
- Support remote AppImage builds.

## 202104

- SDL: provide explicit format strings.

## 202103

- Perfusion: implement a single-compartment model

## 202102

- SDL\_Viewer: make specific views toggleable.
- SDL\_Viewer: added basic operation documentation menu.
- Convert version macro to extern variable.
- CI: GitHub: support manual launch.
- Merge pull requests/branches from anthonyho.

## 202101

- Simple\_Meshing: accept disoriented contours.
- PurgeContours: expand functionality.
- Expose Drover partitioning backend.
- Merge pull requests/branches from mojanjz and ceringstrom
- Provide version information.
- CI: use GitHub actions to deploy CI to GitHub page.
- Add no-op operation.
- Bug fix: properly document Repeat operation.
- Support fuzzy operation names and explicit aliases.
- Clean up and similarity threshold
- Rearranged includes and fixed a lot of warning and added more parameters
- ContourViaThreshold: enable partial support without CGAL.
- Added DeleteContours operation.
- Avoid running tests when functionality unavailable.
- Add a non-trivial example (double-threaded screw).
- ConvertContoursToMeshes: contour selection bug fix.
- CGAL: use exact-exact kernel for boolean operations.
- CopyContours: rely on metadata update member.
- CopyContour: added missing contour duplication operation.
- WarpImages: test spatial mirroring.
- GenerateTransform: rely on upstream affine factories.

## 202012

- Enable rigid and affine registration implementation
- Improve compatibility with older Boost::Serialization versions.
- Support for Boost::Serialization 1.74.
- Support loading point clouds and surface meshes from both ASCII and binary PLY files.
- Added support for vertex normals (surface meshes, point clouds).
- Added basic support for vertex colours (surface meshes, point clouds).
- Partial migration from Travis CI to GitHub actions.
- Added explicit support surface mesh export in (ASCII+binary) PLY, OBJ, OFF, and (ASCII+binary) STL formats.

## 202011

- Support loading point clouds from OBJ and OFF files.
- Added ConvertMeshesToPoints operation with vertex extraction and random surface sampling.
- SYCL: added test file generation program, SCDI perfusion model minor bug fixes.
- Nix: overhaul of all nix files, slightly more idiomatic file hierarchy.
- SDL\_Viewer: make image array viewing optional, but still a WIP.
- CI: ensure all systems provide libOpenGL.so.
- Docker: added WIP debian:oldstable support. Might have to wait for next oldstable.

## 202010

- Reworked EQDx operation to also provide BED conversion, added tests.
- Simplified very old code (contours\_with\_meta class).
- Simplified ROI operation parameter selectors and documentation.
- Webserver: relocate config directory, simplify parameter selection when exmaples are exhaustive.
- Add SDL2 and Glew dependencies on all platforms.
- SFML\_Viewer: provide cross-platform partial replacements for Zenity dialogs.
- Explicator: provide built-in lexicon, default to most generic lexicon provided by Explicator (if available).
- ContourSimilarity: sync documentation with current implementation.
- Documentation: indicate version in online documentation.
- Added mock-up starting point for deformable registration algorithm development.

## 202009

- Added development log.
- Modernized use of 'new' for Contour\_Data and Image\_Data unique\_ptr/shared\_ptr allocations.
- Added support for nested operations and meta-operations, Repeat and ForEachDistinct.
  These can be used to partition data, analyze group-wise, or implement control flow at the level of operations.
- Nix: nix files reconfigured to use git master branches rather than commit pinning.

## 202008

- Successfully ported DCMA to Windows.
- CI: added MXE cross compilation and expose build artifacts.
- CI: expanded number of builds and test matrix, added nightly scheduled builds.

## 202007

- CI: updateCI scripts and AppImage creation, switched base image to Debian for greater stability.
- Docker: separated setup scripts from Dockerfiles for easier re-use.
- Docker: parameterized multi-part builds.
- Addressed some static analysis warnings (memcpy, localtime, if(false), std::bind, ...).
- CI: publish AppImage artifacts.
- CMake: better support for ARM builds.

## 202006

- ReduceNeighbourhood: support statistical standarization.
- ReduceNeighbourhood: support adaptive renormalization.
- CMake: easier support for sanitizer builds.
- TPS-RPM: report bending energy, support for alternative solver methods, support double-sided outlier handling,
  added tolerance-based Sinkhorn normalization for adaptive refinement, finalize user-interface.
- IO: normalize floating-point precision to maximum for exact round-trips.
- AnalyzeHistograms: fix conversion typo (cm^3^ to mm^3^).
- Added Transform3 as first-class object to support registrations.

## 202005

- Preliminary TPS-RPM implementation for deformable registration.
- Many small tweaks throughout code base (UID generation, debugging features, IO
- Added test files for meshing.
- Simple\_Meshing: force manifoldness at edges.
- Added Simple\_Meshing routine to quickly stitch together well-behaved contours.
  It is experimental, but has limited support for N-to-M contour connections, holes, and nested contours.
  Meshes produced this way are extremely unreliable, except in well-constrained situations!
- Support for explicit DVH Line\_Sample files.

## 202004

- Attempted WASM compilation, but SFML and few others are currently blocking such a port.
  Will likely have to move to SDL at some point, probably with a more functional interface.
- Added script to generate AppImages.
- Added minimal Nix scripts for building with Nix and integrating into NixOS.
- Overhauled entire histogram sub-system.
- Docker: added X-passthrough script for distributing full `DICOMautomaton` builds, including graphical parts.
- Added Linux live-cd distribution method, which provides full `DICOMautomaton` builds and all requirements.
  Also provides a script to download all sources for all packages. This distribution method is rock-solid, but
  fairly inconvenient. Best for archival or offline distribution.
- Support reading and writing of gzipped-TAR files.
- File\_Loader: support (uncompressed) TAR files.
- DICOMExportImagesAsCT: upstream class name change.
- Support writing TAR archives when exporting multiple files (e.g., CT images).

## 202003

- SimulateRadiograph: faster ray marching, smarter ray pruning, optimized code hot-paths, and truncate rays to bounding
  volumes.
- Upstream (Ygor) planar\_image\_adjacency change: switched from map to linear lookups for int-to-images, which will
  significantly speed-up ray-casting through rectilinearly voxelized geometries.
- Added basic treatment plan analysis routine. Basically just a shell to build on later (e.g., for automated plan
  checks).

## 202002

- Address excessive Docker build memory consumption.
- Added small collection of test files of various kinds, mostly for development and testing.
- Added support for reading STL files.
- Added TransformContours operation.
- Meshing: prune irrelevant candidate vertices (Marching Cubes) to speed-up large meshing operations.
- Added Fuzzing Docker container for easier fuzzing with AFL.
- EQDConvert: typo fix: conversions mistakenly always used a non-pinned conversion scheme.

## 202001

- Added documentation for fuzz testing, expanded Termux documentation.
- PointSeparation: reduce to constant memory usage.
- PointSeparation: report Hausdorff distance.
- Added operation to compute extreme distances between point clouds (PointSeparation).
- Improved linking, dependency tracking, and invalid flags when clang is used.
- Switched to C++17 to expand compiler compatibility (wrt std::experimental).

## 201912

- portable\_dcma: provide default lexicon for all wrapper scripts.
- Provide an embedded fallback font.
- PartitionContours: ensure sub-segment ordering when some are omitted.
- Added operation to partition ROIs (PartitionContours).
- SubsegmentContours: provide means of re-ordering cleaves.
- Extracted sub-segmentation operation from earlier segment+analyze operation.
- AnalyzeDoseVolumeHistograms: hoist macro expansion routine into Ygor for broader use.
- DeDuplicateImages: add simple means of de-duplicating image arrays.
- ContourBooleanOperations: ensure at least one contour has been selected.
- DCMA\_DICOM: Provide a custom DICOM writer.

## 201911

- Imebra: extract DICOM tag writing sub-routines.
- Optional external dependencies: support conditional compilation for major external dependencies.
  This will eventually be needed for increasing portability to other Linuxes, Windows, and WebAssembly.

## 201910

- Joint\_Pixel\_Sampler: rely on Mutate\_Voxels img\_refw.
- Joint\_Pixel\_Sampler: eliminate racy static logging.
- AlignPoints: provide implementation of exhaustive ICP.

## 201909

- AlignPoints: handle cases of 2D and 1D degeneracy for PCA alignment.
- ConvertContoursToPoints: provide an operation to extract vertices from contours as a point cloud.
- Added an operation for registration of point clouds (AlignPoints).
- XYZ loading: load each file as a separate point cloud.
- Moved point cloud implementation to Ygor.
- AnalyzeDoseVolumeHistograms: increased support for literals, sig. figs., and unrecognized constraints.
- Added support for Line\_Samples as first-class objects. This will be used for DVH handling and possibly some tabular
  data applications.

## 201908

- Provide a routine to deal with non-manifold meshes.
- EvaluateDoseVolumeHistograms: switch to more sophisticated backend.
- Supersampling: clean up metadata and intermediate images.
- Interpolate\_Image\_Slices: exploit rectilinearity to interpolate slices much more quickly.
- Experimental support for treatment plans.
- Docker: Added Void Linux Docker (musl) build. Note that this container still builds dynamic artifacts.
- Generalized compilation convenience scripts wrt which Linux distribution is being called and whether to delegate to
  Docker.
- Surface\_Meshes: specify image traversal order.
- SimulateRadiograph: WIP: added path integration, parameterized more image characteristics.

## 201907

- Added operation for exporting images in FITS format.
- Added operation for convolution, correlation, and pattern-matching with two images.
- Added support for reading DOSXYZnrc 3ddose files.
- Added operation for rigid mesh transformations.
- Added mesh processing routines (remeshing, subdivision, and simplification).
- Added preliminary support for first-class surface meshes.
- ContourSimilarity: revamped earlier quick implementation.
- Added runtime ELF patching for portable dcma binary.
- DumpImageSurfaceMeshes: added user-facing interpolative surface mesher.
- VolumetricNeighbourhoodSampler: clarify exception when images overlap.
- ContourViaThreshold: support non-binary marching cubes contouring.
- DrawGeometry: added solid sphere pattern.

## 201906

- Docker: Arch: use dynamic mirrorlist to avoid timeouts or reliance on any one mirror.
- PresentationImage: expose window and level configuration, permit not specifying any window/level.
- DumpROISurfaceMeshes: expose contour interprettation options.
- Expose contour inclusivity for mesh generation.
- DumpImageMeshes: simplify specification of output base and expose more options, including normalization.
- SFML\_Viewer: added explicit window/level entry, replace DB contour export with local export.
- DrawGeometry: added a wireframe cube shape.
- DumpImageMeshes: added image export as 3D OBJ and MTL models.
- SubtractImages: support more advanced image selection.
- BoostSerializeDrover: support export of inidividual components.

## 201905

- ReduceNeighbourhood: explicitly mention 'erosion' and 'dilation'. 
- ReduceNeighbourhood: add fixed isotropic spherical neighbourhoods.
- DetectGrid3D: WIP tweaks for improving image intensity-based detection of regular grids (RANSAC, reporting,
  Procrustes and algorithmic tweaks).
- FVPicketFence: hide parameters from picket fence web interface, use whitelist for exposing parameters.
- Added operation implementing Otsu thresholding.
- Add a volumetric spatial blur operation.
- ImprintImages: update image description.
- Added conservative filter for speckle reduction.
- DumpROISurfaceMeshes: generate more precise surfaces by default (CGAL).
- Ensure Gram-Schmidt orthogonalization retains unit vector magnitudes.
- ContourViaGeometry: pre-filter degenerate contours.
- Added operation for making volumetric contours from 3D shape primitives.
- SFML\_Viewer: added a simple DICOM distance measurement tool, factored common coordinate transformations.
- ScalePixels: added operation for scaling pixel/voxel intensities.
- VirtualGenerators: correct invalid PatientOrientation metadata.
- ModifyImageMetadata: allow modification of image spatial characteristics.
- GenerateSyntheticImages: bug fix: make metadata optional.
- Avoid null deference when no contours are loaded.
- GenerateSyntheticImages: bug fix: stringify metadata ints.
- LoadFiles: added operation for on-the-fly file loading.
- VolumetricSpatialDerivative: switch to pre-indexed trilinear interpolation.
- VolumetricSpatialDerivative: added non-maximum suppression for 3D derivatives.
- VolumetricSpatialDerivative: separate end-user-callable operation from implementation.
- DetectGrid3D: added support for PLY file writing.


## 201904

- Added operation to imprint images with point clouds.
- Added read support for ASCII XYZ point cloud file format.
- Added routine for more easily creating test images.
- Structs: enable point\_data copying for Drover class.
- DroverDebug: add image modality and min/max pixel values.
- DroverDebug: add point clouds to debug operation.
- WebServer: improve clarity of user-facing parameters.
- Add a customized reduced-memory CMake option.
- Hide shared template functions rather than expose them directly.
- ConvertPixelsToPoints: added routine for generating point clouds from images.
- Docker: base/archlinux -> archlinux/base.
- Add support for CI (Travis).
- Partial switch from std::regex() to more homogeneous Compile\_Regex().
- Recognize RTRECORD file signatures.
- Breaking change: removed Dose\_Arrays in favour of Image\_Arrays.
- SupersampleImageGrid: support for 3D supersampling.
- CompareVoxels: elaborate DTA and discrepancy computations.
- ComparePixels: support for multipe types of discrepancy.
- PresentationImage: allow specification of colour map.
- Added operation for image slice interpolation.

## 201903

- Added ability to mimic planes with planar contours.
- Explicitly indicate Drover serialization version.
- Break-out operations created for image detection functionality.
- Added script for easy syntax checking.
- Added operation for generating calibration curves.
- ReduceNeighbourhood: added NaN replacement variants.

## 201902

- ReduceNeighbourhood: support for logical filters (is\_min, is\_max).
- Added 3D voxel neighbourhood sampler.
- Added an isotropic remeshing routine.
- MarchingCubes: handle inconsistent contour orientation and spacing.
- Surface Dose Sampler: switch to Surface\_Meshes Marching Cubes implementation.
- Added a Marching Cubes surface generator.
- CompareImages: Migrate to Ygor image 3D adjacency routines.
- CompareImages: replace R\*-tree approach with grid-traversal.
- ThresholdImages: fix threshold inclusivity.
- Added basic man page render of reference guide.
- Added script to compile documentation to pdf and html.
- Added html and pdf versions of documentation.
- Added snapshot of documentation.
- Revamp runtime documentation.

## 201901

- Added rudimentary shell tab completion.
- RankPixels: added operation for ranking pixel values.
- Added script for bundling dispatcher binary and necessary shared libraries.
- Added routine for negating image pixel values.
- Boost Serialization: switch from binary to XML by default.
- DetectShapes3D: add WIP clustered RANSAC detector for planes and spheres.
- WebServer streamlining.
- DoseDecay: switch to planar inclusive catchment by default.
- DoseDecay: add a mask channel to avoid re-decay in multi-pass workflows.
- OrderImages: added a robust image sorter.
- GroupImages: added two logic-based grouping options.
- Added regex selector inversion.
- RTDOSE export: ensure UIDs do not have leading zeros.

## 201812

- ThresholdImages: reduce verbosity.
- Increased flexibility in image selection.
- Added image thresholding operation.
- Edge-detection: added non-maximum suppression to thin edges.

## 201811

- Added support for upstream vertex collapse contour simplification.
- Radiomics: add a simplified interface for web.
- Gracefully handle REG-modality DICOM files.

## 201810

- Surface Meshing: default to reasonable criteria to avoid ambiguities.
- Added contour simplification operation.
- Radioimics: added additional morphology features based on surface meshed ROIs.

## 201809

- Added perimeter-based contour features.
- Validation of select distribution-based statistical radiomic features.
- Limit number of concurrent build jobs to reduce memory usage.
- Honour TreatAsEmpty regex selector option.
- Added convenience script for remote building.
- Added 3D surface-based inset, outset, and shell operations.
- Static beam optimizer: switch from hypersphere-constrained parameters to direct hyperplane parameters, constrain beam
  weights to the coordinate n-sphere cartesian coordinate sector, improve transparency and reporting.
- Field-weight optimization: permit run-time parameter tweaking.

## 201808

- Switch docker base from Debian to Arch Linux to access newer upstream packages.
- Support subtraction of arbitrary images, if necessary.
- Support for surface mesh processing.

## 201807

- Switch from deprecated CGAL routine.
- Protect against incomplete data and when zero junctions detected.

## 201806

- Permit inexact decimation factors.
- Added partial support for Boolean negation.
- Expose EQD2Convert operation to web server.
- FVPicketFence: swap autocrop and pixel crop.
- AutoCrop: account for non-identity SAD-to-SID scaling.
- Protect against degenerate contours.
- Added dual operation to TrimROIDose.
- Only show hover info on parameter name column.
- Added image array subtraction operation.
- Added operation to delete image arrays.

## 201805

- Respect the tag element count.
- Added TemporalPositionIndex DICOM tag to dictionary.
- Renamed default PACS file store location.
- Support for non-default DB store locations.
- Added an operation for static beam optimization.
- Simplified ROI dose trimming operation.

## 201804

- Added multi-stage Picket Fence analysis operation.
- Added centroid-based selectors for ContourVote operation.
- Sub-segmentation: permit specification of bisection stopping parameters.
- More clearly documented OBJ/MTL contour export operation.
- Picket Fence: provide threshold boundaries as a contour, added contours for detected peak locations.
- Picket fence: added a summary file for quick inspection.
- Picket fence: project analysis results onto isoplane.
- Added image grouping operation.
- WebServer: layout updates using CSS animations, exposed operation descriptions.
- Translated in-source operation descriptions to runtime documentation functions.
- Plumbing for RTIMAGEs with z!=0 and dz!=0.
- Disallow explicit rescaling for RTDOSE exports.
- More clearly differentiate anonymization levels in the source.
- Default to more obviously phony date for missing/anonymized RTDOSEs.
- Feed Imebra correct FrameIncrementPointer value.
- Basic support for concealing parameters from user-facing interfaces.
- Added explicit dependencies for building.
- Added additional boost library dependencies.

## 201803

- Added temp file suffixes.
- Picket fence: explicitly colour junction contour lines to avoid colour clash, crop line contours to the provided
  image.
- Remove unnecessary termination points and irrelevant metadata lookups.
- Fix infinite loop when loading a bad DICOM image file.
- Fix setColor() SFML deprecations.
- Harden against missing contour metadata.
- Added a slew of named colours.
- Refined MLC model selection for picket fence analysis.
- Added contour metadata injection operation.
- Added simplistic screenshot extractor.
- Handle special case where all voxels are exactly zero.
- Added an operation to apply arbitrary calibration curves.
- Genericized HighlightROIs YgorFunctor.
- Mass header cleaning using include-what-you-use.
- Workaround for libc++'s std::optional::swap.
- Fixed UB in old Imebra code.
- Removed several unnecessary std::move (-Wpessimizing-move).
- Let the user opt-out of interactive plots.
- Added sanity check for images with volume.
- Added light-rad analysis edge visualization.
- Added basis spline peak-finding.
- Light-rad field coincidence analysis operation completed.
- Added autocrop operation using per-image metadata to crop.
- DICOM reading: added support for nested sequence items.
- Added free-form image cropping operation.
- Added percentile-based contour thresholding.
- Added multi-sequence metadata support for deeply nested DICOM tags.
- Split edge detection routine into two parts.
- Indicate invalid plots more clearly.
- Added an operation for selecting a contour via various criteria.

## 201802

- Completed percentage-based contouring.
- Added relative contour by threshold capabilities.
- Added a cyclic colour map for viewing gradient orientation angles.
- Support for 5x5 variants of the Sobel and Scharr derivative estimators.
- Reoriented gradient orientations for consistency.
- Added pixel sharpening operator.
- Imbued LogScale operation with an image selector.
- Exposed new upstream blur convolution-based estimators.
- SFML\_Viewer: indicate where the mouse is in sub-plots.
- Overhauled row, column profiles in the SFML-based viewer.
- Permit selection of first image in an array.
- Support additional upstream derivative estimators for edge detection.
- Auto-compile without CMakeCache invalidation.
- Removed audio from SFML-based viewer.
- Generalized partial derivative imaging functor for edge detection.
- Added WIP edge detection operation.
- Removed hard-coded gcc-isms.
- Added image metadata insertion operation.
- Added contour simplification pass.
- Permit display of contours that extend beyond image bounds.
- Added metadata to generated contours.
- Many clang-tidy modernizations.
- Patched in-tree Imebra to rid auto\_ptrs.
- Added a new, standalone DVH routine.
- Migrated to libWt v4.
- Updated CGAL header name changes.

## 201712

- BED: added a longer description for an alpha/beta parameter.
- Removed BED from TCP/NTCP evaluation operations.

## 201711

- Added explicit EQD2 conversion operation.
- Added image sampling routine to the SFML\_Viewer.
- Protect against empty dose arrays.
- Added UserComment column to all recent evaluation operations.
- Avoid NaN contamination in LKB NTCP model.
- Exposed dose melding operation.
- Webserver displays generated CSV files.
- Added mimetypes to file OptArgDocs.
- Added a from-scratch window update routine.
- Added a routine to read standard (uint8\_t-typed) FITS images.
- Added operation for reporting dose-volume statistics.
- Changed behaviour for missing ROI NormalizedROINames in Boolean operation.
- Added support for new YgorImages voxel iteration wrapper routine.
- WIP implementation for direct contour growth.
- Added a routine for explicit contour seaming.
- Webserver: added support for multi-pass operations.
- Added a 'paranoia' mode for dose export.
- Added a virtual dose data generator.
- Added paranoid mode for RTDOSE export.
- Bug fix: erroneously used hypothetical course 2 fractionation for course 1 BED conversion.

## 201710

- Implemented a NTCP models operation.
- Added the Fenwick TCP model.
- Added a contour-based Boolean function operation using CGAL.
- Added CMake-sanctioned pthreads flags.
- Added image --> dose conversion routine.
- Added two temporal dose decay models for re-irradiation dose estimates.
- Added utility file for dose <--> BED conversion.
- Recognize but skip over RTPLAN files and emit warning.

## 201708

- Strip executables and shared libraryies for Debian packages to reduce excessive binary sizes.
- Improved support for foreign FITS files.
- Added option for voxel inclusivity tweaking.
- Bump from c++14 --> c++17.
- Capped dose\_scaling.
- Dead code elimination.
- Completed simplistic, constrained RTDOSE export routine.

## 201707

- Extended functionality of Highlight images functor and created standalone operation for it.
- Added gnuplot optional dependency.
- Added font dependencies.

## 201704

- Clarified that YgorClustering is a header-only library dependency.
- Cleaned up PKGBUILD.
- Removed some auxiliary analysis files.
- Small changes for segmentation visualization.

## 201703

- Added a backup font found on a Debian installation.
- Added Debian packaging scripts (via CPack).
- Added ROI exclusion capability.
- Added pixel stats reporting for the working contour ROI.

## 201702

- Fixed race condition.
- Made ContourWholeImages operation tolerant of spatially overlapping images.
- Added whole-image ROI generation.
- Added a custom colour map.
- Added on-the-fly colour mapping.
- Toned down NaN colouring.
- Added empty image\_array pruning operation.
- Added a no-op option for easier script tweaking.
- Use contour colour to convey contour orientation.
- Added rudimentary contour-via-pixel-threshold operation.
- Added SNR computations for comparison purposes.

## 201701

- Fully pruned CSVTools dependency.
- Removed old, unused code causing unspecified custom library dependencies.
- Added output file options for surface-based ray casting operation.
- Added safer internal-to-ROI check for surface seed point.
- Exposed surface-based ray casting operation options to the user.
- Added trilinear interpolation to surface-based ray casting computation.
- Surface-based ray casting: only consider a single ray intersection.
- Made pacs\_ingress safer to use on duplicate files.
- Added a rudimentary pop-out realtime plotting window.

## 201612

- Additional ROI topological/morphological data in dumps.
- Disabled non-critical computations in surface mesh dump operation.

## 201611

- Added a voxel stats dump operation.
- IVIM/ADC tweaks.
- Operationalized many existing sub-operations.

## 201610

- Added sub-segment area logging for sub-segmentation routine.

## 201609

- Provided an option to forgo dumping of voxel distributions.
- Moved 'usemtl' specification prior to vertices to accomodate evrtex colouring.
- Converted ROI contour dumping to export Wavefront OBJ files.
- Added a routine for dumping ROI contours.
- Purged old CSVTools, Demarcator, and Distinguisher references from modern executables.
- Added supersampling and blurring to raster grid surface map.
- Added a standalone FITS file loader for easier viewing.
- Decoupled grid-based ray casting surface raster grid voxel size from ray source/detector grid size.
- Added an ROI-based cropping routine. Added to van Luijk script.
- Added supersampling pre-processing step to sub-segmentation script.
- Separated bisection and sub-segmentation to more easily switch sub-segmentation schedule.
- Improved bisection reporting and added a script for viewing multiple sub-segmentations.
- Improved stewardship of file-based loaders.
- Added additional sub-segmentation to van Luijk operation.
- Added a boost.thread and asio based thread pool to improve grid-based ray casting throughput.

## 201608

- Converted grid-based surface mask construction to use simple parallel for-loop.
- Converted ray-casting to use simple parallel for-loop.
- Separated surface mask grid creation from ray casting source+detector creation.
- Added a routine for generating a surface mask (and interior mask) over an image grid.
- Made 'van Luijk' subsegmentation selection extents less sensitive to numerical issues.
- Reverted PACS\_Loader to load dose files as Dose\_Arrays, not Image\_Arrays.
- Added basic DICOM file loader based on PACS loader.
- Added a lexicon building operation.
- Fixed bug in which clustered surface mesh facets' clusters were not reported correctly.

## 201607

- Added runtime parameters to control fast, approximate Chebyshev multiplication.
- Updated post-computation model evaluation for 1C2I model Reduced3Param.
- Added CXX\_FLAGS needed for CGAL (-frounding-math) and others to improve compiler optimization.
- Another hierarchial rename. Added Reduced3Param variant of 1C2I model (no gradient yet!).
- Hoisted a deserialization outside of a loop.
- Added a post-modeling parameter dumping routine.
- Added a Levenberg-Marquardt-based direct linear interpolation variant of the 1Compartment2Input\_5Param model.
- Exposed B-spline and Chebyshev coefficient totals as runtime parameters for level-of-detail adjustment.
- Added B-spline interpolation for AIF and VIF approximation.
- Basic support for storing model state with parameter maps.
- Streamlined pre-pharmacokinetic modeling AIF & VIF plotting.
- Factored some similar plotting routines together.
- Added pre-pharmacokinetic modeling AIF and VIF plot popup.
- Added runtime ROI selection for Cheby-based pharmacokinetic modeling.
- Added runtime plotting specification to pharmacokinetic modeling operation.
- Clean-up of GSL modeling routine.
- Completed 5Param Chebyshev Least-Squares modeling via Levenberg-Marquardt in GSL.
- Improved linear (non-Chebyshev) perfusion model optimization stopping criteria.
- Wrapped plotting calls with fork() to improve stability.
- Added another optimization criteria.
- Removed possibly misleading vestigial code.
- Added an ROI-based slice selector for pruning image slices.
- Made example values more explicit for disabling pre-analysis decimation.

## 201606

- DVSM: updated minetest skeleton location.

## 201605

- Added a dry-run mode for the ingress tool.
- Migrated CGAL code to CGAL>=4.8, addressing breaking API changes.

## 201604

- Decoupled contour resolution from image pixel size, fixed is-cursor-in-contour bug.
- Added dependencies to the PKGBUILD.

## 201603

- Added support for computing centroids for CGAL surface mesh polyhedra.
- Added per-ROI dose info dumping.
- Changed regex type from grep to extended POSIX throughout code base.
- Attempted to improve surface meshes by emulating planar contour extrusion with point set techniques.
- Added CGAL Scale-Space Surface Reconstruction operation.
- Added standalone operations for Max-Min pixel filtering, ROI time course plotting, and image copying.
- Added workaround for reading NaN and +-Inf in Boost.Serialization text and xml formats.
- Completed support for Boost.Serialization of the Drover class.
- Replaced Protobufs with Boost.Serialization.
- Created stubs for serialization, separated into a new file.
- Renamed 'Analyses' to 'Operations' where applicable to be more accurate.
- Added a default, reasonable .clang-format file.
- Major refactoring to decouple invocation handling, file loading, analysis dispatch, and analysis.
- Removed rarely-used but pervasive verbosity mechanism using awkward global state.
- Added support for image modality 3D rotation.
- Added explicit image reordering after modeling for improved viewing.
- Applied tweaks from 3-parameter testing to the 5-parameter perfusion modeling routine.
- Partially converted many image processing tasks to parallel execution.
- Added a work-around for recurring inconvenience regarding DICOM pixel mapping.
- Optimizer tweaking and transparency improvements.

## 201602

- Added plotting capabilities to perfusion modeled pixels for in-situ inspection.
- Incorporated pixel decimation into the CT liver perfusion modeling routine.
- Added pharmacokinetic modeling library sources.
- Added pixel decimation to do block-wise spatial pixel aggregation.
- Bug fix. Supersampled pixel 3D offset was not properly set.
- Split (some) pharmacokinetic modeling into separate library.
- Removed vestigial files from years ago.
- Added a more focused metadata dumper for image registration and non-DICOM image viewers.

## 201601

- Added ability to call a modified Minetest for contouring purposes ("DVSM")
- Tweaked orthogonal view defaults; added filters for new data; added an executable.

## 201511

- Simplified YgorImage\_Functor image description and window centre/width updating and Liver Perfusion map generation.
- Added support for custom display window centre and level.
- Added a proper orthogonal slice functor. Required modifications in Ygor.
- Implementation error: incorrect integral manipulation fixed.
- Migrated from hand-made Makefiles to CMake.
- Liver perfusion functionality enhancements.
- Migration to git.

