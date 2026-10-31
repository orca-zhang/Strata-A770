# A770 native GU/Q8 fusion with WG256 adjacent row pairs

A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034.

Following the completed WG512 fusion screen,keep SG16 and the unchanged integer dots,FP32 accumulation,Q8 arithmetic and down kernel. IQ2_S uses WG256 with each subgroup computing two adjacent rows sequentially. The original three-kernel baseline remains in the same EXE. This isolates smaller workgroup/row scheduling; it does not add shared-dot or approximate arithmetic.

Native resource inspection precedes the new compile and uses the exact frozen measured runtime options. The previous IQ2_S WG512 fusion reports128GRF,1KiB SLM,224spill bytes and256scratch bytes; Q2 fusion/baseline do not list spill. This static evidence does not establish performance cause. The candidate retains1KiB SLM,original4/2/1 entry chunks and original Q8 reduction order. Real240experts,eight physical sets,N2560/F640,downQ2_0 lanes4. Syntheticmixed maps every fourth physical expert pointer to host memory; this is not measured model routing/cache residency.

Before timing,331 quantizer blocks compare every byte against the original SG32 quantizer,including signed zeros,large finite values,NaN/Inf and guards.192 edge complete-chain outputs/Q8 arrays pass with group lengths1..7,including4/2/1 chunks and random/outlier inputs. Across15 timing arms,735 eager/empty checks and90 final graph outputs pass,including full Q8 arrays,inactive sentinels,nonzero starts,scattered destinations and scratch/output guards. Each T=1..6 uses a48-chain graph,16warmups and11 retained batches of4replays;990 timing batches are retained. Timing does not read events. Both independent paired rounds must gain>=1% with<=3% baseline time drift per format/T/memory.

| GU | Memory | T | Gain1 | Gain2 | Drift1 | Drift2 | Pass both |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| 22 | mapped | 1 | -0.522% | +0.222% | +3.878% | -4.144% | False |
| 22 | mapped | 2 | -1.066% | -0.494% | +2.480% | -2.989% | False |
| 22 | mapped | 3 | -1.735% | -1.274% | +2.097% | -2.689% | False |
| 22 | mapped | 4 | -0.616% | -0.914% | +2.655% | -3.142% | False |
| 22 | mapped | 5 | -0.827% | +0.725% | +1.290% | -2.074% | False |
| 22 | mapped | 6 | -0.817% | +0.919% | +1.788% | -2.357% | False |
| 22 | mixed | 1 | -0.073% | +2.749% | +2.726% | -3.041% | False |
| 22 | mixed | 2 | +0.371% | +1.969% | +1.882% | -2.512% | False |
| 22 | mixed | 3 | -3.571% | -2.291% | +1.428% | -2.442% | False |
| 22 | mixed | 4 | -1.036% | -0.054% | -1.137% | -1.631% | False |
| 22 | mixed | 5 | -0.397% | -0.298% | -0.788% | -2.869% | False |
| 22 | mixed | 6 | -1.645% | -0.222% | -0.267% | -2.711% | False |
| 22 | vram | 1 | -6.535% | -4.410% | -2.725% | -3.755% | False |
| 22 | vram | 2 | -5.891% | -5.206% | -1.894% | -2.233% | False |
| 22 | vram | 3 | -15.224% | -12.945% | -1.752% | -2.053% | False |
| 22 | vram | 4 | -8.035% | -6.498% | -2.644% | -2.447% | False |
| 22 | vram | 5 | -11.620% | -8.498% | -0.402% | -4.062% | False |
| 22 | vram | 6 | -14.792% | -10.342% | -0.161% | -2.437% | False |

No shape passes both rounds. Maximum adjacent baseline drift is 4.144%; pairs exceeding3% are excluded from performance conclusions. The remaining pairs also do not establish a repeatable1% gain. No adoption or model timing follows this candidate.

Only eligible syntheticmixed shapes can proceed to separate actual-model dispatch,full-output and original-binary speed confirmation. The corrected GDN layout requires a separately registered model reference; historical frozen-output hashes are not a correctness reference for that layout. This result is an operator screen; no model speed,production adoption or stable40tok/s claim.

Runner completed numerical,dispatch,all-sample/source/binary audits under its exclusive GPU lock and exited0. Publication reads small results only.

[All samples](gu-q8-rowpair-2026-10-26.json). Offline check:

```powershell
python bench/results/a770/check_gu_q8_rowpair_20261026.py --self-check
```
