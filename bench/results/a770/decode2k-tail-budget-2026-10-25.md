# A770 original 2K/128: remaining-output budget

A77016GB / PCIe3.0, Windows, driver32.0.101.9034, DPC++2025.3.2. All A1/B1/A2 arms use the same isolated executable, matched frozen generator headers and unchanged engine archives. Only STRATA_TAIL_BUDGET=0/1/0 changes: after suffix selection, clamp verifier T to the remaining output token count. The source audit verifies only the switch and this clamp were added. Draft generation itself is unchanged. This tests an older frozen generator; the working source already has an output-budget clamp, so this is not a new production patch.

Fixed settings: requested expert count6000/actual6297 slots, FP16 KV, reserve1536MiB, context/prefill8192, explicit CLI spec6/mtp-max-t4/suffix3, min-p0.5, MTP attention1024, full248320-row vocabulary, prompt-cache0, adapt-every1/swaps32/decay0.5. The explicit capacities reproduce the earlier CLI-spec4 automatic verify6/MTP4 capacities.

Each fresh process runs Short16 initialization, max1/2/3 short output boundary checks, one warmup per original2K case and three interleaved retained cycles. All24 complete2048-input/128-output requests must match frozen references (3072 tokens). All9 short boundary outputs match the corresponding initializer prefixes. Every warmup and low observation is retained. Cache residency and the suffix policy continue adapting across requests.

| Arm | Clamp | Case | Decode median tok/s | Full retained range |
| --- | ---: | --- | ---: | ---: |
| A1 | 0 | Code2K | 30.961 | 30.795–30.996 |
| A1 | 0 | Prose2K | 30.610 | 30.111–30.722 |
| B1 | 1 | Code2K | 31.155 | 31.017–31.166 |
| B1 | 1 | Prose2K | 30.692 | 30.361–30.847 |
| A2 | 0 | Code2K | 30.853 | 30.768–30.970 |
| A2 | 0 | Prose2K | 30.585 | 30.097–30.707 |

| Case | Paired throughput change | Adjacent baseline time drift |
| --- | ---: | ---: |
| Code2K | +0.802% | +0.351% |
| Prose2K | +0.308% | +0.084% |

Both cases must gain>=3% with<=3% absolute baseline time drift. Screen passed: **False**. Positive results still require independent confirmation. This cohort establishes neither stable40 tok/s nor general model quality. Production executable/configuration is unchanged.

A separate read-only Sysman monitor samples at1Hz with coverage during every decode. All per-request frequencies, observed throttle flags, accepted/offered counts, startup free VRAM and timing ranges are retained. Sampling cannot rule out short changes or prove a paging cause; startup memory is not peak request memory. Monitoring overhead is not separately quantified.

[Data](decode2k-tail-budget-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_decode2k_tail_budget_20261025.py
```

The optional --local flag audits frozen raw artifacts. Run it under the shared lock on the measurement machine.
