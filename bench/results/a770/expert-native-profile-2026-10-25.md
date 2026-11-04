# A770 mapped expert-chain native diagnostic

A770 16GB / PCIe3.0; DPC++2025.3.2, driver32.0.101.9034, VTune2026.4. IQ2_S gate/up and Q2_0 down,N2560/F640,GU16/down4.240 real experts in eight pinned-host sets. Eager in-order submissions, continuous collection, no engine or command graphs.

All72 complete output/guard checks passed. Capture coverage is exactly2088 GU,2088 down and2088 fused SwiGLU/Q8 calls: for eachT1/4/6,24 parity calls plus48*(8 warm+3 batches*2 replays)=696 chains. Work-size grouping creates five rows each for GU/down; their counts must be added.

The first metrics export exited2 because Occupancy/GRF filters were not matched. Collection and timing export exited0; those original files remain unchanged. A separate trace-copy export with valid filters exited0 and passed all coverage checks.

| Stage | Standard group count | Instances | XVE active | XVE stalled | Occupancy | L3 read GB/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| GU | 10 | 680 | 2.0% | 91.3% | 85.8% | 10.545 |
| GU | 20 | 680 | 2.5% | 95.3% | 94.9% | 10.915 |
| GU | 30 | 680 | 2.6% | 96.1% | 95.4% | 10.974 |
| down | 10 | 680 | 2.1% | 88.1% | 74.8% | 10.538 |
| down | 20 | 680 | 2.4% | 90.4% | 86.4% | 11.003 |
| down | 30 | 680 | 2.3% | 93.5% | 87.0% | 11.053 |

GU/down report SIMD16,128 GRF and zero spill bytes. The two additional work sizes each contain24 parity calls. The fused SwiGLU/Q8 kernel uses SIMD32 and has2088 instances. All metric rows, including parity groups, are retained in the JSON.

High stalled time and high occupancy in this mapped-weight workload support investigating weight access and cache reuse. These counters alone do not identify the sole cause of stalls. L3 and GPU-memory read rates are the profiler-reported metrics; neither is a direct PCIe bandwidth measurement. This is not a whole-model hotspot ranking, stage share or throughput improvement. Final acceptance still requires unprofiled original2K/128 model comparisons.

[Numeric record](expert-native-profile-2026-10-25.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_expert_native_profile_20261025.py
python bench/results/a770/check_expert_native_profile_20261025.py --local
```
