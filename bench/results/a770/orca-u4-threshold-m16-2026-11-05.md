# U4N16/load4/M16 L0: min_rows32 versus16

2026-11-05 Asia/Shanghai; Arc A77016GB; Xeon E5-2696v4; Windows; driver32.0.101.9034; oneAPI2025.3.

This experiment lowers only `STRATA_U4_MIN_ROWS` from32 to16 on the adopted U4N16/load4/generic-M16 L0 executable. The same9402 experts, images, cache5500(actual5771), reserve1536, read256 and MTP4 settings are retained. Fused dequant remains off after its independent reverse-order gate failure. More low-row experts may use U4; outputs and speculative counts may differ between arms. Request throughput therefore measures the actual resulting workload, not an isolated kernel change.

The fixed256-target native quality gate requires aggregate PPL increase at most1% and every case at most3%. These are limited diagnostic prompts, not a broad quality evaluation.

| Case | PPL change versus native | PPL change versus current U4/min32 |
|---|---:|---:|
| Code2K | +1.167354% | -0.049370% |
| Prose2K | -0.539840% | +0.421368% |
| Aggregate | +0.310125% | +0.185723% |

Quality gate passes: True.

Each performance process uses Short23/16 initialization and three interleaved Code/Prose2048-input/128-output cycles. Cycle0 is discarded, cycles1/2 give the median, and outer controls use their geometric mean. Both independent old/new/old and new/old/new orders must gain at least1% prefill throughput and0.5% request throughput in both cases, with absolute outer-control drift at most3%. All raw requests, complete outputs, control mismatches and failed attempts are retained.

| Cohort | Case | Prefill s, min32 → min16 | Prefill gain | Request s, min32 → min16 | Request gain | Control drift, prefill / request |
|---|---|---:|---:|---:|---:|---:|
| performance-forward-r1 | Code2K | 15.154 → 14.858 | +1.989% | 19.343 → 19.059 | +1.489% | +0.952% / +0.759% |
| performance-forward-r1 | Prose2K | 14.736 → 14.437 | +2.075% | 18.943 → 18.672 | +1.450% | +2.439% / +1.855% |
| performance-reverse-r1 | Code2K | 15.390 → 14.936 | +3.040% | 19.588 → 19.139 | +2.346% | -0.620% / -0.520% |
| performance-reverse-r1 | Prose2K | 14.809 → 14.443 | +2.532% | 19.015 → 18.689 | +1.745% | -1.263% / -1.069% |

| Cohort / arm | Case | U4 expert calls | U4 routed rows | Explicit weight DMA GiB |
|---|---|---:|---:|---:|
| performance-forward-r1/0-old | Code2K | 6828 | 861351 | 17.582 |
| performance-forward-r1/0-old | Prose2K | 6305 | 871930 | 16.235 |
| performance-forward-r1/1-new | Code2K | 7902 | 887022 | 20.347 |
| performance-forward-r1/1-new | Prose2K | 7229 | 893024 | 18.614 |
| performance-forward-r1/2-old | Code2K | 6828 | 861351 | 17.582 |
| performance-forward-r1/2-old | Prose2K | 6305 | 871930 | 16.235 |
| performance-reverse-r1/0-new | Code2K | 7902 | 887022 | 20.347 |
| performance-reverse-r1/0-new | Prose2K | 7229 | 893024 | 18.614 |
| performance-reverse-r1/1-old | Code2K | 6828 | 861351 | 17.582 |
| performance-reverse-r1/1-old | Prose2K | 6305 | 871930 | 16.235 |
| performance-reverse-r1/2-new | Code2K | 7902 | 887022 | 20.347 |
| performance-reverse-r1/2-new | Prose2K | 7229 | 893024 | 18.614 |

| Cohort | Case | Complete tokens equal between arms | min32 accepted/offered | min16 accepted/offered |
|---|---|---|---|---|
| performance-forward-r1 | Code2K | True | 72/93 | 73/90 |
| performance-forward-r1 | Prose2K | False | 67/92 | 62/96 |
| performance-reverse-r1 | Code2K | True | 72/93 | 73/90 |
| performance-reverse-r1 | Prose2K | False | 67/92 | 62/96 |

The Short23/16 quality initializer emits 0 U4 reports and complete output equality is True. Performance outputs must remain deterministic within each arm; outer controls additionally match complete tokens and speculative counts for each cycle. Changed candidate content/counts are retained and are not reported as decode-kernel acceleration.

Both orders pass. The actual server additionally completes first8000-input/128-output, Short23/16, Code/Prose2K/128, then the same8K/128 again. Both8K outputs and speculative counts agree across that mixed history; Short/Code/Prose match the frozen min16 candidate outputs/counts. Actual localhost health/models and two identical greedy chat responses pass. Long tokens equal the previous min32 reference: False. This is limited compatibility/determinism evidence, not8K quality equivalence. Only STRATA_U4_MIN_ROWS changes to16 in the main config. The optional min16 config is strata-iq2_xs-u4n16-orca-l0-m16-r16.json; previous main bytes remain in build-orca-u4-threshold-m16-20261105/baseline.json. The same9402 assets, launcher RAM guard, native fallback and learned profile are retained. No new decode/GEMV arithmetic change or speedup is claimed. Source preparation initially stopped on an ambiguous substring match before any benchmark was written or executed; the corrected source and diagnostic are retained.

Recompute: `python -X utf8 build-orca-u4-threshold-m16-20261105/publish.py --check`. GPU work and heavy resource hashes use the shared experiment lock. The archive retains source, configuration, raw logs, TSV and JSON, excluding weights, executable/object/device binaries, input binaries and tensor dumps.

[JSON](../../../sycl/orca/u4-threshold-m16-a770-windows-2026-11-05.json) · Text evidence (`u4-threshold-m16-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
