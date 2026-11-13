# Strata integration: Orca Direct and Orca L0

**Orca L0 is the recommended path. Orca Direct is WIP and an alternative.**
Orca L0 uses Level Zero. Orca Direct names the DG2 backend (`STRATA_ORCA_DG2=ON`),
with Windows D3DKMT or Linux DRM submission. Installer runtime identifiers
remain `orca`. The [2026-11-04 frozen-version comparison](../../bench/results/a770/orca-direct-anchor-2026-11-04.md)
records **49.30 tok/s Direct / 56.24 tok/s bracketed L0**. Linux GPU execution
remains unvalidated. The build flow and historical measurements below describe
the L0 path unless explicitly marked DG2/Direct.

The [full current-source comparison](../../bench/results/a770/orca-direct-full-current-2026-11-06.md)
matches all 120 complete protocols and draft counters with Direct128/L0256.
Direct repeat-2K TTFT is 16.630%/13.509% shorter in opposed orders, and
repeat-8K is 7.154%/6.765% shorter. Short net decode remains
51.192/51.384 versus L0 52.493/52.755 tok/s (-2.479%/-2.598%).
The maximum control drift, 5.866%, fails the 3% gate; these are observations,
not a stable overall speedup. The initial pre-READY 7.74-GiB cache residency
failure is retained and unresolved. The completed comparison uses a runtime
relink that only adds failure diagnostics, with allocation controls passing;
cache capacity, reserve, production EXEs/configs and Linux GPU status are retained.

Windows Direct now also takes global local-memory availability into account.
The [native-counter calibration and model check](../../bench/results/a770/orca-direct-global-memory-2026-11-06.md)
compares D3DKMT counters with independent Sysman readings, checks allocation
rollback and query failures, and matches seven complete mixed 2K/8K/Short
protocols at 5771 slots and 1536-MiB reserve. Fixed adapter/segment identity is
cached; memory checks refresh residency without adding kernel-dispatch queries.
This is an accounting correction; no new performance or Linux GPU result is
claimed, and production EXEs/configs and the earlier failure status are retained.

The [actual-replay Short state census](../../bench/results/a770/orca-direct-state-domains-2026-11-06.md)
excludes another proposed source of Direct overhead: both exact-output requests
show no within-list systolic transition and no redundant invariant-SBA rewrite.
They each execute 67,271 walkers, including request preparation and prefill;
intrusive logging does not establish throughput or per-token counts. The original
publisher's JSON-key comparison failures are retained; the [supplemental audit](../../bench/results/a770/orca-direct-state-domains-2026-11-06-audit.md)
verifies the unchanged capture and exact archive without another GPU run.

The [startup window-recapture follow-up](../../bench/results/a770/orca-direct-scratch-recapture-2026-11-06.md)
reduces whole-request scratch-base changes from 28 to 7 and parameter-base
changes from 50 to 0, with identical walker counts and four diagnostic outputs.
All 90 uninstrumented model protocols match at the same cache/reserve, but net
decode changes by -0.352%/-0.582% in opposed orders; TTFT changes by
-1.751%/+0.497%. Control drift passes; the gain gate fails. The candidate stays
isolated, with no default or deployment change. Recording layout also changes,
so the packet reduction cannot be attributed to scratch alone.

The [frozen full-build context census](../../bench/results/a770/orca-direct-queue-topology-2026-11-06.md)
matches 14 calibrated mixed-history protocols and 28 phase counts. L0 net decode
uses one compute/one copy context; Direct uses three compute/one copy across
Short, 2K and 8K. Total created contexts, including inactive ones, are four and
nine. The initial seven exact L0 outputs retain an API-attribution failure;
the corrected observer also tracks submits triggered by host synchronization.
The report counts (API, first-argument handle) identities, not unique handles
or Streams. These intrusive diagnostic counts establish no timing gain, context
merge safety or Linux GPU result; runtime and deployment defaults are retained.

This is Strata's native Orca backend, now selected by the Intel installer. It builds a separate `strata`
executable with native device/context ownership, memory, queues, events and
recorded graphs. It compiles Strata's existing numerical kernels and the native
XMX launch function from `prefill/gemm.dp.cpp` through the Orca compiler
adapter. Generated copies replace host submission and scratch types. The native
FP16 XMX path now uses M16×N64 register reuse for M≥128 with N/K divisible by
64, preserving K16 accumulation order. The original M4 path handles trailing
rows and other shapes; BF16 retains its existing implementation.
The SYCL baseline source files remain available for comparisons.

The current U4/min16 L0 model also has two IQ2_XXS GEMV screens:
[targeted 256 GRF](../../bench/results/a770/orca-gemv-xxs-grf-2026-11-05.md)
and [physical SG16](../../bench/results/a770/orca-gemv-xxs-sg16-2026-11-05.md).
Both match complete outputs, scratch and draft counters. Code/Prose decode
changes are −0.232%/−0.268% and −0.222%/+0.103%, respectively; neither passes
the 1% gate. Production images and the U4N16 configuration remain unchanged.

The [current expert-chain diagnostic](../../bench/results/a770/orca-decode-expert-stages-2026-11-05.md)
retains normal forks and validates 249 windows. IQ2_S dominates the sampled GU
intervals, which also contain matching shared-expert work. An uninstrumented
2K/full128 trial of the existing three-entry reuse path matches all 21 requests;
Code decode changes −2.849%, while Prose's +11.871% control-time drift excludes
that pair. The candidate is not adopted; no new stable decode gain is established.

The [chunk-work cache-heat trial](../../bench/results/a770/orca-decode-cache-work-2026-11-05.md)
matches all 21 current-U4 requests. Code's observed +0.894% is excluded by
−3.147% control-time drift; Prose observes +1.281% with −2.999% drift.
The joint gate fails, so there is no reverse trial or adoption. This is a
logical chunk-read heuristic, not measured PCIe traffic; production heat
scoring, U4N16 assets and the native fallback remain unchanged.

The [current-U4 HC1 register-image screen](../../bench/results/a770/orca-hc1-q8-grf-u4-2026-11-05.md)
changes only the fused Q8 image from 128 to 256 GRF. All 42 full protocols match;
Code/Prose decode observations are +1.349%/+1.730% forward and
+1.932%/+4.415% reverse, with outer time drift within 3%. Reverse runs also
show substantial variation between retained samples; all samples remain in
the report. These are limited same-day measurements.
The [service and adoption checks](../../bench/results/a770/orca-hc1-q8-grf-u4-adoption-2026-11-05.md)
subsequently pass all 18 mixed-history/actual HTTP generation requests and two
final-path smoke requests. With 166 memory samples per process at 1 Hz, minimum
free VRAM is 1519.145/1390.508 MiB for 128/256 GRF; sampling cannot rule out
lower values between samples. The primary U4/min16 L0 configuration now selects
the permanent HC256 images on its next start. Its EXE, U4N16 assets, RAM guard,
native fallback and profiles are retained. The L0/Direct generated build
commands also carry the single-kernel flag; Direct deployment is unchanged.
That service/adoption check does not establish 8K speed, new PPL or Linux GPU results.

The [subsequent matched U4/M16 context comparison](../../bench/results/a770/orca-hc1-production-contexts-2026-11-05.md)
matches all 120 complete protocols. Within Direct, HC128→HC256 improves Short
net decode **45.494→47.119 tok/s (+3.571%)** while reducing TTFT
**766.087→754.491 ms (−1.514%)**. L0 improves both Short metrics too:
**+2.479% decode / −1.485% TTFT**. With the same HC256 math, Direct Code2K/8K
TTFT is **11.039%/7.462% shorter than L0**, while net decode is still
**2.211%/0.627% lower**; Short decode remains **2.524% lower**. Prefill and
decode have different bottlenecks; shorter TTFT does not establish a causal
decode regression. Long cases have one retained sample per process, with all
controls archived. These shared kernel gains do not isolate API-layer gains.
The targeted GRF flag is now a source build default; this context experiment
did not replace deployed executables or configs, or close the Direct 8K fault.

The [generic M16 comparison](../../bench/results/a770/orca-generic-projection-m16-2026-11-04.md)
matches all 102 model requests. Each backend uses one EXE/pack and switches
M16 off/on/off: Direct 2K/8K TTFT changes **23.676→15.754 s / 67.963→38.362 s**;
L0 changes **27.071→19.917 s / 71.359→43.575 s**. With the same M16 math,
Direct TTFT is **20.90%/11.96% shorter than L0**, but Short remains
**58.30 Direct / 59.37 L0 tok/s**. Long cases run once per process; no stable
Short gain or broad PPL conclusion follows. Both transports benefit from this
math optimization. `STRATA_ORCA_XMX_WIDE` defaults to 1; explicit 0 disables
it, and changing it requires a process restart. Rebuild the native backend
and its XMX pack together; deployed EXEs/configs are unchanged. Linux GPU
execution remains unvalidated; the intermittent Direct 8K fault remains open.

The [current-v14 GRF state screen](../../bench/results/a770/orca-direct-current-grf-2026-11-05.md)
uses frozen generic M16 Host/math and the current runtime source, with one Direct
EXE selecting full or compact invariant-state programming. All 150 Short
protocols match; decode changes **+0.139%/+0.204%** in opposed windows, with
**−0.490%** full-policy control drift in the second. Compact still trails L0 by
**2.075%/2.466%**. The candidate remains isolated and disabled; these are not
production U4, long-context or Linux GPU results.

The [current-U4 adjacent GRF-barrier screen](../../bench/results/a770/orca-direct-grf-barrier-2026-11-06.md)
matches all 150 clean Short protocols; decode changes **-0.050%/+0.160%**
in opposed orders. Removing 1,097 tracked duplicate barriers at capture does
not meet the preset 1% gain gate. The isolated candidate stays default-off;
capture counts are not replay-weighted per-token counts. Both policies pass
28 native CTests, 128 HC checks and five Linux/WSL CPU checks in total;
there are no new long-context or Linux GPU measurements.

The [current SG16 HC Q8 128/256-GRF screen](../../bench/results/a770/orca-hc-sg16-grf-2026-11-06.md)
passes 256 full HC checks and all 180 Short protocols. Direct decode changes
**+1.142%/+0.743%** in opposed orders while TTFT shortens
**0.237%/0.385%**; L0 decode changes **+0.070%/+0.059%**. Both fail the preset
two-order 1% gain gate; that original failure remains recorded. Under the user's
later acceptance of small positive gains, the [source-default adoption](../../bench/results/a770/orca-hc-sg16-grf-adoption-2026-11-06.md)
omits this override for Direct (128 GRF on the measured A770); L0 retains 256.
Both generated build recipes and source equivalence pass. Rebuild/rebake is
required; deployed executables and configs are unchanged.
Both variants report zero spill and 9,728 private bytes per hardware thread;
payload ABI matches but machine code changes too. These are resource/codegen
observations; the initial screen did not measure occupancy or long contexts,
and Linux GPU execution remains unvalidated.

The [follow-up context check](../../bench/results/a770/orca-hc-sg16-grf-contexts-2026-11-06.md)
matches 100 complete protocols. Direct128 versus256 net decode changes
**+1.320% Short / +3.010% repeated2K / +2.440% repeated8K**, with shorter TTFT.
The original 3% control-drift gate fails (L0 repeated8K decode +5.145%, Direct
Short -3.211%), so these are supporting observations. Direct128 still trails
L0 Short by 1.359%; long positions are single samples. Defaults retain the
user-authorized adoption above; no stable all-case superiority or deployment
change is claimed.

The [matched GRF128 HC1/router split](../../bench/results/a770/orca-direct-hc-route-split-2026-11-06.md)
matches 90 protocols and 2,610 windows in opposed orders. Direct's HC1 interval
takes **10.706%/9.639%** longer, versus **2.891%/2.279%** for router/device-plan.
HC1 T4 has the larger gap. Primary control drift stays below 0.65%; the 3% gate
passes. Both arms use the same fused-Q8 GRF128 pack; defaults remain Direct128
and L0256. The 146 ordered marks/window perturb scheduling, so these are
diagnostic intervals, not clean throughput gains or pure kernel times.

The [five-point actual-HC1 follow-up](../../bench/results/a770/orca-direct-hc-internal-2026-11-06.md)
matches all 90 protocols and 2,610 windows with the same GRF128 math on both
backends. Opposed-order norm intervals take **95.162%/96.302%** longer on
Direct; down takes **2.586%/2.866% less**, while up takes **5.842%/5.011% more**.
The whole-HC1 interval takes **13.785%/13.660% more**. Whole-window and Host-wall
control drift stay below 0.20%. These are 242 ordered marks/window with a
1,952-byte readback, using actual half1 boundaries rather than kernel labels.
Marker/synchronization costs remain inside each interval; the norm result
prioritizes a no-intermediate-marker replay, not a proven pure-kernel slowdown.
Defaults and deployment stay unchanged; no new long-context or Linux GPU result.

The [independent norm replay](../../bench/results/a770/orca-direct-hc-norm-replay-2026-11-06.md)
then reuses that exact baked norm kernel/392-byte ABI with only two endpoints
per 128/512-launch list. All 72 CPU-reference cases and 1,440 complete replays
pass with identical output hashes. Across both orders and all T1/4/8 cases,
Direct GPU time per norm is **13.029%–16.669% lower**; maximum GPU control drift
is **0.181%**. For residual-on T4/input0/512 reads, Direct/L0 are
**3.882904/4.488322 µs** forward and **3.882650/4.486694 µs** reverse.
This hot-cache single-buffer control does not reproduce the marked model's
norm slowdown: layer working sets, surrounding state/dependencies and probe
boundaries need investigation. It is not a model-throughput result. The four
pre-GPU build/audit failures remain preserved; no numerical kernel was rebuilt.

The [buffer-bank follow-up](../../bench/results/a770/orca-direct-hc-norm-banks-2026-11-06.md)
keeps that zebin/ABI and rotates 1/48/192 independent buffers, with residual-on
T1/4/8, two inputs and 192/768 launches. All **108 CPU/eager/source/guard cases
and 2,160 full replays** pass. For T4/input0/768, Direct GPU time relative to
L0 changes **-15.205%/-14.065%** with one bank, versus **-7.356%/-4.028%**
with 48 banks (18.756 MiB logical active footprint). Thus expanding independent
working sets narrows this component advantage; it does not reproduce the
marked model's near-doubling. Across all 72 GPU comparisons, time changes range
from **-19.876% to +3.096%**, with maximum control drift **11.970%**;
23 comparisons exceed the earlier 3% drift reference. Some directions reverse,
so this is not a universal stable gain. All samples remain archived, without
rescue repeats. The next isolation is surrounding kernel state/dependencies
and marker boundaries; bank rotation alone neither models nor rules out real
layer-cache effects. No runtime/default/deployment change or model-speed claim.

The [ordered-marker density control](../../bench/results/a770/orca-direct-hc-norm-density-2026-11-06.md)
now changes only the timestamp period (endpoints only, every 8 norms, every
norm), preserving the zebin, ABI, data and 1/48-bank layouts. All **72 CPU/
eager/source/guard cases and 4,320 complete replays** pass, with every raw
marker checked and saved. For T4/one-bank/input0/768, Direct relative GPU time
is **-13.493%/-13.499%** with endpoints, **-2.381%/-2.857%** every 8 norms,
and **+60.818%/+61.338%** every norm. Those six controls stay within 2.906%.
Ordered instrumentation can therefore reverse this component's backend ranking;
the marked model's earlier +95% interval cannot be treated as pure norm cost.
All 144 comparisons remain archived (10 exceed the earlier 3% drift reference,
maximum 8.259%). Even every-8 instrumentation materially changes some cases;
there is no universal per-marker correction. Recheck model gaps with no inner
markers and coarse window endpoints before prioritizing surrounding state.
This is diagnostic progress, not new model throughput or a runtime change.

The [getter128 build/output check](../../bench/results/a770/orca-hc-q8-getter128-2026-11-06.md)
keeps 128 GRF and zero spill while removing the fused-Q8 declaration of 9,728
private bytes per hardware thread. ELF text grows 27,392→27,456 bytes. All 256
complete generic128/getter128 HC checks pass across both backends; production
entries 2/3 are covered, with all 64 output hashes identical. This is an isolated
candidate; compiler resources alone do not establish speed, and earlier
getter256 failures remain.

The [clean Direct Short model trial](../../bench/results/a770/orca-hc-q8-getter128-model-2026-11-06.md)
matches all 90 full protocols. Its stable first bracket changes net decode by
**-0.364%**, TTFT by **-0.105%**, and whole-request time by **+0.303%**.
The reverse bracket's -3.152% decode comes with -4.289% decode / +5.970% TTFT
control drift, so it cannot establish a regression of that size. Original
drift/gain/time gates remain failed. No positive gain is established; getter128
stays isolated and the adopted Direct GRF128 default remains. There are no
new long-context, PPL, Linux GPU or 8K reliability results.

The [fused HC Q8 preferred-SLM trial](../../bench/results/a770/orca-direct-hc-slm-2026-11-06.md)
matches L0's allocation preference for the target fused Q8 kernel, with 28
native CTests, 72 full packet comparisons, 128 HC checks and all 90 clean Short
protocols passing. Opposed-order net decode changes are **+0.057%/+0.065%**,
versus **+0.257%/+0.587%** control drift; TTFT changes **+0.314%/+0.026%**.
These observations establish no throughput benefit. The SLM candidate stays
isolated and default-off; the adopted Direct128/L0256 build defaults remain.

The [fused HC Q8 indirect-length trial](../../bench/results/a770/orca-direct-hc-indirect-2026-11-06.md)
changes only the target walker length **832→800 bytes**, retaining the padded
allocation, payload, SBA window and other policies. All 28 CTests, 144 complete
CPU packet comparisons, 128 full HC checks and 90 clean Short protocols pass.
Opposed-order decode changes are **-3.427%/+0.007%**; TTFT changes
**+2.835%/+0.866%**. Controls pass the original drift gate, but the first whole
request regresses 3.427%. No consistent gain is established; the candidate
remains isolated and default-off. Cached component logs also place both
backends' compute submissions on node5; this adds no new hardware queries.

Both field trials select kernel metadata/shape, so matching HC0 and HC1 fused-Q8
calls can both be affected. The checker's `HC1.*` labels do not identify a model
stage; these results are not HC1-only ablations. The earlier SLM report retains
its original title and evidence. The separate HC1/router interval diagnostic
above uses actual Verifier stage boundaries.

The [fixed-T HC follow-up](../../bench/results/a770/orca-hc-fixed-t-2026-11-06.md)
keeps one kernel entry and specializes T1/T4 without changing dispatch or policy.
Both helper versions pass 256 full HC checks each; all 180 corrected-candidate
Short protocols match. Opposed-order decode changes are **+0.032%/-0.413%
Direct** and **-0.887%/-0.441% L0**. Neither passes the preset 1% gain gate;
defaults stay unchanged. Correcting parameter copies nearly restores declared
private space, but text still grows 27,840→90,880 bytes. Resource metadata and
exact ISA are archived; they alone do not establish the performance cause.

The [separate T1/T4 entry follow-up](../../bench/results/a770/orca-hc-split-t-2026-11-06.md)
reduces captures to 112/328 bytes and declared private space to 1,792/5,248
bytes per hardware thread, retaining the original 25 native images/Host ABIs.
All 256 HC cases and 180 Short protocols match, but opposed-order decode changes
are **-0.025%/-0.469% Direct** and **-2.275%/+0.061% L0**. The candidate stays
isolated: the resource savings do not establish consistent model gains, including
under the later authorization to accept smaller positive improvements.

The service source defaults `STRATA_ORCA_PLE_ROW0` to 1: after submitting commit,
prefetch the known next-window first token, then draft. The consumer now
[reuses a matching prefetched prefix](../../bench/results/a770/orca-ple-prefix-2026-11-05.md)
and submits the new suffix before collecting, with all writers complete before
buffer reuse. For one prefetched token followed by a four-token batch, logical
row requests fall from 80 to 64; physical I/O and model-speed gains are unmeasured.
The existing CPU selftest passes 336 prefix/batch cases per reader implementation;
the native CPU-only `strata-orca-ple-reader-check` CMake/CTest target also passes.
All 16 rebuilt U4 Host/retained-library protocols match on both backends through
cold Code2K→8K→Prose2K→Short histories. These use HC128, not the HC256 combination.
Set 0 to disable service prefetch; rebuilding is required and deployed EXEs are
unchanged. The [earlier source checks](../../bench/results/a770/orca-ple-row0-source-2026-11-05.md)
retain the initial Direct 8K all-zero failure; it remains open.

The [earlier HC128 GPU endpoint diagnostic](../../bench/results/a770/orca-direct-hc1-span-2026-11-05.md)
passes 360 output/guard/span checks and finds Direct T1 fused-Q8 spans about 19%
longer inside the GPU list.
The [single-image 256-GRF screen](../../bench/results/a770/orca-hc1-q8-grf-2026-11-05.md)
then removes reported spills and passes 960 complete-output/guard checks on both
backends. Private memory remains; component percentages do not establish model gains.
The [post-HC256 endpoint replay](../../bench/results/a770/orca-hc1-post256-span-2026-11-05.md)
passes 720 output/guard/span checks and 72 CPU references. For T1/input0, Direct's
fused GPU span is now **4.917%/5.424% shorter** in opposed orders; ordinary HC
still takes **4.699%/4.303% longer**. The old fused-chain attribution does not
apply to this HC256 replay. Ordinary HC and submission boundaries need mapping
to real decode; hot-buffer spans include dependency gaps and do not explain
the entire residual model difference.

The [ordinary-HC GRF screen](../../bench/results/a770/orca-hc-plain-grf-2026-11-05.md)
retains the fused HC256 image and changes only ordinary SG16 up from 128 to
256 GRF. All 720 output/guard/span checks and 72 CPU references pass. Direct
T1 ordinary-chain wall throughput improves **1.81–5.23%**, but T4 loses
**7.55–8.71%**, and L0 regresses. The preset joint gate fails; ordinary up
remains 128 GRF. Both images have zero spills and identical private allocation,
so register count alone cannot select the best policy. This is component
evidence, without a new full-model, TTFT or Linux speed claim.

The [narrow-capture model check](../../bench/results/a770/orca-hc-up-capture-2026-11-05.md)
passes 108 complete protocols but changes L0/Direct Short net decode by
−0.879%/−1.208%; TTFT also lengthens, so the candidate remains isolated.
The [Q8 split/subgroup follow-up](../../bench/results/a770/orca-hc-q8-sg16-2026-11-05.md)
retains those failures and separately records SG16 bitwise checks, the failed
joint component gate, and the matched full model. Shared kernel gains do not
establish gains from API removal; neither experiment changes production defaults.

The subsequent source adoption selects the combined narrow-capture + SG16 fused-Q8
implementation, preserving all eight token slots and the 256-GRF setting for
that fused kernel. The earlier narrow-capture-only and component-gate failures
remain recorded. Both complete current-tree Windows L0 and Direct builds pass
the PLE CPU CTest and all 64 HC eager/replay output, input and guard checks per
backend, using the retained matching HC pack for that build-time check. Static
PE audits find the expected `ze_loader.dll` on L0 and no Level
Zero/SYCL/UR/oneMKL direct imports on Direct; this does not audit dynamically
loaded or transitive DLLs. The matched clean-build L0/Direct/L0 cohort passes
all 60 complete protocols and its 3% control-drift gate. It uses one fresh pack:
1,254 ZEBIN images, with 24 unsupported variants explicitly retaining SPIR-V.
Warm Short measures **51.228 L0 / 49.480 Direct tok/s** (Direct −3.411%).
The Direct decode gap narrows to **−1.298% at repeated Code2K / −1.181% at
repeated 8K**, while TTFT is **16.496% / 7.772% shorter**. Cold first Code2K
TTFT is instead **4.971% longer**. Each long position has one sample per arm;
the complete history and both L0 controls are retained. This comparison does
not deploy executables or change the primary launch configuration, and does
not close the intermittent Direct 8K fault. Earlier isolated speed results
remain distinct from this clean build.
The [frozen-build report](../../bench/results/a770/orca-current-tree-2026-11-05.md)
includes both startup histories and the raw-protocol archive. It explicitly
records that the historical run did not snapshot complete model/MTP/U4 assets
or the external Direct memory module; later asset snapshots do not prove their
past contents. Subsequent source edits are preserved separately from this build.

The portable HC check uses CPU FP64 references, compares fused Q8 with a separate
quantizer, and covers T=1..8, pending writes on/off and changed inputs:

```sh
cmake --build <orca-build> --target strata-orca-hc-check
<orca-build>/strata-orca-hc-check <matching-image-directory>
```

Use `.exe` on Windows. The Linux `--frontend` validation script builds and audits
this executable without running GPU work. A new current-tree image collection
must match its generated ABI; retained deployment images are not automatically
interchangeable with a new build. No Linux GPU result follows from a build check.

The [follow-up Short model screen](../../bench/results/a770/orca-hc1-q8-grf-model-2026-11-05.md)
matches 120 protocols and measures **+2.959% Direct / +1.901% L0** net decode
in opposed 128/256-GRF orders, with small 128-GRF control drift. This uses the
frozen generic M16 Short28 profile and unchanged Host EXEs, without the new PLE
prefetch. That screen used an isolated HC image and did not test current U4;
the later current-U4 adoption is recorded above. Linux GPU remains deferred,
and matched 256-GRF Direct still trails L0 in that Short screen.
No production executable was replaced.

After rebuilding, Orca L0 defaults to asynchronous service refills from persistent
GGUF USM mirrors and preparation of known MTP position metadata before the first
round submission. `STRATA_ORCA_MIRROR_REFILL=0` and `STRATA_MTP_PRESTAGE=0`
restore the respective previous paths. Switches are read once at startup;
transient refill sources, blocking overrides, final waits and draft confidence
checks retain their ordering. Direct retains opt-in defaults following a failed
combined 8K check. See the [adoption record](../../bench/results/a770/orca-preparation-defaults-2026-11-06.md)
for source/build validation and the limits of the earlier small measurements,
and the [pipeline notes](../../docs/ORCA_PARALLEL_PIPELINE.md) for buffer capacities.

The [Host copy-capacity census](../../bench/results/a770/orca-copy-capacity-census-2026-11-05.md)
matches eight full model protocols. Default refill peaks at 32/64 with no
capacity flushes; asynchronous mirror refill reaches 64/64 with 62 capacity
flushes. This measures Host calls before DONE, without a timing claim.
GPU staging occupancy and FP16 double buffering remain separate questions.
The [supplemental archive audit](copy-capacity-census-a770-windows-2026-11-05-audit.json)
verifies all 1,003 files and records the later U4 primary-launcher change;
the original publication remains frozen.

The [routed prefetch-depth screen](../../bench/results/a770/orca-routed-ring-depth-2026-11-05.md)
matches 12 complete protocols with actual depth8→16→8, reserving 16 GPU slots
in both arms. It establishes no stable TTFT benefit; the default remains eight
slots. This uses frozen generic M16 and does not measure production U4, L0,
GPU occupancy or the full allocation change from production eight slots.

The [Host descriptor-depth comparison](../../bench/results/a770/orca-copy-depth-2026-11-05.md)
adds 64→256→64 with fixed 256-item storage and GPU batch64. Independent auditing
matches all 12 model protocols; 256 establishes no TTFT gain, so the default
stays 64. The retained original verifier error and corrected rerun instructions
are documented in the [pipeline notes](../../docs/ORCA_PARALLEL_PIPELINE.md).

The [original-binary zero-output reprobe](../../bench/results/a770/orca-legacy-zero-reprobe-2026-11-05.md)
matches six complete historical M4 requests in two fresh 2K→2K→8K processes.
Only the existing post-zero diagnostic is enabled; it triggers no GPU readback.
The original all-zero 8K failure remains archived and unresolved. This is
correctness evidence, without a fix, ring-capacity cause or performance claim.

The [combined U4/M16 measurement](../../bench/results/a770/orca-u4-m16-combined-2026-11-05.md)
preserves new L0 and Direct profiles using 9,402 existing U4 experts and four-reader
loading. Against the retained U4/load4 executable, independent opposing cohorts
improve warm Code/Prose prefill throughput by 47.47–49.68% on L0 and 55.94–58.17%
on Direct. Both complete quality TSVs match the previous U4 version byte for byte;
L0 also matches one fresh-process 8K compatibility pair. At the user's request,
this measured Windows A770 machine's main entry selects U4/M16 L0, with automatic
native fallback below the 63.23 GiB available-RAM preparation budget or when RAM
cannot be queried. Previous executables, profiles and weight files remain available.
Installer-wide defaults are unchanged, decode retains its existing kernels, and
the intermittent Direct 8K issue remains open.

The [current-U4 fused-dequant trial](../../bench/results/a770/orca-u4-fallback-2026-11-05.md)
matches raw quality scores, full outputs and draft counts. Its reverse Code cohort
gains only 0.78% prefill and 0.33% request throughput, below the predeclared
1%/0.5% gates. The existing switch remains off; no default change is adopted.

The [current-U4 row-threshold trial](../../bench/results/a770/orca-u4-threshold-m16-2026-11-05.md)
adopts `STRATA_U4_MIN_ROWS=16` on this Windows A770 L0 main profile. Independent
opposing cohorts versus min32 gain 1.99–3.04% Code and 2.08–2.53% Prose prefill
throughput; request throughput gains 1.49–2.35% and 1.45–1.74%. The fixed256-target
aggregate PPL is +0.3101% versus native and +0.1857% versus min32. Candidate Prose
content and draft counts change, so this is a measured quantization-dispatch
tradeoff. Actual server first8K→Short→Code/Prose2K→same8K plus real localhost HTTP
checks pass; the two8K outputs/counts agree with each other, while their tokens
differ from min32. No8K quality equivalence, steady8K speedup or new GEMV speedup
is claimed. The same EXE/images/9402 assets, RAM guard and native fallback remain
available, together with the previous min32 profile and a new min16 optional config.

Fresh default builds pass 168 edge checks and 240 canonical FP16/BF16 shapes,
including eight changed-input replays, across both backends. Rebuilt
`strata-orca-kernel-check` exercises the M16 threshold, M4 tails, strided inputs,
dimension fallbacks and the existing 840 pair checks per backend.

The [Short bounded-wait/CPU screen](../../bench/results/a770/orca-direct-hybrid-wait-2026-11-04.md)
matches 210 requests across two opposed windows. Adding 8-us/32-us spin before
Direct's event wait establishes no throughput improvement; the candidate stays
isolated. Direct event decode remains 2.13–4.40% below L0, with about 97.92%
less **whole-request process CPU** on the fixed profile (startup/warmup excluded,
prefill/decode included). Separate 30-request diagnostics locate most L0 CPU
inside `zeCommandListHostSynchronize`. Default waiting and deployed EXEs remain
unchanged; this does not measure energy or close the intermittent Direct fault.

U4 N16 prefill is an opt-in A770/oneAPI 2025.3 path for both L0 and Direct.
It retains native FP16 pairs and batching for the remaining experts, with the
original routed output offsets. Decode keeps its original kernels. The 3,234
BF16-derived U4 experts add 8.33 GiB of host weight payload and 42.2 MiB of
device weight banks, plus activation workspace. Optional profiles and the
RAM/resource preflight are documented in
[A770 performance](../../docs/A770_PERFORMANCE.md#内存充足时的可选-u4-n162026-11-04).
The [U4 report](../../bench/results/a770/orca-u4n16-2026-11-04.md) separates
whole-model measurements from the complete-output operator checks.

The optional [optimized profiles](../../bench/results/a770/orca-u4-optimization-2026-11-04.md)
reuse 9,402 existing experts (24.21 GiB host payload) with `STRATA_U4_MIN_ROWS=32`
and remove redundant GU activation read masks. On A770/oneAPI 2025.3, warm
2K/128 Code/Prose prefill throughput improves 21.19%/19.64% on L0 and
15.99%/14.71% on Direct against their current native controls. The 256-target
diagnostic aggregate PPL change is +0.1242%. Preparation checked a 63.23 GiB
RAM budget; first-request lazy loading is slower. The old profiles remain
available, decode is unchanged, and the Direct 8K issue remains open.

`STRATA_PREFILL_U4_EXPERTS=1` enables this path, with
`STRATA_PREFILL_U4_EXPERT_DIR` pointing to the checked assets, `STRATA_U4_TILE_N8=0`,
`STRATA_U4_GROUP_PRIORITY=id` and `STRATA_U4_MIN_ROWS=128` for the original
3,234-expert profiles (`32` for the optimized profiles). It requires native,
single-A770 routed staging. CMake includes the U4 kernel pack; the manual
`strata-orca-u4n16-check` target checks real weights, routed gaps, tails, bank
reuse and guards against SYCL references. Compile `u4n16_check.cpp` with SYCL
without `STRATA_ORCA_NATIVE` and use `images reference-dir write` to create
references; use `images reference-dir compare` with each Orca check executable.
Keep references outside version control and hold `tools/bench_deepinfer.py`'s
`gpu_lock()` for compilation and GPU checks. The CPU-only XMX adapter check is
`python tools/test_orca_xmx_prepare.py`.

`STRATA_PREFILL_FP16_PAIR=1` selects native two-expert FP16 prefill on A770
with native XMX and the expert chain enabled. Separate GU and Down grids each
combine two experts with distinct weights and their actual row counts. The
current path uses M16 tiles for experts with at least 128 rows and M4/NSG8 for
smaller experts and tails, retaining FP32 accumulation and FP16 hidden values;
see the [2026-11-04 pair-tile checks](../../bench/results/a770/orca-fp16-pair-tiles-2026-11-04.md).
It supports the existing batch8 completion policy and reuses the two DQ slots
and GEMM workspace; IQ weight storage and transfer formats stay unchanged.
When absent, the flag now defaults to on only in native-expert builds on
A770 with native XMX and the expert chain enabled. Explicit `0` disables it. The native kernel check compares complete
GU/hidden/Down outputs and guards against separate expert chains, including
partial rows, asynchronous submission, graph replay and rejected inputs.

On Windows A770 on 2026-10-31, two independent OFF/ON/OFF windows confirmed
Code2K prefill throughput gains of 16.384% / 15.978% and Prose2K gains of
15.729% / 15.706%. Whole-request time fell by 11.770% to 12.557%; decode
throughput did not pass the gain/drift gates. The same native executable kept
batch8, packed4 value KV, MTP4/min-p0.8, lookup and automatic expert caching.
Both arms reserved 3072 MiB after runtime memory calibration; actual automatic
cache capacities varied. Complete outputs and draft work matched, 368 paired
fixed targets had identical logged scores, all six CTests passed, and the
lowest 1 Hz physical free-memory sample was 1337.012 MiB. An earlier Prose
window failed the 3% baseline-drift gate and remains in the
[measurement report](../../bench/results/a770/orca-fp16-pair-2026-10-31.md).
These are gains within the calibrated native profile, not Orca versus SYCL or
the previous 1536 MiB reserve profile.

The local `strata-iq2_xs-orca-fp16-pair.json` profile passed the actual Python
server engine with complete Short16/Code128/Prose128 outputs and native module
audits. Start it with `python -m serve.server --engine strata --config
strata-iq2_xs-orca-fp16-pair.json`. It explicitly enables the pair flag and keeps
the accepted min-p0.8/reserve3072 settings. The main profile's concurrent
min-p0.5 update was preserved during that optional-profile test. Subsequently,
the main profile adopted the checked pair build with min-p0.5 and reserve3072.
OFF/ON/AUTO passed 11 full 64-token requests (704 tokens), identical outputs,
native module audits and normal exits; the lowest 1 Hz physical free-memory
sample was 1675.168 MiB. Automatic expert slots were 5145/5186/4800; these
functional checks are not a new speed comparison. No service was restarted.
The user subsequently restored the main profile's reserve to 1536 MiB,
keeping the pair flag and min-p0.5. This takes effect at the next engine start;
the restored profile has now been measured with 5896 slots/8093 MiB; see the latest measurements below.
See the [default-adoption evidence](prefill-pair-default-a770-windows-2026-10-31.json).
See the [raw native evidence](fp16-pair-a770-windows-2026-10-31.json) for source,
artifact and log hashes, complete outputs/scores, and the retained failed window.

The 2026-10-30 build adopted the FP16 4x8/NSG8 candidate and requested 256
GRFs through a Level Zero module option. That frozen executable passed a clean
68-request regression on the Windows A770, including actual T=1, MTP capacities
4/8 and two multichunk Long8K processes. This is scoped regression evidence;
see the [M4 checks and source integration](xmx-m4-wide-a770-windows-2026-10-30.json).

The retained pre-M4 8x8 native builds and frozen SYCL baseline had intermittent
full-model errors. Increasing GRFs did not resolve those errors; the
[later 8x8 comparison](model-sycl-largegrf-a770-windows-2026-10-30.json)
failed after earlier passing runs. Historical binaries remain preserved. The
M4 checks do not identify that defect's root cause or establish a backend
speedup. The Intel installer now defaults to Orca and requires a separately
built native executable. Root setup selects it with `--backend orca`;
`--backend sycl` retains the comparison baseline. Linux compilation and GPU
validation remain unverified.

The repository hero now uses a same-parameter deployed-backend comparison:
**SYCL 41.42 → Orca L0 50.54 tok/s (+22.01%)**. Independent SYCL/Orca/SYCL
processes reached 41.46/50.54/41.39, with 0.17% baseline drift. Each sent ten
warmups and five retained Fibonacci28/natural76 requests; all 45 complete
outputs and 57/57 draft counts matched. Both held 5771 expert slots, MTP4,
min-p0.5 and decay0.95. This includes replay, scheduling and kernel differences;
it does not isolate API savings or establish general workload speed or quality
equivalence. Decode excludes preparation/prefill/HTTP. Minimum sampled free
VRAM was 847.480 MiB, below the 1024 MiB check; Linux remains unverified.
Daily balanced parameters are unchanged. See the [hero and exact reproduction
conditions](../../bench/results/a770/orca-anchor50-2026-10-31.md) and
[all samples/hashes](anchor50-a770-windows-2026-10-31.json).

Restored-reserve1536 mixed-request Windows A770 measurements: 5896 expert slots/8093 MiB; Short 24.34 tok/s, Code2K TTFT 28.728s, Long8K 77.195s. Sixteen 64-token requests, twelve retained samples, minimum 1 Hz sampled free VRAM 947.051 MiB. Same executable/images as the earlier reserve3072 run; separate time periods and automatic cache sizes, no causal reserve speedup or paired mixed-workload SYCL/quality-equivalence claim. The 1024 MiB headroom check failed; the raw failure is preserved, and these are timing observations only. See the [mixed-request samples and conditions](../../bench/results/a770/orca-reserve1536-2026-10-31.md).

The same current build reached 38.54 tok/s on continuous Sky23/64 requests using the full production profile (auto 5949 slots, ten warmups/five retained). Historical fixed5771-slot settings reached 38.42. All30 outputs and37/50 draft counts matched the mixed Short. These controls separate continuous repeated-input behavior from the mixed-request table; cache/adaptation/PCIe contributions are not isolated. See the [workload diagnostic](../../bench/results/a770/orca-short-workloads-2026-10-31.md).

The following same-source comparison is supplementary and is not the total deployed-version upgrade gain.

On 2026-10-31, an aligned Windows A770 comparison completed 72 requests and
4608 identical generated tokens, with identical draft counts, reuse and finish.
The SYCL control used the same frozen engine source, the native FP16 M4/NSG8
tile and a 256-GRF request; BF16 retained its numerical implementation. GDN A/B
used `xmx`, and the small HC/shared gates were enabled. Both backends held 6297
resident experts / 8641 MiB (`--expert-cache 6000` is a slot budget). Context
was 8192, prefill chunks 2048, with exact CPU mode and fixed MTP-only workloads.

| Maximum window | Input | Decode throughput | Whole-request throughput | Decode baseline drift |
|---|---|---:|---:|---:|
| 1 (actual T=1) | Short | +4.12% | +3.87% | -0.08% |
| 1 (actual T=1) | Code2K | +5.56% | +2.73% | -0.15% |
| 4 | Short | +2.89% | +2.66% | -2.12% |
| 4 | Code2K | +5.38% | +2.58% | -2.44% |
| 8 | Short | +2.97% | +2.99% | +0.16% |
| 8 | Code2K | +3.94% | +2.54% | +0.29% |

Each capacity used one A/B/A sequence and three retained samples per case/arm,
after warmup. The capacity-4 drift adds uncertainty to its gain estimates.
Prefill throughput improved 2.12--2.86%; the full report also retains TTFT.
Executable, image, configuration, frozen-source and benchmark-script audits
passed. These are backend gains, including scheduling and GEMM integration;
they do not isolate API entry-point cost or establish Linux performance.
Samples, hashes and controls:
[aligned M4 backend comparison](model-m4-aligned-a770-windows-2026-10-31.json).

The local main IQ2_XS profile was then switched to Orca after its actual
settings passed four reference requests (256 tokens): Code2K, Long8K, Short
and Code2K again. This used automatic expert-cache sizing, 8192-token prefill,
capacity-4 MTP and `spec-min-p=0.8`. Startup and workload module audits passed;
all model arguments were preserved and the original SYCL configuration was
backed up before the atomic configuration update. The next normal launch uses
Orca; existing processes were not restarted. This is a functional check, not
a performance measurement. Evidence:
[main-profile validation and activation](production-activation-a770-windows-2026-10-31.json).

On Windows A770, a paired four-request test of ordinary batching versus serial
MTP (capacity 4) matched all output tokens. Whole-group throughput, including
prompt reads and admission, was 2.82--2.97% lower in the three A/B/A cycles;
baseline drift stayed below 0.23%. Aggregate batch throughput was 12.98 tok/s.
This used four short prompts, 64 generated tokens each, `--expert-cache 4000`
(a slot budget; runtime INFO reported 4191 resident experts and 5760 MiB),
2048-token context, 256-token prefill chunks, fixed placement and exact CPU mode. It compares scheduling
within Orca; it does not measure a SYCL/backend speedup. Both arms reserved the
same four slots; the corresponding four-slot cost with automatic expert-cache
sizing remains unmeasured. The serving batch default remains 0; a separate
two-slot comparison below measures the single-request cost. Full samples and hashes:
[batch-group comparison](batch-groups-a770-windows-2026-10-31.json).

The subsequent `--batch 4 --batch-mtp` comparison used the same configuration
and exact four-request outputs. Compared with serial MTP, whole-group throughput
improved 10.06--10.40% in three A/B/A cycles (median 10.13%, 14.69 tok/s), with
baseline drift below 0.30%. Both arms again preallocated four slots; this does
not measure the cost of enabling slots or compare Orca against SYCL. The
executable, image packs and measurement script stayed unchanged. See the
[grouped MTP samples and audits](batch-groups-mtp-a770-windows-2026-10-31.json).

A separate native A/B/A test compared single requests with `--batch 0` against
two preallocated MTP slots, using automatic expert-cache sizing, 8192-token
context, 2048-token prefill chunks, capacity 4, exact CPU mode, no prefill
borrowing and PCIe fraction 0. The slots displaced 701 MiB of expert cache:
6184 resident experts / 8485 MiB became 5670 / 7784 MiB. All 24 requests
(1536 tokens) matched outputs and draft workloads. Three retained samples per
case and arm, after warmup, gave these changes:

| Input | Decode throughput | Whole-request throughput |
|---|---:|---:|
| Short | -3.49% | -3.64% |
| Code2K | -5.06% | -0.62% |

Decode baseline drift stayed below 0.45%. This measures the cost of keeping
slots ready while processing single GEN requests; it does not measure
concurrent throughput. It uses a different context/cache configuration from
the four-request result above. Default GEMM aggregation remains enabled;
serving slots remain opt-in. Samples, complete INFO lines and audits:
[slot memory/cache comparison](batch-memory-a770-windows-2026-10-31.json).

The shared 106299-token MTP subset head also passed serial/batch comparisons
with two requests: greedy decoding and seeded sampling with repetition,
frequency and presence penalties. Both cases matched all 128 batch output
tokens against serial execution and exercised accepted and rejected drafts.
Executable, image packs and subset files stayed unchanged. This is a
functional Windows A770 check; it does not measure a speedup. Evidence:
[subset vocabulary checks](subset-vocabulary-a770-windows-2026-10-31.json).

The root CMake entry builds this backend without configuring CUDA, HIP or SYCL:

```sh
cmake -S . -B build-strata-orca -G Ninja \
  -DSTRATA_ENABLE_ORCA=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx
cmake --build build-strata-orca
ctest --test-dir build-strata-orca --output-on-failure
```

This puts the executable and packs in `build-strata-orca/orca/`. The standalone
entry below puts them directly in `build-strata-orca/` instead.

```sh
cmake -S sycl/orca -B build-strata-orca -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=icpx
cmake --build build-strata-orca
ctest --test-dir build-strata-orca --output-on-failure
```

`-DSTRATA_ORCA_ALL_KERNELS=ON` includes the complete numerical source list.
Generate the existing IQ oracle inputs with `python tools/iq_fixture.py --out
build-iq-fixtures`, then set `-DSTRATA_ORCA_IQ_FIXTURES=<absolute-directory>`
to register the native IQ parity check.

Windows uses `icx` and `-DORCA_L0_SDK=C:/Program Files/LevelZeroSDK/1.33.1`.
Use `-DORCA_L0_BUILD_CHECKS=OFF` to omit the base runtime's separate checks.
The engine uses the same pinned ggml CPU dependency as the SYCL build;
`-DSTRATA_GGML_DIR=<llama.cpp-checkout>` reuses an existing checkout.
`-DSTRATA_ORCA_BUILD_ENGINE=OFF` builds only the numerical checks, without ggml.
Builds and GPU checks in this workspace require the shared
`tools.bench_deepinfer.gpu_lock` lease.

`strata-orca-kernel-check` composes packed embedding, scale/add, GDN gate,
weighted RMSNorm, FP16/BF16 conversion, Q8_0 dequantization and host-mapped
doorbells in one native command list. Three replays use different inputs and
check every output against CPU references, including the softplus threshold
and the changing doorbell sequence. It also checks 36 native XMX FP16/BF16
GEMM shapes with padded input strides and partial output tiles, using exact
CPU references and output guards. These are correctness checks, not model
performance measurements.

`strata-orca-queue-check` checks cached native device queries, completion of an
earlier kernel while the compute queue has later blocked work, compute/copy
dependencies, SPSC host callbacks, captured events across graph replays,
callback exception propagation, and range/fill bounds. Both executables check
loaded modules before and after GPU work for SYCL/UR/oneMKL dependencies.

`cmake --build <build> --target strata-orca-gguf-ring-check` builds the optional
CPU check of the generated native GGUF file ring. On Windows it passed invalid
geometry, tensor metadata, exact gate/up/down bytes, repeated hits, wrap,
truncated-read retry and reopen cases. The model executable stayed unchanged;
this checks the file ring without loading a model or running GPU work. See the
[ring check and hashes](gguf-ring-a770-windows-2026-10-31.json).

`strata-orca-engine-check` exercises actual device-memory budgets, poisoned
arenas and overflow rejection, an allocation above `maxMemAllocSize` when
sufficient free memory is available, external host-memory import, pitched
copies in replayed graphs, recorder/executable lifetimes and Strata's existing
`CapturedGraph` API. Large allocations negotiate the driver's advertised
relaxed-allocation extension. Unavailable host import is reported as a fallback,
not counted as registered memory. `STRATA_ORCA_IMAGES` overrides the build-time
image directory when moving the executable and its packs.

Stable queries are cached for the installed engine's lifetime: driver extension
capabilities, host import/release function pointers, Sysman device-memory module
identity/location, and the Windows DXGI adapter selected by device LUID. Free
memory, process budget/usage and completion states are queried live. The engine
check compares cached capabilities with raw driver queries, repeats host-arena
import/copy/release, and checks memory accounting after a touched allocation.

On 2026-10-31, a fresh root-CMake build with these caches passed all four native
GPU suites on the Windows A770 and the engine check with independent scheduling
disabled. Ten real-model requests produced 640 tokens matching the frozen M4
reference exactly, including draft counts, reuse and finish, at capacities
1/4/8 and across a multichunk Long8K history. Startup/workload module audits
excluded SYCL, UR and oneMKL. The executable, images, source snapshot and ggml
stayed unchanged; workspace source matched the snapshot. This checks
correctness, without establishing a cache-related throughput gain or Linux GPU
support. Commands, hashes and complete outputs are in the
[root-build/cache report](root-build-cache-a770-windows-2026-10-31.json).

The native service also implements the existing `SAVE`/`RESTORE` protocol for
`POST /slots/0?action=save|restore` when the server has `--slot-save-path DIR`.
It streams K/V through the existing session-file format, binds files to the
model, resolved settings and backend, and validates before changing device
state. `--session-min-free-mib` defaults to 4096. On the Windows A770, all six
native CTests passed on 2026-10-31, including 167 disk-format assertions and
4280 snapshot assertions with streamed payload comparisons across FP16, INT8,
Q4 and K8V4. The [component report](session-components-a770-windows-2026-10-31.json)
records the frozen build; numerical kernel images match the preceding build.
The real HTTP handlers also passed five 64-token generations: continuation
after restore and restoration in a fresh process matched the uninterrupted
reference token for token. A corrupt payload returned HTTP 400; complete
session bytes before and after refusal had the same SHA-256. Empty saves
were refused, and a filename containing Unicode and a space worked. Native
module audits passed in both processes, and executable, images and source
hashes stayed unchanged. This checks persistence on this Windows A770, without
establishing throughput gains or Linux behavior. Full outputs and artifact
fingerprints are in the [HTTP report](session-http-a770-windows-2026-10-31.json).

Root setup selects Orca when a known discrete Arc is available and no NVIDIA or
AMD card can be used. Explicit backend and CUDA-toolkit choices take precedence.
The Intel installer requires a local native build and its matching kernel packs;
it also accepts explicit host paths:

```sh
python setup.py --backend orca --model IQ2_XS
```

It discovers either CMake layout above. For another location, supply matching
executable and image paths, or run the Intel installer directly:

```sh
python sycl/setup_intel.py --orca-engine <build-directory>/strata \
  --orca-images <build-directory>/images --model IQ2_XS
```

On Windows the executable ends in `.exe`; add `--orca-lib-dir` for the native
compiler math DLL directory when it is not already on `PATH`. The option can
be repeated. The installer validates the executable and pack indexes, reuses
the model/download/pack steps, and writes `backend: orca` with direct host
paths on both platforms. It uses the Intel server wrapper without a SYCL
engine wrapper or Docker. The native config enables device routing and a host
expert mirror; it does not need the SYCL registry VRAM estimate. Use
`python sycl/setup_intel.py --runtime sycl` for the baseline. Use matching executable and image packs from
the same native build. The M4 regression below covers one Windows A770;
complete serving parity and Linux validation remain unfinished.

When all saved profiles use Intel backends, root setup also activates Orca on
startup. It discovers a saved custom Orca executable when no canonical local
build exists. Switching runtimes writes the previous config to
`<config>.sycl-backup` or `<config>.orca-backup` before conversion, retains
existing engine arguments and sampling settings, and leaves an unchanged
config untouched. Startup port, browser, host/API-key and VRAM reserve options
also apply to existing profiles. SYCL-to-Orca conversion retains custom environment values
while removing SYCL/UR selectors and old memory estimates; Linux `/work` paths
are mapped through the saved mount root. All 350 CPU-only setup checks passed
on this Windows host on 2026-10-31, including mocked Windows/Linux activation,
backup, repeated starts and explicit SYCL selection. This does not validate
Linux execution. Evidence: [installer checks](installer-a770-windows-2026-10-31.json).

Independent serving slots are available with `--batch 2..8` (currently off
by default). Active compatible slots share projections, routing, experts and
the output head; recurrence, K/V, PLE history and sampling remain separate.
The native BGEN scheduler passed exact 64-token comparisons for 2, 4 and 8
slots, plus a fixed-seed sampled two-slot comparison, on this Windows A770.
Long-prompt admissions, cancellation/yield, continued conversations, turn
checkpoints and return to solo decoding also passed with both fixed and
default weight placement. A full-slot captured window was required in each
2/4/8-slot check. These are correctness checks; their diagnostic timings do
not establish a performance gain. A separate
[penalty check](batch-penalties-a770-windows-2026-10-31.json) passed exact
two-slot comparisons with repetition, frequency and presence penalties for
both greedy and fixed-seed sampling (256 matched batch tokens). Real sequential
and concurrent HTTP requests also passed for greedy and sampled penalties:
eight 64-token generations, with exact raw token IDs and matching API choices
and usage. A captured two-slot window and native module audits were required;
the executable and checked sources stayed unchanged. See the
[HTTP batching report](batch-http-a770-windows-2026-10-31.json).
With `--batch 2..8 --batch-mtp`, each compatible slot can draft one token and
verify its two rows alongside other slots. This opt-in path requires serving,
an MTP model, `--spec >= 2` and one GPU. Slots share immutable draft weights;
their KV, scratch, sampling and accepted-prefix state remain independent.
Verification windows hold at most eight rows and rotate slots when needed.
On 2026-10-31, all six native CTests and the engine check with independent
scheduling disabled passed on Windows A770. Exact 2/4/8-slot, sampled-penalty,
output-boundary and ordinary-batch regressions matched 1674 batch tokens;
825 proposals included 692 accepted tokens. Interleaved admission, cancellation,
checkpoints and return to solo decoding passed with fixed and default placement.
These counts cover the basic comparisons; interleaving adds separate lifecycle
checks. See the [grouped MTP checks](batch-mtp-a770-windows-2026-10-31.json).
Real sequential and concurrent HTTP JSON/SSE checks also passed with two-slot
MTP: 16 generations of 64 tokens, covering greedy and fixed-seed sampled
penalties, matched raw tokens, content, finish and usage. Both accepted and
rejected proposals and a captured four-row/two-slot window were required.
Native module audits passed, and executable, images and checked service/engine
sources stayed unchanged. See the
[grouped MTP HTTP report](batch-mtp-http-a770-windows-2026-10-31.json).
Disk session files require `--batch 0`. See the
[scheduler report](batch-scheduler-a770-windows-2026-10-31.json).

For source-backed optimization ideas from other platforms, see the
[cross-platform research notes](../../docs/ORCA_CROSS_PLATFORM_RESEARCH.md).

The native `prefill::Gemm` implementation covers FP16/BF16 products, beta,
input/output row strides, padded N/K, workspace chunking and sliced native
weight dequantization. The check exercises 96 layout/shape/beta combinations
and Q8_0 scratch slices against exact CPU references.

Consecutive independent FP16/BF16 products sharing weights, N/K and dtype are
batched **by default**. Contiguous rows use one larger GEMM directly; other
layouts use one gather, one larger GEMM and one scatter. A single request uses
the ordinary path. Kernels, copies, callbacks, graph boundaries and explicit
waits flush pending work; overlapping outputs/inputs or different weights
also flush. There is no timer or wait to fill a batch. Inputs, outputs and
workspace must remain alive until queue completion. `set_auto_batch(false)`
provides the same asynchronous API without combining requests.

The current bounded window holds at most 16 requests. Combining requires
N divisible by 8, K divisible by 16 and sufficient workspace; other products
use the ordinary path. Native quantized requests are not combined across
dequantization. Arbitrary scalar functions and dependent decode steps cannot
be inferred or combined by the raw Level Zero layer. Elementwise operators
already launch vector ranges over their supplied tensor size.

The batching check verifies four products become one, including FP16/BF16,
contiguous and scattered layouts, beta, dependency boundaries, watchdog status,
disable mode and graph replay. Run `strata-orca-kernel-check <images>
--bench-batch` for an isolated native comparison on 1280x2560 and 2560x640
expert projections, T=1/4, one/eight requests and both layouts. It uses five
ABBA repetitions and reports enqueue + GPU + completion time, product counts
and variation. These are operator measurements, not model throughput.

The runtime also supports `orca::l0::replay(target, count, closed_lists)` for
one-call submission of ready command lists. Strata's queue still submits each
graph individually; it does not automatically aggregate graphs or HTTP
requests through this API. The standalone
[submission comparison](../../orca-l0/demo/api-submission-a770-windows-2026-10-31.json)
measures independent lists with disjoint outputs. Dependent MTP graphs need
their ordering preserved. With a positive `spec_min_p`, the current drafter
reads each step's probability before deciding whether to submit the next;
blindly batching those steps would change the work and stopping policy.

Measured on 2026-10-29: Arc A770 16 GB, Windows driver 32.0.101.9034,
IntelLLVM 2025.3.2, Level Zero SDK 1.33.1. All three native Strata suites and
the four standalone Orca checks passed, with import and loaded-module audits.
The IQ check covers ten types and 1..8 columns using the existing parity
tolerances; dequantization was exact, while MMVQ includes activation
quantization error. This establishes component correctness, not full-model parity.

Build the optional `strata-orca-mixed-check` target to test independent operator
branches with the existing native model kernels:

```sh
cmake --build build-strata-orca --target strata-orca-mixed-check
build-strata-orca/strata-orca-mixed-check build-strata-orca/images new-outputs.bin --bench
```

It compares an ordered graph with one relaxed-order graph and a GPU consumer
after the join. One branch quantizes FP32 inputs and runs Q6_K GEMV; the other runs BF16
GEMV and router TopK. Inputs are ready before either branch starts, and outputs
and scratch are disjoint. The demo covers 1/4/8 tokens, 640/10240 projection
rows and one/eight independent weight banks. Three changing inputs check every
output bit, guards and independent CPU64 dot-product samples. It rejects an
existing output file. `--bench` adds raw ABBA window timings, GEMV-only timings
and eager serial timings; all include host submission and completion. Timings
exclude allocation, compilation, input upload and output readback.

The eight-bank set exceeds the A770's L2 capacity, but all weights remain in
device memory. This is a scheduling demo, not a model pipeline or a mapped-host
expert test. Queue group counts are printed. Ready kernels permit scheduling but
do not prove simultaneous hardware execution. The caller supplies dependencies;
the runtime cannot infer tensor hazards from opaque kernel arguments or execute
an unknown next token. No model dispatch or default backend changes here.

On the Windows A770 above, two fresh processes passed 144 complete output
comparisons. The driver reported one compute queue index and one separate copy
queue index. The two-queue candidate's per-case median changes ranged from
-14.40% to +2.11%; eleven of twelve cases had negative medians in both processes.
The remaining case (10240 rows, one token, eight banks) changed from -5.52% to
+2.11%. No repeatable throughput gain was established, so this does not justify
adding compute queues to the model by default. Full raw samples, input/artifact
hashes and the excluded first attempt's provenance failure are in
[`mixed-schedule-a770-windows-2026-10-30.json`](mixed-schedule-a770-windows-2026-10-30.json).

A separate prototype used one regular command list with Level Zero's
`RELAXED_ORDERING` flag. It records independent quantization/BF16 products,
then a barrier, independent Q6_K products/TopK, then a barrier and the consumer.
Its numerical kernel images and weights are unchanged. Two fresh processes
passed another 144 complete output comparisons. T=1 window throughput rose
28.76%/27.21% for 640 projection rows and one bank, and 86.53%/88.44% for eight
banks. This is a change to the complete operator schedule, not an isolated
measurement of simultaneous hardware execution. The prototype's build-only
runtime patch, checker patch and raw samples are in
[`mixed-relaxed-a770-windows-2026-10-30.json`](mixed-relaxed-a770-windows-2026-10-30.json).

The confirmation adds a mapped GPU completion sequence after the consuming
kernel and its barrier. The host verifies exactly one increment after every
wait, including all timed windows. Two more processes passed all 9096 sequence
checks and 144 complete output comparisons, CPU64 samples and guards. The
sequence guards were intact and complete output files matched the ordinary
backend's frozen outputs. With these checks included, the two process medians
were as follows; each entry is a paired window throughput change:

| Projection rows | Tokens | Independent weight banks | Process 1 | Process 2 |
| ---: | ---: | ---: | ---: | ---: |
| 640 | 1 | 1 | +27.09% | +24.80% |
| 640 | 1 | 8 | +98.50% | +91.43% |
| 10240 | 1 | 8 | +32.49% | +34.50% |
| 640 | 8 | 8 | +10.38% | +10.90% |

The larger single-bank T=1 case varied from +2.96% to +13.74%; other shapes
and all individual samples remain in the
[completion-sequence confirmation](mixed-relaxed-epochs-a770-windows-2026-10-30.json).
These windows contain several operators and ready inputs. They are not eight
HTTP requests, and they establish no full-model speedup. The measurements above
describe the retained prototype, before the supported API below. In the actual verifier's GDN path, QKV and gate
GEMVs share read-only Q8 inputs while alpha/beta uses the FP32 input. Their
outputs are disjoint, but all readers must finish before the recurrent step
reuses Q8 scratch. Model integration must express those dependencies explicitly.

The supported queue API is `begin_recording(true)` followed by
`queue.independent([&] { /* disjoint, ready primitive operators */ });`.
Each phase joins before returning. Ordinary commands between phases retain
their dependencies through native barriers, without a host wait. A typed
operator containing internally dependent kernels cannot be put in a phase
without spelling out those dependencies. Eager execution and ordinary
recording use the existing in-order fallback. Nested phases are rejected.

Further scheduling opportunities follow the model's data dependencies:

| Work that can become ready together | Required join | Current coverage |
| --- | --- | --- |
| GDN QKV, alpha/beta and gate projections | Before recurrence and Q8 scratch reuse | Opt-in integration and real-weight standalone check below |
| QSA Q/K/V and index projections | Before their normalization, KV/index updates and attention; preserve Q8 input until every reader finishes | Real-weight standalone check and opt-in model integration |
| Shared-expert gate/up and scalar-gate projections | Before SwiGLU and Q8 scratch reuse; scale after down and sigmoid | Real-weight complete-chain check and opt-in integration below |
| Router and shared-expert projections | Route before selected experts; join shared/routed results before residual combine | Candidate; preserve early routing publication to the host |
| Next expert's weight upload and current expert's computation | Upload event before dequantization; last reader before staging-slot reuse | Existing copy/staging pipeline; broader lookahead needs A/B measurement |

Different operator types can share a ready phase. Only compatible products
sharing weights and dimensions can use the automatic GEMM batching path;
independent operators with different weights do not become one dense GEMM.
Module compilation, graph recording and known-weight uploads can run ahead of
their first use. Future activations and routing decisions still require their
input values. Concurrent bandwidth-heavy kernels may compete for the same
memory bandwidth, so compare the complete joined window and exact outputs.

`STRATA_ORCA_READY_OPS=1` opts native model graphs into this scheduling mode.
The verifier groups audited Q6_K, Q8_0, IQ4_XS and IQ3_S GDN QKV/gate projections
with alpha/beta, and QSA Q/K/V with BF16 indexer K/Q projections. Convolution,
recurrence and QSA consumers run after their respective joins. Other GDN/QSA
formats and their instrumented paths keep their original ordering. This experiment remains
off by default; the checks below cover the Windows A770 and one model/KV
configuration. The scheduling option preserves numerical kernels, automatic
GEMM batching and backend selection.

The shared-expert path also groups audited gate/up MMVQ projections with the
BF16 scalar-gate dot product, which reads the original FP32 activation. Its
SwiGLU, down projection, sigmoid and final scale retain their dependencies.
This does not delay the verifier's existing early router publication or move
the routed expert plan into the shared phase. Module loading and recording can
run ahead of execution; the existing copy/staging pipeline can overlap known
weight uploads with computation. Router lookahead predicts weights to warm,
not exact future expert results. GDN state and KV cache writes still require
the actual inputs and their normal commit ordering.

Build `strata-orca-shared-schedule-check` for the standalone complete-chain
check. It accepts `images fixtures new-output.bin [--bench]`; fixtures are raw
`blk.0` shared-expert gate IQ3_S `[2560,640]`, up IQ4_XS `[2560,640]`, down
IQ4_NL `[640,2560]` and scalar BF16 `[2560]` tensors, named `gate.iq3_s`,
`up.iq4_xs`, `down.iq4_nl` and `scalar.bf16`. Four fresh Windows A770 processes
covered T=1/4/8, caller/self quantization, batch enabled/disabled and three
changing synthetic activations. All 192 complete bitwise comparisons and 6048
GPU-tail epoch checks passed, including guards, immutable FP32 input and scratch
bytes. With batching enabled, paired complete-window throughput changes were:

| T | Self-quantized input, processes 1/2 | Caller-quantized input, processes 1/2 |
| ---: | ---: | ---: |
| 1 | +28.80% / +11.98% | +14.58% / +18.96% |
| 4 | +28.39% / +40.32% | +28.87% / +35.88% |
| 8 | +36.41% / +31.11% | +38.91% / +34.72% |

These are seven ABBA repetitions of eight complete windows per sample, including
submission and GPU completion. They exclude allocation, upload, JIT and readback.
They compare the new shared schedule with the same ordered numerical kernels,
not full-model throughput. There is no separate CPU64 oracle, physical concurrency
timeline or Linux GPU result. All samples, batch-disabled cases and build
provenance are in the
[shared-expert scheduling report](shared-schedule-a770-windows-2026-10-30.json).

The new shared schedule was then compared with the previous GDN/QSA-enabled
verifier in six fresh Windows A770 model processes. All 36 requests and 2304
generated tokens matched references, including accepted/offered/reused/finish;
numerical images were frozen and native dependency audits passed. Each arm
retained two Short/Code2K pairs after one warmup pair:

| Decode workload | Incremental throughput change | Baseline elapsed-time drift |
| --- | ---: | ---: |
| T=1, Short | +1.46% | +0.33% |
| T=1, Code2K | +1.56% | +0.90% |
| Capacity 8, Short | -0.13% | +0.28% |
| Capacity 8, Code2K | +0.48% | -0.11% |

Prompt-plus-decode throughput changes were +1.09%/+0.32% at T=1 and
-0.15%/+0.10% at capacity 8. These small, limited-sample results concern only
the additional shared schedule, not combined GDN/QSA gains or Orca versus SYCL.
See the [complete model comparison](shared-ready-performance-a770-windows-2026-10-30.json).
A source rebuild reproduced all twelve shared-expert SPIR-V exports and the
module index byte for byte. Export names, capture sizes and parameter layouts
also matched; the generated header changed only its debug source-line numbers.
The standalone report's `source_reproduction` includes that exact diff.

On 2026-10-31, a separate native MTP confidence screen compared request
`spec_min_p=0.5/0.7/0.8` at capacity 8 with suffix lookup disabled. This is not
the output sampler's `min_p`: greedy output remains fixed while offered and
accepted draft work changes. Fifteen screening requests and eighteen fresh
0.5/0.7/0.5 confirmation requests all matched complete 64-token references.
In Code2K, acceptance was 41/49 (83.67%) at 0.5, 39/40 (97.50%) at 0.7 and
36/37 (97.30%) at 0.8. Short measured 43/68, 39/55 and 35/55 respectively;
acceptance need not rise monotonically when window placement changes.
The one-process screen had large timing drift and does not establish a gain.
Fresh-process confirmation of 0.7 measured decode throughput changes of
+0.36% Short/+0.54% Code2K, with baseline elapsed-time drift +0.37%/-0.26%.
Prompt-plus-decode changes were +0.25%/-1.43%. Raw counters, timings and audits
are in the [MTP confidence report](mtp-minp-a770-windows-2026-10-31.json).

A subsequent fresh-process 0.5/0.8/0.5 comparison passed another eighteen
requests and 1152 generated tokens. Accepted/offered counts reproduced the
screen above. Compared with the two bracketing 0.5 baselines, 0.8 changed
decode throughput by -6.91% Short/-4.03% Code2K; baseline elapsed-time drift
was -0.63%/-0.55%. Prompt-plus-decode throughput changed -5.50%/-0.19%, with
baseline drift -0.55%/-0.48%. Code2K's higher acceptance fraction came with
fewer accepted drafts, so that fraction alone did not predict throughput.
Both confirmation experiments retain only two samples per case/arm after
warmup; these two prompts do not select a global threshold. No default was
changed, and Linux GPU behavior remains unverified. All eighteen outputs,
module audits and raw timing samples are in the
[0.8 confirmation](mtp-minp08-a770-windows-2026-10-31.json).

The supported API was checked on the Windows A770 above in two fresh processes:
144 complete output comparisons and all 9096 GPU completion-sequence checks
passed. Complete output files equal the frozen ordinary backend's files. The
queue state/exception/event-lifetime checks, numerical suite and engine check
also passed. The private build initially omitted a test-only header from its
source snapshot; that build failure and its successful retry remain in the
evidence. Model numerical images were unchanged.

| Projection rows | Tokens | Weight banks | Process 1 | Process 2 |
| ---: | ---: | ---: | ---: | ---: |
| 640 | 1 | 1 | +25.01% | +10.72% |
| 640 | 1 | 8 | +94.04% | +93.22% |
| 10240 | 1 | 8 | +32.96% | +30.49% |
| 640 | 8 | 8 | +13.53% | +10.67% |

These are paired median changes for complete ready operator windows, using the
same ABBA method above. The small single-bank case varies substantially between
processes. All twelve shapes, individual samples and build/input hashes are in
[`ready-operators-a770-windows-2026-10-30.json`](ready-operators-a770-windows-2026-10-30.json).
They establish no complete-model speedup or Linux GPU result.

`strata-orca-gdn-schedule-check` narrows the demo to the deployed GDN geometry
(K=2560, C=10240, 16/48 heads, head width 128). Pass raw `blk.0.attn_qkv.weight`
IQ4_XS and `blk.0.attn_gate.weight` IQ3_S tensor bytes from the GGUF, excluding
its header. The executable checks fixture lengths before uploading:

```sh
cmake --build build-strata-orca --target strata-orca-gdn-schedule-check
build-strata-orca/strata-orca-gdn-schedule-check build-strata-orca/images qkv.iq4_xs gate.iq3_s new-gdn.bin --bench
```

It uses real projection weights, synthetic changing activations, synthetic BF16
alpha/beta weights and synthetic convolution/norm/state. Two processes passed
96 complete output comparisons and 3012 GPU completion-sequence checks, with
unchanged state/history, finite outputs, Q8 scratch bytes and guards. The
quantization/projection/convolution/recurrence window changes were:

| Tokens | Fused output Q8 | Process 1 | Process 2 |
| ---: | :---: | ---: | ---: |
| 1 | No | +10.67% | +27.91% |
| 1 | Yes | +25.76% | +18.08% |
| 4 | No | +37.58% | +30.91% |
| 4 | Yes | +24.30% | +25.11% |
| 8 | No | +27.15% | +31.40% |
| 8 | Yes | +19.93% | +25.62% |

The window excludes the final output projection and is not a complete layer or
model. Kernel images were unchanged. Fixture hashes, every sample and exact
check scope are in
[`gdn-schedule-a770-windows-2026-10-30.json`](gdn-schedule-a770-windows-2026-10-30.json).

`strata-orca-qsa-schedule-check` compares five ready projections: Q, K, V,
indexer K and indexer Q. It joins them before their normalization/RoPE
consumers, using the deployed geometry and real `blk.3` weights:

```sh
cmake --build build-strata-orca --target strata-orca-qsa-schedule-check
build-strata-orca/strata-orca-qsa-schedule-check build-strata-orca/images fixtures new-qsa.bin --bench
```

The fixture directory contains raw tensor bytes without GGUF headers:
`q.iq4_xs`, `k.q6_k`, `v.iq4_xs` for the attention projections;
`ik.bf16`, `iq.bf16` for the indexer projections; and `q.norm`, `k.norm`,
`ik.norm`, `iq.norm` for their FP32 normalization weights. Lengths are checked.
Three changing synthetic inputs at T=1/4/8 compare every output bit against
ordered and eager execution, including Q8 input bytes, guards, unchanged FP32
inputs and GPU completion sequences. `--bench` reports seven ABBA repetitions
of the complete projection/normalization window, including submission and
completion. It excludes KV storage, pooled index updates, selection, attention
and full-model throughput. Model dispatch is unchanged by this demo.

On the Windows A770 above, two fresh processes passed 48 complete bitwise
comparisons and 1512 GPU completion-sequence checks. Both output files matched.
The paired median window changes were:

| Tokens | Process 1 | Process 2 |
| ---: | ---: | ---: |
| 1 | +34.58% | +26.34% |
| 4 | +21.46% | +21.38% |
| 8 | +18.13% | +13.17% |

Absolute T=1 ordered medians varied from 625.53 to 238.13 microseconds between
processes; these ratios describe paired windows within each process. Kernel
images were unchanged. This check reuses existing numerical kernels and has no
independent CPU64 numerical oracle, GPU timeline or Linux result. Every sample,
real-weight fixture hash and check scope is in
[`qsa-schedule-a770-windows-2026-10-30.json`](qsa-schedule-a770-windows-2026-10-30.json).

A verifier-only rebuild then enabled these five QSA projections in the native
M4 model, alongside GDN independent projections. Two fresh processes passed
eight complete requests and 512 generated tokens: actual T=1 Short/Code2K
twice, then capacity-8 Code2K -> Long8K -> Short -> Code2K with 1024-token
Prefill chunks. Exact token IDs, accepted/offered drafts, reuse and finish
matched saved healthy workloads. Logs confirmed QSA types Q=23/K=14/V=23 and
GDN types QKV=23/gate=21. Numerical images were unchanged, automatic GEMM
batching stayed enabled and native dependency audits passed. No finite scans,
Down capture, tracing, poisoning or eager mode were used; the post-zero-only
diagnostic did not trigger. This is an integration smoke check, not broad
stability proof or a QSA full-model throughput comparison. See
[`qsa-ready-integration-a770-windows-2026-10-30.json`](qsa-ready-integration-a770-windows-2026-10-30.json).

The first same-native-executable model screen stopped at T=1, candidate
iteration 1, Code2K: 64 zero token IDs after eleven successful requests. Its
`performance_comparison_valid` is false and `metrics` is empty. That executable
still limited GDN phases to Q6_K/Q8_0; the real GGUF matrices are IQ4_XS/IQ3_S,
so the GDN branch did not run, although relaxed lists and ordered fallback
barriers were enabled. The failure does not establish a numerical diagnosis or
its cause. The source now includes the audited IQ formats after the standalone
check above; this does not resolve complete-model stability.
The failed screen is preserved in
[`model-ready-operators-a770-windows-2026-10-30.json`](model-ready-operators-a770-windows-2026-10-30.json).

With the IQ formats eligible, a rebuilt candidate then matched all eight
complete Short/Code2K requests (512 generated tokens, effective T=1). Its log
confirmed independent QKV type 23 and gate type 21 projections. Token IDs,
accepted/offered drafts, reuse and finish status matched the healthy native A0
reference; automatic GEMM batching remained enabled and native module audits
passed. This is an integration smoke check, not stability proof, capacity-4/8
model acceptance or a throughput comparison. The independent-phase option
remains off by default. See
[`gdn-ready-integration-a770-windows-2026-10-30.json`](gdn-ready-integration-a770-windows-2026-10-30.json).

A later diagnostic used that same latest executable with capacity 8 and
1024-token Prefill chunks. Scheduling off passed eight complete requests,
including two Long8K requests. Scheduling on matched Code2K, then Long8K
returned 64 zero IDs. Its first non-finite Down capture was T=8, N=2560,
K=640, layer 16, expert 97, chunk position 2048. Post-execution X/W snapshots
were finite; the CPU64 comparison found exactly two bad 8x8 output tiles:
128 wrong elements, comprising 37 non-finite and 91 finite out-of-bound values.
The failure occurred in Prefill after the independent GDN phase had run in
the preceding request's decode. This does not establish scheduling causality
or the first finite-only error. Automatic batching remained enabled; model
timings are rejected. Evidence:
[`ready-down-capture-a770-windows-2026-10-30.json`](ready-down-capture-a770-windows-2026-10-30.json).

A private FP16 4x8-tile candidate then passed the native kernel/engine checks
and the same Code2K -> Long8K -> Short -> Code2K sequence: four requests,
256 generated tokens and 12 finite Prefill chunks. Complete output IDs,
draft counts, reuse and finish matched the saved healthy SYCL workloads with
independent scheduling and automatic batching enabled. BF16 exports stayed
byte-identical. Native disassembly confirms DPAS 8x4 in this candidate.
Both 8x8 and 4x8 tiles also passed 2048 products with and without an immediate
consumer on the latest captured operands: 8192 isolated checks total.
Neither tile reproduced the failure in isolation. The four model requests
are a smoke check; no fix, stability or performance claim is made from them.
The historical 8x8 binaries remain preserved. Evidence:
[`xmx-m4-model-a770-windows-2026-10-30.json`](xmx-m4-model-a770-windows-2026-10-30.json),
[`xmx-m4-isa-a770-windows-2026-10-30.json`](xmx-m4-isa-a770-windows-2026-10-30.json).

The same private M4 candidate subsequently passed a clean 68-request regression
across five fresh model processes: twenty Short/Code2K requests each at actual
T=1 and maximum MTP capacities 4/8, then two Code2K -> Long8K -> Short -> Code2K
processes with 1024-token Prefill chunks. All 4352 generated token IDs, draft
counts, reuse and finish states matched saved healthy references. Independent
GDN projections and automatic GEMM batching remained enabled. No finite scans,
Down capture, tracing, poisoning or eager mode were used; the post-zero-only
diagnostic did not trigger. Images/executable stayed unchanged and native
module audits passed. This is stronger regression evidence for these workloads,
not a root-cause diagnosis, performance comparison or default-backend switch.
See [`xmx-m4-wide-a770-windows-2026-10-30.json`](xmx-m4-wide-a770-windows-2026-10-30.json).

The canonical adapter now reproduces that checked M4 source byte for byte.
A narrow rebuild also reproduced all three SPIR-V exports, `index.txt` module
flags and the generated parameter ABI byte for byte. BF16 stayed unchanged;
the canonical build selects the checked M4 FP16 export with 256 GRFs. Source,
command and artifact hashes are in the same report's `source_integration`.
This is not a clean full-engine or Linux build check.

With the frozen M4 executable, a separate native A0/B/A1 model comparison
disabled/enabled/disabled GDN independent projections. All 36 complete requests
and 2304 generated tokens matched references and workload counts. Each arm
used one warmup and two retained Short/Code2K pairs; automatic GEMM batching
remained enabled and native module audits passed. QSA independent scheduling
was not present in this executable.

| Decode workload | Throughput change | A0-to-A1 elapsed-time drift |
| --- | ---: | ---: |
| Actual T=1, Short | +9.43% | +9.81% |
| Actual T=1, Code2K | +3.04% | -1.21% |
| Capacity 8, Short | +3.67% | +1.11% |
| Capacity 8, Code2K | +2.74% | +0.77% |

Changes use the geometric mean of A0/A1 median decode times divided by B's
median, minus one. The T=1 Short drift limits any persistent-gain claim for
that case. Code2K prompt-plus-decode changes were -0.82% at T=1 and +0.64% at
capacity 8; their elapsed-time baseline drifts were -2.17% and +1.21%.
These samples compare native scheduling policies, not Orca against SYCL.
All times, exact outputs, drift and limits are in
[`m4-gdn-performance-a770-windows-2026-10-30.json`](m4-gdn-performance-a770-windows-2026-10-30.json).

The existing Q6_K PF2 path was also screened against the default aligned-load
path using the same frozen executable, A0/B/A1 processes and complete output
files. All 216 output comparisons passed. Quantization + GEMV window changes
ranged from -59.81% to +10.76%. The positive case was 640 rows, eight tokens,
eight banks; it lacks independent confirmation. Several large-projection
regressions were much larger than baseline drift. This compares complete kernel
paths, including their compiler/register choices, not prefetch alone. No global
or shape-specific dispatch was changed. Every sample and drift is in the
[GEMV PF2 screen](gemv-prefetch-a770-windows-2026-10-30.json).

For **eight ready requests**, the same native implementation with batching
disabled/enabled measured as follows (microseconds per complete window):

| N / K | T per request | Layout | Disabled | Enabled | Paired median speedup |
| --- | --- | --- | ---: | ---: | ---: |
| 1280 / 2560 | 1 | contiguous | 2713.15 | 336.81 | 8.22x |
| 1280 / 2560 | 1 | gather/scatter | 2562.53 | 327.83 | 7.76x |
| 1280 / 2560 | 4 | contiguous | 2600.12 | 460.93 | 5.58x |
| 1280 / 2560 | 4 | gather/scatter | 2460.08 | 365.14 | 6.81x |
| 2560 / 640 | 1 | contiguous | 1524.04 | 188.55 | 8.53x |
| 2560 / 640 | 1 | gather/scatter | 1562.69 | 271.45 | 5.65x |
| 2560 / 640 | 4 | contiguous | 1509.80 | 212.25 | 7.37x |
| 2560 / 640 | 4 | gather/scatter | 1566.05 | 292.45 | 5.59x |

Each enabled window used one matrix product instead of eight. The two latency
columns are medians of ABBA window means; the speedup is the median paired
ratio, so it need not equal the ratio of those columns. With only one request,
paired medians ranged from -3.27% to +5.43%; every case's five-repetition range
crossed zero. These short measurements do not establish a single-request gain
or regression. The inputs/weights are constant FP16 values with checked outputs
and guards. These operator results do not predict full-model throughput.
All 16 cases, variation ranges and source/binary hashes are in
[`batch-a770-windows-2026-10-29.json`](batch-a770-windows-2026-10-29.json).

On 2026-10-30, the managed 256-GRF pack passed the same operator demo in two
fresh processes, sixteen cases each. For eight ready requests, the paired
median window speedups ranged from 5.165x to 9.020x across cases and processes;
each enabled window used one GEMM instead of eight. Timings include host
enqueue, GPU work and completion, with constant FP16 inputs and checked
outputs/guards. Single-request medians ranged from -7.130% to +5.426%, with
some signs reversing between processes; these samples establish no persistent
single-request gain or regression. The demo emits per-case medians and paired
ranges, not individual timing samples. These are native batching comparisons,
not SYCL or full-model speedups. Current hashes, both runs and limits:
[`batch-largegrf-a770-windows-2026-10-30.json`](batch-largegrf-a770-windows-2026-10-30.json).

Completion events reuse slots in native pools through atomic bitmaps; the last
queue reference returns each slot after its GPU uses finish. The queue check
also exercises repeated reuse. GPU event profiling is not implemented and fails
explicitly. Native XMX performance selection still needs full-model validation.

On 2026-10-30, the native executable passed all four Strata suites and all five
standalone Orca checks, including numerical kernels and copy-queue access above
4 GiB. A full-model **actual T=1** A/B/A run completed 24 requests with identical
greedy output tokens and decode workloads. Startup and workload module audits
found no SYCL, UR or oneMKL modules in the native process.

The configuration was IQ2_XS, FP16 KV, context 8192, 6297 expert-cache slots
(8641 MiB) and a 33.02 GiB host mirror on the same Windows A770 platform listed
above. Each arm ran one warmup and three retained 64-token requests per case;
MTP and suffix drafting were disabled. Changes below use the geometric mean
of the A0/A1 time medians divided by the B median, minus one:

| Input | Prefill throughput | Decode throughput | Prompt + decode throughput |
| --- | ---: | ---: | ---: |
| Short | -1.20% | -0.53% | -0.78% |
| Code2K | +13.58% | -0.62% | +11.33% |

Decode's A0-to-A1 drift was +1.09% and +0.80% in elapsed time, respectively.
These three-request samples do not establish a persistent decode regression.
The Code2K gain is an observation for this backend and configuration; identical
greedy tokens do not establish identical internal Prefill states or isolate a
gain from runtime submission alone. All per-request tokens/times, inputs,
module names and frozen source/binary/SPIR-V hashes are in
[`model-a770-windows-2026-10-30.json`](model-a770-windows-2026-10-30.json).
Those hashes identify the measured build before host diagnostic logging was
added. The enclosing original suite remains incomplete because its later
speculative comparison was rejected.

The long-prompt speculative run produced identical output tokens and 38
accepted drafts, but offered 43 drafts on the baseline and 44 on Orca. This also
occurred with suffix lookup disabled and MTP cost-based window selection off.
Those speculative timings are excluded from same-workload comparisons.
The remaining oneMKL BF16 scalar shared-expert gate (N=1, K=2560) produces
different rounded FP32 results from native XMX. Its first-layer inputs and
weights were byte-identical; gate outputs differed by at most 8.35e-7, while
both products were close to a CPU double reference. Enabling the opt-in
`STRATA_PREFILL_XMX_SHARED_GATE=1` on the SYCL comparison arm, together with
`STRATA_PREFILL_NATIVE_XMX=1`, runs that projection with the same padded XMX
algorithm already used by Orca.

With that one projection aligned, all 16 captured first-two-layer tensors,
32 sampled final residual rows, GDN state digests and 53 draft-window records
matched across backends. Code2K then offered 44 drafts on both. Batching stayed
enabled and native SPIR-V was unchanged. The state digests are only 16-bit and
the residuals are sampled; this is evidence for the observed divergence,
not universal numerical or model-quality validation. Diagnostic readbacks and
logging invalidate those runs' timings. Evidence and limits are in
[`prefill-gate-a770-windows-2026-10-30.json`](prefill-gate-a770-windows-2026-10-30.json).
The original T=1 table above retains its original scalar-gate baseline and
hashes.

A subsequent A/B/A run aligned that scalar gate and kept default matrix
batching enabled, with tracing and diagnostic readbacks off. Actual T=1 and
MTP capacity 4 each completed all 24 requests with identical output tokens,
accepted/offered drafts and reuse. Observed throughput changes were:

| Maximum MTP window | Input | Prefill | Decode | Prompt + decode |
| --- | --- | ---: | ---: | ---: |
| 1 (no MTP) | Short | +0.16% | +0.13% | +0.15% |
| 1 (no MTP) | Code2K | +4.38% | +0.07% | +3.83% |
| 4 | Short | +1.55% | +1.37% | +1.48% |
| 4 | Code2K | +7.38% | -2.48% | +6.67% |

T=1 decode baseline drift was -0.11% for both inputs. Capacity-4 decode drift
was +4.28% / +2.42%; Code2K Prefill drift was +6.15%. Three retained samples
do not establish a persistent decode gain or regression. The longer Prefill
results include the native GEMM layout/staging implementation, so they do not
isolate submission overhead. This build's sources, binaries, SPIR-V, all 48
validated requests, ranges and module audits are in
[`model-aligned-partial-a770-windows-2026-10-30.json`](model-aligned-partial-a770-windows-2026-10-30.json).
Its hashes identify the measured build before asynchronous finite diagnostics.

**MTP capacity 8 remains unstable.** The native arm's sixth request returned
64 zero tokens and accepted/offered 0/0 instead of 41/49. The comparison
rejected this workload; the enclosing suite remains incomplete and capacity-8
timings are excluded. A separate no-readback repeat with startup/workload
module audits also reproduced the problem on its first long request; the
following long request recovered. A concrete cross-thread event-publication
race was fixed, and three initial clean processes passed 24 requests, but that
fix did not eliminate the model failure. Passing component checks or a few
repeats does not establish full-model stability.

A 1024-token-chunk comparison later reproduced the all-zero symptom on an
8000-token prompt after a successful Code2K request. The post-zero-only check
found all 248320 logits and all 10240 values of the final verifier residual
non-finite at position 7999; the logged first four logits were NaN. Orca
returned 64 zero IDs with no drafts. SYCL completed the same sequence normally,
and Orca's following Short and Code2K requests matched their references.
This establishes numerical corruption for this reproduction; the first bad
operation and its relationship to the historical failures remain unknown.
The check failed and its timings are excluded. The executable, kernel images
and selected source files were preserved locally before further diagnosis.
Exact workloads, hashes and logs:
[`multichunk-failure-a770-windows-2026-10-30.json`](multichunk-failure-a770-windows-2026-10-30.json).

On the same build with per-half finite counting enabled, all ten Prefill chunks
of Code2K followed by Long8K had finite residuals. Code2K matched, but Long8K
still failed the workload comparison: its first differing output ID was at
index 6, and draft counts were 38/46 instead of 35/57. This different outcome
under additional GPU scans does not locate the earlier corruption or establish
numerical parity. Evidence:
[`multichunk-finite-a770-windows-2026-10-30.json`](multichunk-finite-a770-windows-2026-10-30.json).

Scanning only each chunk's final residual row reproduced the corruption within
Prefill: Long8K's first chunk was finite at that point, while every later chunk
had 10240 non-finite values in the scanned row. Code2K still matched its
reference. Other rows and the first bad operation were not checked. Evidence:
[`multichunk-last-a770-windows-2026-10-30.json`](multichunk-last-a770-windows-2026-10-30.json).

Last-row scans after every half-layer found no non-finite values and matched
all output IDs, but Long8K's draft counts differed (35/54 versus 35/57).
Single-checkpoint runs reproduced all-zero output again: at layer 23 half 1
the last row remained finite in every chunk; at layer 35 half 1 it was entirely
non-finite from the second chunk onward. These are separate processes with
different scan locations, so they guide diagnosis without proving the first
bad layer or excluding earlier corruption of other rows or persistent state.
The executable and model kernel images were identical between those two
single-checkpoint runs. Logs, workload comparisons and hashes:
[`prefill-half-locate-a770-windows-2026-10-30.json`](prefill-half-locate-a770-windows-2026-10-30.json).
A third checkpoint at layer 29 half 1 matched both workloads and counted only
finite values. Different checkpoint locations alter execution; these results
cannot be treated as a binary search for the first bad layer.

An optional fused diagnostic counts non-finite values while the ordinary
residual-write body writes the last row, without another scan dispatch. Its
Code2K/Long8K control matched all output IDs and draft counts, with finite
checked rows. The native suite checks its finite/NaN/Inf counts, last-row scope,
guards and residual parity with the ordinary kernels. Two existing SPIR-V
images changed only `OpName` type suffixes; their headers and all other
instructions were identical. Two separate diagnostic kernels were added.

The native XMX adapter now also selects the existing SYCL A770 NSG1 variant
for BF16 N=8, K=10240, T=1024/2069. All 1263 existing symbol-mapped kernel
images stayed byte-identical; one NSG1 image was added. Four native suites
passed, including the expanded 96-case GEMM API check. With Prefill diagnostics
disabled, Code2K -> Long8K -> Short -> Code2K matched every output ID, draft
count, reuse and finish. This single passing sequence does not establish a
NaN fix or a performance gain. Both controls, module audits and hashes:
[`prefill-xmx-validation-a770-windows-2026-10-30.json`](prefill-xmx-validation-a770-windows-2026-10-30.json).

The next three fresh native processes repeated that four-request sequence
without Prefill diagnostics: 10 of 12 requests matched the full reference
workload. In the first process Long8K kept identical output IDs but offered
56 drafts rather than 57; its following final Code2K returned 64 zero IDs.
All 248320 logits and 10240 final residual values were non-finite at position
2047. The other two processes matched every request. The failed executable,
selected sources and images were frozen locally. This repeat failed acceptance;
the NSG1 alignment did not eliminate the numerical problem. The report also
contains three repeats of the frozen SYCL baseline:
[`xmx-repeat-a770-windows-2026-10-30.json`](xmx-repeat-a770-windows-2026-10-30.json).
All three SYCL processes returned 64 zero IDs and drafts 0/0 on Long8K,
then recovered on Short and Code2K; their other nine requests matched. The
executable hash matches the original
reference and was preserved locally. It predates the post-zero diagnostic,
so this SYCL failure has no logits/residual NaN classification. The shared
output symptom means the failure is not observed exclusively in Orca; a common
root cause is still unproved, and both failed workloads remain rejected.

Three further native processes repeated the same sequence with fused
last-row diagnostics enabled. Eight of twelve requests matched exactly;
four returned 64 zero IDs with all logits and the final residual non-finite.
Nine Prefill chunks recorded bad last-row residuals. Their first recorded
bad half was a GDN half: layer 12, 24, 25 or 36 (zero-based), depending on
the request. Earlier rows and finite errors are not covered by this probe,
so this does not identify the originating operator. The executable, selected
sources and images match the preserved failure capsule above. Timings are
excluded. Full logs, counters and hashes:
[`prefill-fused-repeat-a770-windows-2026-10-30.json`](prefill-fused-repeat-a770-windows-2026-10-30.json).

An additional run scanned complete FP32 GDN tensors and recurrent state at
twelve points per GDN layer. All twelve Prefill chunks had finite checked
values and all four requests matched output token IDs; the first Code2K
offered 51 drafts instead of 49, so the complete workload check still failed.
No non-finite GDN stage was captured in this run. Extra scans change
scheduling, and finite counts do not detect finite arithmetic errors or
overflow in unscanned FP16 buffers. All 1264 existing GPU images were
unchanged and four native suites passed before the model check. Evidence:
[`prefill-gdn-locate-a770-windows-2026-10-30.json`](prefill-gdn-locate-a770-windows-2026-10-30.json).

Two fresh processes then repeated this stage probe. Both Long8K requests
returned 64 zero IDs and non-finite logits/residuals. In the first failing
chunk, the first recorded bad GDN stage was already its mixed input: layer 26
at position 1024 in process 0, and layer 16 at position 2048 in process 1.
Both counted 20480 bad mixed elements, equivalent in count to eight 2560-wide
rows; element coordinates were not captured. Subsequent chunks entered that
GDN layer with 786432 non-finite recurrent-state values. This shows a path
from bad input to persistent contamination; it does not locate where the
input first became bad. Five of eight full request workloads matched, with
one additional Code2K draft-count mismatch. The failing build was frozen.
Logs, counts, output IDs and hashes:
[`prefill-gdn-repeat-a770-windows-2026-10-30.json`](prefill-gdn-repeat-a770-windows-2026-10-30.json).

The next run checked **all residual rows** within each half-layer write,
plus FP32/BF16 HC intermediates and mixed FP16/BF16 buffers before GDN.
Long8K first recorded 232 non-finite residual elements in layer 37 half 1
(MoE) at position 4096; every earlier half in that chunk recorded zero.
The following layer's normalization images and GDN inputs/state then became
non-finite. The final Code2K also failed, first recording 156 bad residual
elements in layer 34 half 1 at position 0. Two of four complete workloads
matched. These observations move the investigation before GDN to the MoE
half, without proving which operation produced the bad values. The build
and images were frozen; no performance result is accepted from this run.
All 1262 non-diagnostic kernel images were byte-identical to the preceding
build. Four native suites passed, including every FP16/BF16 bit pattern in
the new finite checker and full/last-row fused counter guards/parity.
Logs and hashes:
[`prefill-gdn-all-a770-windows-2026-10-30.json`](prefill-gdn-all-a770-windows-2026-10-30.json).

A focused MoE probe then checked routing, shared-expert outputs, gathered
FP16 inputs, Gate/Up output, hidden FP16 values, Down output and final combine,
along with all residual rows. Its first four-request sequence matched exactly.
In two fresh repeats, six of eight workloads matched; one final Code2K and
one Long8K returned all-zero IDs and non-finite logits/residuals. In their
first bad chunks, layer 28 had 66 non-finite Down and combined-output values,
and layer 10 had 71. Other scanned MoE tensors in those layers were finite;
their residual writes recorded 264 and 284 bad elements respectively.
Dequantized expert weights were not scanned, so this does not establish the
cause of the bad matrix outputs. Extra scans change scheduling. All 1265
GPU images were unchanged from the preceding build. An initial implementation
error in this probe's mode guard was caught and its run stopped/excluded;
the corrected build passed four native suites before the checks above.
Evidence:
[`prefill-moe-locate-a770-windows-2026-10-30.json`](prefill-moe-locate-a770-windows-2026-10-30.json)
and [`prefill-moe-repeat-a770-windows-2026-10-30.json`](prefill-moe-repeat-a770-windows-2026-10-30.json).

A further `moe-weights` run added FP16 scans of each selected expert's
dequantized Gate/Up and Down weights before GEMM and before buffer reuse.
All four requests matched output IDs and draft workloads, and all twelve
Prefill chunks had finite checked weights, MoE tensors and residuals.
This run did not capture the failing output, so it cannot establish whether
weights were finite in the earlier failed runs. These extra scans perturb
scheduling; the unchanged GPU images and passing checks are not a fix.
The passing build was frozen. Evidence:
[`prefill-moe-weights-a770-windows-2026-10-30.json`](prefill-moe-weights-a770-windows-2026-10-30.json).

An opt-in snapshot then captured the first non-finite per-expert Down output
before dequantization or scratch reuse. In Long8K, at position 4096, layer 45,
expert 191, its shape was T=16, N=2560, K=640. The saved X and W were entirely
finite. CPU64 comparison found 64 bad cells, precisely rows 0..7 and columns
1920..1927: 15 NaNs and 49 finite values outside the FP32 accumulation bound.
This is one 8x8 XMX output tile; T=16 needs no tail padding. Other three
requests in the sequence matched IDs and draft workloads. Four component
suites passed, including an injected-NaN check of capture ordering and guards;
all 1265 existing images were unchanged, with one capture image added.

The frozen failing operands subsequently passed 6144 isolated GEMMs and
6144 GEMMs followed immediately by a GPU consumer, across six fresh processes.
Every output, guard and final operand byte was checked. A deliberately shifted
reference was rejected. These replays change allocation and scheduling, so they
do not clear the model failure. The snapshot observes operands immediately
after the product, not during it; compiler, driver, hardware and ordering or
memory corruption remain hypotheses. Evidence and local frozen capsule hashes:
[`prefill-down-capture-a770-windows-2026-10-30.json`](prefill-down-capture-a770-windows-2026-10-30.json).

A diagnostic FP16 NSG1 comparison also failed. All 1266 preceding images
were unchanged, with one NSG1 image added. Four suites and 6144 captured-input
consumer GEMMs passed with NSG1 explicitly enabled. Across three full model
processes, only 6 of 12 requests matched the reference. The initial Long8K
diverged at zero-based token 17 despite finite counters; later processes
returned NaNs. Two Down snapshots again had finite X/W: T=2 with 32 bad
output cells across two column tiles, and T=32 with one bad 8x8 tile. Changing
work-group subgroup count is rejected as a fix; default remains NSG8.
Evidence: [`prefill-down-nsg1-a770-windows-2026-10-30.json`](prefill-down-nsg1-a770-windows-2026-10-30.json).

A separate register-file comparison keeps NSG8, the same executable and every
SPIR-V image. Only the FP16 kernel's module flags add
`-ze-opt-large-register-file` in a private image pack. Three fresh model
processes matched all 12 reference requests, with all 36 Prefill chunks finite
under the MoE/residual probes. Two more fresh processes matched eight requests
without those scans or Down capture. Each process ran Code2K -> Long8K ->
Short -> Code2K, generating 64 tokens at MTP capacity 8 with 1024-token chunks.
The native component kernel check and 2048 captured-input immediate-consumer
GEMMs also passed. A direct Level Zero native-binary query confirmed 128 GRFs
by default and 256 with this flag; both module build logs were empty. Evidence:
[`prefill-down-largegrf-a770-windows-2026-10-30.json`](prefill-down-largegrf-a770-windows-2026-10-30.json).

The managed native build now applies that module option only to the FP16 NSG8
export. All 1266 reachable kernel SPIR-V hashes and module flagsets match the
checked pack. Numerical kernel bodies are unchanged; the rejected FP16 NSG1
diagnostic branch was removed. Four native suites passed after rebuilding,
and Short -> Code2K matched two further full workloads without finite scans or
Down capture. Loaded-module audits excluded SYCL, UR and oneMKL. An earlier
source-GRF-property attempt changed XMX scheduling decorations and native code;
it was rejected before any model request and remains excluded from evidence.
The module-option comparison does not prove the root cause, broad stability,
performance or Linux behavior. Integration artifacts, hashes and checks:
[`prefill-down-grf-integration-a770-windows-2026-10-30.json`](prefill-down-grf-integration-a770-windows-2026-10-30.json).

Native-binary disassembly of that same FP16 SPIR-V found DPAS and no DPASW
in both the 128-GRF and 256-GRF builds. Both binaries request disabled EU
fusion. These are static instruction/metadata observations, not a scheduling
trace or a diagnosis of the intermittent corruption. Binary and assembly hashes:
[`xmx-isa-a770-windows-2026-10-30.json`](xmx-isa-a770-windows-2026-10-30.json).

The managed build subsequently passed 36 further Short/Code2K requests across
three fresh processes: actual T=1, MTP capacity 4 and capacity 8, twelve requests
each. Every output ID, accepted/offered draft count, reuse and finish matched
the previously validated reference for that configuration. These runs use
8192-token Prefill chunk capacity and 64-token generation limits, with automatic
matrix batching enabled. No finite scans, Down capture, tracing or allocation
poisoning ran. The post-zero-only diagnostic never triggered; startup/workload
module audits excluded SYCL, UR and oneMKL. This is a regression check against
saved references, not a new successful SYCL performance comparison. Evidence:
[`native-largegrf-wide-a770-windows-2026-10-30.json`](native-largegrf-wide-a770-windows-2026-10-30.json).

The unchanged real-input GEMM consumer check also ran with a separate native
model process loaded and idle, under one shared GPU lease. Three fresh checker
processes passed 6144 products with exact operands and no bound, non-finite
or guard failures. Short before the checks and Code2K afterwards matched
all output IDs and draft workloads. The model kept its 6297-slot cache and
33.02 GiB host mirror; startup reported 1858 MiB free VRAM. This control does
not use the failing Down shape (N=2560, K=640) or its dequantized weight reuse,
and does not clear full-model instability. Evidence:
[`gemm-model-resident-a770-windows-2026-10-30.json`](gemm-model-resident-a770-windows-2026-10-30.json).

The manual `strata-orca-gemm-replay-check` target isolates the existing FP16
XMX GEMM using the frozen real T=703, N=1280, K=2560 case from the earlier
standalone SYCL numerical investigation. Build it explicitly, then run:

```sh
strata-orca-gemm-replay-check <images> <frozen-fixture> <new-capture-directory> [--consumer]
```

It keeps eight independent outputs, reads them after each eight-call batch,
and checks every element against the frozen CPU64 reference and FP32 error
bound, plus output guards and exact final input/weight bytes. On this Windows
A770, three independent processes completed 2048 calls each (6144 total)
without a numerical or guard failure. The fixture hashes match the historical
SYCL reproduction. A failure saves all eight outputs and operand readbacks.
An additional three processes passed 6144 GEMMs with `--consumer`: the existing
GPU scale kernel doubles each output immediately after its GEMM, without a
host wait between the two, and every element is checked against twice the
reference and bound. The checker also rejected all eight outputs and saved
exact operand readbacks when one reference cell was deliberately shifted.
The additional readbacks change scheduling; these passing checks do not
explain the historical standalone GEMM errors or full-model corruption, and
their timings are excluded. Results and hashes:
[`gemm-replay-a770-windows-2026-10-30.json`](gemm-replay-a770-windows-2026-10-30.json).

Fixtures may provide `shape.txt` containing T, N and K; without it the original
703/1280/2560 case is used. X is padded to a multiple of eight rows with zero tail rows.
To inspect a captured Down product and create a new replay fixture, use
`python sycl/orca/inspect_down_capture.py down.bin new-fixture-directory`.
It uses the workspace's existing NumPy for full CPU64 products and conservative
FP32 error bounds; `--self-check` checks a known product and malformed input.

The unchanged historical SYCL reproducer was also rerun on this host using
the same frozen real-input fixture. Three fresh processes each passed 2048
joint-matrix calls and 2048 independent ESIMD FP16 calls: 12288 products in
total, with GPU checks after every product and counter readback every eight.
No bad output was captured. This failure did not reproduce in these runs;
the saved historical bad outputs and the current full-model failures remain
unexplained. The executable hash matches the historical failing report.
Results and hashes:
[`frozen-sycl-gemm-a770-windows-2026-10-30.json`](frozen-sycl-gemm-a770-windows-2026-10-30.json).

An A/B control with synchronous residual readbacks matched all output IDs and
draft counts: 32 sampled Code2K rows and 125 sampled Long8K rows were also
byte-identical. SYCL repeated its earlier workload. Readbacks alter scheduling;
this passing control does not clear either failed comparison above or identify
the first bad operation. Evidence:
[`multichunk-residual-a770-windows-2026-10-30.json`](multichunk-residual-a770-windows-2026-10-30.json).

A later capacity-8 A/B/A run completed all 24 requests with exact output IDs,
draft counts, reuse and finish. Its observed decode changes were -0.29% for
Short and -0.90% for Code2K; Prefill changes were +0.69% and +3.96%.
These timings remain excluded from release performance claims because the
earlier zero-output failure is unexplained. The measured executable and images
were frozen before the Windows mirror-budget fix. Full diagnostic evidence is
in [`model-cap8-repeat-a770-windows-2026-10-30.json`](model-cap8-repeat-a770-windows-2026-10-30.json).

The rebuilt engine also passed a capacity-8 check with a 256-token generation
limit: two Short/Code2K pairs per backend, eight requests total. Short inputs
finished normally at EOS after 70 tokens; long inputs produced 256 tokens.
Every output ID, draft count, reuse and finish matched. All eight supported
verifier window sizes were captured. The post-zero-only diagnostic did not
trigger, and native module audits excluded SYCL, UR and oneMKL. This expands
the checked decode length but does not resolve the historical failure. Timings
remain diagnostic. Full workloads and hashes are in
[`long-generation-a770-windows-2026-10-30.json`](long-generation-a770-windows-2026-10-30.json).

The queue regression covers 128 threaded compute/copy handoffs. Explicit
barrier events are published by their submitting thread; a foreign thread
cannot materialize an unpublished lazy completion. For further diagnosis,
`STRATA_PREFILL_FINITE_ASYNC=1` counts non-finite residual values on the device
after each half-layer and reads the counters at the existing chunk completion
wait. Its check covers finite inputs, NaN/Inf, strided iterations and output
guards. It is disabled by default and its timings are diagnostic only.
Mode `STRATA_PREFILL_FINITE_ASYNC=last` scans only the final residual row at
the chunk's end. Mode `tail` scans that row after every half-layer; `half:N`
scans it only after half-layer N (zero based: `2 * layer + half`). No extra host
waits are added. Mode `fused` counts non-finite last-row values in separate
residual-write kernel variants, retaining the ordinary residual arithmetic.
Mode `gdn` adds full FP32 GDN stage and state scans to those fused counters,
with the same chunk-end readback; these extra GPU dispatches also perturb execution.
Mode `gdn-all` also checks HC intermediates and their BF16 images, mixed
FP16/BF16 images, and every residual row instead of just the last row.
Mode `moe-all` checks the full MoE intermediate tensors and all residual
rows instead of the GDN stages. The existing chunk wait reads every counter.
Mode `moe-weights` also scans every selected expert's FP16 dequantized
Gate/Up and Down buffers before its products on the non-MMQ path.
`STRATA_PREFILL_CAPTURE_DOWN=<new-file.bin>` adds an optional per-expert check
on that FP16 path. The first non-finite Down output saves six little-endian
int64 values (T, N=2560, K=640, layer, expert, chunk position), then complete
row-major FP16 X, FP16 W and FP32 Y. One work-group detects and copies it on
the same in-order compute stream before known reuse; later calls preserve
the first capture. Buffers live outside borrowed cache memory and are read
at chunk completion. No per-GEMM host wait is added. It detects non-finite
outputs, so finite-only errors may precede it. The extra launches and mapped
memory accesses change scheduling; these timings are diagnostic only.
Invalid half indices fail explicitly. `STRATA_VERIFY_ZERO_DIAG=1` reads logits and the final residual
only after a verifier window has already returned all-zero token IDs; it adds
no GPU work before that result. An all-zero ID is a diagnostic trigger, not
by itself a numerical-error test.

`ORCA_L0_POISON_ALLOCATIONS=1` fills fresh device, host and shared allocations
with `0xff` before returning them. The standalone poison check verifies every
byte, including an odd-sized tail, then runs the inference checks. With this
mode enabled and disabled in separate processes, eight capacity-8 Short/Code2K
requests matched the exact reference workloads. Four native Strata checks and
four standalone checks passed, and kernel images were unchanged by the runtime
edit. This covers fresh allocations, not unwritten parts of reused scratch;
it does not resolve the earlier failure. Timings are diagnostic. Evidence:
[`allocation-poison-a770-windows-2026-10-30.json`](allocation-poison-a770-windows-2026-10-30.json).

The queue check also forces overlap in an eight-slot staging ring. Each of
16 bursts submits 128 copies and consuming kernels before releasing a host
gate, so slot reuse must wait on the correct compute completion. Every output
is checked; there is no CPU wait per expert. All 2048 jobs passed with default
and poisoned allocations, including completion-pool reuse. The model executable
and its kernel packs were unchanged. This checks the queue dependency pattern,
not the complete Prefill path or model stability. Results, logs and the hashes
of the strengthened standalone poison check are in
[`staging-ring-a770-windows-2026-10-30.json`](staging-ring-a770-windows-2026-10-30.json).

The same queue check now exercises PLE's double-buffer upload protocol with
64 ten-MiB payloads. The host reuses a buffer only after the following in-order
`uint64_t` fill publishes its sequence; GPU kernels check every uploaded word.
The first copy is host-gated and its sequence must remain zero until release.
Default and poisoned allocations both passed. The model executable and model
kernel packs stayed unchanged. This isolates the upload protocol and does not
clear the complete pipeline's Long8K failure. Logs and hashes:
[`upload-sequence-a770-windows-2026-10-30.json`](upload-sequence-a770-windows-2026-10-30.json).

`STRATA_PREFILL_FINITE_ASYNC=poison` also fills the overlaid attention/MoE
scratch with `0xff` at each half-layer boundary, then counts non-finite
residuals as in mode `1`. Persistent residuals, HC intermediates, GEMM workspace
and staging/dequant buffers are outside that fill. Two capacity-8 Code2K
requests had zero bad halves and matched all reference tokens and draft counts;
the accompanying short requests and four requests with the mode disabled also
matched. The rebuild passed four native suites without changing any kernel
image. Poisoning perturbs execution and does not resolve the earlier failure.
Results and logs:
[`scratch-poison-a770-windows-2026-10-30.json`](scratch-poison-a770-windows-2026-10-30.json).

With the per-half-layer diagnostic enabled, a separate 32-request repeat
matched every reference workload and all 16 long prompts had finite residuals
at every half-layer. This perturbs GPU execution and does not clear the
failure above. An audit by kernel symbol found all 1269 existing kernel
images byte-identical to the frozen pre-diagnostic build; only the finite
counter kernel was added (the pack's file numbering changed).

A later build passed all four native GPU suites, including 12 additional
long-Prefill GEMM cases (84 API cases total): 2047/2048 rows, interleaved
outputs, scalar/HC projections, beta=0/1, NaN-poisoned workspace and guards.
On that build, 16 consecutive capacity-8 requests with final-row checks and
32 with Prefill checks disabled matched every reference workload. The latter
enabled only the post-zero verifier diagnostic; it never triggered. All
eight final-row checks were finite. These repeats add evidence but do not
resolve the earlier intermittent failure. Exact request tokens, counts,
hashes and component results are in
[`repeat-validation-a770-windows-2026-10-30.json`](repeat-validation-a770-windows-2026-10-30.json);
its timing fields are diagnostic and excluded from performance comparisons.
That report also includes 16 exact matches with the frozen pre-diagnostic
executable and kernel images, with all diagnostic flags disabled. The original
frozen snapshot lacked `packs.txt`; the replay capsule supplies the unchanged
pack list after checking every frozen pack exists. The original snapshot is
unchanged. This passing repeat does not explain its earlier failure.

The real HTTP/service/engine path also passed 32 requests across the two
backends: OpenAI JSON and SSE, Anthropic Messages and Responses, at MTP
capacities 4 and 8. Later cases read a 2077-token code prompt twice. With two
conversation checkpoints enabled, repeat requests reused 16 short-input tokens
or 2070 long-input tokens. Exact input/output IDs, accepted/offered drafts,
reuse counts and API content matched SYCL. Native startup/workload module
audits excluded SYCL, UR and oneMKL. These diagnostic checks do not establish
capacity-8 stability or measure performance. Full results and hashes are in
[`http-a770-windows-2026-10-30.json`](http-a770-windows-2026-10-30.json).

Run `python tools/check_orca_http.py --config <config.json> --baseline-exe
<sycl-strata> --candidate-exe <native-strata> --images <native-images> --out
<new-directory> --spec 8 --long-prefill` to repeat this check. It uses an
ephemeral loopback port, the shared GPU lease and the actual HTTP handlers;
it does not test the server CLI bootstrap or installer. The config controls
conversation caching; these checks used `--prompt-cache 0` and `2` separately.

A separate check launched the actual `sycl/serve/server_intel.py` CLI with
isolated configs produced by the native installer conversion. Six requests
across both backends passed: short OpenAI JSON/SSE and a 2077-token code
input, at capacity 4. API content, usage, finish and draft counts matched.
This CLI check does not capture raw token IDs. The SYCL control enabled XMX
for HC injection and GDN A/B as well as the scalar shared gate; omitting the
first two controls had produced different long-input text and draft counts.
That preceding failed comparison is retained. The native config used automatic
host mirroring and the compiler math DLL directory; module audits excluded
SYCL, UR and oneMKL. The model unloaded through HTTP before each server was
stopped. Twenty CPU-only installer checks cover config conversion, existing
SYCL behavior, invalid native inputs and mocked config/run-script writing;
the real native installer `--check --model IQ2_XS` also passed on this host
without changing production configs. No model download or production install
was performed. Evidence and limits:
[`cli-a770-windows-2026-10-30.json`](cli-a770-windows-2026-10-30.json).

The automatic host-mirror budget now uses the existing cross-platform memory
query. Previously its Linux-only `/proc/meminfo` read silently gave a zero
budget on Windows unless `STRATA_MIRROR_MIB` was set. With that override absent,
the rebuilt native engine mirrored all 24576 experts (33.02 GiB), passed four
GPU suites, and matched 64-token Short and Code2K reference workloads at MTP
capacity 4. Kernel image hashes were unchanged. This is a startup-default fix,
not a fix for the intermittent capacity-8 failure; the earlier failing runs
already supplied an explicit mirror budget. Results and hashes are in
[`windows-auto-mirror-2026-10-30.json`](windows-auto-mirror-2026-10-30.json).

The normal `sycl/` build remains available as the comparison baseline. Linux is
the primary deployment target, but Linux compilation and GPU behavior have not
yet been verified.

The current adapter exposes one Intel GPU. CUDA VMM and the second-GPU expert
tier retain the SYCL port's explicit unsupported answers. GPU event profiling
is also explicitly unsupported. `ORCA_L0_KERNEL_RESOURCES=1` logs driver-reported
subgroup, static SLM, private and spill bytes once at kernel creation; leave it
off for timing runs. These fields do not report an exact GRF count or achieved
occupancy. SPSC endpoint cursors occupy separate 64-byte cache lines and cache
the opposite cursor, refreshing it only at an empty/full boundary.

For an end-to-end comparison, `tools/bench_orca_strata.py --exe <sycl-baseline>
--candidate-exe <native-strata> --config <config.json> --out <new-directory>`
uses identical inputs/history and rejects different token/acceptance workloads.
Add `--images <native-images>` to pin the native kernel files. The comparison
reports separate prefill, decode, first-token and full-request wall times,
rejects nonfinite timings and changed executables/images/config, and records
baseline drift. A zero-duration prefill is retained without a percentage.
The fixed text fixtures also reject all-zero output immediately, including
from the baseline. This is a benchmark-validity gate, not a NaN diagnosis.
It preserves the baseline runtime flags and removes SYCL/UR selector flags from
the native arm. Do not interpret operator batching speedups as model speedups.

The optional
`STRATA_SYCL_ORCA_L0` verifier bridge is documented separately in
[`orca-l0/demo/STRATA.md`](../../orca-l0/demo/STRATA.md).

## Prefill submission and min-p 0.5 pair check

`tools/bench_orca_prefill.py --self-check` checks phase attribution and timing
gates without a GPU. With a frozen native config and a new `--out`, the default
mode screens existing submission policies. There is no wait-to-fill timer;
the final expert drains even below batch8. Disabling batch8 added 4.91%/2.21%
to Code2K/Long8K TTFT in a one-sample-per-arm Windows A770 screen.

For a binary containing the FP16 pair kernels, add `--pair --repeats 2 --tokens 64`
and use a min-p0.5 config. This runs same-binary OFF/ON/OFF at fixed5771 slots,
adapt0/pcie0 and8192 context/prefill. The completed check reduced Code2K TTFT
33.001->28.230s (-14.46%) and Long8K84.178->76.296s (-9.36%). All12complete
outputs and draft counts matched; TTFT baseline drift was below0.08%.
Decode uses the engine's separate timer and excludes each case's first cycle
to remove first-use preparation; no decode gain was confirmed. Each case/arm
has two prefill samples and one warmed decode sample, without a confidence
interval. This extends the earlier0.8/automatic-cache evidence, without changing
the production profile or proving the original SYCL regression eliminated.
See the [report](../../bench/results/a770/orca-prefill-2026-10-31.md),
[policy screen](prefill-policy-a770-windows-2026-10-31.json) and
[pair results](prefill-pair-minp05-a770-windows-2026-10-31.json).

## Incremental input demo

`tools/demo_orca_incremental_prefill.py` compares waiting for all input with
processing each arriving prefix through the existing live-prefix cache. It
sends the same 8000 token IDs in two 4000-token blocks over a local socket;
the sender keeps running while the engine computes. Each intermediate `GEN 1`
prediction is discarded, and its computation is included in the measured cost.
The final request generates 64 tokens. This prototype requires no engine rebuild.

```powershell
python tools/demo_orca_incremental_prefill.py --self-check
python tools/demo_orca_incremental_prefill.py --config strata-iq2_xs.json --out build-orca-input-zero --chunk-tokens 4000 --interval-ms 0
python tools/demo_orca_incremental_prefill.py --config strata-iq2_xs.json --out build-orca-input-slow --chunk-tokens 4000 --interval-ms 30000 --baseline serial-chunks
```

Use new output directories. Each run takes the shared GPU lease, then runs
bulk / incremental / bulk in fresh processes with a Short warmup. It pins a
6000-slot expert budget, disables adaptation and PCIe sharing, and enables the
live-prefix cache in its child processes. The production config is unchanged.
The report checks each exact resume length, full final output equality, artifact
hashes and baseline drift. It records both first-block-to-first-token and
last-block-to-first-token latency, including intermediate work and input waits.
The default `--baseline bulk` compares the entire change against one full-prompt
request. `--baseline serial-chunks` waits for all input, then executes the same
prefix requests as the incremental arm. This second control isolates overlap
from changes in GEMM shapes and cache boundaries; it cannot establish a win
over one full-prompt request. Different outputs invalidate a performance claim.

On Windows A770, two fresh-process A/B/A experiments completed with the same
frozen native executable, MTP4 / spec-min-p 0.8, 6297 resident experts / 8641 MiB,
8000 input tokens and 64 final output tokens per arm:

| Baseline / simulated arrival | First block to first token, baseline → incremental | Latency change | Final output check |
| --- | ---: | ---: | --- |
| One full-prompt request / 0 ms | 84.404 → 98.170 s | +16.31% | 63/64 match; comparison rejected |
| Same prefix requests after all input / 30000 ms | 128.321 → 98.196 s | -23.48% | 64/64 match; comparison passed |

The slow-input control also reduced last-block-send-to-first-token latency from
98.310 to 68.194 seconds (-30.63%), and full elapsed time from 131.187 to
101.045 seconds (-22.98%). Accepted/offered drafts were 27/28 in all three arms.
The trace places 29.998 seconds of prefix work before the last block finished
sending. Maximum baseline drift was 2.91%, within the predeclared 3% gate; this
is one A/B/A per experiment, without a confidence interval.

The zero-interval comparison changed output at position 63 (one-based), with
the first 62 tokens identical. Both bulk baselines agree, and the same-chunks
control reproduces the chunked output. The numerical/state difference between
chunked and full-prompt execution remains unresolved. These results establish
overlap for the same chunked workload; they do not establish an equivalent-output
improvement over one full-prompt request, an optimal chunk size, or a production
network speedup. No Linux GPU run was performed.

Exact inputs, outputs, timings, frozen scripts, protocol/stderr and hashes,
including the failed comparison, are in
[`incremental-prefill-a770-windows-2026-10-31.json`](incremental-prefill-a770-windows-2026-10-31.json).

Arrival intervals are simulated. This hides input wait time when GPU work can
overlap it; it does not reduce the matrix work. With immediately available input,
smaller GEMMs and extra requests can cost more. The demo sends token IDs, not
incremental text, and does not change the HTTP API. A text-stream implementation
must retain the unstable tokenization tail and finalize the chat template before
answering. Many small chunks also resend growing prefixes; a future prefill-only
delta command would remove that traffic and the discarded predictions.
