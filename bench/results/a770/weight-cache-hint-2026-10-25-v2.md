# A770 mapped IQ2_S/Q2_0 weight-cache hint screen - 2026-10-25

A770 16 GB, PCIe 3.0, Windows driver 32.0.101.9034, DPC++ 2025.3.2. The candidate only adds the installed native `annotated_ptr` L1 cached read hint to the existing page-safe 4/8-byte weight loads. Arithmetic, precision, weight bytes and expert layout stay identical.

**No confirmed gain; not adopted.** This is a mapped-host operator screen, with no VRAM, whole-model decode or model-quality claim. The current 2K decode target of 40 tok/s remains unmet.

Complete IQ2_S gate/up -> fused SwiGLU/Q8 -> Q2_0 down chain, N=2560/F=640, GU16/down4, 240 real experts in eight physical sets. Fresh A1/B1/A2/B2/A3 processes. Each T has 48 graph calls, 16 warm graphs and 11 retained batches of four graphs. All 165 batches are in the JSON. Each process passes 72 eager full-chain parity/guard checks and the graph output check; total 360 eager checks.

| T | A1 us | B1 us | A2 us | B2 us | A3 us | B1 paired gain | B2 paired gain | B1/B2 baseline drift |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1593.135 | 1590.483 | 1589.722 | 1592.736 | 1586.794 | +0.059% | -0.281% | -0.214% / -0.184% |
| 4 | 3163.843 | 3163.513 | 3175.724 | 3153.026 | 3154.579 | +0.198% | +0.384% | +0.376% / -0.666% |
| 6 | 4726.114 | 4719.016 | 4724.673 | 4723.166 | 4722.685 | +0.135% | +0.011% | -0.030% / -0.042% |

Positive gain means higher throughput against the geometric mean of adjacent A times. Every pair is within the predeclared 3% drift limit. None meets the predeclared >=3% gain threshold in both B runs. The observed -0.281% to +0.384% changes do not establish a speedup. No retries or low-sample removal.

Checks:

```powershell
python -X utf8 bench/results/a770/check_weight_cache_hint_20261025.py
python -X utf8 bench/results/a770/check_weight_cache_hint_20261025.py --local
```

The JSON retains all samples, executable/source/log hashes and original build commands. Original trial: `build-sycl-a770-weight-cache-hint-20261025`.

This corrects text encoding in the original hashed report; all measurements are unchanged. The commands above verify the original frozen JSON, report and retained artifacts.
