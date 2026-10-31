# A770 pre-decode cache seed cap4/64

A77016GB / PCIe3.0, Windows, driver32.0.101.9034, DPC++2025.3.2. Same isolated frozen generator, matched headers and engine archives. Only increase the existing pre-decode LFU preparation cap4 to64 with a default-off flag, plus identical accounting in both arms. Reuse the existing prefill last256-row histogram, factor0.125,score,decay0.5,swap limit32 and all buffers. No representation, arithmetic, model-precision or GPU-memory-budget change. The generator source is checked to contain no other changes.

Same requestedcache6000/actual6297slots, reserve1536MiB,FP16KV,fullvocabulary,context8192,verify6/MTP4/lookup3,min-p0.5,MTPwindow1024,prompt-cache0. Fixed30s wait before each process, startup free>=1500MiB. Short16 then Code2K/Prose2K twice full128. All8 complete outputs (1024tokens) equal historical reference. Preparation stays in prompt time. These smoke timings are preserved and are not a speed acceptance cohort.

| Arm | Case | Seed cap | Executed batches | Uploaded experts | Tail entries |
| --- | --- | ---: | ---: | ---: | ---: |
| A | Code2K | 4 | 3 | 96 | 122880 |
| A | Prose2K | 4 | 3 | 96 | 122880 |
| A | Code2K | 4 | 3 | 96 | 122880 |
| A | Prose2K | 4 | 4 | 111 | 122880 |
| B | Code2K | 64 | 3 | 96 | 122880 |
| B | Prose2K | 64 | 3 | 96 | 122880 |
| B | Code2K | 64 | 3 | 96 | 122880 |
| B | Prose2K | 64 | 4 | 111 | 122880 |

Any candidate request executing more than4 preparation batches: **False**. This is the prerequisite for the prepared performance follow-up. An empty swap list terminates the existing loop early; a larger cap does not force additional uploads. The counters establish actual pre-decode uploads, not identical internal LFU state or all later cache behavior. A future speed cohort must separately account for prompt-plus-decode cost and independent confirmation. No performance cohort has been launched, no production executable/configuration changed, and40tok/s is not accepted.

[Data](prefill-cache-seed-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_prefill_cache_seed_20261025.py
```

Optional --local audits private frozen records; run under the shared GPU/compiler lock on the measurement machine.
