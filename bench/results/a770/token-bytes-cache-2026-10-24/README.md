# Token byte-cache experiment — 2026-10-24

Decision: **adopted under the user's updated operator retention rule**: general performance gain threshold 1%, retain positive operator gains. The live tokenizer exactly matches the tested candidate. The original coverage gates remain failed and are preserved; this is an explicit policy update after measurement, not a new measurement or a claim of stable model acceleration. See `adoption-update.json` and `summary.before-adoption.json`. No third phase or new experiment was sampled to seek a pass.

Move the existing per-tokenizer byte-cache initialization to `Tokenizer.__init__`, then access that attribute directly in `token_bytes()`. This removes an empty dictionary allocation and `__dict__.setdefault()` call for every decoded token. The conversion, ID validation, exception paths, BPE and UTF-8 decoding remain the same. No dependency, flag, extra cache or cache limit is added; the constructor now allocates one empty dictionary rather than waiting for the first decoded token.

## Measurement method

Windows 10, Xeon E5-2696 v4, 64-bit CPython 3.11.9, logical CPU 40. Every child runs on that CPU. The real IQ2_XS pack supplies 248,320 vocabulary entries and 247,587 merges. These are CPU conversion/decoding measurements; GPU operators, model prefill/decode, TTFT, HTTP and expert cache capacity are not measured. The frozen serving implementation uses the existing incremental UTF-8 decoder.

Screen and confirmation each use independent-process A/B/A/B/A: ten processes in total. Every case has five warmups and seven timed batches; `timeit` disables GC during timing. Counts, seed, source/dependency/production hashes and the adoption rule are recorded in `preparation.json` before measurement. Performance measurements wait for the shared GPU/compiler lock; correctness checks run before acquiring the lock. The lock does not control unrelated CPU tasks.

| Case | Operation | Calls per timed batch |
|---|---|---:|
| warm_bytes16k | Concatenate cached bytes for 16,384 token IDs | 64 |
| cold_bytes4k | Clear the cache, then convert 4,096 token IDs | 32 |
| decode16k | Decode 16,384 token IDs | 64 |
| stream4k | New `Detokenizer`, 4,096 pushes, join emitted text | 64 |
| decode64 | Decode 64 token IDs | 4096 |

The 16K input is encoded once outside timing from repeated English/code/CJK/combining-mark/emoji/Hangul text; stream and short cases take its prefix. The cold case selects the first 4,096 vocabulary tokens whose characters belong to the byte alphabet. It includes cache clearing, but excludes tokenizer construction and model generation. All inputs and output hashes must agree across arms and processes. The full-vocabulary differential check verifies the byte boundaries independently of concatenated output hashes.

For each candidate round, compare the candidate median against the geometric mean of its adjacent baseline medians. Gain is `100 * (sqrt(A_before * A_after) / B - 1)`; baseline drift is `100 * (A_after / A_before - 1)`. Exclude absolute drift above 3%. Require at least 8/10 eligible pairs per phase and at least one eligible pair for each of five cases in each phase. Every candidate round must have a positive eligible median and at least 75% positive eligible pairs. No case's pooled eligible geometric gain may be below -2%. A positive gain below 3% may qualify. There is one experiment with screen and confirmation, without repeated-until-pass sampling. Adjacent pairs share their middle baseline, so the summaries are descriptive, not confidence intervals.

## Results

Screen has 4/10 eligible pairs covering three cases; confirmation has 6/10 covering four cases. Both fail the required 8/10 and coverage of all five cases. All ten eligible pairs are positive; the ten drift-excluded observations are also preserved. Maximum absolute baseline drift is 13.068%, and its cause was not isolated.

| Case | Eligible pairs across both phases | Pooled eligible geometric gain |
|---|---:|---:|
| warm_bytes16k | 1/4 | +41.28% |
| cold_bytes4k | 3/4 | +10.41% |
| decode16k | 2/4 | +38.29% |
| stream4k | 1/4 | +15.31% |
| decode64 | 3/4 | +43.21% |

These sparse eligible observations fail the original coverage rule. After reviewing that outcome, the user explicitly accepted positive operator gains; all five cases have positive eligible pooled gains and all ten eligible pairs are positive, so the source is retained under that update. In particular, the warm-byte and stream cases still each have only one eligible comparison. Do not present the pooled gains as confirmed stable speedups, extrapolate them to model generation or multiply them together. `summary.json` retains every original gate, eligible observation and per-round result; `results.json` retains all included and excluded comparisons.

The baseline hash is `cccf8d5e59c1d585e30ddf8b4d05b968e9b76145261016e4a124d56dba845150`. The adopted runtime source matches the tested candidate `2cbdd2006e0705eccd65cc2b101df123d93b327dbd5e5fd80ff5bdd55daa98d4`. Production EXE/config hashes match their preparation records. Two portable live tests pass after applying the exact candidate; their previous baseline results are preserved in `live-tests.before-adoption.log`.

## Correctness and audit

The frozen candidate passes 496,640 cold/hot byte comparisons over all 248,320 real vocabulary entries (no vocabulary error tokens), eight invalid-ID checks, synthetic empty-byte/invalid-alphabet checks, 7,576 incremental output/pending-state comparisons and 192 decode-policy comparisons (`replace`, `ignore`, `strict`). The existing nine detokenizer/BPE regressions also pass without skips. The new portable regression verifies empty bytes, two independent tokenizer vocabularies, cached results and invalid IDs; its snapshot is under `regression/`.

```powershell
python bench/results/a770/token-bytes-cache-2026-10-24/check_results.py
python -m unittest serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached -v
```

The offline audit needs only the standard library, without a GPU, compiler, tokenizer pack or private build directory. It verifies archived hashes, the exact two-line source change, all 50 medians/350 batches/20 pairs, output hashes and the recorded adoption decision. `archive.json` covers source, measurements and tooling; binary dependencies and pack contents are omitted and their hashes are retained. Frozen test sources correspond to the nine-test suite; the separately added regression is checked after the adoption decision against live code.

The executed local command is:

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-a770-token-bytes-cache-20261024/run.py
```

For an independent reproduction, copy `baseline/`, `candidate/`, `frozen/`, `run.py` and `preparation.json` to a new build directory immediately under the repository root. Supply the matching scanner binary under `frozen/serve/` and tokenizer assets at the recorded path; verify their hashes first. Run the new `run.py` through the shared-lock wrapper. The runner refuses to overwrite existing results. Its final live-source check expects the baseline tokenizer, so reproduction after adoption must use an isolated checkout containing the baseline; do not replace the live optimized source just to rerun the benchmark. Production EXE/config are never replaced.
