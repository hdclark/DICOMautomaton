# Python integration tracker

Use this checklist with prompt.md. Keep it current as implementation proceeds.

## Architecture and build

- [x] Add explicit Python feature switches; Python-disabled builds must remain dependency-free.
- [x] Verify the CMake minimum required by the selected FindPython3 components before changing any project-wide minimum.
- [x] Add system-package-first discovery for CPython development artifacts.
- [x] Add system-package-first discovery for pybind11, with only an opt-in immutable FetchContent fallback.
- [x] Do not make pip, Poetry, uv, scikit-build-core, Conda, or another Python package manager a native build prerequisite.
- [x] Refactor repeated OBJECT-library aggregation into a reusable DICOMautomaton core target.
- [x] Ensure the reusable core target is suitable for linking into a loadable Python extension.
- [x] Replace any Python-incompatible global static linker behaviour with target-specific handling while preserving static executable builds.
- [x] Keep GUI/server dependencies conditional; a headless Python-enabled build must remain possible.
- [x] Preserve C++17 for all new C++ code and follow local formatting/naming conventions.

## Native application facade

- [x] Add a small C++17 Session/Workspace facade over Drover, invocation metadata, filename lexicon, loading, scripts, and operation dispatch.
- [x] Preserve File_Loader script/operation staging semantics.
- [x] Reuse Operation_Dispatcher rather than reimplementing operation lookup/defaults/macros in Python.
- [x] Reuse Script_Loader for native DICOMautomaton scripts.
- [x] Expose OperationDoc / OperationArgDoc introspection through the facade.
- [x] Refactor the CLI to use the same facade where practical so Python and CLI paths cannot drift.

## Python extension

- [x] Build the extension directly from CMake.
- [x] Bind Session construction and lifetime.
- [x] Bind path loading.
- [x] Bind generic single-operation execution.
- [x] Bind operation-sequence execution.
- [x] Bind native script execution.
- [x] Bind invocation metadata.
- [x] Bind operation enumeration/documentation.
- [x] Convert Python scalar arguments deterministically into OperationArgPkg string values.
- [x] Translate native operation/configuration failures into useful Python exceptions.
- [x] Keep binding-library types out of core/domain headers.

## Data exchange

- [x] Define and document ownership, invalidation, and mutation semantics before exposing buffers.
- [x] Implement copy-based native-image to NumPy conversion.
- [x] Implement NumPy to native-image copy-back with geometry/metadata preservation.
- [x] Document Python axis ordering.
- [x] Add contour conversion.
- [x] Add point-cloud conversion.
- [x] Add line-sample conversion.
- [x] Add sparse-table conversion; keep pandas optional and pure-Python.
- [x] Add surface-mesh conversion; defer arbitrary std::any attributes unless safely typed.
- [x] Add RT-plan bindings conservatively.
- [x] Design and implement tagged Transform3 bindings after simpler adapters are stable.
- [ ] Do not add writable zero-copy views until invalidation rules are enforceable and tested.

## Embedded Python

- [x] Add a separate build feature for embedded CPython.
- [x] Implement a generic Python/PythonScript operation using the same Session/data adapters as the external extension.
- [x] Initialize the interpreter conservatively and avoid initialize/finalize cycles per operation.
- [x] Use RAII for Python references and GIL handling.
- [x] Do not run pip or mutate the user's Python environment at runtime.
- [x] Make module-search-path configuration explicit.
- [x] Translate Python exceptions/tracebacks into useful DICOMautomaton operation diagnostics.
- [x] Document that embedded Python executes arbitrary code.

## Dependency and system-operations integration

- [x] Inventory every active build target in scripts/get_packages.sh, cmake/PackageLists.cmake, docker/build_bases, docker/builders, GitHub CI, GitLab CI, AppImage scripts, MXE, static Alpine, and macOS.
- [x] Classify each target as extension-supported, embed-supported, intentionally Python-disabled, or pending ops work.
- [x] Promote Python packages from prospective/development-only entries into feature-aware dependency lists.
- [x] Add verified pybind11 package names where available.
- [ ] Update long-lived native Linux build-base images with matching CPython interpreter/development packages.
- [x] Install NumPy only where Python array integration/tests require it; do not make it a core C++ dependency.
- [x] Report selected Python executable/version/include/link targets during CMake configuration.
- [x] Keep MXE Python-disabled until a target-Windows CPython strategy exists.
- [x] Keep fully static Alpine Python-disabled until a dedicated static/embedding design exists.
- [x] Keep AppImage embedded Python disabled until the interpreter, stdlib, relocation, and extension-module story is tested.
- [x] Document manual system-operations steps for each environment that cannot be updated in-source.
- [x] Define and document the supported CPython version window; do not require an EOL interpreter solely for legacy images.

## Testing and CI

- [x] Keep at least one CI configuration with all Python features OFF.
- [x] Add at least one native Python-enabled CI configuration.
- [x] Add extension import/Session construction smoke test.
- [x] Test operation documentation/introspection.
- [x] Test a deterministic operation through Python against the CLI/native path.
- [x] Test native script execution through Python.
- [x] Test a native data fixture through Python loading and inspect Drover content.
- [x] Test image/NumPy round-trip including geometry and metadata.
- [x] Test currently exposed wrapper lifetime and repeated calls in one process; writable-view invalidation remains deferred.
- [x] Test useful diagnostics when Python support is requested but dependencies are absent.
- [x] If embedding is enabled, test Python mutation followed by a native operation.
- [x] If embedding is enabled, test exception traceback propagation.
- [ ] Run existing integration tests to detect non-Python regressions.

## Documentation and release readiness

- [x] Replace/update the placeholder py/ README and packaging metadata without making Python packaging the native build authority.
- [x] Document build prerequisites and CMake flags.
- [x] Document Session usage, operations, scripts, metadata, and data conversion.
- [x] Document copy/lifetime semantics and any unsupported Drover fields.
- [x] Document supported Python versions and intentionally unsupported targets.
- [x] Document embedded-Python security/environment behaviour.
- [x] Document maintainer procedure for updating Python and pybind11 dependencies.
- [x] Keep tracker.md updated with deferred items and platform-specific blockers.
- [ ] Confirm all acceptance criteria in prompt.md before marking the integration milestone complete.

## Progress notes

### 2026-09-24: build foundation

- Added `WITH_PYTHON`, `WITH_PYTHON_EXTENSION`, and `WITH_PYTHON_EMBED`, all dormant when Python support is disabled.
- Kept the project-wide CMake 3.12 baseline. Python-enabled configurations require CMake 3.18 because that is the first release with distinct `Development.Module` and `Development.Embed` components.
- Added `FindPython3` component discovery and system-first pybind11 discovery. The optional pybind11 fallback is pinned to the immutable v2.10.4 commit.
- Added feature-aware Debian/Ubuntu and Arch package names to `cmake/PackageLists.cmake`.
- Added the build-tree `DICOMautomaton::core` static PIC target, reusing the existing production OBJECT libraries. The dispatcher and webserver now consume this target while retaining test-registration objects directly. Keeping the core archive static avoids introducing a Windows DLL export ABI and allows the same archive to back future loadable modules. Installation/export is deferred until the Session API defines a deliberate public-header surface.
- Removed directory-wide `-static`; fully static application linkage remains executable-specific so a future loadable module will not inherit it.
- Validated `BUILD_SHARED_LIBS=ON` and `BUILD_SHARED_LIBS=OFF` Python-disabled CMake target generation with CMake 3.13.4, including a configuration with an intentionally invalid `Python3_ROOT_DIR`. Both generated link plans contain the static PIC `dcma_core`.
- Validated clear configuration failures for inconsistent Python switches and for requesting Python with CMake older than 3.18.
- A real compile/test run was not possible in the current environment because Ygor, YgorClustering, and Explicator package installations are absent. Temporary no-op package configs were used only to validate CMake target generation; no build success is claimed.
- Python extension, Session facade, data adapters, CI, broader platform package inventory, and user documentation remain deferred to subsequent milestones.

### 2026-09-24: Session and initial extension

- Added the C++17 `dcma::Session` facade over `Drover`, invocation metadata, the lexicon path, native file loading, native script parsing, operation dispatch, and operation documentation. File-discovered scripts retain CLI precedence and staged operations are consumed once when run.
- Migrated the command-line dispatcher to the same Session state and dispatch path without moving CLI-only parsing or virtual-data policy into the facade.
- Added an optional CMake-built pybind11 module with Session construction, loading, single and sequence execution, native scripts, metadata, operation documentation, deterministic scalar conversion, native failure details, and a lifetime-safe read-only data summary. pybind11 remains isolated to `Python_Bindings.cc`.
- Removed the Poetry placeholder metadata rather than exposing a broken pure-Python wheel workflow. Added build, API, ownership, version-window, unsupported-target, and dependency-maintenance documentation; CMake remains the only supported build entry point.
- Added Python smoke tests for construction, introspection, operation success/failure, metadata, scripts, loader staging, sequence execution, repeated calls, and dependent-view lifetime.
- Registered extension tests from the top-level CMake test tree, kept module output importable with single- and multi-configuration generators, enforced CPython 3.8-3.11, and made the unimplemented embedding switch fail explicitly rather than configuring an unused dependency.
- Validated Python-disabled CMake generation with CMake 3.13.4 and compiled `Session.cc`, `Operation_Dispatcher.cc`, and the migrated CLI translation unit as C++17. Python source syntax checks also passed.
- The extension could not be compiled or executed in the current environment: it provides CMake 3.13.4 and CPython 3.7.3, below the documented Python integration requirements, and has no pybind11 development package. Python test checkboxes above indicate tests added, not a successful Python-enabled test run.
- Copy-based NumPy image exchange, richer Drover adapters, Python-enabled CI, embedded Python, and the broader platform/system-operations inventory remain deferred.

### 2026-09-24: Quick update

- Operations team installed pybind11-dev system package to support the implementation.

### 2026-09-24: NumPy image exchange and platform integration

- Added detached `ImageSnapshot` wrappers with copy-based C-contiguous `float32` NumPy export in `(row, column, channel)` order and exact-shape pixel copy-back through `DataView`. Geometry and metadata remain native and unchanged during copy-back; no native buffer views are exposed.
- Added tests for non-square multi-channel axis ordering, copy isolation, geometry/metadata preservation, invalid access, native operation execution after copy-back, and loading a multi-slice 3ddose fixture through the native loader.
- Added a Debian Bullseye/Bookworm `python_extension` package tier, wired it into both build-base definitions, and added a native Bookworm GitHub CI job. The long-lived images still require an operations rebuild, and the new CI job has not run in this local environment.
- Pinned the explicitly enabled Ygor, YgorClustering, and Explicator FetchContent fallbacks to the reviewed upstream revisions used by the Python CI job.
- Explicitly disabled all Python features in generic packaging, MXE, static Alpine, and Arch package builds. Added a platform support matrix and image rebuild procedure in `py/PLATFORM_SUPPORT.md`.
- Validated C++17 syntax for `Python_Bindings.cc` against the locally installed pybind11 2.2.4 headers, Python source syntax, shell syntax/package-tier output, and patch whitespace. Full configuration/build/tests remain blocked locally by CMake 3.13.4, CPython 3.7.3 without NumPy, and missing installed Ygor/YgorClustering/Explicator CMake packages.
- Validated Python-disabled CMake 3.13.4 generation after the final changes using temporary no-op package targets for the three unavailable native packages; this validates target generation only, not compilation or runtime behaviour.

### 2026-09-24: Session stabilization

- Corrected undefined behaviour in `Session::load()` by using the destination operation list's iterator when splicing file-discovered operations. This affects CLI and Python script-file loading, including repeated loads.
- Made Python floating-point operation argument conversion locale-independent while retaining round-trip precision.
- Rebuilt the `Session_obj` and `Operation_Dispatcher_obj` targets in a Python-disabled configuration, and revalidated C++17 binding syntax, Python syntax, modified shell-script syntax, patch whitespace, and clear failures for inconsistent Python switches and CMake versions older than 3.18.
- A full Python-disabled dispatcher build was attempted but remains blocked by the local no-op Ygor package configuration, which does not provide `YgorThreadPool.h`. The Python extension remains untestable locally with CMake 3.13.4 and CPython 3.7.3, so the corresponding full regression and acceptance items remain unchecked.

### 2026-09-24: NumPy contour exchange

- Added detached contour snapshots with C-contiguous `float64` NumPy export in `(point, xyz)` order, explicit closed/open state and metadata, collection-aware indexing, and atomic copy-back into an existing contour without changing collection grouping.
- Added contour round-trip, copy isolation, grouping preservation, and invalid-input tests using native synthetic images and `ContourWholeImages`.
- Added detached point-cloud snapshots and atomic copy-back for points, optional normals, packed RGBA colours, and metadata, with shape validation and copy-isolation tests.
- Added detached `(x, sigma_x, f, sigma_f)` line-sample arrays with uncertainty-mode and metadata round trips, shape validation, and copy-isolation tests.
- Validated C++17 syntax with warnings against the locally installed pybind11 2.2.4 headers, Python source syntax, and patch whitespace. Runtime extension tests remain blocked locally by CMake 3.13.4 and CPython 3.7.3; the Python-enabled CI configuration is expected to execute the new tests.

### 2026-09-24: Sparse-table exchange

- Added detached sparse-table snapshots using deterministic `(row, column, value)` tuples and copied string metadata. The representation preserves signed coordinates, holes, and explicitly stored empty strings without adding pandas or another dependency.
- Added atomic table replacement with strict cell validation and duplicate-coordinate rejection, plus round-trip, copy-isolation, native-operation, and invalid-input tests.
- Added a deterministic `ConvertParametersToTable` and `ExportTables` parity test that compares Python Session output byte-for-byte with the native command-line dispatcher.
- Made the extension test build depend on the dispatcher used by the parity test, and aligned legacy pybind11 interpreter discovery with the `FindPython3` selection.
- Validated Python source syntax, C++17 binding syntax with warnings against the locally installed pybind11 2.2.4 headers, and patch whitespace. Runtime extension tests remain blocked locally by CMake 3.13.4 and CPython 3.7.3.

### 2026-09-24: Surface-mesh exchange

- Added detached surface-mesh snapshots for vertices, optional normals and packed colours, variable-length polygon faces, and string metadata.
- Added validated atomic copy-back with face-index bounds checking and derived adjacency reconstruction. Meshes with opaque runtime `std::any` attributes are rejected rather than silently discarding or incorrectly remapping those attributes.
- Added PLY fixture round-trip, copy isolation, native `CopyMeshes` continuation, and invalid-input/no-mutation tests, plus ownership and attribute-limit documentation.
- Validated C++17 binding syntax with warnings against the locally installed pybind11 2.2.4 headers, Python source syntax, and patch whitespace. Runtime extension tests remain blocked locally by CMake 3.13.4 and CPython 3.7.3.

### 2026-09-24: RT-plan exchange

- Added detached plan, beam, and control-point snapshots covering the complete native `RTPlan`, `Dynamic_Machine_State`, and `Static_Machine_State` representations, including metadata and copy-based `float64` jaw/MLC arrays.
- Added complete nested plan copy-back with strict field/type/shape validation. Conversion finishes before native lookup and commit; member swaps preserve the existing shared plan identity without sorting, normalization, or rejection of semantically meaningful NaNs.
- Added a focused real-DICOM RTPLAN fixture round-trip, detached-copy checks, native `CopyRTPlans` continuation, and invalid-input/no-mutation coverage. Documented the complete-replacement schema and ownership/NaN semantics.
- Validated C++17 binding syntax with warnings against the locally installed pybind11 2.2.4 headers, Python source syntax, fixture extraction, and patch whitespace. Runtime extension tests remain blocked locally by CMake 3.13.4 and CPython 3.7.3.

### 2026-09-24: Transform exchange

- Added detached tagged transform snapshots and complete replacement for disengaged, affine, thin-plate-spline, and deformation-field variants.
- Affine matrices preserve the homogeneous 4x4 representation. TPS conversion preserves control points, optional normals/colours, point-set metadata, kernel dimension, and coefficients. Deformation fields preserve every displacement plane, geometry, and metadata while rebuilding the native adjacency index through the validated native constructor.
- Made `Session` explicitly non-copyable so C++ callers cannot accidentally create apparently independent sessions that share `Drover` payloads through copied `shared_ptr` values.
- Added round-trip, copy-isolation, native `CopyWarps` continuation, snapshot lifetime, and invalid-input/no-mutation tests. RPC transform serialization remains intentionally unsupported and zero-copy writable views remain deferred.
- Validated C++17 binding syntax with warnings against the locally installed pybind11 2.2.4 headers. Runtime extension tests remain blocked locally by CMake 3.13.4 and CPython 3.7.3.

### 2026-09-24: Embedded Python operation

- Implemented the separately gated `Python`/`PythonScript` operation for native dynamic builds. It lazily initializes one isolated CPython interpreter, does not finalize it during normal shutdown, executes on the dispatch thread, and uses explicit temporary module search paths.
- Refactored module registration so the external extension and embedded built-in module share the same copy-based data adapters. Embedded callbacks receive a non-owning operation session whose retained session/data wrappers fail safely after callback return.
- Added tests for table mutation, subsequent native `CopyTables` execution, repeated callbacks in one process, expired-wrapper rejection, and traceback diagnostics. Enabled embedding in the existing Debian Bookworm Python CI job.
- Documented arbitrary-code execution, non-transactional mutation, interpreter/GIL lifetime, path isolation, and unsupported static/cross/AppImage targets.
- Validated C++17 syntax for the shared binding and operation sources, Python test syntax, patch whitespace, Python-disabled CMake generation with dependency stubs, and Python-disabled `Session_obj`/`Operation_Dispatcher_obj` builds. Full embedded compilation/runtime testing remains unavailable locally because this host has CMake 3.13.4 and CPython 3.7.3; Bookworm CI is expected to provide CMake 3.25 and CPython 3.11 development/embed artifacts.

### 2026-09-24: acceptance hardening

- Moved fully static linkage from the directory-wide executable linker flags to the individual DICOMautomaton application targets. Loadable Python modules no longer depend on a global static-link exception, while existing `BUILD_SHARED_LIBS=OFF` executables retain `-static`.
- Added a Python CI configure check that deliberately suppresses pybind11 discovery and verifies the actionable missing-development-package diagnostic before the enabled build.
- Regenerated dynamic and static Python-disabled builds with CMake 3.13.4 and dependency stubs, rebuilt `Session_obj` and `Operation_Dispatcher_obj`, and verified the generated static link commands apply `-static` only to application executables. Python and shell syntax checks and patch whitespace checks also passed.
- The diagnostic check and full Python runtime suite still require the Debian Bookworm CI environment; this local host remains below the supported CMake and CPython versions.

### 2026-09-24: integration boundary hardening

- Removed `Python_Bindings_obj` and `Python_Embed_obj` from `DICOMautomaton::core`. The extension, dispatcher, and webserver now attach the Python implementation objects and CPython embed linkage only when their corresponding feature is enabled, leaving the reusable core free of binding implementation.
- Scoped configure-time NumPy validation to extension builds. Embed-only builds can run dependency-free adapters such as sparse tables without NumPy; callbacks using array adapters still require NumPy at runtime.
- Preserved operations discovered by `File_Loader` even when a non-transactional load later reports failure, and added Session loader diagnostics plus a mixed script/unsupported-file regression test.
- Regenerated a minimal Python-disabled build with CMake 3.13.4 and dependency target stubs, rebuilt `Session_obj` and `Operation_Dispatcher_obj`, checked C++17 binding syntax and Python syntax, and verified the generated `dcma_core` archive contains `Session_obj` but no Python binding/embed objects.
- Ran all 41 native integration scripts against the installed dispatcher: 36 passed and 5 failed because that installed binary predates operations or methods present in this source tree (`PatchMeshHoles`, `OptimizeSchedule`, mesh remeshing/convex-hull paths). This does not validate the modified source dispatcher, so the full non-Python regression and final acceptance items remain unchecked pending a complete source build in a supported environment.

### 2026-09-24: lifecycle and packaging hardening

- Prevented embedded callback code from retaining temporary module search paths by restoring the original `sys.path` object even when the script rebinds it, and added regression coverage for that case.
- Stopped masking extension initialization failures in an already-running interpreter, retained Python exception details during owned-interpreter initialization, and kept the internal embed compile definition off exported global compile settings.
- Disabled POSIX process-forking paths after either Python binding initializes because raw `fork()` can inherit CPython locks in an unsafe state. This covers the `Fork` operation, detached plotting, and portable file dialogs; Python-disabled native use and Windows thread-emulated forking are unchanged.
- Added native script-parser diagnostics to `Session::last_error()`, preserved those details in CLI failures, tightened malformed `run_many()` diagnostics, and fixed deformation-field NumPy shape construction for 32-bit `Py_ssize_t` platforms.
- Decoupled the production extension target from the dispatcher used only for parity testing, restored the omitted SDL viewer test object on the webserver, scoped NumPy configure validation and Python tests to `BUILD_TESTING`, and stopped requiring NumPy for embed-only package dependency lists.
- Added an origin-relative install RPATH for the extension so project shared libraries remain discoverable under custom installation prefixes. CI now builds the dispatcher explicitly before running the CLI parity test.
- Regenerated shared and static Python-disabled builds with CMake 3.13.4 and dependency target stubs, rebuilt `Session_obj` and `Operation_Dispatcher_obj`, and checked binding/Fork C++17 syntax, Python and shell syntax, old-CMake diagnostics, and patch whitespace. Supported-version extension/embed compilation and runtime tests remain unavailable on this CMake 3.13.4/CPython 3.7.3 host and must run in the Bookworm CI job.

### 2026-09-24: embedded runtime review

- Fixed the optional `ModuleSearchPaths` argument so omitting it uses the documented empty default instead of throwing `bad_optional_access` before CPython runs.
- Corrected the combined extension/embed test to pass one semicolon-separated module-search value rather than duplicate operation arguments.
- Ensured embedded initialization-error objects are destroyed while the GIL is held, and serialized callbacks around temporary process-global `sys.path` replacement without acquiring the GIL while waiting for the serialization lock.
- Removed the common binding object's dependency on `pybind11::module`; embed-only compilation now consumes the binding headers without requiring the extension-module target.
- Updated the documented extension test build to include the dispatcher required by the native/Python parity test.
- Revalidated Python syntax, patch whitespace, the Python operation's C++17 syntax, the expected actionable CMake 3.13 failure for requested Python support, and a Python-disabled CMake generation plus `Session_obj`/`Operation_Dispatcher_obj` build using dependency target stubs. `Python_Embed.cc` cannot be compiled against this host's CPython 3.7 headers because the supported implementation uses the CPython 3.8+ configuration API; supported-version compile/runtime tests and full source integration tests remain pending.

### 2026-09-24: facade and build review

- Centralized default lexicon discovery/creation in `Session`, so CLI and Python loading and operation execution now receive the same lexicon initialization while preserving explicitly selected lexicons.
- Serialized POSIX `fork()` against the transition to CPython initialization with a child-safe atomic lifecycle state. Embedding disables new forks before CPython creates process-global state rather than waiting for module binding.
- Added explicit `Python3::Module` linkage, AdaptiveCpp final-target setup for the extension, and install-RPATH calculation that handles relative or absolute Python and library destinations.
- Loader exceptions now retain the Python API's non-transactional state warning, and generated Python bytecode was removed from the test tree.
- Reconfigured shared and static Python-disabled builds with CMake 3.13.4 and dependency target stubs, rebuilt `Session_obj` and `Operation_Dispatcher_obj`, compiled the modified session/CLI/fork/binding sources as C++17, checked Python syntax and patch whitespace, and reconfirmed the actionable old-CMake Python diagnostic. The full source regression and supported CPython runtime suite remain pending on a supported build host.

### 2026-09-24: acceptance review follow-up

- Restored CLI lexicon initialization before PACS loading by exposing the Session-owned initialization step; PACS and standalone loaders again receive the same discovered or generated lexicon.
- Centralized POSIX fork serialization in a small native guard shared by the `Fork` operation, detached plotting, and portable file-dialog process discovery/launch. CPython initialization now waits for an in-progress fork and permanently rejects every known raw-fork path afterward.
- Added combined extension/embed coverage that invokes the `Python` operation from an already initialized Python interpreter and requires operation registration when embedding is configured.
- Regenerated a Python-disabled CMake 3.13.4 build with dependency target stubs; built `processforkshim`, `Session_obj`, `Operation_Dispatcher_obj`, and the modified plotting/dialog translation units; and passed Python syntax, shell syntax, static source checks, old-CMake diagnostics, and patch whitespace checks.
- Supported CPython compile/runtime tests, the complete source dispatcher build, and full native integration tests remain pending because this host has CMake 3.13.4 and CPython 3.7.3 and lacks complete installed Ygor dependencies. Acceptance and build-image checkboxes remain open accordingly.

### 2026-09-24: final static review follow-up

- Fixed an embed-only compile failure by including the native process-fork lifecycle declaration directly rather than the unrelated `Fork` operation header.
- Made pre-dispatch lexicon setup failures follow the Session boolean/error contract, with useful operation-setup diagnostics for Python callers.
- Strengthened embedded traceback coverage to require a failing dispatcher exit status, and documented whole-batch consumption plus external `DataView` ownership semantics.
- Expanded the platform matrix to classify the active Ubuntu, Debian Stretch/Buster, Arch/SYCL, Void, Fedora-package, WebAssembly, GitLab, and AppImage paths without overstating untested Bullseye support.
- Supported CPython compilation/runtime testing, full source regressions, and the long-lived build-image rebuild remain pending in the environments identified above.
