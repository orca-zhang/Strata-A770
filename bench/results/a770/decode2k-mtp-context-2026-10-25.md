# A770 original2K draft-attention context screen

Same frozen graph engine and runtime, full248320 draft/verifier vocabulary, FP16 KV,5771 actual expert slots, reserve1536MiB, context/prefill8192,spec4/min_p0.5,prompt-cache0. Only --mtp-window changes: A1=1024,B1=4096,A2=1024. The4096 window covers the complete original2048 prompt plus all128 generated tokens.

Each fresh process uses Short16 initialization, one warmup per case, then three interleaved retained cycles. All24 requests completed128 outputs, all3072 tokens equal the frozen references. Every warmup and low sample is retained. GPU/compiler lock held, no collector in the model measurements. A completed native trace was independently exported on the CPU during A1 warmup; the export finished before retained samples.

| Run | Case | Median tok/s | Retained range | Accepted | Offered |
| --- | --- | ---: | ---: | --- | --- |
| A1 | Code2K | 28.636 | 28.472 to 29.353 | [75, 75, 75] | [98, 98, 98] |
| A1 | Prose2K | 29.370 | 27.821 to 29.385 | [73, 73, 73] | [98, 98, 98] |
| B1 | Code2K | 29.222 | 29.216 to 29.326 | [75, 75, 75] | [97, 97, 97] |
| B1 | Prose2K | 29.188 | 28.531 to 29.317 | [73, 73, 73] | [99, 101, 99] |
| A2 | Code2K | 29.214 | 28.211 to 29.287 | [75, 75, 75] | [98, 98, 98] |
| A2 | Prose2K | 29.327 | 28.616 to 29.430 | [73, 73, 73] | [98, 98, 98] |

| Case | Paired throughput change | Adjacent baseline time drift |
| --- | ---: | ---: |
| Code2K | +1.034% | -1.978% |
| Prose2K | -0.547% | +0.147% |

The screen did not pass: both cases require at least3% paired gain and at most3% absolute adjacent baseline time drift. No adoption, independent speed confirmation or stable40 attainment. The candidate did not increase retained accepted-token counts. Keep1024; do not interpret accepted/offered ratios alone as throughput.

[All numeric samples](decode2k-mtp-context-2026-10-25.json). Recompute without a GPU:

```powershell
python bench/results/a770/check_decode2k_mtp_context_20261025.py
python bench/results/a770/check_decode2k_mtp_context_20261025.py --local
```
