# A770 original 2K/128: adaptive-cache decay

A77016GB / PCIe3.0, Windows, driver32.0.101.9034, DPC++2025.3.2. Change only --adapt-decay0.5/0.75/0.5 on the frozen graph engine, with requested expert count6000/actual6297 slots, FP16 KV, reserve1536MiB, context/prefill8192, CLI spec4/min-p0.5, MTP attention1024, full248320-row vocabulary, prompt-cache0, adapt-every1/swaps32. The verifier, precision and weight layout are unchanged.

The candidate followed a fixed6297-slot historical Python replay estimating3.112%/4.146% fewer mapped blob bytes for Code/Prose. That is not PCIe traffic or model throughput measurement. Its first manifest attempt failed before creating a protocol because the layout file is outside the repository; a fresh directory corrected only external-path registration, preserving the failed source. The recovered replay retained both values and all three cases, with per-window cardinality/capacity checks.

Each fresh A1/B1/A2 process runs Short16 initialization, one warmup/case and three interleaved retained cycles. All24 original2048-input/full128-output requests match frozen references (3072 tokens). Every warmup and low observation is retained. Expert residency and the suffix lookup policy continue adapting across requests; prompt-cache0 does not reset them.

| Arm | Decay | Case | Decode median tok/s | Full retained range |
| --- | ---: | --- | ---: | ---: |
| A1 | 0.5 | Code2K | 30.966 | 30.793–30.982 |
| A1 | 0.5 | Prose2K | 30.557 | 30.117–30.735 |
| B1 | 0.75 | Code2K | 30.590 | 30.492–30.719 |
| B1 | 0.75 | Prose2K | 30.345 | 29.840–30.378 |
| A2 | 0.5 | Code2K | 30.980 | 30.786–31.010 |
| A2 | 0.5 | Prose2K | 30.604 | 30.583–30.760 |

| Case | Paired throughput change | Adjacent baseline time drift |
| --- | ---: | ---: |
| Code2K | -1.238% | -0.044% |
| Prose2K | -0.769% | -0.155% |

Both cases must gain>=3% with<=3% absolute baseline time drift. Screen passed: **False**. A positive screen still needs independent confirmation; stable40 tok/s is not established by this cohort. No production change or general quality claim.

A predeclared separate read-only Sysman process samples once per second, with at least one sample during every decode. All per-request frequency/throttle observations, accepted/offered counts, startup free VRAM and timing ranges are retained in the compact data. Monitoring overhead is not separately quantified, and sampling gaps do not rule out short changes or establish a paging cause.

[Data](decode2k-decay-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_decode2k_decay_20261025.py
```

The optional --local flag audits frozen local artifacts. Run it under the shared lock on this measurement machine to avoid hashing contention.
