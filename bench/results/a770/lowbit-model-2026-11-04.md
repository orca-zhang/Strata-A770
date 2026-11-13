# A770 real-model low-bit check — 2026-11-04

Measured on Intel Arc A770 16 GB, driver32.0.101.9034; Xeon E5-2696 v4; Windows; oneAPI2025.3. This is a matched legacy SYCL integration, not the currently deployed Orca engine. No production executable or configuration changed.

Two fixed prompts (Code2K and Prose2K), 2,048 input tokens and 128 generated tokens. Each process runs Short16, then one discarded long-prompt cycle and two retained interleaved cycles. Each displayed latency is the median of two retained requests. Native controls bracket U4 N16 and U4 N8 in separate processes; comparisons use the geometric mean of the two native medians. All samples, including warmups and failures, are retained.

Settings: expert-cache5500 (actual5771 slots), reserve1536 MiB, read1024, decay0.5, MTP4/min-p0.5, prompt-cache0, expert batch0 and min_rows128. U4 has one weight plane and two residual S4 activation planes. U2 replaces only raw-Q2 Down; Gate/Up and decode remain original. No inner profiling, timing/sync flags, telemetry or frequency lock.

| Case | Candidate | Native prefill ms | Candidate prefill ms | Prefill tok/s | Throughput change | Native request wall ms | Candidate request wall ms | Request throughput change |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Code2K | u4n16 | 21210.58 | 20215.30 | 101.31 | +4.92% | 25854.41 | 24851.18 | +4.04% |
| Prose2K | u4n16 | 20534.38 | 19456.55 | 105.26 | +5.54% | 25412.03 | 24658.23 | +3.06% |
| Code2K | u4n8 | 21210.58 | 20438.50 | 100.20 | +3.78% | 25854.41 | 25050.10 | +3.21% |
| Prose2K | u4n8 | 20534.38 | 19621.70 | 104.37 | +4.65% | 25412.03 | 24826.61 | +2.36% |

End-to-end request latency is host wall time from GEN to DONE, excluding process startup; this is the engine protocol, not an HTTP server benchmark. The engine prefill+decode sum and TTFT are also archived. Positive percentages mean higher throughput, not percentage reduction in latency.

| N8 relative to U4 N16 | Prefill throughput change | Request throughput change |
|---|---:|---:|
| Code2K | -1.09% | -0.79% |
| Prose2K | -0.84% | -0.68% |

Both U4 modes also generate identical full outputs: True. These direct ratios are from the same small four-arm cohort, without a separate reverse-order confirmation.

| Case | Candidate | Native decode tok/s | Candidate decode tok/s | Native TTFT ms | Candidate TTFT ms | Prefill control duration drift | Request control duration drift |
|---|---|---:|---:|---:|---:|---:|---:|
| Code2K | u4n16 | 27.58 | 27.62 | 21260.77 | 20268.00 | +0.29% | +0.29% |
| Prose2K | u4n16 | 26.25 | 24.61 | 20583.49 | 19505.46 | +0.93% | +0.76% |
| Code2K | u4n8 | 27.58 | 27.77 | 21260.77 | 20490.33 | +0.29% | +0.29% |
| Prose2K | u4n8 | 26.25 | 24.60 | 20583.49 | 19670.90 | +0.93% | +0.76% |

Decode arithmetic is unchanged. Different prefill states can change generated tokens, expert routing and speculative acceptance, so these decode rates are full-request observations, not a GEMV speedup measurement. Outputs are stable within each arm and the two native controls match.

Prefill and request control duration drifts are within 3%. This small cohort is not a confidence interval or an independent confirmation.

| Fixed-target check | Code2K PPL change | Prose2K PPL change | Aggregate PPL change |
|---|---:|---:|---:|
| u4n16 | +0.6251% | +0.5891% | +0.6071% |
| u4n8 | +0.6251% | +0.5891% | +0.6071% |

The two native score dictionaries match exactly. Both U4 modes have identical scores and 97.65625% native top-1 agreement in each case. The 256 frozen targets pass the limited diagnostic screen (aggregate PPL increase ≤1%, each case ≤3%); this is not broad coding or knowledge quality acceptance. Scored runs use prompt-cache1/read256 only to invoke the existing teacher-forced split; their timings are excluded above.

U2 Down failed with 0xC0000005 on the second long quality request. Code2K alone completed 2,121 calls / 545,846 routed rows and its partial PPL changed +0.5210%; the incomplete model gate does not pass. A private minidump records ur_loader.dll+0x21f8f (queue-wait path); root cause remains unresolved. No U2 model speedup is claimed. The independent two-real-expert Q2 test passed 931,840 exact CPU output comparisons and guards at M1/4/17/160. This did not establish multi-request model stability.

U4 uses 3,234 prepacked original-BF16 experts, 8,941,363,200 host payload bytes and two eight-slot device banks (44,236,800 weight bytes, plus activation workspaces). An expert is 2,764,800 bytes versus native IQ2/Q2 1,510,400 bytes (+83.05%). Per-request activation packing, staging and expert computation are included. Offline asset preparation is excluded; first-long-request asset loading/JIT is present in the archived warmup, separate from READY startup. The asset manifest and all 6,468 weight files were verified before each cohort.

The first attempt failed before READY because the current Orca configuration omitted the legacy SYCL relaxed-allocation option. That failure is retained. The successful cohorts restore the engine’s established ONEAPI_DEVICE_SELECTOR, UR_L0_ENABLE_RELAXED_ALLOCATION_LIMITS and correctly-rounded-divide compile option.

Evidence: [summary JSON](../../../sycl/orca/lowbit-model-a770-windows-2026-11-04.json), text-only raw evidence archive (`lowbit-model-a770-windows-2026-11-04.json.gz`, raw capture retained locally). The archive includes frozen sources, commands, outputs, score TSVs, protocols, coverage and asset hashes; process-memory dumps remain private. Local verification: `python -X utf8 build-lowbit-model-20261104/publish.py --check`.
