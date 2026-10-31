# A770 host-copy plus unchanged expert chain

A77016GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2. IQ2_S gate/up,Q2_0 down,N2560/F640,GU16/down4.240 real experts in eight pinned-host sets. Same executable, fresh A1/B1/A2/B2/A3. Both arms allocate the same45,312,000-byte staging slab plus guards.

Direct runs the existing complete GU/SwiGLU-Q8/down chain on mapped host pointers. Stage invokes the existing frozen fetch_blobs copy before EVERY chain, then runs the same native kernels on VRAM. Copy cost is INCLUDED. Source pointers remain in a separate immutable table; fixed destination pointers refer to staging. Every graph replay therefore reads pinned host weights again. The source-pointer table is copied back and compared after timing to prevent a false VRAM-to-VRAM replay speedup.

All360 complete output/guard checks pass against a fresh A1 capture, including nonzero starts, scattered outputs and unused output rows. Every staged eager call also compares all active copied weight bytes with source and checks slab guards/unused capacity.15 final graph-output checks and source-table checks pass. For eachT1/4/6, graph48chains rotates eight sets,16warm replays and11retained batches of4replays. All165timing batches remain in the data; initialization/readbacks are excluded from timing.

| Candidate | T | Paired throughput change | Baseline time drift |
| --- | ---: | ---: | ---: |
| B1 | 1 | -3.663% | -0.067% |
| B1 | 4 | -4.296% | -0.100% |
| B1 | 6 | -4.279% | -0.017% |
| B2 | 1 | -3.813% | +0.061% |
| B2 | 4 | -4.481% | -0.000% |
| B2 | 6 | -4.371% | -0.109% |

All6pairs require>=3% gain and<=3% baseline drift. Screen passed: **False**. This candidate consistently regresses and is not integrated into the model. There is no new whole-model throughput, quality or40tok/s acceptance. Production executable/configuration is unchanged.

The current all-mirror GPU routing path bypasses the older host-handshake staging branch, so changing its old PCIe mode alone would not test this path. This operator test copies an all-host group; it does not measure mixed-residency routing, asynchronous overlap, partial copies or all staging strategies.

[Data](expert-stage-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_expert_stage_20261025.py
```

Optional --local audits private frozen artifacts; run it under the shared GPU/compiler lock on the measurement machine.
