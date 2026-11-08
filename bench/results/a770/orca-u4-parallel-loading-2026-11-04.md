# U4 parallel loading - 2026-11-04 UTC

Arc A770 16 GB; Xeon E5-2696 v4; Windows; driver 32.0.101.9034; oneAPI 2025.3.

Adopted: four bounded standard-library async readers load disjoint expert payloads directly into the existing pinned per-layer allocations. GPU allocation stays on the main thread; worker futures join before allocation cleanup on errors. All file-length, header and finite-positive-scale checks remain. No staging buffers, new dependencies or GPU arithmetic changes are introduced. Previous executables and profiles remain available, and the default deployment is unchanged.

The resource set remains 9,402 experts: 25,994,649,600 payload bytes (24.21 GiB), 44,236,800 bytes of device banks, and a 67,891,552,256-byte (63.23 GiB) RAM preparation budget. The budget is not a measured minimum. The internal constructor accepts1/2/4 readers for calibration, default4.

## Isolated loader

Every run compares all 25,994,649,600 loaded bytes with the source files. File hashes warm the OS cache before construction timing. These measurements exclude the full engine and request processing.

| Cohort | Mode | Readers | Pinned allocation ms | Read and validate ms | Total ms |
|---|---|---:|---:|---:|---:|
| probe-r1 | direct-to-pinned | 1 | 2270.87 | 28530.58 | 30801.45 |
| probe-r1 | direct-to-pinned | 4 | 2260.11 | 8975.04 | 11235.15 |
| probe-r1 | direct-to-pinned | 1 | 2346.41 | 27288.91 | 29635.32 |
| probe-reverse-r1 | direct-to-pinned | 4 | 2201.51 | 9113.64 | 11315.15 |
| probe-reverse-r1 | direct-to-pinned | 1 | 2102.55 | 26714.64 | 28817.19 |
| probe-reverse-r1 | direct-to-pinned | 4 | 2291.42 | 9191.19 | 11482.61 |
| cached-r1 | original | 1 | 2194.53 | 27856.75 | 30051.28 |
| cached-r1 | cached | 1 | 2187.35 | 30266.50 | 32453.84 |
| cached-r1 | cached | 4 | 2215.11 | 9556.24 | 11771.35 |
| cached-r1 | original | 1 | 2285.44 | 27412.17 | 29697.61 |

The independent4/1/4 cohort reduces allocation plus loading time by 60.45%, with +1.48% parallel-control drift. The initial1/4/1 controls drift beyond3% and are retained as diagnostic evidence. A separate ordinary-memory staging-buffer experiment is slower than the simple direct-to-pinned loader and is rejected.

## Real model

Each transport runs independent old/new/old and new/old/new process cohorts with identical retained GPU images, inputs, resource files and settings. A Short23-token/16-output initializer precedes three interleaved Code/Prose2048-input/128-output cycles. The first Code request triggers lazy U4 loading; it is measured separately. Steady performance uses the median of cycles1/2. Files are already in the OS cache: these are fresh-process first-long-request measurements, not cold storage or complete process-startup latency.

| Backend | Order | Previous U4 first Code request s | Parallel first Code request s | Request time reduction | Prompt / request control drift |
|---|---|---:|---:|---:|---:|
| l0 | performance-forward-r1 | 58.002 | 39.855 | 31.29% | +1.03% / +1.00% |
| l0 | performance-reverse-r1 | 58.403 | 39.325 | 32.67% | -0.91% / -0.98% |
| direct | performance-forward-r1 | 59.509 | 42.068 | 29.31% | -1.54% / -1.42% |
| direct | performance-reverse-r1 | 57.996 | 41.399 | 28.62% | +0.05% / +0.07% |

| Backend | Order | Case | Warm prefill throughput change | Warm request throughput change | Prefill / request control drift |
|---|---|---|---:|---:|---:|
| l0 | performance-forward-r1 | Code2K | +0.23% | +0.32% | +0.60% / +0.53% |
| l0 | performance-forward-r1 | Prose2K | +0.02% | +0.12% | +0.03% / +0.04% |
| l0 | performance-reverse-r1 | Code2K | -0.08% | -0.28% | -0.80% / -0.83% |
| l0 | performance-reverse-r1 | Prose2K | -0.43% | -0.56% | -0.67% / -0.63% |
| direct | performance-forward-r1 | Code2K | +0.13% | +0.15% | -0.91% / -0.85% |
| direct | performance-forward-r1 | Prose2K | +0.11% | +0.09% | -0.24% / -0.19% |
| direct | performance-reverse-r1 | Code2K | -0.76% | -0.68% | -0.69% / -0.57% |
| direct | performance-reverse-r1 | Prose2K | -0.38% | -0.37% | +0.36% / +0.43% |

All complete generated outputs match the previous optimized U4 implementation. Each transport also matches the entire raw quality TSV byte for byte over256 fixed diagnostic targets; the earlier U4 versus native aggregate PPL change remains +0.12417175%. This loading change establishes no additional steady GEMM or decode/GEMV speedup. The earlier measured U4 prefill gains remain documented in [the preceding optimization report](orca-u4-optimization-2026-11-04.md).

The adoption gate requires at least15% less first-request time, no steady prefill/request throughput regression beyond3%, and control drift within3% in both independent orders. All gates pass. Each transport checks all payload bytes with the final simplified loader, cleanly rejects copied NaN-scale/header/length/missing-file fixtures, and compares3,302,400 GU/Down outputs against the original SYCL references. Both actual CMake U4 targets compile. Retained optional profiles are then executed and their complete Code/Prose outputs checked again.

Runnable payload check: set `STRATA_PREFILL_U4_EXPERT_DIR` to the retained asset directory, then run `strata-orca-u4n16-check <images> unused load`. It checks every loaded byte, including all scales; malformed files fail before inference. The existing `compare` mode checks arithmetic against SYCL references.

Optional configurations: `strata-iq2_xs-u4n16-orca-l0-load4.json` and `strata-iq2_xs-u4n16-orca-direct-load4.json`. They use separately preserved executables/images and the existing retained U4 resources; no weight files are copied or replaced. The default deployment is not switched. The existing intermittent Direct8K issue remains open.

Evidence: [JSON](../../../sycl/orca/u4-parallel-loading-a770-windows-2026-11-04.json), [text archive](../../../sycl/orca/u4-parallel-loading-a770-windows-2026-11-04.json.gz). Recompute with `python -X utf8 build-orca-u4-loading-20261104/publish.py --check`. Compilation and all GPU execution use the shared experiment lock. Weights, binaries and tensor dumps are excluded from the archive.
