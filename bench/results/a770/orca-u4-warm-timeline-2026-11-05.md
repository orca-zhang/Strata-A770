# U4 warm prefill localization - 2026-11-05

Arc A77016 GB; Xeon E5-2696v4; Windows; driver32.0.101.9034; oneAPI2025.3.

Diagnostic only; no new throughput or production adoption claim. The retained U4/unmask/load4 implementation and its device images are reused. Each isolated host TU replaces the old event timer with the previously checked guarded coarse-timestamp recorder, adding one boundary before and after the U4 expert chain. Fine per-expert boundaries remain omitted.

Short23/16 initializer, three alternating Code/Prose2048/128 cycles per backend; warm cycles1/2; exact outputs and speculative work against previous U4 references.

Each long request has591 raw GPU marks. All used and unused slot guards,36-bit wrap handling, phase counts and telescoping sums pass. Each request contains48 U4-chain,48 host-grouping and48 combine intervals,36 GDN recurrence intervals and12 QSA attention intervals. All complete outputs, generated counts, speculative accepted/offered counts, reuse and termination states match the uninstrumented U4 references.

| Backend | Case | GDN input projection ms | GDN output projection ms | QSA projection ms | Projection total ms | U4 chain ms | Gather / fallback experts ms | Full diagnostic span ms |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| l0 | Code2K | 5980.14 | 1774.05 | 2287.76 | 10041.95 | 2980.57 | 8332.50 | 23889.92 |
| l0 | Prose2K | 5974.83 | 1769.72 | 2284.22 | 10028.77 | 2799.07 | 7865.71 | 23187.97 |
| direct | Code2K | 7182.29 | 1961.39 | 2605.43 | 11749.11 | 2939.46 | 6475.22 | 23595.77 |
| direct | Prose2K | 7028.49 | 1905.97 | 2531.63 | 11466.09 | 2701.86 | 5927.45 | 22464.74 |

The projection intervals are a larger target than the isolated U4 chain in these diagnostic requests. The next candidate combines the independently validated generic FP16 M16 projection module with U4/load4, followed by new uninstrumented old/new/old and reverse model comparisons. Independent speed percentages must not be added. Gather/fallback intervals remain a separate substantial target.

A barrier precedes each marker and the enabled prefill timer adds setup waits and logs. Intervals contain GPU work plus host, copy and dependency gaps; they are not exclusive compute times, removable-time bounds or clean throughput measurements. Nominal19.2MHz scaling is retained from the checked A770 diagnostic; it is not a new high-precision clock calibration. No Linux GPU, broad quality or Direct8K reliability conclusion is drawn.

Evidence: [JSON](../../../sycl/orca/u4-warm-timeline-a770-windows-2026-11-05.json), text archive (`u4-warm-timeline-a770-windows-2026-11-05.json.gz`, raw capture retained locally). Recompute with `python -X utf8 build-orca-u4-timeline-20261105/publish.py --check`. All compilation and GPU work hold the shared experiment lock. The archive excludes binaries, weights and tensor dumps.
