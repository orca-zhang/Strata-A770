# Tokenizer byte mapping experiment — 2026-10-24

Decision: **not adopted**. The runtime tokenizer remains byte-for-byte equal to the baseline. Keep the small UTF-8 regression check in `serve/test_detok.py`.

The candidate replaces one Python generator expression with standard library operations:

```python
# baseline
mapped = "".join(BYTE_TO_UNICODE[b] for b in piece.encode("utf-8"))
# candidate
mapped = piece.encode("utf-8").decode("latin1").translate(BYTE_TO_UNICODE)
```

## Method and limits

Windows, Xeon E5-2696 v4, 64-bit CPython 3.11; every child is pinned to logical CPU 40. The real pack has 248,320 vocabulary entries and 247,587 merges. Each phase runs A/B/A/B/A in independent processes, with three warmups and seven timed batches per case. Calls per batch: 100 for short text, three for 8K characters, one for 32K characters. `timeit` disables GC during timing. Inputs and template are identical across arms; frozen serving modules and the existing native text scanner are reused.

There are ten cases: short, prose8k, code8k, unicode8k and prose32k, each measured through `Tokenizer.encode` and `Service.encode_prompt`. These lengths are characters, not token counts. The serving measurement uses `MockEngine` and a real chat template; it measures CPU prompt preparation without HTTP or generation. Expert cache capacity, GPU operator throughput, prefill, decode and TTFT are not measured.

Each experiment contains screen and confirm phases: ten processes, 100 medians, 700 batch values and 40 adjacent comparisons. Pair throughput gain is `100 * (sqrt(A_before * A_after) / B - 1)`; baseline drift is `100 * (A_after / A_before - 1)`. Only pairs with absolute drift at most 3% qualify. The shared middle baseline makes adjacent comparisons dependent; pooled results are descriptive, not confidence intervals.

Adoption requires a positive eligible median and at least 75% positive eligible pairs in every candidate round, and no case with pooled eligible regression worse than 2%. The one complete repeat additionally requires at least 30/40 eligible pairs and two eligible pairs for every case. No minimum 3% speed gain is required. These rules were recorded before the corresponding measurements; `check_results.py` recomputes the decision rather than trusting the runner's completion message.

## Results

| Experiment | Eligible pairs | Positive eligible pairs | Cases with at least two eligible pairs |
|---|---:|---:|---:|
| Initial | 15/40 | 15/15 | 5/10 |
| One complete repeat | 14/40 | 13/14 | 4/10 |

The repeat fails coverage. It also has a -14.689% eligible short `Tokenizer.encode` comparison; the two eligible short comparisons pool to **-5.581%**. Confirm round 1 has two positive eligible pairs out of three, below 75%. Sparse positive observations do not establish reproducible performance. All negative values and excluded comparisons remain in the raw JSON and derived pairs. No third repeat was run and no model speedup is claimed.

Initial and repeat each pass 10,000 random Unicode/no-merge comparisons, four surrogate-error checks, all 256 byte mappings and 1,600 real-vocabulary token ID comparisons. Token hashes agree across all 20 timing processes. The frozen candidate passes 23 existing/additional tests with no failures or skips, including literal think/control tokens and image markers. That suite uses live serving test modules, separately from the frozen timing modules. The retained UTF-8 regression also passes on the unchanged live tokenizer; its log and source are under `regression/`.

The initial ten timing processes all completed, but the reporting code raised `StatisticsError: no median for empty data` for a confirm round with zero eligible pairs. The saved result contains all ten runs and only the screen pairs. `initial/results.json` and `initial/interrupted-results.json` are preserved unchanged; `initial/recovered-pairs.json` recovers all 40 comparisons from the original process JSON, without replacing samples. `run.initial.py` is the executed script, while `run.py` preserves the subsequent reporting fix.

The single repeat uses identical candidate code, inputs, affinity and batch counts. It waits for the repository's shared GPU lock to avoid overlap with engine/compiler work that uses that lock. Other CPU jobs are not governed by that lock. Parallel work was observed around the initial run, but the cause of drift was not isolated; the earlier preparation note mentioning driver JIT is a hypothesis, not an established cause. The repeat still exhibits drift and is rejected by the declared rules.

## Audit and reproduction

Offline audit needs only the Python standard library, without a GPU, compiler, model or private build directory:

```powershell
python bench/results/a770/tokenizer-translate-2026-10-24/check_results.py
python -m unittest serve.test_detok.HeapBpe.test_byte_mapping_is_utf8 -v
```

`archive.json` hashes every archived file; `preparation.json` records tokenizer assets, frozen dependencies and production hashes. Binary dependencies and tokenizer assets are omitted; the native extension's hash is recorded. Production EXE/config hashes were rechecked and remain equal to the preparation records.

The original local commands were `python build-sycl-a770-tokenizer-translate-20261024/run.py` and, for the one repeat:

```powershell
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-a770-tokenizer-translate-repeat-20261024/run.py
python build-sycl-a770-tokenizer-translate-20261024/check_candidate.py
```

The runners refuse to overwrite `results.json`. For a future independent reproduction, create a new directory immediately under the repository root, copy `sources/baseline`, `sources/candidate`, `sources/frozen`, `repeat/run.py` and `repeat/preparation.json` into it, and supply the matching native scanner under `frozen/serve/`. Verify the scanner/tokenizer asset hashes against preparation first; then run the new `run.py` through the shared-lock wrapper above. Its parent directory must be the repository root because the archived runner resolves helper tools there. Do not reuse the completed measurement directories or change the adoption rule after seeing results.
