# A770 next-block weight prefetch

A770 16GB / PCIe 3.0, Windows, driver 32.0.101.9034, DPC++ 2025.3.2. Profile-guided operator screen of the complete mapped-host IQ2_S gate/up, SwiGLU/Q8 and Q2_0 down chain, N2560/F640, GU16/down4. The preceding native profile showed high stalled time without register spills.

Only IQ2_S paired gate/up adds installed oneAPI native prefetch calls: lane 0/8 each requests the next row-local 82-byte block for gate and up, one iteration ahead. Bounds exclude the end of the row. Arithmetic, rounding, input layout and precision remain unchanged. Extraction and ocloc disassembly under the frozen runtime confirm **12 null-destination UGM load messages**, versus zero in the baseline. This verifies that the hint generated hardware instructions; it does not establish their effectiveness.

| T | B1 paired throughput | B2 paired throughput | Maximum absolute baseline time drift |
| --- | ---: | ---: | ---: |
| 1 | +0.038% | +0.117% | 0.255% |
| 4 | -0.213% | -0.105% | 0.095% |
| 6 | +0.170% | +0.181% | 0.094% |

No pair reached the predeclared 3% benefit threshold; the change is rejected. The small signed changes do not establish a useful improvement or a material regression.

Five fresh processes ran A1/B1/A2/B2/A3 against 240 real experts in eight physical mapped-host sets. Each process checked 72 complete chains and guards against the immutable reference, plus graph replay output. All 360 checks passed; all 165 measured batches remain in the compact evidence. Timing includes gate/up, activation/quantization and down. The frozen baseline executable and worker object were reused. No collector ran during timing.

This is an operator result. There is no new whole-model speed result, no general quality claim, and no production/default change. [All samples and hashes](weight-prefetch-2026-10-25.json). Check without a GPU:

```powershell
python bench/results/a770/check_weight_prefetch_20261025.py
python bench/results/a770/check_weight_prefetch_20261025.py --local
```
