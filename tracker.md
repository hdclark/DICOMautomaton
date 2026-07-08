# Ygor Serialization Migration Tracker

## Preparation

- [x] Update or pin Ygor to a revision that provides `YgorIOXMLSerialization.h`, `YgorMathIOSerialization.h`, `YgorImagesIOSerialization.h`, `YgorMathChebyshevIOSerialization.h`, and `YgorIOgzip.h`.
- [x] Confirm DICOMautomaton configures against that Ygor revision before changing local serialization code.
- [x] Record whether legacy Boost archives must be converted externally before final Boost.Serialization removal. Legacy Boost archives must be converted before final Boost.Serialization removal; the final no-Boost path will not read them directly.

## Core Serialization

- [x] Add `src/StructsIOSerialization.h` under `namespace ygor::serialization`.
- [x] Port `Contour_Data` serialization.
- [x] Port `Image_Array` serialization and preserve schema handling for old filename/bits fields where applicable.
- [x] Port `Point_Cloud` serialization.
- [x] Port `Surface_Mesh` serialization.
- [x] Port `Static_Machine_State` serialization.
- [x] Port `Dynamic_Machine_State` serialization.
- [x] Port `RTPlan` serialization.
- [x] Port `Line_Sample` serialization.
- [x] Port `Drover` serialization with an explicit schema/version field.
- [x] Add local `std::shared_ptr<T>` serialization support or explicit pointee handling.
- [x] Replace old Ygor Boost adapter includes with `Ygor*IOSerialization.h` includes.

## Common API

- [x] Add `src/Common_Serialization.h`.
- [x] Add `src/Common_Serialization.cc`.
- [x] Implement default `Common_Serialize_Drover` as gzip-compressed Ygor XML.
- [x] Implement `Common_Deserialize_Drover` for gzip-compressed Ygor XML.
- [x] Implement uncompressed Ygor XML read/write if retained.
- [x] Remove binary and simple text helpers, or replace them only if Ygor provides equivalent archives.
- [x] Port `Serialize`/`Deserialize` for `KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters`.
- [x] Port `Serialize`/`Deserialize` for `KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters`.
- [x] Port `Serialize`/`Deserialize` for `KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters`.

## File Loader

- [x] Rename `Boost_Serialization_File_Loader.h` to `Serialization_File_Loader.h` or `Ygor_Serialization_File_Loader.h`.
- [x] Rename `Boost_Serialization_File_Loader.cc` to match the new header.
- [x] Rename `Load_From_Boost_Serialization_Files` to a non-Boost name.
- [x] Update `src/File_Loader.cc` include.
- [x] Update loader comments and warning text.
- [x] Re-evaluate serialized archive extensions and loader priority. Kept the existing early priority and overlapping extensions so serialized Drover archives are attempted before generic XML/text loaders.
- [x] Add a magic/header check for new archives if false-positive parse attempts become costly. No magic/header check was added because the current deserializer fails closed and the plan makes this conditional on observed parse cost.

## Operations and Converter

- [x] Rename `src/Operations/BoostSerializeDrover.h` to `SerializeDrover.h` or `YgorSerializeDrover.h`.
- [x] Rename `src/Operations/BoostSerializeDrover.cc` to match.
- [x] Rename operation doc/function symbols away from `Boost`.
- [x] Update operation default filename.
- [x] Preserve component selection behavior.
- [x] Decide whether to keep a deprecated `BoostSerializeDrover` alias with no Boost.Serialization implementation. No deprecated alias was retained because the migration plan prefers removing public Boost-named serialization surfaces unless explicitly required.
- [x] Replace or remove `src/Boost_Serialization_Archive_Converter.cc`. Removed the obsolete Boost archive converter because the Ygor path supports only XML and gzip XML.
- [x] If retained, rename the converter and restrict options to supported Ygor formats. Not retained; no converter options remain.

## Build System

- [x] Remove `serialization` from root `find_package(Boost COMPONENTS ...)`.
- [x] Remove `Boost::serialization` from all target link lists.
- [x] Remove serialization-specific `Boost::iostreams` linkage after switching to `YgorIOgzip.h`, unless another target still needs it. `Boost::iostreams` remains linked because non-serialization TAR and DICOM export code still uses it.
- [x] Rename `Common_Boost_Serialization_obj` in `src/CMakeLists.txt`.
- [x] Rename `Boost_Serialization_File_Loader_obj` in `src/CMakeLists.txt`.
- [x] Update all object target references in executable/library target source lists.
- [x] Update `src/Operations/CMakeLists.txt` for the renamed operation source.
- [x] Update converter target source/name or delete the converter target.

## Tests

- [x] Add round-trip test for an empty `Drover`.
- [x] Add round-trip test for contours.
- [x] Add round-trip test for image arrays.
- [x] Add round-trip test for point clouds.
- [x] Add round-trip test for surface meshes.
- [x] Add round-trip test for RT plans.
- [x] Add round-trip test for line samples. Verified with `cmake --build llm_build` and `llm_build/bin/dicomautomaton_dispatcher -t`.
- [x] Add gzip XML file round-trip test through `Common_Serialize_Drover` and `Common_Deserialize_Drover`.
- [x] Add loader test that consumes a Ygor serialized file. Verified with `cmake --build llm_build --target dicomautomaton_dispatcher` and `llm_build/bin/dicomautomaton_dispatcher -t`.
- [x] Add loader test that leaves a non-serialized file unconsumed. Verified with `cmake --build llm_build --target dicomautomaton_dispatcher` and `llm_build/bin/dicomautomaton_dispatcher -t`.
- [x] Add kinetic-model parameter round-trip tests under `DCMA_USE_GNU_GSL`. Verified by compiling `Serialization_Tests_obj` in `/tmp/opencode/dcma_gsl_build` with `WITH_GNU_GSL=ON` and running standalone GSL kinetic serialization round trips using the production `Serialize`/`Deserialize` APIs.
- [x] Add NaN/Inf round-trip coverage. Verified by standalone GSL kinetic serialization round trips covering NaN, +Inf, and -Inf, and by the normal dispatcher doctest suite.
- [x] Add operation-level test for component selection. Verified with `cmake --build llm_build --target dicomautomaton_dispatcher` and `llm_build/bin/dicomautomaton_dispatcher -t`.
- [x] Add converter tests if the converter remains. Not applicable because the converter was removed earlier in the migration.

## Documentation

- [x] Update `documentation/reference_guide.md` examples mentioning `BoostSerializeDrover`.
- [x] Update the archive converter documentation or remove it.
- [x] Update generated operation reference text.
- [x] Update dependency documentation to remove active Boost.Serialization requirements. No active dependency documentation listed Boost.Serialization after the CMake cleanup.
- [x] Add a migration note explaining the new Ygor XML/gzip archive format and legacy Boost archive status.

## Cleanup and Verification

- [x] Delete old Boost serialization source/header files after replacements are wired.
- [x] Search for `<boost/archive` and remove all matches.
- [x] Search for `<boost/serialization` and remove all matches.
- [x] Search for `boost::archive` and remove all matches.
- [x] Search for `boost::serialization` and remove all matches.
- [x] Search for `BOOST_CLASS_VERSION` and remove all matches.
- [x] Search for `Common_Boost_Serialization`, `StructsIOBoostSerialization`, `Boost_Serialization_File_Loader`, and `Boost_Serialization_Archive_Converter` and remove or update all active references.
- [x] Run a fresh CMake configure.
- [x] Run a full build.
- [x] Run the project test suite. `ctest --test-dir llm_build --output-on-failure` completed with no registered tests.
- [x] Re-run the forbidden-reference searches after tests/build generation.
