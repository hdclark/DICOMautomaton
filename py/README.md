# DICOMautomaton Python interface

The Python module is a thin interface to DICOMautomaton's native C++ operation
dispatcher. CMake remains the authoritative build system; Poetry, pip, and an
internet connection are not required.

## Build

Install matching CPython development files and pybind11 development files, then
configure a native build:

```sh
cmake -S . -B build \
    -DWITH_PYTHON=ON \
    -DWITH_PYTHON_EXTENSION=ON \
    -DWITH_PYTHON_EMBED=OFF
cmake --build build --target _dicomautomaton dicomautomaton_dispatcher
ctest --test-dir build -R python_extension --output-on-failure
```

Python integration currently requires CMake 3.18 or newer. Python-disabled
builds retain the project's CMake 3.12 baseline. The initial supported CPython
window is 3.8 through 3.11 on native Linux builds. pybind11 2.6 or newer is
required; CPython 3.10 requires pybind11 2.7 or newer and CPython 3.11 requires
pybind11 2.10 or newer. Cross-compiled, fully static,
and AppImage configurations remain disabled or unsupported. Debian/Ubuntu builders normally
need `python3`, `python3-dev`, `pybind11-dev`, and `python3-numpy`; use the
corresponding system packages on other distributions rather than installing
build dependencies with pip.

For a build-tree import, set `PYTHONPATH` to `build/py`.
See `PLATFORM_SUPPORT.md` for the per-target support matrix and build-image
maintenance procedure.

## Session API

```python
import dicomautomaton as dcma

session = dcma.Session()
session.load(["CT001.dcm", "CT002.dcm"])
session.run("ThresholdImages", Lower="100", Upper="500")
session.run_script("NormalizePixels();")

for operation in session.operations():
    print(operation.name, operation.description)
```

`Session.load()` uses the native file loader. Scripts discovered during loading
are staged and run before the next explicitly requested operation or script.
`Session.run_many()` accepts `(operation_name, argument_dict)` pairs. Each call
consumes the complete staged batch even if an operation fails and later
operations are not attempted; submit a new batch to continue after handling the
exception.

Operation argument values may be strings, booleans, integers, or floats. They
are converted deterministically to the string-valued native operation format.
Failures raise `RuntimeError`. Native loading and operations are not
transactional, so data or metadata changed before a failure can remain changed.
On POSIX systems process-forking paths, including the `Fork` operation,
detached plotting, and portable file dialogs, are rejected after the Python
binding has initialized because a raw fork can inherit CPython locks in an
unsafe state.

`Session.metadata` returns a copy; assign the property or call `set_metadata()`
to modify native invocation metadata.

`Session.data` is a lightweight view that retains its owning external
`Session`, so keeping the view alive also keeps the native data alive. Snapshot
objects returned by the view are detached copies. Embedded-operation session
and data views have the stricter callback-only lifetime described below.

## NumPy image exchange

Image exchange is copy-only. The NumPy axis order is `(row, column, channel)`,
including the channel axis for single-channel images. Arrays use `float32` and
C-contiguous storage. Native image collections can contain images with different
shapes or geometry, so images are addressed individually:

```python
image = session.data.get_image(array_index=0, image_index=0)
pixels = image.to_numpy()
pixels *= 2.0
session.data.set_image_pixels(0, 0, pixels)
```

`ImageSnapshot` is a detached deep copy. Its `to_numpy()` method creates another
copy, so neither object observes later native operations. `set_image_pixels()`
resolves the requested native image at call time, requires the exact existing
shape, copies the values, and preserves native geometry and metadata. It accepts
values convertible to `float32`; conversion completes before native pixels are
changed. No writable native buffer views are exposed.

`ImageSnapshot.spacing` is `(column_spacing, row_spacing, slice_thickness)`.
`row_direction` points along increasing column indices and `column_direction`
points along increasing row indices. The centre of pixel `(0, 0)` is
`anchor + offset`. Metadata is returned as an exact copied `dict[str, str]`.

## NumPy contour exchange

Contours retain their native collection grouping and are addressed by collection
and contour index. `ContourSnapshot.to_numpy()` returns a detached C-contiguous
`float64` array with shape `(point, xyz)`. Closed/open state and metadata are
copied explicitly:

```python
contour = session.data.get_contour(collection_index=0, contour_index=0)
points = contour.to_numpy()
points[:, 2] += 5.0
session.data.set_contour(0, 0, points, contour.closed, contour.metadata)
```

`set_contour()` replaces one contour in its existing collection after all input
conversion succeeds. It copies points, closed/open state, and `dict[str, str]`
metadata; snapshots do not observe later native changes. No native contour
buffer views are exposed.

## NumPy point-cloud exchange

`PointCloudSnapshot` exposes detached `float64` `(point, xyz)` arrays for points
and normals, a `uint32` `(point,)` array for packed RGBA colours, and copied
string metadata. Optional normals and colours are represented by empty arrays.
Use `set_point_cloud()` to atomically copy all four components back; normals and
colours must be empty or have the same length as points. Snapshots and arrays do
not alias native storage.

## NumPy surface-mesh exchange

`SurfaceMeshSnapshot` exposes detached `float64` `(vertex, xyz)` arrays for
vertices and optional normals, a `uint32` `(vertex,)` array for optional packed
RGBA colours, copied string metadata, and a copied list of variable-length face
index lists. Face winding and polygon arity are preserved.

`set_surface_mesh()` validates all arrays and zero-based face indices, rebuilds
the native vertex-to-face adjacency index, and swaps the complete replacement
into the selected mesh only after conversion succeeds. Normals and colours must
be empty or match the vertex count. Meshes carrying untyped native runtime
attributes are rejected because those attributes cannot be safely remapped;
arbitrary `std::any` attributes are not exposed. No native mesh storage is
shared with Python.

## RT-plan exchange

`RTPlanSnapshot` contains detached `DynamicMachineStateSnapshot` beam values,
which in turn contain detached `StaticMachineStateSnapshot` control points.
All plan, beam, and control-point metadata is copied exactly. Jaw and MLC
positions are copied one-dimensional `float64` arrays; the isocentre is an
`(x, y, z)` tuple. Numeric NaNs are preserved because the native model uses
them for unavailable or inherited machine-state values.

`set_rtplan()` accepts a complete list of beam dictionaries and plan metadata.
Each beam dictionary must provide `beam_number`,
`final_cumulative_meterset_weight`, `static_states`, and `metadata`. Each
control-point dictionary must provide every property exposed by
`StaticMachineStateSnapshot`, using the same snake-case names. This explicit
complete-replacement format distinguishes NaN and empty-vector values from
fields that were accidentally omitted.

The entire nested replacement is converted before the selected native plan is
changed. Copy-back preserves beam and control-point order, metadata strings,
NaNs, and the existing native plan object's shared identity. It does not sort,
normalize, or otherwise reinterpret treatment-machine state. Snapshots and
their arrays never alias native storage.

## NumPy line-sample exchange

Line samples use a detached C-contiguous `float64` array with columns
`(x, sigma_x, f, sigma_f)`. `LineSampleSnapshot` also copies the native
uncertainty-independence flag and string metadata. `set_line_sample()` validates
and converts all input before replacing the native samples, flag, and metadata;
no native storage is exposed.

## Sparse-table exchange

Sparse tables use a dependency-free list of `(row, column, value)` tuples, with
signed integer coordinates and string values. This representation preserves
holes and distinguishes a missing cell from a stored empty string. Cells are
returned in deterministic row-major order:

```python
table = session.data.get_table(0)
cells = list(table.cells)
cells.append((10, 4, "new value"))
session.data.set_table(0, cells, table.metadata)
```

`SparseTableSnapshot` and its metadata are detached copies. `set_table()`
validates and copies the complete replacement before changing the existing
native table. Duplicate coordinates are rejected. pandas is not required; a
caller may build a DataFrame from `cells` when pandas is available.

## Transform exchange

Transforms use a detached `TransformSnapshot` with an explicit `kind`, copied
transform-level `metadata`, and a variant-specific `payload`. Supported kinds
are `disengaged`, `affine`, `thin_plate_spline`, and `deformation_field`.
`DataView.set_transform()` validates and constructs a complete replacement
before changing the selected native transform; no native buffers are exposed.

Affine payloads contain a C-contiguous `float64` `matrix` with shape `(4, 4)`.
The homogeneous bottom row must be exactly `(0, 0, 0, 1)`.

Thin-plate-spline payloads contain `control_points` and optional
`control_point_normals` as `(point, xyz)` `float64` arrays, optional packed
`uint32` `control_point_colours`, copied `control_point_metadata`, a
`kernel_dimension` of 2 or 3, and a `float64` `coefficients` matrix with shape
`(control_point_count + 4, 3)`. These fields preserve the complete native point
set rather than the smaller native text-serialization subset.

Deformation-field payloads contain an `images` list. Each item stores
`float64` displacement `values` in `(row, column, [dx, dy, dz])` order together
with spacing, anchor, offset, row and column directions, and per-plane metadata.
The native constructor verifies that the non-empty image set is a regular grid.
The derived native adjacency index is rebuilt rather than serialized.

Setting a `disengaged` transform requires an empty payload but preserves its
transform-level metadata. Transform snapshots and every array returned through
their payloads are copies. Transform RPC serialization remains unsupported and
is separate from this in-process API.

## Embedded Python operation

Native dynamic builds can enable the `Python` operation separately from the
extension:

```sh
cmake -S . -B build \
    -DWITH_PYTHON=ON \
    -DWITH_PYTHON_EXTENSION=OFF \
    -DWITH_PYTHON_EMBED=ON
cmake --build build --target dicomautomaton_dispatcher
ctest --test-dir build -R python_embed --output-on-failure
```

An embed-only configuration does not require NumPy at configure time. Calling
one of the NumPy array adapters from an embedded callback still requires NumPy
to be importable by the embedded interpreter.

`Python` (alias `PythonScript`) executes a source file and calls `process(session)`
by default:

```text
Python(Filename="algorithm.py", Function="process");
```

The callback receives an operation-scoped session with `data`, `metadata`,
`set_metadata()`, and read-only `lexicon_filename`. Its `data` property uses the
same copy-based adapters as the external extension. The session and any retained
`DataView` become invalid when the callback returns; later access raises an
exception. Native and Python mutations are non-transactional.

The interpreter is initialized lazily once per process and is not finalized
during normal operation. Callbacks execute on the dispatch thread with the GIL.
Embedded callbacks are serialized because their explicit module search paths
temporarily replace the process-global `sys.path`.
The operation does not run pip, modify a Python installation, or automatically
add the working directory or script directory to `sys.path`. Explicit additional
directories can be passed as a semicolon-separated `ModuleSearchPaths` value;
the original search path is restored after the callback. Isolated interpreter
configuration ignores environment-driven and user-site path injection.

Embedded Python executes arbitrary code with the application's privileges. Only
run trusted scripts. Fully static, cross-compiled, and relocatable bundle builds
remain unsupported because a matching interpreter, standard library, and binary
module environment must be supplied and tested together.

## Maintainer notes

CPython is discovered exclusively with CMake's `FindPython3`. To change the
supported Python window, first update a native Python-enabled CI image and test
the extension against matching interpreter and development files. Do not add
host-Python discovery to cross builds.

pybind11 is discovered as a system CMake package first. Its optional
`FetchContent` fallback is declared in the top-level `CMakeLists.txt`; update it
only to a reviewed immutable commit and verify compatibility with the complete
supported Python window. Do not replace this process with `pip install` in the
native build.

The install destination defaults to the selected interpreter's prefix-relative
module directory, such as `lib/pythonX.Y/site-packages/dicomautomaton` or a
distribution's `dist-packages` equivalent. Set `DCMA_PYTHON_INSTALL_DIR`
explicitly when staging for a different interpreter layout.
