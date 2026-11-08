# Orca U4 N16 GU optimization — 2026-11-04

Arc A770 16 GB; Xeon E5-2696 v4; Windows; driver 32.0.101.9034; oneAPI 2025.3. Default deployment unchanged.

Warm 2048-input/128-output model throughput relative to current native brackets: l0: Code prefill +21.19%, Prose prefill +19.64%; direct: Code prefill +15.99%, Prose prefill +14.71%. These are measured combined gains from expanded coverage and the GU change.

Two independent candidates retain U4 N16 weights, group 64 and both residual S4 activation planes. Unmask removes redundant activation read masks after padded source IDs are mapped to valid token 0; output stores remain masked. Gather copies the already quantized routed activations once per batch, then reuses the existing contiguous block-load GU product. The gather experiment adds one kernel per batch and 25 MiB of activation workspace at max_rows 8192; it is retained as an experiment.

Both final expanded-coverage model cohorts select the same single-kernel unmask implementation. It needs no new kernel launch or activation allocation. Both experimental candidates on both transports matched all 3,302,400 GU/Down outputs byte for byte with the original SYCL reference, including tails, routed gaps, guards and alternating/shrinking banks. Weight formats remain unchanged. The model tests also reuse the existing additional 6,168 compatible experts, increasing coverage from 3,234 to 9,402 and host U4 payload from 8.33 to 24.21 GiB. Device weight banks remain 42.2 MiB. No weights are downloaded or reconverted.

Component screen: ABBA, 16 warmups and 9 timed samples per phase; each sample batches 8 GU calls and includes native host submission/wait cost. Every sample is followed by complete output/guard comparison. Positive percentages mean higher throughput. These are hot real-weight/synthetic-activation results, excluding weight transfers and the rest of the model.

| Candidate | Backend | Experts × rows | GU throughput change | Control duration drift | Valid positive screen |
|---|---|---:|---:|---:|---|
| gather | l0 | 2 × 128 | -42.06% | -0.43% | False |
| gather | l0 | 2 × 160 | -37.58% | -0.16% | False |
| gather | l0 | 2 × 248 | -26.58% | -0.89% | False |
| gather | l0 | 8 × 48 | -27.47% | -0.61% | False |
| gather | direct | 2 × 128 | +8.68% | +1.02% | True |
| gather | direct | 2 × 160 | +11.54% | +0.42% | True |
| gather | direct | 2 × 248 | +28.43% | +0.37% | True |
| gather | direct | 8 × 48 | +14.07% | -1.42% | True |
| unmask | l0 | 2 × 128 | +18.95% | +0.54% | True |
| unmask | l0 | 2 × 160 | +16.49% | +1.23% | True |
| unmask | l0 | 2 × 248 | +17.07% | +1.92% | True |
| unmask | l0 | 8 × 48 | +23.56% | -2.01% | True |
| unmask | direct | 2 × 128 | +20.75% | -0.09% | True |
| unmask | direct | 2 × 160 | +16.63% | +1.55% | True |
| unmask | direct | 2 × 248 | +15.81% | -0.21% | True |
| unmask | direct | 8 × 48 | +20.34% | -5.02% | False |

Gather is rejected on L0 because the added submission cost exceeds the benefit. Direct unmask at 8 × 48 fails the 3% drift rule and is not accepted as a gain. The final larger expert set includes more small batches; the unmask candidate has valid positive screens on both transports without an extra allocation. Gather is retained with its measured intermediate 3,234-expert Direct cohort.

Model quality uses 256 fixed targets. Expanded coverage at 32 and 64 rows is screened against the frozen native reference; prefer 32 if it passes, otherwise 64. This changes which experts run through U4, so its scores can differ from the previous 3,234-expert cohort. The diagnostic gate remains aggregate ≤1%, each case ≤3%. Performance then uses new native/candidate/native processes with the same candidate executable per backend, Short16 initialization, three interleaved 2048→128 Code/Prose cycles, first cycle discarded, median of the remaining two. Lazy U4 loading is included in the first Code request and excluded from these warm medians; the larger data set increases that first-load cost. Native controls use their geometric mean. Native control tokens must match the previous native outputs in full; expanded U4 outputs can differ and must remain identical within each arm.

| Backend | Candidate | Code PPL change | Prose PPL change | Aggregate PPL change |
|---|---|---:|---:|---:|
| l0 | unmask, min_rows 32 | +1.2173% | -0.9572% | +0.1242% |
| direct | unmask, min_rows 32 | +1.2173% | -0.9572% | +0.1242% |

| Backend | Case | Native prefill ms | Optimized U4 prefill ms | Optimized prefill tok/s | Prefill throughput vs native | Request throughput vs native | Prefill / request control drift |
|---|---|---:|---:|---:|---:|---:|---:|
| l0 | Code2K | 27526.44 | 22712.70 | 90.17 | +21.19% | +17.45% | -0.31% / -0.21% |
| l0 | Prose2K | 26637.00 | 22264.85 | 91.98 | +19.64% | +18.86% | +0.40% / +0.43% |
| direct | Code2K | 25098.28 | 21639.00 | 94.64 | +15.99% | +12.78% | +0.73% / +0.75% |
| direct | Prose2K | 24388.65 | 21261.05 | 96.33 | +14.71% | +14.68% | -0.05% / +0.11% |

| Backend | Case | U4 expert calls | Routed rows | Routed row coverage | Explicit weight DMA GiB |
|---|---|---:|---:|---:|---:|
| l0 | Code2K | 6828 | 861351 | 87.62% | 17.582 |
| l0 | Prose2K | 6305 | 871930 | 88.70% | 16.235 |
| direct | Code2K | 6828 | 861351 | 87.62% | 17.582 |
| direct | Prose2K | 6305 | 871930 | 88.70% | 16.235 |

First long Code request after Short16, one discarded sample including lazy U4 loading:

| Backend | Native first Code request ms | U4 first Code request ms |
|---|---:|---:|
| l0 | 32698.66 | 53093.78 |
| direct | 30108.85 | 59549.99 |

These new percentages are total gains from the GU optimization plus expanded U4 coverage against their current native brackets. They are not added to the previous U4 percentages; no same-cohort original-U4 model bracket was run. Pair tiles, expert batch, MTP 4/read 256, cache 5500 (actual 5771), reserve 1536 and warm graphs stay fixed. Request duration is host GEN→DONE, excluding startup, not HTTP. GPU work and compilation hold the shared experiment lock.

The actual CMake U4 image target also configures and builds on both transports with oneAPI 2025.3, compiling the native source directly so its adjacent U4 header resolves. The simplified common unmask source is checked separately on SYCL/L0/Direct against the complete original output references. Both generated Prefill Host translation units compile against the frozen checked engine headers/runtimes. This is not a new full-current-runtime deployment or long-context reliability claim. Old EXEs, assets and profiles are retained. New optional profiles are `strata-iq2_xs-u4n16-orca-l0-opt.json` and `strata-iq2_xs-u4n16-orca-direct-opt.json`; their measured EXEs/images and verified asset links are retained under `engine/`. Both relocated profiles pass Short16, the first Code2K token, U4 coverage and normal exit. The startup RAM budget is measured arena plus 24.21 GiB U4 payload plus 6 GiB headroom (a budget, not a measured minimum). Decode/GEMV arithmetic is unchanged.

Preparation checked a RAM budget of 63.23 GiB against available RAM on this PC, including the 24.21 GiB U4 payload. This is not a measured minimum RAM requirement. Optional profiles: `L0`, `Direct`.

The separate [threshold screen](orca-u4-threshold-2026-11-04.md) found only small further gains at 32 rows; 1 row was rejected. The final model cohorts use the expanded 9,402-expert set and their measured quality-gated threshold; component GU timings do not include this coverage change.

Evidence: [JSON](../../../sycl/orca/u4-optimization-a770-windows-2026-11-04.json), [text archive](../../../sycl/orca/u4-optimization-a770-windows-2026-11-04.json.gz). Recompute with `python -X utf8 build-orca-u4-adoption-20261104/publish.py --check`. Weights, input tensors and dumps are excluded.
