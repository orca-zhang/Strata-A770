# A770 HC Q8: independent confirmation at the updated1% gate

A77016GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2.

The user changed the original3% gain gate to **both cases>=1%**. Baseline time drift remains<=3%. Original measurements/protocols remain in the [first report](hc-q8-model-2026-10-25.md); the [rule update](hc-q8-model-1pct-update-2026-10-25.json) changes its active screen decision to pass without changing timings.

The first independent attempt stopped after15 complete2K/128 requests (A1 eight,B1 seven), with no final BF16 arm. Its runner and child were verified absent; the stale lock owner PID had been reused by an unrelated process. The interruption cause is unknown. All files are preserved and excluded from confirmation; this is a new complete cohort.

This independent cohort reuses the identical frozen EXE and configuration: originalBF16/cache6000/6297slots versus192 main-layer HC Q8/cache6350/6666slots. Reserve1536MiB, FP16 KV, full vocabulary, verify6/MTP4/lookup3,min-p0.5,context/prefill8192,MTPwindow1024,promptcache0. Each fresh process waits30s outside timing, runs Short16, one warmup/case and three retained interleaved cycles. All24 original2048-input/full128-output requests are kept. Both arms must reproduce the complete outputs of the previous same arm. Because Q8 can change continuations, this measures generated decode throughput, not equal-output arithmetic work.

| Cohort | Case | Candidate median tok/s | Paired gain | Baseline time drift |
| --- | --- | ---: | ---: | ---: |
| Original | Code2K | 31.122 | +1.058354% | +1.544392% |
| Original | Prose2K | 31.114 | +1.958322% | +1.293310% |
| Independent | Code2K | 31.048 | +0.204424% | -0.653847% |
| Independent | Prose2K | 30.677 | +0.182038% | -0.291434% |

| Arm | Case | All retained tok/s |
| --- | --- | --- |
| A1 | Code2K | 30.722, 30.883, 30.915 |
| A1 | Prose2K | 30.692, 29.956, 30.577 |
| B1 | Code2K | 30.709, 31.048, 31.103 |
| B1 | Prose2K | 30.456, 30.677, 31.409 |
| A2 | Code2K | 30.924, 31.095, 31.086 |
| A2 | Prose2K | 30.810, 29.891, 30.666 |

The runner completed the full raw/source hash audit under its exclusive GPU lock before successfully exiting. This offline publication reuses the audited executable digest, checks it against the first public report, and independently recomputes the compact result. It does not repeat large binary reads while another experiment is timing.

Both cohorts pass the updated1% performance rule: **False**.

The independent performance prerequisite failed, so the prepared enlarged-cache quality run was not launched.

Candidate accepted under these speed/limited-quality checks: **False**. No production EXE/configuration changed. Stable40tok/s remains **unmet**.

All raw warmups and low samples, complete output hashes, accepted/offered counts and24 read-only1Hz frequency summaries are retained. Frequency sampling cannot exclude shorter events or identify a unique cause of variation.

[Data](hc-q8-model-1pct-confirm-2026-10-26.json). Offline check:

```powershell
python bench/results/a770/check_hc_q8_1pct_confirm_20261026.py
```
