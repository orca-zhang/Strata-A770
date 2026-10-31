# A770 original 2K: normalized recent prefill routing for cache seeding

Later on 2026-10-26, the user explicitly requested adoption despite the failed performance screen below. Production now enables `STRATA_PREFILL_CACHE_TAIL64=1`. Only the prefill object was rebuilt against the current production headers and 32 MiB workspace; the already-adopted V4 storage, eager execution, auto-cache, 1536 MiB reserve and every other linked object remain in place. The earlier screen data and its decision at measurement time remain unchanged.

Production integration was checked with the same new executable, tail256/off versus normalized tail64/on, with cache capacity fixed at 6297 slots to isolate the feature. Six complete requests (Short16, Code2K/128, Prose2K/128 per mode) produced identical outputs across modes; an earlier completed Short16 is also retained and matches both, totaling seven requests and 560 checked tokens. Short uses the production window reader and emits no batched-tail histogram. Both 2K cases record normalized mass122880, with different histogram digests between modes. Production auto-cache startup was checked separately: old6921slots/1407MiB free, new6916slots/1450MiB free, both exit0. Auto-cache varies with available memory; these observations establish no memory savings or throughput change. The fixed-slot integration runs start with2255/2249MiB free.

This is user-requested adoption with functional checks, without independent production performance confirmation. The earlier graph-engine rates are not production rates. The initial configuration-change, linker-log, memory-gate and window-reader validation attempts are retained and described in the [adoption data](prefill-cache-tail-adoption-2026-10-26.json). Recompute the adoption checks with `python bench/results/a770/check_prefill_cache_tail_20261026.py --adoption` (add `--local` for raw records, build inputs and active configuration). Set the flag to0 to restore the256-row policy without rebuilding.

A77016GB / Xeon E5-2696 v4 / PCIe3.0 x16, Windows, driver32.0.101.9034, DPC++2025.3.2, Qwen3.8-Flash-Next IQ2_XS. Same isolated binary: A1 last256 prefill rows / B1 last64 rows / A2 last256 rows. The candidate weights the recent rows by min(T,256)/min(T,64), preserving histogram mass. Short prompts of64 rows or fewer retain their original histogram. For these single-chunk2047-position prefills, each case records mass122880 in both arms. Only the age distribution changes. The existing factor0.125,LFU score,decay0.5,maximum4 preparation batches and32 swaps/batch remain the same.

Only the matched workspace-prefill object is rebuilt; class layouts and headers retain the frozen versions. The same existing generator logs actual preparation batches/uploads for both arms. Both arms also perform the same small CPU histogram digest accounting before decode. There are no new GPU buffers, precision changes or kernels.13 linked libraries are checked unchanged before/after measurement. Graph replay,FP16KV,full248320 vocabulary,requestedcache6000/actual6297slots,reserve1536MiB,startupfree>=1500MiB,verify6/MTP4/lookup3,minp0.5,context/prefill8192,MTPwindow1024,prompt-cache0 remain fixed.

These absolute speeds belong to the isolated frozen graph engine, which differs from the production executable. Startup free VRAM is recorded for each process; it is neither peak headroom nor an allocation-saving measurement. The cause of its increase relative to older builds has not been established. Seed uploads count expert replacements; they do not measure PCIe bytes or prove identical cache contents.

Fresh A1/B1/A2 processes wait30s each, initialize with Short16, then run one warmup/case and three retained interleaved cycles. All24 original2048-input requests complete128 outputs and match the historical references (3072tokens). Every warmup, low value and failure is retained; no refills. Compiler/GPU work uses the shared exclusive lock, with profiling disabled and identical read-only1Hz Sysman monitoring. Each decode has a successful frequency sample. The screen requires at least1% paired median gain in both cases and absolute adjacent baseline time drift at most3%. A positive screen needs independent fresh confirmation and prompt-plus-decode cost review. These observations do not establish general model quality.

| Run | Case | Median tok/s | All retained tok/s | Actual seed batches | Actual uploads |
| --- | --- | ---: | --- | --- | --- |
| A1 | Code2K | 30.881 | 30.769, 30.933, 30.881 | 3, 3, 3 | 96, 96, 96 |
| A1 | Prose2K | 30.138 | 30.648, 30.108, 30.138 | 4, 4, 4 | 111, 117, 117 |
| B1 | Code2K | 31.030 | 31.051, 31.030, 31.019 | 3, 3, 3 | 96, 96, 96 |
| B1 | Prose2K | 30.703 | 30.754, 30.703, 30.657 | 3, 3, 3 | 96, 96, 96 |
| A2 | Code2K | 30.957 | 30.843, 30.996, 30.957 | 3, 3, 3 | 96, 96, 96 |
| A2 | Prose2K | 30.579 | 30.761, 30.150, 30.579 | 4, 4, 4 | 111, 117, 117 |

| Case | Paired decode throughput | Baseline time drift | Prompt+decode throughput change |
| --- | ---: | ---: | ---: |
| Code2K | +0.357% | -0.244% | +0.320% |
| Prose2K | +1.137% | -1.441% | +0.488% |

All raw histogram digests, normalized masses, actual seed counters, prompt/decode/TTFT observations, accepted/offered counts and all24 frequency coverage records are retained in the compact data. Frequency samples do not establish PCIe traffic or the cause of a low value. No low observation is substituted or attributed to an unmeasured cause. The predeclared screen does not pass; the candidate was not adopted at the time of this screen. Its later user-requested production adoption is recorded above; stable40tok/s has not been established.

[Data](prefill-cache-tail-2026-10-26.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_prefill_cache_tail_20261026.py
python bench/results/a770/check_prefill_cache_tail_20261026.py --local
```
