# A770 existing expert group-row concurrency screen

A77016GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2.

Only the existing grid_groups parameter changes: baseline0 uses cap_groups=n*K, candidates4/8 stride over the same groups. The same frozen IQ object, arithmetic, reduction order and precision remain in all arms. IQ2_S GU/Q2_0 down,N2560/F640,GU16/down4; mapped-host and VRAM weights are tested separately.

Nine fresh arms A1/G4-1/A2/G8-1/A3/G4-2/A4/G8-2/A5, two memory placements each:18 processes. T1..6,240 real experts,eight physical sets. Synthetic grouping uses ten singleton groups atT1 and5*T groups with two entries otherwise; this is not a measured model routing distribution. Cap_entries64,3 leading inactive entries,scattered outputs and guards.

Every eager full output in both memory placements must equal the first mapped baseline bitwise. All882 eager/empty-group checks and108 final graph outputs passed. Per shape:graph48chains rotates eight sets,16warm,11 retained batches of4replays. All1188 timing batches remain in the data. Each candidate/T/memory must pass BOTH adjacent-baseline rounds at>=1% gain and<=3% absolute time drift; no retries or dropped samples.

| Group rows | Memory | Paired gain range, allT and both rounds | Maximum absolute drift |
| ---: | --- | ---: | ---: |
| 4 | mapped | -0.168207% to +0.893215% | 0.601513% |
| 4 | vram | -7.452495% to -1.575713% | 0.563098% |
| 8 | mapped | -0.936605% to +0.263194% | 0.298824% |
| 8 | vram | -11.608897% to +2.228951% | 0.666235% |

Only **VRAM,8group rows,T1/2** passes both rounds. No mapped-host shape passes. Other shapes retain their previous setting.

| Eligible T | First round gain | Second round gain |
| ---: | ---: | ---: |
| 1 | +1.882976% | +2.228951% |
| 2 | +1.765900% | +1.839263% |

This is an operator result, not full-model throughput. A separate matched-ABI,originalBF16/6297-slot Code2K/Prose2K full128-output screen is required. The model prototype only limits the eligible VRAM-hit call; mapped PCIe calls,other formats/shapes and routing collection keep their previous behavior. No production change or40tok/s claim.

The original runner completed the raw/source hash audit under its exclusive GPU lock. This publication independently recomputes the small numeric record without repeating large artifact reads during another experiment.

[Data](group-concurrency-2026-10-26.json). Offline verification:

```powershell
python bench/results/a770/check_group_concurrency_20261026.py --self-check
```
