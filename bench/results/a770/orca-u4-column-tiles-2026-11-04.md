# U4 column-tile experiments - 2026-11-04

Arc A770 16 GB; Xeon E5-2696 v4; Windows; driver 32.0.101.9034; oneAPI 2025.3.

No tile change is adopted. GU32/64 has no stable advantage across the tested shapes. Down32 with 256 GRF shows positive signals on Direct, but its reverse cohort exceeds the 3% control-drift limit. The existing N16 profiles, weight files and default deployment are retained. No real-model speedup is claimed.

The isolated header keeps the existing math and U4 format. Tiles 16/32/64 and 128/256 GRF are compared using the same eight real BF16-derived experts. Both transports match 19,814,400 GU/Down outputs byte for byte with the frozen SYCL reference across 54 cases, including tails, routed gaps, guards and bank reuse. The focused GU16/128 plus Down32/256 chain also matches 6,604,800 outputs across 18 cases per transport and per process cohort.

The pilot uses ABBA, 16 warmups and 9 samples of 8 calls. Focused runs use 64 warmups and 9 samples of 64 calls, followed by complete output/guard comparison after each sample; the second run reverses shape order and uses BAAB. Host submission and waits are timed. Weight DMA, quantization, SwiGLU and the rest of the model are excluded. Neither these hot component results nor invalid-control pilot averages establish request acceleration.

| Cohort | Backend | Component | Columns | GRF | Geometric screen gain (unconfirmed) | Maximum control drift | Worst shape gain | All-shape gate |
|---|---|---|---:|---:|---:|---:|---:|---|
| build-r1 | l0 | down | 16 | 256 | -1.27% | 19.47% | -6.38% | False |
| build-r1 | l0 | down | 32 | 128 | +0.92% | 17.52% | -8.37% | False |
| build-r1 | l0 | down | 32 | 256 | +10.22% | 14.45% | +2.78% | False |
| build-r1 | l0 | down | 64 | 128 | -43.09% | 16.16% | -56.18% | False |
| build-r1 | l0 | down | 64 | 256 | -0.04% | 16.83% | -11.84% | False |
| build-r1 | l0 | gu | 16 | 256 | -11.62% | 12.00% | -15.74% | False |
| build-r1 | l0 | gu | 32 | 128 | -22.18% | 15.10% | -34.89% | False |
| build-r1 | l0 | gu | 32 | 256 | -1.21% | 9.60% | -16.34% | False |
| build-r1 | l0 | gu | 64 | 128 | -59.21% | 13.43% | -70.59% | False |
| build-r1 | l0 | gu | 64 | 256 | -9.28% | 14.38% | -31.92% | False |
| build-r1 | direct | down | 16 | 256 | -1.88% | 12.54% | -3.84% | False |
| build-r1 | direct | down | 32 | 128 | +2.54% | 5.64% | -14.97% | False |
| build-r1 | direct | down | 32 | 256 | +8.36% | 9.12% | -2.36% | False |
| build-r1 | direct | down | 64 | 128 | -48.01% | 8.28% | -56.44% | False |
| build-r1 | direct | down | 64 | 256 | -2.35% | 5.62% | -21.20% | False |
| build-r1 | direct | gu | 16 | 256 | -7.93% | 3.91% | -12.17% | False |
| build-r1 | direct | gu | 32 | 128 | -22.95% | 6.75% | -39.18% | False |
| build-r1 | direct | gu | 32 | 256 | -1.21% | 11.41% | -10.28% | False |
| build-r1 | direct | gu | 64 | 128 | -56.92% | 5.77% | -71.22% | False |
| build-r1 | direct | gu | 64 | 256 | -5.64% | 5.80% | -31.73% | False |
| focus-r1 | l0 | down | 32 | 256 | +6.99% | 11.35% | -3.16% | False |
| focus-r1 | direct | down | 32 | 256 | +12.19% | 2.24% | +2.23% | True |
| focus-r2 | l0 | down | 32 | 256 | +5.35% | 10.51% | -2.81% | False |
| focus-r2 | direct | down | 32 | 256 | +16.35% | 9.22% | +5.95% | False |

The predeclared component gate requires all five shapes, absolute control drift at most 3%, geometric throughput gain at least 3%, and no shape regression beyond 1%, in two independent process cohorts. Direct Down32 passes only the forward focused run; no focused candidate passes both runs. All results, including invalid controls and negative candidates, are retained.

Evidence: [JSON](../../../sycl/orca/u4-column-tiles-a770-windows-2026-11-04.json), text archive (`u4-column-tiles-a770-windows-2026-11-04.json.gz`, retained locally for privacy). Recompute with `python -X utf8 build-orca-u4-tiles-20261104/publish.py --check`. Compilation and GPU checks use the shared GPU experiment lock. No weights, reference tensors, dumps, EXEs or shader binaries are archived.
