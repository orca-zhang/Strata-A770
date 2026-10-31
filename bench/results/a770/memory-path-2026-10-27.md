# A770 A/B/C memory-path trial

The [execution checklist](../../../docs/A770_MEMORY_PATH.md) confirms pinned host USM for mapped expert misses. This trial compares the same frozen IQ expert chain on host USM (A), device USM (B), and H2D memcpy into two device staging buffers (C). C tests 1/5/10/30 experts per chunk. No production change or model-speed acceptance has been made.

The first IQ2_S forward process passed all 352 eager full-output/Q8/guard checks and all retained pipeline checks, but its timing audit failed. The internal `ext_oneapi_get_last_event()` supplied a constant apparent Down duration around 223 seconds while complete calls took milliseconds. Copy/Down overlap also violated the event-duration bound. The complete failed protocol, stdout, partial result and traceback remain in `build-sycl-a770-memory-path-20261027/`; that cohort is not accepted or continued by replacing samples.

V2 uses a new directory, `build-sycl-a770-memory-path-20261027-v2/`, and a new protocol:

- The 11 retained timing batches use wall time and direct memcpy events, without marker kernels. Explicit queue barriers release a staging slot only after its previous complete computation.
- A separate warmed diagnostic batch uses explicitly returned single-task marker events before and after each unchanged full expert chain. It reports a compute **queue span**, which includes submission and scheduling gaps. Its intersection with copy intervals does not establish simultaneous active kernel execution.
- Each event duration, sum of serial copy/compute spans and interval overlap must fit inside its own batch wall time. Pure Down/all-kernel time remains unavailable because the frozen API does not return these events.
- Logical compressed weight bytes remain separate from actual hardware traffic. Host-read bandwidth, stalled, occupancy and model tokens/s remain unavailable in this operator trial.
- Both fresh forward/reverse processes must pass complete numerical checks and the 1% gain/3% adjacent-baseline drift screen for each reported shape. Absolute relative throughput changes below 0.3% are noise, with all samples retained.

The fixtures pack selected real expert blobs contiguously. C therefore measures contiguous transfer chunks; a winning result would still need a real-route test including scattered expert sources, staging-memory accounting and original Code/Prose 2K/full128 model confirmation.

```powershell
python build-sycl-a770-memory-path-20261027-v2/run.py --self-check
python build-sycl-a770-memory-path-20261027-v2/run.py
python build-sycl-a770-memory-path-20261027-v2/run.py --check
```

V2 completed all six fresh processes for IQ2_S (22), IQ2_XXS (16) and Q2_0 (42) GU, with Q2_0 Down. No C chunk/shape passed the independent two-process gate. V2's post-pipeline numerical readback happens after the eager marker diagnostic, so it does not separately validate the unmarked timed outputs before those are overwritten. It is not used as proof that every timed batch produced correct outputs.

## Command-graph confirmation

The new `build-sycl-a770-memory-path-20261027-graph/` cohort captures both queues and all eight weight sets into one graph. Each graph replay copies again from immutable pinned-host sources; all source/destination tables are read back and checked. Each method warms 16 graph replays and retains 11 timed replays. **Immediately after every timed batch, before any diagnostic or subsequent computation can overwrite outputs**, all eight full output sets, final Q8 bytes, scratch/staging guards and pointer tables are checked.

The graph's individual memcpy/kernel timestamps are unavailable, so all timed fields except wall time and logical bytes are null. The separate eager marker diagnostic is retained with its own wall-time envelope; its interval overlap cannot establish graph overlap.

Both cohorts completed six processes and their extra `--check` audits exited 0. In total they retain 5808 timing batches and 4224 eager full-chain checks. The graph cohort additionally checks 23232 complete timed output sets, with final Q8 and guards after every batch. All three GU formats use the unchanged frozen IQ object, N2560/F640, GU16/Down4 and fixed Q8 inputs. Production remains unchanged; there is no new whole-model speed or stable35 acceptance.

For single-token T1, the graph's device-resident B is 12.07–14.55 times faster than its adjacent mapped A controls; all six T1 B comparisons pass the drift gate. This is a small compressed working set resident in VRAM, not an achievable whole-model speedup with all expert weights resident. It confirms substantial sensitivity to the weight-access path without identifying a unique hardware stall cause.

Graph C30 throughput changes below are **forward / reverse**, relative to the adjacent A geometric mean. All values and all other chunk sizes are in the [numeric record](memory-path-2026-10-27.json).

| GU format | T1 | T2 | T4 | T6 |
| --- | ---: | ---: | ---: | ---: |
| IQ2_S | −2.653% / −3.271% | −2.494% / −4.723% | −3.796% / +7.711%* | −3.654% / −4.892% |
| IQ2_XXS | −3.073%* / −2.938% | −2.860%* / −6.133%* | −5.766% / −6.470% | −6.716% / −6.328% |
| Q2_0 | −3.681% / −5.022% | −4.087% / −6.007% | −5.101% / −5.952% | −5.756% / −5.676% |

`*` means adjacent baseline time drift exceeds 3%; that comparison cannot pass. In particular, IQ2_S T4's +7.711% has −4.396% baseline drift and the other independent process reports −3.796%. The eager Q2_0 T4 C30 +2.094% also fails to repeat (reverse −1.600%). Eager IQ2_S T6 C30 +0.223% is inside the noise interval and also has excessive baseline drift. These samples remain in the record.

Neither eager nor graph C1/C5/C10/C30 passes the repeated 1% gate for any tested format/shape. The tested two-buffer strategy is not adopted. The results support prioritizing hot expert residency and reducing miss bytes; they do not prove that every possible transfer or repack strategy is exhausted. Cache reduction has not been tested by this cohort.

Run the portable record check without a GPU:

```powershell
python bench/results/a770/check_memory_path_20261027.py
```

On the measurement machine, the graph's frozen-source audit is `python build-sycl-a770-memory-path-20261027-graph/run.py --check`; it uses the shared lock. Original failed V1 and all V2/graph raw logs, references and protocols remain in their respective build directories.
