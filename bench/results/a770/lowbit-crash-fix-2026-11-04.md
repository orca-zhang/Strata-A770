# A770 U2 crash repair — 2026-11-04

Measured on Intel Arc A770 16 GB; driver 32.0.101.9034; Xeon E5-2696 v4; Windows; oneAPI 2025.3. This follows the [failed U2 model experiment](lowbit-model-2026-11-04.md). The repaired executable is isolated; the production executable and configuration were not replaced.

The access violation came from queue ownership during exception cleanup. A default-constructed `Verifier` set both `cs_` and `copy_` to the shared default queue. Its destructor destroyed `cs_`, then waited through the dangling `copy_`. The original private dump unwinds through `urQueueFinish`, SYCL and executable RVA `0x10d13b`, the return from that second wait (`this+0x4f8`). The triggering C++ exception was `nonfinite quantization input`.

The header now initializes both stream pointers to null; `init()` creates the owned streams. The destructor also clears aliases to the borrowed default queue, protecting callers built with the former inlined initializers. A small CTest target, `verify_cleanup_test`, checks normal, nested and exception cleanup and submits work on the default queue afterward.

| Check | Original | Repaired |
|---|---|---|
| Same cleanup regression, frozen legacy header | `0xC0000409` fast-fail (the original model failed with `0xC0000005`) | Exit 0; default queue usable after all 3 checks |
| Current checkout Verifier/header/test, Windows L0 events enabled | — | Direct compile and link; exit 0, all 3 checks pass |
| Injected U2 nonfinite error after real expert work | Historical request crashed during unwinding | `ERR prefill U2 Down: nonfinite quantization input`, exit 1; no watchdog or crash dump |
| Real quality requests, two independent processes | Healthy old replay retained | 4 long requests completed; raw scores exactly match each other and the healthy old replay |
| Real generation, two independent processes | Original arithmetic retained | 4 long requests completed, 128 generated tokens each; full outputs and speculative acceptance match |

Each quality process runs Short 23→16, Code 2176→1 and Prose 2176→1, scoring the same 128 fixed targets per long request. Each generation process runs Short 23→16, Code 2048→128 and Prose 2048→128, with prompt reuse disabled. U2 Down coverage is checked on every long request. Test-only nonfinite injection is compiled behind `STRATA_U2_ENABLE_FAULT_INJECTION` and enabled only for the rejection request.

| Process | U2 calls | Routed rows | Long targets | Exit |
|---|---:|---:|---:|---:|
| quality-r1 | 4240 | 1136186 | 256 | 0 |
| quality-r2 | 4240 | 1136186 | 256 | 0 |
| generation-r1 | 4251 | 1139170 | 0 | 0 |
| generation-r2 | 4251 | 1139170 | 0 | 0 |

| Fixed-target diagnostic | PPL change |
|---|---:|
| Code2K | +0.5210% |
| Prose2K | +0.4032% |
| Aggregate | +0.4621% |

The 256 distinct targets pass the existing limited diagnostic screen (aggregate PPL increase ≤1%, each case ≤3%). This does not establish broad coding or knowledge quality.

| Generation request | Run 1 prefill ms | Run 2 prefill ms |
|---|---:|---:|
| Code2K-0 | 23526.40 | 23709.70 |
| Prose2K-0 | 22513.40 | 22523.00 |

These are unbracketed stability timings, not a U2 speedup claim. GEMM, U2 quantization and decode arithmetic were retained. The earlier FP32 detour was stopped after a real 2K request took about 200 seconds and was not adopted.

The upstream FP16 numerical instability remains unresolved: nonfinite activations are rejected, and independent historical tests also found wrong finite outputs. This repair prevents the secondary access violation; it does not make those activations valid. Further optimization or deployment needs a separate numerical-stability decision.

The U2 kernel check also passed 931,840 exact CPU comparisons and 3,002 checks, including NaN/±Inf rejection and recovery. The first cleanup-test link failed because replacing the original main removed its implicit oneMKL BLAS import; the resumed link explicitly names the already-installed import library. The first baseline assertion expected the model’s access-violation code and stopped on the standalone test’s different fast-fail code; that observation is retained. The current-checkout test needed the existing engine libraries and Windows `advapi32.lib` registry import for ggml. Failed attempts, original sources and measurements are retained. All compilation and GPU execution used the shared experiment lock.

Evidence: [summary JSON](../../../sycl/orca/lowbit-crash-fix-a770-windows-2026-11-04.json), text evidence and source archive (`lowbit-crash-fix-a770-windows-2026-11-04.json.gz`, raw capture retained locally). Memory dumps remain private; only their hashes and selected diagnostic text are included. Verify with `python -X utf8 build-lowbit-model-crashfix-20261104/publish.py --check`.
