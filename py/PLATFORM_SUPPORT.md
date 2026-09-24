# Python platform support

Python support is opt-in. CMake builds without Python remain the default.

## Current classification

| Target | Extension | Embedded | Policy |
| --- | --- | --- | --- |
| Native Debian Bookworm x86-64 | supported | supported | CPython 3.11; extension and embed GitHub CI job configured |
| Native Debian Bullseye x86-64 | pending | pending | CPython 3.9 packages are defined, but no Python-enabled build/test job exists yet |
| Manual native Linux with CPython 3.8-3.11 | supported | supported | requires matching system development packages and a dynamic executable build |
| Ubuntu quick build and AppImage | disabled | disabled | generic packaging keeps Python off; no relocatable interpreter design |
| Debian Stretch/Buster builders and AppImage | disabled | disabled | available CPython versions are outside the supported window |
| Arch rolling native/SYCL builders and AppImage | disabled | disabled | distribution Python can exceed the supported window; package builds explicitly keep Python off |
| Void native builder | pending | disabled | package names and interpreter version need verification; generic packaging keeps Python off |
| Fedora package mappings | pending | disabled | no maintained Python-enabled builder or matching package verification |
| MXE Windows cross-build | disabled | disabled | host Python is not a target Windows runtime |
| Fully static Alpine builds | disabled | disabled | extension modules are dynamically loaded |
| Emscripten/WebAssembly cross-build | disabled | disabled | no target CPython runtime or extension ABI integration |
| AppImage and other relocatable bundles | disabled | disabled | no interpreter, standard library, or relocation design |
| macOS builder | pending | disabled | existing CI definition is inactive and unvalidated |

Python is explicitly disabled in the generic packaging script, MXE and
WebAssembly cross-builds, static Alpine builder, AppImage paths, and Arch
`PKGBUILD`. The existing GitLab Buster/Arch jobs and GitHub Ubuntu quick build
therefore remain Python-disabled. Users building a supported native extension
or embedded dispatcher should invoke CMake directly as documented in
`py/README.md`.

## Build-base operations

`scripts/get_packages.sh` exposes a `python_extension` tier for Debian Bullseye
and Bookworm. It installs the interpreter, matching development and embedding
files, pybind11, and NumPy. The corresponding build-base implementation scripts
consume that tier.

After changing those package lists, operations staff should:

1. Build the relevant image with its `docker/build_bases/<distribution>/build.sh` script.
2. Run a Python-enabled native build and the `python_extension` CTest in the image.
3. Record the image tag or digest, build date, CMake version, CPython version, and pybind11 version in the release/build log.
4. Push the validated image using the repository's normal image publication process.

Do not add Python packages to MXE or static Alpine images. Do not enable Python
inside AppImage assembly until a complete matching interpreter and standard
library can be relocated and tested together.
