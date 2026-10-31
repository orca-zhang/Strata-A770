# A770 MTP draft acceptance above 80% (2026-10-27)

Intel Arc A770 16 GB, Windows, oneAPI 2025.3, IQ2_XS, FP16 KV.

The installed configuration now uses `--spec-min-p 0.8` (previously `0.5`). The installed executable and weights are unchanged. Production auto-cache validation measures **620/674 = 91.99%** pure MTP acceptance across ten fixed workloads. Every tested production workload exceeds 80%. Suffix lookup counters are subtracted from protocol totals.

Existing per-token confidence gate, not improved draft model weights. Higher acceptance comes with fewer offered and fewer accepted draft tokens.

Screening disables lookup and fixes `--expert-cache 5500` (5771 actual slots, 7922 MiB), context/prefill 8192, prompt cache zero, full 248320-row draft vocabulary, MTP attention window 1024. The normal lookup policy is restored for fresh-process A/B/A confirmation (0.5/0.8/0.5); each arm has Short16 initialization, Short/Code2K/Prose2K warmups and two retained cycles of nine cases. The final production check restores auto cache and includes the 8K case. GPU/compiler lock held; no profiler. Every warmup and low sample is retained.

| Workload | Pure-MTP baseline 0.5 | Pure-MTP candidate 0.8 | Production 0.8 |
| --- | ---: | ---: | ---: |
| Short | 48/67 (71.64%) | 42/45 (93.33%) | 42/45 (93.33%) |
| Code2K | 74/93 (79.57%) | 62/64 (96.88%) | 62/64 (96.88%) |
| Prose2K | 70/104 (67.31%) | 59/65 (90.77%) | 52/56 (92.86%) |
| Long8K | 73/92 (79.35%) | 58/62 (93.55%) | 54/58 (93.10%) |
| zh_cache | 77/106 (72.64%) | 62/69 (89.86%) | 59/66 (89.39%) |
| zh_db | 75/104 (72.12%) | 68/73 (93.15%) | 68/73 (93.15%) |
| en_explain | 73/104 (70.19%) | 58/64 (90.62%) | 58/64 (90.62%) |
| en_plan | 78/102 (76.47%) | 70/79 (88.61%) | 70/79 (88.61%) |
| python | 86/102 (84.31%) | 80/88 (90.91%) | 80/88 (90.91%) |
| sql | 88/98 (89.80%) | 76/82 (92.68%) | 75/81 (92.59%) |

| Workload | Baseline tok/s A1/A2 | Candidate tok/s | Paired change | Baseline time drift |
| --- | ---: | ---: | ---: | ---: |
| Short | 20.815 / 20.783 | 21.158 | +1.72% | +0.15% |
| Code2K | 23.302 / 23.292 | 22.582 | -3.07% | +0.04% |
| Prose2K | 23.403 / 23.337 | 23.344 | -0.11% | +0.28% |
| zh_cache | 20.277 / 20.288 | 20.015 | -1.32% | -0.05% |
| zh_db | 23.079 / 23.097 | 24.381 | +5.60% | -0.07% |
| en_explain | 21.732 / 21.760 | 21.716 | -0.14% | -0.13% |
| en_plan | 24.895 / 24.870 | 24.949 | +0.27% | +0.10% |
| python | 22.314 / 22.324 | 22.195 | -0.55% | -0.04% |
| sql | 23.874 / 23.910 | 22.778 | -4.66% | -0.15% |

The paired decode changes range from -4.66% to +5.60%. This is an acceptance-target configuration; it does not establish a general speedup. The single-request threshold screen includes cache/priming drift and is not a performance confirmation.

All 121 measured requests (14688 output tokens; initializers excluded) match their corresponding greedy references token for token. Ten fixed greedy workloads, 23 to 8000 input tokens, at most 128 output tokens. No guarantee for arbitrary prompts or sampling.

Only the local `strata-iq2_xs.json` confidence value is changed. Its previous bytes are saved in `build-draft-acceptance-20261027/production-p80/config-before.json`. Other platforms retain their defaults.

[Numeric samples and hashes](draft-acceptance-2026-10-27.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_draft_acceptance_20261027.py
```
