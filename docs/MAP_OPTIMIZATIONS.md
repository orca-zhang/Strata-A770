# Map indexes: bbtree review and measurements

Measured on 2026-10-26, Windows 10, Intel Xeon E5-2696 v4 @ 2.20 GHz.
These are single-threaded synthetic lookup measurements, with no CPU affinity,
GPU kernels, model weights or end-to-end tokens/s measurement. The initial lookup
table alternates before and after three times and reports medians. Follow-up
measurements specify their run counts below. Timing varies with host load.

## What transfers from bbtree

Reviewed [bbtree at f0a5d361](https://github.com/XiangpengHao/bbtree/tree/f0a5d361ecb4df176b3e07d4e33bb399dc3d22bf).
Its [inline-key design](https://github.com/XiangpengHao/bbtree/blob/f0a5d361ecb4df176b3e07d4e33bb399dc3d22bf/design/04-inline-key.md)
keeps the comparison stream compact and avoids dependent heap reads; its
[batch API](https://github.com/XiangpengHao/bbtree/blob/f0a5d361ecb4df176b3e07d4e33bb399dc3d22bf/design/12-batch-api.md)
overlaps independent searches. The library is Rust, so it is not a drop-in
replacement for C++ `std::map`. None of its code or dependencies was imported.

Strata's affected containers have simpler requirements:

- `FileExpertSource::stage_of_` uses bounded `layer * n_expert + expert` keys.
  A `vector<size_t>` records slot + 1, with zero meaning absent. It allocates
  on the first staging request, under the existing mutex. CUDA/HIP and SYCL
  use the same implementation. Hits and invalidations need no hash nodes.
- The SYCL `GgufExpertSource::where_` uses the same dense keys. Its ring has
  512 slots, so `uint16_t` holds slot + 1; a compile-time check guards that limit.
- `GgufFile::find` replaces a linear directory scan with binary search over
  a sorted vector of borrowed names and tensor indexes. This index also detects
  duplicate names, replacing the temporary `set<string>` used during parsing.
- `GgufModel::find` replaces its tree with a sorted vector of borrowed names,
  shard indexes and tensor pointers. Both find methods accept `string_view`,
  including non-null-terminated names. Tensor directories keep their file order.

The dense tables trade fixed space for fewer lookup and allocation operations:
48 layers x 512 experts use 192 KiB for the staging index and 48 KiB for the SYCL
ring index on this 64-bit build. The staging index is absent for mapping-only
sources. A single-file GGUF now retains an additional 24 bytes per tensor for its
index; the model index uses 32 bytes per tensor, without copied names or tree nodes.
These sizes describe the indexes, not measured process memory consumption.

The weight table, metadata maps, graph registries and Python dictionaries are
unchanged. There is no measured case here for adding a general B-tree, prefix
compression, speculative prefetch or another concurrent container.

## Lookup results

| Path | Compiler | Before, ns | After, ns | Speedup |
| --- | --- | ---: | ---: | ---: |
| `GgufFile::find`, 4,102 tensors | ICX 2025.3, MSVC STL | 11,991.05 | 216.83 | 55.30x |
| `GgufModel::find`, 4,102 tensors | ICX 2025.3, MSVC STL | 363.91 | 226.67 | 1.61x |
| `GgufFile::find`, 4,102 tensors | MinGW GCC 14.2 | 11,541.41 | 223.97 | 51.53x |
| `GgufModel::find`, 4,102 tensors | MinGW GCC 14.2 | 281.37 | 223.36 | 1.26x |
| `claim_stage` hits, 512 slots | MinGW GCC 14.2 | 13.48 | 5.65 | 2.39x |
| SYCL `blob` ring hits, 512 slots | ICX 2025.3, MSVC STL | 35.37 | 28.70 | 1.23x |

Both compilers use C++20 and O2. ICX uses MSVC STL 14.44.35207 with
`_USE_STD_VECTOR_ALGORITHMS=0`, as in this machine's SYCL build.
GGUF queries make 200,000 hits over shuffled names, including short names,
shared prefixes and embedded NULs. The staging and ring loops make 4,000,000
hits over 512 keys. Staging timing excludes its caller's mutex and I/O;
ring timing includes the real `blob` hit path and mutex, with no I/O on hits.
Matching checksums were verified in each before/after pair.

GGUF lookup savings primarily affect loading and tensor resolution. Cache hits
save nanoseconds; this table does not establish an inference throughput gain.

## Checks and reproduction

From the repository root, with a C++20 GCC available:

```sh
mkdir -p build-map-check
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Iinclude src/artifact/gguf_reader_test.cpp -o build-map-check/gguf_reader_test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Iinclude tests/core/gguf_split_test.cpp -o build-map-check/gguf_split_test
./build-map-check/gguf_reader_test --bench
./build-map-check/gguf_split_test
python tools/test_expert_stage_index.py --bench
```

The staging check compiles the actual two backend methods and their member
declarations, without GPU adapters, and requires the implementations to agree.
It covers slot zero, hits, busy slots, eviction, failed reads, invalid keys and
geometry changes. GGUF checks include empty directories, duplicates within and
across shards, borrowed-name lifetimes, file order, missing names and model moves.

In an existing SYCL build configured with `STRATA_SYCL_PARITY=ON`:

```sh
cmake --build build-sycl --target gguf_expert_source_test
ctest --test-dir build-sycl -R '^gguf_expert_source_test$' --output-on-failure
```

That standalone test links the real GGUF source to a tiny synthetic layout.
It checks every byte across two layers, ring wrap, repeated hits, warming skips,
invalid geometry, truncated reads, retry after repair, and close/reopen.
It does not submit GPU work. CUDA/HIP kernel execution and full-model parity
were not tested by this change.

The local before snapshots, build logs, checksums and all benchmark samples are
in `build-bbtree-map-20261026/`; they are ignored build artifacts.

## Follow-up: PLE page grouping

Both readers now sort compact `Use` records by starting page, then group adjacent
records into jobs. This replaces the temporary page hash and the sort of larger
`Job` objects. Sorting destinations within each page preserves the old request
order, including the row cache's replacement order. Page-straddling rows still
extend their job to two pages. Already ordered requests skip sorting. Each
page's use range is copied once; a single-page request transfers the miss vector
directly into its job. All-cache-hit and empty requests allocate no job
metadata, including with MSVC STL, whose empty `unordered_map` allocated twice.

The two temporary vectors initially reserve at most 16 entries, only after a
miss. Larger requests grow normally; there is no quadratic small-batch fallback.
The extra miss vector holds 16 bytes per capacity slot on this machine, until
`issue` returns. This replaces per-page hash nodes and buckets, but can increase
temporary memory in multi-page requests. Single-page requests reuse that vector.

The following medians use five alternating before/after runs, C++20/O2 and the
same CPU as above. The actual job-building source block is compiled separately;
timing includes filtering, grouping, sorting, allocations and destruction. It
excludes tickets, queueing, mutexes, worker wakeups and disk I/O. Each case repeats
one seeded request for at least 100 iterations and about two million rows, after
warm-up. The synthetic table has 500,000 rows of 90 bytes at offset 192.
Before/after checksums agree.

| Request | GCC before / after, us | ICX/MSVC STL before / after, us |
| --- | ---: | ---: |
| 1 uncached row | 0.378 / 0.195 | 0.393 / 0.184 |
| 16 random uncached rows | 4.941 / 2.361 | 6.176 / 1.589 |
| 16 copies of one uncached row | 0.909 / 0.274 | 1.361 / 0.285 |
| 16 rows, 15 cached | 0.680 / 0.481 | 0.705 / 0.449 |
| 16 rows, all cached | 0.308 / 0.301 | 0.417 / 0.265 |
| 17 random uncached rows | 5.217 / 2.761 | 6.497 / 1.922 |
| 17 copies of one uncached row | 1.021 / 0.402 | 1.311 / 0.376 |
| 256 random uncached rows | 77.405 / 34.765 | 99.435 / 41.669 |
| 256 copies of one uncached row | 5.053 / 2.389 | 7.157 / 2.982 |
| 20,000 random uncached rows | 6,380.927 / 3,864.566 | 6,600.232 / 3,623.791 |
| 20,000 copies of one uncached row | 354.812 / 196.201 | 468.627 / 208.898 |

The 16-row random case drops from 39 to 18 allocations with GCC, and from 43 to
18 with MSVC STL. These are CPU request-building gains, not SSD or token rates.
The 20,000-row random case drops from 27,963 to 9,302 allocations with GCC,
and from 29,204 to 9,315 with MSVC STL.

The real disk selftest passes with GCC on the native reader and ICX/MSVC STL on
the SYCL reader. It covers every PLE format, 90- and 110-byte rows, threaded and
caller-thread submission, cache on/off, inflight limits, invalid rows, straddles,
20,000-row requests, concurrent tickets, injected delay and keep-alive. Added
checks cover empty and small requests, dedup/read counts, two-page extensions
and exact same-page cache replacement. No GPU is required.

From the root, with GCC available (on Windows add `-ldxgi` before `-o`):

```sh
g++ -std=c++20 -O2 -Iinclude src/ngram/ple_reader_test.cpp src/ngram/ple_reader.cpp src/platform/direct_file.cpp src/kernels/ngram.cpp src/platform/memory.cpp -o build-map-check/ple_reader_test
./build-map-check/ple_reader_test --selftest
```

## Follow-up: staging victim scan

The linear victim scan first rejects slots newer than its current candidate,
before reading busy and epoch metadata. Eligibility, the LRU choice and busy-slot
protection are unchanged. The hit path adds no bookkeeping. The scan remains
O(number of slots); no replacement-policy container was added.

Three alternating GCC/O2 runs of the actual `claim_stage` and `publish_stage`
methods give these medians. Each miss case runs 100,000 completed misses, with
16-byte synthetic buffers and the epoch advanced before each miss. Timings
include publishing, its mutex and counters, but exclude caller locking and I/O.
Both sequential and permuted initial LRU orders produce matching checksums.

| Slots / initial order | Before, ns | After, ns | Speedup |
| --- | ---: | ---: | ---: |
| 256 / sequential | 356.05 | 213.54 | 1.67x |
| 256 / permuted | 383.24 | 247.53 | 1.55x |
| 512 / sequential | 669.39 | 361.59 | 1.85x |
| 512 / permuted | 682.07 | 435.67 | 1.57x |
| 2,048 / sequential | 2,489.47 | 1,231.94 | 2.02x |
| 2,048 / permuted | 2,530.65 | 1,328.22 | 1.91x |

The unchanged 512-slot hit prefix measured 5.18 / 5.54 ns; no hit-speed gain is
claimed. Run `python tools/test_expert_stage_index.py --bench` for correctness
checks and both hit/miss workloads. The modified SYCL source also compiles with
ICX; this does not validate GPU execution.

## Follow-up: batch graph keys

The first follow-up reserved `S + 2` vector elements to remove a second key
allocation (the original header and check remain in
`build-bbtree-map-followup-20261026/`). The final CUDA/HIP verifier uses
`array<int, 11>`: row count, row base, doorbell flag and up to eight ordered slot
IDs. `stage_batch` already validates this row bound. The explicit count prevents
different-length layouts with trailing zero rows from sharing a graph.

The key stores 44 bytes inline, eliminating heap allocation during key
construction and a separate key buffer in each map node. A larger inline key
trades some node space for short layouts. The three maps and graph lifetime,
synchronization and eviction code retain their existing structure.

`python tools/test_batch_graph_key.py` extracts the actual key method and map
declarations. It passes 59,040 layouts (all row IDs 0-2 for lengths 1-8, three
bases and both flags), checks key reuse, no aliasing, all three maps and zero
key allocations. It passes with GCC and ICX/MSVC STL.

A separate GCC/O2 microbenchmark alternates before/after three times: four
million key constructions plus map hits across 64 layouts, with matching
checksums. Median time falls from 112.74 to 37.13 ns (3.04x), and construction
allocations from one to zero. Its baseline includes the earlier reserve fix.
This does not measure graph capture, GPU launches or inference throughput.

All follow-up snapshots, source extraction scripts, samples and build logs are
in `build-bbtree-map-ple-20261026/`. `bench.py --generate` extracts both reader
blocks and checks backend agreement; `bench.py` runs the GCC pair and `--icx`
runs the ICX pair. `compile-icx.bat`, `stage-bench.py`, `graph-bench.py` and
`final-checks.py` reproduce the local builds and checks.

## Follow-up: all map sites and the A770 runtime

Source review on 2026-10-29 covers the C++ map/multimap/unordered-map declarations
in `include/`, `src/` and `sycl/`, the tokenizer and server caches, and selected
UR/NEO memory and submission containers below. It does not audit every container
inside SYCL, UMF, oneDNN or the Windows driver. This follow-up adds no engine or
driver change, compiler run, GPU request or new throughput measurement.

The useful bbtree ideas are compact comparison data, fewer dependent loads and
independent batch lookups. Dense IDs usually need an array; a small immutable
directory usually needs a sorted vector. Neither needs a new general tree.
bbtree's batch API overlaps CPU searches; it does not by itself combine GPU
experts or remove their kernel submissions. Router-score order must retain each
token's routing weights, destinations and accumulation semantics.

### Project containers

| Container / source | Actual use | Decision for the current A770 decode path |
| --- | --- | --- |
| Expert staging and SYCL GGUF ring | Repeated bounded `(layer, expert)` lookups | Dense indexes are already implemented and checked above. No remaining tree lookup here. |
| GGUF tensor directories | Load-time name resolution | Sorted borrowed-name indexes are already implemented. Metadata in `GgufFile::meta_` remains a map; changing it would mainly affect loading. |
| `WeightTable::table_`, `include/strata/core/weights.hpp` | String-to-weight lookup; `all()` exposes the concrete map type | The measured verifier enables `STRATA_VERIFY_DESC_CACHE`, resolving layer/output descriptors once. Keep stable `WeightRef` addresses and iteration/API behavior. More caching is preferable to a new tree if an uncached path is measured hot. |
| `MtpDrafter::record_forward`, `sycl/src/core/mtp.cpp` | Includes `wt_->find("token_embd.weight")` and draft-weight resolution | Graph mode records these in capture; cached graph replay does not repeat the body. Eager mode calls the body again and remains a possible descriptor-cache target. Do not count capture lookups as per-token replay work. |
| `exec_[9]`, `sycl/include/strata/core/verify.hpp` | Current verifier graph selection | Already an array. `GraphRegistry::graphs_` is a separate map with no callers found outside its own implementation in the reviewed trees. |
| CUDA/HIP `exec_bm_`, `commit_bm_`, `bm_used_` | Batch-layout graph caches and optional LRU metadata | Inline keys are already implemented above. Combining the three values in one entry could remove repeated searches, but requires checking capture failure, commit-only entries and graph destruction. These maps are absent from the SYCL verifier. |
| DPCT launcher global/TLS maps, `sycl/include/dpct/kernel.hpp` | Generic eager dispatch through a raw function pointer | TLS hits already avoid the global mutex. A compact registry could help eager dispatch if measured; graph replay does not dispatch every recorded kernel through this map. Typed launch overloads also bypass it. |
| Three TLS queue-to-scratch caches, `sycl/src/kernels/cuda/fused_gr.dp.cpp` | Per-thread scratch lookup before kernel submission | Global locking was already removed. A last-queue shortcut is only a hypothesis for eager submission; preserve multiple queues, thread ownership and scratch lifetime. Captured pointers need no new lookup on replay. |
| DPCT graph maps, `sycl/include/dpct/graph.hpp` | Queue recording and graph node/root-node introspection | Capture/diagnostic containers. No change for steady graph replay. Node-kind maps in both verifiers are also guarded diagnostics. |
| DPCT virtual-buffer address map, `sycl/include/dpct/detail/memory_detail.hpp` | Range translation for buffer emulation | Allocation/translation uses are guarded by `DPCT_USM_LEVEL_NONE`. The manager can still be constructed by `device_memory`; construction does not establish address-map lookups on the active USM path. |
| PLE `tickets`, both `ple_reader.cpp` files | Sparse live ticket IDs shared by caller and I/O worker | Keep hashing. IDs wrap, completions can arrive out of order, and collection waits release/reacquire the mutex. A dense table indexed by the monotonically increasing ID would grow with request history; slot reuse needs generation checks. Page grouping was optimized separately above. |
| Shard-file maps and `NativeDense` sets | Opening/deduplicating files and building the model layout | Startup work; leave unchanged for decode optimization. `session_model_fingerprint`'s path cache avoids repeated file reads during identity validation. |
| `generate.cpp::part_at` | Joining per-stage mid-prompt checkpoints by position | Prompt-cache/multi-stage work, outside the measured cache-zero steady decode path. The string map in `disk_wait_threads` is Linux stall reporting, not normal Windows inference. |
| HIPBLASLt algorithm maps in both GEMM sources | Algorithm and fallback-shape cache | Inside the HIP/hipBLASLt guards; not an A770 opportunity. |
| Native CUDA Q8/Q6 repack registries and host-alias cache | Pointer-to-packed-weight or CUDA batch-copy alias lookup | Other-backend candidates. Bound their lifetime before considering a compact index; these are not the SYCL MMVQ path. |
| DPCT oneDNN/BLAS/sparse/image/CCL/codepin maps and parity-test maps | Library wrappers, image interop, instrumentation or tests | No evidence of a hot map on the measured native-kernel graph path. Replacing header declarations alone cannot establish an inference gain. |
| Python tokenizer ranks/byte cache, server image cache and live requests | BPE/byte resolution, encoded-image reuse, request bookkeeping | Keep Python dictionaries: these are exact-key queries. The image cache's oldest-first eviction also depends on insertion order. A dense token-byte list is a separate memory/CPU tradeoff, not a reason to add a B+tree. |

Reproduce the C++ declaration search with:

```powershell
rg -n '\b(unordered_map|multimap|flat_map|BTreeMap|btree_map|map)\s*<' include src sycl -g '*.hpp' -g '*.h' -g '*.cpp' -g '*.cu'
```

### NEO: the main USM index is already flat

Reviewed compute-runtime commit
[`3c7d20d781da468dc60a8204f0735004918e0940`](https://github.com/intel/compute-runtime/tree/3c7d20d781da468dc60a8204f0735004918e0940).
The installed Windows DLL has not been matched to this source commit.

[`getSVMAlloc`](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/memory_manager/unified_memory_manager.h#L271)
queries `svmAllocs`, declared as `SortedVectorBasedAllocationTracker` at line 374.
The nearby `MapBasedAllocationTracker` member is `svmDeferFreeAllocs`, used for
deferred frees. Treating that map as the main address lookup would target the
wrong container.

The [sorted-vector implementation](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/utilities/sorted_vector.h#L20)
holds contiguous `(base pointer, unique_ptr<metadata>)` pairs. It already has
compact base keys while keeping metadata addresses stable. Offset search's source
expression reads `metadata->size` at each binary-search probe; optimized machine
code may defer or eliminate some reads. An explicit base-only descent followed
by one size check is worth considering only after checking generated code,
hotness and nonoverlapping-range invariants. A predecessor search can select a
different allocation for overlapping ranges. Nulls, exact bases, gaps, zero-size
entries and exclusive ends must keep their behavior. `SvmAllocationData` is
nonmovable; putting it directly in moving tree nodes is not a compatible change.

Two further containers have different workloads:

- `indirectAllocationsResidency` maps CSR pointers to task-count/allocation-ID
  state. A small inline list or a validated last-CSR cache could avoid tree
  descent, but must preserve synchronization and CSR lifetime. Its lock and
  task-count updates remain necessary for safe frees.
- [`makeIndirectAllocationsResident`](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/memory_manager/unified_memory_manager.cpp#L1313)
  already skips new-allocation traversal when the allocation counter is unchanged.
  When it changes, the code searches `internalAllocationsMap.equal_range(id)` for
  every integer ID. A bounded iterator traversal over the existing multimap could
  skip freed-ID gaps and retain equal-ID/multi-device ordering. This targets
  allocation churn or a new CSR, with no established steady-decode benefit.

The deferred-free and SVM map-operation trees remain separate lifetime/map/unmap
workloads. Replacing them does not remove the main address lookup or prove a
submission improvement.

### UR: remove searches only on paths that run

Reviewed Unified Runtime commit
[`5e422374f5f6710714d970f66c71658182d448d7`](https://github.com/oneapi-src/unified-runtime/tree/5e422374f5f6710714d970f66c71658182d448d7),
legacy Level Zero adapter. This public source also does not establish the
installed oneAPI 2025.3 DLL's exact implementation.

| Container | Source finding | Potential change / constraint |
| --- | --- | --- |
| Queue groups by thread ID | `queue.hpp:289` hashes even the shared, non-per-thread group | Return the sole entry directly when `PerThread == false`, preserving the per-thread fallback. A tiny fast path, with no measured cost yet; a B+tree is unnecessary. |
| Command-list registry/cache | Queue `CommandListMap`, context compute/copy caches and `ImmCmdLists` | Some paths already retain command-list iterators. Preserve their lifetime before using moving storage. Measure cache size, lock time and actual searches per graph enqueue. |
| Queue event caches and device-index maps | `queue.cpp:1639` performs `find` then `at` for a cache hit | Reuse one lookup result first. Only consider a direct device-index field after measuring the actual single-/multi-device cache paths. Preserve host visibility and event reuse/completion rules. |
| Context event-pool device maps | `context.hpp:218` uses `find` then `operator[]` | Reuse the iterator. A multi-device cache needs device validation, while the existing multi-device-event branch already indexes fixed cache slots. |
| Event-pool free/unreleased counters | Two maps keyed by the same native pool | One pool-state record could reduce lookups, but the counters mean different things. Preserve destruction/reuse synchronization; do not merge the counters themselves. |
| Kernel-to-device map | `getZeKernel`, `kernel.cpp:22` | Empty-map native-handle shortcut already exists. For populated maps a validated one-entry shortcut is possible; it must still reject a mismatched device. Ordinary graph replay does not resolve every recorded kernel anew. |
| Program device/spec-constant maps and USM P2P cache | Program creation/build and allocation setup | Lower priority for steady replay. Keep exact device/context ownership. |
| Command-buffer sync-point map | Integer `NextSyncPoint` starts at zero and advances on registration | Dense indexing is plausible after bounds/overflow/invalid-ID validation. The reviewed lookups build dependency lists during command recording, so this mainly targets capture. |
| Command-buffer queue-to-fence map | `getFenceForQueue` is called by `enqueueWaitEventPath` | A small native-queue cache may help the older replay path. The reviewed immediate-append path bypasses this fence lookup; it is not an extra gain to assume on top of append. |
| Context allocations and per-kernel indirect sets | `kernel.hpp:79` explicitly retains pointers to context map elements | Node addresses are part of lifetime tracking. Moving values into a flat hash/vector/B+tree can invalidate them. Measure whether indirect tracking is enabled and the traversal runs before changing storage; preserve retained-allocation snapshots and safe frees. |
| Queue timestamp maps | `event.cpp:765` passes the address of a map value to `zeCommandListAppendWriteGlobalTimestamp`; release can transfer its node to the evicted map | Stable addresses are required until asynchronous writes finish. A moving flat container is unsafe without separately owned storage. This timestamp-recording path is not every ordinary graph event. |

All referenced public files and their hashes are recorded in
`build-sycl-a770-neo-map-audit-20261029/source-manifest.json`; its `check.py`
verifies the retained snapshots without a GPU or compiler. These are source
observations and candidate directions, not new runtime performance results.
The two bbtree design files have local Git CRLF line endings; the manifest keeps
both their local byte hashes and the matching upstream LF hashes. The other
17 snapshots match their public commits byte for byte.

```powershell
python -X utf8 build-sycl-a770-neo-map-audit-20261029/check.py
```

### Priority and the throughput ceiling

First locate critical-path time in the existing model trace/time-line work.
If queue/event lookup is material, start with one-lookup reuse and validated
single-entry paths. Next check NEO offset-search code generation and measured
allocation churn. Extend descriptor caching only for a measured eager path.
Loading, capture-only, diagnostic and other-backend containers come after these.

For a lookup taking fraction `f` of the original critical path, making it 4x
faster yields `1 / (1 - f + f / 4)` overall. At `f = 0.1%`, that is only about
`0.075%`; achieving 1% overall requires at least `f = 1.32%` even before new
overheads. CPU work overlapped by GPU execution is not all critical-path time.
The earlier lookup microbenchmarks therefore remain separate from model tok/s.
Any adopted decode candidate still needs same-scene independent throughput
confirmation, adjacent baseline drift, quality/history and memory checks.
