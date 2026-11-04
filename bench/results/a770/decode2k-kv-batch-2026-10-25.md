# A770 original 2K: FP16 KV and indexer batch integration

A770 16 GB / Xeon E5-2696 v4 / PCIe 3.0 x16, Windows, driver 32.0.101.9034, DPC++ 2025.3.2, Qwen3.8-Flash-Next IQ2_XS. This screen integrates the existing FP16 KV-step and indexer-step batch calls into the newer frozen engine that completes original 2K prefill. It reuses the previously checked store implementation and arithmetic. Only the verifier call sites and the FP16 store object are overridden; class layouts and other engine/kernel objects retain the frozen versions.

One rebuilt executable runs A1 scalar / B1 batch / A2 scalar in fresh processes, with command graphs, full248320 vocabulary, FP16 KV, fixed5771 actual expert slots, reserve1536MiB, spec4/min_p0.5, context/prefill8192, MTP attention1024 and prompt-cache0. Each process has Short16 initialization, one warmup per case and three interleaved retained cycles. GPU/compiler work uses the shared exclusive lock; no profiler runs. All warmups, low samples and the first preparation failure are retained. Both cases require at least1% paired median throughput gain and at most3% absolute adjacent baseline time drift; a positive screen still needs independent confirmation.

| Run | Case | Median tok/s | All retained samples tok/s |
| --- | --- | ---: | --- |
| A1 | Code2K | 29.374 | 29.245, 29.374, 29.472 |
| A1 | Prose2K | 29.329 | 29.329, 29.595, 16.609 |
| B1 | Code2K | 29.365 | 29.295, 29.365, 29.480 |
| B1 | Prose2K | 29.444 | 29.559, 29.444, 29.375 |
| A2 | Code2K | 29.383 | 29.326, 29.383, 29.412 |
| A2 | Prose2K | 29.399 | 29.503, 28.952, 29.399 |

| Case | Paired throughput change | Adjacent baseline time drift |
| --- | ---: | ---: |
| Code2K | -0.045% | -0.030% |
| Prose2K | +0.274% | -0.238% |

A1's first Code warmup was 12.028tok/s, and its last retained Prose request was 16.609tok/s. Both complete outputs match the reference and remain in the data. The cause of these low values was not isolated; no request was repeated or substituted.

All24 requests completed128 outputs, and all3072 output tokens equal the frozen references. The predeclared screen does not pass; the integration is not adopted. No production executable, configuration or defaults were changed, and40 tok/s has not been established. These are full-model decode measurements on this fixed workload; the earlier isolated KV/indexer operator speedups are not multiplied into them.

A separate upstream check on2026-10-25 found [adaptive MTP depth #27210](https://github.com/ggml-org/llama.cpp/pull/27210) still open and unmerged. Its author recommends a maximum depth of at least7, presents dual-R9700 dense-model tests, and cautions about weaker MoE/older-hardware results. This is not A770 MoE evidence or the source of the batching change. Local longer-window regressions remain relevant; the proposal is not transplanted in this experiment.

[Flash-attention graph safety #30916](https://github.com/ggml-org/llama.cpp/pull/30916) was closed without merge. It excludes oneDNN/MKL attention calls from graph recording. Strata's native QSA decode does not use the modified call path, so this patch is not transplanted either.

[All numerical samples and output hashes](decode2k-kv-batch-2026-10-25.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_decode2k_kv_batch_20261025.py
python bench/results/a770/check_decode2k_kv_batch_20261025.py --local
```
