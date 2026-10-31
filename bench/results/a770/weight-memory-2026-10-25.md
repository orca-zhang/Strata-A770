# A770 mapped expert weights: alignment, host allocation and L1 bypass

A770 16GB / PCIe 3.0, Windows, driver 32.0.101.9034, DPC++ 2025.3.2. These are operator screens prompted by the high-stall mapped-weight profile. No production executable, setting or kernel was changed by these experiments. No whole-model speedup is claimed.

The complete chain uses 240 real IQ2_S gate/up and Q2_0 down experts in eight physical sets, N=2560/F=640, GU16/SG16 and down4. Each timed experiment uses fresh A1/B1/A2/B2/A3 processes, T=1/4/6, 72 complete output/guard checks per process and graph-output checks against the frozen reference. Each T has 16 warm graph replays and 11 retained batches of four 48-chain replays. Timing includes the full gate/up → SwiGLU/Q8 → down chain. All six paired comparisons must gain at least 3% with at most 3% adjacent baseline time drift. Every batch is retained.

| Experiment | Six paired throughput changes | Maximum absolute baseline time drift | Screen passed |
| --- | ---: | ---: | --- |
| iq2s-align84 | -2.008% to -1.727% | 0.372% | False |
| host-wc | Not timed: requested hardware behavior unobserved | — | False |
| weight-uncached | -0.158% to +0.164% | 0.231% | False |
| host-readonly | -0.349% to +0.162% | 0.264% | False |

The alignment candidate pads each IQ2_S block from 82 to 84 bytes after its FP16 scale and uses aligned 32-bit code/sign reads. All 240 inputs pass a complete byte-exact inverse transformation before timing. Gate/up storage increases by 2.439%; total expert blob size increases from 1510400 to 1536000 bytes (1.695%). Arithmetic, precision, down bytes and reduction order are unchanged. Repacking and allocation are outside operator timing; a positive screen still needs a complete model test accounting for cache capacity and initialization. Other format/fallback paths are outside this candidate's supported scope.

The host-allocation candidate uses native Level Zero zeMemAllocHost BIAS_WRITE_COMBINED in the existing SYCL context, with the frozen IQ object and unchanged weights. A separate same-sized allocation probe requires VirtualQuery PAGE_WRITECOMBINE before any timing. Mapping observed: **False**. An accepted driver hint alone is insufficient; an unobserved mapping is not a measured regression.

The L1-bypass candidate changes only the two existing annotated 4/8-byte loader hints from cached to uncached. The IQ2_S lookup table retains its usual caching. Actual A770 GU disassembly must contain new .uc.ca load messages before its unique EOT, or timing is skipped. Candidate messages: **30**; baseline: **0**. This instruction evidence is limited to the inspected GU kernel.

The device-read-only host candidate separately uses zeMemAllocHost MEM_READ_ONLY in the same native context, then initializes the allocation before device reads. The SDK declares this flag mutually exclusive with WC, and the candidate does not combine them. Successful allocation establishes API acceptance; it does not prove a particular physical mapping or cache policy. Weight bytes, IQ object, layout and arithmetic are unchanged.

Across the timed screens, **1080** complete output/guard checks and **495** retained timing batches passed the audits. These samples do not establish general model quality or end-to-end gains. Any screen that passes needs an independent confirmation and original 2K/128 whole-model validation. The existing cache-only model gain cannot be added to these operator percentages.

[All samples and hashes](weight-memory-2026-10-25.json). Offline recomputation:

```powershell
python bench/results/a770/check_weight_memory_20261025.py
```

The optional --local flag checks retained local artifact hashes; run it under the shared experiment lock on this measurement machine because hashing the weight bundle can contend with timing.
