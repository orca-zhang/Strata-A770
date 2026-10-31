# BPE rank-table reference — 2026-10-24

Decision: **not adopted**. The candidate reads `self.ranks` once per merge pass into a local variable instead of once per adjacent pair. It changes two lines, with no merge-order or vocabulary change. The baseline already includes the adopted per-tokenizer byte-cache initialization.

A baseline-only `cProfile` run on repeated English/code text puts short-word `_bpe` at about 47% of instrumented total time (`profile.txt`); this identifies a candidate, not an uninstrumented speed estimate.

## Method and results

Xeon E5-2696 v4, Windows, 64-bit CPython 3.11.9, logical CPU 40. Screen and confirm each run independent-process A/B/A/B/A under the shared GPU/compiler lock. No profiler is enabled during timing. Each case has three warmups and seven timed batches. The real vocabulary has 248,320 entries and 247,587 merges.

Nine operator cases cover lengths 1/2/4/8/16/32/64, an unmergeable 64-character word and the unchanged 128-character heap fallback, with 256 calls per batch. Ten full-prompt cases measure `Tokenizer.encode` and frozen `Service.encode_prompt` for short/prose8k/code8k/unicode8k/prose32k inputs; lengths are characters. Their batches use 100/3/1 calls for short/8K/32K. Serving uses a real template with `MockEngine`, without HTTP or generation.

Each candidate median is compared with the geometric mean of the adjacent baselines. Gain is `100 * (sqrt(A_before * A_after) / B - 1)`; drift is `100 * (A_after / A_before - 1)`. Absolute drift above 3% is excluded. Under the user's positive-operator rule, the predeclared check requires positive pooled eligible gain for each scope in both phases, at least one eligible case in every candidate round, and no full-prompt case with pooled regression worse than 2%. No repeat is sampled to seek a pass. Pooled scope gains give cases equal weight and describe this workload only; adjacent pairs share baselines and are not independent confidence samples.

| Scope | Screen eligible / gain | Confirm eligible / gain |
|---|---:|---:|
| BPE operator | 7 / -2.09% | 5 / +2.32% |
| Tokenizer.encode | 6 / -4.73% | 0 / unavailable |
| Service.encode_prompt | 6 / -2.82% | 1 / +1.18% |

The sign is inconsistent and prompt preparation has regressions. The source is rejected even under the updated positive-gain rule. All 76 pairs, including excluded and negative observations, remain in `results.json`; per-case results are in `summary.json`. There is no model, TTFT, decode, prefill or expert-cache measurement, and no speedup claim.

Correctness passes 10,000 random Unicode/no-merge comparisons, all 256 byte mappings, four surrogate-error checks and 1,600 real-vocabulary token ID comparisons. Ten existing regression tests pass without skips, including the independent heap implementation as the short-word scan oracle. All 190 process medians/1,330 timed batches have equal output hashes across arms.

The first verification attempt imported the test module before selecting the external tokenizer path, so five real-pack tests were decorated as skipped. It failed the no-skips assertion before starting any performance process. `precheck-skipped/` preserves that script, log and differential result. The executed timing runner sets the tokenizer path before importing tests and uses the frozen serving modules; the full suite passes. No timing sample was replaced.

## Audit and reproduction

```powershell
python bench/results/a770/bpe-ranks-2026-10-24/check_results.py
```

The audit needs only the standard library, no GPU, model, pack or private build directory. It verifies the source diff, all hashes, samples, pairs, exclusions, correctness records and rejection. `final-checks.json` describes the unchanged live baseline at the close of this experiment; later byte-conversion changes are a separate experiment. Production EXE/config remain unchanged.

Executed command:

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-a770-bpe-ranks-20261024/run.py
```

To reproduce independently, copy `baseline/`, `candidate/`, `frozen/`, `run.py` and `preparation.json` to a fresh directory immediately under an isolated repository root containing the baseline tokenizer. Supply the matching native scanner and tokenizer assets and verify preparation hashes. Run the new runner through the shared-lock wrapper. It refuses to overwrite `results.json`. Binary dependencies and pack contents are omitted from the archive, with their hashes retained; executable/config are never replaced.
