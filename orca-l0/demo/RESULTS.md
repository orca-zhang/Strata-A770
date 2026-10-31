# A770 dispatch experiment, 2026-10-29

Orca L0 improves the small dispatch workload on the measured Windows PC.
The larger workload also improves in all three processes when comparing
whole-run medians; its filtered subsets include local slowdowns.
These are standalone execution-chain results. At the time of this experiment,
Strata's decoder had not been migrated and its token throughput was not measured
here. The later native engine, model measurements and serving checks are in
[the integration README](../../sycl/orca/README.md).

Machine: Intel Xeon E5-2696 v4 (22 cores / 44 threads), Intel Arc A770,
Windows 10 Pro 10.0.19045, Intel GPU driver 32.0.101.9034. The demo was built
in Release mode with IntelLLVM 2025.3.2, MSVC 14.29 headers/libraries,
Level Zero SDK 1.33.1 and ocloc 2025.3. CPU/GPU clocks were not fixed or sampled.

## Method

All paths use the same loaded native module, device, context, allocations,
arguments and launch geometry. Each step copies input to the GPU, executes
400 dependent vector-increment kernels, copies output back and waits for
completion. All output elements must exactly match the CPU reference.
Queue, barrier, event and scheduling policies contribute to the measured
difference alongside host runtime overhead.

Three 15-round pilot processes were followed by three fresh 100-round
confirmation processes to resolve insufficient stable-pair counts in the
pilot. Each case has three warmups per path. Each round runs graph A0,
Orca L0 B, graph A1, then SYCL eager. Raw observations from **all six
processes** are retained in [the CSV](a770-windows-2026-10-29.csv): 2,760 rows.
Validation and CSV formatting happen outside the timed interval.

Pairing requires `abs(A1 / A0 - 1) <= 0.03` and at least five accepted pairs.
The paired throughput change is the median of
`100 * (sqrt(A0 * A1) / B - 1)` over accepted pairs. The time columns below
are medians of **all** samples, so their ratio can differ from the paired
statistic. Positive and negative observations are both retained.

## Confirmation results

Times are milliseconds per complete 400-kernel step, including transfers
and completion wait. The primary control is SYCL graph replay.

| Elements | Process | SYCL eager | SYCL graph | Orca L0 | Stable pairs | Paired change vs graph |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 64 | 1 | 3.306 | 1.076 | 0.475 | 44/100 | +127.854% |
| 64 | 2 | 3.093 | 1.076 | 0.469 | 32/100 | +126.168% |
| 64 | 3 | 3.303 | 1.069 | 0.460 | 46/100 | +130.172% |
| 1,048,576 | 1 | 13.307 | 5.689 | 5.113 | 55/100 | +8.746% |
| 1,048,576 | 2 | 13.221 | 5.718 | 5.089 | 53/100 | +8.951% |
| 1,048,576 | 3 | 12.819 | 5.765 | 5.051 | 26/100 | -1.319% |

The small case has a repeatable paired gain of about 2.3x for this workload.
Its median host submission time is 12.7–14.2 microseconds for Orca L0 and
51.3–55.0 microseconds for SYCL graph replay. The larger case changes sign
across processes under the original filtering rule. That observation describes
the selected subsets, rather than the complete distribution.
The optional SPSC worker transport is excluded from these performance numbers.

## Whole-run view

This descriptive summary uses every confirmation round and keeps the
original filtered results above. All processes have the same 100 rounds.
Whole-run changes use `100 * (median(graph) / median(native) - 1)`;
faster-round counts compare native time with the bracketing geometric mean.

| Elements | Whole-run median throughput change, processes 1 / 2 / 3 | Faster native rounds | Unfiltered paired median, 300 rounds | Filtered paired median, pooled |
| ---: | --- | ---: | ---: | ---: |
| 64 | +126.6% / +129.5% / +132.1% | 300/300 | +130.0% | +128.4% (122 pairs) |
| 1,048,576 | +11.3% / +12.4% / +14.1% | 276/300 | +14.1% | +8.6% (134 pairs) |

The larger workload's overall direction is positive in this experiment.
The negative -1.319% confirmation subset and -2.283% pilot subset remain
in the record as variability to investigate; they do not by themselves
establish a general regression. The cause of that variability is unmeasured.
Within-process rounds share runtime state, so these counts are descriptive,
not 300 independent trials. The existing filtering rule has not been relaxed.
Linux and full-model validation are still required for claims about the
intended deployment or model throughput.

## Runtime checks

All three CTest checks passed: CPU ring, SPIR-V runtime and native-binary
runtime. The GPU checks include RMSNorm → GEMV → SwiGLU with three dynamic
parameter updates (maximum absolute error 3.8147e-6, tolerance 2e-5), event
dependencies, host signaling, timeout/reuse guards, command-list reset,
400 dependent kernels and 24 ring-driven GPU replays per format. The
standalone CPU-only build also passed its 200,000-handoff ring check.

The runtime object and a static ring-check executable cross-compiled for
Linux x86-64/musl with Zig 0.15.2. They were not executed on Linux. Linux
GPU correctness and performance remain unmeasured; the added Ubuntu CI
configuration builds/links the host harness and runs only the CPU check.

## Source fingerprints

SHA-256 of the measured sources and retained CSV:

```text
demo/dispatch.cpp          bd184678e907165a6bb17998b0109e860195e1ba7c5eb8e7f70fc64ad1b924bb
src/runtime.cpp            dc0373c307f571a0442b9587173344e8a3c6004551e2469b0dcdbde917090b51
include/orca_l0/runtime.hpp 39894c9e1dfcc959fc8a1acfdfa7cc55fb434e5c38ba48c86ce1dab5b4d1e2ac
tests/inference.cl         03647efb665f6d2ef16b95ee47526bf42832a71355b6f1c5b78668beefbee1b2
a770-windows-2026-10-29.csv 83052813b98b4d895ee8ab848b9297d91a8b549bb7030a672bc7f64098bd5b7c
```
