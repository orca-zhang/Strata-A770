# Orca L0 U4 row-threshold screen — 2026-11-04

Arc A770 16 GB; Windows; driver32.0.101.9034; oneAPI2025.3. Default deployment unchanged.

Frozen model-r3 executable and the same 3,234 U4 experts. Order: min_rows128 /32 /1 /128. Quality uses 256 fixed targets and the original native scores, with both U4-128 controls required to match their previous scores exactly.

| Minimum rows | Code PPL vs native | Prose PPL vs native | Aggregate |
|---|---:|---:|---:|
| u4r32 | +0.6770% | +0.3449% | +0.5108% |
| u4r1 | +1.4373% | +0.4675% | +0.9513% |

Performance: Short16 initialization; three interleaved Code/Prose2048→128 cycles, discard first; median of two retained requests. Controls use their geometric mean. Positive speed changes mean higher throughput relative to U4-128, not native.

| Candidate | Case | Prefill change | Request change | Prefill / request control drift |
|---|---|---:|---:|---:|
| u4r32 | Code2K | +1.65% | +1.01% | +1.22% / +1.07% |
| u4r32 | Prose2K | +1.77% | +2.66% | +0.52% / +0.49% |
| u4r1 | Code2K | +0.36% | -0.69% | +1.22% / +1.07% |
| u4r1 | Prose2K | +0.58% | +1.17% | +0.52% / +0.49% |

| Mode | Case | U4 expert calls | U4 routed rows | Explicit weight DMA GiB |
|---|---|---:|---:|---:|
| u4n16 | Code2K | 2007 | 527743 | 5.168 |
| u4n16 | Prose2K | 2001 | 567682 | 5.152 |
| u4r32 | Code2K | 2781 | 590409 | 7.161 |
| u4r32 | Prose2K | 2752 | 630177 | 7.086 |
| u4r1 | Code2K | 3135 | 595855 | 8.072 |
| u4r1 | Prose2K | 3070 | 633920 | 7.905 |

These are small diagnostic cohorts. Native pair tiles, expert batching, MTP4/read256, cache5500(actual5771), reserve1536 and warm graphs stay fixed. GPU work holds the shared lock. Lower thresholds keep the same host payload but copy more expert weights. Decode math is unchanged; output and speculative acceptance can differ.

Evidence: [JSON](../../../sycl/orca/u4-threshold-a770-windows-2026-11-04.json), text archive (`u4-threshold-a770-windows-2026-11-04.json.gz`, raw capture retained locally). Recompute: `python -X utf8 build-orca-u4-tune-20261104/publish.py --check`. No weights, input tensors or dumps are archived.
