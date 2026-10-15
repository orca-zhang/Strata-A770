# ArcInt comparison notes

This page is for one question: should Strata add an OpenVINO backend because ArcInt's Intel Arc numbers look good?

Short answer: not yet. Measure the same workload first.

ArcInt is a deliberately narrow Intel Arc engine. Its OpenVINO path depends on a patched `marfrit-openvino`
runtime, and ArcInt owns the serving loop, scheduler, KV memory, prefix cache, expert tier, and drafting around
that runtime. Treat its numbers as a whole-system result, not as proof that an OpenVINO backend alone is faster.

Useful sources:

- ArcInt README: <https://github.com/marfrit/arcint>
- Strata Intel Arc status: [INTEL_ARC.md](INTEL_ARC.md)
- Strata B60 community report: [2026-10-04-community-2x-arc-pro-b60](https://github.com/Niko1221/Strata/blob/f40c418e483adc89e2e4f58e602c32a7e7d892c8/bench/results/2026-10-04-community-2x-arc-pro-b60/README.md)

## What is comparable

Compare only rows with the same:

- GPU and number of cards
- model family and quantization
- context length and actual prompt tokens
- MTP or no MTP, and draft count
- KV type and whether KV is resident or streamed
- fresh prompt versus prefix reuse
- engine build, driver, and runtime

ArcInt's served `coder` and `agent` rows are not Strata Flash-Next rows: they use different checkpoints and
OpenVINO IR artifacts. They are useful Intel Arc evidence, not a direct backend comparison.

The closest public Flash-Next clue in the ArcInt README says that, on one Arc Pro B60, ArcInt's Flash-Next path was
422 tok/s prefill at 20K tokens and 33-35 tok/s decode with MTP 2, while "Strata's own engine" was 620 / 37.5 on the
same card. That points away from an urgent OpenVINO rewrite.

## Decision rule

Use this ladder before adding backend code:

1. If Strata SYCL is within about 20% on the same card, model, prompt, and MTP settings, do nothing.
2. If ArcInt wins only in one phase, copy the small idea first: KV format, prefix/cache policy, expert tier sizing,
   runtime flags, or drafting policy.
3. If patched OpenVINO wins Flash-Next by 1.5x or more in repeated same-workload runs, prototype one isolated path.
4. Build a real OpenVINO backend only after the isolated path wins and its maintenance cost is clear.

ponytail: this deliberately avoids a benchmark harness; the existing community report format is enough until the
same-card runs show a real gap.
