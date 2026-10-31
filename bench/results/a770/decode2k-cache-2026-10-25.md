# A770 2K decode: cache-only confirmation

A770 16GB / PCIe 3.0, Windows, driver 32.0.101.9034, DPC++ 2025.3.2. Two fresh A1/B1/A2 cohorts use the same frozen complete graph engine, original 2048-token Code/Prose inputs and full 128-token greedy outputs. Only the requested expert count (--expert-cache) changes from 5500 to 6000; actual slots increase from 5771 to 6297. The reserve remains 1536 MiB, with FP16 KV, context/prefill 8192, CLI spec4/min-p 0.5, MTP attention 1024, full 248320-row draft/verifier vocabulary and prompt-cache 0.

Each fresh process runs Short16 initialization, one warmup per case and three interleaved retained cycles. All 48 complete requests and 6144 output tokens match the frozen references. Warmups and every low observation are retained. The expert cache adapts during these repeated requests; prompt-cache 0 does not reset expert residency.

| Formal cohort | Case | B median tok/s | B retained range | Paired throughput | Adjacent baseline time drift |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | Code2K | 30.890 | 30.780–30.931 | +5.170% | -0.547% |
| 1 | Prose2K | 30.528 | 30.050–30.761 | +3.805% | +0.011% |
| 2 | Code2K | 30.997 | 30.858–31.002 | +5.467% | -0.190% |
| 2 | Prose2K | 30.591 | 30.146–30.796 | +3.923% | +0.081% |

Both cases in both formal cohorts must gain at least 3%, with absolute adjacent baseline time drift at most 3%. Confirmation passed: **True**. Stable 40 requires every retained B request in both cohorts to reach 40 tok/s. Target achieved: **False**. This compares the cache setting on one frozen build; it does not independently establish a gain over the older production build.

A predeclared separate read-only Sysman process samples once per second, with at least one successful sample during every decode. Observed GPU frequency range across all 48 requests: **2400–2400 MHz**; throttle flags observed: **False**. Monitoring is identical across arms; its overhead is not separately quantified, and gaps between samples may hide short changes. These counters do not prove a paging cause or measure PCIe traffic.

The engine reports **1535–1537 MiB free VRAM** at startup for the two B processes. This is a startup observation, not a peak or per-request memory measurement; the configured reserve remains 1536 MiB.

The earlier 24-request initial screen had a source-registration issue: the waiting process had loaded an older script before three offline/provenance checks were added on disk. Reconstruction confirms identical CLI construction and measurement/output/logging code, but does not cryptographically capture its executed source. Its numerical observations remain preserved, and it is excluded from these two formal acceptance cohorts. Original records were not overwritten.

The compact data includes all 48 request timings, output digests, accepted/offered counts, ranges, paired results and per-request telemetry coverage. Output equality is limited to these prompts; no general quality/PPL conclusion is claimed. Production executable and configuration remain unchanged.

[Data](decode2k-cache-2026-10-25.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_decode2k_cache_20261025.py
python bench/results/a770/check_decode2k_cache_20261025.py --local
```
