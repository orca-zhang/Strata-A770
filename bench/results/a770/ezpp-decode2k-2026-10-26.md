# A770 actual2K ezpp diagnostic

A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034.

The user requested ezpp instead of VTune. Rebuilt only the main driver and verifier of the matched frozen HC-arena engine with the existing scopes and the locally patched Windows/inline ezpp header. Original BF16,6297expert slots,FP16KV,full vocabulary,verify6/MTP4/lookup3,min-p0.5,context/prefill8192,MTPwindow1024,promptcache0 and reserve1536MiB remain fixed. Clear scopes immediately before decode to exclude prefill and initialization; add pending-cache and adapt-join scopes. No production binary/configuration changed.

Each fresh process waits30seconds outside request timing, runs Short16,one warmup per case and two retained interleaved cycles. All12 original2048-input/full128-output requests equal the original BF16 reference,including warmups. Output fingerprints,all low values and speculative counts remain in the JSON. Five Windows clock checks pass; the largest difference from steady_clock is 0.601ms.

## Ordinary command graph

These are host elapsed scopes from the existing graph path. Verifier time includes GPU waiting. Medians use both retained requests; ms values have the profiler report's rounding. Nested scopes and concurrent cache work must not be added to make a total.

| Case | Decode ms | Verify ms | Draft ms | Pending-cache wait ms | Adapt join ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K | 4129.60 | 3495.0 | 581.5 | 37.5 | 3.0 |
| Prose2K | 4192.85 | 3565.0 | 582.5 | 27.5 | 3.0 |

## Eager phase-wait diagnostic

Existing eager phase waits deliberately drain the queue after major phases. This changes submission and overlap: these times include phase synchronization overhead and cannot be transferred as GPU kernel times or stage shares of the command graph. Millisecond resolution also limits small-scope conclusions. The two retained samples per case identify substantial work in experts,attention and HC; they do not prove which kernel implementation will improve unprofiled throughput.

| Scope | Code2K median ms | Prose2K median ms |
| --- | ---: | ---: |
| verify.submit.post.experts | 1840.0 | 1895.0 |
| verify.submit.pre.attn | 1470.0 | 1530.0 |
| verify.submit.pre.hc0 | 726.5 | 721.0 |
| verify.submit.pre.hc1 | 689.0 | 710.0 |
| verify.submit.pre.shared | 803.0 | 804.5 |
| verify.submit.pre.route | 560.5 | 551.5 |
| verify.submit.post.combine | 301.5 | 316.5 |
| verify.submit.post.plan | 35.0 | 37.0 |
| verify.submit.post.misses | 32.5 | 33.5 |
| verify.submit.input | 12.5 | 19.0 |
| verify.submit.head | 99.5 | 98.0 |
| HC0+HC1,combined per request | 1415.5 | 1431.0 |

Every eager layer scope is checked against48layers times the actual window count. Primary phase scopes are reported individually; overlapping parent and child scopes are retained in JSON and excluded from this table.

## All request times

| Mode | Case | Warmup | Decode ms | Accepted / offered |
| --- | --- | --- | ---: | ---: |
| graph | Code2K | True | 4817.9 | 75 / 98 |
| graph | Prose2K | True | 4470.4 | 72 / 99 |
| graph | Code2K | False | 4137.9 | 75 / 98 |
| graph | Prose2K | False | 4146.5 | 73 / 98 |
| graph | Code2K | False | 4121.3 | 75 / 98 |
| graph | Prose2K | False | 4239.2 | 73 / 100 |
| eager-phases | Code2K | True | 7961.8 | 75 / 98 |
| eager-phases | Prose2K | True | 7849.8 | 72 / 99 |
| eager-phases | Code2K | False | 7368.5 | 75 / 98 |
| eager-phases | Prose2K | False | 7508.7 | 73 / 99 |
| eager-phases | Code2K | False | 7369.5 | 75 / 98 |
| eager-phases | Prose2K | False | 7537.0 | 73 / 99 |

This is a completed diagnostic,not a gain screen or performance confirmation. The current screen remains both Code/Prose>=1% with baseline time drift<=3%; eventual acceptance still requires independent unprofiled model comparisons and quality checks. Stable40tok/s remains unmet.

Runner completed scope/output/source/binary audit under its exclusive GPU lock and exited0. Publication reads small reports/results only; does not rehash large binaries during other measurements.

[Data](ezpp-decode2k-2026-10-26.json). Offline check:

```powershell
python bench/results/a770/check_ezpp_decode2k_20261026.py --self-check
```
