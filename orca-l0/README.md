# Orca L0

A small inference compute runtime using Intel's Level Zero C API directly.
Linux is the primary deployment target; Windows is a compatibility target.
The host library uses C++17 and `ze_loader`; it does not link SYCL, Unified
Runtime, oneMKL or the OpenCL runtime. `ocloc` is needed only to build the check
kernels. The Intel GPU driver and its compiler are still required.

`orca-l0/` builds on its own and can be copied into a separate repository.
Its CMake target is `Orca::L0`, and its C++ namespace is `orca::l0`.
The source is covered by the included MIT license.

## Build and check

Linux, with Level Zero development files, an Intel GPU driver and `ocloc`:

```sh
cmake -S orca-l0 -B build-orca-l0 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-orca-l0
ctest --test-dir build-orca-l0 --output-on-failure
```

Windows, from an x64 Visual Studio developer shell:

```powershell
cmake -S orca-l0 -B build-orca-l0 -G Ninja -DCMAKE_BUILD_TYPE=Release `
  '-DORCA_L0_SDK=C:/Program Files/LevelZeroSDK/1.33.1' `
  '-DORCA_L0_OCLOC=C:/Program Files (x86)/Intel/oneAPI/ocloc/2025.3/bin/ocloc.exe'
cmake --build build-orca-l0
ctest --test-dir build-orca-l0 --output-on-failure
```

`ORCA_L0_DEVICE` defaults to `dg2` for the A770 check. Set it to an `ocloc`
target supported by your installation for another GPU. The checks load both
the generated SPIR-V and a device-specific native binary through
`zeModuleCreate` (`ZE_MODULE_FORMAT_NATIVE` for the latter).
Use `ORCA_L0_BUILD_CHECKS=OFF` to build only the host library, without `ocloc`.
Use `ORCA_L0_BUILD_GPU_CHECKS=OFF` to compile/link the check harness and run
the CPU ring check on a machine without a GPU or `ocloc`.
The library can be consumed with `add_subdirectory(orca-l0)` and
`target_link_libraries(your_target PRIVATE Orca::L0)`.

The ring also has a standalone CPU check, with no Intel tools, loader or GPU:

```sh
cmake -S orca-l0 -B build-orca-ring -G Ninja -DORCA_L0_BUILD_RUNTIME=OFF
cmake --build build-orca-ring
ctest --test-dir build-orca-ring --output-on-failure
```

The Linux GitHub workflow builds and links the check harness against Ubuntu
24.04's Level Zero development files and runs the CPU ring check. It does not claim
GPU coverage on a hosted runner. When extracting this directory into a
repository, also copy `.github/workflows/orca-l0.yml` and adjust its source
paths. Native Linux GPU parity and full-model performance need an Intel GPU
runner; Windows measurements cannot substitute for them.

Coordinate GPU checks and builds with any existing benchmarks. In the Strata
workspace, use the existing `tools.bench_deepinfer.gpu_lock` lease.

`replay(immediate_or_queue, count, closed_lists)` submits a ready array of native
command lists in one driver call. It allocates no memory and performs no host
wait. The original single-list overloads remain available. Keep the lists and
their resources alive until native completion, and express dependencies between
lists explicitly. Independent lists with disjoint outputs need no added
dependency. This entry point does not perform model-request aggregation.

`Runtime::properties()`, `driver_properties()` and `extensions()` return
immutable snapshots owned by that Runtime. Driver properties and extension
enumeration run at construction, including the capability list used for large
allocations. Create a new Runtime to rediscover a device/driver. Kernel ABI and
module lookups remain cached by `compiler::Program`; completion queries still
read current driver state.

The optional `orca-l0-submit-check` target compares queue/fence/immediate/event
completion, DDI function-table calls, and batched versus individual submissions:

```sh
cmake --build build-orca-l0 --target orca-l0-submit-check
./build-orca-l0/orca-l0-submit-check build-orca-l0/inference.spv
./build-orca-l0/orca-l0-submit-check build-orca-l0/inference.bin native
```

The supported API passed three fresh Windows A770 processes on 2026-10-31:
2916 exact GPU-tail windows and 1728 complete GPU batches, including invalid
input rejection, distinct inputs and output guards. Two processes used SPIR-V;
one loaded the native binary directly. For 32 independent one-kernel lists,
individual appends with one final wait took 267.6--288.9 us of host submission
time; one batch append took 30.5--30.6 us. Complete execution/readback windows
fell from 1633.8--1689.3 us to 383.35--384.9 us. The paired window speedups were
4.33x/4.42x; the native-binary process measured 5.00x. With 16 kernels per list,
the three process speedups were 1.83x--2.15x. These are standalone ready-list
batches, not full-model throughput. The 400-kernel single-list DDI comparison
changed complete windows by less than 0.5%; small-list samples varied more.
Every sample and the prior raw-C-API prototype are retained in the
[API submission report](demo/api-submission-a770-windows-2026-10-31.json).
Linux GPU results and a hardware concurrency timeline remain pending.

`ORCA_L0_POISON_ALLOCATIONS=1` fills each new device, host and shared allocation
with `0xff` bytes before returning it. This is a NaN pattern for the common
floating-point types and exposes reads before initialization. Device fills use
a separate immediate list and wait once per allocation; it adds no work to
later kernel submissions. The diagnostic is off by default and invalidates
timing measurements. `orca-l0-poison` checks every byte, including an odd-sized
tail, for all three memory kinds and reruns the inference check with it enabled.

## Reusing compiled GPU kernels

The optional `compiler/` project uses the oneAPI compiler and SYCL headers
to compile existing device code. Its host executable submits directly through
Orca and links without SYCL, UR or oneMKL. It requires C++20; the base runtime
still requires only C++17.

```sh
cmake -S orca-l0/compiler -B build-orca-compiler -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=icpx
cmake --build build-orca-compiler
ctest --test-dir build-orca-compiler --output-on-failure
```

Use the Windows compiler and SDK paths from the examples above on Windows.
`compiler/build.py` emits a kernel image pack and index through device-only
compilation, `sycl-post-link` and `llvm-spirv`. `compiler::Program` loads those
images with Level Zero and binds arguments using the compiler's capture
descriptors. Multiple translation-unit packs can be added to one program.
`compiler::Stream` provides native in-order eager submission and recording/replay
on the same context, allocations and stream.

For explicitly scheduled graphs, `runtime.commands(Ordering::relaxed)` and
`stream.begin_recording(Ordering::relaxed)` request a regular Level Zero list
with `ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING`. The caller supplies barriers or
event dependencies before a command reads another command's outputs, and before
scratch or state is overwritten. Ordinary stream recording remains in order.
The driver decides whether ready kernels execute concurrently; the flag does
not create another compute engine or make unknown future inputs available.

`build.py --large-grf-kernel='<kernel-name-glob>'` records the Level Zero
`-ze-opt-large-register-file` option for matching ordinary kernels, preserving
their SPIR-V bytes. The pattern must match at least one export. ESIMD and
kernels declaring a GRF property reject this override. Existing GRF properties
continue to supply their own module options. Hardware behavior and performance
need checks on the target GPU; larger register files can reduce occupancy.

This is a compiler adapter for Strata's kernels, with pointers, standard-layout
captures and linear local scratch. It is not a general implementation of the
SYCL host API. The compiler check exercises scalar/aggregate captures, an
indirect host pointer, dynamic local scratch, ordinary and ESIMD kernels and
three native graph replays. It also checks that the process has not loaded
SYCL, UR or oneMKL. These checks passed on Windows/A770 with IntelLLVM 2025.3.2
and driver 32.0.101.9034 on 2026-10-29; native Linux GPU coverage is pending.

The Strata numerical-source integration is in `../sycl/orca/`. The earlier
verifier-only bridge remains a separate, hybrid path described in
[demo/STRATA.md](demo/STRATA.md).

## Standalone performance demo

The comparison executable lives in `demo/`. Only this executable links SYCL
to provide a baseline; the runtime library keeps its Level Zero-only dependency.
Build it separately with a SYCL compiler (Linux example, after initializing
the compiler environment):

```sh
cmake -S orca-l0/demo -B build-orca-l0-demo -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=icpx
cmake --build build-orca-l0-demo
./build-orca-l0-demo/orca-l0-demo ./build-orca-l0-demo/inference.spv 15 > dispatch.csv
```

On Windows use `icx`, the `.exe` suffix and the same Level Zero/ocloc CMake
paths shown above. When extracting the runtime as a repository, replace
`-S orca-l0` with `-S .` and `-S orca-l0/demo` with `-S demo`.

The demo imports the **same compiled native module** into SYCL, keeping the
same device, context, allocations, kernel arguments and launch geometry.
It compares SYCL eager submission, SYCL command-graph replay and Orca L0
replay for a chain of 400 dependent vector-increment kernels. Cases use
64 elements and 1,048,576 elements to expose different ratios of submission
work to GPU/memory work. This is a dispatch experiment, not a model replica.

Each timed step includes input transfer, all kernels, result transfer and
completion wait. Every output is checked exactly against a CPU reference
outside the timed region. Three warmups precede retained samples. Each native
sample is bracketed by graph baseline samples (A0/B/A1); all raw times are
printed as CSV. Summary pairing requires adjacent baseline drift <=3% and
at least five stable pairs. Both positive and negative changes are reported;
insufficient stable pairs yield `inconclusive`. Report graph-relative results
as the primary comparison when evaluating an engine that already uses graphs.
Run fresh processes to assess repeatability, without other GPU jobs or CPU
compilation. This demo does not establish a Strata token-throughput change.
It measures direct submission; the optional ring transport is exercised by
the separate runtime check rather than included in these timings.
The measured Windows A770 results and all retained samples are linked in
[demo/RESULTS.md](demo/RESULTS.md).

The experimental Strata verifier integration and its real-model checks are
described in [demo/STRATA.md](demo/STRATA.md). That hybrid path keeps SYCL
kernel compilation and graph recording, then submits the borrowed Level Zero
list through Orca. It does not remove SYCL from the engine.

## Execution contract

`Runtime` selects an Intel GPU by index across the installed Level Zero
drivers. It owns its context, compute queue, allocations, modules, kernels,
events and command lists. Resources remain alive until the runtime drains
its queues and destroys them. Stop producers and join the execution thread
before destroying the runtime. Signal outstanding host gates first.

The free `replay` and `wait` functions also accept borrowed native queues and
closed command lists. They neither destroy these handles nor synchronize an
external runtime's pending submissions automatically. The caller must flush
that runtime first, keep every owner alive, and use native completion before
resuming external submissions. An exported graph may need additional private
event-reset or copy lists; one handle alone does not establish replay safety.

`Arena` slices one device allocation using checked absolute alignment and
capacity arithmetic. Allocate weights, scratch and parameter buffers during
setup. Allocation bookkeeping is outside submission. Allocations are virtual
device memory; this API does not promise contiguous physical pages or
per-allocation reclamation.

Device allocations explicitly establish residency before returning. SPIR-V
modules disable stateful buffer addressing so kernels can access arenas above
4 GiB, matching UR's relaxed-allocation mode. Native/AOT images must already
have been compiled for the allocation sizes they will access.

`arguments(kernel, values...)` passes explicit C ABI values to
`zeKernelSetArgumentValue`. Pass pointers as pointer values and match scalar
widths, struct layout and local sizes to the compiled kernel. OpenCL C entry
points avoid SYCL's compiler-generated closure ABI. Porting an existing
SYCL/ESIMD kernel requires inspecting its ABI or giving it an explicit native
entry point; loading an arbitrary extracted image is not enough.

Regular `Commands` lists are recorded, closed, submitted and waited on.
`launch` captures the arguments at append time. Changing a kernel handle's
arguments after recording does **not** update a closed list. Put changing
values in stable device parameter buffers and upload them before each replay,
or reset and record a list after its previous execution completes. Indirect
device pointer tables also require the relevant `zeKernelSetIndirectAccess`
flags and correct residency; the runtime never enables all indirect memory
implicitly.

Memory dependencies are explicit. Put `barrier()` between dependent kernels
and copies, or use events with signal/wait dependencies. Immediate lists allow
dynamic work and copies; their appends execute immediately and cannot be
closed or replayed. An immediate copy list selects a dedicated copy engine
when available, otherwise the compute engine. For cross-engine transfers,
signal an event after the copy and wait on it before the consuming kernels.

One fence belongs to each regular list. `submit()` rejects an unclosed or
already in-flight list. `wait(timeout_ns)` returns false on timeout while
preserving in-flight ownership. A successful `wait()` or `ready()` allows the
next submission or reset. Never recycle inputs, staging buffers or events
while their GPU users are still in flight. Host-visible events use host
memory scope. Host event signaling still goes through the driver; it does
not bypass WDDM scheduling.

## Ring submission

`Ring<T, Capacity>` borrows the sequence publication and consumer gating idea
from Disruptor. It is a bounded SPSC queue with acquire/release publication,
cache-line-separated cursors, power-of-two indexing and no allocation in
`try_push`/`try_pop`. Full and empty return false; the caller chooses its
backpressure and idle policy. The target must have lock-free word atomics.

Use one ring for requests and another for completions. A single execution
thread owns Level Zero submissions and kernel mutations. A popped request
only frees a CPU ring slot: its GPU buffers remain owned until a completion
is published after a successful GPU wait. The runtime check exercises this
with 24 real GPU command-list replays and sequence-tagged completions.
Each ring has exactly one producer and one consumer. Multiple callers need
separate SPSC channels or a separately designed multi-producer sequencer.
The driver itself can still allocate or lock internally.

## Validation and scope

The CPU check tests empty/full handling, backpressure, slot wraparound and
200,000 ordered request/completion handoffs across two threads. It uses
explicit checks that remain enabled in release builds.

The GPU check compares RMSNorm → gate/up GEMV → SwiGLU against a CPU
reference, with non-multiple dimensions and three dynamic parameter updates.
It also checks module errors, arena overflow/alignment, host gates, timeouts,
in-flight reuse rejection, list reset, captured arguments and a chain of
400 dependent kernels. It separately prints recording time, mean CPU
submission time and mean submission-plus-wait time after warmup. Submission
includes fence reset. These numbers describe this microbenchmark only.

The native Strata engine integration is in [`sycl/orca`](../sycl/orca/README.md).
It runs the existing numerical kernels through native queues, command lists,
memory and events; its README records component and full-model validation.
The SYCL reference build remains available for comparisons. MoE routes,
cache misses, sequence lengths and CPU-produced results prevent treating every
decode as one fixed 48-layer list. Record stable regions and connect them with
explicit dependencies.

The proposed +8–15% decode gain and sub-10 µs submission are hypotheses.
Existing SYCL command graphs already amortize much dispatch work. Measure
CPU time exposed on the critical path against the same model, accepted
tokens, context, memory budget, GPU clocks and driver. A synthetic dispatch
improvement does not establish an end-to-end token-throughput gain.
