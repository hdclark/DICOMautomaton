## DICOMautomaton — Guidance for AI coding agents

Be pragmatic and work from concrete, discoverable facts in the repository. This file highlights the project's architecture, developer workflows, conventions, and useful file pointers so an AI agent can be immediately productive.

Overview:
- DICOMautomaton is a set of tools with a common interface that supports medical physics.
- Functionality are organized into Operations which can be composed together to build complex data processing pipelines.
- Supported data types include radiotherapy data formats (medical images, structure sets, radiotherapy plans), surface meshes, tabular data, image deformations, geospatial traces, and one dimensional line samples.
- Supported operations are numerous, including image processing, registration deformation, geospatial data reduction amd dose accumulation, visualization, computer-aided design, physical simulation, and meta operations that support composition.
- New functionality can be added via C++ code.
- Composition and development of existing operations can be accomplished with DCMA scripts, a custom domain-specific scripting language.

Key constraints (short):
- Primary language: C++17 (large codebase in `src/`).
- Build system: CMake (top-level `CMakeLists.txt`). Many helper scripts live in `scripts/` and top-level helpers (`compile_and_install.sh`, `compile_on.sh`).inline
- Test harnesses: `integration_tests/Run.sh` provides test runners that expect a built `dicomautomaton_dispatcher` binary in PATH. Unit tests can be specified inline using `C++ doctest` and invoked after recompilation via `dicomautomaton_dispatcher -t`.

Big picture architecture (where to look):
- Core C++ code: `src/` — contains operations, file loaders, viewers, and the dispatcher (`DICOMautomaton_Dispatcher.cc`). Use `src/Documentation.cc` to find user-facing CLI examples.
- CLI entrypoint: `dicomautomaton_dispatcher` (built from `src/`) — operations are invoked via the dispatcher; many examples in `src/Documentation.cc` and the top-level `README.md`.
- Supporting libraries: the repo depends on the author's `Ygor`, `YgorClustering`, and `Explicator` projects (mentioned in `src/Documentation.cc`) and many external libs (Eigen, CGAL, Boost, SDL2, SFML, WT, nlopt, GSL, libpq, jansson, thrift). Check `CMakeLists.txt` for compile-time options (e.g., `WITH_EIGEN`, `WITH_CGAL`).
- Packaging/portable builds: Docker images and MXE tooling are under `docker/` and `docker/build_bases/`. Pre-built Docker images which can simplify dependency management, compilation, and develooment are publicly available at Docker Hub under username "hdclark". Top-level scripts create AppImages and portable bundles (`scripts/dump_portable_dcma_bundle.sh`, `scripts/extract_system_appimage.sh`).
- End-users using DICOMautomaton will often want to write and run scripts and invoke them with `dicomautomaton_dispatcher` executable rather than add or modify C++ code. There are several examples of DCMA scripts in `artifacts/`, most of which are bundled into the `dicomautomaton_dispatcher` executable. Features not available through the existing suite of operations will need to be added via C++ code and recompiled into `dicomautomaton_dispatcher`.

Developer workflows & concrete commands:
- Quick local debug build (generic, non-package-managed):
  - mkdir -p build && cd build
  - cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ..
  - make
- Distribution-aware build + install: use `./compile_and_install.sh` (supports `-d arch|debian|mxe|generic`, `-b <buildroot>`, `-i <installprefix>`). This script is the canonical way CI and developers build for different targets.
- Remote or container builds: use `./compile_on.sh user@remote` or the `docker/` builders (examples in `compile_on.sh`). This method generally requires access to a remote system capable of downloading and running a public Docker image.
- Run integration tests (after building): `integration_tests/Run.sh` — it expects `dicomautomaton_dispatcher` available and will copy test artifacts to `dcma_integration_testing`.
- Run unit tests (afrer building): `dicomautomaton_dispatcher -t`.

Project-specific conventions and patterns:
- Dispatcher & operations pattern: operations are implemented in `src/Operations/` and invoked through the dispatcher. Use `dicomautomaton_dispatcher -u` to print operation parameter descriptions (see `README.md` and `src/Documentation.cc` for examples). This documentation is comprehensive and voluminous, so it is best to filter and summarize as needed. It is built by self-describing operations, so it can be more straightforward to consult individual operations in `src/Operations/`.
- Ygor usage: many internal types and macros come from `Ygor` (e.g., `YgorMisc.h`, `YgorLog.h`, `YgorMath.h`). When editing code that manipulates core data structures (images, meshes, point clouds), inspect the Ygor headers included in the same files.
- Common classes and structs are defined in `src/Structs.cc`.
- Short routines used across the code base can be implemented as a top-level file in `src/`, for example `src/String_Parsing.cc`. Routines limited in scope to a specific domain that are only called in one context can be implemented where they are needed, for example in an Operation file.
- Routines used as kernels for image processing or analysis can be added to `src/YgorImages_Functors/`. Kernels are split according to their concurrency model and their data dimensionality (Processing, Transform, and Compute).
- CLI fuzzy selectors: name/label selectors in the dispatcher support fuzzy matching (examples in `src/Documentation.cc`). Tests and examples use patterns like `CT*dcm` to select multiple files. Operation names also use fuzzy matching.
- Build flags controlled via CMake: toggles such as `WITH_SFML`, `WITH_SDL`, `WITH_CGAL`, sanitizers (ASAN/TSAN/MSAN), and `MEMORY_CONSTRAINED_BUILD` are used widely — prefer to mirror existing CMake invocations in `compile_and_install.sh` when producing new build instructions.

Integration points & external dependencies to be aware of:
- Ygor and YgorClustering (local include/dependency assumptions). Many source files include `Ygor*.h` headers; ensure they are on the include path when building.
- GUI/viewers: SDL2 (ImGui backends) and SFML are used for interactive viewers (`src/Operations/SDL_Viewer.cc`, `SFML_Viewer.cc`). These expect OpenGL and platform-specific setup (note macOS OpenGL differences in `CMakeLists.txt`).
- Database and RPC: optional components rely on libpq/postgres and Apache Thrift (`WITH_POSTGRES`, `WITH_THRIFT`). If modifying RPC code, consult `src/rpc/` and `src/rpc/DCMA.thrift`.
- Packaging: AppImage generation and portable bundles are used in CI — see `scripts/` and `docker/` for examples; use `compile_and_install.sh` to reproduce packaging steps locally.

Concrete examples to cite when making edits:
- Add or change an operation: mirror structure in `src/Operations/HighlightROIs.cc` and register via the dispatcher (see `DICOMautomaton_Dispatcher.cc`). The implementation should also be added to `src/Operarions/CMakeLists.txt`.
- Add a new CMake option: follow existing patterns in top-level `CMakeLists.txt` (option(), find_package(), add_definitions). Keep defaults aligned with other options (ON/OFF) and add docs to `src/Documentation.cc` where appropriate.
- Fix viewer rendering issues: inspect `src/Operations/SDL_Viewer.cc` and the ImGui/SDL glue in `src/imgui20210904/` to ensure mouse/GL state handling is consistent.

Quality gates and verification steps (what AI should run):
- After code changes, run a debug build (cmake + make -j) and confirm compilation. Use the same CMake variables developers use in `compile_and_install.sh` for parity.
- Run `integration_tests/Run.sh` for quick functional validation; its scripts are self-contained and copy outputs to `dcma_integration_testing`. Run unit tests by running `dicomautomaton_dispatcher -t`.

What not to assume / common pitfalls:
- Do not assume all optional dependencies are present — `CMakeLists.txt` gates features via `WITH_*` options. Prefer to keep changes backward-compatible when a dependency is absent.
- The project is permissively modular: multiple viewers and backends exist; changing one may require adjusting compile flags or linking (SDL vs SFML).
- Avoid changing public build defaults without updating `compile_and_install.sh` and documentation in `src/Documentation.cc`.
- Do not assume specific platform support is available. The default build target system is a generic older version of Debian Linux (codename Buster) which, when built as a portable `AppImage`, provides `glibc` compatibility across nearly all modern Linux platforms. Both dynamic and static linked builds are possible too, and there are facilities to cross-compile for Windows and macOS operating systems. DICOMautomaton can also target ARM cpus and both 64 and 32 bit platforms, but not all functionality is available for all targets.
- Do not assume C++ language features beyond C++17 are available. Limiting the language version improves portability.

If you need clarification from a human: ask for
- which target platform and build flags to use (e.g., macOS vs Linux, WITH_CGAL=ON),
- whether the change should be packaged (AppImage) or installed system-wide, and
- if Ygor header/library locations need to be adjusted for CI.

Please review and tell me which section should be expanded or any missing commands you want included.
