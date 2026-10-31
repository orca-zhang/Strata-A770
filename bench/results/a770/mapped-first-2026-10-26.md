# A770 mapped-first expert scheduling screen

A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034.

Following the actual2K ezpp diagnostic, test a stable partition of the real GPU expert plan: mapped-host experts first,then VRAM experts. Retain first-occurrence order within each class and entry order within each expert. Weight bytes, dot-product arithmetic, precision, grid size and destination scatter remain unchanged. This targets earlier PCIe requests in the mixed-residency path that the previous VRAM-only candidate did not reach.

One isolated matched-ABI frozen HC-arena EXE,original BF16,6297slots,FP16KV,full vocabulary,verify6/MTP4/lookup3,min-p0.5,context/prefill8192,MTPwindow1024,promptcache0,reserve1536MiB. A1/B1/A2 differ only by the default-off scheduling flag. Profiling is disabled. Each process waits30seconds outside timing,runs Short16,one warmup/case and three retained interleaved cycles. No Sysman monitor in this cohort. All24 original2048-input/full128-output requests must equal the originalBF16 reference; all lows and warmups are retained.

195 GPU-plan checks cover default,enabled and enabled-with-serial-fallback modes: VRAM,mixed,allmapped,unique/repeated experts,empty plans,the64-entry boundary,missing-mirror fallback and invalid sizes. Full plan bytes,group starts,pointers,token indices,destinations and untouched sentinels are checked. First-use host witnesses prove the real mirrored parallel dispatch has the candidate enabled only in B; they do not count device-side residency.

| Case | Baseline tok/s | Candidate tok/s | Paired gain | Baseline time drift | Candidate minimum tok/s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K | 30.119 | 29.628 | -1.629% | +0.668% | 29.483 |
| Prose2K | 29.803 | 29.459 | -1.154% | +0.040% | 29.344 |

Both-case1% gain / <=3% time-drift screen: **FAIL; do not adopt**. This experiment changes no production binary or configuration. Stable40tok/s remains unmet.

| Arm | Case | Warmup | Decode ms | Accepted / offered |
| --- | --- | --- | ---: | ---: |
| A1 | Code2K | True | 6377.8 | 75 / 98 |
| A1 | Prose2K | True | 4606.8 | 72 / 99 |
| A1 | Code2K | False | 4261.0 | 75 / 98 |
| A1 | Prose2K | False | 4294.0 | 73 / 99 |
| A1 | Code2K | False | 4235.7 | 75 / 98 |
| A1 | Prose2K | False | 4270.8 | 73 / 98 |
| A1 | Code2K | False | 4231.2 | 75 / 98 |
| A1 | Prose2K | False | 5234.4 | 73 / 100 |
| B1 | Code2K | True | 5058.5 | 75 / 98 |
| B1 | Prose2K | True | 4803.4 | 72 / 100 |
| B1 | Code2K | False | 4341.5 | 75 / 98 |
| B1 | Prose2K | False | 4338.0 | 73 / 98 |
| B1 | Code2K | False | 4307.5 | 75 / 98 |
| B1 | Prose2K | False | 4362.0 | 73 / 98 |
| B1 | Code2K | False | 4320.2 | 75 / 98 |
| B1 | Prose2K | False | 4345.0 | 73 / 98 |
| A2 | Code2K | True | 4933.4 | 75 / 98 |
| A2 | Prose2K | True | 4597.4 | 72 / 99 |
| A2 | Code2K | False | 4262.0 | 75 / 98 |
| A2 | Prose2K | False | 4273.0 | 73 / 98 |
| A2 | Code2K | False | 4277.9 | 75 / 98 |
| A2 | Prose2K | False | 4363.0 | 73 / 100 |
| A2 | Code2K | False | 4264.0 | 75 / 98 |
| A2 | Prose2K | False | 4295.7 | 73 / 98 |

The runner completed device-plan, full-output, dispatch and source/binary hash audit under its exclusive GPU lock and exited0. Publication reads only small reports/results; no large binary reads or GPU commands.

[Data](mapped-first-2026-10-26.json). Offline check:

```powershell
python bench/results/a770/check_mapped_first_20261026.py --self-check
```
