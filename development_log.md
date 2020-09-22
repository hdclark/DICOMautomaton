
# Timeline

Below is a simplified timeline of `DICOMautomaton` development, broken down by month.


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

