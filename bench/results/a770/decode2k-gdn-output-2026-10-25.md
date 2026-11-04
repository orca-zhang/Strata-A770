# A770 original 2K: GDN verification-output integration

A770 16 GB / Xeon E5-2696 v4 / PCIe 3.0 x16, Windows, driver 32.0.101.9034, DPC++ 2025.3.2, Qwen3.8-Flash-Next IQ2_XS. This screen transplants the already operator-confirmed GDN output kernel into the frozen engine that completes original 2K prefill. The two changed regions request 256 GRF, use local fences and skip unnecessary replay-prefix output work. State-update arithmetic and reduction order are preserved. The frozen model's replay/commit kernels, headers, class layouts and other objects remain unchanged. Both the original and candidate objects/executables are rebuilt with identical flags and the same 13 unchanged libraries.

A1 original / B1 candidate / A2 original use fresh processes, command graphs, full248320 vocabulary, FP16 KV, fixed6297 actual expert slots (request6000), reserve1536MiB, CLI spec4/min_p0.5, context/prefill8192, MTP attention1024 and prompt-cache0. Each process has Short16 initialization, one warmup per case and three interleaved retained cycles. Compiler/GPU work holds the shared exclusive lock, with profiling disabled. Both cases must gain at least1% paired median throughput, with absolute adjacent baseline time drift at most3%; a positive screen requires independent confirmation. All warmups and low samples remain in the data.

| Run | Case | Median tok/s | All retained samples tok/s |
| --- | --- | ---: | --- |
| A1 | Code2K | 30.769 | 30.645, 30.784, 30.769 |
| A1 | Prose2K | 30.361 | 30.491, 17.660, 30.361 |
| B1 | Code2K | 30.753 | 30.599, 30.788, 30.753 |
| B1 | Prose2K | 30.454 | 30.554, 29.842, 30.454 |
| A2 | Code2K | 30.919 | 30.679, 30.919, 30.937 |
| A2 | Prose2K | 30.504 | 30.694, 30.053, 30.504 |

| Case | Paired throughput change | Adjacent baseline time drift |
| --- | ---: | ---: |
| Code2K | -0.295% | -0.483% |
| Prose2K | +0.072% | -0.467% |

A1's second retained Prose request was17.660 tok/s. It completed the full reference output and remains in the data. Its cause was not isolated; no request was rerun or substituted.

All24 original2048-input requests completed128 outputs; all3072 output tokens match the frozen references. The screen fails its predeclared gate; this integration is not adopted. No production executable, configuration or defaults changed. Stable40 tok/s has not been established. Earlier GDN operator speedups do not establish model throughput gains, and this workload check does not establish general model quality.

The separate upstream check found [llama.cpp #30976](https://github.com/ggml-org/llama.cpp/pull/30976) open and unmerged. It vectorizes reordered-layout dequantization stores for several quantization types. Its author reports gains in primed prefill on Arc Pro B70 and decode changes no larger than0.1%; this is neither an A770/IQ2_S expert decode result nor the source of this GDN change.

[llama.cpp #26631](https://github.com/ggml-org/llama.cpp/pull/26631) merged on2026-10-24. It adds the -1 expert-index sentinel to skip explicitly masked MUL_MAT_ID slots, useful for partitioned heterogeneous experts. This does not authorize dropping routed top-k experts or establish a speedup on the current Strata expert layout; no such pruning is introduced here.

[Data](decode2k-gdn-output-2026-10-25.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_decode2k_gdn_output_20261025.py
python bench/results/a770/check_decode2k_gdn_output_20261025.py --local
```
