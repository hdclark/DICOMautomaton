# Python integration tracker

Use this checklist with prompt.md. Keep it current as implementation proceeds.

## Architecture and build

- [ ] Add explicit Python feature switches; Python-disabled builds must remain dependency-free.
- [ ] Verify the CMake minimum required by the selected FindPython3 components before changing any project-wide minimum.
- [ ] Add system-package-first discovery for CPython development artifacts.
- [ ] Add system-package-first discovery for pybind11, with only an opt-in immutable FetchContent fallback.
- [ ] Do not make pip, Poetry, uv, scikit-build-core, Conda, or another Python package manager a native build prerequisite.
- [ ] Refactor repeated OBJECT-library aggregation into a reusable DICOMautomaton core target.
- [ ] Ensure the reusable core target is suitable for linking into a loadable Python extension.
- [ ] Replace any Python-incompatible global static linker behaviour with target-specific handling while preserving static executable builds.
- [ ] Keep GUI/server dependencies conditional; a headless Python-enabled build must remain possible.
- [ ] Preserve C++17 for all new C++ code and follow local formatting/naming conventions.

## Native application facade

- [ ] Add a small C++17 Session/Workspace facade over Drover, invocation metadata, filename lexicon, loading, scripts, and operation dispatch.
- [ ] Preserve File_Loader script/operation staging semantics.
- [ ] Reuse Operation_Dispatcher rather than reimplementing operation lookup/defaults/macros in Python.
- [ ] Reuse Script_Loader for native DICOMautomaton scripts.
- [ ] Expose OperationDoc / OperationArgDoc introspection through the facade.
- [ ] Refactor the CLI to use the same facade where practical so Python and CLI paths cannot drift.

## Python extension

- [ ] Build the extension directly from CMake.
- [ ] Bind Session construction and lifetime.
- [ ] Bind path loading.
- [ ] Bind generic single-operation execution.
- [ ] Bind operation-sequence execution.
- [ ] Bind native script execution.
- [ ] Bind invocation metadata.
- [ ] Bind operation enumeration/documentation.
- [ ] Convert Python scalar arguments deterministically into OperationArgPkg string values.
- [ ] Translate native operation/configuration failures into useful Python exceptions.
- [ ] Keep binding-library types out of core/domain headers.

## Data exchange

- [ ] Define and document ownership, invalidation, and mutation semantics before exposing buffers.
- [ ] Implement copy-based native-image to NumPy conversion.
- [ ] Implement NumPy to native-image copy-back with geometry/metadata preservation.
- [ ] Document Python axis ordering.
- [ ] Add contour conversion.
- [ ] Add point-cloud conversion.
- [ ] Add line-sample conversion.
- [ ] Add sparse-table conversion; keep pandas optional and pure-Python.
- [ ] Add surface-mesh conversion; defer arbitrary std::any attributes unless safely typed.
- [ ] Add RT-plan bindings conservatively.
- [ ] Design tagged Transform3 bindings only after simpler adapters are stable.
- [ ] Do not add writable zero-copy views until invalidation rules are enforceable and tested.

## Embedded Python

- [ ] Add a separate build feature for embedded CPython.
- [ ] Implement a generic Python/PythonScript operation using the same Session/data adapters as the external extension.
- [ ] Initialize the interpreter conservatively and avoid initialize/finalize cycles per operation.
- [ ] Use RAII for Python references and GIL handling.
- [ ] Do not run pip or mutate the user's Python environment at runtime.
- [ ] Make module-search-path configuration explicit.
- [ ] Translate Python exceptions/tracebacks into useful DICOMautomaton operation diagnostics.
- [ ] Document that embedded Python executes arbitrary code.

## Dependency and system-operations integration

- [ ] Inventory every active build target in scripts/get_packages.sh, cmake/PackageLists.cmake, docker/build_bases, docker/builders, GitHub CI, GitLab CI, AppImage scripts, MXE, static Alpine, and macOS.
- [ ] Classify each target as extension-supported, embed-supported, intentionally Python-disabled, or pending ops work.
- [ ] Promote Python packages from prospective/development-only entries into feature-aware dependency lists.
- [ ] Add verified pybind11 package names where available.
- [ ] Update long-lived native Linux build-base images with matching CPython interpreter/development packages.
- [ ] Install NumPy only where Python array integration/tests require it; do not make it a core C++ dependency.
- [ ] Report selected Python executable/version/include/link targets during CMake configuration.
- [ ] Keep MXE Python-disabled until a target-Windows CPython strategy exists.
- [ ] Keep fully static Alpine Python-disabled until a dedicated static/embedding design exists.
- [ ] Keep AppImage embedded Python disabled until the interpreter, stdlib, relocation, and extension-module story is tested.
- [ ] Document manual system-operations steps for each environment that cannot be updated in-source.
- [ ] Define and document the supported CPython version window; do not require an EOL interpreter solely for legacy images.

## Testing and CI

- [ ] Keep at least one CI configuration with all Python features OFF.
- [ ] Add at least one native Python-enabled CI configuration.
- [ ] Add extension import/Session construction smoke test.
- [ ] Test operation documentation/introspection.
- [ ] Test a deterministic operation through Python against native behaviour.
- [ ] Test native script execution through Python.
- [ ] Test native file loading through Python.
- [ ] Test image/NumPy round-trip including geometry and metadata.
- [ ] Test lifetime/invalidation behaviour and repeated calls in one process.
- [ ] Test useful diagnostics when Python support is requested but dependencies are absent.
- [ ] If embedding is enabled, test Python mutation followed by a native operation.
- [ ] If embedding is enabled, test exception traceback propagation.
- [ ] Run existing integration tests to detect non-Python regressions.

## Documentation and release readiness

- [ ] Replace/update the placeholder py/ README and packaging metadata without making Python packaging the native build authority.
- [ ] Document build prerequisites and CMake flags.
- [ ] Document Session usage, operations, scripts, metadata, and data conversion.
- [ ] Document copy/lifetime semantics and any unsupported Drover fields.
- [ ] Document supported Python versions and intentionally unsupported targets.
- [ ] Document embedded-Python security/environment behaviour.
- [ ] Document maintainer procedure for updating Python and pybind11 dependencies.
- [ ] Keep tracker.md updated with deferred items and platform-specific blockers.
- [ ] Confirm all acceptance criteria in prompt.md before marking the integration milestone complete.
