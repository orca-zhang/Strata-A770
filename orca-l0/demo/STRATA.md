# Experimental Strata integration

`STRATA_SYCL_ORCA_L0` builds an optional bridge into the real speculative
verifier. `STRATA_ORCA_L0=1` selects it at process startup; leaving that variable
unset retains SYCL submission. Both the verification window and the state
commit use Orca's native replay and native completion.

This is a hybrid integration. SYCL still compiles all existing Strata kernels,
owns their allocations and records the graphs. A native-command callback
obtains the Level Zero compute list during graph finalization. Orca borrows
that list and submits it on the **same queue and context**. The graph owner
stays alive. Pending SYCL work drains before native submission; completion,
watchdog polling and destruction use native synchronization as well.

This first bridge uses a small mutex-protected registry for borrowed graph
handles, preserving the verifier's object layout. The standalone runtime's
optional SPSC transport is not used in this single-submitter integration.
Storing these handles directly in the verifier during a full rebuild can
remove this registry lock; native dependency events can replace the current
host drain at the SYCL/Orca ownership boundary.

The initial bridge is restricted to Arc A770 and the device-planned verifier
(`STRATA_VERIFY_DEVICE_PLAN=1`, `STRATA_VERIFY_NO_HOST=1`). Host-task graph
partitions and other devices' private copy lists are not supported. Kernels,
mathematics, expert residency and model settings remain identical between
the two arms.

Build on Linux after initializing the oneAPI compiler environment:

```sh
cmake -S sycl -B build-sycl-orca -G Ninja \
  -DCMAKE_CXX_COMPILER=icpx -DSTRATA_SYCL_ORCA_L0=ON
cmake --build build-sycl-orca --target strata strata-orca-replay-check
```

On Windows use `icx` and pass
`-DORCA_L0_SDK=C:/Program Files/LevelZeroSDK/1.33.1`.
Install the usual Strata SYCL build dependencies as described in
[`docs/INTEL.md`](../../docs/INTEL.md).

The graph export requires these settings **before process startup**:

```sh
export UR_L0_USE_IMMEDIATE_COMMANDLISTS=1
export UR_L0_CMD_BUFFER_USE_IMMEDIATE_APPEND_PATH=1
export UR_L0_USE_DRIVER_INORDER_LISTS=1
export UR_L0_USE_DRIVER_COUNTER_BASED_EVENTS=1
export STRATA_VERIFY_DEVICE_PLAN=1
export STRATA_VERIFY_NO_HOST=1
unset STRATA_VERIFY_EAGER
STRATA_ORCA_L0=1 ./build-sycl-orca/strata-orca-replay-check
```

The four UR settings let the recorder create a self-contained in-order
compute list. Older graph configurations also depend on private wait/reset
lists and cannot be submitted through this bridge. The independent runtime
itself does not use UR. `strata-orca-replay-check` alternates window and commit
graphs for 32 rounds, checks pending SYCL input writes and exact output
readback, and exercises the same bridge as the model.

The real-model collector uses the same executable with the switch off/on/off:

```sh
python tools/bench_orca_strata.py \
  --exe build-sycl-orca/strata --config strata-config.json \
  --out build-orca-model-results
```

It sets the same four UR settings in **all** arms, tests speculative widths
1 and 4, warms each prompt, retains three samples, saves all generated token
IDs and timing rows, and requires every output token to match. It reports
each prompt's median decode time and adjacent baseline drift. A single A/B/A
cohort is an integration trial; small changes need more independent cohorts.
This collector shares Strata's GPU experiment lease. It does not change the
production executable or configuration.

The single-token case uses verifier capacity 2 to satisfy the native pack's
CLI constraint, then disables MTP and suffix drafting. Every decode window
therefore has T=1; the collector also requires zero offered drafts. The T=4
case retains the model's usual MTP confidence threshold and may use shorter
windows when the drafter lacks confidence.

Native Linux GPU validation remains necessary; Windows results cannot establish
Linux performance. The earlier dispatch demo results describe their recorded
source version and are not measurements of this integration.

## Windows A770 trial, 2026-10-29

The integration ran the complete Qwen3.8-Flash-Next IQ2_XS model on an Intel
Arc A770 16 GB, driver 32.0.101.9034, Windows 10 Pro 10.0.19045 and a Xeon
E5-2696 v4 (22 cores/44 threads). IntelLLVM 2025.3.2 and Level Zero SDK 1.33.1
were used. The measured executable reuses the validated model objects from
`build-sycl-a770-down-device-counter-20261028/baseline` and recompiles the
ABI-compatible verifier with the bridge and Orca runtime. The current live
verifier also compiled successfully, and the CMake feature configured
successfully. The raw artifact includes the measured verifier's patch and
source/executable/runtime fingerprints.

Both bridge checks passed 32 alternating window/commit replays with exact
floating-point output. The model trial used one OFF/ON/OFF cohort per decode
mode: six fresh processes, 48 requests, 36 retained timing samples and 12
warmups. Every request generated 64 tokens. All 3,072 output tokens were
retained; each Orca and final-control request matched its corresponding first
control request in full token sequence, accepted/offered drafts, reuse and
finish reason. All six processes exited successfully. Execution logs confirmed
native replay in both Orca processes and its absence in all four controls.

Settings included expert cache 6000 MiB, context/prefill 8192, reserve 1024 MiB,
prompt cache disabled, short-read 1024, V4 value packing, and adaptation every
token with at most 32 swaps. The speculative case used MTP and min-p 0.5.
All builds and GPU work used the shared experiment lease.

The baseline below uses **the same optimized UR graph settings** as Orca.
Each arm's value is its three retained requests' median. The baseline is the
geometric mean of the two control medians; change is `baseline / Orca - 1`.
The combined column uses the median of each request's
`prompt_ms + decode_ms`, so it measures those engine phases rather than full
HTTP/request wall latency.

| Decode mode | Prompt | Baseline decode, ms | Orca decode, ms | Decode throughput change | Prompt + decode change | Control drift |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| T=1, no drafter | Short, 23 tokens | 3288.44 | 3298.30 | -0.30% | -0.28% | -0.52% |
| T=1, no drafter | Code2K, 2048 tokens | 3552.35 | 3535.70 | +0.47% | +0.13% | +0.06% |
| MTP, maximum T=4 | Short, 23 tokens | 2112.15 | 2086.00 | +1.25% | +1.05% | +0.27% |
| MTP, maximum T=4 | Code2K, 2048 tokens | 2111.49 | 2067.90 | +2.11% | +0.79% | -2.80% |

All retained samples are kept, with no outlier or drift filtering. Comparing
the same retained request index/history across the three processes gives
these additional views:

| Decode mode | Prompt | Three matched decode changes | Matched median |
| --- | --- | --- | ---: |
| T=1 | Short | -0.40%, +0.48%, +0.31% | +0.31% |
| T=1 | Code2K | +0.66%, +0.62%, -0.10% | +0.62% |
| MTP, maximum T=4 | Short | +0.59%, +1.25%, +0.99% | +0.99% |
| MTP, maximum T=4 | Code2K | +0.72%, +0.77%, +2.11% | +0.77% |

Ten of twelve matched retained requests were faster. These observations share
processes and are not twelve independent trials. The different summaries
show why changes this small need more fresh cohorts: the Code2K speculative
control's decode time decreased 2.80%, and its typical matched change was 0.77%.
This trial establishes a functioning full-model replay bridge and a small
positive trend in speculative decode. It does not establish a stable 2%
gain or the earlier proposed 8–15% model gain. Single-token results were
close to parity. The feature remains disabled by default.

[All raw requests, timings, source patch and fingerprints](strata-a770-windows-2026-10-29.json)
are retained. The independent dispatch demo's larger gains describe a
different workload and graph configuration.
Measured source hashes stay historical; the raw artifact records the later
comment-only edit that documents the bridge's mutex and host-wait limits.
