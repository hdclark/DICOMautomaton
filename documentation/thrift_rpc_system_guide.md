# Apache Thrift RPC system guide for LLM extensions

## Purpose

This document summarizes the current Apache Thrift RPC implementation in DICOMautomaton and highlights what would need to change to make the RPC layer genuinely useful for remote execution, processing offload, and distributed computation.

The intended audience is an LLM or developer extending the current work-in-progress implementation.

## Relevant files

- `src/rpc/DCMA.thrift`
- `src/rpc/Serialization.{h,cc}`
- `src/rpc/compile_thrift.sh`
- `src/rpc/gen-cpp/*`
- `src/Operations/RPCReceive.{h,cc}`
- `src/Operations/RPCSend.{h,cc}`
- `src/Structs.{h,cc}`
- `src/Tables.h`
- `src/Operation_Dispatcher.{h,cc}`
- `documentation/wip_zeromq_network_protocol.md`

## Executive summary

The current RPC system is real, but still alpha-quality:

- Apache Thrift is the active RPC mechanism in code.
- The server exposes three Thrift methods: `GetSupportedOperations`, `LoadFiles`, and `ExecuteScript`.
- Only `GetSupportedOperations` and `ExecuteScript` are implemented.
- The in-tree client operation `RPCSend` does **not** expose general remote execution yet; it enumerates operations and then sends a hard-coded `noop();` script.
- The data model is only partially marshalled:
  - most `Drover` payload types work,
  - `Surface_Mesh` drops run-time attributes,
  - `Transform3` is completely unsupported and causes hard failure,
  - some scalar types are widened or signedness-shifted to fit Thrift.
- The current design is enough for experimentation, but not yet enough for robust offload, service discovery, retries, scheduling, checkpointing, or distributed fan-out.

## Current architecture

### Build-time structure

- Thrift support is compiled only when `WITH_THRIFT` is enabled in the top-level `CMakeLists.txt`.
- `src/CMakeLists.txt` adds `src/rpc/` only when Thrift support is enabled.
- `src/rpc/CMakeLists.txt` builds committed generated files from `src/rpc/gen-cpp/` together with `Serialization.cc`.
- `src/rpc/compile_thrift.sh` regenerates bindings and ancillary outputs, but regeneration is manual and interactive rather than integrated into the normal build.

This means the RPC schema has three separate synchronization points:

1. native C++ data structures in `src/Structs.h` and `src/Tables.h`,
2. the transport schema in `src/rpc/DCMA.thrift`,
3. the hand-written conversion logic in `src/rpc/Serialization.cc`.

Any change to one of these layers can silently invalidate the others unless it is updated everywhere.

### Runtime structure

The active service is `Receiver` in `src/rpc/DCMA.thrift`.

`RPCReceive`:

- creates a `ReceiverHandler`,
- binds a `TServerSocket`,
- uses `TBufferedTransport` and `TBinaryProtocol`,
- runs a blocking `TSimpleServer`.

`RPCSend`:

- opens a `TSocket` to `Host:Port`,
- wraps it in `TBufferedTransport`,
- uses `TBinaryProtocol`,
- instantiates `dcma::rpc::ReceiverClient`.

### Effective call flow today

The current in-repo flow is:

1. launch a server with the `RPCReceive` operation,
2. connect with the `RPCSend` operation,
3. enumerate supported operations with `GetSupportedOperations`,
4. serialize a `Drover`, invocation metadata, and filename lexicon state,
5. send them to `ExecuteScript`,
6. run a hard-coded `noop();` remotely,
7. deserialize the returned state.

So, although the Thrift service can in principle run arbitrary scripts, the shipped `RPCSend` operation currently behaves like a connectivity and round-trip test more than a general-purpose remote execution client.

## What the Thrift methods currently do

### `GetSupportedOperations`

Implemented in `src/Operations/RPCReceive.cc`.

Current behavior:

- performs several local serialization smoke tests,
- calls `Known_Operations_and_Aliases()`,
- returns all known operation names and aliases.

Implications:

- the server advertises the entire dispatcher surface, not a curated RPC-safe subset,
- there is currently no capability model beyond a flat list of names,
- there is no distinction between safe, expensive, interactive, local-filesystem-only, or recursively networked operations.

### `LoadFiles`

Declared in `src/rpc/DCMA.thrift`, but effectively a stub in `src/Operations/RPCReceive.cc`.

Current behavior:

- logs invocation,
- returns without populating the response.

Implications:

- there is no usable remote file-loading workflow,
- the `LoadFilesQuery`/`LoadFilesResponse` types exist in the schema but do not yet correspond to working behavior,
- clients cannot rely on server-side file discovery or server-local dataset loading.

### `ExecuteScript`

Implemented in `src/Operations/RPCReceive.cc`.

Current behavior:

- deserializes `Drover`, invocation metadata, filename lex state, and the script text,
- parses the script with `Load_DCMA_Script`,
- executes it through `Operation_Dispatcher`,
- serializes the resulting state back to the client.

Implications:

- this is the only current path that can actually invoke DICOMautomaton functionality remotely,
- the remote execution scope is the dispatcher itself, not a purpose-built RPC API,
- once a custom client can provide arbitrary script text, the remote surface becomes “any operation the server build exposes”.

### Scope of `RPCReceive` and `RPCSend`

There are two different scopes to keep straight:

1. **Thrift service scope**  
   The `Receiver` service exposes:
   - `GetSupportedOperations`
   - `LoadFiles`
   - `ExecuteScript`

2. **Built-in operation scope**  
   The DICOMautomaton operations `RPCReceive` and `RPCSend` expose only a thin wrapper around that service:
   - `RPCReceive` starts the server and blocks forever in `server.serve()`.
   - `RPCSend` only takes `Host` and `Port`; it does not accept a script, operation list, retry policy, timeout policy, or endpoint-discovery input.
   - `RPCSend` currently invokes `GetSupportedOperations` and then `ExecuteScript(..., "noop();")`.
   - `RPCSend` does not invoke `LoadFiles`.

As a result, general remote execution is present in the protocol but not yet surfaced as a useful first-class DICOMautomaton operation.

## Source of truth: native C++ classes versus `DCMA.thrift`

For transport payloads, the native C++ structures in `src/Structs.h` and `src/Tables.h` should be treated as the semantic source of truth. `DCMA.thrift` is a transport projection of those structures, not the authority on what the data means.

`src/rpc/Serialization.cc` is the actual contract-enforcement layer because it decides what is preserved, approximated, flattened, dropped, or rejected.

## Marshalling coverage

### Fully or mostly represented payloads

| Native type | Thrift type | Current status | Notes |
| --- | --- | --- | --- |
| `vec3<double>` | `vec3_double` | Supported | Direct field mapping. |
| `contour_of_points<double>` | `contour_of_points_double` | Supported | Points, closed flag, metadata preserved. |
| `contour_collection<double>` | `contour_collection_double` | Supported | Contours preserved. |
| `point_set<double>` | `point_set_double` | Mostly supported | Colour type is coerced to signed `i64`. |
| `samples_1D<double>` | `samples_1D_double` | Supported | Samples and metadata preserved. |
| `fv_surface_mesh<double,uint64_t>` | `fv_surface_mesh_double_int64` | Mostly supported | Indices and colours are coerced to `i64`. |
| `planar_image<float,double>` | `planar_image_double_double` | Mostly supported | Pixel data widened from `float` to `double`. |
| `planar_image_collection<float,double>` | `planar_image_collection_double_double` | Supported | Collection preserved. |
| `tables::table2` | `table2` | Supported | Sparse cell representation is preserved. |
| `Contour_Data` | `Contour_Data` | Supported | `ccs` preserved. |
| `Image_Array` | `Image_Array` | Supported | Image collection plus filename preserved. |
| `Point_Cloud` | `Point_Cloud` | Supported | Point set preserved. |
| `Static_Machine_State` | `Static_Machine_State` | Supported | Current exposed fields are preserved. |
| `Dynamic_Machine_State` | `Dynamic_Machine_State` | Supported | Beam state list and metadata preserved. |
| `RTPlan` | `RTPlan` | Supported | Dynamic states and metadata preserved. |
| `Line_Sample` | `Line_Sample` | Supported | Sample line preserved. |
| `Sparse_Table` | `Sparse_Table` | Supported | Table preserved. |

### Partially represented or mismatched payloads

| Native type | Thrift type | Current status | Problem |
| --- | --- | --- | --- |
| `Surface_Mesh` | `Surface_Mesh` | Partial | `vertex_attributes` and `face_attributes` exist in C++ but are omitted on the wire. |
| `Drover::contour_data` | `optional list<Contour_Data>` | Partial/mismatched | C++ currently holds one `shared_ptr<Contour_Data>`, but Thrift already models a list in anticipation of a future shape. Serialization flattens incoming list entries into one native object. |

### Unsupported payloads

| Native type | Thrift type | Current status | Problem |
| --- | --- | --- | --- |
| `Transform3` | `Transform3` | Unsupported | The Thrift struct is an empty placeholder and the serializer throws at runtime. |
| `Drover::trans_data` | `optional list<Transform3>` | Unsupported | Any attempt to serialize or deserialize transform data throws. |

## Important mismatches and edge cases

### 1. `Transform3` is a schema placeholder, not a working transport type

`Transform3` in native code is a `std::variant` over:

- `std::monostate`
- `affine_transform<double>`
- `thin_plate_spline`
- `deformation_field`

The Thrift version is still an empty shell with TODO comments. The serializer therefore refuses to continue.

Consequences:

- any `Drover` containing transforms is not RPC-safe,
- remote registration and warp workflows are blocked,
- distributed processing cannot yet operate over one of the most valuable medical-physics data types in the project.

### 2. `Surface_Mesh` silently loses run-time attributes

Native `Surface_Mesh` contains:

- `meshes`
- `vertex_attributes`
- `face_attributes`

Only `meshes` is transported. The serializer emits warnings when attributes are present, but still drops them.

Consequences:

- run-time annotations do not survive a round trip,
- remote mesh-processing results may be semantically incomplete even when the transfer “succeeds”.

### 3. `Drover` already diverges between native and Thrift cardinality

Native `Drover` currently stores:

- one `shared_ptr<Contour_Data>` for contours,
- lists of shared pointers for the other categories.

The Thrift `Drover` already uses `optional list<Contour_Data>` for contours “in anticipation of future change”.

Consequences:

- the transport schema and native container shape are already out of sync,
- deserialization has to flatten several RPC contour payloads into one native `Contour_Data`,
- the current wire format is slightly more future-facing than the native container layout.

### 4. Transport types are approximate for several numeric fields

Current approximations include:

- `float` → `double`
- `uint32_t` → `i64`
- `uint64_t` → `i64`

Consequences:

- larger payload size for images,
- signedness/range ambiguity for colours and mesh indices,
- no exact statement in the schema about the intended domain constraints.

### 5. Existing documentation does not match the current transport reality

The repository already contains `documentation/wip_zeromq_network_protocol.md`, which describes a ZeroMQ-based distributed-computing design. The currently implemented networking path in code is Apache Thrift, not ZeroMQ.

This is not necessarily wrong—the ZeroMQ document may describe a future or alternate design—but it means the existing documentation is not a faithful description of the RPC system that actually ships today.

### 6. Error handling is too weak for automation

Current behavior is fragile:

- the service does not define Thrift exceptions,
- `ExecuteScript` returns only a boolean success flag,
- parsing and execution diagnostics are logged server-side rather than transported structurally,
- `RPCSend` catches `std::exception`, logs a warning, and still returns `true`,
- the client ignores returned payloads when the script is unsuccessful,
- there are no retry, backoff, timeout, or idempotency controls.

Consequences:

- higher-level orchestration cannot reason about failures well,
- transient and permanent faults are not distinguished,
- client code cannot reliably decide whether to retry.

### 7. The server is single-threaded and blocking

`RPCReceive` uses `TSimpleServer`.

Consequences:

- one slow request can block all others,
- there is no built-in worker-pool behavior,
- the operation is awkward to use inside larger pipelines because it blocks in `server.serve()`.

### 8. RPC capability advertisement is too shallow

`GetSupportedOperations` reports names only.

Missing information includes:

- operation argument schemas,
- data requirements,
- whether an operation is interactive,
- whether it touches local files,
- whether it is deterministic or resumable,
- whether it is safe to run remotely,
- estimated resource needs.

Without this, endpoint discovery and scheduling cannot make informed decisions.

## What is currently RPC-safe

The safest current remote-execution pattern is:

- use a `Drover` containing contours, images, point clouds, meshes without extra attributes, RT plans, line samples, and sparse tables,
- avoid `Transform3`,
- avoid expecting server-local file loading,
- use a custom Thrift client if arbitrary remote script execution is needed.

The built-in `RPCSend` operation is currently best viewed as a transport probe and round-trip smoke test, not a complete distributed-computing frontend.

## What kinds of DICOMautomaton computation fit RPC well

The best RPC candidates are coarse-grained, non-interactive, partitionable jobs with small coordination cost relative to compute cost.

Strong candidates:

- registration, warping, and image-analysis batches once `Transform3` is supported,
- parameter sweeps and optimization sub-problems,
- per-image, per-ROI, per-plan, or per-beam fan-out workloads,
- expensive mesh generation or analysis,
- long-running numerical routines with checkpointable intermediate state.

Poor candidates:

- interactive viewer operations,
- operations tightly coupled to local UI or local filesystem assumptions,
- very small jobs where serialization dominates,
- operations that depend on shared mutable process state.

## Extension opportunities

### Near-term hardening

1. **Finish `LoadFiles`**  
   Make server-local loading work, or remove the method until semantics are clear.

2. **Make `RPCSend` useful**  
   Add arguments for:
   - script text or script file,
   - timeout,
   - retry count / retry policy,
   - whether to ship a `Drover`, request a returned `Drover`, or both.

3. **Transport structured failures**  
   Add Thrift exceptions or a richer result envelope:
   - parse failure,
   - execution failure,
   - unsupported data type,
   - temporary overload,
   - authentication / authorization failure.

4. **Add exact schema-sync tests**  
   The serializer already uses layout checks for some classes; add dedicated tests that fail whenever:
   - a native field is added but not represented in Thrift,
   - a Thrift field is added but not serialized,
   - a conversion becomes lossy in an untracked way.

5. **Make unsupported data explicit**  
   Do not silently drop mesh attributes. Either:
   - serialize them correctly, or
   - return a structured “unsupported feature” error.

### Medium-term API redesign

The current RPC service is script-centric. That is flexible, but not ideal for scheduling or introspection. It would help to separate several layers.

Recommended layers:

1. **Control plane**
   - endpoint registration,
   - capability advertisement,
   - liveness/heartbeat,
   - load reporting,
   - worker versioning.

2. **Data plane**
   - `Drover` transfer,
   - artifact transfer,
   - chunking/compression,
   - optional content-addressed caching.

3. **Execution plane**
   - submit job,
   - poll status,
   - cancel job,
   - fetch result,
   - fetch logs,
   - resume from checkpoint.

4. **Typed RPC plane**
   - high-value operations exposed as explicit RPCs where that improves safety and performance.

This allows the system to keep script-based execution for flexibility while still creating typed, schedulable, observable jobs.

### Align code structure with RPC-friendly computation

RPC works best when work units are explicit, serializable, and side-effect-bounded.

That suggests extending the codebase toward:

- explicit job descriptors,
- deterministic partitioning of `Drover` payloads,
- explicit result-merging policies,
- pure or mostly-pure compute kernels,
- fewer hidden dependencies on process-global state or local filesystem context.

Existing DICOMautomaton patterns already point in this direction:

- `Operation_Dispatcher` provides a scriptable execution core,
- `For`, `ForEachDistinct`, `ForEachRTPlan`, `Fork`, `Repeat`, and `Transaction` express structured units of work,
- `Drover` already acts as a transportable bundle of domain data.

An RPC-oriented redesign should preserve those strengths while lifting them into a job/scheduler model.

### Offload to remote high-end or cloud servers

A practical first distributed-computing target is selective offload:

- run lightweight orchestration locally,
- ship large compute-heavy partitions to high-end servers,
- bring back only reduced results or a transformed `Drover`.

Useful enabling components:

- endpoint registry or worker catalog,
- worker capability metadata (CPU, RAM, GPU, installed optional features, supported operations),
- transfer policy (ship raw `Drover`, server-local file references, or cached object IDs),
- cost model to decide whether offload is worth the serialization cost,
- optional compression and chunking for large image payloads.

### Fan-out distributed computation

DICOMautomaton is already rich in operations that naturally decompose:

- split by image,
- split by ROI,
- split by plan/beam/control-point subset,
- split by parameter range for optimization/search,
- split by time window for longitudinal analysis.

To support this well, add:

- a partitioner interface,
- a merger/reducer interface,
- an abstract execution backend,
- a thread-pool-backed local backend,
- a network-backed RPC backend,
- job provenance and reproducibility metadata.

A good long-term abstraction is a common “executor” interface with at least two implementations:

- **local executor** using threads/processes,
- **remote executor** using RPC.

That lets high-level operations fan out work without caring whether the work is local or remote.

### Checkpointing, persistence, and scheduled background work

Long searches and optimization runs should be able to pause and resume. DICOMautomaton already has nearby building blocks:

- `Drover` deep-copy and duplication facilities,
- `Drover_Cache` in `src/Structs.h`,
- existing Boost-based Drover serialization elsewhere in the codebase.

This suggests a roadmap for time-shifted computation:

1. represent a job as:
   - input state,
   - execution plan,
   - partial results,
   - merge policy,
   - progress metadata;
2. persist snapshots periodically;
3. resume from the latest durable checkpoint;
4. terminate cleanly on a schedule or low-priority resource budget;
5. continue later on the same or a different worker.

This is especially attractive for:

- exhaustive search,
- coarse-to-fine optimization,
- large registration batches,
- opportunistic overnight or low-utilization execution.

## Components that would materially improve the RPC system

The following components would make the current implementation much more useful.

### 1. Schema discipline

- authoritative schema map from native classes to Thrift types,
- tests for round-trip integrity,
- automated regeneration and validation of Thrift bindings,
- explicit versioning for the wire schema.

### 2. Execution abstraction

- abstract executor interface,
- local thread-pool executor,
- remote RPC executor,
- cancellation and timeout support.

### 3. Scheduling and discovery

- worker registry,
- capability advertisement beyond names,
- heartbeat and health model,
- resource-aware scheduler,
- queueing and backpressure.

### 4. Data movement

- payload compression,
- chunked transfer for large `Drover` objects,
- optional object store / content-addressed cache,
- policy for “ship data” versus “ship server-local reference”.

### 5. Reliability

- structured errors,
- retries with backoff,
- idempotency keys,
- resumable jobs,
- persisted logs and job metadata.

### 6. Security and deployment

- authentication,
- authorization / allowlists,
- transport security,
- remote-operation policy separating safe and unsafe operations,
- support for background daemon deployment rather than only a blocking foreground operation.

## Suggested implementation order

If extending the current Thrift path rather than replacing it, the most useful order is:

1. make `RPCSend` able to submit an arbitrary script and return structured errors,
2. complete `LoadFiles` or remove it from the public surface until implemented,
3. support `Transform3`,
4. stop dropping `Surface_Mesh` attributes,
5. add capability metadata, timeouts, retries, and health checks,
6. add a local/remote executor abstraction,
7. add checkpointable job submission and status polling,
8. add scheduling, worker discovery, and fan-out/fan-in orchestration.

## Bottom line

The current Thrift RPC layer is a useful seed, but it is not yet a complete distributed-computing substrate.

Today it is best understood as:

- a working transport experiment,
- a mostly-complete `Drover` marshaller for several major data types,
- a remote script-execution hook,
- and a foundation that still needs explicit execution abstractions, reliability features, schema discipline, and scheduling infrastructure before it can support serious distributed or cloud-backed computation.
