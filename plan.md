# Boost.Serialization to Ygor Serialization Migration Plan

## Scope

Migrate DICOMautomaton's archive serialization from Boost.Serialization to Ygor's 2026 serialization stack while preserving the existing high-level behavior where practical:

- Serialize and deserialize `Drover` state for save/resume workflows.
- Load serialized `Drover` files through the generic file loader pipeline.
- Preserve the `BoostSerializeDrover` operation's user-facing purpose, with a rename/deprecation decision documented before implementation.
- Serialize pharmacokinetic model parameter structs used under `DCMA_USE_GNU_GSL`.
- Keep gzip-compressed text/XML as the default portable archive format.
- Remove all direct Boost.Serialization dependencies, includes, archive converter behavior, documentation, and CMake linkage.

This migration does not remove Boost from the whole project. Boost components still used elsewhere should remain, but the `Boost::serialization` component and all `boost/archive` or `boost/serialization` code should disappear.

## Current Dependency Surface

The current Boost.Serialization surface is concentrated in these files:

- `src/Common_Boost_Serialization.{h,cc}`: public Drover read/write API, format-specific archive writers, fallback reader that tries gzipped XML/text/binary and uncompressed XML/text/binary, and string serialization for kinetic-model state.
- `src/StructsIOBoostSerialization.h`: Boost adapters and versioning for `Contour_Data`, `Image_Array`, `Point_Cloud`, `Surface_Mesh`, `Static_Machine_State`, `Dynamic_Machine_State`, `RTPlan`, `Line_Sample`, and `Drover`.
- `src/Boost_Serialization_File_Loader.{h,cc}`: generic loader hook that tries to deserialize each candidate file as a serialized `Drover`.
- `src/File_Loader.cc`: includes and registers the Boost serialization loader early in the loader priority list.
- `src/Boost_Serialization_Archive_Converter.cc`: command-line converter between Boost archive encodings.
- `src/Operations/BoostSerializeDrover.{h,cc}`: operation that writes selected Drover components through `Common_Boost_Serialize_Drover`.
- `src/KineticModel_1Compartment2Input_*_Common.h`: Boost adapters for parameter structs and includes of old Ygor Boost adapters.
- Build and docs: root `CMakeLists.txt`, `src/CMakeLists.txt`, `src/Operations/CMakeLists.txt`, `documentation/reference_guide.md`, generated/reference operation docs, and development notes.

The upstream Ygor replacement currently provides:

- `YgorIOXMLSerialization.h`: `ygor::serialization::xml_oarchive`, `xml_iarchive`, `make_nvp`, scalar/STL container support, field names, and explicit NaN/Inf handling.
- `YgorMathIOSerialization.h`: serializers for `vec2`, `vec3`, `line`, `line_segment`, `plane`, contours, surface meshes, point sets, regression results, and `samples_1D`.
- `YgorImagesIOSerialization.h`: serializers for `planar_image` and `planar_image_collection`.
- `YgorMathChebyshevIOSerialization.h`: XML archive serializers for `cheby_approx`.
- `YgorIOgzip.h`: Boost-free `ygor::io::gzip_ostream` and `gzip_istream` wrappers.
- `YgorSerialize.h`: low-level `SERIALIZE::Put/Get` buffer primitives. These are not a direct replacement for named archive I/O and should only be used if the XML archive helpers are insufficient.

The installed headers in this environment are stale and still include old `Ygor*IOBoostSerialization.h` headers. Update or pin Ygor to a revision that exports the new `*IOSerialization.h` headers before implementation.

## Target Design

Create a DICOMautomaton-local Ygor serialization layer with names that describe the new implementation:

- `src/Common_Serialization.{h,cc}` replaces `Common_Boost_Serialization.{h,cc}`.
- `src/StructsIOSerialization.h` replaces `StructsIOBoostSerialization.h`.
- `src/Serialization_File_Loader.{h,cc}` replaces `Boost_Serialization_File_Loader.{h,cc}`.
- `src/Serialization_Archive_Converter.cc` replaces `Boost_Serialization_Archive_Converter.cc`, or the converter is removed if it no longer has multiple archive formats to convert between.
- `src/Operations/SerializeDrover.{h,cc}` replaces `BoostSerializeDrover.{h,cc}`.

Prefer a small compatibility surface during implementation only. Do not keep public `Common_Boost_*` names after the migration unless the project maintainers explicitly require old external APIs or scripts to keep working.

Default output should be gzip-compressed Ygor XML using `ygor::io::gzip_ostream` plus `ygor::serialization::xml_oarchive`. The reader should support the new gzipped and uncompressed Ygor XML formats. Legacy Boost archive reading is optional only as a temporary, separately guarded bridge; the final requested state is no Boost.Serialization references.

## Adapter Work

Implement local serializers in `StructsIOSerialization.h` under `namespace ygor::serialization`:

- Port each Boost adapter from `boost::serialization::serialize(Archive&, T&, version)` to `ygor::serialization::serialize(Archive&, T&)`.
- Replace `boost::serialization::make_nvp` with `make_nvp`.
- Preserve field names exactly where possible: `dicom_data`, `imagecoll`, `contour_data`, `image_data`, `point_data`, `smesh_data`, `rtplan_data`, `lsamp_data`, kinetic-model field names, and RT plan member names.
- Replace `BOOST_CLASS_VERSION` with explicit archive fields, for example `schema_version` on `Drover` and versioned child structs. Read old Ygor versions by branching on the serialized schema value.
- Preserve known historical version behavior in the new format: `Image_Array` version 0 placeholders, `Drover` versions 1 through 3, and kinetic parameter version 1 fields.
- Add Ygor serializers for `std::shared_ptr<T>` or avoid pointer serialization by writing pointee presence markers and values in the owning DICOMautomaton structs. Ygor's current XML archive supports STL containers but not `std::shared_ptr` directly.
- Include upstream `YgorMathIOSerialization.h`, `YgorImagesIOSerialization.h`, and `YgorMathChebyshevIOSerialization.h` instead of old `Ygor*IOBoostSerialization.h` headers.

For `std::shared_ptr<T>` support, prefer an explicit representation:

- Write a `has_value` boolean before each pointee.
- If present, serialize the pointee as `value`.
- On read, reset the pointer when absent and allocate `std::make_shared<T>()` before reading when present.
- Do not attempt to preserve Boost's pointer identity tracking unless DICOMautomaton has a concrete aliasing requirement. Current Drover export copies shared pointers superficially before archive writing, but serialized data is expected to reconstruct equivalent values, not pointer graph identity.

## Common Serialization API

In `Common_Serialization.{h,cc}`:

- Provide `Common_Serialize_Drover(const Drover&, std::filesystem::path)` and `Common_Deserialize_Drover(Drover&, const std::filesystem::path&)`.
- Provide explicit helpers for the retained formats only, likely `Common_Serialize_Drover_to_Gzip_XML` and `Common_Serialize_Drover_to_XML`.
- Remove binary and simple-text archive helpers unless Ygor has equivalent archives. If a converter is retained, it should convert only supported Ygor formats.
- Use `std::ofstream`/`std::ifstream` with `ygor::io::gzip_ostream`/`gzip_istream` for `.gz` paths or explicit gzip helpers.
- Use `ygor::serialization::xml_oarchive` and `xml_iarchive` with `make_nvp("dicom_data", drover)`.
- Keep zero-length and unreadable-file guards from the existing deserializer.
- Keep exception-to-`false` behavior for loader compatibility.
- Rely on Ygor's floating-point facet for NaN and Inf instead of Boost.Math nonfinite facets.

For kinetic-model state serialization:

- Keep the existing `Serialize(...) -> std::string` and `Deserialize(...) -> bool` API unless callers are renamed in the same change.
- Replace text archives with Ygor XML archives over `std::stringstream`.
- Port the parameter struct serializers into `namespace ygor::serialization` and include the new Ygor serializers.
- Add round-trip tests with finite, NaN, and Inf values.

## File Loader and Operation Changes

Rename and update loader files:

- `Load_From_Boost_Serialization_Files` becomes `Load_From_Serialization_Files` or `Load_From_Ygor_Serialization_Files`.
- Update `File_Loader.cc` include, loader comment, warning text, and extension list.
- Keep serialized archive loading early in the loader order because `.gz`, `.xml`, and `.txt` overlap with other loaders.
- Consider adding a cheap magic/header prefix to new Ygor archives to reduce false-positive parse attempts. If added, write it before the top-level archive and check it in the loader.

Rename/update the operation:

- Prefer operation name `SerializeDrover` or `YgorSerializeDrover`.
- Update docs, generated reference text, examples, and default filename from `/tmp/boost_serialized_drover.xml.gz` to `/tmp/serialized_drover.xml.gz` or `/tmp/ygor_serialized_drover.xml.gz`.
- Keep the component-selection behavior unchanged.
- Decide whether to keep `BoostSerializeDrover` as a user-facing alias. If kept, document it as deprecated and ensure it does not contain Boost.Serialization code or docs claims.

Archive converter:

- If only gzipped and uncompressed Ygor XML are supported, replace the converter with a narrower gzip/unzip Ygor archive converter or remove the binary/text conversion options.
- Update examples and validation to reject removed `binary`, `gzip-binary`, `txt`, and `gzip-txt` options.
- Rename the target and executable to avoid `Boost_Serialization` in file and target names.

## Build Changes

Root `CMakeLists.txt`:

- Remove `serialization` from `find_package(Boost COMPONENTS ...)` once no target uses it.
- Keep `Boost::iostreams` only if other code still needs it; Ygor serialization gzip should use `YgorIOgzip.h` and not Boost.Iostreams.
- Re-run a full configure to confirm imported target lists and package-manager docs still match real requirements.

`src/CMakeLists.txt`:

- Rename object targets from `Common_Boost_Serialization_obj` and `Boost_Serialization_File_Loader_obj` to new names.
- Replace source/header filenames in object targets and target object lists.
- Replace `Boost_Serialization_Archive_Converter.cc` executable target with the new converter or remove it.
- Remove `Boost::serialization` from all `target_link_libraries` blocks.
- Remove `Boost::iostreams` from serialization-related linkage if no other linked object needs it.

`src/Operations/CMakeLists.txt`:

- Replace `BoostSerializeDrover.cc` with the renamed operation source, or keep the filename only temporarily during the transition.

## Documentation Changes

Update at least:

- `documentation/reference_guide.md` operation examples and archive converter section.
- Any generated operation docs or standard guides that mention `BoostSerializeDrover` or Boost.Serialization.
- `development_log.md` only if adding a new migration note; do not rewrite historical entries.
- Package/dependency documentation if it lists Boost.Serialization as required.

Docs should clearly state:

- New archives are Ygor XML, optionally gzip-compressed.
- Legacy Boost archives are not supported after the final migration unless a separate converter build is kept outside the no-Boost.Serialization code path.
- Binary Boost archives were not portable and are intentionally not reproduced unless Ygor adds a binary archive with defined compatibility guarantees.

## Testing Strategy

Add focused tests before broad cleanup:

- Unit or doctest round trip for `Drover` with each component type: contours, image array, point cloud, surface mesh, RT plan, and line samples.
- Round trip for empty `Drover` and partially populated `Drover`.
- Round trip through `Common_Serialize_Drover` and `Common_Deserialize_Drover` using gzipped XML and uncompressed XML.
- Loader test that consumes a serialized file and leaves non-serialized files untouched.
- Kinetic-model state round trips under `DCMA_USE_GNU_GSL`, including version-1 fields and NaN/Inf values.
- CLI/operation integration test for `SerializeDrover` component selection.
- Converter test only if the converter remains.

Verification commands should include a fresh CMake configure, a full build, and the project's doctest/test target. Also run a final repository-wide search for forbidden terms.

## Migration Sequence

1. Update the Ygor dependency to a revision containing `YgorIOXMLSerialization.h`, `YgorMathIOSerialization.h`, `YgorImagesIOSerialization.h`, `YgorMathChebyshevIOSerialization.h`, and `YgorIOgzip.h`.
2. Add `StructsIOSerialization.h` with DICOMautomaton serializers and pointer helpers.
3. Add `Common_Serialization.{h,cc}` and implement Ygor XML/gzip Drover and kinetic-model serialization.
4. Add round-trip tests for the new common serialization layer.
5. Rename/update the file loader and wire it into `File_Loader.cc`.
6. Rename/update the Drover serialization operation and its documentation.
7. Replace or remove the archive converter.
8. Update CMake targets and remove `Boost::serialization` linkage.
9. Remove old Boost serialization files and includes.
10. Update docs and generated references.
11. Run configure, build, tests, and final forbidden-reference searches.

## Final Acceptance Criteria

- No code includes `<boost/archive/...>` or `<boost/serialization/...>`.
- No code references `boost::archive`, `boost::serialization`, `BOOST_CLASS_VERSION`, `Common_Boost_Serialization`, `StructsIOBoostSerialization`, `Boost_Serialization_File_Loader`, or `Boost_Serialization_Archive_Converter`.
- `Boost::serialization` is not requested or linked in CMake.
- New Ygor serialized Drover files can be written and loaded through the operation and generic file loader.
- Kinetic-model `Serialize`/`Deserialize` callers still work.
- Tests cover round trips and loader behavior.
- Documentation no longer describes active functionality as Boost.Serialization-based.
