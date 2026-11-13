# U4N16/M16 L0: fused fallback dequantization

2026-11-05 Asia/Shanghai; Arc A770 16 GB; Xeon E5-2696 v4; Windows; driver32.0.101.9034; oneAPI2025.3.

The existing `STRATA_PREFILL_FUSED_DEQUANT` switch fuses the gate/up and down dequantization submissions for eligible small experts. The same retained U4/load4/M16 executable, images, weights and arguments are used in both arms. Baseline explicitly selects0, candidate1. The U4 row threshold remains32. No new kernel or decode/GEMV arithmetic change is introduced.

Each process initializes with Short23/16, then runs three interleaved Code2K/128 and Prose2K/128 cycles. Cycle0 is excluded; cycles1/2 supply the median. Outer controls use their geometric mean. The predeclared gate requires both cases to gain at least1% prefill throughput and0.5% request throughput, with absolute outer-control drift at most3%, in independent forward and reverse cohorts.

| Cohort | Case | Prefill s, off → on | Prefill throughput change | Request s, off → on | Request throughput change | Control drift, prefill / request |
|---|---|---:|---:|---:|---:|---:|
| performance-forward-r1 | Code2K | 15.362 → 15.176 | +1.225% | 19.564 → 19.370 | +1.001% | -0.155% / -0.102% |
| performance-forward-r1 | Prose2K | 14.969 → 14.566 | +2.767% | 19.205 → 18.774 | +2.298% | +1.734% / +1.695% |
| performance-reverse-r1 | Code2K | 15.274 → 15.156 | +0.782% | 19.478 → 19.413 | +0.335% | +0.802% / +0.765% |
| performance-reverse-r1 | Prose2K | 14.866 → 14.631 | +1.608% | 19.079 → 18.877 | +1.068% | +1.669% / +1.675% |

Complete generated token sequences and speculative counts match between arms in every performance cycle. The full256-target raw quality TSV byte-matches the frozen U4 reference. All child processes exit successfully. Raw T/TB streams, DONE lines, hashes and model metrics are independently rechecked.

Rejected for adoption: the reverse Code cohort gains only0.782% prefill and0.335% request throughput, below the predeclared1%/0.5% gates despite valid control drift. No additional performance repetition is used to rescue this stable gate failure. The default deployment and retained U4 resources are unchanged; the prepared server/adoption script is not executed.

The exact JSON records every completed cohort and all raw requests; failure logs stay in the text archive. Two retained observations per case/process are a small diagnostic cohort. Inspect raw per-cycle timing as well as medians: forward final-control Prose has a slower15.535s sample. Independent reverse confirmation is required before adoption. Control drift is not a confidence interval. These two prompts on one machine do not establish broad quality, reliability or decode performance.

Recompute: `python -X utf8 build-orca-u4-fallback-20261105/publish.py --check`. Compilation/GPU execution/heavy resource verification uses the shared GPU lock. The archive contains source, configuration, raw logs and JSON; no weights, input binaries, EXEs, objects, device binaries or tensor dumps.

[JSON](../../../sycl/orca/u4-fallback-a770-windows-2026-11-05.json) · Text evidence (`u4-fallback-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
