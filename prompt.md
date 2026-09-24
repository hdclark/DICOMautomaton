# Python integration implementation prompt

## Purpose

Implement durable, maintainable Python integration for DICOMautomaton without rewriting the project in Python and without weakening the existing C++ architecture.

The objective is to make DICOMautomaton useful to Python-oriented contributors and to permit bidirectional data exchange with the Python scientific ecosystem, while keeping the C++ implementation authoritative. Python should become an interface and extension layer over the existing application state, operation dispatcher, file loaders, and scripting machinery.

This document is an implementation handoff. It describes the architecture to preserve, the intended integration boundary, build-system constraints, dependency strategy, testing expectations, and the system-operations work that may be required on supported build environments.

Read tracker.md alongside this file and keep it updated as implementation proceeds.

## Non-negotiable constraints

1. All new C++ code must use C++17. Do not introduce C++20 or newer language/library requirements.
2. Follow the local DICOMautomaton style in the files being modified. Do not apply broad reformatting. Preserve existing naming, indentation, include ordering, comments, logging style, ownership conventions, and CMake style unless a local change is necessary.
3. Preserve existing non-Python builds. Python support must be feature-gated so platforms that do not yet provide a compatible Python development environment continue to build.
4. Do not make network access a normal requirement of configure, compile, test, or install.
5. Do not make Poetry, uv, Hatch, scikit-build-core, Meson-Python, Conda, virtualenv tooling, or another fast-moving Python packaging layer the foundation of the native build.
6. CMake remains the source of truth for compiling C++ and the Python extension.
7. CPython is a system/platform dependency, not something DICOMautomaton should download and build automatically.
8. Prefer system packages for dependencies. If a C++ binding helper such as pybind11 is used, discover an installed package first and only use the repository's existing explicitly enabled, immutable-commit FetchContent fallback pattern as a secondary option.
9. Do not make Apache Thrift the canonical in-process Python API. Thrift remains useful for remote execution and interchange, but local Python integration should bind the native application state and dispatcher directly.
10. Avoid zero-copy interfaces until ownership, lifetime, mutation, and invalidation semantics are explicit and tested. Correctness is more important than avoiding a copy.
11. Do not silently broaden the set of required dependencies for all targets. In particular, static Alpine, MXE cross builds, AppImage packaging, and other constrained targets must remain buildable with Python disabled until deliberate support is added.
12. Preserve behaviour of the existing command-line dispatcher and operation pipeline while refactoring shared internals.
13. New Python-facing APIs must report errors as Python exceptions with useful messages; do not expose raw false-return conventions as the primary Python user experience.
14. Do not introduce a second independent implementation of file loading, operation lookup, argument defaulting, macro expansion, or script parsing in Python. Reuse the C++ implementation.

## Relevant current architecture

DICOMautomaton is fundamentally a C++17 application. The existing architecture already contains an effective Python integration boundary; it is not necessary or desirable to bind hundreds of implementation functions individually.

The central in-memory state is Drover in src/Structs.h. It aggregates the principal data types used by operations, including contour data, image arrays, point clouds, surface meshes, radiotherapy plans, line samples, transformations, and sparse tables. Most of those objects ultimately wrap Ygor data structures and carry metadata.

The operation layer is defined in src/Operation_Dispatcher.h and src/Operation_Dispatcher.cc. Operation_Dispatcher accepts a Drover, invocation metadata, a filename lexicon, and a list of OperationArgPkg objects. Known_Operations exposes the operation registry, and OperationDoc / OperationArgDoc provide machine-readable documentation and argument metadata. The dispatcher performs operation-name canonicalization, default insertion, macro expansion, and error handling before invoking the selected operation.

The script parser in src/Script_Loader.* produces the same OperationArgPkg tree consumed by Operation_Dispatcher. Meta-operations recursively invoke the same dispatcher. Direct operation calls and DICOMautomaton scripts therefore already converge at one layer.

The file loader in src/File_Loader.* populates Drover and can also discover scripts/operations while loading paths. The command-line dispatcher owns the application state required for loading and execution.

The repository currently does not expose a reusable application-level core library target. src/CMakeLists.txt defines many OBJECT libraries and the executables aggregate a large number of object targets directly. The Python integration should not duplicate this aggregation.

The repository contains a placeholder py/ package. Its current pyproject.toml uses Poetry and describes a pure Python package. Treat it as a placeholder, not as an architectural constraint.

Apache Thrift support already mirrors much of Drover and provides RPC endpoints, but it is incomplete for some native types and necessarily copies/serializes data. Keep it as a remote transport option rather than the local binding boundary.

## Target architecture

The intended layering is:

~~~
                         DICOMautomaton C++ core
                                  |
                         dcma::Session / Workspace
                                  |
                +-----------------+-----------------+
                |                 |                 |
                v                 v                 v
               CLI          Python extension      web/RPC
                                  |
                                  v
                         Python scientific tools
                                  |
                       optional modified data
                                  |
                                  v
                       continuing DCMA pipeline
~~~

The two principal integration levels are:

1. The operation/session level: expose loading, operation execution, script execution, operation documentation, invocation metadata, and access to the current Drover through one stable C++ facade.
2. The data-exchange level: expose selected Drover constituent types in Python-friendly form, beginning with images and other numerically simple types.

Do not start by binding individual numerical algorithms. A small binding around the operation/session layer exposes a large fraction of DICOMautomaton immediately and keeps Python behaviour aligned with the native application.

## Phase 1: create a reusable C++ core target

Refactor the build so the application logic needed by the dispatcher can be linked through a reusable target instead of repeated TARGET_OBJECTS lists.

Preferred outcome:

~~~
DICOMautomaton::core
~~~

or an equivalent local target with an exported alias.

Requirements:

- Reuse the existing OBJECT libraries rather than recompiling large translation units unnecessarily.
- Propagate include directories, compile definitions, and linked dependencies through target-based CMake.
- Keep POSITION_INDEPENDENT_CODE enabled where needed for a loadable Python extension.
- Preserve existing executable behaviour.
- Make the command-line dispatcher, web/RPC components where appropriate, and Python extension consume the same reusable core target.
- Do not accidentally pull GUI-only dependencies into a headless Python build when the corresponding features are disabled.
- Preserve install/export behaviour where practical and document any new public target.
- Avoid global linker flags that make a loadable extension impossible.

Pay particular attention to the current static-link path. The top-level build applies global static linker settings when BUILD_SHARED_LIBS is disabled. A Python extension is itself a dynamically loaded module and cannot inherit indiscriminate executable-only static linker flags. Refactor static-link behaviour toward target-specific settings if required, while keeping the existing static executable builds working.

Do not change the meaning of BUILD_SHARED_LIBS merely to make the Python module compile.

## Phase 2: introduce a stable application facade

Add a small C++17 facade, tentatively named dcma::Session. Workspace is also acceptable if that better matches local terminology. Avoid a generic name that is likely to collide with unrelated Context types.

The facade should own or coordinate the state currently assembled by the dispatcher path:

- Drover
- invocation metadata
- filename lexicon
- any operation staging needed to preserve loader/script semantics

A representative shape is:

~~~
namespace dcma {

class Session {
    // Native application state.

public:
    Drover &data();
    const Drover &data() const;

    // Load paths using existing DICOMautomaton loaders.
    // Exact signature should preserve script-loading semantics.
    bool load(...);

    // Run one or more OperationArgPkg objects.
    bool run(...);

    // Parse and run native DICOMautomaton script text.
    bool run_script(...);

    // Expose operation documentation/introspection.
    std::vector<OperationDoc> operations() const;

    std::map<std::string, std::string> &metadata();
    const std::map<std::string, std::string> &metadata() const;
};

}
~~~

The exact signatures are not fixed by this document. The important requirement is that the facade owns the application state and delegates to the existing File_Loader, Script_Loader, and Operation_Dispatcher code rather than reimplementing their logic.

Preserve script-file loading semantics. File_Loader can discover operations while loading paths; do not lose that behaviour accidentally. If a clean Session API requires explicit staging or a returned list of discovered operations, design and test that explicitly.

Refactor the native command-line path to use this facade where practical. This is important: the Python API must exercise the same implementation path as the normal application rather than creating a parallel pathway that gradually diverges.

Keep this facade free of Python headers and Python-specific types.

## Phase 3: add optional CPython build support

Introduce explicit CMake feature switches. A single master WITH_PYTHON option is acceptable, but separate subfeatures are preferable because building a Python extension and embedding CPython have different linkage and packaging requirements.

Suggested structure:

~~~
WITH_PYTHON
WITH_PYTHON_EXTENSION
WITH_PYTHON_EMBED
~~~

The exact names may follow local naming conventions.

Expected behaviour:

- Default must not break current platforms.
- If a Python feature is disabled, no Python headers or libraries are required.
- If a Python feature is enabled and the required development artifacts are missing, configuration must fail early with a clear message explaining what to install.
- Use CMake's FindPython3 support rather than ad hoc searches for python executables, include directories, or libpython files.
- Extension-module builds should request the module-development components required by CMake.
- Embedded-interpreter builds should request the embed-development components required by CMake.
- Do not assume that the interpreter executable found on the build host is suitable for a cross-compiled target.
- Do not download CPython using FetchContent.
- Do not require pip in order to compile DICOMautomaton.

The implementation team must verify the minimum CMake version needed for the selected FindPython3 components. Do not silently raise cmake_minimum_required. If a higher CMake baseline is genuinely required, make that a deliberate compatibility change, update supported build images first, and document the reason.

## Binding technology

Prefer a thin, well-isolated C++ binding layer.

The recommended default is pybind11, discovered as a normal CMake package. It is mature, C++ oriented, has long-standing CMake integration, and can keep the binding code much smaller than hand-written CPython wrappers.

However, pybind11 is a convenience dependency, not part of the domain model. Isolate its use to the binding layer so replacing it later would not affect Session or core application code.

Dependency policy:

1. Try find_package(pybind11 CONFIG QUIET) or the equivalent appropriate to the supported version.
2. Add pybind11 package names to the repository's centralized package lists where the target package manager provides them.
3. If and only if WITH_FETCHCONTENT_FALLBACK is enabled, an immutable-commit FetchContent fallback may be used, consistent with existing repository policy.
4. Pin the fallback to an immutable commit, not a mutable branch or tag.
5. Do not fetch pybind11 through pip during the CMake build.
6. Do not use pybind11 as justification for changing C++17.

If direct CPython C API code is needed for interpreter initialization or low-level embedding, keep it behind a small RAII wrapper with explicit ownership and GIL rules.

## Python package/build policy

The native CMake build is authoritative.

The current Poetry-based py/pyproject.toml is placeholder scaffolding. Do not require Poetry to build or install the extension.

For the first implementation:

- CMake must be able to build and test the extension directly.
- CMake install rules may install the extension and pure-Python helper files into a configured Python installation location.
- Python packaging metadata should remain thin and should not duplicate compiler/linker logic from CMake.
- A pip/wheel workflow is useful but is not required to be the first deliverable if making it robust would distort the native build.

If a PEP 517 frontend is eventually provided, choose a conservative, widely supported mechanism and make it a wrapper around the CMake build rather than a second build system. Keep the pyproject metadata standards-based. Avoid tying the project to a workflow that is difficult to reproduce outside Python packaging tools.

Do not make editable installs, virtual environments, pip build isolation, or an internet connection prerequisites for building DICOMautomaton itself.

## Phase 4: expose the session-level Python API

The first Python API should be small and explicit.

Target usage should look approximately like:

~~~
import dicomautomaton as dcma

s = dcma.Session()
s.load(["CT001.dcm", "CT002.dcm"])

s.run(
    "ThresholdImages",
    Lower="100",
    Upper="500",
)

s.run_script("""
    NormalizePixels();
    SpatialBlur(Estimator=Gaussian);
""")

for op in s.operations():
    print(op.name, op.description)
~~~

Python keyword values may initially be converted to strings because OperationArgPkg is already string-valued. Do this in one well-defined conversion function and keep behaviour deterministic.

Expose at least:

- Session construction
- loading paths
- running one operation
- running a sequence of operations
- running native DICOMautomaton script text
- invocation metadata access
- operation enumeration
- OperationDoc and OperationArgDoc fields needed for introspection
- access to the current data through controlled wrappers/adapters

Operation failures should become Python exceptions. Include the operation name and useful diagnostic context. Do not leave the Session in an undocumented partially mutated state; where native operations are non-transactional, document that limitation clearly.

Do not generate hundreds of Python methods as the canonical interface. Generic run plus OperationDoc introspection is more durable. Pure-Python ergonomic helpers can be added later.

## Phase 5: data exchange

Expose data incrementally. Do not attempt to bind every Ygor template or every method on every native class.

### General ownership rules

- Copy-by-default for numerical arrays.
- Never expose a pointer into a vector or image buffer unless its lifetime and invalidation rules are explicit.
- A Python object must not silently retain a dangling view after a later DICOMautomaton operation reallocates or replaces storage.
- Where top-level Drover entries are held by shared_ptr, wrappers may share ownership if that matches native semantics, but replacing an entry in Drover must be handled/documented.
- Metadata conversion must preserve strings exactly.
- Round trips must not silently change coordinate systems, dimensions, orientation, units, NaN values, or metadata.
- Prefer explicit to_numpy / from_numpy or equivalent copy APIs before adding writable views.

### Images: first priority

Images provide the largest immediate benefit for Python tooling.

Expose pixel values as NumPy-compatible float32 arrays and expose the geometry required to reconstruct the native image exactly, including dimensions, spacing, anchor/offset, row direction, column direction, and metadata.

Use native array ordering consistently and test it with non-square, multi-channel, and multi-slice examples where applicable. Document axis order in Python.

Do not make the NumPy C API a deep dependency of core DICOMautomaton. Keep NumPy-specific code in the Python binding/adaptor layer. Runtime NumPy may be required for the NumPy adapter.

### Contours

Expose each contour as an N x 3 numeric point array plus closed/open state and metadata. Preserve contour collection grouping and metadata.

### Point clouds

Expose point coordinates and any normals/associated values that are part of the Ygor point_set representation. Preserve metadata.

### Line samples

Expose a compact numeric representation and metadata. Test exact round trips.

### Sparse tables

Keep the native table representation dependency-free. A pure-Python adapter may convert it to and from pandas when pandas is installed, but pandas must not become a native build dependency.

### Surface meshes

Expose vertices and faces first, then normals/colours where present. The current runtime std::any vertex/face attributes are intentionally difficult to serialize generically; do not invent unsafe type erasure. Support only explicitly recognized attribute types or defer arbitrary attributes until a typed representation exists.

### RT plans

Bind the existing nested static/dynamic machine-state structures conservatively. Preserve metadata and NaN semantics.

### Transformations

Defer full Transform3 support until the simpler data types are stable. The native variant contains affine, thin-plate-spline, and deformation-field forms with very different data. Design a tagged Python representation rather than flattening them into an ambiguous structure.

## Phase 6: pure-Python ergonomics

After the native extension is stable, add a small pure-Python layer for:

- type hints
- docstrings generated or adapted from OperationDoc
- convenience conversion of common Python scalar types to OperationArgPkg strings
- optional pandas helpers
- optional NumPy convenience helpers
- readable repr methods

Keep this layer thin. Domain behaviour remains in C++.

Where possible, use OperationDoc as the single source for operation documentation so Python help output does not drift from the native CLI documentation.

## Phase 7: embedded Python operation

After the extension and data adapters are stable, add an optional DICOMautomaton operation that executes Python against the current Session/data.

Possible interface:

~~~
Python(
    Script="algorithm.py",
    Function="process"
)
~~~

or an equivalent explicit module/function form.

A Python callback could look like:

~~~
def process(session):
    image = session.data.images[0]
    a = image.to_numpy()
    result = some_python_library(a)
    image.from_numpy(result)
~~~

The operation must use the same bindings/adapters as the external Python extension wherever possible. Do not maintain separate conversion code for embedded and extension modes.

### Interpreter lifecycle

Embedding CPython must be conservative.

- Prefer the modern CPython configuration API supported by the selected minimum Python version.
- Initialize the interpreter once per process, lazily when Python is first requested unless there is a strong reason to initialize at program start.
- Do not initialize and finalize CPython for every operation.
- Avoid Py_FinalizeEx during normal operation unless shutdown behaviour is proven safe with all extension modules in use.
- Use RAII for Python references and GIL acquisition.
- Never pass Python objects to worker threads without explicit GIL/lifetime handling.
- Convert Python exceptions into DICOMautomaton operation failures with traceback text where possible.
- Do not automatically run pip or install packages at runtime.
- Do not mutate a user's Python environment.
- Do not silently prepend arbitrary working directories to sys.path.
- Provide explicit configuration for additional module search paths if needed.
- Treat embedded Python code as arbitrary code execution and document it as such.

Initially execute Python on the operation-dispatch thread. Do not combine the first implementation with a new concurrency model.

## Phase 8: remote Python

Keep remote use conceptually separate from local bindings.

A later dicomautomaton.remote.RemoteSession may use the existing Thrift transport while presenting a Session-like Python API. This can be useful for remote servers and environments where the native extension is unavailable.

Do not force local Session through Thrift.

Before advertising full remote data fidelity, address known RPC gaps such as unsupported Transform3 serialization and runtime mesh attributes. GetSupportedOperations should eventually expose rich operation documentation rather than operation names only, and LoadFiles should be completed if it is part of the public remote API.

## Python dependency integration in CMake

Python support must be optional and explicit.

A representative native configuration should eventually be possible with:

~~~
cmake -S . -B build \
    -DWITH_PYTHON=ON \
    -DWITH_PYTHON_EXTENSION=ON \
    -DWITH_PYTHON_EMBED=OFF
cmake --build build
ctest --test-dir build
~~~

Do not hard-code /usr/bin/python3, /usr/include/pythonX.Y, or libpythonX.Y.so.

Use imported CMake Python targets where available.

Be precise about the difference between:

- the Python interpreter used to run tests or packaging helpers
- headers/libs for building a Python extension for the target
- headers/libs for embedding a Python interpreter into a target executable

This distinction becomes critical during cross compilation.

## System operations handoff

Python is not currently a consistently installed DICOMautomaton dependency. The implementation team must make the source tree ready for Python first, but some build images and package repositories may require manual system-operations changes later.

The system-operations work must be documented in the repository so it can be performed without reverse-engineering CMake errors.

### Inventory all supported environments

Review at minimum:

- scripts/get_packages.sh
- cmake/PackageLists.cmake
- docker/build_bases/
- docker/builders/
- .github/workflows/main.yml
- .gitlab-ci.yml
- AppImage assembly scripts
- MXE toolchain/build scripts
- static Alpine builders
- macOS build scripts
- any package recipes maintained outside the repository

Classify each target as one of:

1. Python extension supported
2. embedded Python supported
3. Python intentionally disabled
4. pending system-operations work

Do not leave this implicit.

### Package requirements

For native Linux builds, the required system package is normally the CPython interpreter plus matching development headers/libraries. Exact package names vary by distribution.

Examples to verify against each active distribution:

- Debian/Ubuntu: python3 and python3-dev or libpython3-dev; python3-numpy for NumPy integration tests where packaged
- Arch Linux: python; python-numpy for NumPy integration tests
- Alpine: python3 and python3-dev; py3-numpy for integration tests where appropriate
- Void Linux: python3 and the matching python3 development package; python3-numpy if available
- macOS/Homebrew: the supported Homebrew Python formula; verify that CMake discovers the same installation intended for build/test

Do not copy these package names blindly into every historical image. Verify the package exists for that exact release.

The repository currently lists libpython3-dev in scripts/get_packages.sh only as a prospective Debian development package. Promote Python dependencies into the correct required/optional tier according to the feature switches rather than relying on that incidental entry.

### pybind11 package requirements

Where a supported distribution provides a suitable pybind11 development package, add it to the centralized dependency mapping for Python-enabled builds.

If a supported release provides an unusably old pybind11 package, prefer one of:

- keeping Python disabled on that legacy target
- using the explicitly enabled immutable FetchContent fallback

Do not add pip install pybind11 to system build scripts.

### Version matching

The interpreter used for Python tests must match the development headers/module ABI used to build the extension.

CMake configure output should report:

- selected Python executable
- Python version
- include directory
- extension module target/link information
- whether embed support is enabled

Do not manually symlink libpython versions to satisfy the build.

### Cross compilation and MXE

Do not enable Python automatically in the MXE build merely because a host python3 exists. The host interpreter is not the Windows target runtime.

Keep Python disabled for MXE until the target has a deliberate Windows CPython development/runtime strategy. When support is added, document:

- target Python version and architecture
- source of Windows headers/import libraries
- how the extension is built for the target
- how the matching python DLL/stdlib is supplied
- how tests are executed under Wine or a Windows runner
- whether embedded Python is supported separately from the extension

Do not contaminate target discovery with host Python paths.

### Static Alpine builds

Fully static applications and CPython extension ecosystems do not combine trivially. Keep Python disabled in existing fully static targets until a dedicated design is implemented and tested.

Do not weaken the static artifact just to claim Python support.

### AppImage and other relocatable bundles

If embedded Python is enabled in a relocatable application bundle, the runtime requires more than libpython: it also needs a matching standard library and a predictable module search path.

Do not bundle a partial interpreter.

For the first integration, it is acceptable for AppImage builds to keep embedded Python disabled while the external Python extension is tested separately against system Python.

If a self-contained interpreter is later bundled, add explicit packaging tests for:

- startup without host Python installed
- stdlib imports
- binary extension imports
- sys.path
- relocation to a different directory
- SSL/certificates if relevant
- NumPy import if NumPy is part of the supported bundled environment

### Build image updates

When source support is ready, system operations should rebuild the long-lived DICOMautomaton build-base images with the verified Python development packages.

Do not rely on installing packages ad hoc in every CI job if the normal project pattern is to maintain build-base images.

Record the image rebuild procedure, resulting tags/digests where appropriate, and the date/version of the Python toolchain included.

### Security and maintenance policy

Do not pin the project permanently to an end-of-life CPython merely to preserve a historical distribution image.

If a legacy build environment only provides an unsupported Python version, leave WITH_PYTHON disabled there or update the environment. Native non-Python DICOMautomaton should continue to build.

Python support should be tested against a small supported-version window, not every CPython version ever released. Define that window in documentation and update it deliberately.

## Testing requirements

Add tests at multiple levels.

### Non-Python regression

At least one CI path must configure and build with all Python features disabled. This prevents accidental unconditional includes or links.

### Configure failures

Test or manually verify clear failures when Python is requested but the development artifacts are absent.

### Extension smoke test

With Python extension support enabled:

~~~
import dicomautomaton
s = dicomautomaton.Session()
assert s is not None
~~~

### Operation introspection

Verify that known operations can be enumerated and that operation names/descriptions/arguments match the native OperationDoc data.

### Operation execution

Run a deterministic, lightweight operation through Python and compare the result with the native path.

### Script execution

Parse and run a small native DICOMautomaton script through Session.run_script and validate the resulting state.

### Loader round trip

Load a small fixture through the Python Session using the native loader and inspect the expected Drover content.

### NumPy image round trip

Test:

1. native image to NumPy copy
2. mutation in Python
3. copy back to native image
4. native operation after the copy-back
5. geometry and metadata preservation

Use a fixture with dimensions/orientation chosen to catch axis-order mistakes.

### Lifetime tests

Explicitly test that:

- deleting Python wrappers does not invalidate native Session state unexpectedly
- deleting a Session invalidates dependent wrappers safely
- running an operation that replaces/reallocates data cannot leave a silently dangling writable view
- exceptions do not leak Python references

### Embedded Python tests

When WITH_PYTHON_EMBED is enabled, run a small script that reads and modifies Session data and then continue with a native operation.

Also test a Python exception and verify the DICOMautomaton error includes useful traceback context.

### Repeated execution

Exercise repeated Python calls in one process to catch interpreter/GIL/reference leaks.

## Documentation requirements

Add user-facing documentation covering:

- how to build with Python enabled
- required system packages
- how to import the module
- Session basics
- running operations
- running native scripts
- data conversion and ownership rules
- supported Python versions
- which build targets intentionally disable Python
- embedded Python security implications
- environment/module-search configuration
- remote Python status if implemented

Keep operation documentation derived from OperationDoc where practical.

Add maintainer documentation describing how to update the Python/pybind11 dependencies without changing the architecture.

## Style and implementation discipline

For every modified C++ file:

- use C++17 only
- follow adjacent naming and formatting
- use existing logging macros rather than stdout/stderr ad hoc
- use RAII for ownership
- avoid raw owning pointers
- keep Python headers out of general-purpose headers where possible
- minimize preprocessor conditionals outside Python-specific translation units
- do not catch and discard exceptions without logging or translation
- do not introduce global mutable Python state beyond a tightly controlled interpreter manager
- do not reformat unrelated code
- compile with the same warning policy as adjacent targets

For CMake:

- use target_* commands rather than directory-global include/link state where practical
- prefer imported targets
- keep feature checks local and explicit
- preserve existing OFF configurations
- print enough configuration information to diagnose Python discovery
- do not install/download Python packages from CMake

For Python:

- keep pure-Python code small
- use standard library features where sufficient
- do not require pandas, scipy, torch, or similar packages for the core module
- make optional ecosystem adapters import lazily
- include type hints where they improve discoverability
- do not hide expensive native copies behind surprising property access

## Explicit non-goals for the first implementation

Do not make the first implementation responsible for all of the following at once:

- binding every C++/Ygor class method
- zero-copy writable NumPy views
- a complete Python-native replacement for .dscr scripts
- dynamic registration of arbitrary Python callables as first-class native operations
- complete Thrift schema fidelity
- self-contained Python inside every AppImage/static/cross artifact
- supporting arbitrary Python environments or package managers
- producing wheels for every OS/architecture
- replacing the existing C++ operation documentation system
- rewriting operations in Python

These can be revisited after the Session API and copy-based data exchange are stable.

## Later enhancement: Python-defined operations

The current operation registry is constructed from Known_Operations and is not a plugin registry.

After embedded Python is proven stable, a later refactor may introduce an OperationRegistry abstraction that allows Python callbacks to register operation names and OperationDoc metadata.

Do not block the initial integration on this work. A generic Python operation provides most of the practical extension benefit with far less disruption.

## Acceptance criteria

The implementation is considered complete for the first production-worthy milestone when all of the following are true:

- DICOMautomaton still builds and tests with Python completely disabled.
- A reusable C++ core target exists and is used by the Python module without duplicating application object aggregation.
- A C++17 Session/Workspace facade reuses native file loading, script parsing, operation dispatch, and documentation.
- A Python extension can construct a Session, load data, enumerate operations, run operations, and run native scripts.
- Operation failures are translated to useful Python exceptions.
- At least image data can be round-tripped through NumPy with geometry and metadata preserved.
- Ownership/lifetime semantics are documented and tested.
- Embedded Python, if included in this milestone, is separately feature-gated and tested.
- Python/pybind11 dependencies are integrated through CMake and centralized package lists without requiring pip/Poetry for the native build.
- At least one Python-enabled native CI job passes.
- At least one Python-disabled CI job passes.
- Unsupported cross/static/package targets remain explicitly Python-disabled rather than accidentally broken.
- System-operations instructions identify exactly what must be added to long-lived build images.
- User and maintainer documentation exists.
- tracker.md is updated to reflect completed work and any intentionally deferred platform support.

## Implementation order

Use this order unless repository findings justify a documented deviation:

1. establish dependency/feature switches and preserve Python-off builds
2. create the reusable core target
3. create/refactor the Session facade and native CLI use
4. add the Python extension with Session and operation introspection
5. add operation/script execution
6. add copy-based image/NumPy exchange
7. add additional data adapters incrementally
8. add Python-enabled tests and CI
9. add embedded Python operation
10. update build-base/package-manager support as platforms become ready
11. consider remote Session and Python-defined operation registration later

The implementation should prefer a small, boring, understandable integration surface over clever packaging or binding machinery. The success criterion is that a maintainer ten years from now can still identify where Python enters the build, where Python enters the process, how data crosses the boundary, and how to disable the feature without unraveling the rest of DICOMautomaton.
