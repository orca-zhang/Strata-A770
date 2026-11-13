# U4N16 + generic M16: real-model combination

Arc A770 16 GB; Xeon E5-2696 v4; Windows; driver 32.0.101.9034; oneAPI 2025.3. Measured 2026-11-05 Asia/Shanghai (2026-11-04 UTC).

The retained U4/unmask/four-reader loader executable had not included the separately verified generic FP16 M16 projection. This experiment links those two existing changes together. Its baseline is the current U4/load4 executable, not native IQ2. Only the XMX host object and its matching four-export device pack change; U4 kernels, loader, weights and other device modules stay the same. The final production kernel implementations were already adopted separately. The benchmark does not change the main configuration. After comparison, the user explicitly requests U4 as the primary solution; the qualified L0 configuration is promoted as described below.

## Steady model measurements

Each backend has independent old/new/old and new/old/new process cohorts. Each process first runs a 23-input/16-output Short request, then three interleaved Code and Prose requests, each with 2048 input tokens and 128 generated tokens. Cycle 0 is excluded from steady medians; cycles 1/2 are retained. The outer controls use a geometric mean, including the reversed cohort after normalization. No profiling probes or added synchronization are present. Cache request 5500 (actual 5771), reserve 1536 MiB, read 256, MTP4/min-p0.5, cumulative0, suffix0 and prompt-cache0 are unchanged.

| Backend | Order | Case | Prefill s, load4 → combined | Prefill tok/s, load4 → combined | Prefill throughput gain | Request s, load4 → combined | Request throughput gain | Control drift: prefill / request |
|---|---|---|---:|---:|---:|---:|---:|---:|
| l0 | old/new/old | Code2K | 22.908 → 15.340 | 89.40 → 133.51 | +49.34% | 27.146 → 19.524 | +39.04% | +1.28% / +1.23% |
| l0 | old/new/old | Prose2K | 22.208 → 14.837 | 92.22 → 138.03 | +49.68% | 26.457 → 19.042 | +38.94% | +0.27% / +0.33% |
| l0 | new/old/new | Code2K | 22.476 → 15.241 | 91.12 → 134.37 | +47.47% | 26.672 → 19.437 | +37.22% | +0.96% / +0.70% |
| l0 | new/old/new | Prose2K | 21.993 → 14.749 | 93.12 → 138.86 | +49.11% | 26.186 → 18.954 | +38.15% | +0.14% / +0.18% |
| direct | old/new/old | Code2K | 21.299 → 13.608 | 96.16 → 150.50 | +56.52% | 25.611 → 17.902 | +43.06% | +0.02% / +0.03% |
| direct | old/new/old | Prose2K | 20.822 → 13.165 | 98.36 → 155.57 | +58.17% | 25.137 → 17.491 | +43.71% | +0.27% / +0.30% |
| direct | new/old/new | Code2K | 21.025 → 13.483 | 97.41 → 151.90 | +55.94% | 25.302 → 17.761 | +42.46% | +0.03% / +0.06% |
| direct | new/old/new | Prose2K | 20.608 → 13.054 | 99.38 → 156.89 | +57.87% | 24.912 → 17.347 | +43.61% | -1.08% / -0.90% |

Prefill tok/s is 2048 divided by prompt time. Request time is client wall time for the entire 128-output-token request; its throughput change is the inverse time ratio for the same fixed output count. These are two fixed diagnostic workloads on one machine, with two warm observations per case and process; control drift is not a confidence interval. Earlier independent U4 and M16 percentages are not added. Absolute decode, TTFT, first Code request and READY times remain in JSON; no new decode/GEMV speedup is claimed.

The predeclared gate requires each case to gain at least 5% prefill throughput and 3% request throughput, with absolute prefill/request outer-control drift within 3%, in both orders on each backend. All four selected cohorts pass.

## Correctness and retained resources

Both backends match the entire previous U4 raw 256-target quality TSV byte for byte. Every complete generated token sequence matches the frozen U4 baseline; speculative accepted/offered counts match between arms within each cohort. Processes exit successfully. After preservation, both real optional configurations run Short16 + Code2K/128 + Prose2K/128 again and match complete outputs. The earlier U4 versus native aggregate PPL diagnostic remains +0.12417175%; this combination does not broaden that quality evaluation.

U4N16 remains available: 9,402 experts, 25,994,649,600 payload bytes (24.21 GiB), 44,236,800 bytes of device banks, and a 67,891,552,256-byte (63.23 GiB) RAM preparation allowance. This allowance is not a measured minimum. The existing retained U4 resources are reused; no weight conversion, duplication or replacement occurs. Previous executables and optional configurations remain intact. The previous main configuration is backed up before promotion.

New optional configurations are `strata-iq2_xs-u4n16-orca-l0-m16.json` and `strata-iq2_xs-u4n16-orca-direct-m16.json`. They select `STRATA_ORCA_XMX_WIDE=1` and separately preserved executable/images directories. The frozen U4 prefill object retains its matching older unmask kernel ABI; replacing only its U4 images with a newer generator pack is not supported. L0 remains the recommended transport while the existing intermittent Direct 8K issue remains open.

## Primary local entry

At the user's explicit request, `strata-iq2_xs.json` now selects the retained U4N16/load4/M16 L0 executable and its measured configuration. The existing `run-iq2_xs.bat` queries available physical RAM using Windows PowerShell before startup: at least 67,891,552,256 available bytes selects U4; low or unknown RAM selects `strata-iq2_xs-native-before-u4-20261105.json`. The launcher uses the original server and preserves port 8080. The original model identifier is kept for existing clients. This changes this local machine's main entry; installer-wide defaults are not changed.

The actual launcher expression passes tests just below, exactly at and just above the RAM threshold, plus a failed-query test. The proposed primary configuration then runs through the actual server StrataEngine and child environment: complete Short16, Code2K/128 and Prose2K/128 outputs match the frozen U4 reference, followed by real local HTTP health/models and two identical greedy chat responses. The main files are switched only after these checks, with the launcher written before the configuration. The original main configuration and launcher snapshots remain in the archive.

L0 additionally checks a first 8000-input/128-output request in fresh old-U4/load4 and combined processes without Short warmup. Both complete outputs and speculative accepted/offered counts match, U4 executes, and both processes exit successfully. This is one compatibility pair, not steady 8K performance evidence or a broad reliability claim; it does not close Direct 8K faults.

The previous daily configuration used automatic cache sizing, read1024, decay0.5 and cumulative0.5. The qualified primary profile retains measured fixed cache5500 (actual5771), read256, decay0.95 and cumulative0. Accordingly, the performance table measures the M16 combination against U4/load4 under identical settings; it does not quantify total improvement against the previous daily configuration. The native fallback preserves that original daily configuration exactly.

The first proposed primary inherited persistent expert-profile learning from the daily configuration. Its GPU/HTTP checks passed but evidence serialization failed on an incorrect tokenizer source path; that attempt is retained and excluded. The next startup selected the learned profile and allocated5783 rather than5771 cache slots, so the equality guard rejected it before requests. The final primary uses the original measured expert profile and omits persistent profile loading/saving; in-process cache adaptation remains enabled. The native fallback retains its original learning configuration and latest learned profile. The final server run checks that its actual arguments equal the measured configuration and that the native learned file stays unchanged. Both earlier diagnostics remain in the archive, excluding their binary profile snapshot.

The independent implementations and checks are documented in [U4 optimization](orca-u4-optimization-2026-11-04.md), [parallel loading](orca-u4-parallel-loading-2026-11-04.md) and [generic M16 projection](orca-generic-projection-m16-2026-11-04.md).

Evidence: [JSON](../../../sycl/orca/u4-m16-combined-a770-windows-2026-11-05.json), text archive (`u4-m16-combined-a770-windows-2026-11-05.json.gz`, raw capture retained locally). Recompute with `python -X utf8 build-orca-u4-m16-20261105/publish.py --check`. Compilation, GPU execution and heavy resource verification use the shared experiment lock. The archive includes source snapshots, commands, logs, configurations and results; weights, executable/object/device binaries, input binaries and tensor dumps are excluded.

Rejected diagnostic: l0/performance-forward-r1 has up to 3.687% outer-control drift. Its complete outputs pass, but performance is excluded from adoption. Both orders are repeated independently under the original gates; the failed cohort and repeat plan remain in the archive.
