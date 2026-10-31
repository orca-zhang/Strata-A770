# A770 native read-only host allocation: CPU initialization correction

The earlier [memory screen](weight-memory-2026-10-25.md) used queue.memcpy to initialize a MEM_READ_ONLY allocation. That does not guarantee CPU-only writes. Its 360 checks and 165 batches remain numerical observations with this qualification; they do not establish a screen satisfying the device-read-only usage contract. Original files remain unchanged.

This fresh screen changes only that untimed weight initialization to CPU std::memcpy. A normalized source comparison verifies that this is the only worker change. The flag, native SYCL/Level Zero context, frozen IQ object, exact weights/layout/arithmetic and reference are unchanged. API acceptance still does not prove a physical caching policy.

A77016GB / PCIe3.0, Windows, driver32.0.101.9034, DPC++2025.3.2. Mapped240 real IQ2_S GU/Q2_0 down experts, eight physical sets,N2560/F640,GU16/SG16 and down4,T1/4/6. Five fresh A1/B1/A2/B2/A3 processes complete360 full output/guard checks, graph-output checks and retain all165 timing batches. Each T has16 warm graph48-chain replays and11 batches of4 replays. Timing covers the complete chain and excludes initialization. All six pairs need>=3% throughput gain and<=3% absolute adjacent baseline time drift.

| Pair | T | Throughput change | Baseline time drift |
| --- | ---: | ---: | ---: |
| B1 | 1 | -0.141% | -0.150% |
| B1 | 4 | -0.001% | -0.070% |
| B1 | 6 | +0.005% | +0.028% |
| B2 | 1 | -0.365% | -0.015% |
| B2 | 4 | -0.233% | -0.069% |
| B2 | 6 | +0.073% | -0.058% |

Screen passed: **False**. No production changes or whole-model gain claim. A positive screen still needs independent confirmation and original2K/128 model validation. All low values and the qualified predecessor remain preserved.

[Data](host-readonly-cpuinit-2026-10-25.json). Offline check:

```powershell
python bench/results/a770/check_host_readonly_cpuinit_20261025.py
```

The optional --local flag also verifies retained local artifacts and the single initialization-source change. Run it under the shared lock on this measurement machine to avoid hashing contention.
