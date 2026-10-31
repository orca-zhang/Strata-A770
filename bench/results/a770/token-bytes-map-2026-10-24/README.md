# Native-iteration token byte conversion — 2026-10-24

Decision: **adopted** under the user's positive-operator rule. Replace the Python character/bytearray loop in a token byte-cache miss with `bytes(map(UNICODE_TO_BYTE.__getitem__, token))`. Keep invalid-ID validation and rethrow a missing alphabet character as the previous `KeyError` type/message. Empty tokens, zero-valued bytes and independent per-tokenizer caches remain supported. No dependency, flag, numerical precision or additional cache is introduced. The earlier adopted cache initialization is present in both arms.

## Method and results

Xeon E5-2696 v4, Windows 10, 64-bit CPython 3.11.9, logical CPU 40. The real pack has 248,320 tokens and 247,587 merges. Screen and confirm each use independent-process A/B/A/B/A, five warmups, seven timed batches, with GC disabled by `timeit`. Correctness checks run before acquiring the shared GPU/compiler lock; timing runs after acquiring it. The lock does not control unrelated CPU work.

The changed operator case clears the existing cache and converts the first 4,096 vocabulary tokens whose characters belong to the byte alphabet, 32 calls per batch. Tokenizer construction is outside timing. Warm controls concatenate cached bytes or decode a mixed-text 16,384-token sequence (64 calls/batch), incrementally decode its 4,096-token prefix (64 calls/batch), or decode its 64-token prefix (4096 calls/batch). Inputs are prepared outside timing. They include English/code/CJK/combining marks/emoji/Hangul. All output and input token hashes agree across processes.

Gain is `100 * (sqrt(A_before * A_after) / B - 1)` and baseline drift is `100 * (A_after / A_before - 1)`. Exclude absolute drift above 3%. The rule recorded before measurement requires positive eligible cold-conversion gain in both phases, with at least one eligible cold pair per phase. Unchanged warm paths are controls, not a requirement that every control improve. This follows the user's operator threshold of gain above zero; the general performance threshold remains 1%. No repeat is sampled to seek a pass.

| Cold conversion comparison | Baseline drift | Throughput gain | Used for decision |
|---|---:|---:|---|
| Screen B1 | +0.30% | +44.02% | yes |
| Screen B3 | -3.09% | +36.06% | no |
| Confirm B1 | -5.07% | +28.20% | no |
| Confirm B3 | +2.88% | +36.57% | yes |

There is only **one eligible cold comparison per phase**; preserve this coverage limit. Both are positive, so source retention passes the predeclared positive-operator rule. Maximum drift across all cases is 8.06%, without an isolated cause. Sparse observations support retention under that rule, not a claim of a stable gain of this size.

Warm controls: cached-byte concatenation observes +2.35%/+3.66%, and 16K decode +3.62%/+4.69%, each from one eligible pair per phase. Stream4K has no eligible screen pair and -1.03% pooled gain across two confirm pairs; decode64 has none in screen and +0.88% across two confirm pairs. These paths use warm caches and do not execute the changed miss branch. Their variation is reported without assigning a cause or claiming a warm-path speedup.

These are CPU token conversion/decoding measurements, without HTTP or model generation. No model prefill, decode, TTFT or expert cache capacity is measured. Do not multiply this gain by the earlier cache-initialization result or call it a model speedup.

## Correctness and source

The candidate passes 496,640 cold/hot byte comparisons over all 248,320 real vocabulary entries, eight invalid-ID checks, synthetic empty-byte/invalid-alphabet checks, 7,576 incremental output/pending-state comparisons and 192 decode-policy comparisons (`replace`, `ignore`, `strict`). Ten existing regressions pass without skips. The portable regression additionally checks the first invalid alphabet character inside `a中b` on repeated failures, preserving the error message without caching a partial conversion.

Live source exactly matches frozen candidate `baf22f4806974a8fff5a54cef6526d4d05f06e81344f3f081dc9effec59fcad8`; baseline is `2cbdd2006e0705eccd65cc2b101df123d93b327dbd5e5fd80ff5bdd55daa98d4`. Two live portable checks pass after adoption. Production EXE/config hashes are unchanged, and the independent BPE rank-table candidate was rejected separately.

After applying source, passing live checks, saving the archive and passing offline audit, an extra report print loop referenced `cases` instead of `phases` and raised `KeyError`. The executed finalizer is retained in `tooling/finalize.py`; its print-only correction is `tooling/finalize.corrected.py`. `packaging-status.json` records this and the original measurement hashes. No correctness or performance samples were rerun or replaced.

## Audit and reproduction

```powershell
python bench/results/a770/token-bytes-map-2026-10-24/check_results.py
python -m unittest serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached -v
```

Offline audit uses only the standard library, without a GPU, compiler, tokenizer pack or private build directory. It verifies every archived hash, the exact source diff, all 50 medians/350 batches/20 comparisons, output hashes, error/state check counts and adoption. Binary dependencies and pack contents are omitted with hashes recorded. The public archive contains only source and measurement records.

Executed command:

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-a770-token-bytes-map-20261024/run.py
```

For independent reproduction, copy `baseline/`, `candidate/`, `frozen/`, `run.py` and `preparation.json` to a fresh build directory directly under an isolated repository root containing the baseline tokenizer. Supply the matching native scanner and pack assets, verify their recorded hashes and use the shared-lock wrapper. The runner refuses to overwrite results and expects the baseline live source at completion; do not revert the user's working source to rerun it. Production EXE/config are never replaced.
