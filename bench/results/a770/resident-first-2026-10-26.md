# A770 resident-first expert scheduling screen

A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034.

Opposite order after the mapped-first screen failed: stable-partition VRAM experts before mapped-host experts in the actual parallel mirrored GPU plan. Preserve first-occurrence order within each class,entry order,weight bytes,dot products,precision,group-grid size and destination scatter. Default-off;serial and host fallback remain unchanged.

Matched frozen HC-arena EXE,originalBF16,6297slots,FP16KV,full vocabulary,verify6/MTP4/lookup3,min-p0.5,context/prefill8192,MTPwindow1024,promptcache0,reserve1536MiB. Same EXE A1/B1/A2,30seconds settle,Short16,one warmup/case plus three retained interleaved cycles. Profiling disabled. All24 original2048/full128 requests equal the originalBF16 reference. Keep every low value and warmup. All195 GPU-plan checks pass,including default/enabled/serial,mixed/allmapped/VRAM,repeated/unique,empty,64-entry boundary,missing mirror and invalid sizes. Real dispatch witnesses are enabled only in B; these host witnesses do not count device-side residency.

Separate read-only1Hz Sysman monitoring across all arms; no clock or power writes. Every request has successful frequency samples during decode. Sampling does not identify the cause of an individual slow request. Monitor overhead is shared and is not separately quantified.

| Case | Baseline tok/s | Candidate tok/s | Paired gain | Baseline time drift | Candidate minimum tok/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K | 30.138 | 29.185 | -3.163% | +0.255% | 29.175 |
| Prose2K | 29.879 | 29.049 | -2.778% | +0.079% | 28.568 |

Both-case1% gain / <=3% baseline time-drift screen: **FAIL; do not adopt**. This experiment changes no production binary or configuration. Stable40tok/s remains unmet.

| Arm | Case | Warmup | Decode ms | Accepted / offered | MHz min–max | Throttle sampled |
| --- | --- | --- | ---: | ---: | ---: | --- |
| A1 | Code2K | True | 4944.0 | 75 / 98 | 2400–2400 | False |
| A1 | Prose2K | True | 4599.5 | 72 / 99 | 2400–2400 | False |
| A1 | Code2K | False | 4241.7 | 75 / 98 | 2400–2400 | False |
| A1 | Prose2K | False | 4253.8 | 73 / 98 | 2400–2400 | False |
| A1 | Code2K | False | 4271.2 | 75 / 98 | 2400–2400 | False |
| A1 | Prose2K | False | 4343.8 | 73 / 100 | 2400–2400 | False |
| A1 | Code2K | False | 4237.3 | 75 / 98 | 2400–2400 | False |
| A1 | Prose2K | False | 4282.3 | 73 / 98 | 2400–2400 | False |
| B1 | Code2K | True | 5119.6 | 75 / 98 | 1350–2400 | False |
| B1 | Prose2K | True | 4737.8 | 72 / 99 | 2400–2400 | False |
| B1 | Code2K | False | 4387.3 | 75 / 98 | 2400–2400 | False |
| B1 | Prose2K | False | 4398.0 | 73 / 98 | 2400–2400 | False |
| B1 | Code2K | False | 4369.4 | 75 / 98 | 2400–2400 | False |
| B1 | Prose2K | False | 4480.6 | 73 / 100 | 1350–2400 | False |
| B1 | Code2K | False | 4385.8 | 75 / 98 | 2400–2400 | False |
| B1 | Prose2K | False | 4406.4 | 73 / 98 | 2400–2400 | False |
| A2 | Code2K | True | 4956.5 | 75 / 98 | 2400–2400 | False |
| A2 | Prose2K | True | 4686.2 | 72 / 100 | 2400–2400 | False |
| A2 | Code2K | False | 4273.5 | 75 / 98 | 2400–2400 | False |
| A2 | Prose2K | False | 4265.0 | 73 / 98 | 2400–2400 | False |
| A2 | Code2K | False | 4252.5 | 75 / 98 | 2400–2400 | False |
| A2 | Prose2K | False | 4298.5 | 73 / 98 | 2400–2400 | False |
| A2 | Code2K | False | 4242.9 | 75 / 98 | 2400–2400 | False |
| A2 | Prose2K | False | 4285.7 | 73 / 98 | 2400–2400 | False |

Runner completed all device-plan/full-output/dispatch/frequency/source/binary audits under the exclusive GPU lock and exited0. Publication reads small files only.

[Data](resident-first-2026-10-26.json). Offline check:

```powershell
python bench/results/a770/check_resident_first_20261026.py --self-check
```
