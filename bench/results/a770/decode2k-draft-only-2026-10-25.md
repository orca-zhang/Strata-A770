# A770: fixed-cache draft vocabulary screen

A770 16GB / PCIe3.0; driver32.0.101.9034, DPC++2025.3.2. Same frozen engine and command graphs, FP16 KV, context/prefill8192, CLI spec4/min_p0.5, prompt-cache0.

A uses the full248320 draft vocabulary. B uses the installed106299 CJK/English/code subset, adding137.9MiB of VRAM. Only the MTP directory changes; draft dense/expert files remain the same native hardlinks. The verifier retains its full248320 vocabulary. Both request5500 cache slots/reserve1536MiB and actually get5771 slots.

Fresh A1/B1/A2/B2/A3 processes, each with Short16 initialization, one warmup per case, then three interleaved retained cycles. All40 original2048-input requests completed128 outputs, each identical to its frozen reference. Every warmup and low sample is retained. GPU/compiler lock held; no profiler in these runs.

| Mode | Case | Median tok/s | Retained range |
| --- | --- | ---: | ---: |
| A1 | Code2K | 29.475 | 29.378 to 29.492 |
| A1 | Prose2K | 29.466 | 28.921 to 29.573 |
| B1 | Code2K | 29.870 | 29.738 to 29.900 |
| B1 | Prose2K | 29.033 | 28.818 to 29.397 |
| A2 | Code2K | 29.366 | 29.337 to 29.515 |
| A2 | Prose2K | 29.449 | 28.592 to 29.467 |
| B2 | Code2K | 29.684 | 29.622 to 29.734 |
| B2 | Prose2K | 28.909 | 28.373 to 28.966 |
| A3 | Code2K | 29.330 | 28.841 to 29.450 |
| A3 | Prose2K | 29.108 | 28.632 to 29.517 |

| Candidate | Case | Paired throughput change | Baseline time drift | Eligible |
| --- | --- | ---: | ---: | --- |
| B1 | Code2K | +1.528% | +0.373% | True |
| B1 | Prose2K | -1.441% | +0.058% | True |
| B2 | Code2K | +1.144% | +0.122% | True |
| B2 | Prose2K | -1.261% | +1.171% | True |

Confirmed gain: False. Stable40 target achieved: False.

Acceptance requires at least3% paired gain in BOTH B comparisons for BOTH cases and at most3% adjacent baseline drift. Stable40 additionally requires EVERY retained B request in both cases to reach40tok/s. This is a fixed-workload screen; no general quality or production adoption claim. No kernel-specific attribution or addition of historical speedups.

[All numeric samples](decode2k-draft-only-2026-10-25.json). Check without a GPU:

```powershell
python bench/results/a770/check_decode2k_draft_only_20261025.py
python bench/results/a770/check_decode2k_draft_only_20261025.py --local
```
