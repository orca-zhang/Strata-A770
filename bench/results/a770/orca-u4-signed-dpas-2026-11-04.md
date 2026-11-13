# U4 signed-DPAS experiment - 2026-11-04

Arc A770 16 GB; Xeon E5-2696 v4; Windows; driver 32.0.101.9034; oneAPI 2025.3.

Rejected for model integration. XORing each stored U4 nibble with 8 recovers the signed weight q from q+8, allowing signed DPAS without activation-sum reads or zero-point subtraction. Existing weights, N16 tiles and the two S4 activation planes are retained. Integer dots and all tested outputs match exactly, but the Direct GU candidate does not improve the measured model requests. The previous GU kernels, optimized U4 EXEs and optional profiles are retained.

Both transports compare 6,604,800 complete GU/Down outputs across 18 cases in every cohort, including tails, gaps, guards and bank reuse. The pilot uses 64-call batches and 9 samples; calibrated runs use 256-call batches and 25 samples, with 0.5 s of pair warmup before each shape/component. Every sample is followed by a complete output/guard check. Long cohorts include forward ABBA and reverse BAAB. Invalid-control results are retained and are not accepted as component gains.

| Cohort | Backend | Component | Geometric screen gain | Maximum control drift | Worst shape gain | All-shape gate |
|---|---|---|---:|---:|---:|---|
| build-r1 | l0 | gu | +9.11% | 6.05% | +2.33% | False |
| build-r1 | l0 | down | +7.39% | 12.48% | +0.83% | False |
| build-r1 | direct | gu | +12.51% | 6.85% | +9.44% | False |
| build-r1 | direct | down | +11.72% | 7.94% | +6.75% | False |
| confirm-r1 | l0 | gu | +9.91% | 2.69% | +0.75% | True |
| confirm-r1 | l0 | down | +7.46% | 3.97% | +3.68% | False |
| confirm-r1 | direct | gu | +12.20% | 5.05% | +8.04% | False |
| confirm-r1 | direct | down | +16.01% | 1.96% | +12.50% | True |
| confirm-r2 | l0 | gu | +11.19% | 4.07% | +6.21% | False |
| confirm-r2 | l0 | down | +7.30% | 4.33% | +1.22% | False |
| confirm-r2 | direct | gu | +11.16% | 2.52% | +5.64% | True |
| confirm-r2 | direct | down | +14.84% | 5.36% | +11.45% | False |
| confirm-r3 | l0 | gu | +9.15% | 3.17% | +0.42% | False |
| confirm-r3 | l0 | down | +7.32% | 4.31% | +2.17% | False |
| confirm-r3 | direct | gu | +11.30% | 2.33% | +5.37% | True |
| confirm-r3 | direct | down | +14.82% | 5.96% | +13.96% | False |

Only Direct GU passes two independent calibrated component cohorts. The model candidate therefore changes Direct GU alone; Down and L0 remain on the previous implementation.

Real-model protocol: the current optimized U4 EXE and the candidate use identical 9,402-expert assets, min_rows 32, FP16 pairs, expert batching, cache 5500 (actual 5771), reserve 1536, MTP 4/read 256 and warm graphs. Quality uses 256 fixed targets and raw scores match the previous U4 scores exactly. Performance uses old/new/old processes, Short16 initialization and three interleaved 2048-input/128-output Code/Prose cycles, discarding the first cycle and retaining the median of two. Initial U4 loading is included only in the discarded first long request. All complete outputs equal the previous U4 outputs.

| Case | Previous U4 prefill ms | Signed GU prefill ms | Prefill throughput change | Request throughput change | Prefill / request control drift |
|---|---:|---:|---:|---:|---:|
| Code2K | 21798.44 | 22074.05 | -1.25% | -1.03% | +2.28% / +1.76% |
| Prose2K | 21398.84 | 21546.95 | -0.69% | -0.56% | +1.58% / +1.51% |

The model gate requires at least 1% higher prefill throughput in each case, no request regression beyond 1%, and control drift at most 3%. This first cohort fails, so the stop rule omits the reverse run. Positive hot-kernel screens do not establish real-model gains. This result does not claim decode/GEMV acceleration or resolve the existing Direct 8K issue.

Evidence: [JSON](../../../sycl/orca/u4-signed-dpas-a770-windows-2026-11-04.json), text archive (`u4-signed-dpas-a770-windows-2026-11-04.json.gz`, retained locally for privacy). Recompute with `python -X utf8 build-orca-u4-signed-20261104/publish.py --check`. Compilation and GPU work use the shared experiment lock; binaries, weights, input tensors and dumps are excluded.
