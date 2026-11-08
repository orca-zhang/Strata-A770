# Orca native backend: what has actually been replaced

Status on 2026-11-04. Measurements below identify their frozen profiles.

**Orca L0 is the recommended path. Orca Direct remains WIP and an alternative.**
The matched Short brackets measure L0 at **59.38–59.57 tok/s**, ahead of Direct.

The [Windows call-path diagram](../README.md#runtime-call-paths) is retained
separately from performance plots: SYCL uses UR and Level Zero/Intel UMD;
Orca L0 uses Level Zero/Intel UMD directly; Orca Direct encodes its own GPU
commands. All three continue through D3DKMT and the Windows kernel driver.
The default remains Orca L0; Direct is WIP. Offline kernel compilation is
separate from these execution paths.

The latest [current-frontend context comparison](../bench/results/a770/orca-direct-current-contexts-2026-11-04.md)
keeps the same fixed anchor and mathematical pack in an L0/Direct/L0 bracket.
Direct decode is **1.83%/1.51%/2.21% below L0** for Short/2K/8K; 2K/8K
prefill takes **6.62%/7.75% longer**. All 69 clean requests match tokens/drafts.
Independent diagnostics record roughly **17,000–19,000 synchronous parameter
uploads per 8K prompt**; overlapping probe times are not an end-to-end savings
estimate. A shortened-history control also reproduces an **unresolved Direct
8K correctness failure**, while L0 matches the reference. No performance
candidate is adopted. The report retains both failures and successful runs,
plus bitmap candidates; bitmap throughput gains have not been measured.
This MTP-only protocol differs from the earlier Short audit and does not
establish daily-default or Linux GPU performance.

The preceding [idle-machine Short audit](../bench/results/a770/orca-direct-idle-audit-2026-11-04.md)
repeats the matched current frontend in both orders. Direct/L0 gives
**58.3180/59.3843** and **58.3002/59.5705 tok/s**, leaving Direct
**1.7956%/2.1324% below L0**; flank drifts are -0.1405%/-0.3675%.
Independent hot captures find equal **97 submissions, 134 D2D copies and
4,554,752 D2D bytes**, with zero explicit H2D/D2H or adaptive refill. Observed
EXE-import mutex calls are **112 Direct / 1,048 L0**, excluding private,
inlined and driver locks. Active observer polling improves Direct over its own
event wait by **1.8328%/0.9430%** in opposed brackets, but the subsequent
L0/polling/L0 bracket still gives **58.5787/59.8449 Direct/L0 tok/s (-2.1158%)**,
with +0.0079% L0 drift. All **225 clean plus 44 diagnostic** requests match
tokens/drafts; polling passes 14 CTests, six native numerical groups and three
current frontend GPU checks. It remains isolated; defaults and deployed EXEs
are unchanged. These fixed-anchor results establish no native-over-L0 win,
daily-default throughput, PPL or Linux GPU performance.
[Exact results, captures and source](../sycl/orca/direct-idle-audit-a770-windows-2026-11-04.json).

The preceding [whole-frontend rebuild](../bench/results/a770/orca-direct-current-frontend-2026-11-04.md)
compiles both transports from one source snapshot: 48 matching ABIs and 1,269
byte-identical numerical SPIR-V files, then one common freshly baked collection.
It exposes a source-policy mismatch: Direct still used cache margin 0.5 while L0
used 1.5; previous matched experiments had patched frozen main objects only.
The one-line source fix now applies the existing Orca 1.5 margin to both.
Matched forward/reverse Short trials give **56.4972/58.9617** and
**58.7963/59.7296 Direct/L0 tok/s**, respectively **4.1798%/1.5625% below L0**.
Flank drifts are -1.2796%/-0.4015%; the slower forward trial also has slower
prefill. Both rounds remain published. All 180 requests match exact outputs and
drafts; six current frontend GPU checks pass. This is the fixed 256/0.95/
cumulative-zero anchor, not daily-default throughput. That round did not collect
fresh D2D/lock counts, PPL, long-context or Linux GPU results; it established
no native-over-L0 win.
Production configs and deployed EXEs remain unchanged.
[Evidence](../sycl/orca/direct-current-frontend-a770-windows-2026-11-04.json).

The earlier [complete current-runtime rebuild](../bench/results/a770/orca-direct-current-runtime-2026-11-04.md)
corrects the scope of the older frozen-library comparisons below. With the same
Host objects, mathematical pack and matched expert policy, L0/current/old/current/L0
gives geometric **56.5308 current / 55.8782 old / 57.7552 L0 tok/s**: current
improves **1.1678%** over old and remains **2.1199% below L0**. Current/L0 flank
drifts are +0.1787%/-0.0456%. All 75 clean and 11 diagnostic outputs/drafts match.
The exact current source, including ISA padding/early slabs/scratch reuse and
residency rollback, passes 14 Windows CTests, six numerical groups, plus
device-local transfers, 48 GEMV and 144 XMX replays. An independent capture shows
instruction-window changes **268 to 0**, scratch-base changes **98 to 3**, and
SBA commands **926 to 200**, with unchanged walkers/submissions. This is the
older fixed-capacity 256/0.95/cumulative-zero anchor, not daily/automatic-cache
performance. Current runtime is rebuilt; the whole current Strata frontend and
math pack are not rebuilt in this comparison. No native-over-L0 win, fresh PPL,
long-context or Linux GPU result is established. Production defaults/deployed
EXEs are unchanged. [Exact evidence](../sycl/orca/direct-current-runtime-a770-windows-2026-11-04.json).

A [GRF-only follow-up on an older frozen runtime](../bench/results/a770/orca-direct-grf-combined-2026-11-04.md)
removes 83.78% of redundant CFE/LRI/pipeline packets after the existing heap
and invalidation fixes, but establishes no model throughput gain. Two clean
Short orderings change decode by **+0.3170% / -0.3181%** versus the native
control. In the reversed L0/candidate/control/candidate/L0 bracket, rates are
**57.6109 / 55.5861 / 55.7634 tok/s** for L0/candidate/control: the native
candidate remains 3.5146% behind L0. This is the older 256/0.95/cumulative-zero
fixed-capacity anchor with matching expert replacement margin 1.5, not the
daily profile. Its frozen runtime excludes the later adopted ISA padding,
early ISA slabs and queue scratch reuse, so it is not a measurement of the
complete current source. All 135 clean and 33 diagnostic outputs/drafts match; 12 GPU
CTest cases, corrected CPU validation and five numerical GPU groups pass.
All 40,119 ordered walker geometry tuples match the native control, while
7,176 state packets / 95,680 bytes disappear. The candidate remains isolated;
production source/defaults/deployed EXEs are unchanged. Raw buffers, the
initial failed test matcher and corrected checks are preserved in the
[report and archive](../sycl/orca/direct-grf-combined-a770-windows-2026-11-04.json).

A [daily-parameter follow-up](../bench/results/a770/orca-direct-daily-margin-2026-11-04.md)
finds that margin 1.5 hurts the 1024/0.5/cumulative0.5 configuration:
Direct **44.8841 to 41.0168 tok/s (-8.6162%)**, with +0.1123% control drift.
Matched-margin L0 is **42.0261 tok/s**, leaving Direct 2.4016% behind; L0 has
one unbracketed process. All 60 outputs/drafts match at fixed 5,771 slots.
Defaults stay unchanged; the older +10.29% result below is profile-specific.
The same follow-up adopts a WDDM allocation rollback fix and its standalone
check: only completed nonlocal-residency rejection is recoverable after all
backing is synchronously destroyed. Query/paging/cleanup failures stay terminal.
Before/after regression, current-source CPU checks, 920 GPU transfer replays and
three full-engine auto-cache Short smoke requests pass. The smoke starts at
6,106 slots but does not itself trigger a rejection/retry. Deployed EXEs and
production configuration are unchanged; rebuild to use the source fix.
[Raw evidence and exact adoption](../sycl/orca/direct-daily-margin-a770-windows-2026-11-04.json).

A separate [fixed-anchor cache/copy ablation](../bench/results/a770/orca-direct-copy-margin-2026-11-04.md)
finds that the older 256/0.95 anchor compiles expert replacement margin **1.5
for L0 versus 0.5 for DG2**. Changing only the Direct constant, with its
original EU runtime and math, raises **50.5202 to 55.7185 tok/s (+10.2896%)**;
control rate drift is -0.0465%. This is expert-cache hysteresis, not MTP min-p.
The subsequent matched-margin trial gives **54.8578 BCS /54.4805 EU /56.5774 L0**:
BCS adds 0.6926%, Direct remains 3.0395% below L0. Hot diagnostics reduce
adaptive refill **640 copies /843.5547 MiB to zero**, matching L0's zero refill
and 35165/36960 route hits. Verification/draft completion still takes longer.
All **292 complete outputs** match; 80 Host TUs, 1,632 fill checks and 12
cross-engine checks pass. Failed parser evidence is preserved and its 13
completed requests reused. No production source/default/deployed binary or Linux bundle
is changed. These anchor results must not be merged with the distinct later
1024/0.5 math-profile rates below. The [raw report and archive](../sycl/orca/direct-copy-margin-a770-windows-2026-11-04.json)
include isolated replication configurations and clean versus diagnostic data.

The **shared three/four-entry IQ GU body is now integrated as an opt-in** in
the numerical source. Its direct comparison against the original 4/2/1 GU
scheme measures **+0.041% native / +0.127% L0 decode**, close to control drift;
defaults stay unchanged. See **Minimal shared GU source integration** below
and its [source and validation manifest](../orca-l0/demo/iq-compact-source-windows-2026-11-04.json).

An isolated **IQ paired-GU format split** reduces each selected kernel's code
size, with 384 old/new canonical comparisons and 180 exact Shorts passing.
It establishes no stable speed gain: native/L0 candidate decode changes from
**-0.069% forward to +0.676% reverse**, while the forward native ablation is
**-0.565%**. The experiment stays isolated. See **IQ paired-GU format split**
below and its [report](../orca-l0/demo/dg2-iq-pair-format-windows-2026-11-04.json).

A follow-up **Q8-up capacity1/4** experiment passes 64 all-T byte checks and
180 exact Shorts, but establishes no stable speed gain. Its incremental native
decode change is **-0.340%** versus the preceding eight-row candidate; identical
new math gives native **+0.323%** versus L0 forward and **+0.010%** in reverse.
The reverse result is below **+0.251%** L0 control drift. The candidate stays
isolated. See **Q8-up capacity1/4 follow-up** below and its
[report](../orca-l0/demo/dg2-hc-up-capacity-windows-2026-11-04.json).

An isolated **Q8-up argument capture** candidate reduces compiler private
memory **42.038%** and passes 64 all-T output checks plus 180 exact Shorts.
With identical candidate math on both backends, native observes **+2.261%**
decode versus L0 in forward order and **+0.405%** in the reverse bracket.
The latter is small; TTFT changes direction. This is evidence of a small
fixed-Short advantage, not a stable 2% gain or a new production default.
See **Narrow Q8-up capture** below and its
[report](../orca-l0/demo/dg2-hc-up-capture-windows-2026-11-04.json).

The isolated Short copy-role trial wires the explicit `copy_engine` API, including
adaptive expert refill, and adds raw BCS fills. It rebuilds all **80 Host
translation units** with consistent adapter headers and uses no fixed queue
ordinal. Opposite process orderings observe **+2.172% / +1.064% decode versus
native EU copies**, and **+1.505% / +0.244% versus matched L0**. All **256 complete
outputs** and **1,632 fill checks** pass. Actual commands replace 640 EU copy
kernels with raw BLT; logical payloads match in all eleven diagnostic pairs.
Uncaptured EU/BCS requests record **130/124 imported mutex acquisitions**, with
zero SRW acquisitions; background I/O timing affects these totals. L0 control
drift is -2.918% / -0.717%, and TTFT changes direction, so a stable advantage
over L0 remains unproved. This frozen Windows experiment does not adopt sources
or defaults and adds no Linux GPU claim. See **Explicit copy-role API and
native fills** below and the [reproducible report](../orca-l0/demo/dg2-copy-api-windows-2026-11-03.json).

A newer isolated trial removes **640 intermediate BLT flushes**, but decode
versus previous BCS changes **-0.100% / -0.029%** in opposite orderings. No
decode or TTFT benefit is established, so it remains unadopted. All **250 exact
Short outputs**, 1,632 fill checks and 48 cross-engine checks pass. See
**One flush per logical copy: no measured model gain** below and its
[reproducible report](../orca-l0/demo/dg2-blt-flush-windows-2026-11-03.json).

A calibrated native CS/BCS timeline now records all submitted and completed
lists. In its final Short request, **99.646 of 104.867 ms of BCS spans intersect
CS spans (95.022%)**; **5.221 ms** lie outside captured CS, and **29.303 ms**
lie outside both. These are diagnostic execution envelopes, including waits
and context scheduling, not engine utilization or clean throughput. See
**Calibrated compute/copy timeline** below and its
[report](../orca-l0/demo/dg2-bcs-timeline-windows-2026-11-03.json).

A subsequent module-family diagnostic attributes **77.022% of summed CS
envelopes** to IQ kernels, fused GR and native MMVQ. All 11 Short outputs,
116 CS lists and 40,138 kernel calls pass the final-request checks. Its
27,636 added timestamp markers perturb execution; these are coarse diagnostic
envelopes, not clean rates or pure kernel costs. See **Module-family blocks**
below and its [report](../orca-l0/demo/dg2-kernel-blocks-windows-2026-11-03.json).

The shared/routed matching-stage schedule and router/input-quantization overlap
are now integrated into the engine source for both Orca L0 and direct DG2.
They default on for audited single-group windows of at most four rows, with
serial fallback and two disable switches. The integration passes 1,728 full-buffer
checks and 160 exact Short requests. L0 gains **1.129%** in its off/on bracket;
native versus matched L0 is **+0.052% / -0.756%** in opposite orderings, with
**+1.946%** native control drift in reverse. This does not establish a native
backend win. Production executables/configuration and Linux handoffs are preserved.
See **Source integration and default acceptance** below for scope and evidence.

Latest adopted runtime baseline: **v14 queue scratch reuse**. Concurrent
local-ID payload caching in the live worktree is outside the frozen comparisons
below. The exact-v14 adoption verifier no longer describes those additional edits.
Already
allocated queue scratch pools are retained for no-scratch kernels; private
memory stays demand-bound. A separate warmed Short capture reduces within-list
scratch-base changes **98 to 3**, removing **95 full state preambles / 190 SBA
commands**. Clean opposite orderings observe **+0.364% / +0.701% decode** versus
v13 controls, near +0.639% / -0.707% respective control drift. First matched-L0
drift is -4.204%, and native remains **0.967% below its geometric L0 control**;
no stable native or TTFT win is established. All 160 performance and 11 separate
diagnostic outputs match. Windows checks and Linux build/three CPU checks pass;
Linux GPU remains deferred. See **Queue scratch reuse removes 95 more state
preambles** below for complete samples and scope.

V14 retains **v13 early Windows ISA slabs**. Reserving
bootstrap and mathematical code before tensors removes **268 instruction-window
changes and full state preambles** per warmed Short request. SBA commands fall
**1,004 to 468**, while walkers and submissions stay fixed. Clean decode point
estimates improve **+0.473% / +0.224%** in opposite orderings, with +0.204% /
+0.260% respective control drift. The first candidate remains **0.637% below
matched L0**; no stable backend or TTFT win is established. Windows local ISA
reserves 5 MiB upfront, with existing growth for larger programs. Linux keeps
its prior behavior. See **Early code slabs remove 268 state preambles** below.

V13 retains **v12 Windows DG2 ISA padding**. Reducing
each loaded kernel's zeroed tail from 4,096 to 512 bytes observes **+1.267% /
+0.607% Short decode** versus native controls in opposite orderings. The first
bracket is **49.196 native / 49.536 matched L0 tok/s (-0.686%)**; a native win
is still unproven. The 109 loaded kernels save 390,656 captured bytes, while
retained code allocations remain 5 MiB. Linux keeps 4,096-byte padding pending
GPU validation. See **Compact DG2 ISA padding** below for checks and evidence.

The retained **v11 batched local-parameter mirrors** are enabled
for the calibrated Windows A770 C0 backend. Two opposite clean Short brackets
observe **+0.982% / +0.984%** decode versus matched host-parameter builds. A
separate unchanged-V10 bracket observes **+1.350%**, with -0.697% V10 control
drift. The latest reverse bracket gives **47.020 native / 47.702 L0 tok/s**,
so native remains **1.43% below L0**. The exact four validated
files are adopted; production executables/configuration are preserved. Linux
keeps host-backed parameters and awaits GPU testing.
The later unchanged-source controls in the tail-barrier trial below give
**47.522 native / 48.357 L0 tok/s (-1.73%)**. That candidate is not adopted;
the adopted V11 runtime without the experimental engine schedules has not
shown a full-model win over L0.

The earlier isolated matching-stage expert trial observes **+1.871% decode
versus native serial**, reaching **48.576 native / 48.626 L0 tok/s** in its
clean first bracket. A reversed bracket points in the same direction but has
3.13%-4.71% control drift. That isolated schedule was experimental; its results
and limits are recorded below, followed by the current source integration.

Adding router/input-quantization overlap to that schedule now gives **two
Short brackets ahead of the existing L0 baseline**: **47.870 / 47.263 tok/s
(+1.283%)** and **47.735 / 46.616 tok/s (+2.400%)**, native / L0 respectively.
All 160 model requests and 1,280 kernel-window full-buffer checks pass. The
historical candidate was experimental. Its L0 baseline lacks the two schedules,
so this compares complete implementations, not an isolated API-layer
advantage. Details, sample dispersion and reproducible evidence follow below.

V11 retains the **v10 isolated co-aligned copy path**. Its
optional kernel peels at most 15 bytes, then copies aligned vectors. Selection
happens at command recording; module support is cached at binding. The original
aligned-copy execution body is preserved. Older four-export memory modules and
unequal byte alignments keep their existing paths.

A fresh serial Short census runs both native and L0: all **11 paired requests**
have equal submitted logical payloads. Each final warmed request records
**134 D2D operations / 4,554,752 bytes** and **659 H2D operations / 968,318,976
bytes**. Separate verified imported-lock probes of both backends measure
**130 application mutex acquisitions each**: 40 foreground I/O plus 90
background keepalive I/O, with **zero runtime mutex/SRW acquisitions**.
These are submitted logical copies and observable application imports, not
physical bus transactions or private driver locks.
[Current-source separate copy and lock probes](../orca-l0/demo/dg2-local-parameters-current-repeat-windows-2026-11-03.json).

For 16 distinct D2D outputs with both pointers offset by one byte, GPU time
falls **67.46% / 56.62% / 57.00%** at 96 KiB / 1 MiB / 4,194,321 bytes versus
the old native path. The aligned path retains its speed; aligned 96-KiB D2D
measures **26.445 us native / 33.339 us L0 (-20.68%)** in this bracket. The
unaligned candidate still takes **4.40%-11.39% longer than L0** in these cases.
These standalone copy results are not model throughput gains.

The historical v10 100-request Short bracket gives **46.759 / 46.659 / 48.171 tok/s**
for native control / v10 / L0: **-0.21%** versus native, with **+0.12%** native
control drift. Native remains **3.14% below L0**; no full-model win is
established. All **133 clean/diagnostic requests** match the complete 76-token
output, natural stop and 57/57 drafts. Windows seven checks, Linux three CPU
checks and 1,840 candidate/legacy-module transfer replays pass. Linux GPU is
untested. Source is adopted; production executables/configuration and immutable
Linux handoffs are preserved.
[Copy samples and ISA audit](../orca-l0/demo/dg2-copy-throughput-windows-2026-11-03.json),
[clean Short and fresh counters](../orca-l0/demo/dg2-copy-prefix-short-windows-2026-11-03.json),
[v10 source adoption](../orca-l0/demo/dg2-copy-prefix-main-windows-2026-11-03.json).
Read-only verification of frozen v14 evidence:
`python build-orca-native-lower-20261031/publish_dg2_scratch_retain.py --verify`.
Exact live-source v10/v11/v12/v13/v14 verifiers are historical after later edits; their
immutable reports and frozen sources are preserved.

The additional parity run completes **22 exact Short requests**, reusing the
verified V10 native and frozen L0 diagnostic executables. Both use the same
1,242 mathematical zebins, 5,771 expert slots, 1,536-MiB reserve, MTP4,
step/last min-p 0.5, cumulative floor zero and no prefix reuse. Every request
matches all 76 tokens, natural stop and 57/57 drafts. Copies count on every
graph replay, within the engine's decode boundaries. H2D/D2D/D2H/H2H/shared
payloads and fill counts match for every corresponding request.

This run adds copy evidence; its counter mutexes make it unsuitable for lock
or speed claims. The 130/130 lock result comes from the separate V10 observer,
which attributes every call to PLE/DirectFile and excludes its own guards.
Background keepalive remains enabled, so a different request duration can
change total I/O acquisitions without changing runtime locking. Production
sources, binaries, configuration and Linux bundles are unchanged by this run.
Runner and raw protocols/logs（本地归档：`../orca-l0/demo/dg2-copy-lock-parity-windows-2026-11-03.json.gz`）
are archived. Historical v10 live-source check:
`python build-orca-native-lower-20261031/check_dg2_copy_lock_parity.py --verify`.

### Operator parallelism: dependency audit and isolated stage trial

The current source already exposes independent GDN QKV / alpha-beta / Z
projections, QSA's five input projections, and shared-expert Gate / Up / scalar
projections. Eligibility is limited to audited primitive kernel paths. These
phases remove ordering between independent launches and join their outputs;
they do not prove simultaneous hardware execution. The historical native
ordered/relaxed Short trial below measured **+1.74%**, and that behavior is
already included in V11 rather than an additional projected gain.
The native encoder still emits a stalling flush before changes to global
dispatch state, such as GRF mode or heap bases. Removing a queue dependency
does not remove these required state-transition barriers.

| Work | Current dependency / remaining opportunity |
| --- | --- |
| Shared expert versus routed experts | Both read the FFN input and join at combine, using separate arena slices. The current source matches their Gate/Up, activation and Down stages for audited single-group windows of at most four rows; other cases remain serial. |
| Router / device plan versus shared expert | Router output is needed by the routed branch, not the shared branch. Preserve early route publication on host-served paths while considering overlap. |
| VRAM experts versus PCIe experts | Both grouped calls reuse `hit_scratch_`; simultaneous pipelines need separate scratch and disjoint output writes. An extra queue alone is insufficient. |
| Expert weight reads / transfers versus compute | Useful where those transfers exist and engines can overlap. The fixed Short profile below bypasses the host-served per-layer wait/copy branch. |
| Layers / successive decode tokens | The next layer consumes the preceding residual; autoregressive tokens and recurrent/KV state add real dependencies. Existing speculative token batching does not remove these dependencies. |

The frozen Short configuration enables `STRATA_VERIFY_NO_HOST=1`,
`STRATA_VERIFY_ADAPT=1` and device planning. This selects `collect_routing_`,
which bypasses plan-A waiting and the post-VRAM PCIe/CPU-result branch in
`Verifier::record_window`. Experts may still be read from pinned host mirrors;
the earlier explicit H2D census also includes work outside these branches.
Do not attribute the remaining Short gap to waiting for host routing or claim
that all expert weights are resident in VRAM.

`Queue::independent()` requires **every enclosed operation** to be independent,
including kernels hidden inside a typed operator. A complete shared expert or
grouped expert contains Gate/Up, SwiGLU/quantization and Down dependencies, so
wrapping that entire pipeline would break its contract. Shared expert reuses
`xq_` between its input and Down activation, and later attention/head work also
uses this arena slice. Cross-layer overlap must account for that lifetime.

The stage trial below uses one recorded list, preserving internal chains,
scratch ownership and the combine join. A separate-queue fork needs more work:
captured inline event tags currently reset immediately before each
segment is submitted. A fork spanning separate graph replays must rearm all
reused signals before submitting either branch; otherwise a consumer could
observe the preceding replay's completed tag. Graph recording queues also own
their scratch/private allocations. Simply replaying the same list on another
queue is not a valid fork implementation.
Compare exact outputs and drafts first, then uninstrumented warmed Short
with the same parameters and adjacent controls. More queues can also compete
for memory bandwidth; no new throughput percentage is established by this audit.
Sources: [verifier graph and arena](../sycl/src/core/verify.cpp),
[shared expert](../sycl/src/kernels/cuda/shared_expert.dp.cpp),
[grouped experts](../sycl/src/kernels/cuda/iq_kernels.dp.cpp) and
[queue dependency contract](../sycl/orca_queue.hpp), with native state changes
in [dispatch encoding](../orca-l0/src/drm/encoding.hpp).

### Shared/routed stage interleaving: numerical parity, no established gain

The isolated candidate exposes explicit phases in the two expert launchers.
Shared SwiGLU/quantization plus sigmoid can run independently of routed
Gate/Up; shared Down can run independently of routed SwiGLU/quantization;
shared scaling can run independently of routed Down. Each interval joins
before the next stage. All driver state-transition barriers remain.

Eligibility requires native relaxed recording, one token group, device routing,
cached weight descriptors, the native BF16 scalar gate and audited formats.
**43/48 layers** meet the static format gate. Other formats, grouped-v1 and
caller-prequantized windows above four tokens keep their original order.
The candidate is isolated; V11 remains the adopted source.

Both batch settings pass **576 full-buffer/guard bit comparisons** with real
shared weights, two routed formats, T=1/4/8, changing input and routing plans,
empty plans, repeated graph replay and invalid-phase rejection. All 80 Host
objects were rebuilt against consistent headers; a missing declared Host getter
was supplied without recompiling mathematical device code. The 8 shared and
252 grouped native exports are retained. Eight ISA hashes change only in two
`mov null` immediates after the final EOT; the preceding computation and
control instructions and resource metadata are unchanged.

The clean L0 / serial / interleaved / serial / L0 Short bracket contains
**100 exact 76-token requests**, including ten warmups per process. All stop
naturally with 57/57 drafts and no prefix reuse. Both native modes use the same
candidate executable and compiled packs. Parameters stay at 5,771 slots,
1,536 MiB reserve, MTP4, step/last min-p 0.5 and cumulative zero.

| Adjacent controls / candidate | Decode tok/s | TTFT ms |
| --- | ---: | ---: |
| Native serial | 47.526 | 894.353 |
| Native interleaved | 47.766 | 892.212 |
| L0 | 48.423 | 886.939 |

The observed decode change is **+0.505%**, with **+0.741% native control
drift** and +0.719% L0 drift. This does not establish a speedup. Interleaved
native remains **1.36% below L0**; TTFT changes -0.239% versus native serial.
Decode excludes preparation, initialization and prefill. No inner timers, GPU
timestamps or telemetry run in the comparison. Linux GPU execution remains
untested; exposing independent intervals does not prove simultaneous execution.

[Result and fixed parameters](../orca-l0/demo/dg2-expert-overlap-windows-2026-11-03.json)
and 478 checked source, assembly, build and raw protocol records（本地归档：`../orca-l0/demo/dg2-expert-overlap-windows-2026-11-03.json.gz`）
are independently verified by
`python build-orca-native-lower-20261031/publish_dg2_expert_overlap.py --verify`.

### Matching shared/routed stages: isolated schedule-only improvement

The next candidate moves both branches' projections into one independent
interval, then pairs their activation/quantization stages and their Down
stages. Shared scaling and the existing combine retain their dependencies.
Its native flag is `STRATA_VERIFY_EXPERT_FORK=1` together with
`STRATA_VERIFY_EXPERT_OVERLAP=1`; both are cached outside replay. The new mode
requires one token group, audited formats, device routing and T<=4. Other
windows keep the earlier stage or serial fallback. The format gate remains
43/48 layers; this is an isolated experiment, not an adopted V11 default.

The candidate passes **864 full-buffer/guard comparisons**, split equally
between both batching settings, with real shared weights, routed GU22/42,
Down42, T=1/4/8, changing/empty plans, changed inputs and repeated graph replay.
Only the shared and verifier Host objects are rebuilt; the remaining 78 and
consistent headers come from the preceding trial. All **8 shared / 252 grouped
exports** retain byte-identical ISA and capture/resource metadata relative to
that trial. No mathematical-kernel change explains the measured difference.

One executable and pack supply all three native schedules. The first clean
bracket is L0 / serial / interleaved / matching / interleaved / serial / L0,
with 20 requests per process, ten warmups and ten retained requests.

| First bracket | Decode tok/s | TTFT ms |
| --- | ---: | ---: |
| Native serial, geometric adjacent controls | 47.684 | 892.073 |
| Native interleaved, geometric adjacent controls | 48.157 | 887.832 |
| Native matching stages | 48.576 | 885.135 |
| L0, geometric adjacent controls | 48.626 | 881.291 |

Matching stages improve decode **1.871% versus native serial** and **0.871%
versus interleaving**. Native serial drift is +0.141%, interleaved drift
+0.111%, and L0 drift -0.038%. The candidate is **0.102% below L0** in this
bracket; it does not establish a native win. Decode excludes preparation,
initialization and prefill; TTFT is separate. No inner timers or GPU telemetry
run in the model comparison. Hardware concurrency is not proved, and required
DG2 state-transition barriers remain. Linux GPU execution is untested.

A second bracket reverses the candidate/control positions:
matching / L0 / serial / L0 / matching. The unchanged executable and packs
complete another 100 requests without rebuilding.

| Reverse bracket | Decode tok/s | TTFT ms |
| --- | ---: | ---: |
| Native serial, middle arm | 46.799 | 911.042 |
| Native matching stages, geometric adjacent controls | 47.936 | 899.538 |
| L0, geometric adjacent controls | 47.296 | 907.079 |

The second decode point estimate is **+2.429% versus serial / +1.354% versus
L0**, but matching controls drift **-3.128%** and L0 controls **-4.711%**.
All retained samples, including the slower middle/late samples, remain in the
report. These are geometric controls, not confidence intervals. The opposite
L0 comparison signs and larger reverse-run drift do not establish a repeatable
native win. The positive serial comparisons warrant retaining the candidate
for further controlled testing; **it is not adopted**.

After the run, the user reported using other software as a possible cause of
the reverse-run fluctuations. Treat that bracket as potentially affected by
other applications. Resource contention was not measured, so its exact cause
remains unconfirmed. The complete raw evidence is retained unchanged; no
additional GPU benchmark is started while the desktop is in use.

Across both brackets, **all 240 requests** match the complete 76-token output,
natural stop, 57/57 drafts and zero prefix reuse. Parameters remain 5,771
expert slots, 1,536-MiB reserve, MTP4, step/last min-p 0.5 and cumulative zero.
Preparation/init/prefill stay outside decode. No new copy/lock census is
claimed; the V11 source verifier still passes all 12 live source hashes.
An archive-index assertion and a missing summary import were corrected using
the completed artifacts/raw requests; no GPU work was repeated for those fixes.

[Both brackets and exact parameters](../orca-l0/demo/dg2-expert-fork-windows-2026-11-03.json)
and 79 source, build, audit and raw protocol records（本地归档：`../orca-l0/demo/dg2-expert-fork-windows-2026-11-03.json.gz`）
are independently verified by
`python build-orca-native-lower-20261031/publish_dg2_expert_fork.py --verify`.
Unchanged headers, 78 Host objects and grouped device code reference the
preceding immutable interleaving trial; the new evidence retains its hashes.

### Router/quantization overlap: two Short brackets ahead of the L0 baseline

The next candidate overlaps the fast batched router
GEMV with the existing fused Q8/BF16 input quantizer and, when required, the
existing shared-input Q8 quantizer. They read the finished FFN input and write
separate arena slices. A join precedes router Top10; shared Down's later reuse
of `xq_` remains ordered. Completed early quantization is skipped at its former
location, preserving quantizer selection and invocation counts by construction.
The cached opt-in flag is `STRATA_VERIFY_ROUTER_QUANT=1`; it additionally
requires matching-stage eligibility and the existing batched router path
(T=2..4, 256/512 experts, K=10). All other cases keep the previous schedule.

The reversible source-delta and scratch/dependency CPU checks pass. One Host
object is rebuilt and the candidate links against the preceding unchanged
kernel pack; no device math or module compilation changes. After the user
confirmed an idle desktop, the shared experiment lease waited for an existing
test to complete, then covered compilation, numerical checks and model tests.

The GPU checker uses the **exact candidate scheduling fragment**. Four runs
cross the new flag and batching off/on, covering N=2560, NE=256/512,
T=1/2/3/4/8, qdedup off/on and eligible/ineligible paths. All **1,280 full-buffer
comparisons and 1,920 GPU tail epochs** pass, with changed/zero inputs, repeated
graph replay, full input/output/scratch/weight guards, router normalization,
distinct IDs and zero-logit ties. A scalar GEMV oracle checks three expert
columns per token; maximum absolute error is **2.235e-8**. Q8/BF16 payloads use
full-buffer cross-schedule equality, not an independent CPU quantization oracle.

The first bracket is L0 / matching / matching+router-quant / matching / L0.
The confirmation reverses positions: matching+router-quant / L0 /
matching+router-quant. Each process runs ten warmups and ten retained requests.
The same candidate binary and pack control both native modes.

| Bracket | Optimized native tok/s | Existing L0 tok/s | Native versus L0 | Native TTFT ms | L0 TTFT ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Five arms, 100 requests | 47.870 | 47.263 | +1.283% | 904.499 | 907.158 |
| Reverse, 60 requests | 47.735 | 46.616 | +2.400% | 906.734 | 917.253 |

The isolated router/quantization increment is **+0.842%** over matching-only
native's **47.470 tok/s** in the first bracket. Its native controls drift
-0.265%, and L0 controls +0.326%. Reverse native controls drift +0.143%.
Geometric adjacent controls are used where present; no slow retained samples
are removed. The candidate's first-arm retained range is **47.205–48.217
tok/s**, with median absolute deviation **0.286 tok/s**. Every arm's quartiles,
range and deviation are retained in the report. These descriptive values and
control drifts are not confidence intervals or a fixed gain guarantee.

All **160 requests** match the complete 76-token output, natural stop,
57/57 drafts and zero reuse. Parameters remain 5,771 slots, 1,536-MiB reserve,
MTP4, step/last min-p 0.5 and cumulative zero. The expert profile and frozen
configuration hashes remain unchanged through each bracket. Decode excludes
initialization/preparation/prefill; TTFT is separate. No inner timers or GPU
telemetry run in the model tests, and no prefill speedup is established.

The existing L0 executable lacks both experimental schedules. The measured
result is therefore **optimized native DG2 versus the existing L0 baseline**;
the matched-schedule follow-up below separates these scheduling gains from
the remaining backend difference.
**The candidate is not adopted.** Current V11 source, production binaries,
configuration and immutable Linux handoffs remain intact; Linux GPU execution
and physical concurrent execution remain unverified. No new copy/lock census
is claimed. The current V11 verifier still passes all 12 live source hashes.

The original CPU-preparation snapshot stays unchanged; subsequent numerical
and model records are in separate subdirectories of
`build-orca-native-lower-20261031/windows-dg2-router-quant-v1`.
[Both brackets, dispersion and exact parameters](../orca-l0/demo/dg2-router-quant-windows-2026-11-03.json)
and 66 source/build/check/raw-protocol records（本地归档：`../orca-l0/demo/dg2-router-quant-windows-2026-11-03.json.gz`）
pass the independent read-only check
`python build-orca-native-lower-20261031/publish_dg2_router_quant.py --verify`.

### Matched schedules: L0 gains 2.65%; native DG2 remains behind

The L0 control now uses the exact expert matching-stage and router/quantization
scheduling sources from the tested native candidate. All **80 Host objects**
are rebuilt with consistent L0 layouts and phase headers. Both backends use
the same untouched native module pack; no device code is recompiled. The L0
runtime is built from its single translation unit and linked to SDK 1.33.1.
Hardware is Windows, A770 C0, driver **32.0.101.9034**; clocks are not locked.
The shared GPU lease excludes other experiments throughout validation and
both model brackets. Earlier lease waits are outside every measured request.

The L0 checkers pass **2,144 full-buffer comparisons and 2,952 GPU tail
epochs**, covering expert phases, router/quantization, changed inputs, guards
and graph replay with batching off/on. The original DG2 dependency guard
rejected `ze_loader.dll` before Runtime construction. Its rejection is retained;
only that guard is adjusted for L0, still forbidding SYCL/UR. Numerical check
logic, engine objects and device modules stay unchanged.

Each arm runs ten warmups and ten retained exact Short requests. The first
bracket is L0 serial / L0 matched / native matched / L0 matched / L0 serial;
the reverse is native matched / L0 matched / native matched. Adjacent controls
use their geometric mean. Every retained sample is kept.

| Bracket | L0 original schedule tok/s | L0 matched schedule tok/s | Native matched schedule tok/s | Native versus matched L0 |
| --- | ---: | ---: | ---: | ---: |
| Five arms, 100 requests | 47.212 | 48.466 | 47.643 | **-1.697%** |
| Reverse, 60 requests | Not measured | 48.323 | 47.636 | **-1.423%** |

The same L0 binary gains **2.655%** from the new schedules in the first bracket.
Native is **0.912% ahead of the original-schedule L0**, but remains behind
L0 with equal scheduling. L0 original/matched control drift is -0.304%/-0.385%;
reverse native drift is +0.326%. Thus the earlier +1.283%/+2.400% whole-version
observations do **not** establish an advantage from bypassing Level Zero.
These matched tests also compare complete backend implementations, rather
than isolating the cost of one API call. Packet programming, cache policy and
memory placement remain possible sources of the difference, not proven causes.

Matched L0/native TTFT is **899.912/909.387 ms** in the first bracket and
**904.603/909.848 ms** in reverse. All **160 requests** match every output token,
natural stop, 57/57 drafts and zero reuse. Parameters remain 5,771 expert slots,
1,536-MiB reserve, MTP4, step/last min-p 0.5 and cumulative zero. Decode
excludes preparation/init/prefill; TTFT is separate. No inner timers or GPU
telemetry run during these model brackets. Sample dispersion is archived;
it is not a confidence interval or a fixed-gain guarantee. No prefill gain or
new transfer/lock census is claimed. Linux GPU execution remains untested.

This historical control did not adopt the schedules; source integration follows
below. It did not change production configuration/binaries or the immutable Linux handoffs.
[Results, fixed parameters and sample dispersion](../orca-l0/demo/l0-matched-schedule-windows-2026-11-03.json)
and 645 source/build/check/raw-protocol records（本地归档：`../orca-l0/demo/l0-matched-schedule-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/publish_l0_matched_schedule.py --verify`.
The archive retains the exact frozen sources, compiler commands, input/module
hashes and raw requests for reproduction under the same hardware/driver and
policy; fresh measurements need a new output directory. This verifier checks
the retained evidence and does not run another GPU benchmark.

### Source integration and default acceptance

The engine source now enables matching shared/routed Gate/Up, activation and
Down stages, plus router/input-quantization overlap, by default. Selection is
limited to audited primitive shared projections, native BF16 scalar gates,
native grouped experts, device routing with descriptor caching, one token group
and at most four rows, without profiling/tracing. Ineligible paths retain the
original schedule. `STRATA_VERIFY_EXPERT_FORK=0` disables the matching stages;
`STRATA_VERIFY_ROUTER_QUANT=0` separately disables early router quantization.
The shared Q8 scratch remains live until its projections consume it.

Both public function signatures retain their CUDA/HIP and ordinary SYCL ABI;
only Orca builds gain phase parameters. All three ordinary SYCL translation
units compile, both public ABI checks pass, and a runnable
`strata-orca-expert-schedule-check` target covers real shared weights, changing
routed plans, inactive rows, scratch/input guards and graph replay. L0 and DG2,
with batching off/on, pass **1,728 full-buffer comparisons / 2,064 GPU tail epochs**.
The initial L0 link exposed a missing existing BF16 getter in the SYCL source;
its implementation is restored and the failed preflight is retained. No GPU
work ran before that repair.

For acceptance, only the three integrated scheduling sources are rebuilt per
backend. The other **77 Host objects**, model entry point, V11 runtime and
phase-compatible native device pack stay frozen. This preserves concurrent
MTP work and equal workload; it is not a full latest-engine benchmark.

| Short bracket | L0 original schedule | L0 default schedule | DG2 default schedule | DG2 versus matched L0 |
| --- | ---: | ---: | ---: | ---: |
| Five arms, 100 requests | 47.382 tok/s | 47.917 tok/s | 47.942 tok/s | +0.052% |
| Reverse, 60 requests | Not measured | 48.710 tok/s | 48.342 tok/s | -0.756% |

L0 gains **1.129%** in the first bracket, with original/default control drift
**+0.393% / -0.198%**. Reverse DG2 control drift is **+1.946%**. The small backend
differences do not establish a stable native advantage, even with an idle
desktop. Hardware clocks are unlocked; their contribution is not measured.
The earlier 2.655% scheduling result and this 1.129% result are separate
observations, not additive gains or a fixed guarantee.

All **160 requests** match the complete 76-token output, natural stop, 57/57
drafts and zero reuse. Hardware and policy remain Windows A770 C0, driver
32.0.101.9034, 5,771 slots, 1,536-MiB reserve, MTP4, step/last min-p .5 and
cumulative zero. Each arm discards ten warmups and retains ten requests.
Decode excludes preparation/init/prefill; TTFT stays separate. First-bracket
TTFT is **903.769 / 910.874 / 907.331 ms** for original L0 / default L0 / DG2;
reverse L0 / DG2 is **899.858 / 902.942 ms**. No prefill gain, new copy/lock census
or physical hardware concurrency is claimed. No inner timers or GPU telemetry
run during model requests. Linux GPU execution remains untested; production
binaries/configuration and immutable Linux bundles are preserved.

[Results, fixed parameters and dispersion](../orca-l0/demo/expert-schedule-integration-windows-2026-11-03.json)
and 258 source/build/check/raw-protocol records（本地归档：`../orca-l0/demo/expert-schedule-integration-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/publish_expert_schedule_integration.py --verify`.
The archive retains both executed runners, exact source, build commands, input
hashes and raw requests. Reproduction uses the same hardware/driver, modules
and policy, with a fresh output directory; exact speed is not guaranteed.

### Compact DG2 ISA padding

The isolated comparison changes one expression in `src/drm/recording.inl`:
zeroed ISA tail padding falls from 4,096 to 512 bytes, retaining 64-byte kernel
alignment and the existing 1-MiB slabs. Pinned NEO commit
`ee495cce3db8f870a89697febbd7e8b6680151b2` uses a 512-byte DG2 default in
`GfxCoreHelperHw::getPaddingForISAAllocation()`. This is a source reference,
not proof of the installed driver's policy. Kernel bytes, barriers, matched
expert/router schedules, 80 Host objects, module pack and model settings stay
fixed. Both native variants are freshly built with the same compiler.

| Short ordering | Native 4-KiB control | Native 512-B padding | Matched L0 | Native padding gain |
| --- | ---: | ---: | ---: | ---: |
| L0 / control / compact / control / L0 | 48.580 tok/s | 49.196 tok/s | 49.536 tok/s | +1.267% |
| Compact / control / compact | 48.809 tok/s | 49.105 tok/s | Not measured | +0.607% |

Bracket controls are geometric means of adjacent arm medians. The first native
control drifts **+1.047%**, and L0 drifts **+0.477%**; reverse compact controls
drift **+0.052%**. Both orderings observe a small native improvement, with a
**0.686% remaining L0 deficit** in the first. First TTFT is **899.012 / 890.048 /
886.718 ms** for native control / compact / L0; reverse control / compact is
**890.600 / 888.173 ms**. These unlocked-clock observations do not establish
stable TTFT gains, a confidence interval or a guaranteed backend advantage.
Absolute rates from earlier rounds and their improvements are not combined.

All **160 performance requests** match 76 complete tokens, natural stop,
57/57 drafts and zero reuse. Each arm discards ten warmups and retains ten
requests. Policy remains 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p
.5 and cumulative zero, on Windows A770 C0 / driver 32.0.101.9034. Decode
excludes preparation/init/prefill; TTFT is separate. No inner timers, GPU
timestamps or telemetry run during performance requests. Each variant passes
15 Windows checks; batching off/on totals **1,728 full-buffer comparisons /
2,064 GPU tail epochs** across the two builds.

A separate first-use census runs **two exact Short diagnostic requests**,
excluded from timing samples. Both load the same 109 kernels with 3,514,112
text bytes. Captured ISA ranges shrink **3,960,576 to 3,569,920 bytes**
(-390,656 bytes). Actual retained storage stays **five 1-MiB allocations**,
including four in local VRAM. Thus this test shows no released VRAM. Placement
also changes; no physical cache-traffic or cache-miss cause is established.
No new D2D/application-lock census is implied.

The single-file change is adopted as **v12**, gated to Windows. Selecting that
branch reproduces the measured compact source; selecting Linux reproduces the
4-KiB control. The exact adopted stage passes 15 Windows checks, another
864 full-buffer comparisons / 1,032 tail epochs and one exact Short smoke
request. Linux builds and passes three CPU checks; Linux GPU remains deferred.
Production executables/configuration, MTP work, root README and immutable
Linux handoffs are preserved.

[Results, footprint and dispersion](../orca-l0/demo/dg2-isa-padding-windows-2026-11-03.json)
and 243 source/build/check/protocol records（本地归档：`../orca-l0/demo/dg2-isa-padding-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/check_dg2_isa_padding.py --verify-published`.
[V12 adoption and exact-source checks](../orca-l0/demo/dg2-isa-padding-main-windows-2026-11-03.json)
are historical after v13; use the current-source verifier below.
The archives retain executed runners, raw protocols and source/input/module
hashes. A fresh measurement needs a new output directory with the same
hardware/driver, modules and fixed workload; exact speed is not guaranteed.

### Fresh v12 executed-command audit

An independent diagnostic now captures the warmed eleventh Short decode using
the adopted v12 runtime and matched expert/router schedules on both backends.
The same frozen model entry point is rebuilt only to mark Decode boundaries;
the other Host objects, mathematical modules and fixed min-p/cache policy stay
unchanged. All **22 requests** match 76 complete output tokens, natural stop,
57/57 drafts and zero reuse. The observer follows primary and secondary
command buffers with bounded CPU-backed reads. Both captures have **zero
decoding failures**, and a separate typed decoder reproduces every count.

| Executed command evidence | Native v12 | Matched L0 |
| --- | ---: | ---: |
| KMT submissions | 136 | 795 |
| Captured command ranges | 136 | 1,009 |
| Command bytes | 7,418,376 | 7,258,772 |
| COMPUTE_WALKER | 40,778 | 40,932 |
| CFE_STATE | 2,894 | 97 |
| STATE_COMPUTE_MODE | 2,894 | 2,392 |
| STATE_BASE_ADDRESS | 1,004 | 214 |
| PIPE_CONTROL | 34,550 | 34,281 |

These are fresh counts, not a subtraction of previous timing percentages.
Internal copy/fill kernels differ; the extra L0 walkers do not imply a model
output difference. The observer substantially perturbs execution, so its timing
and application-lock observations establish no performance or contention gain.
Command bytes are not physical cache, memory or PCIe traffic.

Extending the decoder to four SBA address fields identifies a concrete next
control. Native serializes two instruction-base windows:
`0xffff800000000000` and `0xffff800300000000`. Its general-state base remains
constant, while **268 within-file changes affect only the instruction base**.
Another **98** affect only surface/bindless bases. There are **136 initial
SBA programs**, one per native captured file, and **502 unchanged-address
repeats** from the existing duplicate-SBA workaround. These categories total
1,004 commands. L0 serializes one instruction base throughout this capture.
Its secondary files do not inherit decoded state in this analysis; their
per-file initial count must not be interpreted as context resets.

The native bootstrap copy code is allocated before large tensors and most
mathematical code afterward. A Windows-only isolated candidate therefore
preallocates a 1-MiB bootstrap code slab and 4-MiB mathematical code slab at
module binding, before tensor allocations. This preserves code bytes, explicit
dependencies and fallback slab growth. The raw address observation motivates
that experiment; it does not establish the timing cost of the 268 changes.
No candidate source is adopted at this diagnostic stage.

The extended parser's first invocation failed to normalize Windows CRLF in
raw stdout. The decoder itself succeeded; the original runner and output are
retained, and the parser resumes without recompilation or GPU recapture.
[Counts, field distributions and scope](../orca-l0/demo/dg2-v12-packets-windows-2026-11-03.json)
and 1,197 checked raw/source/protocol records（本地归档：`../orca-l0/demo/dg2-v12-packets-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/publish_dg2_v12_packets.py --verify`.
This read-only verifier checks frozen execution evidence, independent of later
live-source adoptions. Production binaries/configuration, MTP work, root
README and immutable Linux bundles are preserved; Linux GPU remains untested.

### Early code slabs remove 268 state preambles

The isolated follow-up reserves a 1-MiB host bootstrap code slab and 4-MiB local
mathematical code slab during Windows memory-module binding, before large
tensor allocations. It applies only to local ISA with empty code heaps. Normal
slab growth remains available; Linux and host-ISA behavior stay unchanged.
The exact tested source is now adopted as **v13**, changing only
`src/drm/recording.inl`. No new runtime API, dependency or tuning switch is added.

| Independent warmed Decode capture | v12 | Early slabs | Matched L0 |
| --- | ---: | ---: | ---: |
| Instruction-only window changes | 268 | **0** | Not inferred across secondary files |
| KMT submissions | 136 | 136 | 795 |
| COMPUTE_WALKER | 40,778 | 40,778 | 40,932 |
| Full state / CFE programs | 2,894 | **2,626** | 97 |
| STATE_BASE_ADDRESS | 1,004 | **468** | 214 |
| PIPE_CONTROL | 34,550 | **34,282** | 34,281 |
| Executed command bytes | 7,418,376 | **7,351,912** | 7,258,772 |

The candidate capture decodes every command without failure. Each of the 268
removed preambles contains one CFE, preemption register write, pipeline select,
compute-mode update and invalidating barrier, plus two SBA commands. Native
now uses one serialized instruction base. The remaining 98 within-file
surface/bindless-base changes and 136 file-initial SBA programs are preserved,
including the duplicate-SBA workaround. This proves a command reduction; it
does not quantify the timing cost of an individual packet or cache operation.

| Clean Short | Adjacent v12 control | Early slabs | Matched L0 | Candidate versus v12 |
| --- | ---: | ---: | ---: | ---: |
| L0 / v12 / candidate / v12 / L0 | 49.190 tok/s | 49.423 tok/s | 49.740 tok/s | +0.473% |
| Candidate / v12 / candidate | 49.218 tok/s | 49.328 tok/s | Not measured | +0.224% |

First native control drift is **+0.204%**, L0 drift **-0.301%**; reverse candidate
drift is **+0.260%**. The reverse improvement is smaller than its control drift.
These are small point estimates, not a guaranteed gain or confidence interval.
The first candidate remains **0.637% below matched L0**. A CPU-only archive
publication ran during the first bracket; no parallel archive/compiler job ran
during reverse timing. No causal share of the variation is assigned.

First TTFT is **891.459 / 883.625 / 882.228 ms** for v12 / candidate / L0;
reverse v12 / candidate is **885.181 / 885.610 ms**. A stable TTFT/prefill gain
does not repeat. All **160 clean requests** match the full 76-token output,
natural stop, 57/57 drafts and zero reuse. Each process discards ten warmups
and retains ten requests. The separate **11 diagnostic requests** also match;
their observer timings are excluded. Hardware, fixed 5,771 slots, reserve
1,536 MiB, MTP4, step/last min-p .5 and cumulative zero stay the same. Decode
excludes preparation/init/prefill, with no inner timers or GPU telemetry.

The candidate passes **15 Windows checks**, **864 full-buffer comparisons /
1,032 GPU tail epochs** with batching off/on, Linux compilation and three CPU
checks. The adopted source is byte-identical to that stage, and all 12 tracked
current-source hashes pass. Mathematical code, 80 Host objects and matched
schedules stay fixed. Code allocation size/placement and tensor addresses can
change; no physical-cache cause or new copy/application-lock census is claimed.

Windows local-ISA initialization now reserves **5 MiB upfront**, including
4 MiB in local VRAM. Small programs can retain unused initial space; larger
programs keep the existing growth path. This is not a new full-model retained
code census or automatic-cache-size benchmark. The model comparison fixes the
expert count explicitly. Production executables/configuration, MTP work, root
README and immutable Linux bundles are preserved; Linux GPU remains untested.

[Both brackets, packet deltas and limits](../orca-l0/demo/dg2-early-isa-windows-2026-11-03.json)
and 242 checked evidence files（本地归档：`../orca-l0/demo/dg2-early-isa-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/publish_dg2_early_isa.py --verify`.
[V13 single-source adoption](../orca-l0/demo/dg2-early-isa-main-windows-2026-11-03.json)
and 18 adoption records（本地归档：`../orca-l0/demo/dg2-early-isa-main-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/adopt_dg2_early_isa.py --verify`.
The frozen packet and benchmark verifiers remain independent of future source
adoptions. A fresh measurement needs the same hardware/driver, module hashes
and explicit workload with a new output directory; exact rates are not promised.

### Queue scratch reuse removes 95 more state preambles

The existing v13 command files were decoded offline, without rerunning the GPU.
Old module loading order and ELF metadata reconstruct all 84 observed kernel
addresses, checking address/SIMD/GRF for every walker; GPU addresses from the
older process are not reused. Of 98 within-list scratch-base changes, **97 go from zero scratch to a required
pool after leading no-scratch kernels**, and one grows the slot-0
pitch from 4 KiB to 8 KiB. All 136 lists initially bind zero scratch. The observed
model pack uses slot 0; slot 1 is empty.

The exact tested source is adopted as **v14**, changing only
`src/drm/recording.inl`. Scratch lookup can return this queue's already allocated
pool when a kernel requests zero scratch; private-memory lookup keeps its old
demand-bound behavior. Descriptors remain immutable, pools remain writable and
owned per queue, and each recording retains the allocations it binds. No new
runtime API, dependency or tuning switch is introduced.

| Independent warmed Decode capture | v13 | v14 |
| --- | ---: | ---: |
| KMT submissions | 136 | 136 |
| COMPUTE_WALKER | 40,778 | 40,778 |
| Within-list scratch-base changes | 98 | **3** |
| Full state / CFE programs | 2,626 | **2,531** |
| STATE_BASE_ADDRESS | 468 | **278** |
| PIPE_CONTROL | 34,282 | **34,187** |
| Executed command bytes | 7,351,912 | **7,328,352** |

Exactly **95 list-initial programs** now bind an existing scratch pool. The
remaining three transitions are two first bindings and the 4-KiB to 8-KiB
growth. The two independent captures match all 40,778 walker address/GRF/SIMD/
local/group shapes as a multiset, and all **39,985 mathematical walkers** appear
in identical order. Two independent memory submissions exchange file positions
93/94; each complete walker sequence matches its counterpart. Direct comparison
by file number would therefore be wrong. Both decoders report zero failures.
This proves removal of 95 full preambles and 190 SBA commands, **23,560 command
bytes**; it does not measure physical traffic or the time spent on each packet.

| Clean Short | Adjacent v13 control | v14 | Matched L0 | v14 versus v13 |
| --- | ---: | ---: | ---: | ---: |
| L0 / v13 / v14 / v13 / L0 | 48.151 tok/s | 48.326 tok/s | 48.798 tok/s | +0.364% |
| v14 / v13 / v14 | 47.814 tok/s | 48.149 tok/s | Not measured | +0.701% |

First native control drift is **+0.639%**, reverse candidate drift **-0.707%**.
These improvements are near drift. First L0 control drift is **-4.204%**, making
the backend comparison particularly uncertain; v14 is **0.967% below** the
geometric L0 control. These point estimates establish neither statistical
significance nor a stable native-over-L0 win. No samples are discarded beyond
the declared warmups, and gains from earlier rounds must not be added.

First TTFT is **910.753 / 902.015 / 897.556 ms** for v13 / v14 / L0; reverse
v13 / v14 is **913.151 / 910.826 ms**. Reverse candidate TTFT drift is +1.371%;
the smaller TTFT difference is not an established repeatable gain.

All **160 clean performance requests and 11 separate diagnostic requests**
match the full 76 tokens, natural stop, 57/57 drafts and zero reuse. Each
performance process discards ten warmups and retains ten requests. A770 C0,
driver 32.0.101.9034, fixed 5,771 slots, 1,536-MiB reserve, MTP4, step/last min-p
.5 and cumulative zero stay fixed. Decode excludes preparation/init/prefill;
TTFT is separate. No inner timers, GPU timestamps or telemetry run in timing
arms. No archive/compiler job overlaps timing; lightweight offline trace
analysis and script preparation do occur. Hardware clocks remain unlocked.

The candidate passes **15 Windows checks**, **864 full-buffer comparisons /
1,032 GPU tail epochs** with batching off/on, Linux compilation and **three CPU
checks**. Adopted source is byte-identical to the measured stage; all 12 tracked
source hashes pass. The fixed mathematical pack and 80 frozen Host objects
preserve the matched expert/router schedules; this is **not a full latest-engine
benchmark**. Allocation/data addresses may differ. No new copy/application-lock,
retained-memory or automatic-cache-size census is claimed. Linux GPU is untested;
production executables/configuration, MTP, root README and Linux bundles are
preserved.

[Attribution, timing, raw samples and limits](../orca-l0/demo/dg2-scratch-retain-windows-2026-11-03.json)
and 261 evidence files（本地归档：`../orca-l0/demo/dg2-scratch-retain-windows-2026-11-03.json.gz`）
pass `python build-orca-native-lower-20261031/publish_dg2_scratch_retain.py --verify`.
[V14 exact source adoption](../orca-l0/demo/dg2-scratch-retain-main-windows-2026-11-03.json)
passes `python build-orca-native-lower-20261031/adopt_dg2_scratch_retain.py --verify`.
V13's live-source adoption verifier is now historical; its immutable report and
frozen benchmark/packet checks remain valid. New timing needs the same
hardware/driver, explicit workload, module hashes and a fresh output directory;
exact tok/s are not guaranteed.

### C0 single SBA: verified command reduction, no repeatable model gain

Fresh offline decoding of v14 and matched L0 reproduces every captured command
count. V14 contains **139 pairs of adjacent, byte-identical SBA commands**;
the installed L0 capture has **zero such pairs**. The cached
[pinned NEO product policy](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/xe_hpg_core/dg2/os_agnostic_product_helper_dg2.inl#L74)
requires an additional SBA for G10 B-before-C or G11. Cached device IDs place
0x56a0 in G10, revision 8 maps to C0, and the workaround helper excludes the
fix stepping from its range. This is direct evidence for testing one SBA on
this calibrated A770 C0; the reference source alone would not prove the
installed driver's behavior.

An isolated candidate changes only the second `emit(base)` into a conditional
on the existing calibrated Windows A770 C0 profile. It retains the first SBA,
all barriers, cache fields, state writes, walkers and storage ownership. Linux
and other stepping fallbacks keep duplicate SBA. This differs from earlier
pure-GRF trials that omitted **both** unchanged SBA commands.

| Separate warmed Decode capture | v14 | Single-SBA candidate |
| --- | ---: | ---: |
| KMT submissions | 136 | 136 |
| COMPUTE_WALKER | 40,778 | 40,778 |
| STATE_BASE_ADDRESS | 278 | **139** |
| CFE / LRI / pipeline / compute-mode programs | 2,531 each | 2,531 each |
| PIPE_CONTROL | 34,187 | 34,187 |
| Executed command bytes | 7,328,352 | **7,316,120** |

Independent typed and raw-header decoding agree. All non-SBA counts and all
walker address/GRF/SIMD/local/group shape multisets match; all **39,985
mathematical walkers** retain their order. No duplicate SBA remains. The
**12,232 saved command bytes** are not physical memory/PCIe traffic and do not
measure the cost of individual packets.

| Clean Short | Adjacent v14 control | Candidate | Matched L0 | Candidate versus v14 |
| --- | ---: | ---: | ---: | ---: |
| L0 / v14 / candidate / v14 / L0 | 48.158 tok/s | 48.292 tok/s | 48.414 tok/s | +0.279% |
| Candidate / v14 / candidate | 48.213 tok/s | 48.023 tok/s | Not measured | -0.393% |

The sign reverses. First native control drift is **-0.833%**, L0 drift **-0.983%**;
reverse candidate drift is **+0.294%**. The first candidate remains **0.252%
below matched L0**. These point estimates do not establish a throughput gain,
statistical significance or a stable native backend win. First TTFT changes
-0.228%, reverse +0.283%, so TTFT gains also do not repeat. The candidate stays
**isolated**; the adopted runtime baseline remains **v14**. Concurrent local-ID
payload-cache changes in the live worktree are outside this frozen comparison.
A real command reduction
does not justify advertising a model speedup.

All **160 performance and 11 separate diagnostic requests** match all 76 output
tokens, natural stop, 57/57 drafts and zero reuse. Each performance process
discards ten warmups and retains ten requests. Model policy remains 5,771 slots,
1,536-MiB reserve, MTP4, step/last min-p .5/.5 and cumulative zero. The same
mathematical pack and 80 frozen Host objects preserve matched schedules; this
is not a full latest-engine benchmark. Decode excludes preparation/init/prefill,
TTFT stays separate, and no inner timers/GPU timestamps/telemetry run in timing
arms. Clocks remain unlocked; no archive/compiler job overlaps timing.

The final candidate passes **15 Windows checks**, **864 full-buffer comparisons /
1,032 tail epochs** with batching off/on, Linux compilation and **three CPU
checks**. Its CPU regression checks single/duplicate selection for fresh,
untracked, changed-heap, changed-cache and profile-transition cases. An earlier
attempt passed 13 GPU checks but failed one old CPU equal-length assertion
between profiles, before any model run. The corrected test expects one SBA's
size difference and verifies packet counts; runtime encoding is byte-identical
between attempts. Both attempts are archived.

No new copy/lock/retained-memory/automatic-cache census is claimed. Linux GPU,
production executables/configuration, MTP, root README and immutable Linux
bundles remain unchanged. [Both orders, raw evidence and policy references](../orca-l0/demo/dg2-single-sba-windows-2026-11-03.json)
can be checked with
`python build-orca-native-lower-20261031/publish_dg2_single_sba.py --verify`.
The frozen v14 baseline is verified by `publish_dg2_scratch_retain.py --verify`;
the older adoption verifier requires exact v14 live sources.

### Close-time dependency compaction: fewer barriers, no repeatable model gain

An independent offline parse of the frozen v14 capture identifies **1,863
adjacent, byte-identical ordinary GPU dependencies**; matched L0 has 1,727.
The parser reproduces all original packet counts and v14's independently typed
PIPE_CONTROL flag histogram. These counts alone do not assign execution cost.

The isolated candidate compacts those dependencies once at command-list close,
before materialization and the final host-visible flush, on calibrated Windows
A770 C0 only. Replay has no new work. Host-visible, invalidating, signal,
timestamp and wait commands remain, as do intervening commands and inline data
that happen to resemble a barrier. Linux close keeps its existing behavior.

A separate warmed Short capture confirms **34,187 to 32,324 PIPE_CONTROLs**,
saving **44,712 command bytes** (7,328,352 to 7,283,640). Every other packet count
stays fixed: **136 submissions / 40,778 walkers**, including all **39,985
mathematical walkers in order**. Whole-stream walker order can differ because
independent memory lists reorder. Command bytes are not physical bus traffic.

| Clean Short order | v14 control | Candidate | Candidate versus v14 |
| --- | ---: | ---: | ---: |
| L0 / v14 / candidate / v14 / L0 | 48.124 tok/s | 47.330 tok/s | -1.650% |
| Candidate / v14 / candidate | 47.314 tok/s | 48.225 tok/s | +1.926% |

First native control drift is -0.133%, matched L0 drift -2.375%; the first L0
control is 48.083 tok/s. Reverse candidate drift is +0.095%. TTFT also reverses:
**+1.262% / -0.892%**. These observations establish neither a repeatable gain
nor a stable regression. The candidate stays **isolated**, with no adoption.
The real packet reduction does not establish the cause of the model gap.

All **160 performance / 11 separate diagnostic requests** match full 76-token
natural output, 57/57 drafts and zero reuse. Each performance process discards
ten warmups and retains ten requests. Parameters remain 5,771 slots, 1,536-MiB
reserve, MTP4, step/last min-p .5/.5 and cumulative zero. The same 80 frozen Host
objects and math pack preserve matched schedules; concurrent live local-ID
caching is excluded. Decode excludes preparation/init/prefill; TTFT is separate.
No inner timers, GPU timestamps or telemetry run in performance arms, and no
compiler/archive jobs overlap timing. Clocks remain unlocked.

The candidate passes **15 Windows checks**, **864 full-buffer comparisons /
1,032 tail epochs** with batching off/on, Linux build and **three CPU checks**.
The CPU regression covers special flushes/signals/waits, barrier-shaped inline
data, idempotence and truncation. The first attempt failed to compile a test
using MI_NOOP absent from the minimal packet definitions. V2 uses the existing
zero word; runtime helper and close-path bytes are identical between attempts.
No GPU/model run occurred in that failed attempt; its evidence is preserved.

No fresh copy/lock census or Linux GPU validation is claimed. Production
executables/configuration, MTP, root README and immutable Linux bundles are
preserved. [Both orders, command evidence and exact sources](../orca-l0/demo/dg2-barrier-compact-windows-2026-11-03.json)
and the complete archive（本地归档：`../orca-l0/demo/dg2-barrier-compact-windows-2026-11-03.json.gz`）
are verified by
`python build-orca-native-lower-20261031/publish_dg2_barrier_compact.py --verify`.

### Same-process ABBA: dependency compaction still shows no gain

An offline cross-check reuses **nine v14 / six L0 processes** with exactly the
same executable hashes, math pack, arguments and policy. Their 150 retained
requests reproduce raw protocols. Process decode medians span **47.314–48.360
tok/s (2.21%)** for native and **47.509–49.857 tok/s (4.94%)** for L0. These
descriptive ranges include chronological drift; they do not identify a cause.
Cached Windows topology is one Xeon E5-2696 v4 socket, 22 cores / 44 threads
and one NUMA node. Topology queries are not repeated during requests.

A new isolated diagnostic prepares full and compact buffers for each recorded
list, then selects only the submitted batch pointer/length. Both modes share
the model weights, parameter/scratch allocations and runtime/context within a
process. The same selector code is present in both modes. Frozen Host libraries
and math pack remain; only main is regenerated for request-boundary selection.
Each process discards **12 warmups**, then retains **48 requests / 12 four-request
cycles**; a second process reverses ABBA to BAAB.

| Same-process order | Paired cycles | Median paired decode change |
| --- | ---: | ---: |
| Full / compact / compact / full | 12 | **-0.238%** |
| Compact / full / full / compact | 12 | **-0.431%** |

Each cycle compares the geometric rates of its two requests per mode, then the
reported change is the median over 12 cycles. All individual values remain in
the report. There is **no throughput gain** in these observations and no
statistical-significance or stable-regression claim. Individual cycle changes
still span -1.977% to +1.088% in BAAB. The original opposite-order result is
not evidence for adopting compaction; it stays **isolated**.

A separate diagnostic captures request 11 (compact) and 12 (full) in one
process. Actual PIPE_CONTROL counts are **32,324 / 34,187**, with the same
136 submissions and 40,778 walkers. All **97 lists containing mathematical
kernels match byte-for-byte after removing only repeated ordinary barriers**,
including encoded addresses and inline bytes. The remaining **39 memory-only
lists** have per-request dynamic pointers. All walker shape multisets and
**39,985 mathematical walker order** match. The first all-136-list byte-equality
assertion was too strict; the corrected offline check reuses the original
packets and complete protocol, with no GPU rerun or invented host timings.

All **120 performance / 12 diagnostic outputs** match complete 76-token natural
output, 57/57 drafts and zero reuse. Thirty Windows checks exercise both command
selections. Parameters remain 5,771 slots, 1,536-MiB reserve, MTP4, step/last
min-p .5/.5 and cumulative zero. Selection/logging is outside the decode timer;
the final asynchronous commit joins before clearing selection for prompt work.
Dual materialization at close is common to both modes; dynamically closed lists
may include this common work inside decode. Submission performs no compaction
or allocation. No added profiling clocks/GPU timestamps/counters/telemetry run
in timing arms; clocks remain unlocked. No TTFT or matched-L0 gain follows.

Concurrent live local-ID caching is excluded; this is not a full latest-engine
benchmark. No production-source adoption, fresh copy/lock census, Linux GPU
validation or new Linux build claim follows. The Windows diagnostic leaves the
separately checked Linux paths unchanged. [Raw samples, topology and exact sources](../orca-l0/demo/dg2-barrier-abba-windows-2026-11-03.json)
and the complete archive（本地归档：`../orca-l0/demo/dg2-barrier-abba-windows-2026-11-03.json.gz`）
are checked by
`python build-orca-native-lower-20261031/publish_dg2_barrier_abba.py --verify`.

### Independent consecutive copies: small positive paired results, isolated

The ordinary-barrier experiment above does not test concurrent transfers. A
new frozen-v14 diagnostic instead checks every consecutive copy for RAW, WAR
and WAW overlap against its whole pending group. Shared reads and adjacent
disjoint ranges can overlap. Any conflicting copy, intervening command/event
or 64-copy cap retains the original dependency. Existing kernels, transfers,
math schedule and queue boundaries stay fixed; no wait-to-fill is introduced.

Both command variants share allocations/context within each process, using
the same request selector as the preceding ABBA control. Each process discards
12 warmups and retains 48 requests in 12 four-request cycles.

| Same-process order | Full cycle median, tok/s | Overlap cycle median, tok/s | Median paired change |
| --- | ---: | ---: | ---: |
| Full / overlap / overlap / full | 48.111 | 48.301 | **+0.219%** |
| Overlap / full / full / overlap | 47.897 | 48.199 | **+0.559%** |

The paired change is the median of cycle changes between geometric rates;
it is not the ratio of the displayed medians. All outliers remain archived.
Both signs are positive, but these small observations do not establish a
stable production gain or a native-over-L0 win. The candidate stays isolated.
Both buffers and hazard analysis are prepared at close in both modes; this
does not measure the cost of a one-buffer production implementation. There is
no L0 arm, TTFT gain claim or added inner timing/GPU timestamp instrumentation.

A separate diagnostic captures two requests and saves each submitted list's
original commands and exact copy ranges. Independent interval analysis checks
659 in-order 1D copy records per request. Twenty batches of 32 independent
copies permit **620 dependencies / 14,880 command bytes** to be removed.
Actual captures retain **136 submissions / 40,778 walkers**, with PIPE_CONTROL
**34,187 to 33,567**. Every executed byte in all **272 captured ranges** matches
its own reconstructed reference. These are command bytes, not physical traffic.
The initial verifier incorrectly included optional alignment padding after BBE;
the corrected offline check validates that zero DWORD separately and reuses the
original successful capture. No GPU rerun or missing timings are reconstructed.

All **120 performance / 12 diagnostic outputs** match complete 76-token natural
output, 57/57 drafts and zero reuse; **30 Windows checks** pass. The balanced
policy stays 5,771 slots, 1,536-MiB reserve, MTP4, min-p .5/.5 and cumulative
zero. Decode excludes preparation, initialization and prefill; clocks are
unlocked. Frozen Host libraries/math are used with a regenerated selector main,
excluding concurrent live local-ID/MTP work. No production source, executable,
configuration or Linux handoff changes; the staged Windows candidate has no
Linux build/GPU claim. [Paired samples and command proof](../orca-l0/demo/dg2-copy-parallel-windows-2026-11-03.json)
and runnable sources and raw evidence（本地归档：`../orca-l0/demo/dg2-copy-parallel-windows-2026-11-03.json.gz`）
are checked by
`python build-orca-native-lower-20261031/publish_dg2_copy_parallel.py --verify`.

### Native copy cache controls: no consistent model-sized H2D gain

A separate frozen-v14 replay compares the default `load.ca.ca/store.wb.wb`
with three source-read cache policies. Each comparison shares its process,
context, allocations, geometry and data. Four sizes, one or 32 distinct copies,
H2D and D2D, and two overwritten phases reverse ABBA/BAAB ordering. Each phase
has 12 warmups and 48 retained samples. Results use paired GPU tick intervals;
preparation, uploads, validation and readback stay outside timing.

| 1,510,400 bytes, 32 copies: median paired rate change | H2D phase 0 / 1 | D2D phase 0 / 1 |
| --- | ---: | ---: |
| Uncached L1/L3, builtin non-temporal read | -1.446% / -1.724% | +7.704% / +8.024% |
| Uncached L1, cached L3, native cache-bit patch | -1.366% / +1.133% | -0.454% / -0.680% |
| Streaming L1, cached L3, native cache-bit patch | -0.370% / +2.549% | +0.203% / -0.194% |

The builtin also changes instruction scheduling, so its result is not a
cache-bit-only control. The two native controls patch just four decoded load
instructions across two isolated entry points; every other zebin bit is
unchanged. Their three parent execution bodies match **1,864 bytes through
EOT**. Stores stay write-back. Actual IGA output confirms `uc.ca` and `st.ca`;
SIMD16, 128 GRFs and zero declared SLM/scratch/private memory stay fixed.

Three attempted SPIR-V cache-decoration forms bake successfully but still emit
`ca.ca` in this installed pipeline. Those unchanged controls are not GPU-tested.
This does not establish that every SPIR-V cache-control path is unsupported.
The first native patch search also fails before writing or executing a module;
the archived correction searches adjacent field-bit combinations and checks
that only decoded cache policy and its descriptor literal differ. The original
textual-LLVM-IR compiler failure is retained too.

All **5,760 samples, 4,608 retained samples, 1,152 paired cycles and 342
full-buffer checks** pass. Checks include changed source data, complete source
and per-copy/outer guards, scalar tails, unaligned sizes and timestamp guards.
At 98,304 bytes with one copy, the two native cache controls observe H2D rate
changes of -7.10% to -10.09% and D2D changes of -14.84% to -15.97%. The H2D results above
provide no consistent reason to change defaults. The standalone large-D2D gain
is not a model-throughput gain. No full-model or L0 comparison, physical-bus
measurement, significance claim or Linux execution is added.

**No cache-policy change is adopted.** Runtime/adapter/production sources and
configuration, mathematical kernels, root README and immutable Linux handoffs
are preserved. The comparison excludes concurrent local-ID/storage/lookup/MTP
work. [All cache controls and measurements](../orca-l0/demo/dg2-copy-cache-windows-2026-11-03.json)
and 195 archived evidence files（本地归档：`../orca-l0/demo/dg2-copy-cache-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_copy_cache.py --verify`.

### Copy-grid caps and null controls: no default change

Two follow-up matrices use the **same cached kernel** in both arms, changing
only the intended group-count cap from 256 to 64 or 128. Local size, module,
ISA, allocations, context and dependencies stay fixed. Neither cap shows
consistent gains at the common model-sized H2D workload:

| 1,510,400 bytes, 32 H2D copies | Paired rate change, phase 0 / 1 |
| --- | ---: |
| Cap 64 versus 256 | -2.886% / +1.483% |
| Cap 128 versus 256 | -0.839% / +0.826% |
| Null: two separately recorded identical lists | -2.298% / +1.647% |
| Null: the same list replayed under both labels | +0.494% / +3.635% |

The grid tests contain an implicit null control: at 98,304 bytes both arms
launch **24 groups of the same kernel**. The cap-64 run nevertheless records
-14.415% / -0.244% for one D2D copy in its two phases. This is not a grid
optimization cost. Two additional complete matrices therefore test identical
grids/kernels explicitly, first with distinct recorded lists and then with the
same list actually reused for both labels. At 98,304 bytes / one D2D copy,
the latter gives +1.723% / -1.760%.

These controls expose variation without identifying whether clocks, ordering,
list state, addresses or other effects caused it. They also qualify the earlier
immutable cache report: **its small-copy deltas cannot all be uniquely attributed
to cache policy**. Small observed gains need controls beyond correct data and
ABBA ordering. The same-list result above is a timing control, not a performance
improvement. No confidence interval or physical occupancy claim is made.

All **7,680 new samples, 6,144 retained samples, 1,536 paired cycles and 456
full-buffer checks** pass, including source/per-copy/outer/timestamp guards,
changed phases, scalar tails and scratch guards. Together with the cache
matrices this is 13,440 samples / 798 full-buffer checks. Exact generated
sources, library/module hashes and raw logs are retained. No new packet,
physical-transfer or application-lock census is added.

**Keep cap 256 and the existing cache policy.** No full-model or L0 comparison,
native-over-L0 win, runtime-source adoption or Linux execution follows from
these standalone controls. Concurrent live local-ID/storage/lookup/MTP work
is excluded. [Grid and null-control results](../orca-l0/demo/dg2-copy-groups-windows-2026-11-03.json)
and 58 archived evidence files（本地归档：`../orca-l0/demo/dg2-copy-groups-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_copy_groups.py --verify`.

### Raw Copy-node BLT: correct transfers, mixed standalone gains

An isolated Windows extension now records `XY_BLOCK_COPY_BLT` directly on
Copy node 2. It uses no EU copy kernel, SPIR-V or zebin for those transfers.
The original raw path returned completion but failed its first 1-byte
H2D→D2D→D2H check. A process-local installed L0 capture identifies BLT **MOCS 2**,
while the experiment had reused compute **MOCS 6**. Context-private bytes agree
after excluding each process's pointer/PID; submission headers and flushes are
also retained for comparison.

A 16-way matrix varies MOCS, target-memory fields, LLC/CCS flushes and the
captured submission header independently. All eight MOCS-6 combinations fail
the first case; larger cases in those arms are not reached. All eight MOCS-2
combinations pass **73 full-buffer checks each**, including two changed phases,
unaligned/scalar tails, H2D/D2D/D2H/H2H, 32-copy batches and guards. This is
calibrated A770/driver evidence, not a portable MOCS specification.

The next isolated versions reuse immutable BCS lists, existing storage tracking,
KMT queue dependencies and completion APIs. Ordered copies retain visibility
flushes. Explicit-dependency batches use `begin(queue,false)` in **both** EU and
BCS arms, leaving the final visibility flush. Their copies access independent
ranges; this does not infer dependencies automatically. Kernels, fill and CS
timestamp requests on the copy-only queue reject before GPU submission.

These standalone results compare raw BCS with native EU copies, using host
submit-plus-completion time. Preparation, recording, uploads, validation and
readback are outside timing; timed lists contain no GPU timestamps. Each phase
reverses ABBA/BAAB ordering, with 12 warmups and 48 retained samples per case.

| Workload | Ordered BCS versus EU, phase 0 / 1 | Explicit-dependency BCS versus EU, phase 0 / 1 |
| --- | ---: | ---: |
| 1,510,400 bytes × 32, H2D | +1.307% / +2.521% | +1.129% / +4.197% |
| Same BCS list under both labels, matching H2D null | -0.452% / +1.085% | -0.180% / -1.162% |
| 4 MiB × 32, H2D | +4.704% / +4.671% | +2.655% / +3.036% |
| 4 MiB × 1, D2D | +37.178% / +33.944% | +47.656% / +40.791% |
| 4 MiB × 32, D2D | -1.983% / -3.053% | -5.225% / -7.944% |

BCS is not uniformly faster, and removing independent-copy flushes does not
establish an additional speedup across these separate runs. Null variation and
unlocked clocks qualify small deltas. Correct BCS→EU→BCS chains, reusable host
gates, memory tags and batched completion checks prove ordering; they do not
measure simultaneous execution or model-level overlap.

Both comparisons and their same-list nulls pass **7,680 samples / 6,144 retained
samples / 1,536 paired cycles / 456 full-buffer checks**, plus **24 cross-engine
full-buffer checks**. The earlier MOCS matrix adds 584 checks. Original compiler,
wrong-data and test-program staging-release failures are preserved alongside
their corrections. The installed L0 capture passes 11 changed-data checks but
is not a performance arm.

**No production adoption or full-model Short gain is claimed.** These are frozen
v14 Windows stages, excluding concurrent live local-ID/storage/resident/lookup/
MTP work. Linux GPU testing remains deferred; staged Windows changes must not
be copied into Linux blindly. [BLT diagnosis and all measurements](../orca-l0/demo/dg2-blitter-windows-2026-11-03.json)
and 445 archived evidence files（本地归档：`../orca-l0/demo/dg2-blitter-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_blitter.py --verify`.

### Actual Short on the Copy node

A frozen-model queue audit first identifies queue 8 as copy-only, including
its actual decode submissions. Other observed queues include fill or arithmetic
work and retain compute contexts. The same native executable then uses
`ORCA_BCS_EXPERTS=0/1` to select compute node 5 or Copy node 2 for queue 8 only.
Host/engine objects, mathematical zebins, model inputs and policy stay fixed.
This tests the raw BLT extension on the actual model rather than projecting
standalone copy throughput onto decode.

| Clean Short result | First ordering | Reverse ordering |
| --- | ---: | ---: |
| Native EU control, tok/s | 46.434 | 47.186 |
| Native BCS candidate, tok/s | 46.941 | 47.999 |
| Matched L0 control, tok/s | 46.726 | 47.631 |
| BCS versus EU decode | +1.093% | +1.724% |
| BCS versus L0 decode | +0.460% | +0.773% |
| Native flank drift | +0.202% | -0.941% |
| L0 control drift | +0.333% | -1.329% |
| BCS / L0 TTFT, ms | 967.47 / 966.58 | 927.32 / 919.80 |

The first ordering is L0/EU/BCS/EU/L0; the reverse is L0/BCS/EU/BCS/L0.
Each process discards ten warmups and retains ten requests. Table entries use
medians, with geometric means for the adjacent controls or BCS flanks. All
200 timing requests match 76 output tokens, natural stop and 57/57 drafts.
Decode excludes initialization, graph preparation and prefill; TTFT is separate.
The profile uses 5,771 expert slots, 1,536 MiB reserve, MTP4, step/last min-p
0.5, cumulative floor zero and no prefix reuse. Clocks are unlocked. Both
native-over-L0 point estimates are positive, but these small differences and
control drift do not establish a stable win. TTFT is not improved.

Separate diagnostic execution confirms **640 EU copy walkers replaced by
1,280 raw BLT commands**, with **20 Copy-node submissions**. Total submissions
remain **136**; EU walkers fall **40,778 to 40,138**, PIPE_CONTROL commands
**34,187 to 33,487**, and SBA commands **278 to 238**. These packet counts
describe the native EU/BCS comparison, not mathematical fusion or measured
simultaneous execution.

All eleven corresponding EU/BCS logical-transfer censuses match. The final
warmed request has **659 H2D operations / 968,318,976 bytes**, **134 D2D /
4,554,752 bytes**, and no D2H/H2H/shared copies or fills. Uncaptured request
10 records **130 application mutex acquisitions and zero SRW acquisitions**
for each of L0, EU and BCS. Packet capture on request 11 lengthens the L0
decode to about 21 seconds and raises its imported mutex count to 1,222;
that count is excluded from the lock comparison. Capture work and changed
worker timing affect it; no attribution of the excess locks is claimed.
Payload counters run separately from lock probes and clean timing. These
are logical payloads and application imports, not physical bus traffic or
private driver locks.

The queue audit and initial integration add six exact requests; independent
packet/lock and transfer probes add 55, for **261 exact complete Shorts**.
The adapter in that baseline ignores its `copy_engine` constructor argument, so the
fixed queue ordinal is deliberately diagnostic. Production wiring must use
that explicit argument and rebuild its inline adapter users consistently.
No runtime/adapter source or default is adopted by this experiment. Concurrent
live local-ID/storage/resident/lookup/MTP changes and Linux GPU are outside
scope. [Measurements and parameters](../orca-l0/demo/dg2-bcs-short-windows-2026-11-03.json)
and 2,606 archived evidence files（本地归档：`../orca-l0/demo/dg2-bcs-short-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_bcs_strata.py --verify`.

### Explicit copy-role API and native fills

The next frozen candidate honors `Stream(..., copy_engine=true)` and changes
the two single-GPU adaptive-refill callsites to request a copy queue. Their old
`dpct.create_queue(true)` argument meant in-order, not copy-engine selection.
All 80 Host translation units are rebuilt with the same adapter headers;
69 include the adapter, with no old constructor definitions. Main is then
recompiled once to change those two callsites. The same native executable
uses startup-cached `ORCA_DG2_BCS_COPY=0/1` to select EU or BCS for three explicit
copy roles. There is no per-token environment query or queue-ordinal switch.
Mathematical device modules and the integrated scheduling sources stay fixed.

Some copy roles also fill memory. Raw `XY_FAST_COLOR_BLT` now supports the
1..128-byte power-of-two pattern contract, arbitrary alignment, odd byte counts
and immutable replay. Aligned small patterns use compact rectangles; wide or
unaligned patterns use disjoint stripes and byte tails. The independent check
loads no EU kernel, SPIR-V or zebin, and passes **1,632 full-buffer/source/guard
checks**, with changed inputs, caller-pattern mutation, replays and invalid
arguments. This establishes correctness, not fill throughput.

| Clean Short result | First ordering | Reverse ordering |
| --- | ---: | ---: |
| Native EU control, tok/s | 48.828 | 49.303 |
| Native BCS candidate, tok/s | 49.888 | 49.827 |
| Matched L0 control, tok/s | 49.149 | 49.706 |
| BCS versus EU decode | +2.172% | +1.064% |
| BCS versus L0 decode | +1.505% | +0.244% |
| Native flank drift | -2.628% | -0.526% |
| L0 control drift | -2.918% | -0.717% |
| BCS / L0 TTFT, ms | 883.23 / 888.39 | 883.04 / 878.93 |

The orderings are L0/EU/BCS/EU/L0 and L0/BCS/EU/BCS/L0. Each of ten processes
runs ten warmups and ten retained requests; all 200 outputs match 76 tokens,
57/57 drafts, natural stop and zero prefix reuse. No outliers are removed.
Controls use geometric means of their process medians, not confidence intervals.
Decode excludes initialization, graph preparation and prefill; TTFT is separate.
The fixed profile remains 5,771 slots, 1,536 MiB reserve, MTP4, step/last min-p
0.5 and cumulative floor zero. Clocks remain unlocked. Both BCS-versus-EU
point estimates improve; drift still prevents a stable native-over-L0 claim,
and TTFT changes direction. Do not combine rates from different experiments.

Separate packet probes confirm **20 Copy-node plus 116 compute-node submissions**,
still 136 total. Walkers fall **40,778 to 40,138**, with **1,280 BLTs / 1,320
MI_FLUSH_DW**, PIPE_CONTROL **34,187 to 33,487** and SBA **278 to 238**.
All eleven paired logical-transfer censuses match: the final warmed request
has **659 H2D operations / 968,318,976 bytes**, **134 D2D / 4,554,752 bytes**,
and no other copies or fills. Uncaptured request 10 records EU/BCS imported
mutex counts **130/124**, balanced unlocks and zero SRW acquisitions. There is
no new L0 lock census. Background I/O timing affects totals; no fresh stacks
were collected to attribute the six-call difference. Capture-request counts are
excluded. Payload instrumentation, packet probes and clean timing are separate;
these counts do not measure physical bus traffic or simultaneous GPU execution.

Twelve integration requests plus 200 timing and 44 diagnostic requests give
**256 exact complete Shorts**. The archive also retains the initial private
diagnostic-access compile failure, all 80 compile commands, dependency files,
source/object/module hashes and raw protocols/packets. This is frozen Windows
staging, with no runtime/adapter source or default adoption and no Linux GPU
validation. Concurrent live local-ID/storage/resident/lookup/MTP work is excluded.
[Measurements and parameters](../orca-l0/demo/dg2-copy-api-windows-2026-11-03.json)
and 1,539 archived evidence files（本地归档：`../orca-l0/demo/dg2-copy-api-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_copy_api.py --verify`.

### One flush per logical copy: no measured model gain

The next frozen trial changes only `src/wddm/runtime.cpp`: `record_blit` moves
its ordering flush after the disjoint rectangle loop. A 1,510,400-byte expert
copy still uses two BLTs, with one flush instead of two. Ordering between
logical copies, explicit barriers, queue tails, signals, host gates and
cross-engine dependencies remain. All 80 Host objects, mathematical modules,
three copy roles and model settings stay fixed. Both native arms use
`ORCA_DG2_BCS_COPY=1`; the control is the previous BCS runtime.

| Clean Short result | First ordering | Reverse ordering |
| --- | ---: | ---: |
| Previous BCS control, tok/s | 49.533 | 49.880 |
| One-flush candidate, tok/s | 49.484 | 49.866 |
| Candidate versus BCS decode | -0.100% | -0.029% |
| Matched L0, tok/s | 49.672 | 49.050 |
| Candidate versus L0 decode | -0.378% | +1.663% |
| Native flank drift | +0.441% | -0.426% |
| L0 control drift | +0.052% | +2.718% |
| Candidate / BCS TTFT, ms | 889.04 / 889.59 | 884.71 / 883.13 |

Ten processes run L0/control/candidate/control/L0 and the opposite ordering,
with ten warmups and ten retained requests per process. No outliers are
removed. Decode excludes initialization, graph preparation and prefill; TTFT
is separate. All 200 outputs match 76 tokens, 57/57 drafts, natural stop and
zero reuse, at 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p 0.5 and
cumulative floor zero. Controls are geometric means of process medians, not
confidence intervals; clocks are unlocked. The reverse apparent L0 advantage
is smaller than L0 drift. Neither ordering establishes a decode or TTFT gain.

Actual captures confirm **MI_FLUSH_DW 1,320 to 680**, reducing Copy-engine
primary commands **144,320 to 131,520 bytes**. Both sides retain **20 Copy plus
116 compute submissions**, **40,138 walkers**, **1,280 BLTs**, **33,487
PIPE_CONTROL** and **238 SBA** commands. All eleven corresponding logical
transfer deltas match, including kernel and submission counts. The final
warmed payload remains H2D **659 / 968,318,976 bytes**, D2D **134 / 4,554,752
bytes**, with no other copies or fills. Uncaptured request 10 records BCS /
candidate imported mutex counts **124/130**, balanced unlocks and zero SRW
acquisitions. Their direction reverses the previous experiment's 130/124;
background I/O timing affects totals, so this is no lock-removal claim. Packet,
lock and payload probes are separate from clean timing and do not measure
physical bus traffic or simultaneous GPU execution.

Independent old/candidate/old copy matrices and a same-list BCS null retain
**7,680 samples**, including **456 guarded cases / 48 cross-engine checks**.
The cross-engine cases now include 1.5 MB and 4 MiB + 17 bytes. Another
**1,632 fill checks** pass. At 1.5 MB x 32 H2D, candidate point estimates are
**+0.764% / +1.355%**, against old wall-time drift **+1.570% / +1.460%**;
other dimensions and D2D include negative results. These host submit/wait
measurements do not establish a consistent copy or model improvement.

Six integration, 200 timing and 44 diagnostic requests total **250 exact
complete Shorts**. The candidate stays isolated without source/default
adoption or Linux GPU validation. Concurrent live changes and earlier reports
remain outside this experiment.
[All measurements and parameters](../orca-l0/demo/dg2-blt-flush-windows-2026-11-03.json)
and 1,081 archived evidence files（本地归档：`../orca-l0/demo/dg2-blt-flush-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_blt_flush.py --verify`.

### Calibrated compute/copy timeline

The next frozen Windows diagnostic extends `write_global_timestamp` to BCS
using `MI_FLUSH_DW` timestamp post-sync. It starts from the BCS fill baseline,
excluding the rejected flush trial and concurrent live changes. The existing
cached A770/driver calibration is independently checked on two CS and two BCS
queues: 16 same-queue gated samples, eight cross-engine gated samples and a
32-node alternating dependency chain. All guards, replay, integer wrap and
invalid/stale/misaligned/closed-list checks pass. The same checks pass again
on the instrumented library. Only the common lower 32 bits at **19.2 MHz**
are used; higher timestamp bits are not assumed to match between engines.

Two timestamps wrap each immutable list, with one guarded 64-byte slot per
close. The native owner buffers records and harvests a previous execution
before replay/reset/discard. The server intentionally uses `_Exit`, so the
diagnostic main explicitly drains every native queue and exports remaining
records on its owner before exiting. There is no per-list log output during
the request. The 80-TU frozen Host baseline and mathematical modules remain;
only the diagnostic main is rebuilt for this export. Every submitted
`(request, queue, sequence)` must have exactly one completed, guard-checked
span. All **88 calibration spans** and **11 complete model outputs** pass.

| Final diagnostic Short request | Measured span |
| --- | ---: |
| First captured start to last end | 1,568.700 ms |
| CS union, 116 submissions | 1,534.177 ms |
| BCS union, 20 submissions | 104.867 ms |
| CS/BCS intersection | 99.646 ms / 95.022% of BCS union |
| BCS outside captured CS | 5.221 ms |
| Outside both captured span sets | 29.303 ms |

The three observed CS queues have 19, 57 and 40 submissions, with summed
envelopes of **0.344, 215.696 and 1,318.138 ms** respectively. They do not
intersect one another in this request. All CS/BCS intersection occurs in the
57-submit CS queue. Queue indices are observations, not fixed routing rules
or operator labels. Large CS lists are the next useful attribution target;
the trace does not support treating the entire copy duration as serialized
request overhead.

These intervals include in-list waits and context scheduling. Intersection
does not prove continuously active simultaneous engines, and outside-BCS
time is not a bound on removable critical-path time. Gaps do not identify a
CPU lock, disk or driver cause. Only decode-phase submissions are attributed;
their execution can extend beyond that phase. Uncaptured work is excluded.
Timestamp flushes perturb execution, so no diagnostic request rate is used
as a clean throughput result or a native-over-L0 claim.

All eleven requests retain 76 exact tokens, 57/57 drafts, natural stop and
zero reuse/resume, at 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p
0.5 and cumulative floor zero. Another eleven exact requests from the earlier
missing-export build are preserved but supply no usable timeline. Two earlier
checker mistakes and one relative-library link failure are also archived.
This adds no source/default adoption or Linux GPU claim.
[Report and parameters](../orca-l0/demo/dg2-bcs-timeline-windows-2026-11-03.json),
706 evidence files（本地归档：`../orca-l0/demo/dg2-bcs-timeline-windows-2026-11-03.json.gz`）
and [execution-envelope plot](../orca-l0/demo/dg2-bcs-timeline-windows-2026-11-03.svg).
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_bcs_timeline.py --verify`.
The retained CPU parser check is `python build-orca-native-lower-20261031/probe_dg2_bcs_timeline_v3.py --analysis-check`.

### Module-family blocks

The next isolated diagnostic places a marker whenever consecutive kernels
change module family. Integer lower-32-bit intervals must partition every
captured CS list exactly, including prefix and tail regions. A retained CPU
check covers wrap, overlap-free partitions, missing/duplicate indices and
gaps. The same frozen Host baseline, mathematical modules and model policy
are reused; only the diagnostic main is rebuilt for export before `_Exit`.

All 88 list-calibration spans and 11 complete Shorts pass. The final request
has **116 CS lists, 27,752 blocks and 40,138 kernel calls**, matching the
separate baseline's captured walker census. Its summed CS envelopes are
**1,726.832 ms**:

| Module family | Kernel calls | Summed block envelopes |
| --- | ---: | ---: |
| `iq_kernels` | 4,816 | 633.826 ms |
| `fused_gr` | 8,212 | 349.656 ms |
| `native_mmvq` | 7,201 | 346.560 ms |
| `verify_kernels` | 3,446 | 124.102 ms |
| `native_bf16` | 2,936 | 97.035 ms |

The first three total **77.022%** of these envelopes. The longest observed
CS queue contributes 35,874 calls / 1,506.949 ms; its IQ, fused GR and native
MMVQ blocks contribute 630.670, 318.711 and 252.345 ms respectively. These
families are the next useful attribution targets, rather than assuming API
calls or copies account for the entire discrepancy.

This method adds **27,636 host-visible timestamp markers**, so its flushes
can materially change caches, overlap and scheduling. Family blocks also
contain state programming, ordinary barriers and other unmarked commands;
they are not pure kernel instruction times. Do not subtract this run from
the earlier list-only trace to measure observer cost: the runs are not
paired controls. No diagnostic rate is treated as clean throughput.

The separately archived baseline packet capture has zero decode failures,
136 submissions and no `MI_SEMAPHORE_WAIT`. This narrows explicit in-list
semaphore waiting as an explanation for that fixture, while leaving GPU
queue-fence dependencies and context scheduling outside those packets.
It is not a simultaneous capture of the instrumented run.

Every output retains 76 exact tokens, 57/57 drafts, natural stop and zero
reuse/resume at 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p 0.5 and
cumulative floor zero. The earlier missing-`<filesystem>` compilation failure
is preserved; it ran no GPU/model work. There is no source/default adoption,
Linux GPU validation or established native-over-L0 gain.
[All family blocks and checks](../orca-l0/demo/dg2-kernel-blocks-windows-2026-11-03.json)
and 452 evidence files（本地归档：`../orca-l0/demo/dg2-kernel-blocks-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_kernel_blocks.py --verify`.

### Direct walker timestamps: individual kernel targets

A second isolated diagnostic uses `COMPUTE_WALKER` post-sync operation 3 to
write four guarded u32 fields: context start, global start, context end and
global end. Standalone calibration passes **1,196 packets, 80 changed-input
waves and eight gated samples**, across two queues and local sizes 32/64/128.
It checks ordered/independent kernels, replayed inputs, output/tag guards,
write bounds and the common 19.2-MHz lower-32-bit global clock.

The model run passes all **88 calibration list spans and 11 exact Shorts**.
Its last request contains **40,138 kernels, 116 CS lists and 20 BCS lists**.
Every CS submit has complete indexed kernel records; global kernel bounds,
metadata references and guards are checked against its enclosing list.

| Module family | Calls | Sum of global kernel envelopes | Sum of raw context durations |
| --- | ---: | ---: | ---: |
| `iq_kernels` | 4,816 | 667.775 ms | 648.956 ms |
| `native_mmvq` | 7,201 | 408.598 ms | 399.761 ms |
| `fused_gr` | 8,212 | 328.483 ms | 327.816 ms |
| `verify_kernels` | 3,446 | 143.685 ms | 140.474 ms |
| `native_bf16` | 2,936 | 118.919 ms | 113.887 ms |

The main individual targets are IQ `gu_pair_wg128` (**646 calls / 285.406 ms**),
GR `gr_up_multi_q8_kernel` (**1,920 / 200.750 ms**) and IQ
`native_down_port<42,4,16,true,256>` (**912 / 174.845 ms**). The Q8 up image
declares SIMD32, 128 GRFs, **40,192 private bytes and 4,096 scratch bytes per
hardware thread**. This makes its large captured argument structure a useful
optimization target, not an established cause of the backend discrepancy.
The two IQ targets declare zero private/scratch memory. Resource metadata is
static; it does not describe dynamic SLM or actual memory traffic.

Kernel envelopes overlap: their sum is **not a partition of request wall
time or device utilization**. The enclosing request span is 1,609.158 ms;
CS union is 1,574.576 ms, BCS union 105.522 ms, their intersection 100.180 ms,
BCS outside CS 5.342 ms and outside both 29.240 ms. Global durations include
stalls and context scheduling. Raw context durations are not active EU cycles
or causal evidence of preemption. Kernel unions are computed within each
list, then summed across lists.

This method adds no per-kernel `PIPE_CONTROL` timestamp marker, but its
post-sync writes and dataport pipeline/subslice flushes can still change
caching and overlap. It is diagnostic, not clean throughput or zero-cost
profiling. Do not subtract the different profiling runs to infer observer
cost. The retained CPU check covers overlapping unions, wrap and rejection.
Model policy and exact outputs match the preceding diagnostic; no source
adoption, native-over-L0 gain or Linux GPU validation is claimed.
[Hardware kernel report](../orca-l0/demo/dg2-kernel-timestamps-windows-2026-11-03.json)
and 464 evidence files（本地归档：`../orca-l0/demo/dg2-kernel-timestamps-windows-2026-11-03.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_kernel_timestamps.py --verify`.

### Narrow Q8-up capture

The hardware profile identifies the GR Q8-up kernel as a useful target. An
isolated candidate captures only its required fields for all eight token
slots. Its arithmetic, reduction/barrier order, group/local dimensions and
batch capacity are preserved. Only `fused_gr/image_3.zebin` changes in each
comparison; the other **1,241 mathematical modules remain byte-identical**.
The matching Host object is rebuilt separately for each backend using its
actual frozen model headers and compiler command.

| Q8-up resource | Before | Candidate |
| --- | ---: | ---: |
| Captured argument bytes | 1,256 | 728 |
| Full closure, including local accessor | 1,264 | 736 |
| Compiler private bytes per hardware thread | 40,192 | 23,296 |
| SIMD / GRFs | 32 / 128 | 32 / 128 |
| Reported spill / rounded runtime scratch bytes | 2,208 / 4,096 | 2,208 / 4,096 |

The private requirement falls **42.038%**. This is compiler metadata, not a
measured reduction in model memory or physical traffic. Native and L0 each
pass **32 complete byte comparisons** for T=1..8, one/eight weight banks and
residual OFF/ON, including floating-point, residual and Q8 outputs and guards.

Six clean forward arms bracket each backend's candidate with its unchanged
controls. Each process discards ten warmups and retains ten requests:

| Forward Short | Old control, geometric mean | Candidate | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Native decode | 48.451 tok/s | 49.231 tok/s | +1.610% | -1.081% |
| L0 decode | 48.331 tok/s | 48.143 tok/s | -0.390% | -0.177% |
| Native TTFT | 912.001 ms | 902.937 ms | -0.994% | +0.924% |
| L0 TTFT | 906.782 ms | 912.022 ms | +0.578% | -0.111% |

Both candidates use **the same new mathematical pack**. Their forward
comparison is native **49.231** versus L0 **48.143 tok/s (+2.261%)**. A separate
L0-candidate/native-candidate/L0-candidate bracket reuses those exact binaries
and math: native **48.857** versus geometric L0 **48.660 tok/s (+0.405%)**,
with **+0.250% L0 control drift**. Reverse native TTFT is **907.640** versus
L0 **905.722 ms (+0.212%)**, so its direction changes. The two decode
directions are favorable, but the magnitude is not stable enough to advertise
a repeatable 2% win. The small L0 ablation change is also not established as
a persistent regression. Raw ranges and median absolute deviations are saved.

All **180 fresh model requests** have the same 76 tokens, 57/57 drafts,
natural stop and zero reuse/resume. Policy stays at 5,771 slots, reserve
1,536 MiB, MTP4, step/last min-p 0.5 and cumulative floor zero. Decode excludes
preparation, initialization and prefill; TTFT is separate. No GPU timestamps,
inner timing probes or telemetry run, and clocks remain unlocked. The model
uses the later frozen BCS-fill native runtime and matching expert schedule,
with the **older norm kernel** fixed in both arms; this is not a benchmark of
the current production configuration or of Linux GPU execution.

The first closure-size assertion mistake and an initial model Host-header
ABI mismatch are preserved. The latter exited during initialization; its
40 completed controls are excluded from the fresh comparison. Rebuilding
the changed Host object with each model's own Queue/Event/Runtime headers
fixes that integration error. Candidate code stays isolated; production
source/defaults/binaries and immutable Linux handoffs are preserved.
[All measurements and policy](../orca-l0/demo/dg2-hc-up-capture-windows-2026-11-04.json)
and 463 evidence files（本地归档：`../orca-l0/demo/dg2-hc-up-capture-windows-2026-11-04.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_hc_up.py --verify`.

### Q8-up capacity1/4 follow-up

This isolated candidate selects a one-row capture for T=1 and a four-row
capture for T=2..4, retaining the eight-row path for T=5..8. Arithmetic loops,
reductions, barriers and launch geometry stay fixed. All **25 existing capture
ABIs and 1,242 existing model zebins** are retained; only two new images and
their index entries are added. Controls are the preceding validated eight-row
candidate pair. Each changed Host object uses its own exact model headers.

| Q8-up capture | Argument bytes | Full closure bytes | Private bytes / hardware thread | Reported spill bytes |
| --- | ---: | ---: | ---: | ---: |
| Eight-row control / fallback | 728 | 736 | 23,296 | 2,208 |
| Capacity1 | 112 | 120 | 3,584 | 2,112 |
| Capacity4 | 376 | 384 | 12,032 | 2,080 |

All entries use SIMD32 and 128 GRFs; rounded scratch remains **4,096 bytes**.
Private/spill values are compiler metadata, not measured physical traffic.
Native and L0 each pass **32 canonical T=1..8 output/guard checks**, covering
one/eight banks and residual OFF/ON.

| Forward Short | Eight-row control, geometric mean | Capacity candidate | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Native decode | 48.807 tok/s | 48.642 tok/s | -0.340% | -0.341% |
| L0 decode | 49.411 tok/s | 48.485 tok/s | -1.875% | +2.445% |
| Native TTFT | 914.336 ms | 911.156 ms | -0.348% | -0.095% |
| L0 TTFT | 888.253 ms | 906.731 ms | +2.080% | -2.426% |

With identical new math, forward native/L0 decode differs **+0.323%**.
A separate L0/native/L0 bracket reuses the same candidate binaries and pack:
native **50.145** versus geometric L0 **50.140 tok/s (+0.010%)**, below
**+0.251%** control drift. Reverse native/L0 TTFT is **883.029 / 876.508 ms
(+0.744%)**. This does not establish an incremental speed gain or a stable
native advantage. The apparent L0 ablation regression also needs to be read
alongside its larger control drift. Absolute rates shift between windows;
another experiment holds the shared lease between them, with no overlap.

All **180 fresh model requests** match 76 output tokens, 57/57 drafts, natural
stop and zero reuse/resume. Each process discards ten warmups and retains ten
requests. Policy remains 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p
0.5 and cumulative floor zero. Decode excludes preparation, initialization
and prefill; TTFT is separate. No inner timing probes, GPU timestamps or
telemetry run; clocks remain unlocked. The frozen older norm and BCS-fill /
expert-fork schedule are preserved, with no production/default adoption or
Linux GPU claim. The first capacity build's source-hash assertion failure
occurred before Host compilation/GPU and is archived separately from results.
[Measurements, policy and dispersion](../orca-l0/demo/dg2-hc-up-capacity-windows-2026-11-04.json)
and 484 evidence files（本地归档：`../orca-l0/demo/dg2-hc-up-capacity-windows-2026-11-04.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_hc_up_capacity.py --verify`.

### IQ paired-GU format split

The cached kernel profile identifies `gu_pair_wg128` as a substantial target.
It contains both IQ2_S and Q2_0 decoders behind a uniform format branch. This
isolated candidate adds one entry for each format, reusing the original
`gu_pair_kernel` and its **4/2/1-entry chunks**, sum order and WG128 geometry.
All **252 existing capture ABIs and 1,242 existing model zebins** are retained.
`ORCA_IQ_PAIR_FORMAT_SPLIT=1` selects the new entries; its lookup is cached once
per process and the isolated candidate defaults to the original path.

| GPU entry | Machine-code bytes | SIMD / GRFs | Private / spill bytes |
| --- | ---: | ---: | ---: |
| Original combined format entry | 61,824 | 16 / 128 | 0 / 0 |
| IQ2_S entry | 38,464 | 16 / 128 | 0 / 0 |
| Q2_0 entry | 23,936 | 16 / 128 | 0 / 0 |

These are per-entry sizes. The pack appends two images and retains the old
entry, so aggregate pack size is not reduced. No physical instruction-fetch
or traffic reduction is claimed from these figures.

Each backend compares **192 complete old/new canonical buffers**, including
outputs, scratch, inactive rows and guards. Checks cover both formats,
T=1..8, ready OFF/ON, six changing plans and serial/staged/independent replays:
**4,608 internal comparisons and 5,504 GPU completion epochs** in total.
Four expected missing-entry failures prove dispatch requests the new entries.
IQ2_S uses 16 GU lanes and Q2_0 uses eight for numerical coverage; the model
retains its frozen 16-lane policy. Only Host objects compiled with each exact
model's command and headers are reused; the prior eight-row Q8-up object stays
in each kernel library.

| Forward Short | Eight-row control, geometric mean | Format candidate | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Native decode | 50.070 tok/s | 49.787 tok/s | -0.565% | -0.095% |
| L0 decode | 49.786 tok/s | 49.821 tok/s | +0.070% | +0.029% |
| Native TTFT | 883.387 ms | 884.170 ms | +0.089% | +0.356% |
| L0 TTFT | 880.567 ms | 877.813 ms | -0.313% | -0.107% |

With identical candidate math, native/L0 forward decode is **-0.069%**.
A separate reverse L0/native/L0 bracket uses the same binaries, pack and
enabled switch: native **50.258** versus geometric L0 **49.920 tok/s
(+0.676%)**, with **+0.312%** L0 drift. Reverse native/L0 TTFT is **876.719 /
878.702 ms (-0.226%)**, with **-0.499%** L0 TTFT drift. Both backend comparison
directions change between windows. This does not establish a stable backend
win or a reason to adopt the format split; the native incremental ablation
has only one forward bracket.

All **180 fresh Shorts** match 76 tokens, 57/57 drafts, natural stop and no
reuse/resume. Each process discards ten warmups and retains ten requests.
Policy remains 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p 0.5 and
cumulative floor zero. Decode excludes preparation, initialization and prefill;
TTFT is separate. No inner probes, GPU timestamps or telemetry run; clocks
remain unlocked. The older norm, eight-row Q8-up and BCS-fill / expert-fork
baseline is frozen. No production/default adoption or Linux GPU result is added.
[Measurements, policy and dispersion](../orca-l0/demo/dg2-iq-pair-format-windows-2026-11-04.json)
and 476 evidence files（本地归档：`../orca-l0/demo/dg2-iq-pair-format-windows-2026-11-04.json.gz`）
preserve raw protocols, commands, source, ABIs, selected zebins and complete
output hashes. Deterministic empty/active ready-OFF buffers for every T,
format and backend are included; the remaining full buffers are retained
locally and regenerated by the archived probe.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_iq_pair_format.py --verify`.

### Paired GU group census: a measured reason to test three-entry reuse

A separate Windows A770 diagnostic counts group lengths directly from the GPU
routing plan. In the last Short, the paired IQ2_S path has **17,882 groups**:
12,469 of length one, 3,309 of length two, **1,323 of length three**, and 781
of length four. These cover 26,180 entries and 680 paired-GU calls. The paired
Q2_0 branch is unused under this model's frozen 16-lane policy; these are not
counts of all expert kernels.

The existing 4/2/1 loop makes 19,205 weight-decoding traversals per output row.
Replacing the length-three 2+1 tail with a three-entry chunk would remove 1,323
traversals (**6.889%**). This is a logical traversal count, not measured memory
traffic, kernel time or an end-to-end speed prediction. The separate E3
experiment below provides numerical and clean performance checks.

The counter runs as an extra diagnostic GPU kernel. Its storage is allocated
before graph recording, enabled after prompt preparation and read after decode.
No routing D2H copy or host handshake is added to clean performance arms.
The diagnostic's extra dispatches, atomics and boundary waits perturb cadence;
its throughput is excluded. All **192 CPU-predicted histogram cases**, **192
canonical comparisons**, 1,152 internal comparisons and 1,376 completion epochs
pass across both formats, T=1..8, ready OFF/ON and changing graph replays.
Eleven fresh Shorts match all 76 tokens, 57/57 drafts, stop and zero reuse/resume.
All 1,242 original mathematical zebins remain byte-identical. Policy and the
older norm/eight-row-up/BCS-fill baseline stay fixed; no adoption or Linux GPU
result is added.
[Counts and reproduction](../orca-l0/demo/dg2-iq-group-hist-windows-2026-11-04.json)
and 192 evidence files（本地归档：`../orca-l0/demo/dg2-iq-group-hist-windows-2026-11-04.json.gz`）.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_iq_group_hist.py --verify`.

### Paired GU three-entry reuse: small native gain, still below L0

The isolated E3 candidate replaces a remainder-three `chunk2 + chunk1` with
`chunk3`, decoding each weight block once for all three entries. Per-entry
arithmetic, reductions, chunk-four support and WG128 geometry stay fixed.
Both new entries retain SIMD16, 128 GRFs, zero SLM/private/spill and a 120-byte
capture. Selected IQ2_S code grows **38,464 to 52,096 bytes**, and Q2_0 grows
**23,936 to 32,384 bytes**; reduced logical weight decoding does not imply a
smaller instruction footprint. All **254 control capture ABIs and 1,244 control
zebins** remain, with two E3 images appended.

| Forward Short | Format-only control, geometric mean | E3 | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Native decode | 49.571 tok/s | 49.815 tok/s | +0.492% | +0.078% |
| L0 decode | 49.941 tok/s | 49.900 tok/s | -0.082% | -0.066% |
| Native TTFT | 886.739 ms | 884.367 ms | -0.267% | -0.023% |
| L0 TTFT | 878.328 ms | 876.765 ms | -0.178% | -0.119% |

With the same E3 math, native decode is **0.170% below L0** in the forward
comparison. A separate L0/native/L0 ordering reuses the exact E3 executables
and pack: native **49.906** versus geometric L0 **49.994 tok/s**, or **-0.176%**,
with L0 drift -0.089%. Reverse TTFT is **880.913 / 877.702 ms (+0.366%)** for
native/L0, with L0 TTFT drift +0.024%. The reverse bracket checks backend
direction; it is not a second incremental E3-versus-format ablation. The small
native improvement has one forward bracket and does not establish a repeatable
incremental gain or native-over-L0 win. **E3 remains isolated and defaults off.**

Both backends pass **384 full control/E3 canonical comparisons**, **4,608
internal comparisons**, **5,504 completion epochs** and four expected
missing-entry failures. Coverage includes both formats, T=1..8, ready OFF/ON,
six changing plans and serial/staged/independent replays. Numerical Q2_0 checks
use eight GU lanes; the model retains its frozen 16-lane policy. Each IQ Host
object uses its exact model's tested command and headers; the prior eight-row
Q8-up Host binding is retained.

All **180 fresh Shorts** match 76 tokens, 57/57 drafts, stop and zero reuse/resume.
Each of nine processes excludes ten warmups and retains ten requests. The
older norm, eight-row Q8-up, BCS-fill and matching expert-fork baseline is fixed,
with 5,771 slots, a 1,536-MiB reserve, MTP4, step/last min-p 0.5 and cumulative
zero. `ORCA_IQ_PAIR_FORMAT_SPLIT=1` and `ORCA_IQ_PAIR_E3=1` select the candidate;
both lookups are cached per process. Decode excludes preparation, initialization
and prefill; TTFT is separate. Clean arms contain no histogram, inner probes,
GPU timestamps or telemetry. Clocks are unlocked; drift is not a confidence
interval. No production/default adoption, new copy/lock census or Linux GPU
result is added.
[Measurements, dispersion and reproduction](../orca-l0/demo/dg2-iq-pair-e3-windows-2026-11-04.json)
and 485 evidence files（本地归档：`../orca-l0/demo/dg2-iq-pair-e3-windows-2026-11-04.json.gz`）
retain raw protocols, commands, source, ABIs, selected images, complete hashes
and deterministic canonical representatives.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_iq_pair_e3.py --verify`.

### Shared three/four-entry GU body: two small Short decode leads

The next candidate gives E3 and E4 one shared computation body, masking the
fourth activation load, arithmetic, reduction and store for a three-entry tail.
It retains E3's 4/3/2/1 grouping and each entry's arithmetic and sum order.
It consumes the existing routed plan and adds no wait to fill a batch.

| Selected entry | Separate E3/E4 bodies | Shared body | Text reduction |
| --- | ---: | ---: | ---: |
| IQ2_S | 52,096 bytes | 38,784 bytes | 25.553% |
| Q2_0 | 32,384 bytes | 24,192 bytes | 25.296% |

Both entries retain SIMD16, 128 GRFs, zero SLM/private/spill and 120-byte
captures. All **256 control capture ABIs and 1,246 control zebins** remain;
two shared-body entries are appended, so aggregate pack size grows. These are
selected text-size measurements, not measured physical instruction-fetch or
memory-traffic reductions. Logical weight-decoding traversals match E3.

| Forward Short | E3 control, geometric mean | Shared body | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Native decode | 50.021 tok/s | 50.276 tok/s | +0.511% | +0.208% |
| L0 decode | 50.135 tok/s | 49.990 tok/s | -0.289% | -0.185% |
| Native TTFT | 883.426 ms | 878.465 ms | -0.562% | -0.722% |
| L0 TTFT | 875.798 ms | 877.979 ms | +0.249% | +0.190% |

With the same shared-body math, native decode is **0.572% above L0** in the
forward comparison. A reverse L0/native/L0 ordering reuses the exact shared-body
binaries and pack: native **50.315** versus geometric L0 **50.125 tok/s
(+0.377%)**, with L0 drift **-0.119%**. Both orderings observe a small native
decode lead for this fixed Short. The incremental shared-body-versus-E3 gain
has one forward bracket; the reverse tests backend direction, not a second
incremental ablation. Unlocked-clock drift is not a confidence interval or a
guarantee for other workloads.

TTFT does not lead L0: forward native/L0 is **878.465 / 877.979 ms (+0.055%)**,
and reverse is **880.126 / 876.646 ms (+0.397%)**, with reverse L0 TTFT drift
+0.132%. These observations establish no prefill or TTFT improvement over L0.

The two backends pass **384 complete E3/shared-body canonical comparisons**,
**4,608 internal comparisons**, **5,504 completion epochs** and four expected
missing-entry failures. Checks cover formats 22/42, T=1..8, ready OFF/ON and six
changing plans through serial/staged/independent replays. The final plan now
fills entry capacity, including three-entry tails at the buffer end for T=2/8.
All **180 fresh Shorts** match 76 tokens, 57/57 drafts, stop and zero reuse/resume.
Each of nine processes excludes ten warmups and retains ten requests. Raw
protocols preserve every request, including lower-throughput outliers.

Each IQ Host object uses its exact model command and headers, retaining the
eight-row Q8-up binding. The older norm, BCS-fill and matching expert-fork
baseline stays fixed, with 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p
0.5 and cumulative zero. Candidates explicitly set `ORCA_IQ_PAIR_FORMAT_SPLIT=1`,
`ORCA_IQ_PAIR_E3=1` and `ORCA_IQ_PAIR_COMPACT=1`; the new lookup is cached once
per process and defaults off. Decode excludes preparation, initialization and
prefill; no histogram, inner probes, GPU timestamps or telemetry run in clean
arms. The candidate remains isolated, with no default adoption or Linux GPU
result. The initial compile failure from placing `constexpr` before the clang
FP pragma is preserved; the sole revision moves it after the pragma, retaining
disabled FP contraction.
[Measurements, dispersion and reproduction](../orca-l0/demo/dg2-iq-pair-compact-windows-2026-11-04.json)
and 499 evidence files（本地归档：`../orca-l0/demo/dg2-iq-pair-compact-windows-2026-11-04.json.gz`）
retain protocols, commands, sources, ABIs, selected images, complete hashes and
canonical representatives. Verification reparses actual ELF text/resources and
checks the failed-build correction as well as the numerical and model results.
Read-only verification: `python build-orca-native-lower-20261031/publish_dg2_iq_pair_compact.py --verify`.

An independent **compact/E3/compact native ablation** now reverses the original
incremental comparison, reusing the exact executables and each original module
pack without recompilation. Another **60 fresh Shorts** pass all 76 output
tokens, 57/57 drafts, stop and zero reuse/resume. Each process again discards
ten warmups and retains ten requests.

| Reversed native increment | E3 center | Shared-body controls, geometric mean | Change | Shared-body drift |
| --- | ---: | ---: | ---: | ---: |
| Decode | 50.127 tok/s | 50.267 tok/s | +0.279% | +0.017% |
| TTFT | 879.503 ms | 880.129 ms | +0.071% | +0.267% |
| Whole request | 2352.455 ms | 2348.955 ms | -0.149% | +0.078% |

Together with the original **+0.511%** native increment, both incremental
orderings observe a small decode improvement over E3. This new bracket does
not compare native against L0. TTFT shows no improvement, and these fixed-Short
observations do not establish a general speed guarantee or justify a default
change. The fixed policy, timing exclusions and unlocked-clock limits above
remain in force; the implementation remains isolated and Linux GPU is untested.
[Independent incremental results](../orca-l0/demo/dg2-iq-pair-compact-increment-windows-2026-11-04.json)
and 19 archived evidence files（本地归档：`../orca-l0/demo/dg2-iq-pair-compact-increment-windows-2026-11-04.json.gz`）
retain all raw protocols, arm environments, commands, input/module/binary hashes
and the link to the preceding full numerical evidence. Verify with
`python build-orca-native-lower-20261031/repeat_dg2_iq_pair_compact_increment.py --verify-published`.

### Minimal shared GU source integration: opt-in, no established overall gain

The numerical source now contains only the compact helper, kernel, launcher
and one cached `ORCA_IQ_PAIR_COMPACT` switch. The earlier experimental format
split and separate E3 branches are not copied into this integration. Rebuild
Host objects and images together, then set **`ORCA_IQ_PAIR_COMPACT=1`** to use
it. Unset or `0` retains the original 4/2/1 GU path. The existing A770, format,
lane and shape eligibility guards remain; no batching wait is added.

This fresh six-process comparison tests the **original 4/2/1 GU scheme versus
the complete minimal shared-body change**, with the same preceding eight-row
Q8-up pack, older norm, BCS-fill and matched expert schedule on both backends.
It is distinct from the incremental shared-body-versus-E3 comparisons above.

| Fixed Short | Original GU control, geometric mean | Integrated shared body | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Native decode | 50.234 tok/s | 50.255 tok/s | +0.041% | -0.109% |
| L0 decode | 49.940 tok/s | 50.003 tok/s | +0.127% | -0.089% |
| Native TTFT | 878.966 ms | 879.789 ms | +0.094% | +0.043% |
| L0 TTFT | 877.574 ms | 874.684 ms | -0.329% | +0.263% |

Native/shared decode is **0.502% above L0/shared** in this ordering; its TTFT
is **0.584% longer**. The very small native gain over original GU is below
the magnitude of native control drift. This does not establish an overall
speed improvement or justify enabling the feature by default. The prior
positive E3 incremental results remain valid for their own controls.

All **120 fresh requests** match 76 tokens, 57/57 drafts, stop and zero reuse/
resume. Each process excludes ten warmups and retains ten requests. Policy
remains 5,771 slots, reserve 1,536 MiB, MTP4, step/last min-p 0.5 and cumulative
zero. Decode excludes preparation, initialization and prefill; TTFT is separate.
No inner probes, GPU timestamps, histogram or telemetry run in clean arms.
Unlocked-clock drift and recorded dispersion are not confidence intervals.

Current-source native compilation preserves **252 old capture ABIs** and
adds two entries. Their complete machine-code text exactly matches the tested
prototype: **38,784 / 24,192 bytes**, SIMD16, 128 GRFs, zero SLM/private/spill
and 120-byte captures. Both backends directly compare original 4/2/1 against
the integrated body across **384 canonical buffers / 4,608 internal checks /
5,504 completion epochs**, including all T=1..8, both formats, ready OFF/ON,
six changing plans and capacity-ending three-entry tails. Four expected
missing-entry failures confirm the new entries are requested.

Four original/candidate **SYCL Host/device compilations** also pass with
current headers and no native overlays: all **261 old SYCL capture ABIs**
remain, with two additions. The first verification wrongly expected the
native count of 252; that failure and all successful compiler outputs are
retained, with independent validation of the actual SYCL exports.

Complete IQ regeneration retains 250 old texts byte-for-byte and all 252
old resource records. Of the two changed old images, fixed mixed GU has an
identical execution body through EOT and eight changed marker bytes after it.
Generic IQ2_S changes zero-initialization order and scheduling tokens as well
as markers. The actual regenerated generic kernel passes **252 bitwise-v1
comparisons at 512 / 1024 / 2048 dimensions**. Raw images and IGA assembly
are archived, so these two raw binaries are not described as identical.

Only `sycl/src/kernels/cuda/iq_kernels.dp.cpp` and the existing runnable
`sycl/orca/expert_schedule_check.cpp` are adopted. The latter now covers all
T=1..8 and full-capacity tails, passing **1,152 full-buffer checks / 1,376
completion epochs** with the new IQ Host object. Runtime sources, production
executables/configuration, root README and immutable Linux bundles are preserved;
Linux GPU execution remains deferred.
[Exact adopted sources, samples and reproduction](../orca-l0/demo/iq-compact-source-windows-2026-11-04.json)
and 528 evidence files（本地归档：`../orca-l0/demo/iq-compact-source-windows-2026-11-04.json.gz`）.
Verify with `python build-orca-native-lower-20261031/adopt_iq_compact_source.py --verify`.

### V11 command buffers in local memory: no measured gain

A new isolated candidate mirrors command buffers of at least 16 KiB into local
VRAM at close. Bootstrap and short eager lists retain their host storage;
immutable replay adds no upload. Mathematical kernels, indirect-parameter
policy, owner handoffs, waits and model settings stay fixed. This tests command
fetch placement separately from V11's already-adopted parameter mirrors.

The clean L0 / V11 / candidate / V11 / L0 bracket completes **100 exact Short
requests**. Each arm excludes ten warmups and retains ten requests, with
preparation, initialization and prefill outside decode. All 76 output tokens,
natural stops and 57/57 drafts match, using 5,771 expert slots, a 1,536-MiB
reserve, MTP4, step/last min-p 0.5 and the same 1,242 mathematical zebins.

| Current bracket | Decode tok/s | TTFT ms |
| --- | ---: | ---: |
| V11 host commands, geometric adjacent controls | 47.507 | 894.888 |
| Local-command candidate | 47.374 | 897.092 |
| L0, geometric adjacent controls | 48.172 | 891.801 |

Candidate decode changes **-0.279%**, within the native controls' **-0.471%**
drift; L0 control drift is -0.047%. TTFT changes +0.246%, with +0.260% native
TTFT drift. These unlocked-clock observations establish no benefit or stable
regression. The candidate remains **1.657% below L0** in this bracket and is
**not adopted**. They do not support command-buffer placement as a large
remaining optimization for this fixture. V11 production source, executables,
configuration and Linux handoffs remain unchanged.

The candidate passes 15 Windows checks, 24 guarded command-placement replays
on two queues, 64 submissions while recording new work, and the device,
scratch and private-memory numerical groups. The 512/1/512-walker check
exercises large mirrors, small-list fallback, replay and pool reuse. No new
copy/lock census or Linux GPU result is claimed.
[Result](../orca-l0/demo/dg2-local-commands-windows-2026-11-03.json) and
frozen source, runner, commands, raw protocols and checks（本地归档：`../orca-l0/demo/dg2-local-commands-windows-2026-11-03.json.gz`）
retain the rejected candidate. Read-only verification:
`python build-orca-native-lower-20261031/publish_dg2_local_commands.py --verify`.

### Tail-barrier ownership: no measured gain

An isolated candidate removes the adapter's explicit barrier immediately
before `finish()`. Windows `Runtime::close()` already owns a final host-visible
barrier; the candidate adds the corresponding barrier to Linux `close()`.
Only that Linux line and the adapter change. Windows runtime, all other
headers, mathematical modules, parameters, owner handoffs and model policy
remain fixed. The experiment does not change operator parallelism.

All **80 Host objects** are rebuilt against the candidate headers; the
interrupted serial build retains 24 validated objects and four compiler
workers complete the rest. Fifteen Windows checks, the actual rebuilt adapter
queue checks at batch limits **64 and 1**, 64 inflight-recording submissions,
device/scratch/private/inline-event checks and Linux **three CPU checks** pass.
These do not establish Linux GPU correctness or desktop coexistence.

The L0 / V11 / candidate / V11 / L0 bracket completes **100 exact Short
requests**, with ten warmups and ten retained requests per arm. Every full
76-token output, natural stop and 57/57 drafts match. The 1,242 mathematical
zebins, 5,771 expert slots, 1,536-MiB reserve, MTP4, step/last min-p 0.5,
cumulative floor zero and no prefix reuse remain fixed. Decode excludes
preparation, initialization and prefill; no inner timers or GPU timestamps run.

| Tail-barrier bracket | Decode tok/s | TTFT ms |
| --- | ---: | ---: |
| V11, geometric adjacent controls | 47.522 | 894.906 |
| Candidate | 47.359 | 897.651 |
| L0, geometric adjacent controls | 48.357 | 886.212 |

Candidate decode changes **-0.343%**, with **+0.301%** native control drift;
L0 control drift is -0.134%. TTFT changes +0.307%, with -0.297% native TTFT
drift. These unlocked-clock observations establish no gain or stable
regression. The candidate is **not adopted**. V11 source, production binaries,
configuration and immutable Linux handoffs stay unchanged. Existing copy/lock
censuses remain historical; this experiment adds no new copy/lock measurement.
[Result and fixed parameters](../orca-l0/demo/dg2-tail-barrier-windows-2026-11-03.json)
and 364 checked source, command, raw protocol and test records（本地归档：`../orca-l0/demo/dg2-tail-barrier-windows-2026-11-03.json.gz`）
preserve the candidate and interrupted/resumed build evidence. Read-only check:
`python build-orca-native-lower-20261031/publish_dg2_tail_barrier.py --verify`.

The new **same-pages and same-GPU-address GDN control** still finds a large
working-set GPU gap. Both command paths now read/write the same L0-owned local
allocation, and a staged native alias also uses the exporter's exact GPU VA.
For T=1 with 16 independent slots, pure GDN takes **3.59% / 3.68% longer** on
native in two bracket orderings. Data allocation and address differences alone
therefore do not remove this gap. Internal ISA/scratch/command allocations and
contexts still differ; this does not identify a single remaining cause.

### Parameter placement explains most of the large GDN fixture gap

The context crossover now also relocates immutable indirect parameters. The
native batch's ISA allocation, data pages/GPU VA, real L0 context, barriers,
timestamps and all other command fields stay fixed. Two owned command copies
change only the general-state base and walker parameter offsets. Their host
and local parameter images are byte-identical, including full padding guards;
local GPU readback and original captured parameters are checked after every
arm. A second matrix preserves **every source-relative offset and reused
payload address**, avoiding duplicated parameters for repeated launches.

| GPU interval with original parameter reuse; forward / reversed | Original host parameters us | Local parameters us | Ordinary L0 us |
| --- | ---: | ---: | ---: |
| T=1, one slot, GDN only | 157.318 / 157.428 | 157.292 / 157.474 | 160.677 / 160.716 |
| T=1, 16 slots, GDN only | 339.434 / 339.831 | 329.505 / 330.143 | 328.581 / 328.913 |
| T=1, 16 slots, mixed | 365.293 / 365.879 | 355.573 / 356.081 | 361.243 / 361.706 |

The large pure interval falls **2.92% / 2.85%**, leaving **0.28% / 0.37%**
longer GPU time than L0. The mixed interval falls **2.66% / 2.68%** and is
**1.57% / 1.56% shorter** than L0. Merely relocating the host parameters changes
large pure time by -0.075% / -0.031%. Small pure GDN stays within -0.017% /
+0.029% of native controls. Parameter storage therefore explains most of this
specific large-workset gap; these intervals are not model throughput or a
measurement of physical PCIe traffic.

The first matrix duplicated payloads per walker and regressed small pure
native GDN by 2.56% / 2.57% with local parameters, demonstrating why preserving
reuse matters. Both matrices total **576 guarded cases / 17,280 retained
samples**, with the original FP32 scalar oracle and frozen FP32/Q8 hashes.
They retain post-timestamp GRF128 normalization, 200-ms conditioning, ten
warmups, 30 samples and opposite orderings. One preflight assertion failure is
preserved. This is a causal follow-up to the earlier 2026-11-02 local-mirror
implementation and model trial, not a first implementation claim. Production
source was left at V10 by this diagnostic; the model controls follow below.
[Parameter placement controls](../orca-l0/demo/dg2-parameter-placement-windows-2026-11-03.json)
and 837 evidence files（本地归档：`../orca-l0/demo/dg2-parameter-placement-windows-2026-11-03.json.gz`）
retain all packet changes, parameter images, exact sources and raw samples.

### Current-source parameter mirrors: repeatable 0.98% decode gain

The earlier batched parameter mirrors are now rebased onto frozen V10. The
host/local builds differ only in the Windows C0 constructor enabling local
parameters. Both keep immutable host captures, their existing weak cache and
completion ownership; new ranges merge by slab at close, with separate
bootstrap pools. Immutable graph replay performs no parameter uploads.

| Clean Short bracket | Host parameters tok/s | Local parameters tok/s | L0 tok/s | Local vs host | Local vs L0 |
| --- | ---: | ---: | ---: | ---: | ---: |
| L0 / host / local / host / L0 | 46.490 | 46.947 | 47.671 | +0.982% | -1.518% |
| L0 / local / host / local / L0 | 46.562 | 47.020 | 47.702 | +0.984% | -1.431% |

Each arm has ten warmups and ten retained requests. Adjacent repeated controls
use their geometric mean. The repeated parameter controls drift +0.067% and
+0.074%; the L0 controls drift -0.035% and +0.488%. Clocks are unlocked, so
these are observed brackets, not confidence intervals. All 200 requests match
the complete 76-token output, natural stop and 57/57 drafts, using the same
80 frozen host objects, 1,242 mathematical zebins, 5,771 slots, 1,536-MiB
reserve, MTP4, step/last min-p 0.5, cumulative floor zero and no prefix reuse.
Decode excludes preparation, initialization and prefill; no inner timers,
GPU timestamps or telemetry are enabled.

TTFT is **912.913 vs 910.501 ms (+0.265%)** in the first bracket and
**906.745 vs 916.189 ms (-1.031%)** in the reverse bracket. Whole-request time
falls 0.469% and 0.994%. These TTFT observations do not establish a stable
prefill gain. The 0.98% decode result isolates parameter policy within the
new pair of builds; it does not measure the full combined patch against
unchanged V10, and native still has no full-model win over L0.

Each build passes 15 Windows checks and eight numerical/lifecycle groups,
including 17,000 immutable captures, 900 reclamation submissions and 64
inflight recordings. The Linux runtime compiles and passes three CPU checks;
its parameters remain host-backed, and Linux GPU testing is deferred. One
failed capture-check invocation is preserved with the corrected run. The
exact four validated source files are now adopted as V11; production
binaries, configuration, root README, MTP work and immutable Linux handoffs
are preserved.
[First bracket, exact sources and lifecycle checks](../orca-l0/demo/dg2-local-parameters-current-windows-2026-11-03.json)
archive 309 evidence files.

The final **V10 / local / V10** control tests the complete patch against the
unchanged adopted V10 executable, rather than only isolating policy between
the new builds. Its 60 exact requests give **46.409 vs 47.036 tok/s (+1.350%)**;
V10 control drift is **-0.697%**. TTFT is 917.311 vs 905.982 ms (-1.235%),
with +1.311% TTFT control drift. These single-bracket observations retain the
unlocked-clock limitation; they are not a new throughput anchor or a stable
prefill claim.

Fresh probes count both candidate and L0 in separate payload and lock
executables. All **11 paired logical-payload censuses** match, including every
graph replay inside decode; all 44 counter requests match the full output.

| Final warmed decode census | V11 native | L0 |
| --- | ---: | ---: |
| D2D operations / bytes | 134 / 4,554,752 | 134 / 4,554,752 |
| H2D operations / bytes | 659 / 968,318,976 | 659 / 968,318,976 |
| Imported application mutex acquisitions | 130 | 130 |
| Foreground / background I/O mutex acquisitions | 40 / 90 | 40 / 90 |
| Runtime mutex / shared SRW / exclusive SRW acquisitions | 0 / 0 / 0 | 0 / 0 / 0 |

Both lock probes attribute every imported mutex call to PLE/DirectFile, with
15 keepalive reads and no business SSD reads in the observed final decode.
Keepalive stays at 100 ms; background totals may change with request duration.
The copy probe's own diagnostic locks are excluded by using a separate lock
executable. These counters measure submitted logical payloads and observable
application imports, not physical bus traffic, private/inlined/driver locks
or contention. Native still uses 136 submitted lists versus 97 L0 API graph
replays; those are different API levels, not directly comparable KMT counts.

[Three clean brackets and current-source counters](../orca-l0/demo/dg2-local-parameters-current-repeat-windows-2026-11-03.json)
and 113 follow-up evidence files（本地归档：`../orca-l0/demo/dg2-local-parameters-current-repeat-windows-2026-11-03.json.gz`）
retain 260 clean and 44 counter requests plus the first archive.
[V11 source adoption and 12 current-source hashes](../orca-l0/demo/dg2-local-parameters-current-main-windows-2026-11-03.json)
can be checked without a GPU using
`python build-orca-native-lower-20261031/adopt_dg2_local_parameters_current.py --verify`.

### Same native batch on a real L0 context

A process-local diagnostic now executes the **identical native batch bytes,
ISA allocation, parameter heap and data pages/GPU VA** through both the native
and real L0 contexts. A guarded two-jump trampoline calls the immutable native
batch, then resumes the untouched L0 submission and completion tail. The
original L0 context, flags, private metadata and submission sequence remain.
Before timing, an independent 4,099-byte copy checks four changed-data phases,
complete 64-KiB source/destination guards and the trampoline padding.

The GDN probe resets all data for every arm, conditions for 200 ms, discards
ten warmups and retains 30 samples. Two opposite bracket orders reuse one
binary. One 128-GRF `advance` kernel runs **after the end timestamp on every
arm** to keep hardware GRF state consistent with the L0 driver's cached state
for subsequent copy/dummy work. This normalization is excluded from the GPU
interval, but included in completion and checked marker counts. The measured
interval contains 16 GDN launches, with optional interleaved advance launches.

| GPU interval, forward / reversed | Native context us | Same native batch on L0 context us | Ordinary L0 us |
| --- | ---: | ---: | ---: |
| T=1, one slot, GDN only | 157.350 / 157.350 | 157.305 / 157.253 | 160.723 / 160.638 |
| T=1, 16 slots, GDN only | 340.078 / 341.207 | 339.909 / 340.065 | 329.245 / 329.082 |
| T=1, 16 slots, mixed | 366.601 / 365.820 | 365.729 / 365.690 | 361.556 / 361.387 |

Changing context/submission path alone does not remove the large-workset gap:
the borrowed-context native batch still takes **3.24% / 3.34% longer** than
ordinary L0 in pure T=1. The reverse native control drifts -0.69%; the small
native-context difference is not a stable gain. This narrows the remaining
search to command/state programming and internal allocations, without proving
one cause. It is a standalone control, not a new Short throughput result.

A separate nine-arm control primes the complete native code on the same L0
context, then clears either the first instruction-invalidate bit or all four
initial state/constant/texture/instruction invalidation bits in owned command
copies. Independent raw-DWORD checks require exactly one changed word per
copy; stall/HDC/untyped/DC, double SBA, barriers and all other bytes remain.
This differs from adopted V9, which narrows invalidations on intra-list pure
GRF transitions while retaining initial programming.

| T=1, 16 slots, GDN only; forward / reversed | Candidate GPU us | Change versus adjacent full-invalidation controls |
| --- | ---: | ---: |
| Clear initial instruction invalidation | 339.870 / 338.997 | -0.027% / -0.071% |
| Clear all four initial invalidations | 339.714 / 338.828 | -0.013% / -0.132% |

The four-bit candidate still takes **3.27% / 3.18% longer** than its L0
controls. Initial invalidation alone therefore does not explain the gap.
There is no production adoption: mutable code and context lifecycle require
their own invalidation policy, and this experiment does not establish a model
benefit. Both probes total **448 guarded cases / 13,440 retained samples**;
the original FP32 scalar oracle, full guards and frozen FP32/Q8 output hashes
pass. Q8 parity is not an independent quantization oracle. Both staged native
builds pass seven Windows checks; two compile failures before GPU execution
are retained. Linux GPU testing remains deferred. V10 source, production
binaries/configuration, MTP work, root README and Linux bundles are preserved.
[Context crossover and first-invalidation results](../orca-l0/demo/dg2-context-cross-windows-2026-11-03.json)
and 746 evidence files（本地归档：`../orca-l0/demo/dg2-context-cross-windows-2026-11-03.json.gz`）
include exact sources, libraries, executables, raw samples and packet dumps.
Read-only check:
`python build-orca-native-lower-20261031/publish_dg2_context_cross.py --verify`.

### Same physical data and GPU VA: controlled GDN comparison

Each regular list contains 16 frozen SIMD16 GDN verification launches, with
or without an interleaved `advance` kernel. The verification state stays
read-only (`n_keep=null`). One process loads both runtimes. Native-owned data,
native-remapped shared data, native data at the exporter's original VA and L0
are compared within each fixture. Two bracket orderings reuse the same binary.
Every arm first resets the complete inputs, outputs, packed-Q8 region and
padding; it then conditions for 200 ms, discards ten warmups and retains 30
samples. Uploads/readbacks and CPU reference checks stay outside GPU timing.

| GPU interval, forward / reversed controls | L0 us | Native at same data VA us | Native time change |
| --- | ---: | ---: | ---: |
| T=1, one slot, GDN only | 160.814 / 160.814 | 157.826 / 157.617 | -1.86% / -1.99% |
| T=1, one slot, mixed | 189.844 / 189.961 | 184.714 / 184.375 | -2.70% / -2.94% |
| T=1, 16 slots, GDN only | 329.447 / 329.538 | 341.289 / 341.680 | +3.59% / +3.68% |
| T=1, 16 slots, mixed | 361.699 / 361.751 | 368.945 / 367.682 | +2.00% / +1.64% |
| T=4, one slot, GDN only | 311.393 / 311.497 | 308.125 / 308.203 | -1.05% / -1.06% |
| T=4, one slot, mixed | 338.691 / 338.600 | 332.708 / 332.695 | -1.77% / -1.74% |
| T=4, 16 slots, GDN only | 504.993 / 504.909 | 518.815 / 516.628 | +2.74% / +2.32% |
| T=4, 16 slots, mixed | 557.832 / 557.630 | 563.867 / 571.706 | +1.08% / +2.52% |

Values are medians of two fixture-phase metrics; controls use the geometric
mean of before/after medians. T=4 large native controls drift by up to 2.67%
within one phase, so small changes there need care. All phase metrics and
samples remain in the archive. The earlier same-pages/different-VA control
also retains the large-workset gap. Neither experiment measures model speed,
physical bus traffic, or Linux GPU execution.

The first import failed because the installed driver's exported handle had
access mask `0x1`: even the exporter device's query returned `0xc0000022`.
A separate handle to this process's own resource, requesting the SDK's
`SHARED_ALLOCATION_ALL_ACCESS` (`0xF0001`), allows both exporter and sibling
devices to query it. The original L0 export stays unchanged. This establishes
that the second access mask works here, without identifying the minimum
sufficient rights or a general external-memory limitation. No global security,
privilege, display or driver setting changes.

Both the remapped and same-VA imports pass 61 alternating cross-runtime GPU
updates with full 3-MiB guards and exporter use after borrower teardown. The
GDN controls total **384 guarded cases / 11,520 retained samples**; every
corresponding FP32/Q8 dump matches byte for byte across all five/seven arms
and both orderings. FP32 passes the original scalar oracle; Q8 has finite and
parity checks. Both staged runtimes pass seven Windows checks. The first import
failure and a corrected alias-prototype compile error are preserved. Import
and alias APIs remain diagnostic-only; current source stays V10 and the latest
clean model comparison stays **46.659 versus 48.171 tok/s**.
[Results, phase metrics and limitations](../orca-l0/demo/dg2-shared-pages-windows-2026-11-03.json),
588 source/log/binary/output evidence files（本地归档：`../orca-l0/demo/dg2-shared-pages-windows-2026-11-03.json.gz`）.
Read-only verification:
`python build-orca-native-lower-20261031/publish_dg2_shared_pages.py --verify`.

An additional current-V10 check follows the **actual executed GDN entry**:
the observed instruction base plus walker entry identifies one live code
allocation after a completed 16-launch replay. Both native and L0 use the
same 192-byte hardware-local-ID prologue skip. GPU readback of the complete
**30,592-byte text is byte-identical** between both paths and the input zebin.
This supplements the earlier loaded-pack check; it rules out different code
bytes for this executed fixture, without equating internal heaps or contexts.
The reader passes two changed-pattern checks, interior reads, destination
guards and invalid-range rejection. All 64 GDN cases and seven Windows checks
pass. The first compile failure is retained, and the read API remains staged.
No model or timing result is derived from this instrumented run.
[Executed-address readbacks and limitations](../orca-l0/demo/dg2-executed-isa-windows-2026-11-03.json),
221 evidence files（本地归档：`../orca-l0/demo/dg2-executed-isa-windows-2026-11-03.json.gz`）.
Read-only check:
`python build-orca-native-lower-20261031/publish_dg2_executed_isa.py --verify`.

Two further controls keep one native queue, its code allocations, the shared
data pages and GPU VA fixed. First, a four-policy cross compares baseline,
L1 write-back alone, preferred 16-KiB SLM alone and both. Exact before/after
command dumps independently verify that only the two selected fields change;
actual SLM size, numerical ISA, geometry and barriers stay fixed. Each
candidate has adjacent native controls and outer L0 controls; two opposite
variant orderings reuse the same binary. Every arm resets all data, conditions
for 200 ms, discards ten warmups and retains 30 samples.

For T=1 with 16 slots, the combined policy increases pure-GDN GPU time by
**0.89% / 0.71%** and mixed-GDN time by **1.23% / 1.12%**. Cache alone changes
the pure interval by +0.20%/+0.14%; SLM alone by +0.20%/+0.08%. None removes
the gap. One T=4 large-control phase drifts by -3.83%, so that phase's apparent
benefit is not stable evidence. All **288 guarded cases / 8,640 samples**
pass, including frozen-fixture output hashes. The first diagnostic parser
rejected a valid single-DWORD pipeline-select packet before any timed arm;
the corrected attempt and failure are both archived. No source adoption.
[Cache/SLM cross and raw evidence](../orca-l0/demo/dg2-cache-slm-cross-windows-2026-11-03.json),
552 evidence files（本地归档：`../orca-l0/demo/dg2-cache-slm-cross-windows-2026-11-03.json.gz`）.

Second, actual captured timestamp packets have CS stall in both paths, but
native additionally sets HDC, untyped and DC flush; L0 clears these three
bits. A marker-only control clears them on exactly the two native timestamp
packets. All ordinary barriers and queue-tail flushing remain. T=1 pure GDN
with 16 slots changes **340.605 to 339.896 us**, versus L0 **328.984 us**;
the reversed bracket gives **340.384 to 339.831 us**, versus **328.906 us**.
The marker cost is only **0.21% / 0.16%** here: native still takes **3.32%**
longer in both orders. All **160 guarded cases / 4,800 samples** pass, and
independent raw-DWORD checks prove only the intended timestamp bits change.
Each staged runtime passes seven Windows checks. These are standalone GPU
intervals, not new model speed results. The clean model has no GPU markers,
so this does not explain its 3.14% native deficit. Source remains V10.
[Timestamp-scope calibration](../orca-l0/demo/dg2-timestamp-scope-windows-2026-11-03.json),
299 evidence files（本地归档：`../orca-l0/demo/dg2-timestamp-scope-windows-2026-11-03.json.gz`）.
Both publishers support read-only `--verify`.

The preceding **v9 C0 SBA/invalidation interaction fix**, on top
of v8's compiler launch-state correction. Pure GRF changes omit redundant
double-SBA and four invalidation bits together; all other synchronization and
the full initial/coupled/fallback path remain. Two opposite clean Short brackets
observe **+0.79% and +0.41%** decode versus v8, with respective control drifts
**+0.25% and -0.19%**. Native still runs **2.95%/2.87% below L0** in these
rounds. A real mixed-GDN probe improves 50.40% at T=1; its small local advantage
over L0 repeats at 2.69%/2.18%, without implying that gain for the model.
Windows 15 checks, Linux three CPU checks, eight GPU numerical groups and all
200 exact model requests pass. Linux GPU remains untested. Only encoder and
checker sources are adopted; production executables/configuration and immutable
Linux handoffs are preserved. [v9 source adoption and limits](../orca-l0/demo/dg2-sba-invalidation-main-windows-2026-11-03.json).
The historical v9 verifier expects its own v9 live source; use the v10 verifier above for current sources.

The preceding **v8 compiler launch-state fix** initializes
the denorm control bits clear, as IGC's kernel prologue expects. This corrects
explicit-flush FP32 kernels; it preserves the frozen model's own denorm-retention
instructions. The clean 100-request Short comparison gives **47.049 / 47.130 /
48.628 tok/s** for native control / fix / L0. The observed **+0.17%** is below
**+1.08%** native control drift and establishes no stable speedup. Native remains
**3.08% slower than L0** in this round. Windows 15 checks, Linux three CPU checks
and an old-fails/new-passes GPU regression pass; Linux GPU execution is untested.
Production executables/configuration and immutable Linux handoffs are not
replaced. [Compiler-contract trial](../orca-l0/demo/dg2-denorm-contract-windows-2026-11-03.json)
and [v8 source adoption](../orca-l0/demo/dg2-denorm-fix-windows-2026-11-03.json).

A newer **100-request SBA L1 writeback interaction trial** keeps v8's adopted
scratch writeback, dispatch and floating-point modes fixed. Native control /
SBA-WB candidate / L0 measure **46.172 / 46.203 / 48.001 tok/s**. The candidate's
**+0.07%** is below **+0.44%** native control drift; L0 drift is -0.33%.
TTFT is **908.370 / 906.494 / 891.190 ms**. No stable gain is established, and
the candidate remains **3.75% below L0**, so SBA-WB is not adopted. All 100
requests match 76 tokens, stop and 57/57 drafts. Windows 15 checks, six GPU
numerical groups, both denorm regressions and Linux three CPU checks pass.
Decode excludes preparation/init/prefill; policy, 80 Host objects and 1,242
mathematical zebins stay fixed. Source remained v8 at that checkpoint. This tests interaction
with scratch writeback, rather than relabeling the earlier v5 SBA experiment.
[Trial and source snapshots](../orca-l0/demo/dg2-sba-wb-v8-windows-2026-11-03.json)
and 147 evidence files（本地归档：`../orca-l0/demo/dg2-sba-wb-v8-windows-2026-11-03.json.gz`）
include raw protocols and exact commands. Read-only verification:
`python build-orca-native-lower-20261031/publish_dg2_sba_wb_v8.py --verify`.

A new **standalone GDN kernel diagnostic** separates repeated GPU work from
submission/wait wall time. It loads the same two frozen Strata verification
zebins through L0 and native v8. Current SIMD16/Q8-tail settings use 256 GRFs,
3,136 bytes of SLM and no declared spill; the SIMD32 control needs 4-KiB
scratch. Two opposite brackets reuse the same binaries and test host/local
data, T=1/4 and 1/16 launches, with two overwritten input phases.

| SIMD16, local data, 16 launches: GPU time | L0 | Native v8 | Native time change |
| --- | ---: | ---: | ---: |
| T=1, L0/native/L0 bracket | 162.661 us | 158.372 us | -2.64% |
| T=1, native/L0/native bracket | 163.203 us | 160.181 us | -1.85% |
| T=4, L0/native/L0 bracket | 313.568 us | 309.219 us | -1.39% |
| T=4, native/L0/native bracket | 314.115 us | 309.818 us | -1.37% |

These are medians of two fixture-phase medians. GPU control drift is -0.63% /
+0.53% for the two T=1 rows and -0.08% / -0.08% for T=4. Single-launch local
GPU time is instead 3.15%-4.59% longer on native across T=1/4 and both brackets.
Host-data cases do not show a similarly stable advantage. Submit-and-wait wall
time remains longer even in the faster local GPU cases: **223.400 / 247.850 us**
(L0/native, T=1 first bracket) and **403.700 / 465.189 us** (T=4 reverse).
This does not identify pure CPU cost or the dominant cost of full-model decode.

All **192 case runs** pass scalar output-reference and complete input/state/
padding/timestamp guard checks. All 32 FP32-output/packed-Q8 fixtures match
byte-for-byte across six arms, with maximum scalar-reference error
**6.333e-7**; there are **5,760 retained samples**, plus warmup and conditioning.
The probe leaves state unchanged, as in verification; commit kernels, mixed
kernel transitions and the model working set are outside this measurement.
Repeated real GDN GPU work can therefore favor native, but an end-to-end native
win is still unestablished. No runtime/default/source adoption is made.
[Results and all phase summaries](../orca-l0/demo/dg2-gdn-kernel-windows-2026-11-03.json)
and 243 verified evidence files（本地归档：`../orca-l0/demo/dg2-gdn-kernel-windows-2026-11-03.json.gz`）
include both runners, exact commands, binaries' hashes, frozen source, original
samples and numerical output dumps. Verify without running GPU work:
`python build-orca-native-lower-20261031/publish_dg2_gdn_probe.py --verify`.

The preceding **v7 scratch-surface L1 writeback** update was calibrated
for A770 C0 / Windows driver 32.0.101.9034. Clean Short improves **44.222 →
45.714 tok/s (+3.37%)**, with **+0.31%** native control drift; the same round's
L0 is **47.024 tok/s**, so native remains **2.78% slower**. TTFT falls **1.67%**.
The executable/configuration is not replaced. The older measurements below
retain their original sources and conditions.
[Latest source adoption and samples](../orca-l0/demo/dg2-scratch-cache-c0-windows-2026-11-02.json).

The separate v7 counter probe measures **130 application mutex acquisitions
for both native and L0**, with zero runtime mutex/SRW acquisitions. Fresh native
D2D remains **134 operations / 4,554,752 bytes**, matching the earlier L0
fixture reference. Background keepalive remains enabled; equal total counts
are observed in this probe rather than guaranteed for every timing window.
V8 preserves the copy/owner implementation; these remain separate v7 census
measurements, not a new v8 counter run.
The newer trial independently checks nine copy/owner source hashes against
that census, with frozen baseline snapshots so later source changes do not
invalidate its historical evidence.
[Current v7 counters](../orca-l0/demo/dg2-v7-copy-lock-census-windows-2026-11-02.json).

**Known Windows desktop issue:** the user reported continuous cursor flickering
during native DG2 execution and confirmed that it stopped after the experiment
was terminated. Short-only experiments resumed with the user's authorization.
Numerical checks do not establish desktop coexistence; this opt-in backend is
not ready to replace production L0. A private WDDM low-priority prototype now
passes 15 exact model requests, but has no confirmed desktop fix or speedup.
[Incident and incomplete experiment](../orca-l0/demo/dg2-desktop-incident-windows-2026-11-02.json)
and logs, staged sources and CPU-only runner（本地归档：`../orca-l0/demo/dg2-desktop-incident-windows-2026-11-02.json.gz`）
preserve the evidence.

The completed 75-request Short28/natural76 bracket compares production Orca L0,
native DG2, low-priority DG2, native DG2 and L0, in that order. Each process
discards ten warmups and retains five requests. Both control drifts pass the
3% decode bound. With 5771 expert slots, 1536 MiB requested reserve and
`spec-min-p=0.5`:

| Decode measurement | Production Orca L0 | Native DG2 | Low-priority DG2 |
| --- | ---: | ---: | ---: |
| Throughput (tok/s) | 47.734 | 38.712 | 38.710 |
| Process CPU use (average cores) | 1.017 | 0.031 | 0.032 |

Native remains **18.90% slower**; low priority changes native throughput by
**-0.005%**, providing no measured improvement. CPU use does not support the
idea that DG2 consumes more CPU. The CPU interval uses host receipt of
`REUSED`/`DONE`, separately from internal decode timing; native CPU times are
quantized in 15.625 ms steps. System CPU load drifted and is not causal evidence.
Low CPU use alone cannot distinguish GPU work from fence notification delay,
or explain cursor flickering. All 75 outputs and 57/57 draft acceptances match.
[Results](../orca-l0/demo/dg2-short-priority-cpu-windows-2026-11-02.json) and
exact runner, sources and logs（本地归档：`../orca-l0/demo/dg2-short-priority-cpu-windows-2026-11-02.json.gz`）.

| Path | Implementation | Validation |
| --- | --- | --- |
| SPIR-V → driver-native binary | `orca-l0-native-pack`; preserves compiler options and the >4 GiB addressing requirement | Full-model exact-output replay passed. The tested Short/Code2K/Prose2K/Long8K workloads loaded 150 native modules with zero SPIR-V fallbacks. Unexercised variants remain SPIR-V. |
| `ze_loader` → vendor DDI | `ORCA_L0_DIRECT_DRIVER=ON`; fixed function tables, raw vendor handles, no loader linkage | Standalone checks and the full Qwen model passed on Windows. Loaded-module audit found no `ze_loader`, SYCL or UR. |
| Vendor Level Zero driver → Windows D3DKMT | Opt-in `Orca::DG2`: native allocations/transfers, large logical buffers, independent queues, GPU dependencies, host gates, retained graphs, local code/thread/module data, XMX, eager batching, in-list events and explicit-dependency recording | Windows CTest 20/20. Isolated dependency-flush check: +5.95% with 45 exact requests. Latest 81-request L0/native/L0 context bracket: Short 47.060 / 38.699 tok/s (-17.77%); 8K 29.713 / 24.077 (-18.97%). The 2K decode control fails the drift bound. Earlier ordering gain +1.74% is a separate comparison. Remaining L0 gap, broad PPL and Linux GPU execution are open. Limited to A770/32.0.101.9034. |
| Vendor Level Zero driver → Linux DRM | Opt-in `Orca::DG2`: i915 allocations, shared VM, per-queue contexts/timelines, explicit GPU dependencies, host-word semaphore gates, zebin ABI reader and Orca-encoded compute walkers | Full Strata frontend compiles with Linux oneAPI 2025.3.2; 48 packs generated, 3/3 CPU checks and four ELF dependency audits pass. A separate Ninja-only GCC build passes the same three cases. Native Linux GPU execution, including model parity and gate coherence/preemption, remains unvalidated. |

On the Windows A770 (driver 32.0.101.9034), the published Fibonacci28/natural76
anchor with identical parameters, ten warmups and five retained requests per
process measured **49.70 → 50.38 → 50.01 tok/s** for loader/direct-DDI/loader.
The candidate is **+1.05%** relative to the geometric mean of the two controls;
the controls drifted **0.61%**. Every request produced the same 76 token IDs
and the same 57 accepted/offered drafts. This single round does not establish
a stable throughput improvement. Preparation, loading and prefill are excluded
from decode timing. It is a comparison against existing Orca, not original SYCL.

The later five-arm comparison used the same rebuilt host wrappers and numerical
source images, ten warmups and five retained anchor requests per process:

| Arm, execution order | Median decode tok/s | Loaded SPIR-V / native modules |
| --- | ---: | ---: |
| Loader + SPIR-V, before | 50.301 | 150 / 0 |
| Loader + zebin | 47.518 | 0 / 150 |
| Direct DDI + zebin | 47.355 | 0 / 150 |
| Direct DDI + SPIR-V | 45.187 | 95 / 0 |
| Loader + SPIR-V, after | 47.757 | 95 / 0 |

All anchor requests produced identical 76 token IDs and 57/57 drafts. The first
three arms also exercised Short/Code2K/Prose2K/Long8K, with identical complete
output tokens and finish conditions. The last two arms only ran the anchor,
explaining their smaller module counts. Both native arms loaded zero SPIR-V
modules. Coverage is complete for these workloads, not for every exported kernel.

The two controls drifted **−5.06%**. Relative to their geometric mean, the
native arms were −3.05% / −3.38%, and direct SPIR-V was −7.81%. This round
establishes correctness and dependencies, **not a stable speedup or a clean
isolated regression estimate**. No cause of the drift was established.
Preparation/loading/prefill are excluded from decode; TTFT is separate in the
report. Setup tracing was enabled, so readiness times are not startup benchmarks.
These results do not strengthen the earlier single-round +1.05% observation.

The previous strict bake failed at `iq_kernels/images_8.spv` with
`IGC: Internal Compiler Error: Divide by zero`, through both loader and direct
DDI. Inspection identified the SG16 XMX specialization, unreachable through
Orca's SG8-only matrix capability view. The Orca build now omits that
specialization; the original SYCL path keeps it. The remaining 252 IQ images
bake successfully, including all eight SG8 XMX variants. All 611 exported
GEMV images also bake successfully.

The expanded Windows collection has 1,267 referenced images across 48 packs.
`--compatible` converts **1,243 to zebin** and explicitly preserves **24 as
SPIR-V**: 18 declare FP64, unavailable on this device, and six use SG16
cooperative matrices on DG2. Each preserved image is listed with its reason
in `retained-spirv.tsv`. Other compiler errors still fail the bake. Strict
mode still rejects the unsupported exports. This is not a claim that every
exported feature runs on A770. No arithmetic or precision option was changed.

The optional prompt XMX path now checks matrix capabilities once before
dispatching, returning to its existing fallback when the shape is unsupported.
Ordinary SG16 kernels remain eligible for conversion; the compatibility rule
requires cooperative-matrix instructions as well as SG16 execution metadata.
CPU self-checks cover that distinction and malformed metadata. Selected
`--modules trace.tsv` baking remains available independently.

The expanded pack was revalidated on the full model with the same profile,
ten warmups and five retained anchor requests per process:

| Arm, execution order | Median decode tok/s |
| --- | ---: |
| Existing Orca, loader + SPIR-V, before | 51.539 |
| Updated Orca, direct DDI + compatible zebin pack | 51.529 |
| Existing Orca, loader + SPIR-V, after | 51.599 |

The candidate changed by **−0.078%** against the geometric mean of controls;
control drift was **+0.115%**. This round is effectively unchanged, without
evidence of a throughput gain. All 45 anchor requests matched the complete
76-token fixture and 57/57 drafts. Short/Code2K/Prose2K/Long8K also matched
complete output tokens and finish conditions. The expanded candidate loaded
150 native modules and zero SPIR-V modules, and had no loader/SYCL/UR dependency.
Decode excludes preparation, loading and prefill. Setup tracing was enabled.
This compares against existing Orca L0, not the original production SYCL stack.
The [Windows report](../orca-l0/demo/native-windows-2026-10-31.json) records
profile parameters, source/executable hashes and the image-manifest hash.
An additional 2K request with `STRATA_PROMPT_ATTN_XMX=1` matched the reference
output and loaded no SG16 attention image or SPIR-V fallback.

Driver UUID is unsuitable for a persistent native cache: Intel's driver
incorporates initialization time into it. Packs use device UUID, vendor/device
IDs, reported driver build, OS and pointer width instead. Rebuild a native pack
after changing the driver or device. Windows zebin packs are not Linux artifacts.

The experimental DRM runtime also builds with `ORCA_L0_BUILD_RUNTIME=OFF` and
`ORCA_L0_BUILD_DRM_CHECKS=ON`. It links neither Level Zero, Intel's user-mode
driver/compiler nor `libdrm`; only the DRM UAPI headers are needed. It provides
system-memory BO ownership, a private compute context, soft-pinned GPU VAs,
close/submit/wait, frozen lists, timeline completion and ordered multi-kernel recording. Replay uses one execution
ioctl and persistent residency entries, without allocations, per-kernel lookups
or user-space locks.

The prepared GPU check records RMSNorm → GEMV → SwiGLU → vector increment and
checks 72 replays against a CPU reference while changing parameters. Submission
and complete/readback windows exclude preparation. It **has not run on native
Linux**. CPU checks exercise the actual dispatch encoder, ELF/ABI decoding,
inline/indirect payloads, malformed input and rejection cases; WSL has no DRM node.

The bounded reader supports IntelGT ELF64 / ze_info 1.73, stateless pointers,
value/aggregate arguments with checked source offsets, 1D/3D workgroups with a
power-of-two total local size, hardware local IDs, inline/indirect data,
128/256 GRFs, static/dynamic
SLM and one barrier. Required subgroup size is checked against SIMD width.
Each module can have one initialized or zero-filled segment for constants and
one for globals; their hidden base pointers are bound to resident GPU memory.
The observed zero implicit-args relocation is supported. Other relocations,
unsupported metadata, images and stack calls remain rejected. Scratch/private
memory was initially rejected; the shared reader/encoder now supports it and
passes Linux CPU checks, with actual GPU parity tested on Windows only.
Selected Intel-generated packet definitions retain their MIT
license and source revision `ee495cce3db8f870a89697febbd7e8b6680151b2`.

Native GPU parity, local-memory placement/cache policy, broader kernel ABI
support, full Strata integration and an equal-workload DRM performance comparison
remain open. This is a compute implementation prepared for hardware testing,
not evidence that the full Intel user-mode driver has already been replaced.

Windows has an opt-in `ORCA_L0_BUILD_WDDM_CHECKS=ON` check, independent of
the Level Zero runtime and SDK. It now creates its own native compute context,
submits Orca-encoded commands through `D3DKMTSubmitCommand`, and waits on a
monitored fence with a ten-second bound. GPU timeout stays enabled; no display
or system setting is changed. Allocation, residency, GPU VA mapping and release
also pass. Loaded-module checks reject Level Zero, Intel UMD, GMM, SYCL, UR
and GPU-compiler dependencies in the execution process.

Three cycles each check 72 exact GPU stores, 72 exact multi-group vector
increments, and 72 RMSNorm → GEMV → SwiGLU → vector-increment pipelines.
Each pipeline replay updates its parameter buffer; all intermediate/final
values are compared with a CPU reference at `2e-4 * (1 + abs(expected))`.
All other input, ISA, payload and guard bytes remain unchanged. Each list is
recorded once and replayed with one submit call. Timings retain 64 samples
after eight warmups per cycle, excluding setup, recording, parameter updates
and correctness checks. The completion window ends after the fence wait.
That initial correctness check had no equal-workload Intel UMD control;
the later matched performance comparison is described below. Neither is a
full-model performance result.

The private ABI is explicitly limited to PCI `0x56a0` and driver
`32.0.101.9034`, checked before context creation. The installed driver uses
144-byte context data and a 184-byte command header. Intel NEO's public
32/56-byte context and 24-byte command-header fallback layouts do not match;
attempts using those layouts returned `STATUS_INVALID_PARAMETER`.
Isolated diagnostic processes checked the installed driver's calls and its
shipped helper's named-field translation. The final execution process loads
neither Intel UMD nor that helper. Unsupported driver versions are rejected,
not tried with guessed layouts.

VidMm selects GPU VA `0x800000000000` on this host. The shared compute encoder
now sign-extends valid high-half 48-bit GPU addresses for hardware packets and
rejects noncanonical addresses and overflowing heaps. GPU VAs remain separate
from CPU mappings. CPU regression checks cover both low and high address ranges.
The [Windows WDDM report](../orca-l0/demo/wddm-windows-2026-10-31.json) records
the calibrated fields, image/source/executable hashes, geometry and timings;
the [Orca README](../orca-l0/README.md) gives the build/check commands.

The later matched benchmark builds this same harness against Orca L0/direct
vendor DDI as its Intel UMD control. The same zebin, inputs, launch geometry
and CPU/guard checks run in three A0/B/A1 rounds, with 128 warmups and 512
retained samples per process. All 54 processes passed, with 27,648 retained
samples. Preparation, module loading, recording, parameter updates and checks
are outside the measured submission/completion windows.

For **400 dependent kernels with polling**, completion latency was
**499.4 → 386.7 µs, −23.3%** (median paired ratio). All three pairs passed
the ≤3% control-drift rule; maximum drift was **1.48%**. Per-round reductions
were 23.26%, 21.23% and 23.57%. CPU submission measured 13.4 → 8.7 µs, but
submission drift was too high to establish a stable CPU gain. The native-wait
chain observed −7.6%; short native-wait jobs observed +43.9%/+45.7% latency.
Those rows and the short polling cases have fewer than three stable pairs.

The initial conservative chain was 5.52 ms. Reusing identical state alone
did not resolve the regression (7.20 ms); the verified DG2 L3 write-back
policy reduced it to 1.42 ms. Keeping GPU dependency barriers while moving
CPU-visible flushing to the end of the list produced the final 386.7 µs.
Shared-encoder CPU checks cover state invalidation, MOCS encoding and the
GPU/host barrier distinction. Final Windows CTest passed 9/9 and WSL CPU
CTest passed 4/4; native Linux GPU execution remains unvalidated.

Both arms use resident system-memory data heaps, but separate allocations,
ISA placement, cache/state programming and native wait implementations differ.
Polling also occupies a CPU thread. This measures the current implementations,
not API overhead alone, and establishes no full-model speedup. The
[performance report](../orca-l0/demo/wddm-performance-windows-2026-10-31.json)
contains exact build parameters, hashes, all cases and optimization history;
the compressed raw reports（本地归档：`../orca-l0/demo/wddm-performance-windows-2026-10-31.json.gz`）
retain samples from the regressed stages as well as the final run.

Windows full Strata integration and the broader kernel ABI remain open.
Device-local allocation now passes the later correctness checks below;
the earlier performance runs use system-memory heaps.
The existing production executable/profile still use
the verified Orca Level Zero path. The [Orca README](../orca-l0/README.md)
includes the benchmark build and replay commands.

The Windows transport is now available as an opt-in reusable library with
`ORCA_L0_BUILD_WDDM_BACKEND=ON`, `Orca::DG2` and `orca_l0/dg2.hpp`. It builds
with `ORCA_L0_BUILD_RUNTIME=OFF`, requiring no Level Zero SDK. Linux uses the
same public API, argument packer and recording implementation; the original
`orca_l0/drm.hpp` names are compatibility aliases. Module ISA/data are cached
during recording, with no parsing or allocation during submission. List reset,
polling, finite waits, in-flight reuse rejection, resident pointer validation
and dynamic local allocations are covered by runnable checks.

Actual Strata GDN gate followed by dependent copy passed 24 replays with 703
elements and changing input, including the large-input softplus branch. Its
maximum absolute error was 9.54e-7. Actual BF16 split GEMV passed 48 replays
on a 19×1027 matrix, at local64/local256 with dynamic SLM and changed inputs;
the fixture uses exactly representable values and maximum absolute error was
zero. Tests verify inputs remain unchanged and check the full padded output
regions. Loaded-module checks reject Level Zero/Intel UMD/GMM/compiler use.

The initial native-pack census accepted **640 of 1,243** exports for this bounded ABI.
The first rejection for each of the remaining 603 was: 578 per-thread memory
buffers (scratch/private memory), 12 global-atomic metadata, 10 target-note
layouts and three EU-fusion requirements. These are metadata results, not GPU
validation of every accepted export. The [initial reusable runtime report](../orca-l0/demo/dg2-runtime-windows-2026-10-31.json)
records that build, hashes, checks and census. The latest census below supersedes
the 640 figure.

The reusable Windows runtime has since been compared through that same
benchmark harness. Three initial A0/B/A1 rounds covered six workload/wait
combinations. Because only two polling-chain pairs were stable, three more
polling-chain rounds were retained, for **63 processes / 32,256 samples**
in total. Every process passed CPU-reference and full data-heap guard checks.
Preparation, allocation, recording, parameter updates and correctness checks
remain outside the timed windows.

For **400 dependent kernels / polling**, median completion was
**496.2 → 382.3 µs**, with a median paired reduction of **23.4%** across all
six pairs. Five meet the ≤3% drift rule; their median reduction is **23.6%**
(range 21.3–27.2%). The sixth has 4.43% drift and is retained in the report.
CPU submission measured 14.7 → 9.2 µs, but only two submission pairs were
stable. Short native-wait cases observed +39.3%/+47.3% latency; their stable
pair counts are 0/3 and 1/3, so the precise regression is not established.
The remaining cases also lack three stable pairs. Event-wait latency remains
an optimization target, rather than evidence for a universal improvement.

The [reusable-backend performance report](../orca-l0/demo/dg2-performance-windows-2026-10-31.json)
and its samples, logs and source snapshot（本地归档：`../orca-l0/demo/dg2-performance-windows-2026-10-31.json.gz`）
separate this result from the older fixed-heap demo. The control is existing
Orca L0/direct DDI through Intel UMD, not original SYCL. These jobs exercise
no scratch/private memory. Production full-model execution, broader ABI and
native Linux GPU testing remain unfinished. Device-local allocation has since
passed the correctness checks below, without a new throughput measurement.

A subsequent instrumented run compared the existing 128-spin/event wait,
50/100 µs bounded-spin fallbacks and continuous polling: **108 processes /
55,296 samples** passed all checks. Calling-thread cycles are measured around
the timing window with `QueryThreadCycleTime`; they are not CPU microseconds,
system CPU use or energy. Driver/background threads are excluded.

On the 400-kernel chain, existing event waiting measured **457.3 µs / 97,173
cycles**, versus polling at **399.3 µs / 878,177 cycles**. Event waiting uses
11.1% of the polling thread cycles. The 50/100 µs hybrids measured
459.9/468.8 µs and 21.0%/33.1% of polling cycles: spending a fixed spin budget
on this long task does not improve event latency. On the short four-kernel
pipeline, the 100 µs hybrid observed 93.0 µs, close to polling's 92.6 µs;
CPU-cycle cost there is also similar. Cross-policy numbers come from separate
processes and are observations, not directly bracketed policy pairs.

The native-wait comparison against existing Orca L0/direct DDI has three
stable pairs for both completion and thread-cycle cost: **−8.1% latency /
−91.1% caller cycles**. Native L0 waiting is therefore not a low-CPU reference
on this workload: it consumed about 1.10 million caller cycles per chain.
Polling remains faster for the chain (−20.9%, three stable pairs), but the
four-kernel polling pipeline is now a confirmed microbenchmark regression
(+12.9%, three stable pairs). Different-kernel state changes require more work.

The [wait-policy report](../orca-l0/demo/dg2-wait-modes-windows-2026-10-31.json)
and complete evidence/source snapshot（本地归档：`../orca-l0/demo/dg2-wait-modes-windows-2026-10-31.json.gz`）
retain all samples and CPU diagnostics. The instrumented harness has separate
source/executable hashes and is not pooled with the previous run. Finite spin
budgets suit latency-sensitive short work; longer work favors event waits.
Completion batching across concurrent requests is still an engine feature to
add; one recorded list already shares one fence across its kernels. Production
defaults, full-model performance and native Linux GPU claims are unchanged.

The scratch/private revision accepted **1,210/1,243** exports by metadata.
The 33 first rejection reasons are 12 global-atomic flags, 11 EU-fusion disable
requirements and ten different-target images. Acceptance is not GPU parity for
all 1,210 exports. Scratch slots 0/1 are parsed; only slot 0 has actual GPU
validation. Cvec passed 12 changed-input/enable replays with 2,048 scratch bytes
per hardware thread (maximum absolute error 1.19e-7). Q3_K-to-BF16 passed 16
replays with 512 private bytes per hardware thread, varied nonzero signed scales
and exact BF16 output. Input/output padding and both 64 KiB thread-pool guards
are checked. Windows CTest passed 3/3 and shared Linux CPU CTest passed 4/4 at
that source revision; Linux GPU calibration is still deferred.

ISA and immutable captures now share aligned 1 MiB slabs. A 17,000-capture GPU
check crosses the slab boundary and verifies the exact sum and full output
padding. Captures remain alive across reset and until runtime destruction;
memory can grow with repeated different recordings.

Against the previous reusable **DG2** implementation, 27 processes / 13,824
retained samples passed. Four-kernel polling pipeline latency fell a median
paired **6.1%** across six attempts; three met the 3% drift threshold, with
6.0–6.5% reductions. The 400-kernel result changed less than 1%, establishing
no material additional gain. This run does not establish a stable CPU-submit
gain. Against existing Orca L0, the latest pipeline control had zero stable
pairs, so the earlier regression is not established as resolved.
[Latest report](../orca-l0/demo/dg2-scratch-arena-windows-2026-10-31.json) and
all checks, attempts and exact source snapshots（本地归档：`../orca-l0/demo/dg2-scratch-arena-windows-2026-10-31.json.gz`）
preserve these results separately from older versions and the production model.

Continuous polling is optional. The current Windows `wait()` already performs
a short spin then blocks on a monitored-fence event. A caller can submit one
recorded batch, prepare the next independent request in host memory, and check
the batch tail once at the result-consumption point, blocking if unfinished.
On Linux, a mandatory join can call `wait()` directly to avoid the extra
zero-timeout ioctl used by `ready()`. The semisynchronous demo is
`orca-l0-dg2-check --gpu-semisync image_0.zebin`; it uses 400 dependent kernels,
one explicit tail query and independent CPU dequantization. GPU dependency
barriers remain in place. At this semisynchronous checkpoint only one list
could be pending. The multilist revision below now exposes batch completion
progress; the full request scheduler, multiqueue and automatic compatible-GEMM
merging remain engine work. Submit ready tail work without waiting to fill a batch.

The Windows semisynchronous demo passed three serial/overlap/serial rounds:
720 GPU replays and 576 retained samples. Combined GPU-completion plus host-work
latency fell a median paired **10.0%** (9.5–13.4% by round), with all three
control drifts below 1.8%. Windows CTest and Linux CPU CTest each passed 4/4.
CPU preparation is included in this combined-work metric; it is not a pure GPU
or full-model decode gain. Preparing the next request independently does not
imply that an autoregressive next token is independent of the current result.
[Report](../orca-l0/demo/dg2-semisync-windows-2026-10-31.json) and
all samples, checks and exact sources（本地归档：`../orca-l0/demo/dg2-semisync-windows-2026-10-31.json.gz`）
are separate from the older wait-mode measurements. Production is unchanged.

The earlier execution revision accepted **1,233/1,243** exports by metadata;
it rejected ten ESIMD target-note layouts. The compiler-binding revision below
corrects their earlier classification as different GPU targets and accepts
**1,243/1,243**; their Xe HPG core matches while the product is unspecified.
The 12 global-atomic and 11 EU-fusion first rejections above are now cleared.
No claim is made that every accepted export has been executed.

The shared recorder now accepts three-dimensional local/group tuples in
hardware x,y,z order, validates a power-of-two local total up to 1,024 and a
total global count within `uint32_t`, and binds hidden local/global/group sizes
per axis. The original 1D API delegates to this path. Required local tuples,
malformed metadata, size overflow and both EU-fusion state transitions are
covered by CPU checks. Xe HPG's inverted fusion-disable bit is included in the
global-state cache key; global-atomic instructions/fences need no extra DG2
state beyond their existing ISA and ordered barriers.

On Windows A770/32.0.101.9034, actual Strata FP32/FP16/BF16 finite-count kernels
passed 24 replays over 8,195 values. They cover contended atomic additions,
all-finite/all-bad/mixed signed Inf/NaN cases, repeated accumulation, a dependent
doorbell and full input/output guards. Actual FP16 M4/BF16 M8 XMX GEMMs passed
144 replays with 128/256 GRFs, one/eight subgroups, padded strides/tails and
multiple Y/Z groups. All outputs match the CPU exactly on the dyadic fixtures.
Non-DPAS doorbells bracket each GEMM, exercising live systolic, GRF and
EU-fusion transitions within one command list. A separate OpenCL coordinate
fixture passed 32 replays with nontrivial local/group X/Y/Z, hidden sizes,
partial/full SIMD and full prefix/output guards.

The [atomic/XMX/3D report](../orca-l0/demo/dg2-atomics-xmx-windows-2026-10-31.json)
and complete checks, compile/bake commands and exact sources（本地归档：`../orca-l0/demo/dg2-atomics-xmx-windows-2026-10-31.json.gz`）
separate kernel parity from model/performance claims. The execution checks load
no Level Zero, Intel UMD, GMM or compiler; the coordinate image was compiled and
baked beforehand. Multiqueue/events, full Strata integration and native Linux
GPU validation remain unfinished. Large local allocations are covered by the
subsequent device-memory checks, not this earlier report.
The final Windows CTest and shared Linux CPU CTest each passed 4/4. Concurrent
host/GPU doorbell handoff is not established by the dependent GPU doorbell check.

The subsequent memory revision removes the prototype's 256 MiB owned-allocation
cap and adds explicit `import_host` for borrowed write-back host pages. Imports
require 64 KiB address/size alignment, reject overlap and retain separate CPU
and GPU addresses. Windows validates committed read/write mappings; teardown
releases the GPU allocation without freeing the caller's pages. Linux prepares
the same contract with `GEM_USERPTR`; native GPU validation is still pending.
This is system-memory import, not device-local allocation or full Strata integration.

On Windows A770/32.0.101.9034, eight GPU replays passed with a >320 MiB allocation
and a borrowed 128 KiB buffer. Four distant GPU VA subranges cross the former
256 MiB boundary. Exact results, all gap/tail guards, invalid imports, in-flight
CPU-access rejection and caller ownership after teardown are checked. Windows
CTest passed 5/5; actual XMX/GEMV/scratch/private/atomic regressions and Linux
CPU CTest 4/4 passed. The [memory report](../orca-l0/demo/dg2-host-memory-windows-2026-10-31.json)
and complete logs/source snapshot（本地归档：`../orca-l0/demo/dg2-host-memory-windows-2026-10-31.json.gz`）
make no new throughput claim. At that checkpoint concurrent submissions,
multiqueue/events and the full-model adapter remained open. Device-local
allocation is covered next; multilist support is covered below.

`Runtime::allocate_device` now explicitly allocates local VRAM without a CPU
mapping (`data == nullptr`, `device_local == true`). The Windows descriptor
requests linear, uncompressed, LocalOnly + NotLockable storage. GPU-memory
residency is required after MakeResident. Its 2,376-byte layout is calibrated
only for A770/32.0.101.9034: LocalOnly is byte 38/mask 4, NotLockable byte 39/
mask 1, and driver build 1019034 at offset `0x8c8` must be retained. Process
pointers, allocation IDs and padding are zero. Calibration loads vendor
libraries in a separate process; execution does not. A separate 16 MiB probe
reported local usage 0 → 16,777,216, shared usage 0 → 0 and GPU-memory residency.
Removing NotLockable was rejected by the driver; CPU mapping is not exposed.

On this Windows host, `--strata-device PACK` passed 12 complete H2D → D2D →
D2H replays at 65,536, 16,842,752 and 335,609,856 bytes (four per size), with
changed finite-bit patterns and exact full-buffer comparison. Actual local
BF16 GEMV passed 48 replays, and local FP16 M4/BF16 M8 XMX GEMM passed 144,
with max error 0 on dyadic fixtures, full guards, dynamic SLM, padded strides/
tails and mixed non-DPAS/XMX state transitions. System-memory regressions
passed XMX144/GEMV48/scratch12/private16/atomic24. Windows CTest passed 5/5;
the final shared-source Linux build/CPU CTest passed 4/4. Linux local-memory
placement uses cached DEVICE-region discovery and GEM_CREATE_EXT, reserves
2 MiB aligned/padded GPU VA spans for DG2 64K PTEs, and remains GPU-unvalidated.

Windows local allocations round to 64 KiB and explicitly reject sizes above
`0xffff0000` bytes; the largest GPU-tested object is about 320 MiB. This older
report's float4 transfer fixture is superseded by the public byte-copy API below.
That revision retained allocations until runtime destruction and permitted
only one pending ordered list. Full Strata integration, reclamation,
multiqueue/events and larger single local allocations remain open. The
[device-memory report](../orca-l0/demo/dg2-device-memory-windows-2026-10-31.json)
and logs, calibration details and exact sources（本地归档：`../orca-l0/demo/dg2-device-memory-windows-2026-10-31.json.gz`）
establish correctness only, with no new bandwidth or full-model speed claim.

Native `Runtime::copy` and `fill` now record memory operations without L0
handles. `copy` checks each full range in one resident allocation, permits
arbitrary byte counts/offsets and rejects partial overlap; exact self-copy and
zero-length operations are no-ops. `fill` captures 1..128-byte power-of-two
patterns and supports a partial final pattern. Common patterns of at most
16 bytes are kernel arguments; larger ones share the immutable capture heap.
`set_memory_module` binds a driver-baked `src/drm/memory.cl` once. Its three
SIMD16/128-GRF kernels have no scratch/private memory; aligned bodies move
16 bytes per work item and unaligned/tail accesses use bytes. The launch cap
is not claimed to be a measured bandwidth optimum. No parsing, allocation or
per-kernel system call was added to replay.

The Windows A770/32.0.101.9034 check passed 276 replays / 138 recordings across
H2D/D2D/D2H/H2H, 0..2,097,189 bytes, six aligned/unaligned offset pairs and all
eight pattern sizes. It changes source data, modifies stack patterns before
execution, verifies complete source/destination allocations and every guard,
and checks invalid ranges, overlapping copies and recording-state errors.
The local roundtrip12/GEMV48/XMX144 checks now use the public byte-copy API,
and the old float4 helper is removed. System-memory regressions and 17,000
scalar captures across a 1 MiB heap rollover passed. Windows CTest passed 6/6;
the same shared sources built on Linux with CPU CTest 4/4. `require` now accepts
`string_view`, avoiding successful-check string allocations without disabling
validation. The compiler's execution/user required-local tuples are both
validated and must agree. Failure attempts are retained separately.

The [native-transfer report](../orca-l0/demo/dg2-transfers-windows-2026-10-31.json)
and all logs, exact sources and baked memory image（本地归档：`../orca-l0/demo/dg2-transfers-windows-2026-10-31.json.gz`）
make no new bandwidth or full-model throughput claim. Offline baking uses the
vendor compiler; the Windows execution process loads no L0, Intel UMD, GMM or
compiler. Linux must bake its native image on Linux and remains GPU-unvalidated.
That transfer checkpoint still lacked multilist support, supplied below.
Full Strata queue/device integration, multiqueue/events, reclamation and single
local allocations of at least 4 GiB remain unfinished.

The independent `compiler/dg2.hpp` binding now indexes packs through
`dg2::Program` and records `compiler::parallel_for` without L0 kernel handles.
It uses compiler capture descriptors plus native pointer-field metadata for
resident CPU/interior/GPU pointers, preserves ordinary scalar bits, and shares
the dynamic/empty local-accessor ABI with the original L0 compiler path.
Ordinary and ESIMD `nd_range<1..3>` and `range<1>` dispatches are exposed. Range
group selection is a cached divisor heuristic, not a measured optimum.
SIMD1/VC packed local IDs require CPU-generated xyz tuples in immutable
32-byte thread records, a SIMD32 walker encoding/mask with logical SIMD1
thread counts, disabled hardware ID generation and an unskipped loading
prologue. All this is recorded once; replay still performs no parsing,
pointer translation or per-kernel system calls.

The target-note reader now follows the relevant NEO family rule: an absent or
unknown product is permitted only with the matching Xe HPG core. Specified
different products, wrong/missing cores and duplicate family notes fail CPU
checks. This resolves the old census's ten false target rejections. Metadata
coverage is **1,243/1,243**; execution of every accepted export is not established.

The [2026-11-01 compiler-binding report](../orca-l0/demo/dg2-compiler-windows-2026-11-01.json)
and all logs, exact sources and portable/native images（本地归档：`../orca-l0/demo/dg2-compiler-windows-2026-11-01.json.gz`）
record Windows CTest **9/9**, 12 mixed SYCL/ESIMD pipeline replays with 3D local
groups, local VRAM outputs, changed inputs and full guards, plus original L0
compiler CTest **4/4** and Linux shared-code CPU CTest **4/4**. Existing transfers,
local/system GEMV/XMX, scratch/private/atomic and 17,000-capture rollover checks
passed. Execution loads no L0/Intel UMD/GMM/compiler. WSL has no oneAPI frontend;
the Linux compiler-binding build and native GPU execution remain unvalidated.
Program entries used at launch require offline zebin baking; indexed SPIR-V
does not enable a compiler fallback. At that compiler checkpoint the runtime
still had one ordered list and one pending submission. Full engine adaptation and reclamation remain open,
with no new full-model or wait-mode performance claim.

The next revision adds immutable `dg2::Commands` handles. `finish()` freezes a
recording and starts another, while all lists share the same allocations and
ordered queue. Different lists can be submitted without intervening host waits;
each submit returns a monotonic queue sequence. `completed()` collects progress
for all earlier submissions on that queue. A list must complete before its own
resubmission; copied handles track the same latest sequence. Identity tokens
outlive the Runtime solely to reject stale handles without address-reuse aliasing.
The original implicit-list API remains usable.

Windows reuses the mapped monitored fence and event wait. Linux now uses the
i915 execbuffer timeline-fence extension, `SYNCOBJ_QUERY` and absolute monotonic
`SYNCOBJ_TIMELINE_WAIT`, requiring `I915_PARAM_HAS_EXEC_TIMELINE_FENCES`.
It avoids waiting on a BO whose reservation may include later unrelated uses.
The Linux transport compiled and passed CPU checks; native Linux execution is
still unvalidated. All host methods require one owner. CPU mappings, allocation
and recording still require a drained queue. At that checkpoint frozen-list
reclamation was unfinished; the later pooled lifecycle is described below.
General allocation release, independent queues/events and full Strata
adaptation remain unfinished.

The [multilist report](../orca-l0/demo/dg2-multilist-windows-2026-11-01.json)
and exact sources, logs and images（本地归档：`../orca-l0/demo/dg2-multilist-windows-2026-11-01.json.gz`）
record Windows CTest **10/10** and Linux CPU CTest **4/4**. Forty frozen lists
are submitted without host waits for twelve changing-input waves, followed by
one final tail join per wave. Every intermediate local-VRAM snapshot and full
guard is verified. Alias replay and implicit-list transitions total **482
submissions**. A separate twelve-replay compiler pipeline spans eight frozen
regions (**96 submissions**) with ordinary/ESIMD 3D launches, local memory and
large/normal GRF transitions. Transfers276, 17,000 captures and actual local/system
GEMV/XMX/scratch/private/atomic regressions passed. The zero-timeout probe need
not time out, and actual hardware backlog depth was not measured. These are
ordered queue correctness checks, with no new throughput or independent-engine
parallelism claim. Production settings and executable remain unchanged.

The later completion-mode demo compares serial waits, polling, a tail wait,
prefix collection and CPU/GPU overlap on **16 lists × 25 dependent kernels**.
Every mode performs identical independent host dequantization. Three fresh
processes, 128 warmups and 512 retained samples per phase produced **13,824
retained samples / 276,480 submissions** with exact CPU/GPU outputs and full
padding checks. Relative to adjacent polling controls, tail waiting increased
combined-work latency **10.3%** and reduced calling-thread cycles **77.8%**;
prefix collection changed them **+6.7% / −67.6%**; preparing CPU data before
prefix collection changed them **−5.6% / −68.2%**. All three arms have three
stable wall/cycle pairs under the 3% drift rule. Serial per-list waits have no
stable pair and establish no percentage. These are combined-work metrics,
including CPU preparation, not pure GPU timing or full-model throughput.

The prefix collector immediately consumes the observed complete prefix and
blocks on the first pending sequence; it does not wait for a batch to fill.
This small-list fixture still uses a median 15–16 explicit queries and 14–15
wait invocations per wave, versus one wait for a tail join. Explicit queries
exclude internal wait probes, and wait invocations do not prove OS blocking.
Cycles measure calling-thread execution, not CPU percentage or driver threads.
Runtime APIs/defaults and resource lifetime restrictions are unchanged.
Windows CTest passed **10/10**, Linux build/CPU CTest **4/4**; Linux GPU testing
and full Strata integration remain unfinished.
[Report](../orca-l0/demo/dg2-completion-modes-windows-2026-11-01.json) and
complete final/exploratory evidence and exact sources（本地归档：`../orca-l0/demo/dg2-completion-modes-windows-2026-11-01.json.gz`）
keep the exploratory short run separate from the final samples.

The latest lifecycle revision adds `discard(Commands&)` and `allocated_bytes()`.
Discard accepts an observed-complete or never-submitted frozen list, returns
its command/argument storage to internal pools, invalidates copied handles and
increments its slot generation before reuse. It rejects in-flight lists. A
completed prefix can be discarded while later queue submissions remain pending;
new recording and checked CPU mappings still require the queue to drain.
Shared internal leases retain captures needed by surviving lists, while weak
argument caches cannot pin or alias a recycled lease. Implicit reset releases
parameter leases. Linux places each executing batch last in its residency list,
including when a batch BO is reused rather than newly allocated.

Windows A770/32.0.101.9034 passed **900 recycling submissions**: four 96-list
waves with 18,432 changing capture instances each, a surviving older capture,
prefix retirement, then 512 wide-fill/scalar re-recordings and implicit resets.
Exact CPU-reference results, every padding byte, stale/foreign/default/reused
handles, expired weak-cache recapture and never-submitted discard were checked.
Retained GPU allocation bytes rose from 9,633,792 to **10,682,368
(10.1875 MiB)** during warmup, then stayed exactly there through later waves and
all 512 re-recordings. This counts all retained allocations/imports, including
host-mapped heaps and idle pools; it is not VRAM residency, RSS or an OS budget.
Larger live sets or new shapes can grow the pools. Backing allocations remain
owned until Runtime destruction; general allocation free/trim, module/scratch
reclamation and full Strata integration remain open. Windows CTest passed
**11/11**, Linux build/CPU CTest **4/4**; Linux GPU behavior remains unvalidated.
[Reclamation report](../orca-l0/demo/dg2-reclamation-windows-2026-11-01.json) and
exact sources, logs and images（本地归档：`../orca-l0/demo/dg2-reclamation-windows-2026-11-01.json.gz`）
record correctness and bounded reuse for this workload, without a new
model-throughput or wait-mode performance claim.

The multiqueue revision adds `Queue` and immutable `Completion` handles.
`begin(queue)` binds a recording to one queue; `finish()` starts another empty
recording on the default queue. Queue-aware `submit(list, queue, waits)` accepts
up to 16 already-submitted completion points from the same Runtime. Dependencies
are validated before submission, deduplicated across foreign queues and omitted
for the same ordered queue. Windows enqueues D3DKMT monitored-fence waits;
Linux submits timeline waits/signals, shares one explicit VM between contexts
and requires `I915_PARAM_HAS_EXEC_ASYNC`. `EXEC_OBJECT_ASYNC` removes implicit
BO ordering, so actual cross-queue dependencies must be supplied explicitly.

Each queue has its own completion sequence and writable scratch/private pools.
An immutable completion still names its original point after list replay,
discard and slot reuse. Waiting a consumer does not update producer cached
progress; observe each queue before recording, allocation or checked CPU access.
Those operations still require **all queues observed idle**, and all Runtime
calls still require one host owner. At that checkpoint dedicated copy engines,
concurrent host observers/callbacks, host-signaled gates and asynchronous recording
remained open. Queue creation selects the same calibrated compute engine.

Windows A770/32.0.101.9034 passed **61 submissions** across three queues:
twelve changing-input GPU fork/join waves with local-VRAM snapshots, later
producer tails, duplicate dependencies, separate queue sequences, immutable
old points, recycled lists, invalid/foreign handles and every output guard.
All five lists in each wave are submitted before host waits. A separate check
passed **24 submissions**, each combining an actual Cvec scratch2048 kernel
and a Q3_K private512 kernel. Queue-specific data, exact analytic results,
full input/output/pool guards and allocation growth for separate writable
pools are verified. These checks do not establish simultaneous execution,
backlog depth or a full-model gain. Windows CTest passed **12/12** plus actual
local/system GEMV/XMX/scratch/private/atomic regressions; updated Linux GCC build
and CPU CTest passed **4/4**. Linux GPU behavior and the full Strata adapter
remain unvalidated/unfinished.
[Multiqueue report](../orca-l0/demo/dg2-multiqueue-windows-2026-11-01.json) and
sources, logs, runners and images（本地归档：`../orca-l0/demo/dg2-multiqueue-windows-2026-11-01.json.gz`）
retain this correctness checkpoint separately from earlier performance evidence.

The host-gate revision adds `create_gate()`, `depend_on(HostGate)` and the
one-shot, idempotent `HostGate::signal()`. A list records leading gates before
kernels/copies and retains them until discard; a gated marker needs no kernels.
There are at most 16 distinct gates per list; submission input dependencies
plus gates must fit 16 before GPU dependency deduplication. Reset clears an
unsubmitted gate. Creation/recording still require an open recording and all
queues observed idle. Gates cannot be reset for a later callback.

`signal()` alone may run on a callback thread. A mutex per gate serializes
release/teardown; recording, completion caches and retirement remain owned by
the submitting thread. Windows uses D3DKMT monitored-fence GPU waits and CPU
signals. Linux i915 rejects an execbuffer wait for a missing future syncobj
point (checked against Linux v6.6 source), so it records the pinned NEO
`MI_SEMAPHORE_WAIT` on a dedicated host word instead. Signal stores one and
flushes its WB cache line without `GEM_SET_DOMAIN`, which would wait on the
blocked request. Linux gate pages are reused after all handle/list references
expire and retained until Runtime teardown. This is GPU semaphore polling,
not a CPU poll loop. Linux coherence, scheduling and preemption need GPU tests.

Windows A770/32.0.101.9034 passed **69 submissions**: sixteen three-queue waves
where earlier prefix copies and independent work finish while a later producer
tail remains blocked, alternating two-gate releases proving conjunction, worker
signals with owner completion waits, duplicate signals, retained 16-gate marker replays,
implicit reset, invalid/foreign/late/overflow rejection and abandoned-gate
teardown. Exact output, unchanged input and complete allocation guards passed.
Windows CTest passed **13/13**, with multiqueue, reclamation, transfer, compiler
and actual Strata GEMV/XMX/scratch/private/atomic regressions. Updated Linux GCC
build and CPU CTest passed **4/4**, including semaphore encoding; no Linux GPU
execution was available. Production still uses Orca L0/direct vendor DDI.
At that checkpoint native callback execution/error propagation, thread-safe
completion observation, async recording, allocation lifetime and full Strata
integration remained open. The following revision adds independent observation.
[Host-gate report](../orca-l0/demo/dg2-host-gate-windows-2026-11-01.json) and
exact sources, logs, runners and images（本地归档：`../orca-l0/demo/dg2-host-gate-windows-2026-11-01.json.gz`）
record correctness with no new model-throughput or wait-mode performance claim.
Actual OS sleep occurrence and thread scheduling overlap were not instrumented.

The observer revision adds worker/watchdog-safe `Completion::ready()` and
`Completion::wait(timeout_ns)` on immutable handle copies. A separate monotonic
cache per queue answers confirmed earlier points without querying again. These
calls do not publish Runtime's owner progress, discard lists or retire resources.
`wait()` returns false on timeout; invalid points, timeouts above `INT64_MAX` and
new calls on closing/destroyed runtimes throw. The original Runtime methods still
require one submitting owner, and `Runtime::wait` still throws on timeout.

Windows reads the mapped monitored fence and lazily reuses one TLS auto-reset
event per observing thread. Each wait registers once with D3DKMT, keeps that
registration across blocking slices, and rechecks the exact fence after any
wakeup, including an old timed-out registration. Linux uses syncobj timeline
queries and absolute-deadline waits. Up to 50 ms blocking slices let teardown
cancel long waits; no CPU busy-poll loop is added. Observers share a lifetime
lock; teardown cancels them before exclusively closing the borrowed fence/fd.
Cache hits never dereference native resources. In-progress teardown races may
complete or report closing. Finish callback bodies before Runtime destruction.

Current-source Windows A770/32.0.101.9034 CTest passed **14/14**, including
**50 observer submissions**: twelve two-queue callback pipelines, earlier prefix
before a gated tail, concurrent worker/watchdog handles, zero/finite waits,
cross-queue event reuse, a 60 ms held gate, owner-only progress/discard,
immutable recycled points, destroyed-runtime rejection and a teardown race.
Exact outputs, unchanged input and full guards passed, together with existing
multiqueue, transfer, reclamation and actual Strata kernel regressions.
Current-source Linux GCC build/CPU CTest passed **4/4**; GPU behavior remains
unvalidated. Actual OS sleep/wakeup overlap is not instrumented. This correctness
check adds no model-throughput or wait-mode latency claim. CPU buffer access,
recording and allocation still required all queues observed idle at this checkpoint. Native callback
body execution/error propagation, async recording, allocation lifetime, larger
Windows logical allocations and full Strata integration remain unfinished.
[Observer report](../orca-l0/demo/dg2-observers-windows-2026-11-01.json) and
exact sources, raw logs, runners and images（本地归档：`../orca-l0/demo/dg2-observers-windows-2026-11-01.json.gz`）
preserve this checkpoint independently of earlier wait-mode measurements.

The dynamic-recording revision now permits `append/copy/fill/begin/close/finish`,
fresh allocations/imports and gate creation while other frozen lists execute.
All Runtime operations retain one host owner. An implicit submitted recording
must complete before reuse by `finish/reset`; frozen lists still require their
own owner-observed completion before replay/discard. Checked public CPU access,
queue creation and scratch diagnostics still require all queues idle.

Internal ISA, indirect arguments, commands, module data, scratch/private and
Linux gate BOs have last-use sequences per queue. Submission updates existing
ledger entries without allocating. Recording rotates away from busy slabs;
pool leases remain immutable until their lists are discarded. Linux execbuffer
residency now includes only the internal storage referenced by the list, together
with conservative user buffers and the executing batch BO last. Unused pooled
storage no longer receives unrelated gated-request reservation fences. Internal
CPU writes enter `GEM_SET_DOMAIN` only after owner-confirmed idle use on every
referencing queue. User-buffer and shared module-global dependencies remain
explicit caller responsibilities. Linux captures user residency at close;
rebuild older lists before indirect use of user allocations introduced later.

Windows A770/32.0.101.9034 passed **64 dynamic-recording submissions** in sixteen
waves. A producer stays gated while independent work is recorded, allocated and
submitted, including first-use ISA and changed scalar/128-byte pattern captures.
The independent queue completes before the producer gate opens. A dynamically
recorded prefix consumer then completes while a later producer tail stays gated.
Queue-ledger growth, recycling, exact outputs, unchanged inputs and full guards
passed. The fixture's backing allocation count stayed at **4,915,200 bytes /
4.6875 MiB** after warmup; this is retained storage, not physical VRAM residency.
A five-second watchdog releases gates on failure. Windows CTest passed **15/15**
plus actual Strata kernel regressions; current-source Linux GCC build/CPU CTest
passed **4/4**. Native Linux GPU behavior remains unvalidated. This establishes
correctness, not recording latency, concurrent kernels or full-model throughput.
The real Strata queue/callback adapter, callback-body execution/error propagation,
general allocation release, larger logical Windows allocations and full-model
native validation remain unfinished.
[Dynamic-recording report](../orca-l0/demo/dg2-recording-windows-2026-11-01.json) and
exact sources, raw logs, runners and images（本地归档：`../orca-l0/demo/dg2-recording-windows-2026-11-01.json.gz`）
retain the new checkpoint separately from earlier measurements.

The actual Strata queue adapter now also runs over `Orca::DG2` through
`STRATA_ORCA_DG2=1` in `sycl/orca_queue.hpp`. It reuses exact events, typed
deferred work, capture/replay, the SPSC callback worker and error propagation.
The shared native recorder is protected by one adapter mutex. Device kernels
coalesce up to 64 launches; host-visible transfers/fills submit immediately to
preserve CPU-polled upload progress. Unregistered host pointers use pooled
staging, and captured H2D sources are refreshed on replay.

Completion collection takes one queue-progress snapshot and retires the entire
completed prefix, including staged CPU readbacks. A timeout still collects
earlier completed work. It does not wait to fill a batch or hold completed work
behind a blocked tail. Native fence waits and the callback ring's atomic wait
provide blocking progress without an added busy-poll thread. Unregistered D2H
copyback still needs observation or a later submission; it has no autonomous
background progress thread.

Windows A770/32.0.101.9034 passed **16/16** CTests, including the real Strata Queue:
128 published cross-thread handoffs, 2048 eight-slot staging jobs, 64 ten-MiB
uploads, exact events, deferred work, ESIMD, graph replay and callback errors.
The new batch check collects **16 readbacks while the tail stays gated**.
The original L0 Queue is checked independently. Current-source Linux GCC/CPU
CTest passed **4/4**; the WSL environment has no GPU node or oneAPI frontend,
so it cannot build/test the adapter. This supersedes the queue/callback gap above;
full-engine memory/device integration, general release, contiguous Windows
allocations above 4 GiB, GPU timestamps and native full-model validation remain
unfinished. Production remains on L0/direct vendor DDI.
[Adapter report](../orca-l0/demo/dg2-adapter-windows-2026-11-01.json) and
exact sources, raw logs, runners and images（本地归档：`../orca-l0/demo/dg2-adapter-windows-2026-11-01.json.gz`）
keep this checkpoint separate from earlier performance measurements.

The current-source raw completion demo ran in three processes, retaining
13,824 samples over 276,480 submissions including warmups. Each wave contains
16 ordered lists x 25 kernels and identical independent CPU preparation.
Relative to adjacent polling controls, preparing CPU work before prefix
collection changed combined latency by **-6.89%** and calling-thread cycles by
**-61.83%** (medians of three pairs within 3% control drift). Direct prefix
collection without overlap was **+8.16% / -60.87%**; waiting for the tail was
**+10.36% / -76.13%**. The serial arm had no qualifying stable pair. These are
scheduling-policy measurements, not an adapter A/B or full-model speedup.
Fixed mode order and unlocked GPU clocks remain limitations. Thread cycles
are not total CPU utilization or driver-thread work. All raw samples are in
the adapter evidence archive.

Public native allocation release and OS memory-budget queries are now implemented.
`dg2::Runtime::release` requires all queues owner-observed idle and an empty/open
recording; callers discard lists using that memory first. Allocation identities
reject stale/recycled, foreign and modified handles. Internal pools remain
Runtime-owned. Windows frees the GPU VA/resource and owned host pages, using
synchronous allocation destruction before `VirtualFree`; teardown uses it too.
The first attempt exposed error 87 from freeing still-pinned pages. Imported
pages stay caller-owned. Linux closes GEM handles, unmaps owned storage and
removes conservative residency entries from unrelated frozen lists. Native
Linux GPU behavior still needs verification.

`memory_info()` uses `D3DKMTQueryVideoMemoryInfo` on Windows and i915 memory-region
queries on Linux, with cached capacity/query layout and dynamic availability.
Results distinguish process budget from global availability. Linux unprivileged
kernel policy may hide other process usage. Availability is capped by capacity
minus this Runtime's local allocations. The Strata adapter now exposes free and
memory-info calls; it excludes rounding padding from logical transfer bounds
and only publishes host-event success after native signaling succeeds.

Windows A770/32.0.101.9034 passed **17/17** CTests, all earlier Strata kernel
regressions and **76 release submissions**: 64 host/local-VRAM roundtrip/release
cycles, three unrelated retained-list replays, one gated rejection and eight
borrowed-page reimports. Exact results/full guards and stale/foreign/modified/
internal/double-release rejection pass. Retained fixture backing storage stays
at **2,162,688 bytes / 2.0625 MiB** after warmup. This is not physical residency.
The real Queue separately checks a one-byte logical allocation and physical
backing-byte decrease after free. Current-source Linux GCC/CPU CTest passes
**4/4**; it does not verify GPU release or the oneAPI frontend. Full-engine
backend/device/host-import integration, contiguous Windows allocations above
4 GiB, timestamps and full-model validation remain open. No new throughput,
latency or TTFT improvement is claimed.
[Release report](../orca-l0/demo/dg2-allocation-release-windows-2026-11-01.json) and
exact sources, raw logs, runners and images（本地归档：`../orca-l0/demo/dg2-allocation-release-windows-2026-11-01.json.gz`）
retain this checkpoint independently of the completion-policy measurements.

The Strata numerical frontend now selects `Orca::DG2` through
`STRATA_ORCA_DG2=ON`, with `STRATA_ORCA_BUILD_ENGINE=OFF`. The default eight-pack
build also includes the router and native RoPE table registry required by the
prefill helpers. First-use constant tables upload outside an active capture,
even while an unrelated queue remains gated. Backend aliases replace explicit
L0 allocation/program types in the numerical facade; the full engine remains open.

A numerical failure exposed an event/readback lifetime bug: waiting on a later
GPU marker did not publish earlier staged host copies. Results could lag by one
submission, and later collection could write through an expired host pointer.
The adapter now retains a shared per-stream sequence ledger and publishes its
completed readback prefix when any later point is observed. It reuses the
existing blocking event/timeline wait; it adds no polling worker.

Windows A770/32.0.101.9034 now passes **15/15** CTests for this frontend:
36 FP16/BF16 XMX shapes, 96 GEMM API cases, 294 FP16 expert-pair output checks,
matrix batching and the actual Queue. A regression checks **64 fresh host
destinations using only returned event waits**, while the earlier gated-tail
test still delivers 16 readbacks before the tail is released. Original L0
numerical/queue/timestamp checks pass **3/3** separately. The earlier 48-pack
build compiled successfully but failed numerical validation before this fix;
it is not a passing full-pack checkpoint. The current default collection retains
one unsupported SPIR-V variant, rejected if selected for native execution.
Full ten-format IQ parity, Linux oneAPI frontend/GPU execution and full-model
validation remain untested at this checkpoint. No new throughput or TTFT gain
is claimed. [Report](../orca-l0/demo/dg2-strata-numerical-windows-2026-11-01.json)
and source/image hashes, raw logs, failures and runners（本地归档：`../orca-l0/demo/dg2-strata-numerical-windows-2026-11-01.json.gz`）
preserve the passing checks and their limits.

The subsequent graph/wait check passes **15/15** native Windows CTests,
**3/3** original L0 checks and **4/4** Linux raw-backend CPU checks. Recorded
graphs retain their native queue and queue-bound scratch/private memory after
the recording Stream is destroyed. Another Stream in the same Runtime joins
both transitions with GPU dependencies. Logical stream sequences preserve
staged readback ordering across these native queues. Thirty-two replays check
both joins and refreshed ordinary host sources using only the final copy event.

Dependencies from the same native queue coalesce to its latest completion
point; 18 events behind one producer gate check this path. A completed-prefix
snapshot still publishes 16 earlier readbacks before a blocked tail, without
waiting to fill a completion batch. At this checkpoint queries remained per
queue; the newer owner batch API and Strata collector integration are described
below. Eventfd notification remains open. This is a
correctness checkpoint, with no new latency, throughput or CPU-cycle claim.
Linux frontend/GPU, reusable verifier events, full-engine integration and
full-model validation remain open.
[Report](../orca-l0/demo/dg2-strata-graph-waits-windows-2026-11-01.json) and
exact source/image delta, check logs and runners（本地归档：`../orca-l0/demo/dg2-strata-graph-waits-windows-2026-11-01.json.gz`）
extend the preceding numerical archive, whose hash is recorded in this report.

The next native Windows checkpoint builds the full Strata engine and passes
**19/19** CTests. Ten IQ formats pass the existing parity checks for
`ncols=1..8`: dequantization is exact, and matvec passes its relative-error
tolerances. Engine startup, a touched 256-MiB allocation and dynamic OS memory
budgets, eight aligned borrowed-page import/release cycles, 4,680 snapshot
checks and 167 session-file checks pass. Original L0 compatibility passes
**3/3**; Linux raw-backend compilation/CPU checks pass **4/4**.

The verifier's reusable host-event implementation now compiles over DG2.
Its queue regression retains the same gate address for 32 graph reset/replays,
including capture while signaled, destruction of the recording queue, timed
waits and rejection of a reset before owner-observed completion. Windows uses
a mapped semaphore word for reusable gates; one-shot gates retain their OS
fences. GPU waits consume host signals without a CPU polling worker.

This validates engine bootstrap and numerical kernels, not full-model outputs,
PPL, tok/s or TTFT. The 48-pack collection retains 24 unsupported SPIR-V files;
selecting them fails explicitly. The 4-KiB host arena test uses staging because
native imports require owned ranges aligned to 64 KiB. Contiguous Windows
device allocations above `0xffff0000` bytes, GPU timestamps, Linux frontend/GPU
execution and internal-pool trimming remain open. The first engine build failed
on L0-only timestamp fields, now explicitly unsupported on DG2; a second build
was cancelled after a resume-directory error. These attempts are preserved with
the passing run in the [engine report](../orca-l0/demo/dg2-strata-engine-windows-2026-11-01.json)
and source/image delta, logs and runners（本地归档：`../orca-l0/demo/dg2-strata-engine-windows-2026-11-01.json.gz`）.
No new performance gain is claimed.

The first bounded model attempt exposed non-power-of-two GR workgroups that
the encoder rejected. It now packs software SIMD8/16/32 local IDs at recording
time when the fixed hardware walk order is unsuitable; existing supported
shapes retain hardware generation. CPU payload/walker checks, **64 exact GPU
coordinate replays**, the native **19/19** CTests and Linux raw **4/4** CPU
checks pass after this change.

At that checkpoint, the diagnostic model used 132 actual expert-cache slots and
mirrored all experts into 33.02 GiB of host memory. Weight loading and T=1..6 graph
recording finished, but commit submission failed with `NTSTATUS=0xc01e0200`.
The [dispatch report](../orca-l0/demo/dg2-strata-dispatch-windows-2026-11-01.json)
and source/image delta, component checks and all model attempts（本地归档：`../orca-l0/demo/dg2-strata-dispatch-windows-2026-11-01.json.gz`）
preserve the exact sources and failure logs. The production profile is unchanged.

The following pointer-table fix converts CPU mappings into GPU virtual addresses
before serializing expert pointers. The 32-replay regression validates host and
interior pointers, existing GPU addresses and null, rejects unknown host storage,
and checks full input guards. Native **19/19** CTests, ten-format IQ checks for
`ncols=1..8` and original L0 **3/3** compatibility checks pass. A second fix maps
the verifier teardown's unsigned infinite timeout into the native signed range;
GDB captured the earlier cleanup exception, `invalid completion observer or timeout`.

The bounded one-token-prompt/two-token model run now exits with code 0 and outputs
**`271, 2`**, exactly matching the unchanged production L0 executable under the
same tiny-cache settings. These single unpaired launches reported **429.0 ms
native / 241.8 ms original L0** for two decoded tokens, including the first decode
round. They showed no speedup and do not establish full-cache throughput, PPL,
long-context parity or TTFT. Linux raw sources are unchanged from the preceding
4/4 CPU checkpoint; frontend/GPU execution remains unvalidated and no new Linux
run is claimed. The [pointer/model report](../orca-l0/demo/dg2-strata-pointer-windows-2026-11-01.json)
and source/image delta, checks, exact runners, model logs and failed/debug attempts（本地归档：`../orca-l0/demo/dg2-strata-pointer-windows-2026-11-01.json.gz`）
preserve this limited correctness result. Runtime wait defaults and production
settings remain unchanged.

The following large-buffer revision keeps the calibrated `0xffff0000`-byte
limit per **physical part**, reserves one contiguous GPU VA range and maps
multiple resident parts into it. A **5-GiB-plus-64-KiB** regression passes eight
replays across both ends, the part boundary and 4 GiB, with full guards,
unchanged input, release accounting and stale-handle rejection. Native Windows
CTest remains **19/19**; the updated Linux GCC build/CPU CTest passes **4/4**,
without a GPU node. The actual model's auto cache now opens **6478 slots /
8.68 GiB**, with prefill/context settings of 8192 and a requested 1536-MiB
reserve, and exits normally with the same two-token diagnostic output.
Those settings alone do not validate an 8192-token prompt or context.

A fixed **5771-slot / 7.74-GiB** cache comparison matches all **128 CLI output
tokens**. The persistent service test uses the same 28-token input and balanced
parameters in original-L0/native-DG2/original-L0 processes, each with ten warmups
and five retained requests. All **45 requests** exactly match the natural
**76-token output**, **57/57** draft acceptance and zero prefix reuse. Loaded
module audits confirm that DG2 loads none of L0, SYCL, UR, Intel UMD/compiler or
GMM. Decode medians are **48.78 / 16.66 / 48.71 tok/s**: **−65.82%** native
throughput, with **−0.13%** control drift. Preparation, loading and prefill are
excluded from `DONE decode_ms`. Prompt medians are **835.6 / 1854.7 / 837.8 ms**;
TTFT medians are **879.1 / 2010.8 / 882.2 ms**.

This establishes a substantial regression on this fixed prompt, without
identifying its cause. The two-warmup screening is retained separately.
Balanced decay=0.5 and short-read=1024 differ from the historical anchor, so
this is not anchor reproduction. Broad model/PPL/long-context correctness,
native Linux GPU, GPU timestamps, unsupported variants and pool trimming
remain open. The reserve figure is requested configuration, not continuously
measured external physical headroom. The
[large-buffer/model report](../orca-l0/demo/dg2-strata-large-device-windows-2026-11-01.json)
and source/image delta, raw logs and exact runners（本地归档：`../orca-l0/demo/dg2-strata-large-device-windows-2026-11-01.json.gz`）
preserve the full comparison. Production settings and executable are unchanged.

The cross-queue completion revision adds owner-only
`Runtime::completed(const Queue*, uint64_t*, size_t)` for up to **16 entries**.
It validates every handle before querying, deduplicates queues without heap
allocation and leaves caller outputs unchanged on error. Windows reads mapped
monitored fences; Linux packs unique pending timelines into one
`DRM_IOCTL_SYNCOBJ_QUERY`, using cached progress for fully completed queues.
The results are per-queue prefixes, not an atomic snapshot across queues.
Ready tails are submitted immediately; no request waits for a batch to fill.

Windows A770/32.0.101.9034 passes **19/19** CTests, including **48 three-queue
batch snapshots** with duplicates, pending/gated/complete prefixes and invalid,
foreign, oversized and null-input rejection with unchanged output guards.
The check reruns 64 coordinate replays and eight 5-GiB-buffer replays. Linux
GCC build/CPU CTest passes **4/4**, without validating GPU queries. At this
checkpoint the API was not yet wired into Strata's collector; eventfd
notification remained open and no model speed gain was claimed. The earlier thread-group policy experiment
showed no benefit in its bracket (**−2.21%**, control drift **−0.06%**); the
original grouping of eight is restored. Required Xe HPG instruction-cache
invalidation is retained. The
[batch report](../orca-l0/demo/dg2-query-batch-windows-2026-11-01.json) and
source/image delta, cached workaround reference and complete checks（本地归档：`../orca-l0/demo/dg2-query-batch-windows-2026-11-01.json.gz`）
preserve this checkpoint. The native model regression remains unresolved;
production settings and executable are unchanged.

The subsequent Windows observer-wait experiment adds at most **128 short
probes** with deadline and teardown checks before the existing TLS event
fallback. The owner wait is unchanged. A real Strata
old-native/candidate/old-native bracket uses the same 5771-slot cache, ten
warmups and five retained requests per process. Decode medians are
**16.54 / 16.60 / 16.52 tok/s**, a **+0.43%** candidate change with **−0.12%**
control drift. All **45 requests** retain identical natural76 output and
**57/57** draft acceptance. Median prompt times are **1883.4 / 1866.6 /
1872.2 ms**; TTFT medians are **2040.1 / 2022.8 / 2029.4 ms**. Preparation,
prefill and loading remain excluded from decode timing.

This small difference in one fixed-prompt bracket does not establish a
material benefit sufficient to change the default. The original event wait
is restored byte-for-byte; candidate and restored builds both pass **19/19**
Windows CTests and the eight-replay 5-GiB-buffer regression. Only the Windows
implementation changed, so the preceding Linux raw CPU checkpoint still
applies; no new Linux GPU run is claimed. Identical zebin bytes, production
hashes, both check runs and all model requests are retained in the
[observer-wait report](../orca-l0/demo/dg2-strata-observer-hybrid-windows-2026-11-01.json) and
complete evidence archive（本地归档：`../orca-l0/demo/dg2-strata-observer-hybrid-windows-2026-11-01.json.gz`）.
This compares two native versions. It does not supersede the earlier
native-versus-original-L0 regression or establish broad performance/PPL.

The following revision integrates the cross-queue batch API into Strata's
native collector. It uses stack snapshots over at most **16 pending entries**,
deduplicates queues and reuses completed prefixes beyond the lookahead for those
queues. More queues use successive batches; collection returns at the first
incomplete entry without waiting for a batch or blocked tail. A GPU regression
checks **18 retained graph queues**, two waves, **72 staged readbacks**, fresh
guards and publication before a separately gated tail. Native Windows CTest
passes **19/19**, alongside 64 coordinate and eight 5-GiB-buffer replays.

The same revision moves verifier window/commit and ordinary MTP round/prefill/
step graph preparation before service `READY`. The existing default-on
`STRATA_WARM_GRAPHS` controls this native service behavior; `0` retains lazy
preparation. Draft policy objects are also constructed before READY. No dummy
inference runs or expert-cache training occurs; dynamic MTP variants and
request-dependent expert transfers/CPU work still belong to actual requests.

A five-process audit uses original-L0, lazy-native, prepared-native,
lazy-native and original-L0, each with ten warmups and five retained requests.
All **75 requests** match the same 28-token prompt, natural **76-token output**,
**57/57** draft acceptance, zero prefix reuse and **5771-slot** cache. Original
L0 decode medians are **46.41 / 47.12 tok/s**, versus **16.27 tok/s** prepared
native: **−65.20%**, with **+1.54%** L0 control drift. A separate host delivery
measurement excludes the first delivered burst: **47.20 / 47.91** versus
**16.61 tok/s**, still **−65.06%**. Engine counters remain unchanged. Prompt
medians are **874.5 / 858.4** versus **1913.9 ms**; TTFT medians are **921.2 /
903.0** versus **2074.0 ms**. These are host/protocol observations, not GPU
timestamps. Neither initialization placement nor the first-output timing
boundary explains the warmed regression.

Lazy-native before/after decode medians are **15.80 / 16.51 tok/s**. The
**+4.45%** drift exceeds the 3% stability bound, so the prepared arm's nominal
**+0.74%** steady-state improvement is inconclusive. First-request TTFT is
**2596.7 / 2588.0** versus **2257.1 ms**, with **494.2 ms** of graph preparation
performed before READY. Process-to-READY is measured separately; OS caches
remain populated, clocks are not locked and the first request follows module
inspection. Pre-recording also changes allocation/capture order. The CPU-only
timing protocol check covers burst, single-delivery and empty cancellation
cases. Linux raw sources are unchanged from the prior CPU checkpoint; the
changed Strata frontend has not been compiled or run on Linux.
[Service preparation/timing report](../orca-l0/demo/dg2-service-preparation-windows-2026-11-01.json)
and sources, images, all requests, checks and cached official references（本地归档：`../orca-l0/demo/dg2-service-preparation-windows-2026-11-01.json.gz`）
preserve the evidence. Production settings and executable remain unchanged.

The native-DG2 model gap is between two paths on the **same A770**: production
Orca L0 using Intel's driver, and Orca's experimental replacement for that
user-mode driver. The warmed large-buffer checkpoint's 48.74 versus 16.66 tok/s
corresponds to approximately **39.5 additional ms/token**. Its ten discarded
warmups per process exclude the observed first-inference slowdown from the
retained comparison; initialization placement still matters for first-request
TTFT. The exact steady-state cause has not been established.

A concrete placement issue at that checkpoint was that native internal ISA and
scratch/private pools used `allocate_storage -> allocate`, whose Windows backing is
`VirtualAlloc` plus `ExistingSysMem`. The public device buffers, including the
model's expert cache, already use a separate local-VRAM allocation path.
Frequent GPU access to the internal system-memory pools can therefore involve
PCIe. Intel's [scratch controller](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/command_stream/scratch_space_controller_xehp_and_later.cpp)
and [memory manager](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/memory_manager/memory_manager.cpp)
provide allocation-type and ISA-placement policies. The following controlled
revision measures local scratch/private placement. ISA and the other immutable
internal heaps stayed on that checkpoint's system-memory path; the subsequent
ISA revision below measures code placement separately.

The local-pool revision keeps ISA, parameter, module-data and batch storage on
the existing path, and moves scratch/private pools into local VRAM when binding
`set_memory_module(path, true)`, the default. Strata exposes the control as
`STRATA_ORCA_DG2_THREAD_MEMORY=host|device`, defaulting to `device` in its
experimental DG2 backend. A reserved native queue initializes pools through the
existing copy/fill kernels; binding rejects transfer kernels with their own
scratch/private requirements. A **192-KiB** readback buffer checks leading and
trailing guards while preserving the caller's open or closed recording.

Windows CTest passes **19/19**, with 64 coordinate and eight 5-GiB-buffer replays.
Both memory placements pass the three-queue regression's **24 Cvec and 24 Q3_K
results**, changing inputs, exact outputs and all guards. First-use pools are
prepared while another queue remains gated; the first guard diagnostic runs on
a closed caller list, with later checks preserving an open recording. The raw
Linux GCC build/CPU CTest passes **4/4**. All **1257 native zebin images** match
the preceding service-preparation checkpoint byte-for-byte; 24 unsupported
SPIR-V variants remain. Changed Strata frontend/Linux GPU execution is unvalidated.

A Cvec `n4096/hc257`, scratch2048 demo includes GPU fill and device input/output
in each replay. Its second host/device/host bracket reports median
submission-plus-wait **603.90 / 207.60 / 596.15 µs**, **−65.40%** latency with
**−1.28%** control drift; the first gives −62.50% with −5.43% drift. Each process
discards ten warmups and retains 50 samples. These are host timings of this
fixed operator, not GPU timestamps or model throughput.

Two full model brackets change only the placement switch in the same native
executable and images. They retain the same 28-token prompt, natural **76-token
output**, **5771-slot** cache, balanced parameters, zero prefix reuse and **57/57**
accepted drafts. All **90 requests** match; each process discards ten warmups
and retains five requests. The repeat bracket's host/device/host decode medians
are **16.75 / 28.53 / 16.67 tok/s**, **+70.75%**, with **−0.46%** control drift.
Post-first-delivery rates are **17.11 / 29.09 / 17.03 tok/s**, still **+70.38%**.
Prompt medians are **1865.2 / 1150.5 / 1892.3 ms** (**−38.76%**), and TTFT medians
**2019.3 / 1235.2 / 2046.9 ms** (**−39.25%**). Engine decode excludes preparation
and prefill; clocks remain unlocked and delivery timing is host/protocol timing.
The first complete bracket gives +70.85% with −4.47% decode-control drift, which
exceeds the 3% stability bound and prompted the repeat. Both remain archived.
[Local-pool report](../orca-l0/demo/dg2-thread-memory-windows-2026-11-01.json) and
source/image delta, both model brackets, isolated demo, checks and failed attempts（本地归档：`../orca-l0/demo/dg2-thread-memory-windows-2026-11-01.json.gz`）
establish a substantial contribution from pool placement. They do not establish
original-L0 parity, a new original-L0 bracket or broad PPL/long-context behavior.

The first auto-cache smoke after local placement opens **6580 slots**, but
leaves **1165 MiB** at READY against **1536 MiB** requested: later pools were not
included in admission. The correction holds an additional **512 MiB** for native
auto sizing with local pools, including the existing post-allocation admission
check. This is calibrated from the observed later startup-availability drop;
the user's requested reserve stays 1536 MiB. Explicit slot counts and host-pool
mode keep their budget. With the correction, the auto smoke opens **6177 slots**
and reports **1697 MiB** free after preparation; the fixed regression retains
**5771 slots** and reports **2236 MiB**. Both match all **76 output tokens** and
**57/57** acceptance. Windows CTest remains **19/19**. Raw Linux sources exactly
match the preceding 4/4 CPU checkpoint; no new Linux frontend/GPU run is claimed.
[Auto-budget report](../orca-l0/demo/dg2-pool-reserve-windows-2026-11-01.json) and
source delta, checks and exact smoke runners（本地归档：`../orca-l0/demo/dg2-pool-reserve-windows-2026-11-01.json.gz`）
preserve this correction. The two cold requests check budget/correctness;
28.53 tok/s remains a measurement at the exact parent revision above, not a new
benchmark of this correction. Startup availability is not continuous physical
headroom. The 512-MiB allowance is not a universal bound: growing kernel scratch
requirements or queue topology requires preallocating/querying those pools.

The subsequent ISA revision adds `set_memory_module(path, local_thread_memory,
local_isa)`, preserving independent placement controls. The standalone API
defaults `local_isa` to false; Strata's experimental DG2 backend defaults
`STRATA_ORCA_DG2_ISA_MEMORY` to `device`, with `host` as the comparison. Code
uses immutable 1-MiB local slabs, one reusable host staging slab and zeroed
prefetch padding. Only the bound copy/fill module retains host-backed ISA for
bootstrap. The upload queue preserves the caller recording/indirect heap and
progresses before an unrelated gated queue completes. At this checkpoint module
data, indirect parameters and command buffers retained their prior backing;
the following state revision changes module-data placement separately.

A five-process original-L0/native-host-ISA/native-local-ISA/native-host-ISA/
original-L0 bracket uses **5771 slots**, the same 28-token input, natural
**76-token output**, **57/57 drafts**, balanced parameters and zero prefix reuse.
Each process discards ten warmups and retains five requests; all **75** match.
Native host/local/host decode medians are **28.287 / 35.983 / 28.329 tok/s**:
**+27.11%** with **+0.15%** control drift. Prompt medians are **1162.8 / 1017.5 /
1166.2 ms** (**-12.62%**), and TTFT **1247.9 / 1079.8 / 1251.6 ms** (**-13.60%**).
Post-first-delivery throughput still improves **26.84%**. Original L0 controls
measure **49.013 / 48.584 tok/s** with **-0.88%** drift; native remains **26.26%
slower** against their geometric baseline. Decode excludes preparation/loading/
prefill; these are host timings with unlocked clocks and populated OS caches.
This isolates a substantial ISA-placement contribution, without accounting for
all remaining differences or establishing broad model/PPL/long-context parity.

Windows CTest passes **19/19**, 64 coordinate replays and eight 5-GiB-buffer
replays. Explicit local-ISA checks validate host/device scratch-private, three
queues, **48 Cvec and 48 Q3_K results**, full guards and gated first use. The
**900-submission** reclamation check also passes with a stable allocation
plateau. An initial host-mode plateau failure was corrected by retaining the
shared ISA heap when local ISA is disabled; failed and passing attempts are
archived. All **1257 zebins** remain byte-identical; 24 unsupported variants
remain SPIR-V. Updated Linux raw GCC/CPU CTest passes **4/4**; the changed Strata
frontend and actual Linux GPU remain unvalidated.

Native auto sizing holds another **192 MiB** for local ISA slab packing on the
current 81.4-MiB padded-image pack, alongside the existing 512-MiB thread-pool
allowance. The requested **1536-MiB** reserve stays unchanged. Its one-request
auto smoke opens **6008 slots**, reports **1857 MiB** free after preparation and
matches all output/drafts. This is startup availability, not continuous
physical headroom; growing packs/queue first-use requires recalibration or
preallocation. [Local-ISA report](../orca-l0/demo/dg2-local-isa-windows-2026-11-01.json)
and source/image deltas, all requests, checks, exact runners and earlier attempts（本地归档：`../orca-l0/demo/dg2-local-isa-windows-2026-11-01.json.gz`）
preserve the evidence and reproduction parameters. Production remains unchanged.

The subsequent local-state revision puts ordinary module globals/constants in
local VRAM when the bound Runtime has an internal upload queue, and scratch
descriptors in local VRAM with local thread pools. The memory module retains
host backing for bootstrap. One first-use uploader reuses ISA staging and
preserves the caller recording/parameter heap. Module data is initialized once;
GPU writes persist across kernels and queues. Neither the Runtime API nor the
auto-cache budget changed. Both local placements disabled retain the host path.

The new module-data fixture validates initialized/zero-valued globals and
volatile constants, two kernels/queues, **16 updates and 17 readbacks per
placement**, exact values, full guards and stable allocations. Windows CTest
passes **19/19**, with local-ISA/thread-memory, 900-submission reclamation,
64 coordinate and eight 5-GiB-buffer checks. The updated raw Linux GCC/CPU CTest
passes **4/4**; frontend/GPU execution remains unvalidated. All **1257 numerical
zebins** remain byte-identical to the ISA checkpoint.

Two full five-process brackets keep the same **5771 slots**, 28-token input,
natural **76-token output**, **57/57 drafts**, balanced parameters and zero
prefix reuse. Ten warmups and five retained requests per process produce
**150 matching requests**. The final native-parent/candidate/native-parent
decode medians are **36.426 / 36.409 / 36.351 tok/s**, nominal **+0.06%**, with
**-0.21%** control drift. Prompt and TTFT change **+0.31% / +0.25%**. This is
effectively unchanged and does not establish a stable throughput improvement.
Original L0 controls are **48.624 / 48.596 tok/s** with **-0.06%** drift; native
remains **25.10% slower** against their geometric baseline. Preparation/loading/
prefill remain outside decode; host timing, unlocked clocks and populated OS
caches remain limitations. Broad PPL/long-context and GPU timestamps are open.

The retained first bracket shows nominal **+1.34%** with **+2.16%** native-control
drift. Its prototype used a 4-KiB descriptor host buffer; the final code uses
256 bytes with uploader zero padding to the identical 4-KiB GPU image. Compiler
disassembly measures prototype-to-final stack reservation **4888 → 1128 bytes**,
removing that Windows `__chkstk` call. The model comparison does not isolate a
benefit from this stack change.

The current auto smoke opens **5877 slots**, reports **1830 MiB** free after
preparation against **1536 MiB** requested and matches every output/draft. Slot
counts follow dynamic OS availability; timed brackets fix 5771 independently.
This is a startup sample, not continuously measured physical headroom.
[Local-state report](../orca-l0/demo/dg2-local-state-windows-2026-11-01.json) and
source/image deltas, both brackets, module fixture, disassembly and exact runners（本地归档：`../orca-l0/demo/dg2-local-state-windows-2026-11-01.json.gz`）
preserve the evidence. Production remains unchanged. The small/no model benefit
leaves parameter storage, state programming, barriers and queues as separate
investigation areas, without attributing the remaining gap to any one of them.

The native encoder already reuses identical global state, but it still emits
a CS stall plus HDC/untyped-cache flush after each kernel. State changes can
also require invalidation and repeated SBA programming. Intel's
[command encoder](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/command_container/command_encoder_xehp_and_later.inl)
and [Xe HPG workaround helper](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/release_helpers/release_helper/release_helper_common_xe_hpg.inl)
show hardware-dependent conditions, including required multi-CCS invalidation.
This is a candidate for dependency-aware measurement, not evidence that the
required workaround can be removed or that barriers account for the whole gap.

Native Windows currently uses `D3DKMTSubmitCommand`. Intel's
[WDDM implementation](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/os_interface/windows/wddm/wddm_interface.cpp)
also has a hardware-queue path selected by capabilities; Microsoft documents
[`D3DKMTSubmitCommandToHwQueue`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtsubmitcommandtohwqueue).
Availability and actual use by this installed Intel driver are not established.
Microsoft's [GPU virtual-memory documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/gpu-virtual-memory-in-wddm-2-0)
also states that the UMD retains responsibility for allocation residency.
Submission, residency/cache policy and kernel occupancy therefore remain
separate investigation areas. These upstream references are not a verified
description of every branch taken by the local proprietary Windows driver.

Here, "native DG2" names Orca's replacement for the user-mode driver; both
paths run on the same DG2 A770. The measured regression cannot be attributed
to DG2 versus another GPU architecture. Keeping Intel's UMD while loading
zebin and using direct DDI was effectively neutral in the earlier full-model
comparison. Replacing that UMD also transfers its state programming, memory
placement, copy scheduling and completion responsibilities to Orca.

There is also a concrete event/submission difference in the current adapters.
In `sycl/orca_dg2_backend.hpp`, `Stream::barrier(signal)` freezes a segment;
`Stream::replay` submits each recorded segment separately. The existing L0
path in `orca-l0/src/runtime.cpp` appends the signal through
`zeCommandListAppendBarrier` inside a command list. An uninterrupted 400-kernel
microbenchmark does not exercise this model's event-driven segmentation.
Inline GPU event signaling is therefore a separate candidate from polling
or changing CPU wait duration. Its performance contribution is unmeasured.

A subsequent original-L0/native/original-L0 bracket holds the local-state
sources, binaries, images, 5771 slots and balanced parameters unchanged. All
**69 requests** match token IDs and draft acceptance. Decode still excludes
loading, preparation and prefill:

| Input / output | Original L0 decode, geometric control baseline | Native decode | Change | Original L0 / native prefill |
| --- | ---: | ---: | ---: | ---: |
| Short28 / natural76 | 48.117 tok/s | 35.370 tok/s | -26.49% | 844.5 / 1049.9 ms |
| Code2K / 64 | 26.419 tok/s | 21.473 tok/s | -18.72%, inconclusive | 28107.8 / 27678.2 ms |
| Long8K (8000 tokens) / 64 | 28.642 tok/s | 21.546 tok/s | -24.77% | 75878.7 / 78466.5 ms |

Code2K decode controls drift **+9.42%** and fail the 3% bound. Short and 8K
decode controls drift **+0.46%** each. Long cases discard one warmup and retain
three requests; their expert caches continue adapting, so the report includes
every retained value and does not establish a converged steady-state plateau.
The short case retains the prior ten-warmup/five-request protocol. Different
contents and MTP acceptance across cases also prevent attributing a cross-case
change solely to context length. Original/native accepted-offered drafts are
**57/57**, **38/44** and **32/44** respectively, matched within each case.

On 8K, prefill and TTFT increase **3.41% / 3.44%**, and total request latency
increases **4.17%**; decode throughput still falls **24.77%**. The long prefill
dilutes the whole-request gap without removing the decode regression. These
host observations favor investigating small-operation/event submission paths;
they do not isolate GPU versus CPU time or prove a particular cause.
[Context diagnostic](../orca-l0/demo/dg2-context-windows-2026-11-01.json) and
all requests, exact runner, logs and 11 cached pinned NEO sources（本地归档：`../orca-l0/demo/dg2-context-windows-2026-11-01.json.gz`）
preserve the evidence. This bracket predates the following eager-batch changes;
historical shared build paths may advance, so reproduce with the archived
source revision and matching executable hashes.

Two additional policy differences deserve isolated checks. Intel's
[Xe HPG encoder](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/xe_hpg_core/command_encoder_xe_hpg_core.cpp)
selects a thread-group dispatch size from the threads per workgroup when
disable-overdispatch is available. Orca currently leaves that field at its
packet default, TG8. Intel's
[SLM calculation](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/command_container/command_encoder_xe_hpg_core_and_xe_hpc.inl)
and [DG2 G10 SLM table](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/release_helpers/release_helper/release_helper_preferred_slm_xe_hpg_g10.inl)
choose preferred SLM allocation from the workload and hardware revision;
Orca currently requests MAX. These are verified source differences, not
measured explanations of the remaining throughput gap. The native transfer
path also uses compute copy/fill kernels on CCS0 rather than implementing a
dedicated blitter transport. The installed Intel driver's actual engine
choice has not been traced. Linux GPU performance remains unmeasured.

## Default mixed-operation batching

The next Strata DG2 adapter revision counts kernels, copies and fills toward
one eager-batch limit, retaining **64** by default. Previously only kernels
counted. `STRATA_ORCA_DG2_BATCH_OPS=1..4096` is strictly parsed once before GPU
initialization. No worker or timer waits for more work; signals, dependencies,
readbacks and queue switches flush the tail. Contiguous 2D copies use one flat
transfer; strided readbacks batch rows within the limit and flush at the end.
Recorded graph signal segments remain separate, and no arithmetic fusion or
reusable native Runtime API change is added.

Windows CTest passes **19/19**, plus 64 coordinate replays, eight 5-GiB-buffer
replays, local-ISA/thread modes and 900-submission reclamation. Separate checks
cover limits **1/64/4096**, eight invalid settings, changing inputs, complete
guards and padded/contiguous 2D copies. A 601-operation mixed dependency chain
uses **10 / 3 / 1 submissions** at limits 64 / 256 / 1024. This compares limits
of the new binary, not the old kernel-only threshold. The final micro bracket
gives **3.141 / 3.401 ms** for 64-control/256, **8.25% slower** at 256, with
**-1.07%** control drift. The 1024 bracket misses the 3% drift bound. All three
earlier prototype brackets are retained too; larger batches do not establish
a consistent latency advantage. The 64 default is conservative, not universally
optimal. Timing includes host encoding/submission/wait, not GPU timestamps.

The matched old-native/new-native/old-native full-model bracket gives
**35.998 / 35.947 / 36.103 tok/s**, **-0.29%** against the geometric control
baseline, with **+0.29%** control drift. Prompt/TTFT/request time change
**+0.06% / +0.06% / +0.01%**. It is effectively unchanged. Ten warmups and five
retained requests per process produce **45 matching requests**: 28 input tokens,
natural76 output, 57/57 drafts, 5771 slots, balanced parameters and no prefix
reuse. Decode excludes preparation/loading/prefill. All **1242 non-queue model
zebins** and other non-manifest model files are byte-identical to the context
checkpoint. The 46 changed manifests differ only in compilation timing;
filenames, flags and sizes are checked. These are two native revisions, not
an original-SYCL or original-L0 comparison.
[Batch report](../orca-l0/demo/dg2-auto-batch-windows-2026-11-01.json) and
source/image deltas, all micro attempts, requests, logs and seven pinned upstream sources（本地归档：`../orca-l0/demo/dg2-auto-batch-windows-2026-11-01.json.gz`）
retain the evidence. The shared build helper was captured after validation;
model/micro runners were saved per run. Raw Linux sources still match the prior
4/4 CPU checkpoint; changed Strata frontend, Linux GPU, broad PPL and post-change
long-context execution remain unvalidated. Production remains unchanged. Eager
batching alone has not closed the previously measured native-driver gap.

## What SYCL, UR and NEO already do

The original chain is Strata → SYCL → UR's Level Zero adapter → Intel's
Level Zero user-mode driver (NEO) → the OS kernel driver → A770. SYCL and NEO
are different layers; Level Zero is an API implemented by the driver, and
zebin is a native module format. Loading zebin still requires kernel ABI,
memory, synchronization and command-stream management.

The following observations are pinned to LLVM
`e9b0e65b0e56f9286aeb0bf00649c7a4877dea6b` and NEO
`ee495cce3db8f870a89697febbd7e8b6680151b2`. They describe upstream mechanisms,
not a trace of the installed oneAPI 2025.3.2 or Windows driver's active paths.

| Layer | Existing optimization | Implication for Orca |
| --- | --- | --- |
| SYCL | Task/dependency management and recorded graph execution; the scheduler removes redundant same-queue event dependencies for eligible in-order commands | SYCL calls do not each imply a new GPU submission or a full general-purpose dependency walk |
| UR, ordinary L0 lists | Separate compute/copy batches, fixed or adaptive sizes; idle-queue and blocking/explicit flush paths can submit early | A bounded batch should preserve progress and observability, rather than delay useful work until full |
| UR, optional immediate completion batching | A batch-tail barrier represents many event completions for cleanup | Aggregate completion queries separately from command submission |
| NEO | In-list GPU events, multiple-list execution, resource residency and dirty-state tracking | An event need not force another OS submission, and unchanged GPU state need not be emitted repeatedly |
| NEO, optional Direct Submission | Resident GPU command ring and semaphore-based progress, with restart/residency handling | A ring is useful below the CPU request queue too; the installed driver's use is unverified |

In the pinned [UR queue implementation](https://github.com/intel/llvm/blob/e9b0e65b0e56f9286aeb0bf00649c7a4877dea6b/unified-runtime/source/adapters/level_zero/queue.cpp),
the ordinary-list adaptive configuration starts at **4** commands with a
default maximum of **64**. Repeated full lists raise the size slowly; frequent
early closure lowers it. When the queue appears idle, dynamic batching sends
work promptly. `UR_L0_BATCH_SIZE` and `UR_L0_COPY_BATCH_SIZE` can select fixed
sizes; the immediate path is separate. The optional completion-batch code uses
**256 events** per batch and at most **10 batches**, but is **disabled by default**
in this revision and requires an out-of-order immediate queue. These numbers
are source defaults, not measurements or recommended settings for this model.
The [SYCL scheduler](https://github.com/intel/llvm/blob/e9b0e65b0e56f9286aeb0bf00649c7a4877dea6b/sycl/source/detail/scheduler/commands.cpp)
also explicitly removes eligible redundant in-order event dependencies.

NEO's [regular queue path](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/level_zero/core/source/cmdqueue/cmdqueue_hw.inl)
programs required state and links command lists through batch-buffer starts.
Its [immediate path](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl)
has distinct flush/submission choices; immediate append does not mean a CPU
wait after every kernel. The [Direct Submission implementation](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/direct_submission/direct_submission_hw.inl)
submits a ring when it needs starting, then can unblock the running GPU for
later work. Capabilities, OS, residency and restart conditions still apply.

These mechanisms batch commands, not arbitrary arithmetic. Combining compatible
GEMVs into GEMM or fusing operators needs tensor shapes, shared weights and
dependency knowledge in Strata. Existing graph replay and CPU/GPU overlap also
mean that per-call host costs cannot simply be summed into recoverable token
latency. Orca's preceding recorded signal segmentation was a concrete difference;
the following revision removes eligible boundaries but has not established a
meaningful model-throughput benefit.

## In-list GPU event signals and waits

Eligible recorded GPU signals now emit flushing `PIPE_CONTROL` immediate writes
to pooled tags; dependent GPU work uses in-list `MI_SEMAPHORE_WAIT` equality.
Tags occupy separate 64-byte cache lines, 64 per staging allocation. Configuration
`STRATA_ORCA_DG2_INLINE_EVENTS=0|1` is parsed once, default **1**. Staged CPU
readbacks and required host/queue transitions retain submission boundaries.
Replay finishes old immutable points before resetting tags, and intermediate
completion never advances the queue fence or retires tail resources. Existing
required barriers and XeHPG multi-CCS invalidation remain in place.

Windows **20/20 CTests** and both sets of 16 gated-tail replays pass, including
cross-queue prefix readback, event reuse and full guards. Linux **4/4 raw CPU
checks** pass, without a GPU or a Linux oneAPI frontend; this does not validate
the changed Strata frontend or Linux GPU execution. Invalid configuration and
native packet/alignment/range checks pass. Partial-tag host waits use a short
spin/sleep fallback; full queue fences retain OS event waits.

An event-heavy microbenchmark records 200 dependent kernels and 200 signals.
Same-binary off/on/off submissions are **200 / 1 / 200**, with median
replay/submit/wait **9.91415 / 0.62150 / 9.97220 ms** over 24 retained samples
per arm. Relative latency is **-93.75%**, control drift **+0.59%**. Recording,
initialization and verification are excluded; this is host wall time, not GPU
timestamps or end-to-end model throughput.

Two matched native-control/candidate/control model attempts retain all **90
exact natural76 requests**, 57/57 drafts, 5771 slots, min-p=0.5, requested
1536 MiB reserve and no prefix reuse. Ten warmups precede five retained
requests per arm. Preparation/loading/prefill remain outside decode timing.
The first attempt shows decode **+0.53%** with **-2.39%** control drift, but
late control deterioration invalidates its TTFT/request comparisons. The
rerun gives **35.534 geometric-control / 35.326 candidate tok/s**, **-0.58%**,
with **+1.13%** control drift; TTFT is **+0.58%**, with **-0.33%** drift. There
is no established meaningful model speedup. These compare native DG2 revisions,
not original SYCL or original L0.

All **1242 non-queue model zebins** and retained SPIR-V remain byte-identical.
The 46 model manifests differ only in compilation timing. Two generated ABI
headers differ solely in debug-only source line numbers; the checked Release
build defines `NDEBUG`, whose return path is unchanged. The first raw preflight
used an invalid fixture assumption about an adjacent registered allocation;
the corrected test passes and both attempts remain archived.
[Event report](../orca-l0/demo/dg2-inline-events-windows-2026-11-01.json) and
all attempts, exact runners, deltas and logs（本地归档：`../orca-l0/demo/dg2-inline-events-windows-2026-11-01.json.gz`）
preserve the evidence. New long contexts, broad PPL and Linux GPU performance
remain unmeasured for this revision.

## Short-model phase localization

The following diagnostic adds capture-time counts to the existing
`STRATA_VERIFY_NODES` branch in `sycl/src/core/verify.cpp`. No queue encoding or
arithmetic changes. The native model zebins, SPIR-V and generated ABI headers
are byte-identical to the preceding event revision; 47 manifests, including
the queue pack, differ only in compilation timing. Windows remains **20/20
CTests**. This identity check covers native revisions; the L0 control uses its
preserved production image pack.

Production Orca L0 through Intel UMD / native DG2 / production L0 retains 45
exact natural76 requests, 57/57 drafts, 5771 slots, min-p=0.5, requested
1536 MiB reserve and no prefix reuse. Ten warmups precede five retained
requests per arm. `STRATA_DECODE_TIMING=1` and `STRATA_VERIFY_NODES=1` enable
existing host-clock diagnostics plus the new counts. Initialization and
prefill remain excluded from decode; diagnostic logging is enabled, so these
numbers are not an updated release-throughput claim or GPU timestamps.

| Per decode window | Geometric L0 control | Native DG2 | Difference |
| --- | ---: | ---: | ---: |
| Total | 80.58360 ms | 107.99000 ms | +27.40640 ms |
| Verifier | 66.68377 ms | 92.51000 ms | +25.82623 ms |
| Draft | 12.98478 ms | 15.05000 ms | +2.06522 ms |
| Commit/emit | 0.13000 ms | 0.17000 ms | +0.04000 ms |
| Verifier submission call | 0.06695 ms | 0.03130 ms | -0.03565 ms |
| Submit return through completion | 66.49916 ms | 92.30165 ms | +25.80249 ms |

There are 20 windows per request, averaging 3.85 input rows and 3.8 output
tokens per window. Decode measures **47.15747 / 35.18844 tok/s**, **-25.38%**,
with **+1.19%** control drift. Total-window/verifier/draft controls drift
**-1.17% / -1.21% / -1.15%**, respectively. Mean submit-call controls drift
**+2.42%**; maximum-submit drift is **+6.12%**, so its apparent improvement is
not a stable comparison. Host submission can overlap GPU progress; do not
add it to the other intervals. Independently rounded median timers do not
form exact additive phase accounting.

All six T=1..6 verifier graphs contain **one submission segment**, **zero
signals, zero queued event waits, zero staged CPU readbacks and zero host
uploads**. At T=4 there are 1769 kernel nodes and 1557 explicit barrier nodes.
This is graph structure, not a complete request-level OS-submission count.
The model does not exercise the event-heavy microbenchmark's 200-to-1
submission reduction in its verifier graph. An eager batch limit does not
split these retained graphs or wait for them to fill.

Expert VRAM-hit counts match exactly across every corresponding request in
all three 15-request arms; the retained median is **91.7397%**. The no-host
path's CPU expert counters are zero by construction and must not be mistaken
for all experts being in VRAM. Matching residency counters rule out a
different hit ratio here, while leaving the cost of GPU reads from host memory
as an open factor.

The principal extra latency is localized to the verifier's
submission-through-completion interval, with a smaller draft regression.
CPU submission itself is faster. This does not yet distinguish GPU execution,
PCIe/cache stalls, internal barriers and completion observation.

One specific implementation difference at this checkpoint motivates the
following isolated ordering check.
`orca-l0/src/runtime.cpp` maps relaxed recording to
`ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING`. In contrast,
`sycl/orca_dg2_backend.hpp::Stream::begin_recording(Ordering)` discarded
the parameter; `orca-l0/src/drm/encoding.hpp::record_dispatch` emitted
a flushing barrier after every kernel. The upper queue already marks known
independent phases and emits explicit joins, so conservative per-kernel
ordering may constrain overlap and repeat some dependency flushes. This is a
source-level candidate, not a measured root cause or permission to remove
hardware workarounds. TG dispatch and preferred-SLM policies remain independent
candidates. No barriers/workarounds changed in this diagnostic.

[Phase report](../orca-l0/demo/dg2-phase-localization-windows-2026-11-01.json) and
every request, exact runners, source delta, policy references and logs（本地归档：`../orca-l0/demo/dg2-phase-localization-windows-2026-11-01.json.gz`）
preserve the evidence. Linux frontend/GPU, broad PPL and post-event long
contexts remain unvalidated.

## Explicit-dependency recording and image identity

The next native revision honors `Ordering::relaxed` requested by Strata.
Raw `Runtime::begin(queue)` remains ordered; `begin(queue, false)` omits the
automatic trailing per-kernel dependency flush. Caller-recorded joins,
completion flushes, leading invalidations and repeated-SBA workarounds remain.
Eager operations stay ordered. `STRATA_ORCA_DG2_RELAXED=0|1`, parsed once before
GPU initialization, defaults to **1**. The dependency check does not measure
concurrent kernel occupancy.

Two same-binary ordered/relaxed/ordered brackets preserve **90 exact natural76
requests**, 57/57 drafts and the same balanced parameters, ten warmups and five
retained requests per arm. The fixed revision measures **35.469 / 36.087 tok/s**
for geometric control/candidate: **+1.74%**, control drift **+0.12%**. TTFT
changes **-0.95%**, with **-0.21%** drift; request time changes **-1.67%**.
The prototype also measured **+1.74%**, with **-0.33%** control drift. These
compare native ordering modes, not original SYCL or L0, and do not explain the
preceding roughly 25% gap. Decode still excludes preparation and prefill.

Windows passes **20/20 CTests**, full queue checks pass in both modes, and eight
malformed flags fail before GPU construction. Shared Linux CPU checks remain
**4/4**. A rejected transfer after activating an empty relaxed recorder exposed
an ownership leak. The new regression fails before the fix and passes after it,
including empty replay and dependent eager work; both model revisions and the
failed regression are archived separately. All **1242 non-queue model zebins**
and retained SPIR-V remain byte-identical to the phase checkpoint.

Entry-point matching against production L0's preserved SPIR-V resolves the
earlier filename-only comparison: **1266 shared entries**, all with equal
compiler options. **1258 are byte-identical**; eight differ only in `OpName`
debug names. All non-debug instruction words match exactly. This narrows the
source/IR explanation without proving identical JIT-generated ISA, cache
policy or command encoding. Two production-only exports are the unsupported
SG16 matrix variant and a GPU-stamp diagnostic. Native has no extra exports.

[Ordering report](../orca-l0/demo/dg2-relaxed-ordering-windows-2026-11-01.json) and
both brackets, complete checks, source/image deltas, SPIR-V audit and exact runners（本地归档：`../orca-l0/demo/dg2-relaxed-ordering-windows-2026-11-01.json.gz`）
preserve all evidence. Post-change long contexts, broad PPL and Linux GPU
execution remain open. Production configuration and executable are unchanged.

## Zero-SLM preferred allocation

The next isolated change uses **0KB** preferred allocation when static plus
dynamic SLM size is zero; nonzero SLM retains **MAX**. This matches the zero
branch of the pinned NEO calculation, without implementing its nonzero-SLM
occupancy/revision policy. It changes no arithmetic, barriers or workarounds.
Windows passes **20/20 CTests**, Linux raw CPU **4/4**, and packet checks cover
zero, static and dynamic SLM. All kernel images and ABI files are byte-identical.

A preserved-native/candidate/preserved-native bracket retains **45 exact
requests**, 57/57 drafts, ten warmups and five retained per arm, with the same
balanced short-anchor parameters and relaxed ordering. Decode is **35.944
geometric control / 35.988 tok/s**, **+0.12%**, control drift **-0.40%**; TTFT
changes **+0.03%**. There is no established throughput benefit. A preferred
allocation field does not measure physical SLM consumption or occupancy.
[SLM report](../orca-l0/demo/dg2-zero-slm-windows-2026-11-01.json) and
complete checks, source delta, pinned references and every model request（本地归档：`../orca-l0/demo/dg2-zero-slm-windows-2026-11-01.json.gz`）
preserve the evidence. Linux GPU/frontend and new long-context checks remain
unvalidated; production is unchanged.

## Explicit GPU dependency flushes

A further source check finds that `Runtime::barrier()` still called
`detail::barrier()` with its default **host-visible DC flush**, even though
automatic dispatch dependencies already used the calibrated device policy.
The earlier T=4 verifier graph contains **1557 explicit barrier nodes**, so
relaxed recording did not eliminate this path.

The isolated fix passes `state_->host_visibility` to that call: **false on
the tested Windows backend**, unchanged **true on Linux**. Command-streamer
stall and HDC/untyped flushing remain, as do required state workarounds.
`signal_value()` and queue-tail completion retain host-visible flushing.
Pinned NEO's [ordinary compute-barrier flags](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl#L585)
enable HDC/untyped flushing, with `dcFlushEnable` defaulting false in
`PipeControlArgs`; host-visible event processing is separate. This motivates
the policy, without proving identical packets in the installed Intel driver.

Windows passes **20/20 CTests**, including cross-queue prefix reads and gated
tails, event ownership and host callbacks. Linux raw GCC/CPU checks pass
**4/4**; its effective barrier policy is unchanged. Kernel images, SPIR-V and
generated ABI files are all byte-identical to the preceding SLM revision.

| Short-anchor measurement | Geometric native control | Candidate | Change |
| --- | ---: | ---: | ---: |
| Decode | 36.396 tok/s | 38.563 tok/s | +5.95% |
| Prefill | 1023.95 ms | 994.80 ms | -2.85% |
| TTFT | 1084.10 ms | 1053.04 ms | -2.86% |
| Request | 3109.73 ms | 2969.86 ms | -4.50% |

The control/candidate/control decode medians are **36.435 / 38.563 / 36.357**,
with **-0.22%** control drift. All **45 requests** match natural76 output and
**57/57** drafts, with 5771 slots, balanced min-p=0.5, requested 1536-MiB
reserve, no prefix reuse, ten warmups and five retained requests per arm.
Decode excludes preparation/loading/prefill; these remain host observations,
not GPU timestamps. TTFT and request control drift are **-0.07% / +0.18%**.

This controlled change identifies over-broad host flushing as one source of
native overhead. It does not fully explain the earlier L0 gap or update the
original-SYCL comparison. Ordering, SLM and dependency-flush gains come from
separate brackets and must not be summed. New long-context, broad PPL and Linux
GPU performance checks remain open; production is unchanged.
[Dependency-flush report](../orca-l0/demo/dg2-dependency-flush-windows-2026-11-01.json) and
complete checks, one-source delta, cached references and every model request（本地归档：`../orca-l0/demo/dg2-dependency-flush-windows-2026-11-01.json.gz`）
preserve the evidence.

## Updated context bracket after the flush fixes

A fresh production-Orca-L0 / latest-native-DG2 / production-Orca-L0 bracket
uses the validated dependency-flush revision, including explicit-dependency
recording and zero-SLM preference. Source, executable, image and production
hashes are checked before and after; no product change occurs during the run.
All **81 requests** match complete output IDs and accepted/offered drafts.
Each process executes Short28, Code2K, then Long8K, with no prefix reuse.

| Workload | L0 geometric control | Native DG2 | Decode change | Decode control drift |
| --- | ---: | ---: | ---: | ---: |
| Short28, natural76 | 47.060 tok/s | 38.699 tok/s | -17.77% | -1.17% |
| Code2K, 64-output cap | 30.613 tok/s | 25.601 tok/s | -16.37%* | -3.45%* |
| Long8K, 64-output cap | 29.713 tok/s | 24.077 tok/s | -18.97% | -0.78% |

*The 2K decode control exceeds the predeclared **3%** drift bound. Its exact
regression magnitude is not established. The three index-matched retained
comparisons are **-16.20%, -16.37%, -17.38%**, with corresponding control drift
**-1.51%, -3.45%, -1.05%**. Both long cases continue adapting the expert cache
after three discarded warmups; three retained requests do not establish a
steady plateau. Short discards ten warmups and retains five.

The same 5771 slots, balanced spec-min-p=0.5, requested 1536-MiB reserve,
fixture IDs, case order and per-case output counts apply in all arms. Drafts
are **57/57**, **38/44**, **32/44** for Short/2K/8K respectively. The native
process loads no Intel UMD, Level Zero, SYCL, UR, compiler or GMM modules.
These are host/protocol times; decode excludes initialization, preparation,
loading and prefill. They are not GPU timestamps.

| Timing | Short change | 2K change | 8K change |
| --- | ---: | ---: | ---: |
| Prefill | +14.48% | -3.59% | +3.81% |
| TTFT | +15.01% | -3.54% | +3.83% |
| Whole request | +19.33% | -2.02% | +4.34% |

Control drift for these timing metrics stays below 3%. FP16 expert-pair work
also matches in every long request across the three processes: **9159 pairs,
22 leftovers** for 2K; **9794 pairs, 24 leftovers** for 8K, using the GEMM
workspace without extra scratch. The remaining gap is not an absent pair path.
On 8K, a much longer prefill dilutes whole-request differences while decode
still regresses about 19%; this round does not support context length removing
the native decode gap. Different prompts and MTP acceptance across cases
prevent treating the gap trend as a controlled experiment on length alone.

The historical context bracket predates batching/event/ordering/flush fixes
and used one long warmup instead of three. Its Short/8K gaps were -26.49% /
-24.77%, but comparing those percentages with this round does not isolate the
combined code changes. Individual gains from separate native revision brackets
must not be summed. Broad PPL and Linux GPU validation remain open; production
configuration and executable remain unchanged.
[Latest context report](../orca-l0/demo/dg2-context-latest-windows-2026-11-01.json) and
all matched curves, exact runner, FP16 pairing checks, logs and pinned references（本地归档：`../orca-l0/demo/dg2-context-latest-windows-2026-11-01.json.gz`）
preserve the completed run.

## Capture-time GPU state attribution

A separate staged copy adds diagnostic logging around the existing encoder
and list closure. It links into frozen engine objects without modifying live
sources or changing packet encoding. Raw CPU/GPU checks pass, followed by one
exact natural76 request with **57/57 drafts**, 5771 slots and no Intel UMD,
Level Zero, SYCL, UR, compiler or GMM modules loaded. Six verifier graphs are
captured before READY. Nested first-use upload lists are tracked by queue.

| Graph T | Dispatches | Full state blocks | ISA-base changes | Parameter-base changes | Scratch-heap changes | GRF changes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1647 | 575 | 59 | 0 | 468 | 72 |
| 2 | 1689 | 1002 | 663 | 226 | 432 | 120 |
| 3 | 1731 | 1136 | 667 | 398 | 520 | 120 |
| 4 | 1773 | 1259 | 806 | 784 | 522 | 120 |
| 5 | 1815 | 1226 | 760 | 519 | 520 | 120 |
| 6 | 1857 | 1212 | 796 | 422 | 592 | 120 |

Dispatch counts include internal copy/fill kernels. Full-state counts include
one initial programming block per graph; field-change counts exclude it and
overlap when several fields change together. ISA span, maximum threads,
systolic mode, MOCS and EU-fusion mode never change within these graphs.

The T=4 list is **626240 bytes**: **71.01%** of its dispatches trigger the full
state path. Of its 1258 later transitions, **1138** change only heap/policy
fields, without changing GRF, pipeline or front-end settings. The conservative
encoder nevertheless emits preemption, pipeline select, compute mode, two SBA
packets and CFE together, preceded by an invalidating barrier. This identifies
concrete redundancy; it does not measure the cost of those commands or prove
that removing them recovers the model gap. Pinned NEO's encoder tracks dirty
heaps and pipeline/compute-mode properties separately.

This cold capture run is **not a performance benchmark** or a hardware-counter
trace. Diagnostic logging is absent from production. The next isolated tests
use only Short28: first suppress unchanged state groups while preserving all
existing synchronization/workarounds, then evaluate common instruction and
parameter addressing windows. Long-context tests are complete for this round
and are not being repeated for each adjustment.
[State report](../orca-l0/demo/dg2-state-trace-windows-2026-11-01.json) and
staged sources, exact runner/parser, setup step, frozen inputs and complete logs（本地归档：`../orca-l0/demo/dg2-state-trace-windows-2026-11-01.json.gz`）
preserve the diagnostic.

## Short-only partial state experiment

The first follow-up updates only the changed state groups: preemption on
initial programming, pipeline select on systolic changes, compute mode on GRF
changes, SBA on heap/MOCS changes, and CFE on front-end/scratch changes. All
existing leading invalidations, dependency/tail flushes and double-SBA
workarounds stay. The encoder and its nine-field CPU regression check are the
only two changed staged files; main product sources stay unchanged.

The isolated library passes **14/14 Windows native CTests**, **3/3 Linux CPU
CTests**, and actual device GEMV/XMX, scratch/private, atomics and local-ISA
checks. Frozen engine objects link to a private executable. All **45 Short28
requests** reproduce natural76 output and **57/57** drafts, with ten discarded
warmups and five retained requests per arm, 5771 slots, balanced min-p=0.5,
requested reserve 1536 MiB and no prefix reuse.

| Metric | Geometric native control | Private candidate | Change |
| --- | ---: | ---: | ---: |
| Decode | 38.695 tok/s | 38.013 tok/s | -1.76% |
| Prefill | 981.56 ms | 1001.80 ms | +2.06% |
| TTFT | 1038.93 ms | 1061.98 ms | +2.22% |
| Whole request | 2946.13 ms | 3012.79 ms | +2.26% |

Decode controls are **38.996 / 38.395 tok/s**, drifting **-1.54%**; all timing
control drifts stay below 3%. This single bracket establishes no gain from
suppressing unchanged state packets. It leaves the frequency of invalidating
state transitions unchanged, so it does not test avoiding heap-base switches.
The candidate remains isolated and is not adopted by default. An optional
legacy WDDM harness failed to compile in the first build attempt because its
required-local-size check predates the 3D ABI; the completed run disables that
separate target and runs all current native backend checks. Both attempts are
archived. Linux GPU execution and broad PPL remain unvalidated.
[Partial-state report](../orca-l0/demo/dg2-partial-state-windows-2026-11-01.json) and
complete checks, staged sources, failed attempt and every short request（本地归档：`../orca-l0/demo/dg2-partial-state-windows-2026-11-01.json.gz`）
preserve the result.

### Shared heap-window Short experiment

A separate completed native/candidate/native bracket tests sharing 4-GiB
instruction and parameter base windows during recording. Checked relative
offsets preserve each GPU address; allocations near a window boundary keep
their original base. Allocation ownership and residency do not change. This
reduces eligible heap-base transitions rather than only suppressing packets
inside each transition. The isolated encoder passes 14 Windows native CTests,
three Linux CPU CTests and actual GEMV/XMX, scratch/private, atomics and local-ISA
checks before the model test.

| Metric | Geometric native control | Shared-window candidate | Change |
| --- | ---: | ---: | ---: |
| Decode | 38.604 tok/s | 40.462 tok/s | +4.81% |
| Prefill | 989.04 ms | 945.70 ms | -4.38% |
| TTFT | 1047.02 ms | 1002.27 ms | -4.27% |
| Whole request | 2966.97 ms | 2827.00 ms | -4.72% |

All 45 Short28/natural76 requests match every output ID and 57/57 draft
acceptances. Ten warmups and five retained requests per process use 5771 slots,
requested reserve 1536 MiB and `spec-min-p=0.5`, with no prefix reuse. Decode
control drift is -0.49%; other timing control drifts are below 3%. This one
bracket supports a gain from shared windows on this A770/driver, without
isolating individual GPU packet costs. The standalone binary remains a historical
control; the combined policy below is now in the main native encoder. This
bracket establishes no desktop fix or Linux GPU result. It is a native/native
comparison and cannot be combined arithmetically with the separate L0 bracket.
The original interrupted experiment below remains incomplete.
[Shared-window results](../orca-l0/demo/dg2-short-heap-window-windows-2026-11-02.json)
and exact runner, changed sources and model logs（本地归档：`../orca-l0/demo/dg2-short-heap-window-windows-2026-11-02.json.gz`）.

### Same-module Short diagnostic

A five-process bracket now compares current-source Orca L0 with its portable
packs, the same L0 with shared zebin, native DG2 with that shared zebin, then
the two L0 controls in reverse order. The L0 control is freshly compiled with
native-module loader support; it is not the frozen production executable or
original SYCL. Production configuration and executable hashes remain unchanged.

Both shared-input backends use the same 1,242 referenced zebin files in 46
nonempty packs. The view excludes all 24 unsupported SPIR-V entries and omits
two resulting empty packs from `packs.txt`. Input hashes and device identity
are checked; neither shared-input arm can fall back to SPIR-V. This does not
compare instruction bytes after the driver's relocation or patching.

| Short decode comparison | Geometric control | Candidate | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Current L0 SPIR-V to current L0 zebin | 48.749 tok/s | 48.705 tok/s | -0.09% | -0.08% |
| Shared zebin, current L0 to native DG2 | 48.705 tok/s | 39.289 tok/s | -19.33% | +0.04% |

All 75 requests match every output token, the natural 76-token stop and 57/57
draft acceptances. Each process discards ten warmups and retains five requests;
the controls use the geometric mean of retained medians. Settings remain
`spec-min-p=0.5`, requested reserve 1536 MiB, 5771 expert slots and no prefix
reuse. Decode uses internal `decode_ms`, excluding loading, initialization,
preparation and prefill. Shared-input TTFT is 880.47 ms for L0 versus 1029.29 ms
for native DG2 (+16.90%, control drift +0.13%).

Replacing portable modules with these zebins does not remove the Short gap.
The remaining candidates include command/state programming, memory access and
completion notification; the result does not establish that all extra time is
GPU execution. Native process CPU use is about 0.024 cores versus 1.031 for the
shared-input L0 controls, in the nearby host-observed decode interval.

Two failed preparations remain archived: the frozen production loader rejected
zebin before any shared-input model request, then the first current-source
attempt left two empty pack entries and native DG2 rejected initialization.
They are not performance comparisons. The completed retry and both failures
are preserved in the
[same-module report](../orca-l0/demo/dg2-short-shared-modules-windows-2026-11-02.json)
and exact runner, build records and logs（本地归档：`../orca-l0/demo/dg2-short-shared-modules-windows-2026-11-02.json.gz`）.

### Full fence-polling Short diagnostic

A separate native/polling/native bracket replaces both WDDM monitored-fence
wait paths: immutable completion observers and owner `Runtime::wait_sequence`.
The isolated runtime polls the mapped fence with `_mm_pause()` while preserving
finite timeouts, cancellation and in-flight ownership. It does not register or
wait on an OS completion event in those two paths. Inline memory-tag waits are
unchanged. The engine objects, zebin inputs, GPU packets, allocations and model
parameters are unchanged; only the private runtime library is relinked.

Before timing, the candidate passes two CPU checks, 50 observer submissions and
61 multiqueue submissions. Those GPU checks cover finite/zero timeouts,
concurrent observations, old completion identity, destruction/cancellation,
queue dependencies, exact outputs and full guards.

| Metric | Geometric blocking control | Full fence polling | Change |
| --- | ---: | ---: | ---: |
| Decode | 39.206 tok/s | 39.674 tok/s | +1.20% |
| Prefill | 972.20 ms | 965.20 ms | -0.72% |
| TTFT | 1029.47 ms | 1021.38 ms | -0.79% |
| Whole request | 2909.89 ms | 2883.24 ms | -0.92% |
| Process CPU, nearby decode interval | 0.028 cores | 1.028 cores | about one additional core |

All 45 Short28/natural76 requests match every output ID and 57/57 drafts.
There are ten warmups and five retained requests per process, 5771 slots,
requested reserve 1536 MiB, `spec-min-p=0.5` and no prefix reuse. Decode uses
internal `decode_ms` and excludes preparation/loading/prefill. Decode control
drift is -0.04%; other timing control drifts are below 0.5%. CPU accounting has
15.625-ms quantization; small native/native CPU differences are imprecise.

This bracket does not establish a stable 1.20% gain, but removing blocking fence
waits does not recover the large native gap in this test. It does not exclude
other synchronization costs, nor separate CPU activity's clock/power effects
from wakeup latency. Full polling remains diagnostic-only. This result and the
independent +4.81% shared-window bracket cannot be added.
[Fence-polling report](../orca-l0/demo/dg2-short-fence-polling-windows-2026-11-02.json)
and changed source, functional checks and exact model logs（本地归档：`../orca-l0/demo/dg2-short-fence-polling-windows-2026-11-02.json.gz`）
preserve the completed experiment.

At this checkpoint, the investigation prioritized GPU state transitions and
memory-access policy. The baseline T=4 capture contains 522 scratch-heap changes
across 1773 dispatches; that encoder clears the scratch base for kernels without
scratch. The next section records validation and adoption of descriptor reuse;
the updated policy has no new state-count capture yet. Identical expert hit counts do not
exclude PCIe/cache/residency costs. GPU execution timestamps and actual clocks
remain unmeasured, so the remaining gap is not attributed entirely to GPU work.
Production L0 remains unchanged; no Linux GPU or desktop-fix claim follows.

### Shared windows and scratch retention adopted in native sources

The main native encoder now uses checked common 4-GiB instruction/parameter
windows and retains an already-owned scratch descriptor across a no-scratch
kernel in the same valid recording. Fresh recordings start invalid and program
their own scratch state. Kernels requiring scratch must still supply it; the
existing validation runs before reuse. The descriptor and its backing remain
retained by the earlier scratch dispatch, with independent writable pools per
queue. Memory ownership and allocation sizes do not change. Required
invalidation/workaround packets remain on actual state changes; explicit
dependency barriers stay. No runtime option or replay-time CPU work is added.

A five-process native/shared-window/combined/shared-window/native bracket
measures both the scratch increment and the combined net effect directly:

| Short decode comparison | Geometric control | Candidate | Change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Shared windows versus native | 39.165 tok/s | 39.701 tok/s | +1.37% | -0.23% |
| Scratch retention versus shared windows | 39.701 tok/s | 42.309 tok/s | +6.57% | -0.05% |
| Combined versus native | 39.165 tok/s | 42.309 tok/s | +8.03% | -0.23% |

The combined candidate also lowers prefill from 974.95 to 881.20 ms (-9.62%),
TTFT from 1032.79 to 933.49 ms (-9.61%), and whole-request time from 2914.47 to
2682.43 ms (-7.96%). All timing control drifts are below 0.6%. These are new
matched measurements, not the sum of the earlier +4.81% window and +1.20%
polling experiments. Window-only gain is smaller in this bracket; its earlier
single-bracket percentage is not a universal expectation.

All 75 Short28/natural76 requests match every output ID, stop condition and
57/57 accepted/offered drafts. Each process discards ten warmups and retains
five requests, with `spec-min-p=0.5`, requested reserve 1536 MiB, 5771 slots and
no prefix reuse. Internal decode excludes loading, initialization, preparation
and prefill. This is native/native, not original SYCL or a fresh L0 comparison.
Before timing, the candidate passes 14 Windows CTests, three Linux CPU CTests,
real GEMV/XMX, scratch/private and atomics checks, and three-queue
scratch/private-no-scratch/scratch chains with changed inputs and full guards.
[Five-arm measurements](../orca-l0/demo/dg2-short-heap-scratch-windows-2026-11-02.json)
and candidate sources, all checks, controls and model logs（本地归档：`../orca-l0/demo/dg2-short-heap-scratch-windows-2026-11-02.json.gz`）
preserve the completed isolated experiment before adoption.

The exact encoder and regression checks are then copied into main sources.
The old Windows harness's required-local-size check is also updated for the
three-dimensional metadata field; that previously broken optional target now
builds. The actual main sources pass **21/21 Windows CTests**, **3/3 Linux CPU
CTests**, the excluded native benchmark's CPU check, and one additional exact
Short model request linked against the main runtime. That cold request verifies
integration; it is not another rate comparison. Production `strata-iq2_xs.json`
and its frozen L0 executable stay byte-identical. Only the native DG2 policy
defaults change. Broad PPL, Linux GPU execution and desktop coexistence remain
unvalidated.
[Main-source adoption report](../orca-l0/demo/dg2-state-reuse-main-windows-2026-11-02.json)
and before/after sources, build commands and main-build checks（本地归档：`../orca-l0/demo/dg2-state-reuse-main-windows-2026-11-02.json.gz`）
record this distinction.

The Linux continuation at this checkpoint is
`build-orca-native-lower-20261031/dg2-current-handoff-20261102-v3/`, including
the adopted native policy and the Ninja-selection fix. Its **2,003 exported
files** pass **3/3 Linux GCC CPU checks** and **three ELF dependency audits**
against the exported source. The v2 snapshot below remains the immutable parent
of the earlier full oneAPI frontend compilation; the new policy still requires
real Linux GPU checks and fresh Linux-driver baking.
[Current exported-source CPU report](../orca-l0/demo/dg2-state-reuse-linux-handoff-cpu-2026-11-02.json)
and manifest, exporter, checks and continuation notes（本地归档：`../orca-l0/demo/dg2-state-reuse-linux-handoff-cpu-2026-11-02.json.gz`）
record the v3 source boundary.

### Parameter capture locality adopted after normalized-state diagnostics

A fresh diagnostic records the encoder's actual state after heap-window
normalization and scratch retention. Logging the raw incoming allocation
addresses would overcount transitions after those policies. The diagnostic
uses a private runtime copy; tracing is absent from the main library.

The current encoder still changes parameter bases when an unchanged kernel
payload is reused from another retained list's 4-GiB window. Main now copies
that payload into the current capture heap when the windows differ. Captures
remain immutable and retain their existing ownership and replay lifetimes.
The extra comparison and occasional copy occur during recording; replay,
arithmetic and dependency barriers do not change.

Two separate current-main/candidate/current-main Short brackets reproduce the
decode improvement:

| Bracket | Geometric main control | Candidate | Decode change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| First | 42.771 tok/s | 44.305 tok/s | +3.59% | +0.18% |
| Repeat | 42.764 tok/s | 44.390 tok/s | +3.80% | -0.29% |

Whole-request time falls 2.13% and 2.18%. Prefill changes +0.23% and +0.42%;
TTFT changes +0.29% and +0.48%, effectively unchanged in these brackets.
Every process discards ten warmups and retains five requests. All 90 requests
match every Short28/natural76 output ID, stop condition and 57/57 drafts, with
5771 expert slots, requested reserve 1536 MiB, `spec-min-p=0.5` and no prefix
reuse. Internal decode excludes initialization, loading, preparation and
prefill. These are native/native measurements; do not add the percentages to
older experiments or infer a fresh L0/original-SYCL comparison.

Both cold diagnostic requests also match exactly. Dispatch counts agree for
all six verifier graphs; the affected graphs show:

| Graph | Dispatches | Main full state blocks | Candidate full state blocks | Parameter-base transitions, main to candidate |
| --- | ---: | ---: | ---: | ---: |
| T=2 | 1689 | 336 | 127 | 226 to 1 |
| T=3 | 1731 | 487 | 128 | 398 to 0 |
| T=4 | 1773 | 591 | 131 | 516 to 1 |

T=1/5/6 retain 77/132/134 full state blocks in both captures. Counts depend on
allocation layout and are capture diagnostics, not GPU timing. Transition
reason counts exclude the initial fresh state and can overlap. The new T=4
capture still has 120 GRF transitions, eight ISA-window transitions and one
scratch-heap transition; these guide further work without proving their costs.
[Two-bracket report](../orca-l0/demo/dg2-parameter-window-windows-2026-11-02.json)
and runners, sources, normalized-state records and logs（本地归档：`../orca-l0/demo/dg2-parameter-window-windows-2026-11-02.json.gz`）
preserve the isolated experiment, including the diagnostic setup failures.

The exact recording change is then adopted in main and rebuilt. Actual main
passes **21 Windows CTests**, **three Linux GCC CPU CTests**, the native
benchmark's CPU check and one more exact cold Short request. Linux's actual
`runtime.cpp` translation unit compiles; this is not a new full oneAPI frontend
build or a Linux GPU run. Production L0 configuration/executable remain
byte-identical; broad PPL and desktop coexistence remain unvalidated.
[Main-source adoption](../orca-l0/demo/dg2-parameter-window-main-windows-2026-11-02.json)
and before/after source and actual-main checks（本地归档：`../orca-l0/demo/dg2-parameter-window-main-windows-2026-11-02.json.gz`）
contain the new complete source map. Earlier benchmark runners pin older
sources and require an explicit checkpoint update before reuse.

The source-only Linux continuation for this policy is
`build-orca-native-lower-20261031/dg2-current-handoff-20261102-v4/`.
Its **2,007 exported files** pass **three Linux GCC CPU checks and three ELF
dependency audits** against the exported source. None of Level Zero, SYCL,
UR, Intel GPU UMD/compiler or libdrm is linked. The v3 snapshot above remains
the immutable pre-locality checkpoint. Linux GPU checks still require fresh
images baked by the Linux driver; the earlier full oneAPI frontend compilation
belongs to the v2 parent.
[Current Linux export verification](../orca-l0/demo/dg2-parameter-window-linux-handoff-cpu-2026-11-02.json)
and manifest, exporter, logs and continuation notes（本地归档：`../orca-l0/demo/dg2-parameter-window-linux-handoff-cpu-2026-11-02.json.gz`）
preserve the v4 source boundary.

### Fresh same-zebin L0 comparison and isolated GRF state update

A new L0/main/GRF-candidate/main/L0 Short bracket uses the same **1,242 zebin
files in 46 active packs** in every arm. The current-source L0 executable is
reused after verifying its applicable inputs: only four native encoder,
recording and native-test files changed since that control was built. This
control is neither the frozen production executable nor original SYCL.

| Path | Decode | Change against current L0 |
| --- | ---: | ---: |
| Current-source Orca L0, geometric outer controls | 48.528 tok/s | reference |
| Latest main native DG2, geometric inner controls | 44.171 tok/s | -8.98% |
| Private GRF-only state update | 44.338 tok/s | -8.63% |

The main native path takes **4.89% longer in prefill**, **5.63% longer to first
token** and **8.26% longer for the whole request** than the matched L0 controls.
The outer L0 decode controls drift -0.43%; the inner main controls drift +0.65%.
These are fresh observations of the remaining gap, rather than a subtraction
of rates from different checkpoints. Every process discards ten warmups and
retains five requests; all **75 requests** match every output ID, stop condition
and 57/57 drafts, at 5771 expert slots, requested reserve 1536 MiB,
`spec-min-p=0.5` and zero prefix reuse. Decode uses engine `decode_ms`, excluding
loading, initialization, preparation and prefill.

The GRF candidate removes unchanged pipeline/preemption, double-SBA and CFE
programming only when GRF is the sole global-state change. It retains SCM,
the complete leading invalidation and all dependencies; fresh and coupled
changes retain the original preamble. The pinned NEO synchronization path may
require extended invalidations with multiple CCS enabled. The installed
effective CCS count remains unknown, so this experiment does not remove those
barriers. It passes **14 Windows CTests**, **three Linux CPU CTests** and **five
real GPU numerical groups**, including scratch, private memory, atomics and
multiple queues. Its decode increment is only **+0.38%** against the inner
main controls, below their drift magnitude. A reproducible benefit is not
established, and the candidate is **not adopted**.

Passive CPU accounting gives about **0.027 main CPU cores versus 1.024 for
L0** over the host-observed decode delivery interval. This again does not
support CPU saturation as the native regression's explanation. Fence wakeup,
GPU execution and GPU clocks remain unseparated; native GPU timestamps were
still uncalibrated in that bracket. Identical input binaries also leave on-device relocation,
allocation cache/PAT policy and residency open.

The source review confirms that this A770 is revision **08**, which pinned NEO
maps to C. Its TG2/TG1 dispatch policy was already tested at an earlier
checkpoint without a gain; the newly prepared revision-gated variant remains
unbuilt rather than repeating that experiment without new evidence.
[Earlier thread-group experiment](../orca-l0/demo/dg2-strata-thread-group-windows-2026-11-01.json)
preserves that separate result. Preferred nonzero SLM allocation remains a
source difference: main requests MAX, while NEO derives a size from subslice
threads and aligned group usage. Actual topology and stepping must guide a
future isolated check; CFE's maximum-thread field is not a subslice topology
measurement. No performance benefit is claimed for this untested change.

[Fresh comparison and GRF report](../orca-l0/demo/dg2-grf-only-windows-2026-11-02.json)
and all requests, source bytes, references, exact runners and logs（本地归档：`../orca-l0/demo/dg2-grf-only-windows-2026-11-02.json.gz`）
preserve the completed check. Main product sources and production pins remain
unchanged; the immutable v4 Linux export above is still current. No new Linux
GPU, broad PPL or desktop-flicker result follows from this bracket.

### Nonzero SLM preference rejected; raw timestamps adopted

A separate main/SLM-candidate/main Short bracket tests NEO's large-data policy:
prefer the smallest 16/32/64-KiB partition that fits the unchanged rounded
per-group SLM allocation. Zero SLM remains 0 KiB. Arithmetic, workgroup dispatch
and every synchronization command remain unchanged. This is a large-data
alternative, not a reproduction of NEO's default occupancy-based SLM policy.
The public Level Zero topology reports 8 threads/EU and 8 EUs/sub-slice here;
its sub-slice field must not be assumed equivalent to NEO's dual-sub-slice
count. No topology constant is guessed in this experiment.

The candidate passes 14 Windows CTests, three Linux CPU CTests and five real
GPU numerical groups. All **45 model requests** match output IDs, stop condition
and 57/57 drafts under the established balanced Short protocol. Yet decode
falls **44.212 to 41.039 tok/s (-7.18%)**, with **-0.19%** control drift.
Prefill/TTFT/request time increase **8.60% / 8.11% / 7.63%**. Every retained
value, including a slow main-after sample, remains in the raw report. Main
therefore retains nonzero preferred SLM **MAX**. This result does not rule out
other occupancy-based policies or establish measured physical occupancy.
[SLM experiment](../orca-l0/demo/dg2-slm-data-windows-2026-11-02.json) and
sources, checks and every request（本地归档：`../orca-l0/demo/dg2-slm-data-windows-2026-11-02.json.gz`）
preserve the negative result without changing the production pins.

Main native Runtime now implements
`write_global_timestamp(uint64_t registered_gpu_address)`. It uses an ordered,
host-visible `PIPE_CONTROL` timestamp post-sync, matching the pinned NEO compute
global-timestamp packet path. It returns **raw CS-domain ticks**; callers must
calibrate frequency and valid bits for their driver/platform and observe queue
completion before reading. The runtime adds no Level Zero or Intel UMD
dependency, and ordinary inference inserts no timestamp markers.

A read-once Windows driver-property calibration reports **19.2 MHz / 36 bits**.
Eight gated replays across two queues validate order, overwritten captures,
full destination guards and the approximate clock scale against actual host
hold intervals. The raw counter values exceed 32 bits. Actual main repeats
the eight samples and passes **21 Windows CTests**, **three Linux CPU CTests**,
the native benchmark CPU check and another exact cold model request. Linux's
actual runtime translation unit compiles; Linux GPU calibration remains pending.
The original failed negative test accidentally addressed an adjacent registered
gate allocation; its corrected unmapped-address check and both attempts remain
archived.

[Timestamp candidate and calibration](../orca-l0/demo/dg2-timestamp-windows-2026-11-02.json),
exact sources, samples and failed attempt（本地归档：`../orca-l0/demo/dg2-timestamp-windows-2026-11-02.json.gz`）,
[actual-main adoption](../orca-l0/demo/dg2-timestamp-main-windows-2026-11-02.json) and
main source bytes and validation logs（本地归档：`../orca-l0/demo/dg2-timestamp-main-windows-2026-11-02.json.gz`）
record this new complete source checkpoint. The correctness executable relinks
frozen engine objects against the rebuilt native library; this is not a full
frontend rebuild or a new throughput comparison. Strata's higher-level
timestamp adapter/profiler integration remains pending. GPU timeline intervals
can include dependency and memory stalls; these checks do not measure active
GPU time or attribute the remaining model gap. Kernel-written clocks must not
be mixed with CS timestamps until their domains are verified.

The new source-only Linux continuation is verified at
`build-orca-native-lower-20261031/dg2-current-handoff-20261102-v5/`;
its **2,017 files** pass **three exported-source Linux CPU checks and three ELF
dependency audits**. The v4 snapshot remains immutable. Real Linux GPU testing
needs its own clock calibration and Linux-driver-baked model images.
[Exported-source verification](../orca-l0/demo/dg2-timestamp-linux-handoff-cpu-2026-11-02.json)
and manifest, exporter, notes and logs（本地归档：`../orca-l0/demo/dg2-timestamp-linux-handoff-cpu-2026-11-02.json.gz`）
preserve this source boundary.

### Desktop coexistence incident and interrupted heap-window experiment

The user reported continuous cursor flickering during native DG2 execution and
confirmed that it stopped after the owned experiment processes were terminated.
This establishes a runtime association. It does not distinguish scheduling or
preemption from memory/state errors or other driver interactions. The production
configuration and executable hashes remain unchanged.

The shared-heap-window candidate passed its Windows numerical checks and Linux
CPU checks before the model comparison started. The native baseline then
completed 15 exact Short28/natural76 requests. The candidate was interrupted
during initialization, before READY, with **zero model requests**. Its original
`complete=false` result is preserved; no heap-window speedup or regression is
claimed. The historical System-log query covers error/warning events from
Display, DxgKrnl, igfx and WHEA providers, from one hour before the recorded run
to one hour after its last save. It found zero matches; that limited query does
not exclude visible display interference.

The native encoder requests thread-group preemption with register `0x2580`,
data `0x60002`, matching the pinned NEO
[`PreemptionConfig` constants](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/command_stream/preemption.inl#L141).
NEO's [Xe HPG specialization](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/command_stream/preemption_xe.inl)
does not require CSR-base programming and installs SIP for
debugging. These source comparisons give no basis for a speculative SIP or
register fix and do not prove effective hardware preemption.

The staged mitigation uses `D3DKMTSetContextSchedulingPriority` with `-7` after
each native context is created, matching NEO's
[optional low-priority context path](https://github.com/intel/compute-runtime/blob/ee495cce3db8f870a89697febbd7e8b6680151b2/shared/source/os_interface/windows/wddm/wddm.cpp#L987).
Ordinary NEO contexts do not all use this priority. At the incident checkpoint,
the isolated library passed the native encoder/ABI and ring CPU checks (**2/2**)
and linked with the frozen full-engine inputs without GPU execution. After the
user authorized resuming tests without desktop observation, the private
executable completed 15 exact Short requests. Its throughput is **38.710 tok/s**
versus **38.712** for bracketed native controls, a **-0.005%** change with
**-0.40%** control drift. The priority API call succeeds; desktop behavior and
effective scheduling remain unvalidated. This is not a default change or a
confirmed fix. The completed
[Short report](../orca-l0/demo/dg2-short-priority-cpu-windows-2026-11-02.json)
records the later test separately.
[Incident report](../orca-l0/demo/dg2-desktop-incident-windows-2026-11-02.json)
and original incomplete result, staged sources, logs and CPU runner（本地归档：`../orca-l0/demo/dg2-desktop-incident-windows-2026-11-02.json.gz`）
preserve the incident and CPU-only validation. The later link record is
`build-orca-native-lower-20261031/windows-dg2-desktop-short-prepared-v1/results.json`.

### Refreshed Linux continuation, full frontend compiled

The older numerical-only handoff is superseded by the local
`build-orca-native-lower-20261031/dg2-current-handoff-20261102-v2/` snapshot.
Its 1,989 files include the current main native runtime, full engine source,
the tested Windows engine's pinned CPU ggml source, the Short anchor and selected
evidence. Twenty-one files present in the old overlay changed, and 1,683 files
were absent there. The source tar and manifest are checked byte-for-byte. The
staged priority, partial-state and shared-heap experiments were not adopted in
that v2 snapshot. The v3 continuation above includes the subsequently adopted
combined shared-window/scratch policy.
No native Linux disk or session was modified.

The exported source, rather than the live working tree, builds under Linux/WSL
GCC and passes **3/3 CPU checks**: ring, packets and encoder/ABI. Dependency
audits of those three binaries find none of Level Zero, Intel UMD/compiler,
SYCL, UR or libdrm. The new
[`tools/check_orca_native_linux.py`](../tools/check_orca_native_linux.py)
reproduces this check in a fresh `--out` directory. Its optional `--frontend`
compiles the full Strata engine with Linux oneAPI and an explicitly selected
ggml source, without executing it. The original missing-`icpx` preflight was a
rejection before output creation; the subsequent full frontend now compiles
with Linux oneAPI **2025.3.2-832** in a private Ubuntu userland. It generates
**48 packs** and passes the same **3/3 CPU checks** and **four ELF dependency
audits**, including the linked Strata executable. No required library is
missing; SYCL, UR, Level Zero, Intel GPU UMD/compiler and libdrm are absent.

The first configure lacked Make despite Ninja being installed. The public
runner now prefers installed Ninja unless `CMAKE_GENERATOR` is specified; a
real Make-free GCC build and the same three CPU cases pass. The completed link
initially failed its encoder postcheck because the private userland lacked
`/proc/self/maps`. All postchecks then passed against the same binaries using
procfs inside private mount namespaces; no procfs mount remains outside them.
The failed configure, failed postcheck and unsuccessful first namespace attempt
remain archived. There was no GPU execution or access to the separate Linux
installation. The 48 frontend packs still require fresh Linux-driver baking;
Linux numerical correctness, PPL, model throughput and desktop coexistence are
unvalidated.
[Full frontend/CPU report](../orca-l0/demo/dg2-linux-frontend-cpu-2026-11-02.json)
and all commands, runner and failed attempts（本地归档：`../orca-l0/demo/dg2-linux-frontend-cpu-2026-11-02.json.gz`）
record the completed verification. The initial missing-Python attempt remains
incomplete in the older handoff archive.
[Exported-source CPU report](../orca-l0/demo/dg2-linux-handoff-cpu-2026-11-02.json)
and exact runner, source manifest, all logs and prior failed attempt（本地归档：`../orca-l0/demo/dg2-linux-handoff-cpu-2026-11-02.json.gz`）
preserve this boundary. The complete source overlay remains in the local
handoff directory for later Linux continuation; preserve local edits before
applying it and bake native images with the Linux driver.

### Strata CS profiler: same-zebin Short localization

The Strata DG2 timestamp adapter is implemented and checked on the actual
current host code. It validates registered ranges and eight-byte alignment,
preserves eager batch tails and recorded ordering, and writes raw CS ticks.
Thirty-six gated samples cover host/device targets, eager/recorded/relaxed
replays and batch limits 64/1. Five malformed calibrations are rejected. Clock
calibration is cached at Runtime construction through
`STRATA_ORCA_DG2_TIMESTAMP_HZ` / `STRATA_ORCA_DG2_TIMESTAMP_BITS`; there is no
Linux default. Windows uses the earlier verified 19.2-MHz/36-bit input and
retains its fractional 52.083333ns period. L0 legacy properties report 52ns,
about 0.16% scale rounding, insufficient to explain the release gap.

Both Windows host frontends were freshly compiled against frozen numerical
ABI headers. All model arms use the same 1242 zebins. The release L0/native/L0
bracket has ten warmups and five retained requests per process, balanced
min-p=.5, 5771 expert slots, requested reserve1536MiB and no prefix reuse:

| Release metric | Current-source Orca L0 controls | Native DG2 | Change |
| --- | ---: | ---: | ---: |
| Decode, engine time without setup/prefill | 48.674 tok/s | 44.507 tok/s | -8.56% |
| TTFT | 880.35ms | 926.25ms | +5.21% |
| Prefill | 836.75ms | 874.10ms | +4.46% |
| Whole request | 2396.85ms | 2580.76ms | +7.67% |

Decode control drift is +0.32%. A separate L0/native/L0 diagnostic bracket
enables CS timestamps and host phase clocks. All 90 requests across both
brackets produce the same 76 output IDs, 57/57 drafts and stop reason. This is
current-source L0 versus native DG2, not the original SYCL production comparison.

Selected diagnostic intervals, milliseconds per verifier window (average T=3.85):

| Interval | L0 controls | Native DG2 |
| --- | ---: | ---: |
| GDN HC-read1 + router | 7.905 | 9.890 |
| GDN recurrent update | 1.415 | 2.800 |
| GDN output projection | 2.470 | 3.790 |
| GDN shared expert + quantization | 2.940 | 4.040 |
| Resident expert compute, GDN + QSA | 26.920 | 27.300 |
| Explicit waitA, GDN + QSA | 0.040 | 0.350 |
| MTP draft, host elapsed | 12.470 | 12.710 |

The largest observed differences are in smaller GDN/HC/router stages, while
resident expert compute is about 1–1.5% slower and draft time is about 1.9%
higher. This prioritizes synchronization, state changes and cache invalidation
around those smaller stages. It does not establish a pure kernel-time cause:
CS intervals include GPU scheduling, memory stalls and dependencies. Markers
also disable some independent scheduling and lower throughput **4.80% on L0 /
9.78% on native**. Their unequal overhead prevents a precise decomposition of
the uninstrumented 8.56% gap. Coarser markers preserving independent scheduling
are a useful next control. No inference speed gain is claimed for adding diagnostics.

Another angle is inline-event wakeup latency. The installed MSVC `<thread>`
uses `ceil<milliseconds>` for `sleep_for(25us)`, requesting at least 1ms after
the adapter's first 128 spins. A separate CPU-only 32-sample check measures
**15.51ms median / 16.06ms p95** for that request, versus 0.3us median for yield.
That process has no active model and can have different timer resolution;
these are function durations, not measured model speedups. Earlier full-fence
polling did not change inline-tag waits. The new source/calibration evidence
keeps this hypothesis open, while the small diagnostic waitA interval cautions
against calling it the principal cause. The wait policy is unchanged.

A subsequent V11/L0 import census now closes that hypothesis for the current
Short fixture: **22 exact decode requests invoke no Sleep(0), Sleep(1) or
Sleep(2..99ms)**. The native compiled `Point::wait` calls `_Thrd_sleep_for`,
whose thunk jumps to the observed application Sleep import; active/inactive
Sleep(1) controls validate the hook. Only one-second sleep requests appear,
at the same helper in both backends. This excludes the inline 25-us sleep
fallback from these observed requests; other waits/spins and private DLL
sleeps remain outside this census. It adds no clean timing claim or wait-policy
change. Both rejected initial hook self-checks are retained.
[Raw counts and call-chain coverage](../orca-l0/demo/dg2-sleep-census-windows-2026-11-03.json)
include the verifier and archived assembly.

The release and instrumented native processes load no Level Zero, SYCL, UR,
Intel compiler/UMD or GMM modules. Production executable/config pins are intact.
[Report](../orca-l0/demo/dg2-profiler-short-windows-2026-11-02.json) and
exact sources, requests, checks, calibration and failed attempts（本地归档：`../orca-l0/demo/dg2-profiler-short-windows-2026-11-02.json.gz`）
include the full 2435-file current source map. This supersedes the earlier
timestamp-main map for current executable sources; preserve earlier checkpoints.
The v6 Linux source overlay includes this adapter, with fresh exported-source
GCC CPU checks and ELF audits; Linux GPU calibration and model validation remain
pending. [Export verification](../orca-l0/demo/dg2-profiler-linux-handoff-cpu-2026-11-02.json).

The current Windows diagnostic logs and source hashes are in the ignored
`build-orca-native-lower-20261031/` directory. These include failed attempts;
only reports explicitly marked complete establish a finished check. The Linux
handoff contains a snapshot for continuation, not an additional performance claim.

### Layer-boundary control: GPU intervals and host waiting

A separate Short experiment reduces the profiler to **48 layer boundaries plus
two head boundaries**, disables internal HC markers and retains the release
GDN/QSA independent projections. Only `verify.cpp` is recompiled for each
transport; the other 17 engine objects, runtime, numerical ABI headers and
same 1242 zebins are reused unchanged from the preceding full host builds.
These are isolated diagnostic executables; main sources, original executables,
production configuration and the v6 Linux source overlay remain unchanged.

The completed release L0/native/L0 bracket and separate diagnostic
L0/native/L0 bracket each use ten warmups and five retained requests per
process. Balanced min-p=.5, 5771 expert slots, reserve1536MiB and no prefix
reuse are retained. All **90 requests** match the 76 output IDs, natural stop
and 57/57 accepted/offered drafts. Decode uses engine time excluding loading,
initialization, preparation and prefill. Geometric means of the before/after
control medians give:

| Uninstrumented metric | Current-source Orca L0 | Direct DG2 | Change |
| --- | ---: | ---: | ---: |
| Decode | 48.671 tok/s | 44.888 tok/s | -7.77% |
| TTFT | 878.54ms | 919.52ms | +4.66% |
| Prefill | 834.45ms | 868.00ms | +4.02% |
| Whole request | 2393.95ms | 2561.05ms | +6.98% |

Decode control drift is +0.69%. This is a repeat of the native transport
comparison, not a new original-SYCL comparison or an established speedup over
the preceding 44.507-tok/s native result.

The coarse profiler reduces throughput **0.79% on L0 and 1.38% on native**,
versus the preceding fine profiler's 4.80%/9.78%. The correctly scoped retained
tables each cover **20 decode windows, average T=3.85**:

| Diagnostic interval, ms/window | L0 controls | Direct DG2 | Difference |
| --- | ---: | ---: | ---: |
| GDN layer spans, 36 layers | 47.020 | 53.010 | +5.990 |
| QSA layer spans, 12 layers | 15.835 | 17.280 | +1.445 |
| Head | 1.595 | 1.620 | +0.025 |
| GPU interval total | 64.465 | 71.960 | +7.495 |
| Host verifier elapsed | 65.410 | 72.700 | +7.290 |
| Host MTP draft | 12.465 | 12.680 | +0.215 |

About **80% of the instrumented GPU difference** is inside GDN layer spans.
GDN spans are 12.74% slower, versus QSA's 9.13%; their larger aggregate also
reflects having three times as many layers. A layer span includes routing,
experts, synchronization and trailing writes, so this does not identify one
kernel or imply that 80% of the release regression has been precisely assigned.
The earlier fine profile still makes small GDN/HC/router stages useful next
targets; the coarse control confirms the broader difference with much less
measurement interference.

Host verifier time outside the sampled GPU intervals is about **0.945ms on L0
and 0.740ms on native** per window. This envelope also contains input/embedding
work before the first marker, submission, readback and host bookkeeping; it
is not a direct fence-latency measurement. Within this diagnostic bracket it
does not support queue-completion wakeup as the dominant loss. GPU intervals
include scheduling, cache/memory stalls and waits for dependencies, including
possible host dependencies; they are not pure active kernel time.

The source review checks several other angles against cached NEO sources:

- Whole-queue `Stream::wait` and observer waits use a published submission
  `Point` without a memory tag, selecting the native fence path. Mid-list
  memory-tag events have a separate sleep path. The idle-process 15.51ms
  `sleep_for(25us)` result must not be assigned to every model queue wait;
  actual inline sleep counts remain unmeasured.
- DG2 `ForceNonCoherent`, the default one walker, and ordinary HDC/untyped
  barrier flushes agree with pinned NEO. Per-allocation PAT/cache/residency,
  enabled CCS topology and state-change invalidations remain open. Keep the
  required invalidation until the topology-dependent workaround is established.
- Timestamp marker implementations differ: native timestamps inherit
  host-visible signal flushing, while pinned NEO blocks default post-sync
  property setting. This helps explain why marker cost needs an empirical
  control, but does not establish installed packet equivalence or a release
  cause. Required production synchronization is unchanged.

An initial LF/CRLF preparation failure and two measurement attempts are
preserved. One harness incorrectly assumed 19 windows/T=4; the previous raw
fine tables actually used 20/T=3.85. Another omitted the legacy profile flag
and included preparation graphs (25 windows/T=4.16), so those phase totals are
discarded. The completed diagnostic bracket uses
`STRATA_VERIFY_PROFILE_LAYERS=1`, `STRATA_VERIFY_PROFILE=1` and
`STRATA_DECODE_TIMING=1`: the legacy flag clears preparation counters before
decode. Sixteen additional exact requests from the discarded attempts are
archived separately and excluded from the completed brackets.

[Layer control report](../orca-l0/demo/dg2-layer-profiler-short-windows-2026-11-02.json)
and sources, commands, object pins, all requests and failed attempts（本地归档：`../orca-l0/demo/dg2-layer-profiler-short-windows-2026-11-02.json.gz`）
include the cached source evidence. This experiment does not replace the
preceding main-source checkpoint or claim a Linux GPU result.

### Three-phase control: mixer and HC/router/shared stages

A further isolated Short profiler keeps independent projections and records
three boundaries per layer plus two head boundaries. It splits each layer into
`mixer` (HC0 through output projection), `hc1+route+shared` (the next HC,
routing, plan/publish, shared expert and quantization), and `experts+tail`
(plan waits, routed experts, combine and trailing writes). Only the verifier
CPP is recompiled; numerical host objects and all 1242 zebins remain unchanged.

The new uninstrumented L0/native/L0 bracket measures **48.759 versus
44.795 tok/s (-8.13%)**, with **-0.17%** control drift. TTFT is 877.98 versus
920.97ms (+4.90%); prefill is 834.60 versus 869.70ms (+4.21%). A separate
diagnostic bracket adds 1.06% L0 / 2.37% native throughput overhead. Each arm
uses ten warmups and five retained requests; all **90 requests** match the
same 76 IDs, natural stop and 57/57 drafts. Balanced min-p=.5, 5771 slots,
reserve1536MiB and no prefix reuse are retained. Decode excludes loading,
initialization, preparation and prefill.

| Diagnostic interval, ms/window | L0 controls | Direct DG2 | Difference |
| --- | ---: | ---: | ---: |
| GDN mixer | 15.550 | 19.090 | +3.540 |
| GDN HC1/router/shared | 10.750 | 13.290 | +2.540 |
| GDN experts/tail | 20.790 | 21.430 | +0.640 |
| QSA mixer | 5.665 | 6.450 | +0.785 |
| QSA HC1/router/shared | 3.775 | 4.570 | +0.795 |
| QSA experts/tail | 6.400 | 6.600 | +0.200 |
| Head | 1.600 | 1.630 | +0.030 |
| GPU interval total | 64.525 | 73.030 | +8.505 |
| Host verifier elapsed | 65.465 | 73.710 | +8.245 |

The mixer and HC/router/shared cells account for about **90% of this
instrumented GPU difference**; routed experts/tail account for about 10%.
GDN's first two cells alone account for about 71.5%. This prioritizes small
operator state transitions, cache handling and dependency gaps over expert
matrix throughput. It does not assign exact release costs: the two marker
implementations have unequal overhead, GPU intervals include dependency
waits/cache stalls, and cells contain multiple kernels. Correctly scoped
tables cover 20 decode windows at average T=3.85. Host time outside sampled
GPU intervals remains small (0.940ms L0 / 0.680ms native per window).

Main sources, production pins and the Linux v6 bundle are unchanged.
[Three-phase report](../orca-l0/demo/dg2-coarse-profiler-short-windows-2026-11-02.json)
and exact sources, object pins, commands and all requests（本地归档：`../orca-l0/demo/dg2-coarse-profiler-short-windows-2026-11-02.json.gz`）
preserve this additional diagnostic experiment.

### GRF transition controls: no established gain

Two isolated runtime variants test the remaining pure GRF state transitions.
The full-flush control emits SCM without unchanged preemption, pipeline,
double-SBA and CFE packets, retaining the full-invalidating leading barrier.
The narrow control additionally omits state/instruction/texture/constant
invalidations on that pure transition. Both keep CS stall, HDC/untyped flush,
host visibility, ordinary dispatch dependencies and the full initial/coupled
state path. Opt-in is limited to the calibrated Windows A770 revision 8;
Linux retains the existing conservative policy.

Pinned NEO source separates SCM synchronization from SBA programming. DG2's
product helper sets enabled CCS count to one, while the extended invalidation
workaround checks for more than one CCS. This supports an isolated experiment;
it does not establish the installed driver's topology or exact packet stream.

The release sequence is main/full/narrow/full/main, with ten warmups and five
retained requests per arm. Both variants relink the same eight frozen host
objects/libraries; the numerical kernels and 1242 zebins are identical. All
**75 requests** match 76 output IDs, 57/57 drafts and natural stop, with the
same balanced parameters and decode-only engine timer as above.

| Release policy | Decode | Change versus main |
| --- | ---: | ---: |
| Main, before/after controls | 43.895 tok/s | baseline |
| Pure GRF SCM, full invalidation | 43.993 tok/s | +0.22% |
| Pure GRF SCM, narrow invalidation | 44.140 tok/s | +0.56% |

The isolated narrow/full difference is only **+0.33%**, versus **+0.98%**
full-control drift. Outer main-control drift is -0.01%. Narrow TTFT is 930.04ms
versus main's 935.03ms (-0.53%); whole request is 2601.46 versus 2612.96ms
(-0.44%). These small single-bracket differences do not establish a speedup;
**neither variant is adopted**. Absolute rates differ from the preceding
bracket under unlocked clocks; this experiment has no L0 arm and cannot
replace its L0 comparison.

Each variant passes **14 Windows checks (two CPU and twelve GPU), three Linux GCC CPU checks
(including actual runtime compilation), and five real GPU numerical groups**:
device memory, scratch, private memory, atomics and multiqueue local ISA.
Frozen inputs are verified before/after the model bracket. Main sources,
original executables, production configuration and Linux v6 remain unchanged.
[GRF control report](../orca-l0/demo/dg2-grf-cache-short-windows-2026-11-02.json)
and source bytes, checks, commands, cached references and all requests（本地归档：`../orca-l0/demo/dg2-grf-cache-short-windows-2026-11-02.json.gz`）
preserve the experiment.

The cross-check of the earlier cold parameter-window trace locates **120 pure
GRF transitions** in T=4, with **65 large-GRF dispatches: 36 GDN norm and 29
Q6K projection dispatches**. Twenty-four Q6K entries change GRF mode. Those
counts do not time a transition or justify globally changing the kernels'
register allocation; Q6K already has a measured shape exception. Register
selection is identical on both transports in the same-zebin comparisons.

A separate source review identifies a more specific memory-placement question.
Native `capture(CaptureHeap, AllocationPool, bytes)` allocates host-backed
parameter slabs; `append` writes indirect payloads into their CPU mappings.
Pinned NEO `HeapHelper` maps the indirect-object heap to `internalHeap`, and
`CommandContainer` records whether the result is local. NEO's forced-system
list includes command/ring buffers but excludes `internalHeap`; its Windows
device-pool path remains conditional on local support and allocation flags.
Thus command buffers being host-backed is not itself a demonstrated mismatch,
while **indirect parameter placement** is a focused next experiment. Actual
installed L0 heap placement, PAT policy and any performance gain remain
unverified. Native allocation-time local-residency checks do not prove those
properties. The cited archive includes the cached placement call chain and
exact source hashes, avoiding another network query.

### Batched local indirect parameters: isolated implementation and Short control

An isolated runtime candidate now mirrors immutable host parameter slabs in
local VRAM. Recording still owns the original host capture and weak cache.
`close()` merges new ranges by slab and uploads them on the independent storage
queue before submitting the consumer. Replays perform no upload. Local mirrors
are tracked alongside their source allocations; ordinary command-buffer
placement, numerical kernels, dispatch barriers, SLM MAX and scratch slots4096
are unchanged. The opt-in gate is Windows A770 revision8; Linux remains
host-backed. No public API or frozen host numerical object is changed.

The first candidate fails existing memory-plateau assertions in reclamation
and release tests. Internal upload lists were sharing normal parameter and
command pools, so recycling expanded the set of mirrored slabs. Giving those
bootstrap lists their own pools fixes the interaction without relaxing the
original assertions. The failed source/build/logs remain in the archive.
Both host/local controls contain the corrected code; their 43-file source
maps differ only in the Windows constructor's opt-in line.

Each control passes 14 Windows checks (two CPU, twelve GPU), three Linux GCC
CPU checks including actual runtime compilation, and eight additional GPU
numerical/lifecycle groups. These include **17,000 captures across the 1MiB
boundary, 900 reclamation submissions and 64 in-flight recording submissions**,
with exact outputs, full guards and stable retained-memory checks. The local
reclaim-ISA check retains 17,235,968 bytes after wave1 and the later waves keep
that value. Linux GPU behavior is not tested.

The same-1242-zebin release sequence is main/host/local/host/main, ten warmups
and five retained requests per arm. All **75 requests** match the same 76 IDs,
natural stop and 57/57 drafts, with min-p=.5, 5771 actual slots, reserve1536MiB
and no prefix reuse. Engine decode time excludes loading, preparation and
prefill; capture uploads are outside that decode interval.

| Release policy | Decode | TTFT | Whole request |
| --- | ---: | ---: | ---: |
| Main before/after controls | 43.971 tok/s | 933.19ms | 2607.75ms |
| New code, host parameters before/after | 44.226 tok/s | 932.31ms | 2600.66ms |
| New code, local parameters | 44.429 tok/s | 927.44ms | 2582.77ms |

The placement-only comparison is **+0.46% decode**, **-0.52% TTFT** and
**-0.69% whole-request time**, against +0.23% host-control decode drift. The
combined local policy is +1.04% versus main, whose drift is +0.32%; the host
control itself is +0.58% versus main. These are one bracket with unlocked
clocks and include allocation-layout effects. They do not establish a
repeatable improvement or explain the earlier approximately8% L0/native gap.
**The candidate is not adopted.** Preserve all retained values; do not add
these percentages to earlier state-reuse gains or treat this as an L0 arm.

Main sources, original executables, production configuration and Linux v6
remain unchanged. [Placement report](../orca-l0/demo/dg2-local-parameters-short-windows-2026-11-02.json)
and source bytes, exact commands, numerical checks, every request and failed attempts（本地归档：`../orca-l0/demo/dg2-local-parameters-short-windows-2026-11-02.json.gz`）
preserve the implementation for further work without changing defaults.

### Read-only WDDM frequency capability probe

A separate Gdi32/SDK probe creates no GPU device/context, submits no commands
and changes no power/display setting. It runs only after the Short lock is
released. Eight idle samples on compute node5 and render node0 return success
for `KMTQAITYPE_NODEPERFDATA`, but all current/max engine-frequency fields are
zero. This does not supply usable engine frequency for the comparison, prove
equal loaded clocks, or establish throttling. The raw adapter fields are
retained without interpreting them as calibrated clocks or power.

The initial selector found virtual/indirect aliases with physical device IDs;
using the native runtime's software/indirect-adapter filter fixes selection.
Failed and completed attempts, typed SDK ABI size checks and source/header
hashes are preserved. Do not repeat the same idle query; loaded sampling or
another calibrated interface would be a distinct investigation. Engine
frequency must not be confused with the calibrated19.2MHz CS timestamp clock.
[Capability report](../orca-l0/demo/dg2-perfdata-capability-windows-2026-11-02.json)
and probe sources, commands and raw query results（本地归档：`../orca-l0/demo/dg2-perfdata-capability-windows-2026-11-02.json.gz`）.

### Multiple-angle cross-check: simple chains do not reproduce the model gap

Two fresh L0/native/L0 brackets replay the same `advance` zebin on A770,
driver32.0.101.9034, using the existing runtime libraries. Both record one
explicit dependency barrier per kernel on relaxed lists. The 12 cases per
arm cover host/local data, 64/1048576 FP32 elements, and 1/64/400 launches.
Native ISA resides in local memory. Initialization, capture, setup/download
copies and validation stay outside the timer. Both backends use raw GPU
timestamps converted with the independently calibrated19.2MHz/36-bit CS
clock; this avoids L0's rounded integer nanosecond period.

| 64-float local-data chain, 400 launches | First bracket | Independent repeat |
| --- | ---: | ---: |
| L0 before/after GPU control | 363.789us | 364.323us |
| Native GPU median | 249.844us | 275.964us |
| GPU elapsed change | -31.32% | -24.25% |
| L0 GPU-control drift | +0.007% | -0.114% |
| L0 host replay control | 471.318us | 463.200us |
| Native host replay median | 416.800us | 450.700us |
| Host replay change | -11.57% | -2.70% |
| L0/native host submit medians | 10.79/22.75us | 9.00/19.35us |

This simple chain has **faster native GPU elapsed but slower native CPU
submission**. Neither quantity alone predicts full request latency. The
64-launch4MiB local-data cases also show stable GPU changes of -30.28% and
-30.24%. One first-bracket400-launch4MiB local control drifts -27.30%; retain
it as unstable evidence, not a precise percentage. The single-launch4MiB
host-data difference changes from +9.84% to +0.016%, so it does not establish
a persistent native host-memory penalty or PAT equivalence. These data also
do not provide loaded core clocks. The custom kernel has no SLM or scratch;
its result cannot validate the settings used by complex model kernels.

Each arm uses ten warmups and thirty retained replays per case. Across both
brackets, **2880 replays and 72 aggregate output/full-guard checks pass**.
Every payload value is checked after forty cumulative updates; the complete
prefix/tail and aligned allocation padding retain their sentinel. The second
bracket reuses the exact successful binaries. The first compiler invocation
failed on library placement in its command line, before GPU work; its logs
and runner remain alongside the corrected build. No runtime code changes.

Two offline checks narrow other hypotheses:

- Pinned Short configuration has `STRATA_VERIFY_NO_HOST=1`,
  `STRATA_VERIFY_DEVICE_PLAN=1` and routing collection enabled. The verifier
  bypasses the per-layer CPU planning loop. This excludes that host round trip
  as the cause for this benchmark, while routing copies and other mapped-host
  accesses remain.
- The existing cold parameter-window trace has one segment and zero API
  signals, inline events, queue waits, graph uploads and staged readbacks for
  allT1..T6 graphs. Its byte counts reconcile with the known encoder: T4 has
  1773 dispatches,131 full state blocks and1559 ordinary `PIPE_CONTROL`s,
  including the two tail barriers, for1690 total. Counts are reconstructed
  from source/recorded byte budgets; raw packets were not captured. They are
  not timings or evidence that semantic barriers can be dropped. Inline tag
  sleep policy cannot account for graph-local cost when no such events exist.

The earlier same-zebin whole-model release comparison remains **48.759 L0 /
44.795 native tok/s (-8.13%)**. No model request is run in this experiment.
The opposite direction in simple chains shifts the next useful isolation to
the **actual mixer/HC kernels and their shapes, local-memory/scratch behavior,
allocation/cache attributes and workload-specific scheduling**. It does not
prove that every dispatch packet is optimal or that one mechanism explains
the model gap. Do not add synthetic gains to model throughput claims.

Main/production pins and the immutable Linuxv6 handoff remain unchanged.
The [multi-angle report](../orca-l0/demo/dg2-multi-angle-diagnostic-windows-2026-11-02.json)
and probe sources, commands, every sample, failed build, cached trace and source fingerprints（本地归档：`../orca-l0/demo/dg2-multi-angle-diagnostic-windows-2026-11-02.json.gz`）
preserve this investigation. No Linux GPU validation is claimed.

### Nine-phase Short localization and a WDDM allocation-size control

A new A770/32.0.101.9034 experiment keeps the same1242 zebins, balanced
min-p0.5,5771 expert slots,7.74GiB cache,1536MiB reserve and no prefix reuse.
Nine ordered spans per layer preserve independent projections; markers stay
outside the internal HC norm/down/up sequence. Release and instrumented
L0/native/L0 brackets each use ten warmups and five retained requests per arm.
All90 requests produce the same76 tokens,57/57 drafts and natural stop.
Decode uses engine `decode_ms`, excluding startup, preparation and prefill.

The native arm uses an **isolated1GiB allocation-part candidate**, rather
than the unchanged native runtime. It keeps the original total allocation
bytes, contiguous GPU virtual address and local-residency rejection. The
unmodified release executables and numerical objects remain frozen.

| Uninstrumented Short | L0 control | Native1GiB candidate | Change |
| --- | ---: | ---: | ---: |
| Decode | 47.094tok/s | 43.192tok/s | -8.287% |
| Prompt processing | 870.30ms | 909.30ms | +4.482% |
| TTFT | 915.08ms | 962.42ms | +5.173% |
| Whole request | 2485.57ms | 2667.89ms | +7.335% |

L0 decode before/after medians are47.311/46.879tok/s, a-0.913% drift.
Controls use their geometric mean; clocks remain unlocked. This confirms
the direction of the previous approximately8% gap. It is not a measured
speedup over the older native executable: that would need its own fresh
bracket. The earlier unchanged-native release bracket and its incomplete
profiling attempt remain in the archive.

| Grouped instrumented GPU spans, ms/window | L0 control | Native candidate | Delta |
| --- | ---: | ---: | ---: |
| HC0 and HC1 | 16.915 | 20.240 | +3.325 |
| Input/output projections | 9.385 | 12.380 | +2.995 |
| Route/plan and shared/quantization | 7.235 | 9.420 | +2.185 |
| GDN convolution/recurrence/normalization | 1.700 | 3.300 | +1.600 |
| Experts and tail | 28.270 | 28.980 | +0.710 |
| QSA KV/index and attention/gate | 1.690 | 1.950 | +0.260 |
| Head | 1.630 | 1.700 | +0.070 |

GPU total is66.859/78.010ms/window; host verifier is67.974/78.730ms/window.
The uncovered host envelope is smaller on native. It is not a direct fence
wakeup measurement. HC and projections are useful next isolation targets;
experts/tail contribute only about6.4% of this instrumented GPU delta.
These spans include dependencies, stalls and scheduling, not pure kernel
active time. Grouped medians need not sum exactly to the median total.

**The profiler perturbs the backends unequally:** instrumented decode is
46.489tok/s for L0(-1.285% against its release control) and41.050tok/s for
native(-4.958%). Native `timestamp_value()` inherits the host-visible
HDC/untyped/DC-flush flags of `signal_value()`; the queue also appends a
dependency barrier before each timestamp. L0 uses the same wrapper barrier
followed by `zeCommandListAppendWriteGlobalTimestamp`; its actual driver
timestamp packets were not captured. This provides a concrete instrumentation
path to investigate, not a measured causal cost of those flags. Markers are
absent from release runs, so they cannot explain the release gap. Do not map
the11.151ms instrumented delta directly onto release latency or call it
removable CPU/API overhead. Calibrate or reduce marker perturbation before
using fine spans for exact attribution.

The allocation experiment also resolves a separate reproducible startup
failure. The7.74GiB cache originally used two parts near4GiB. Failure-only
logging records `STATUS_PENDING`, returned count2, zero requested trim and
a paging fence already at its target, but residency statuses`3,1`:
**not resident/GPU memory**. This occurs before READY and any request.
Process usage is12.043GiB against a15.126GiB budget; that budget does not
guarantee physical placement. A later read-only idle snapshot sees about
1.55GiB resident in the local segment and does not establish persistent
global exhaustion at the earlier failure.

Mapping GPU VA before residency reproduces the same model failure, despite
passing the independent5GiB boundary/guard/release check. Reducing only the
part limit to1GiB passes that check and an exact Short smoke, then starts
both native release/profile arms above with the full5771 slots. Part size
affects startup behavior here; fragmentation, private descriptor limits and
driver placement policy are not yet distinguished. The candidate is not
adopted and this is not a Linux GPU result. SDK/NEO residency references are
cached with exact source hashes instead of queried repeatedly.

All six arms finish, but the final workspace-source guard detects a concurrent
edit to `sycl/src/core/mtp.cpp`. Its content is preserved. A separate verifier
checks every executed binary, reused numerical object, zebin, production pin,
all90 outputs and raw phase tables; the original MTP input matches immutable
Linuxv6, and the other2434 current source pins match. The original failed
guard remains recorded. The first candidate launcher also ended during a
partial profiling control without a recorded terminal reason; that partial
arm is excluded, its complete release arms are copied byte-for-byte and the
three-arm profiling bracket is restarted. Neither interruption is relabeled
as a GPU performance result.

This task changes no main runtime source, production configuration/binary or
immutable Linuxv6 file. The [nine-phase/candidate report](../orca-l0/demo/dg2-one-gib-subphase-short-windows-2026-11-02.json)
and runnable sources, frozen provenance, all requests, raw tables, failed attempts and SDK/NEO excerpts（本地归档：`../orca-l0/demo/dg2-one-gib-subphase-short-windows-2026-11-02.json.gz`）
preserve the evidence. Priorities are a calibrated/sparser profiler, then
same-zebin HC/projection checks with real shapes and allocation/cache state.

### Compile-time removal of verifier/MTP profiling and accounting clocks

An A770 Windows Short control compares the isolated 1GiB native runtime with
an executable that compiles out verifier GPU/event profiling, trace/debug paths
and verifier/MTP accounting clocks. Profiling environment gates are already off
in the baseline. Only two host objects are rebuilt; both executables reuse the
same runtime and 1,242 numerical zebins. Synchronization, request/decode clocks,
algorithmic timing and operational deadlines remain.

| Short decode, retained median | tok/s |
| --- | ---: |
| Baseline before | 43.513 |
| Accounting/profiling compiled out | 43.668 |
| Baseline after | 43.157 |

The two controls' geometric mean is **43.335 tok/s**, giving **+0.769%** for
the candidate, while baseline drift is **-0.818%**. Retained sample ranges
overlap: 43.456–43.943, 43.344–44.004 and 42.979–43.920 tok/s respectively.
This single bracket does not establish a reproducible speedup or support
residual accounting timers as the main cause of the earlier native release
gap. It contains no fresh L0 arm; do not combine its percentage with earlier
L0/native comparisons. The candidate is not adopted.

Each arm has ten warmups and five retained requests. All **45 requests** match
the fixed Short28/natural76 output, **57/57 drafts**, zero prefix reuse and
normal stop. Min-p stays **0.5**, with **5,771 expert slots**, **7,922 MiB**
actual expert cache and **1,536 MiB** reserve. Decode uses engine `decode_ms`,
excluding startup, preparation and prefill. Prompt processing, TTFT and full
request time change by -0.945%, -0.985% and -0.968%; those small differences
also come from this one bracket with unlocked clocks. Every warmup is retained
in the raw evidence, including the initial ramp from about 19 to 43 tok/s.

Passive process CPU accounting during the five retained full requests gives
**0.0259 / 0.0260 / 0.0175 average CPU cores** for before/candidate/after.
This does not indicate CPU saturation, but low CPU use cannot exclude blocked
lock waits. The cached source audit shows this MSVC `steady_clock` wraps QPC,
and debug environment gates are already process-cached. Runtime/point/event
mutexes remain; neither their presence nor this experiment measures contention.

Production executable/configuration pins and reused numerical objects pass
their final guards. The [report](../orca-l0/demo/dg2-no-profile-short-windows-2026-11-02.json)
and runnable build/measurement sources, commands, all requests, lock/clock source audit and failed attempts（本地归档：`../orca-l0/demo/dg2-no-profile-short-windows-2026-11-02.json.gz`）
preserve the exact parameters, hashes and raw results. No Linux GPU result is
claimed, and the immutable Linuxv6 handoff is unchanged.

### Main-source adoption of the WDDM allocation-part limit

Following the isolated candidate above, main now limits each native WDDM
device-allocation part to **1 GiB**. This addresses the observed startup
residency failure of the 7.74 GiB expert cache split into near-4 GiB parts.
It keeps total allocation capacity, contiguous GPU virtual addressing and
strict local-residency rejection. Linux allocation policy is unchanged.

Actual main passes **22 Windows CTests** (19 GPU and three CPU), the native
benchmark's CPU check and **three Linux GCC CPU CTests**, including compilation
of the Linux runtime. The frozen model host/numerical objects linked against
the new main runtime complete one exact Short request: **76 tokens, 57/57
drafts, 5,771 expert slots, min-p 0.5**, no prefix reuse and natural stop.

The large-memory check covers **5 GiB + 128 KiB** with seven distinct regions
at the ends and every 1 GiB boundary. All regions are written before any are
read, so aliasing between parts cannot pass by overwriting identical data.
Eight replays validate full data/guards, release accounting and stale identity
rejection. CPU checks accept exactly 1 GiB and reject 1 GiB + 64 KiB parts.

This adoption makes **no throughput claim**. Earlier isolated-candidate status
describes that historical checkpoint; the default changes only here.
Production L0 executable/configuration pins and immutable Linux v6 remain
unchanged. No Linux GPU validation is claimed. The
[main-adoption report](../orca-l0/demo/dg2-one-gib-main-windows-2026-11-02.json)
and runner, commands, all checks and model evidence（本地归档：`../orca-l0/demo/dg2-one-gib-main-windows-2026-11-02.json.gz`）
preserve the exact source and binary hashes.

### Model-shape HC isolation, with and without GPU timestamp commands

An independent Windows A770 control calls the frozen model's actual
`fused_gr_read_multi` implementation and ABI with the same 1,242 zebin inputs
on L0 and native DG2. Geometry is **n_embd=2560, HC=4, HC_LR=320**, with the
model's fused Q8 mixed-output path. One BF16 weight bank is **12.578 MiB**;
eight banks occupy **100.625 MiB**. A single bank may fit cache, while eight
exceed it; cache occupancy is not measured. Inputs/weights are deterministic
synthetic data, and the intervening model stages are absent.

Each case records **48 HC reads**, executes ten warmups and retains twenty
whole-replay timings. T=1/T=4, one/eight banks and pending residual apply
OFF/ON give eight cases. The first L0/native/L0 bracket writes only two GPU
CS timestamps per replay, using the previously calibrated 19.2 MHz/36-bit
domain. A second L0/native/L0 bracket **removes both timestamp commands and
all inner-operator clocks**, retaining three host clock reads around the
entire replay and the required completion wait. It keeps the unused stamp
allocation to preserve allocation size/order. Functional timeout clocks and
synchronization remain.

| No GPU markers: whole 48-HC replay | L0 control, ms | Native, ms | Latency increase | L0 control drift |
| --- | ---: | ---: | ---: | ---: |
| T1, one bank, residual OFF | 2.898 | 3.371 | +16.33% | -5.61% |
| T1, one bank, residual ON | 3.167 | 4.004 | +26.43% | +0.68% |
| T1, eight banks, residual OFF | 3.510 | 4.226 | +20.39% | -0.29% |
| T1, eight banks, residual ON | 3.959 | 4.803 | +21.33% | +1.03% |
| T4, one bank, residual OFF | 5.423 | 5.739 | +5.83% | +5.33% |
| T4, one bank, residual ON | 6.091 | 6.256 | +2.71% | +4.53% |
| T4, eight banks, residual OFF | 5.850 | 6.293 | +7.57% | +2.45% |
| T4, eight banks, residual ON | 6.524 | 6.993 | +7.18% | +4.12% |

Controls use the geometric mean of before/after medians; clocks are unlocked.
T1's direction persists across all four cases, including low-drift controls.
The smaller T4 differences need repetition where they approach drift.
Native host submission takes 17.75-32.50 us per replay, versus the 165-844 us
whole-replay latency differences; its small, noisy submission increment does
not explain those full differences. Whole-replay host time still includes
completion notification and collection. The separate stamped bracket's GPU
intervals are 3.18%-24.32% longer on native, but retain marker perturbation.
Do not subtract the two brackets to claim a precise timestamp cost: they run
in separate GPU leases, with clocks/placement not pinned.

All **1,440 replays** finish. After each case's thirty replays, the complete
outputs, residual and Q8 bytes match across all six arms: **48 final output
files** with finite/nonzero HC outputs and full end guards. Twenty timings
are logged per case; warmup timings and per-replay readbacks are not recorded.
One canonical complete binary per case is archived, with all 48 original
file hashes checked before publication. Runtime dependency audits and all
frozen source/library/zebin/production guards pass.

The HC discrepancy therefore survives removal of GPU timestamp commands.
This shifts useful investigation toward workload-specific GPU execution,
dependency/state programming and memory behavior. It does not identify one
cause, exclude all lock contention or convert a synthetic HC percentage into
model throughput. No production policy or numerical kernel changes, and no
Linux GPU claim, result from this experiment. The
[combined HC report](../orca-l0/demo/dg2-hc-shape-windows-2026-11-02.json)
and runners, exact commands, raw samples, frozen HC sources and canonical outputs（本地归档：`../orca-l0/demo/dg2-hc-shape-windows-2026-11-02.json.gz`）
preserve the full controls.

### Actual HC launch identity and private-memory follow-up

An isolated runtime trace verifies **1,536 HC launches** across the same eight
cases. Native DG2 and L0 use identical kernel names, local sizes and group
counts, and all **16 complete output files** match the previous no-marker
control. Tracing changes recording cost; none of its timings is a performance
result. The initial configure attempt lacked the SDK path and ran no GPU work;
the corrected attempt and both logs are archived.

| Actual HC kernel | SIMD | Private bytes per hardware thread | Scratch bytes per hardware thread | Total SLM bytes |
| --- | ---: | ---: | ---: | ---: |
| Norm split | 32 | 40,192 | 0 | 64 |
| Down sliced | 16 | 0 | 0 | 4,096 |
| Down reduce | 16 | 20,096 | 0 | 0 |
| Up with Q8 output | 32 | 40,192 | 4,096 | 1,792 / 7,168 for T1 / T4 |

Of those launches, **1,520** change none of the normalized encoder state;
eight initialize list state and eight change the scratch-heap state. This
checks dispatch identity and state reuse; it does not establish equivalent
memory placement, cache behavior or the absence of lock contention. Cached
NEO source agrees with native's SIMD scaling of private memory and does not
support a DG2 EU-fusion-default or missing-L3-programming diagnosis. Reference
NEO code is not a capture of the installed driver's allocation policy.
[Launch/resource report](../orca-l0/demo/dg2-hc-launch-trace-windows-2026-11-02.json)
and runnable trace, cached source audits and raw evidence（本地归档：`../orca-l0/demo/dg2-hc-launch-trace-windows-2026-11-02.json.gz`）.

A separate candidate changes only eleven HC device-helper parameters from
`GrMulti` by value to `const GrMulti&`. Both arms use identical frozen headers,
compiler and numerical flags; the rebuilt baseline reproduces **all 25
original zebins byte-for-byte**. Capture ABI matches both arms and the frozen
host after normalizing only debug source basenames. The candidate changes 21
ISA bodies and 13 resource records, but **reduces neither private memory nor
spill metadata**. For example, Q8 up private memory rises from 40,192 to 40,448
bytes per hardware thread while its reported spill size stays 2,208 bytes
(rounded runtime scratch remains 4,096). Norm private memory does not fall.
This candidate is not adopted; no candidate GPU output or performance result
is claimed. Failed offline parsing/over-strict header comparisons are retained
with the corrected analysis. [Resource comparison](../orca-l0/demo/dg2-hc-const-reference-windows-2026-11-02.json)
and sources, frozen headers, both image packs and recovery evidence（本地归档：`../orca-l0/demo/dg2-hc-const-reference-windows-2026-11-02.json.gz`）.

### Draft-threshold parity of the frozen L0/native measurements

A follow-up audit checks the actual frozen executable/source pins, common
environment, launch arguments and raw request evidence used by the nine-phase
Short comparison. The two backends have the same effective draft policy:

| Setting | Orca L0 | Native DG2 |
| --- | ---: | ---: |
| Explicit `--spec-min-p` | 0.5 | 0.5 |
| Cumulative confidence floor | 0, disabled | 0, disabled |
| Last-draft threshold | 0.5, inherited | 0.5, inherited |
| Maximum MTP draft depth | 4 | 4 |

The bare CLI's `spec_min_p` default is zero in both frozen sources; the common
launch arguments explicitly override it to 0.5. The cumulative/last-threshold
source blocks match exactly. All **90 requests** produce 76 tokens and 57/57
accepted/offered drafts. All **45 instrumented requests** also have 20 windows
and average T=3.85. Threshold mismatch does not explain this measured gap.

Configuration still contains the old key
`STRATA_MTP_LAST_DRAFT_MIN_P=0.6`. Neither compared executable contains that key;
the implemented name is `STRATA_MTP_LAST_MIN_P`, which was unset. The stale
entry therefore does not set a different last threshold. This audit changes
no production configuration or policy, runs no GPU work and makes no new
performance claim. Separately running cumulative-threshold candidates and
historical 0.8 measurements are outside this frozen comparison.
[Effective-policy audit](../orca-l0/demo/dg2-draft-threshold-audit-windows-2026-11-02.json)
and frozen policy sources, runners, requests and provenance（本地归档：`../orca-l0/demo/dg2-draft-threshold-audit-windows-2026-11-02.json.gz`）.

Another MTP experiment subsequently updates the live configuration's
cumulative floor to 0.5. This does not change the frozen evidence above.
The norm-capture controls below use the archived original configuration,
explicit cumulative floor zero and explicit last threshold 0.5 for their
full-model Short comparisons.

The current `prepare.engine_source` also inserts cumulative default **0.5
for L0 and zero for native DG2 when that environment variable is unset**.
This is a later source-generation change, not a property of the two frozen
executables above. The current balanced configuration explicitly supplies
0.5, so both backends reading that configuration receive the same value.
Bare CLI step `spec_min_p` still defaults to zero in the common source.
[Current source/configuration audit](../orca-l0/demo/dg2-current-draft-defaults-windows-2026-11-02.json)
and generator, actual policy source and runnable audit（本地归档：`../orca-l0/demo/dg2-current-draft-defaults-windows-2026-11-02.json.gz`）.

### HC private-buffer allocation control: keep the queue-wide pool

A Windows A770 experiment isolates the current queue-wide reusable private
pool versus one pool per kernel per queue. It rebuilds both native runtimes
from the same source snapshot; HC host objects and all 1,242 zebins stay
identical. Neither arm emits GPU timestamps or inner-operator clocks. Two
baseline/candidate/baseline brackets each run eight HC cases, with ten warmups
and twenty retained whole-replay samples per case.

| Case | Replay latency change, bracket 1 | Bracket 2 | Control drift, brackets 1 / 2 |
| --- | ---: | ---: | ---: |
| T1, one bank, residual OFF | +1.35% | +1.28% | +0.05% / +1.80% |
| T1, one bank, residual ON | +2.26% | +0.82% | -1.54% / -0.09% |
| T1, eight banks, residual OFF | -1.66% | -4.68% | -0.96% / -9.92% |
| T1, eight banks, residual ON | +1.67% | +0.13% | -1.34% / -1.42% |
| T4, one bank, residual OFF | +2.38% | +1.04% | +1.18% / +0.42% |
| T4, one bank, residual ON | +2.44% | +2.86% | -0.75% / -0.31% |
| T4, eight banks, residual OFF | +3.36% | -2.37% | +2.12% / +1.25% |
| T4, eight banks, residual ON | +0.76% | -0.46% | -0.90% / +2.93% |

Positive values mean longer latency; controls use the geometric mean of
before/after medians. The T1 eight-bank residual-OFF direction is favorable
twice, but its second baseline drifts 9.92%. The corresponding T4 direction
changes sign. Other small differences retain unlocked-clock noise. These
results do not establish a broad repeatable benefit or explain the earlier
16%-26% T1 HC discrepancy.

For these three private-using HC kernels, the 4,096-slot policy derives
**157.125 MiB** of payload plus guards for the shared pool versus **392.875
MiB** for separate pools: **235.75 MiB more**. This is a calculation from the
selected kernel requirements, not a measured full-model memory cost. The
queue-wide default remains unchanged; no model throughput claim is made.

All **1,440 replays** complete, and **48 complete final output files** match
the published HC reference. Checks occur after each case, not every replay.
Both runtimes also pass CPU checks, 16 actual Q3_K private-memory replays and
24 multiqueue scratch/private submissions with analytic results and full
guards. This isolates allocation sharing only; it does not reject every
private-memory optimization or capture the installed NEO driver's policy.
No candidate is adopted and no Linux GPU result is claimed.
[Private-pool report](../orca-l0/demo/dg2-hc-private-pools-windows-2026-11-02.json)
and both source snapshots, commands, samples, checks and canonical bytes（本地归档：`../orca-l0/demo/dg2-hc-private-pools-windows-2026-11-02.json.gz`）.

### Narrow HC norm capture: lower private requirements, modest replay changes

An isolated Windows A770 candidate passes only the fields used by the actual
`fused_gr_read_multi` norm-port kernel. Its eight-token capacity, device
arithmetic, loads/stores and reduction/barrier order remain unchanged. The
capture shrinks **1,256 to 392 bytes**, and compiler private memory shrinks
**40,192 to 12,544 bytes per hardware thread (-68.79%)**. SIMD32, 128 GRFs,
64-byte SLM and zero reported spill remain unchanged; `.text` shrinks
22,080 to 15,872 bytes. Private bytes equal capture bytes times SIMD32 in
both images; this alone does not measure memory traffic or model throughput.

All 25 exported capture descriptors, counts and sizes are checked. Only the
norm ABI changes, and its host object is rebuilt. The compiler also changes
ten other ISA hashes, so the deployed pack deliberately replaces only
`fused_gr/image_20.zebin`: the other **1,241 modules remain byte-identical**.
Complete output, residual and Q8 bytes match for **32 cases covering T=1..8,
one/eight weight banks and residual OFF/ON**, with full end guards. Each case
runs two replays of eight HC reads. Another eight-case check matches the
previous 48-read reference exactly.

Two native baseline/candidate/baseline brackets then run ten warmups and
twenty retained 48-HC replays per case. No GPU timestamps or inner-operator
clocks are emitted. Only whole-replay host submission/completion clocks
remain, and GPU clocks are unlocked.

| Case | Replay latency change, bracket 1 | Bracket 2 | Control drift, brackets 1 / 2 |
| --- | ---: | ---: | ---: |
| T1, one bank, residual OFF | -1.72% | -3.63% | +0.70% / +1.37% |
| T1, one bank, residual ON | -0.24% | -0.35% | +0.33% / -0.21% |
| T1, eight banks, residual OFF | -0.69% | -1.71% | +0.57% / -0.26% |
| T1, eight banks, residual ON | +2.06% | +0.08% | +2.82% / -0.48% |
| T4, one bank, residual OFF | +1.66% | -3.51% | +2.21% / -4.17% |
| T4, one bank, residual ON | -1.50% | -0.03% | +0.78% / -3.01% |
| T4, eight banks, residual OFF | -0.52% | -2.33% | +3.30% / -2.48% |
| T4, eight banks, residual ON | -2.09% | -2.02% | -1.84% / -1.63% |

Negative values mean shorter latency; controls use the geometric mean of
before/after medians. Several cases move favorably twice, particularly the
roughly 2% T4/eight-bank/residual-ON result. Some small changes remain close
to drift, and T4/one-bank/residual-OFF changes sign. These are modest HC
replay changes, not a 68.79% performance gain or an explanation of the full
earlier native/L0 difference. All **48 final timing output files** match the
published reference after each case's thirty replays.

Strict offline parsing, ABI parsing and metadata line-ending failures are
retained. A later live configuration change from another MTP experiment is
also detected before the expanded check. The corrected runner uses the
exact archived original configuration and leaves the live file unchanged.
This candidate is not adopted, and there is no Linux GPU claim.
[HC norm-capture report](../orca-l0/demo/dg2-hc-norm-capture-windows-2026-11-02.json)
and runnable sources, selected old/new images, all-T bytes and raw brackets（本地归档：`../orca-l0/demo/dg2-hc-norm-capture-windows-2026-11-02.json.gz`）.

Two separate full-model **native Short** baseline/candidate/baseline brackets
then use the same frozen engine, runtime and other kernels. Each process
has ten warmup requests and five retained requests. Single-step min-p and
last-draft threshold are explicitly 0.5, cumulative floor is explicitly zero
and maximum MTP depth is four. Both arms have 5,771 expert slots and reserve
1,536 MiB, with no prefix reuse. `decode_ms` excludes initialization,
preparation and prefill; TTFT remains a separate full-request measure.

| Short bracket | Decode tok/s, before / candidate / after | Decode change | Decode control drift | TTFT control / candidate | TTFT change |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 44.330 / 44.711 / 44.452 | +0.72% | +0.27% | 924.516 / 921.634 ms | -0.31% |
| 2 | 44.328 / 44.559 / 44.204 | +0.66% | -0.28% | 925.160 / 921.343 ms | -0.41% |

All **90 requests** produce the exact same 76 tokens, natural stop, 57/57
accepted/offered drafts and zero reused/resumed tokens. Profiling gates are
off; the normal request clocks remain. The two native-only brackets support
a small, roughly 0.7% improvement for this fixed Short case. They do not
measure L0 performance with the candidate, eliminate the earlier native/L0
gap or establish Linux GPU performance. These two records precede the
following L0 validation and source adoption.
[First Short bracket](../orca-l0/demo/dg2-norm-capture-short-windows-2026-11-02.json)
and raw requests, commands, policy and hashes（本地归档：`../orca-l0/demo/dg2-norm-capture-short-windows-2026-11-02.json.gz`）;
[confirmation bracket](../orca-l0/demo/dg2-norm-capture-short-confirm-windows-2026-11-02.json)
and confirmation evidence（本地归档：`../orca-l0/demo/dg2-norm-capture-short-confirm-windows-2026-11-02.json.gz`）.

The same capture then passes **32 T=1..8 full-output/guard cases on L0**.
Its first full Short L0 bracket is **48.796 / 49.013 / 48.906 tok/s**, a
**+0.33%** change against +0.23% control drift. A repeat is
48.966 / 49.077 / 47.461 tok/s; its **-3.07% control drift** makes the
apparent +1.80% unsuitable as an optimization claim. Both brackets retain
all 90 exact 76-token outputs and 57/57 drafts, with the same explicit policy.
[L0 bracket](../orca-l0/demo/l0-norm-capture-short-windows-2026-11-02.json)
and sources, all-T checks and requests（本地归档：`../orca-l0/demo/l0-norm-capture-short-windows-2026-11-02.json.gz`）;
[L0 repeat](../orca-l0/demo/l0-norm-capture-short-confirm-windows-2026-11-02.json)
and repeat evidence（本地归档：`../orca-l0/demo/l0-norm-capture-short-confirm-windows-2026-11-02.json.gz`）.

The minimal source change is now adopted. Current `prepare.transform`
generates exactly the GPU-tested candidate. Fresh Linux oneAPI 2025.3.2
device SPIR-V and host-object compilation also pass against the frozen Linux
frontend headers: all 25 exports compile, only the norm ABI changes from
1,256 to 392, and the other 24 capture ABIs match. This is a compile check,
not a full new frontend rebuild, native module bake or Linux GPU result.
The normal rebuild emits the complete fused_gr pack rather than the selected
single-image diagnostic. That complete pack now passes all **64** T=1..8
HC/residual/Q8 output-and-guard cases across Windows native DG2 and L0.
The first complete-pack native Short bracket gives **44.072 control /
43.716 candidate tok/s (-0.81%)**, with +0.37% control drift and all 45 exact
outputs. This does not support carrying the selected-image speedup into a
normal full rebuild. The complete-pack L0 Short bracket now passes all 45 exact
requests: **48.465 control / 48.609 candidate tok/s (+0.30%)**, against **+0.63%**
control drift. TTFT changes from 886.008 to 883.678 ms (-0.26%). This small change
does not establish a speedup. The source cleanup lowers private requirements;
a full-pack speedup is not established.
[Complete-pack L0 bracket](../orca-l0/demo/l0-norm-capture-fullpack-short-windows-2026-11-02.json)
and raw requests, module snapshot and evidence（本地归档：`../orca-l0/demo/l0-norm-capture-fullpack-short-windows-2026-11-02.json.gz`）.
[Complete-pack native bracket](../orca-l0/demo/dg2-norm-capture-fullpack-short-windows-2026-11-02.json)
and full-pack output checks, raw requests and commands（本地归档：`../orca-l0/demo/dg2-norm-capture-fullpack-short-windows-2026-11-02.json.gz`）.
The production executable and immutable v6 source package remain unchanged.
New v7 exports the adopted runtime/norm sources while retaining v6's engine
and MTP generator. Its three Linux CPU checks and three dependency audits
pass; full v7 frontend rebuilding, Linux baking and GPU validation remain open.

### Transfer, library-call and assembly investigation

A read-only audit of the frozen shared-module control checks all **1,242
zebin hashes** and extracts **1,892 `.text` sections, 77,438,144 bytes**.
Both backends receive the same module instruction bytes. This rules out an
offline kernel-code difference in that control, while driver relocations,
dispatch programming, memory placement/cache state and transfers still need
separate checks. It does not establish identical resident GPU instructions.

The frozen HC host objects differ: **411,703 bytes native / 320,109 bytes
L0**. Native's object has 46 static `_Mtx_lock` and 84 `_Mtx_unlock`
relocation sites; L0's has none. These totals cover the entire object,
including alternate paths and cleanup. They are not executed call counts or
proof of decode lock contention; recorded replay need not call those wrappers.
The executable imports `GDI32.dll` on native and `ze_loader.dll` on L0;
neither imports SYCL or UR. Linked static `.lib` files do not create DLL
boundaries. Dynamically loaded modules and calls inside the Intel driver are
outside this import-table check.

The native adapter stages unregistered Host pointers through a CPU copy and
refreshes captured uploads on replay. A separate diagnostic runtime build
counts submitted payloads at the exact engine decode boundaries, multiplying
captured operations on every replay. It passes an independent replay/reset
counter check and **22 exact Short outputs** across eleven requests per backend.
All eleven paired requests match explicit transfer byte totals; the last
request, after ten warmups, gives:

| Decode of the same 76 tokens | Native DG2 | L0 |
| --- | ---: | ---: |
| H2D payload bytes / operations | 968,318,976 / 659 | 968,318,976 / 659 |
| D2D payload bytes / operations | 4,554,752 / 1,445 | 4,554,752 / 134 |
| D2H bytes / operations | 0 / 0 | 0 / 0 |
| Fill bytes / operations | 0 / 0 | 0 / 0 |

Native splits strided 2D regions into rows, adding **1,311 copy operations**
for the same payload. Its 659 H2D and 1,445 D2D copies use explicit compute
kernels. Subtracting those 2,104 launches leaves **39,985 mathematical
launches**, matching the L0 wrapper's mathematical-kernel count. L0's driver
may expand copy APIs into hidden kernels or blits: total hardware dispatch
counts have not been compared. Similarly native's 136 KMT submissions and
L0's 97 replay API calls describe different layers.

These counters exclude preparation and prefill. They measure submitted copy
payloads, not physical PCIe/VRAM transactions or direct kernel reads from
imported Host weights. Extra CPU staging bytes remain unmeasured. Diagnostic
timing is excluded from performance claims. A batched native 2D copy kernel
is tested separately below.
[Decode transfer report](../orca-l0/demo/dg2-decode-transfers-windows-2026-11-02.json)
and counter/self-check sources, exact engine boundaries, raw logs and commands（本地归档：`../orca-l0/demo/dg2-decode-transfers-windows-2026-11-02.json.gz`）.
[Static report](../orca-l0/demo/dg2-transfer-static-assembly-windows-2026-11-02.json)
and runnable audit, host disassemblies, import tables and hashes（本地归档：`../orca-l0/demo/dg2-transfer-static-assembly-windows-2026-11-02.json.gz`）.

An isolated candidate adds one vectorized native copy kernel for registered,
disjoint pitched spans, retaining the existing row/staging fallback for ordinary
Host pointers and overlapping bounding regions. Validation covers 13 shapes and
78 changing-input region-versus-row replays, aligned and unaligned pointers/pitches,
byte tails, full padding/end guards, zero sizes, overflow, bounds, overlap and
self-copy checks. Adapter tests cover registered destinations and unregistered
Host inputs/outputs. Production sources and Linux packages are unchanged.

Only the frozen layer/verify/MTP Host objects are recompiled for this adapter
change; their model/MTP source and policy stay fixed. Native baseline/candidate
share one new Runtime library and one four-export memory module. Both also use
the same 1,242 mathematical zebins as the L0 control. No diagnostic counters or
GPU timestamps run in the timed executables. All four processes run ten warmups
and five retained Short requests; every one of the **60** requests matches the
same natural 76-token output and 57/57 drafts, without reuse. Step/last min-p is
0.5, cumulative floor zero, expert slots 5,771 and reserve 1,536 MiB.

| Short result | Native baseline, bracket control | Native 2D-copy candidate | L0 control |
| --- | ---: | ---: | ---: |
| Decode tok/s | 44.306 | 44.194 | 48.541 |
| TTFT ms | 931.670 | 929.790 | 880.389 |
| Prefill ms | 877.900 | 876.900 | 836.600 |

Native decode changes **-0.25%**, against **-0.30%** baseline drift. TTFT changes
-0.20%. This does not establish a gain; the candidate remains **8.96% slower than
L0** on decode and is not adopted. The earlier copy counters describe the row
control, not newly measured candidate dispatch totals. The comparison supports
looking beyond this particular copy implementation; it does not rule out other
copy-engine choices, cache/memory placement or command-stream scheduling costs.

Read-only live module snapshots from these processes extend the import-table
audit. Native loads GDI32/gdi32full, without ze_loader, ze_intel_gpu64 or Intel GMM
DLLs in the snapshot. L0 loads those Windows libraries plus ze_loader,
ze_intel_gpu64, ze_tracing_layer, GMM libraries and igdfcl64. Neither snapshot has
SYCL/UR DLLs. Loading a compiler/tracing DLL does not prove runtime compilation
or active tracing, and these snapshots do not measure DLL-boundary time or calls
inside the driver. The remaining gap is still unassigned.
[2D-copy report](../orca-l0/demo/dg2-copy2d-short-windows-2026-11-02.json) and
runnable checks, candidate sources, new memory module, all requests, commands and module snapshots（本地归档：`../orca-l0/demo/dg2-copy2d-short-windows-2026-11-02.json.gz`）.

### Dynamic selected DLL and KMT calls at matched Decode boundaries

A new process-local observer patches only the diagnostic executable's selected
import slots and the vendor driver's GetProcAddress import. It observes the
selected driver KMT functions resolved after initialization, without modifying
other processes or system/driver settings. An independent active/inactive
counter check passes. Frozen model objects and the tested uninstrumented native
control/L0 runtime libraries remain fixed. All **22** requests match the same
76 tokens and 57/57 drafts under the explicit policy above.

After ten warmups, the final Decode request gives these actual counts:

| Observed call | Native DG2 | L0 |
| --- | ---: | ---: |
| D3DKMTSubmitCommand | 136 | 795 |
| D3DKMTWaitForSynchronizationObjectFromCpu | 96 | 0 |
| D3DKMTWaitForSynchronizationObjectFromGpu | 0 | 84 |
| Application `_Mtx_lock` / `_Mtx_unlock` | 2,153 / 2,153 | 130 / 130 |
| zeCommandListImmediateAppendCommandListsExp | not used | 97 |
| zeCommandListAppendMemoryCopy | not used | 659 |
| zeCommandListAppendBarrier | not used | 39 |
| zeCommandListAppendLaunchKernel / zeKernelSetArgumentValue | not used | 0 / 0 |

L0's mathematical append and set-argument counts are zero in **all eleven**
Decode requests: the selected calls are already amortized by graph replay.
Native already submits fewer KMT batches; increasing batching cannot be assumed
to close its measured Short gap. This updates the previous comparison of
136 native KMT submissions against 97 L0 *API replays* at different layers.

The diagnostic's inclusive lock-plus-unlock totals are **0.493 ms native /
0.052 ms L0**; its KMT SubmitCommand totals are 5.656 / 6.812 ms. These contain
function/driver work and preemption, include probe overhead and are not pure
DLL transition costs. Native's CPU fence-registration API time excludes its
subsequent WaitForSingleObject; L0 host-synchronization times include waiting
for GPU work. Never compare these wait durations as equivalent dispatch costs
or add nested L0/KMT call durations together.

Only selected application imports and selected vendor dynamic resolutions are
observed. Driver internal locks/private entry points, inlined calls, direct
monitored-fence reads, CPU staging bytes and physical Host/VRAM traffic remain
unmeasured. Counts alone do not identify contention or assign the remaining
8%-9% throughput gap. The separately uninstrumented Short brackets retain
authority for speed comparisons; no new speed result comes from this probe.
[Dynamic call report](../orca-l0/demo/dg2-dynamic-calls-windows-2026-11-02.json)
and observer, exact engine boundaries, requests, logs and build commands（本地归档：`../orca-l0/demo/dg2-dynamic-calls-windows-2026-11-02.json.gz`）.

An extended observer reuses the verified generate objects and correlates actual
context creation/destruction with every successful SubmitCommand. All **22**
additional requests retain exact output/drafts. Every submission has one known
context: all Decode work targets **node 5**, whose driver metadata name is
`Compute`. The driver reports engine_type 0 for that node, so the friendly name
and calibrated node identity are retained rather than relabeling it from the
enum alone. L0 also creates node 2 with engine_type 6 (`DXGK_ENGINE_TYPE_COPY`),
but none of its observed Decode submissions target node 2. Metadata is queried
once per adapter/node and cached. Merely creating a Copy context did not establish
that the measured Decode uses copy-engine overlap.

The final request's submitted *primary* command lengths sum to **8,171,544 bytes
native / 143,936 bytes L0**, across 136 / 795 batches. Secondary command lists,
ISA and tensor data are excluded. L0 can enter recorded work via batch-buffer
jumps; this primary-only ratio cannot establish total command traffic or redundant
packet counts. The subsequent primary/secondary capture below resolves this
command-volume question; resident ISA/relocations remain open. This result
also does not measure actual PCIe transactions or native CPU staging copies.
[Submission-node report](../orca-l0/demo/dg2-submit-nodes-windows-2026-11-02.json)
and observer/check sources, raw requests, node metadata, SDK enum and hashes（本地归档：`../orca-l0/demo/dg2-submit-nodes-windows-2026-11-02.json.gz`）.

### Executed primary/secondary commands and mutex callers

A separate observer captures mapped command buffers for the final matched Short
request, recursively following `MI_BATCH_BUFFER_START` with bounded lengths and
verified DG2 packet definitions. Both backends complete all 22 exact requests;
neither has an unknown/truncated packet or unmapped secondary-list failure.

| Final Short request, including executed secondary ranges | Native | L0 |
| --- | ---: | ---: |
| Decoded command bytes | 8,171,308 | 7,381,108 |
| Captured batch files | 136 | 1,013 |
| COMPUTE_WALKER | 42,089 | 40,932 |
| CFE_STATE | 2,913 | 97 |
| STATE_BASE_ADDRESS | 5,826 | 218 |
| STATE_COMPUTE_MODE | 2,913 | 2,392 |
| PIPE_CONTROL | 39,682 | 39,359 |

These executed-range lengths exclude unused padding and are not physical bus
transactions. Capture strongly perturbs timing, especially for L0 graph replay.
Walkers include hidden copies, so they cannot replace mathematical-kernel counts.
Repeated state programming is a concrete lead, not an established speed cause.
[Packet report](../orca-l0/demo/dg2-packets-windows-2026-11-02.json) and
raw primary/secondary buffers, parser and checks（本地归档：`../orca-l0/demo/dg2-packets-windows-2026-11-02.json.gz`）.

Another 11 exact requests record actual mutex return addresses and resolve them
against the same link's map. All 2,153 imported application acquisitions are
attributed: 659 per-copy context locks; 310 context plus 310 point locks in
`Point::finish`; 117 plus 117 context acquisitions in `Stream::wait`; 97 graph
replay context acquisitions and 97 publication acquisitions; only 19 allocation
plus 19 release locks in the staging pool. CPU I/O workers account for 136.
These are dynamic acquisitions, not static call-site estimates or contention
measurements. [Caller report](../orca-l0/demo/dg2-lock-sites-windows-2026-11-02.json)
and raw logs, observer and same-link map（本地归档：`../orca-l0/demo/dg2-lock-sites-windows-2026-11-02.json.gz`）.

### Device allocation calibration and alignment control

A process-local observer records the vendor driver's allocation descriptors and
GPU VA map results for **15** device size/alignment combinations: 64 KiB through
1 GiB, requesting 64-byte, 64-KiB or 2-MiB alignment. All allocation type, owner,
range and requested-alignment checks pass. The driver reports `pageSize=65536`
through the public API throughout. Pinned NEO returns alignment metadata here;
this is not a measurement of physical GPU page-table entries.

For allocations of at least 2 MiB, the observed resource descriptor and VA policy
use two different calibrated fields from native's 64-KiB template. An isolated
compile-flag candidate changes only those fields for 2-MiB physical chunks and
aligns device VA. It preserves LocalOnly/residency checks, the 1-GiB part limit
and exact physical byte totals. A final nonmultiple chunk splits into a bulk
and unpadded 64-KiB tail, adding at most one allocation. Extra reservation space
is virtual only and the whole original reservation is released.

Both control and candidate pass **15** Windows CTests, five actual numerical
GPU groups and an added ten-size / forty-changing-input allocation/replay check.
Sizes include tails, 1-GiB boundaries and allocations over 4 GiB. The frozen
model objects, 1,242 mathematical zebins, min-p and memory budgets stay fixed.
All **60** requests match the same 76 tokens and 57/57 drafts. Decode excludes
preparation, initialization and prefill; TTFT is separate. Timing has no inner
diagnostic counters or GPU timestamps.

| Expanded alignment trial | Native bracket control | Aligned native | L0 control |
| --- | ---: | ---: | ---: |
| Decode tok/s | 44.076 | 44.395 | 48.426 |
| TTFT ms | 927.259 | 927.614 | 881.646 |
| Prefill ms | 874.849 | 875.000 | 838.200 |

Decode changes **+0.72%**, close to **-0.62%** bracket drift, while TTFT changes
+0.04%. Native remains **8.32% slower** than the matched-policy L0 control. This
does not establish a gain; production and Linux packages remain unchanged.

A separate exact-output diagnostic confirms 147 of 385 cumulative allocation
events use the new policy, covering **13,151,240,192 / 13,420,265,472 bytes
(98.00%)** of device allocation requests. These are cumulative event bytes,
not peak residency, physical traffic or a measured PTE size. An earlier trial
excluded all allocations with tails, covered only 3.81% and gave +0.46% decode
against +1.00% control drift. Each trial has its own L0 control; changes between
their absolute L0 rates cannot be counted as native progress.
[Allocation/alignment report](../orca-l0/demo/dg2-allocation-alignment-windows-2026-11-02.json)
and probe blobs/sources, both brackets, coverage, commands and raw requests（本地归档：`../orca-l0/demo/dg2-allocation-alignment-windows-2026-11-02.json.gz`）.

### Adopted copy batching and native ownership

The combined native source change now records a disjoint pitched copy as one
GPU kernel, batches registered copies at existing command boundaries and reuses
immutable coarse completion points. Producer registry snapshots use allocation
lifetime pins; another queue cannot consume the producer's pending-copy array.
Ordinary Host uploads retain their source contents, while recorded Host sources
refresh from their original addresses on every replay. Collection releases
leases under the native consumer rather than taking duplicate completion locks.
No timer or wait-to-fill delays ready tails.
Eager ordinary Host inputs up to 128 KiB now use producer-owned cached snapshots,
covering the measured 96-KiB uploads. Larger inputs retain immediate staging;
recorded sources still refresh on replay. The extra CPU copy reuses capacity
and preserves the same GPU submitted bytes and command boundaries.
The ordinary Host-input path also checks the registry epoch. A device
allocation added after a pending batch took its snapshot must use the current
registry; it cannot be mistaken for CPU-readable Host memory.

The ownership confirmation fixes step/last min-p at 0.5, cumulative floor zero, MTP4,
5,771 expert slots and 1,536-MiB reserve. All 1,242 numerical zebins remain
identical. The performance bracket, independent payload probe and independent
locking probe complete **82 + 11 + 33 = 126** exact requests: 76 tokens,
57/57 drafts, no reuse. Only uninstrumented `decode_ms` measures performance;
preparation, initialization and prefill remain excluded, with TTFT separate.

| Last warmed Short request | Native before | Adopted native | L0 |
| --- | ---: | ---: | ---: |
| D2D operations | 1,445 | 134 | 134 |
| D2D payload bytes | 4,554,752 | 4,554,752 | 4,554,752 |
| Application `_Mtx_lock` | 2,147 | 136 | 124 |
| Application shared SRW acquisition | 135 | 0 | 0 |
| Application exclusive SRW acquisition | 0 | 0 | 0 |
| Total observed application acquisitions | 2,282 | 136 | 124 |

H2D remains 659 operations / 968,318,976 bytes. D2D operation counts reach
parity; total observed application locking falls **94.04%** and still exceeds
L0. This probe counts direct application Win32 SRW imports as well as mutex
imports: MSVC's `_Smtx` wrappers link statically here, so watching `_Smtx`
imports alone misses them. The observer's own SRW lock is explicitly excluded,
and a real shared/exclusive lock self-check verifies the hooks. Lock/unlock
counts balance. Driver/private/inlined locking and contention remain unmeasured.

All 136 candidate acquisitions belong to CPU I/O. A separate matching-map
census observes 130 I/O calls and **zero native runtime mutex calls**, replacing
the preceding version's 97 graph and 39 checkpoint acquisitions. L0's all-lock
probe has 124 I/O calls: total application counts are not exactly equal, and
I/O scheduling variation must not be counted as another runtime optimization.
An independent follow-up now samples the existing PLE statistics outside the
observed decode boundaries. Both warmed requests perform **zero business SSD
reads**. Native records 15 background keepalive reads, L0 records 14; both
record 20 `issue` and 20 `collect` acquisitions. Each keepalive accounts for
six acquisitions across the file/reader workers: **40 + 6 × 15 = 130** versus
**40 + 6 × 14 = 124**. The original 100-ms keepalive period stays enabled.
This explains the residual count in this probe; total acquisitions still
differ, and no locks or waits were hidden to force equality.
[I/O statistics and matching-map attribution](../orca-l0/demo/dg2-current-fields-windows-2026-11-02.json).
Original native has 2,017 runtime mutex calls plus 130 I/O calls in this probe.
A dedicated consumer now owns native mutations; producers publish stack calls
with a lock-free MPSC exchange and block on atomic completion words. Existing
producer copy batches and immutable checkpoint fast paths avoid per-copy
handoffs. Completion words occupy separate cache lines from their reference
counts. GPU fence waits remain outside the consumer, so callback signals can
progress. These are warm-path observations; cold pools and unresolved host
events retain their own synchronization. Completion
observers now own their Windows device/fence mappings or an `F_DUPFD_CLOEXEC`
duplicate of the Linux DRM fd and its timeline. Their waits no longer need
the 135 shared lifetime-lock acquisitions. Atomic teardown cancellation is
preserved; fd duplication happens once per queue, outside the wait path.
The shared recorder is mutated only by its dedicated consumer.

| Uninstrumented Short | Native bracket control | Adopted native | L0 |
| --- | ---: | ---: | ---: |
| Decode tok/s | 44.294 | 44.061 | 48.665 |
| TTFT ms | 928.653 | 929.200 | 880.203 |
| Prefill ms | 875.846 | 876.600 | 836.700 |

The clean native control is the preceding adopted 128-KiB snapshot version;
the count table compares original native source with the final adopted source.
Native decode changes **-0.53%**, with **+0.27%** before/after control drift;
TTFT changes **+0.06%**. This does not establish a throughput gain; native remains **9.46% slower**
than L0. Clocks are unlocked, and absolute rates from earlier brackets are
not a cumulative progress series. Lower operation counts are established;
complete lock parity and a native throughput win are not.

All 80 Host objects are rebuilt against the changed adapter layout, with four
compiler workers joined before GPU timing under the shared experiment lock.
An eight-producer CPU check covers 32,000 calls, nested dispatch, exception
propagation, references, move-only returns and four teardowns. Its executor
class is byte-identical to the adopted header. Fifteen native
Windows checks, six numerical GPU groups, two memory-module versions and the
complete Queue suite pass. A two-consumer graph replay/discard regression also
fails the prior library and passes the corrected library. Eight two-queue
teardown races retain old-runtime completion handles and pass with resource
ownership; the observer check covers 65 submissions. The checked-in
[`orca_queue_check.cpp`](../sycl/orca_queue_check.cpp) covers 137-copy partial
tails, bounds/free guards, immediate stack snapshots, changing recorded Host
inputs, a newly allocated device source while an older copy batch is pending,
registered pitched copies with full padding checks, eight concurrent
finishers, 96-/128-KiB upload boundaries, overwritten/destroyed Host inputs,
24 cross-thread queue-switch waves and retained graphs replayed by
two consumers. The adopted headers pass this suite at batch limits 64 and 1.
The same new regression rejects the 64-KiB adapter. The unchanged Linux runtime
source retains its four CPU checks plus dependency and missing-driver/device
error checks; Linux adapter compilation and GPU execution remain unvalidated.

Re-bake `src/drm/memory.cl` with its fourth `orca_copy_2d` export for the 134-copy
path. Old three-export modules keep row fallback, with capability checked once
at binding. Native source is adopted; production executables/configuration,
MTP policy, the root README and immutable Linux handoff packages are unchanged.
[Latest batched ownership confirmation](../orca-l0/demo/dg2-owner-batched-windows-2026-11-02.json) and
703 independently verified source, request, command and hash records（本地归档：`../orca-l0/demo/dg2-owner-batched-windows-2026-11-02.json.gz`）.
The [preceding 128-KiB confirmation](../orca-l0/demo/dg2-host-snapshots128-windows-2026-11-02.json)
retains its 272-acquisition result.
The [preceding completion-ownership confirmation](../orca-l0/demo/dg2-completion-leases-windows-2026-11-02.json)
retains its 291-acquisition result.
The [preceding registry-safe batching confirmation](../orca-l0/demo/dg2-copy-lock-batching-confirm-windows-2026-11-02.json)
retains its 426-acquisition result.

An isolated owner-readiness follow-up is rejected. All 126 requests still match,
but the independent all-lock probe remains **2,288 original / 426 candidate /
130 L0**. Clean decode is **43.097 control / 43.081 candidate / 47.056 L0 tok/s**:
**-0.04%**, against **+0.19%** control drift. No throughput or locking gain is
established. The adopter stops before changing production source.

The shared-lock probe now also captures application stacks and resolves them
against the matching executable's linker map. All **135** shared acquisitions
come from `NativeCompletion::wait` (drafting, verification and completion waits).
They do not come from the owner-readiness path changed in this trial. Stack
capture is diagnostic only and is absent from the clean timing executables;
private/driver locks and contention remain outside this observation.
[Rejected trial](../orca-l0/demo/dg2-owner-query-rejected-windows-2026-11-02.json)
and exact sources, matching maps, checks and requests（本地归档：`../orca-l0/demo/dg2-owner-query-rejected-windows-2026-11-02.json.gz`）.

Another isolated trial expands eager ordinary Host snapshots to 64 KiB but is
not adopted. All 80 Host objects are rebuilt; native runtime acquisitions remain
155 (97 replay, 39 checkpoint, 19 copy), and all-lock totals stay 291 versus
130 for L0. An earlier 285-call census merely has six fewer CPU I/O calls.
Clean decode is 44.140 control / 44.127 candidate / 48.596 L0 tok/s: -0.03%,
with -0.29% control drift. A separate diagnostic changes one Host object while
preserving its ABI and identifies all 19 copy acquisitions as
`fallback bytes=98304 to=0 from=-1 epoch=435 cached=435 recording=0`:
96-KiB ordinary Host-to-Device uploads, beyond the tested 64-KiB limit, with a
current registry snapshot. This proves the copy path and size, not a particular
source-level caller. All 137 requests across four probes match.
[Rejected trial and actual paths](../orca-l0/demo/dg2-host-snapshots-rejected-windows-2026-11-02.json)
and 599 source, command, output and hash records（本地归档：`../orca-l0/demo/dg2-host-snapshots-rejected-windows-2026-11-02.json.gz`）.

### Dedicated native owner: zero runtime acquisitions, first trial not adopted

An isolated consumer thread owns the shared native recorder. Producers publish
stack-resident calls through a lock-free MPSC queue and block on a 32-bit atomic
completion word. GPU fence waits stay outside the consumer, so callback signals
and host gates can still progress. No GPU wait or copy is removed.

| Last warmed 76-token Short request | Original native | Owner trial | L0 |
| --- | ---: | ---: | ---: |
| D2D operations | 1,445 | 134 | 134 |
| Imported application mutex acquisitions | 2,153 | 136 | 130 |
| Imported application shared SRW acquisitions | 135 | 0 | 0 |
| Total observed application acquisitions | 2,288 | 136 | 130 |

Every one of the trial's 136 mutex calls resolves to `PleReader` or `DirectFile`
in its matching linker map: native runtime acquisitions fall from the adopted
version's 136 to **zero**. Remaining I/O wakeup counts vary; the application totals
are not exactly equal. D2D stays 4,554,752 bytes, H2D stays 659 operations and
968,318,976 bytes, and native GPU submissions stay 136. Private/driver/inlined
locks, contention and physical bus traffic are outside these measurements.

The clean before/candidate/after bracket gives **44.089 / 43.404 tok/s** for
the adopted native control and trial, versus **48.497 tok/s** for L0.
The change is **-1.55%**, with **-0.05%** control drift; TTFT increases from
927.513 to 951.424 ms. Synchronous per-operation handoffs add scheduling cost,
so this first trial is **not adopted**. It left production at adoption-v4;
the batched follow-up in the adopted section above is now adoption-v5.
Decode excludes initialization, preparation and prefill; TTFT remains separate.

All 80 Host objects are rebuilt from the frozen engine. Validation covers an
8-producer, 32,000-call CPU check, nested calls and exceptions, 15 native checks,
six numerical GPU groups, old/new memory modules, the full staged Queue suite
and 126 exact model requests. Step/last min-p remain 0.5, cumulative floor zero,
MTP4, 5,771 slots, 1,536-MiB reserve, identical numerical zebins, 76 output tokens
and 57/57 drafts with no reuse. Linux adapter/GPU execution remains unvalidated.
[Ownership trial](../orca-l0/demo/dg2-owner-worker-windows-2026-11-02.json)
and 587 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-owner-worker-windows-2026-11-02.json.gz`）.

### Current v5 commands: actual L0 field differences and isolated controls

A fresh A770/32.0.101.9034 capture uses the adopted copy batching and dedicated
owner, with all 80 current Host objects. It follows primary and secondary
command buffers during the eleventh identical Short decode. Both decodes
complete with zero capture failures. These instrumented timings are not
performance evidence.

| Observed command work | Current native | Installed L0 |
| --- | ---: | ---: |
| KMT submissions | 136 | 795 |
| Captured command ranges | 136 | 1,011 |
| Executed-range command bytes | 7,961,144 | 7,380,828 |
| COMPUTE_WALKER | 40,778 | 40,932 |
| CFE_STATE | 2,894 | 97 |
| STATE_COMPUTE_MODE | 2,894 | 2,392 |
| STATE_BASE_ADDRESS | 5,788 | 216 |
| PIPE_CONTROL | 39,624 | 39,357 |

The 1,311-walker reduction from the earlier native capture matches the adopted
D2D batching reduction exactly. L0 has 154 more walkers; internal copy/fill
kernels differ, so this count alone does not identify a mathematical difference.
Command bytes are not physical GPU memory or PCIe traffic.

The typed decoder independently checks every packet count against the capture.
Actual L0 sets CFE `ComputeOverdispatchDisable=1`, while native uses zero.
L0's IDD thread-group dispatch values 2/3 mean two groups for one hardware
thread and one group otherwise; native's default zero means eight groups.
L0 SBA L1 policy is WB (2), native is WBP (0). These are actual installed-driver
observations, not predictions from a newer source checkout. Maximum hardware
threads (4096), EU fusion, force-noncoherent mode and the relevant MOCS values
(6) agree. L0's denorm field is FTZ (0), native is SETBYKERNEL (1); this math
mode is observed but left unchanged.

Three isolated runtime libraries reuse the same current Host objects, 1,242
mathematical zebins, memory module, barriers and model policy. Each passes
15 native checks and six GPU numerical groups. Clean Short controls use ten
warmups and five retained requests per arm; all 90 outputs remain 76 tokens
with 57/57 drafts, no reuse, step/last min-p 0.5 and cumulative floor zero.

| Clean runtime-only control | Decode tok/s | Change from native control | TTFT ms |
| --- | ---: | ---: | ---: |
| Bracketed current native | 43.922 | — | 931.126 |
| L1 WB only | 44.217 | +0.67% | 930.824 |
| Coupled overdispatch/thread-group policy | 44.965 | +2.38% | 921.356 |
| Both changes | 45.050 | +2.57% | 921.672 |
| Installed L0 | 48.593 | — | 883.146 |

Before/after native drift is -0.24%. The dispatch control remains **7.47%
slower than L0**. The cache addition contributes only 0.19% over dispatch in
this round and does not justify adopting both. These isolated libraries do
not change production source; a calibrated C0-only dispatch follow-up is
validated separately. Decode excludes preparation, initialization and prefill;
TTFT remains separate. Linux GPU performance is unvalidated.

[134 exact capture/control/I/O requests](../orca-l0/demo/dg2-current-fields-windows-2026-11-02.json)
and 1,432 evidence files: packet bytes, typed decoder, sources, commands, logs and outputs（本地归档：`../orca-l0/demo/dg2-current-fields-windows-2026-11-02.json.gz`）.

### Adopted C0 dispatch policy: repeated native improvement

The follow-up enables only the observed overdispatch/thread-group policy, on
the existing calibrated A770/32.0.101.9034 WDDM path when `RevisionID == 8`.
Linux and other steppings retain their original fields. L1 policy, denorm mode,
SLM, mathematical ISA, copy batching, ownership, dependencies and model policy
stay unchanged. The encoder tracks the policy with its other global state;
changing it re-emits CFE rather than reusing stale programming.

| Clean C0-only repeat | Native bracket control | Adopted candidate | L0 |
| --- | ---: | ---: | ---: |
| Decode tok/s | 43.803 | 45.034 | 48.621 |
| TTFT ms | 933.226 | 924.809 | 885.362 |
| Prefill ms | 880.499 | 871.500 | 837.800 |

Decode improves **2.81%**, with **+0.57%** native before/after drift; the earlier
isolated dispatch control improved **2.38%** with **-0.24%** drift. TTFT falls
0.90%. These are repeated improvements over native v5; native is still
**7.38% slower than L0**, so a native throughput win remains unestablished.
Decode excludes initialization, preparation and prefill. Clocks are unlocked.

The library reuses the exact current 80 Host objects behind unchanged public
headers. Fifteen Windows runtime checks, six GPU numerical groups and three
Linux CPU checks pass. The CPU checks cover both opt-in and fallback packet
fields, one/two-thread groups, policy transitions and state reuse. All 60 model
requests match 76 tokens, 57/57 drafts, no reuse, step/last min-p 0.5, cumulative
floor zero, MTP4, 5,771 slots, reserve 1,536 MiB and the same 1,242 mathematical
zebins. Linux CPU checks establish encoder/fallback behavior, not Linux GPU or
full-model correctness.

The five changed core/test files are now adopted as
`copy-batch-production-adoption-v6.json`; all 11 tracked production-source hashes
are independently checked against the measured sources. The earlier D2D/lock
counts are v5 measurements; this change retains the same copy/ownership code
and does not relabel them as a new v6 census. The production executable,
configuration, MTP files, root README and immutable Linux bundles are unchanged.
[Calibrated repeat and source adoption](../orca-l0/demo/dg2-overdispatch-c0-windows-2026-11-02.json)
and 133 evidence files with independent hash checks（本地归档：`../orca-l0/demo/dg2-overdispatch-c0-windows-2026-11-02.json.gz`）.

### Occupancy-based preferred SLM matches L0, without a measured gain

An independent typed pass over the cached actual commands correlates required
SLM size, hardware threads per group and preferred partition. The NEO-style
rule rounds SLM per group, multiplies by the rounded-up number of groups sharing
a DSS, clamps to 128 KiB and selects a 16/32/64/96/128-KiB partition. A calibrated
thread parameter of 128 matches **all 40,932 L0 walkers**, including zero-SLM
cases. Parameters 97 through 128 produce the same observed choices; this does
not independently measure hardware topology. A parameter of 64 mismatches
3,228 walkers. No new GPU capture or repeated device query is needed for this
analysis.

This is a distinct experiment from the earlier minimum-per-group policy that
regressed 7.18%. The isolated library retains the current C0 dispatch policy,
cache settings, denorm mode, ISA and dependencies, enabling the calibrated SLM
rule only on the tested WDDM C0 path. Linux keeps the existing MAX fallback.
Fifteen Windows tests, six GPU numerical groups and three Linux CPU checks
pass, including captured-value checks, conservative fallback, bucket boundaries
and changed SLM requirements on consecutive walkers.

The clean native/candidate/native bracket gives **45.056 → 45.021 tok/s
(-0.08%)**, with **-0.51%** control drift; L0 gives **48.677 tok/s**. TTFT is
923.430 → 921.037 ms (-0.26%), against +0.92% control drift. All 60 requests
match 76 tokens and 57/57 drafts with the same balanced policy, expert budget,
1,242 mathematical zebins and no reuse. Decode excludes preparation,
initialization and prefill.

The experiment establishes field matching, **not a performance improvement**.
It is not adopted; source remains adoption-v6 and nonzero preferred SLM stays
MAX. This does not rule out interactions or other workloads.
[SLM policy, joint distributions and all clean samples](../orca-l0/demo/dg2-slm-occupancy-windows-2026-11-02.json)
and 149 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-slm-occupancy-windows-2026-11-02.json.gz`）.

### Actual scratch descriptors identify a repeated writeback gain

The bounded command observer follows each observed L0 context's scratch base
and offset and reads its CPU-backed descriptor mapping. Of 40,932 walkers,
40,923 have a descriptor observation; all 40,923 reads succeed, with zero
unmapped reads. Nine walkers have no observed nonzero base/offset. These are
observations of programmed descriptors, not a count of kernels requiring
scratch. Two descriptor versions occur: the main one is observed 39,957 times
and the other, observed 966 times, is empty.

The main scratch surface is type 6, format 511, width 128, height 32, depth 1
(4,096 threads), slot-0 pitch 8,192, slot-1 pitch 1, GPU coherence 0, L1 WB 2
and MOCS 6, without compression or auxiliary surfaces. The pinned native v6
encoder instead uses IA coherence 1 and L1 WBP 0 with the same MOCS 6. This
distinguishes scratch-surface policy from the previously tested SBA L1 field.

Three isolated libraries reuse the same 80 Host objects and 1,242 mathematical
zebins. SBA, SLM MAX, current C0 dispatch, denorm mode, dependencies and model
policy remain unchanged. Each variant passes 15 Windows checks and six GPU
numerical groups; all 90 requests produce the same 76 tokens and 57/57 drafts.

| Scratch descriptor control | Decode tok/s | Change from native control | TTFT ms |
| --- | ---: | ---: | ---: |
| Native v6 bracket | 45.170 | — | 922.164 |
| GPU coherence only | 45.214 | +0.10% | 913.679 |
| L1 WB only | 46.937 | +3.91% | 895.952 |
| Both | 46.870 | +3.76% | 891.822 |
| L0 | 48.488 | — | 879.430 |

Native control drift is +0.23%. WB alone lowers TTFT 2.84% and leaves native
3.20% slower than L0. Coherence alone has no established decode gain, while
the combined variant is slightly slower than WB alone, so only WB proceeds
to the calibrated follow-up. These isolated sources remain historical,
unadopted experiments.
[Actual descriptors and isolated results](../orca-l0/demo/dg2-scratch-cache-windows-2026-11-02.json)
and 1,386 evidence files（本地归档：`../orca-l0/demo/dg2-scratch-cache-windows-2026-11-02.json.gz`）.

The follow-up enables scratch L1 WB only on the existing calibrated WDDM A770
path when `RevisionID == 8`. IA coherence and MOCS remain unchanged. Linux and
other steppings retain the prior fields. CPU checks cover opt-in and fallback,
64-byte through 256-KiB pitches, low and canonical high addresses and preserved
geometry. All 15 Windows tests, six GPU numerical groups and three Linux CPU
checks pass. The scratch Cvec check has maximum absolute error 1.1920929e-7;
the Q3_K private-memory check matches its analytic reference exactly.

| Clean C0-only repeat | Native v6 bracket | Adopted v7 | L0 |
| --- | ---: | ---: | ---: |
| Decode tok/s | 44.222 | 45.714 | 47.024 |
| TTFT ms | 943.649 | 927.864 | 916.614 |
| Prefill ms | 891.350 | 878.500 | 870.800 |

Decode improves **3.37%**, with **+0.31%** native before/after drift. TTFT falls
**1.67%** and prefill falls **1.44%**. Native remains **2.78% slower than L0**;
the improvement does not establish a native throughput win. All 60 requests
match 76 tokens, 57/57 drafts, no reuse, step/last min-p 0.5, cumulative floor
zero, MTP4, 5,771 slots, reserve 1,536 MiB and the same mathematical modules.
Each arm discards ten warmups and retains five samples. Decode uses clean
`decode_ms`, excluding preparation, initialization and prefill; clocks remain
unlocked. Absolute rates from different rounds are not combined into one
comparison.

The five measured core/test files are adopted as
`copy-batch-production-adoption-v7.json`; all 11 tracked source hashes and
retained medians/deltas are independently verified. Public headers, the 80
Host objects and copy/ownership paths are unchanged. The clean timing run
does not supply a new D2D or application-lock census. Linux CPU validation
does not establish Linux GPU performance or full-model correctness. Normal
executables, configuration, MTP files, root README and immutable Linux bundles
remain unchanged.
[Calibrated repeat and adopted sources](../orca-l0/demo/dg2-scratch-cache-c0-windows-2026-11-02.json)
and 133 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-scratch-cache-c0-windows-2026-11-02.json.gz`）.

### Fresh v7 counters reach the same copy and imported-lock level

The new payload probe builds a diagnostic library from the exact measured v7
stage and relinks the same frozen Host objects. It observes submitted work,
including graph replays. Its own counter mutex is confined to this payload
executable; the separate imported-lock probe contains no payload counters.
Neither diagnostic executable contributes to performance comparisons.

| Last warmed Short request | Native v7 | L0 reference |
| --- | ---: | ---: |
| D2D operations | 134 | 134 |
| D2D bytes | 4,554,752 | 4,554,752 |
| H2D operations | 659 | 659 |
| H2D bytes | 968,318,976 | 968,318,976 |
| Imported application mutex acquisitions | 130 | 130 |
| Shared / exclusive SRW acquisitions | 0 / 0 | 0 / 0 |
| Runtime mutex acquisitions | 0 | 0 |
| Foreground I/O acquisitions | 40 | 40 |
| Background I/O acquisitions | 90 | 90 |

Native transfer counts and both lock censuses are fresh. L0 payload uses the
earlier identical-fixture measurement; it is not relabeled as a fresh L0
payload capture. Native still emits 40,778 walkers and 136 submissions.
Explicit copy bytes are not physical PCIe/GPU traffic, including kernels'
direct reads of imported host weights.

Every observed mutex caller resolves against its executable's own link map
to PLE or DirectFile. Both backends have 20 `issue` and 20 `collect` calls,
15 keepalive reads and zero business SSD reads. Six acquisitions per keepalive
account for the remaining 90 calls exactly. Statistics are sampled outside
the observed boundaries, and keepalive stays enabled at 100 ms. The earlier
136/124 and 130/124 totals reflected differing numbers of background reads;
this round measures **130/130**. Timing-dependent background totals remain
variable, even with matched runtime and foreground synchronization.

These counts cover imported application mutex and Win32 SRW calls, including
the statically linked shared-mutex wrappers' SRW imports. They exclude the
observer's own recording guard and do not measure private/inlined/driver locks
or contention. The result establishes this measured scope rather than a claim
that all driver or system synchronization is absent.

All 33 diagnostic requests match 76 tokens, 57/57 drafts and natural stop,
using the same balanced policy and 1,242 mathematical zebins as the clean v7
repeat. Source checks additionally establish that the copy/fill function
bodies, owner/Queue headers, memory source and ABI are unchanged from the
previously validated v5 batching implementation. All 11 current source hashes,
instrumented sources, libraries, executables, outputs and evidence hashes are
verified. No model policy, production executable or configuration is changed.
[Fresh native payload and both lock censuses](../orca-l0/demo/dg2-v7-copy-lock-census-windows-2026-11-02.json)
and 97 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-v7-copy-lock-census-windows-2026-11-02.json.gz`）.

### Wider copy SIMD does not reproduce the initial Short gain

With the v7 runtime and its 80 Host objects fixed, offline memory modules change
only the SIMD width of the four copy/fill exports. The 1,242 mathematical
zebins, dispatch/cache fields, dependencies, ownership, expert budget and
step/last min-p 0.5 remain identical. All four exports report 128 GRFs at each
width. SIMD32 increases the linear-copy text from 2,112 to 3,584 bytes and the
pitched-copy text from 8,000 to 14,144 bytes; wider SIMD alone does not establish
more efficient execution.

The first native/SIMD8/SIMD32/native/L0 bracket discards ten warmups and retains
five samples per arm. SIMD8 changes decode -0.91%, SIMD32 +2.26%, against
-1.25% native control drift. TTFT increases 2.59% and 3.25%, with +3.36% drift.
These results motivate a larger SIMD32 repeat, rather than source adoption.

The repeat uses L0/native/SIMD32/native/L0, discarding ten warmups and retaining
15 samples per arm:

| Clean repeat | Native SIMD16 bracket | Native SIMD32 | L0 bracket |
| --- | ---: | ---: | ---: |
| Decode tok/s | 45.577 | 45.739 | 47.083 |
| TTFT ms | 933.958 | 935.975 | 917.121 |
| Prefill ms | 883.750 | 886.200 | 871.394 |

The apparent decode gain shrinks to **+0.35%**, against **+0.29%** native
control drift and **+1.12%** L0 drift. TTFT changes +0.22%. SIMD32 remains
**2.85% slower than L0**. This does not establish a stable gain or a native
throughput win; the existing SIMD16 memory source/module remains unchanged.
Absolute rates from the two rounds are not combined.

All **200** model requests match 76 tokens, 57/57 drafts and no reuse. Separate
checks cover 276 memory replays for each width, aligned/unaligned data, fills,
range/overlap failures, 72 copy replays and 21 pitched shapes per microbenchmark
arm, with full buffers and guards checked. Submit/wait microbenchmark times
have substantial control drift and are not physical bus bandwidth or isolated
GPU time. Clean model decode excludes preparation, initialization and prefill,
with internal diagnostics disabled; clocks remain unlocked. These timings add
no new copy/lock census. Production, v7 runtime source, configuration and Linux
packages remain unchanged; Linux GPU performance is unvalidated.
[Both brackets, module metadata and reproduction](../orca-l0/demo/dg2-copy-width-windows-2026-11-02.json)
and 120 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-copy-width-windows-2026-11-02.json.gz`）.

### Exact indirect payload length: valid, with no measured Short gain

Cached actual L0 walkers use 32-byte payload lengths such as 32, 96 and 160;
native rounds the length to 64 bytes. Pinned NEO programs the logical thread
data size into the walker separately from heap alignment. An isolated runtime
changes only the encoded length to 32-byte granularity. The original 64-byte
backing allocation, payload bytes, zero padding, capture cache and Host objects
are unchanged, along with all 1,242 mathematical zebins and v7 packet policies.

An independent eleven-case CPU check covers empty/full-inline payloads and
32/64/96/160/192-byte data, verifies unchanged backing bytes and padding, fails
the old encoder and passes the candidate. Existing SIMD1 and software local-ID
checks also cover the logical length separately from backing padding. The
candidate passes **15 Windows tests**, six real GPU numerical groups and three
Linux CPU checks. Linux GPU execution remains unvalidated.

| Clean Short | Native v7 bracket | Exact-length candidate | L0 control |
| --- | ---: | ---: | ---: |
| Decode tok/s | 45.552 | 45.437 | 47.421 |
| TTFT ms | 925.547 | 931.920 | 906.960 |
| Prefill ms | 876.972 | 882.550 | 862.000 |

All **80** requests match 76 tokens, 57/57 drafts, natural stop and no reuse,
with ten warmups and ten retained requests per arm. Step/last min-p stay 0.5,
cumulative floor zero, MTP4, 5,771 slots and reserve 1,536 MiB. Decode excludes
preparation, initialization and prefill; inner diagnostic counters and GPU
timestamps are disabled. Clocks remain unlocked.

Decode changes **-0.25%**, against **-0.31%** native control drift; TTFT changes
**+0.69%**. This establishes no performance improvement. The unchanged v7
control remains **3.94% slower than L0** in this round; the candidate is 4.19%
slower. These absolute rates are not combined with earlier rounds. The trial
adds no new copy/lock census or physical traffic measurement. It remains
isolated; production/runtime source and Linux handoff packages are unchanged.
[Results, source boundary and reproduction](../orca-l0/demo/dg2-payload-length-windows-2026-11-02.json)
and 115 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-payload-length-windows-2026-11-02.json.gz`）.

### Cached register writes and the limit of kernel-binary export

A CPU-only extension of the typed decoder revisits the earlier **v5** native/L0
capture. All packet counts, capture hashes and earlier decoded fields match.
Native executes **2,894** `MI_LOAD_REGISTER_IMM` writes of `0x60002` to `0x2580`,
with MMIO remapping enabled. L0 executes no LRI in that captured Decode range.
This does not establish L0's retained preemption state from initialization,
and it is not a fresh v7 census or performance measurement. Earlier partial
preamble trials already tested suppressing repeated state writes without an
established gain; the count alone does not justify adopting that policy.

The pinned NEO reference adds a limit on the next ISA comparison:
`KernelImp::getKernelProgramBinary` copies the CPU-side `pKernelHeap`, whereas
`ModuleImp::getKernelHeapPointerAndSize` can select separate patched segments
for upload. Equality from kernel-binary export alone cannot prove equality of
the final resident GPU instructions. These source files and their hashes are
cached, including one missing-file response; no repeat downloads are needed.
The pinned reference does not prove the installed driver's implementation.
Actual resident-ISA readback and initialization preemption state remain open.
No new GPU/model request, runtime change or speed claim comes from this pass.
[Register census, reference boundary and reproduction](../orca-l0/demo/dg2-register-fields-windows-2026-11-03.json)
and 20 verified evidence files（本地归档：`../orca-l0/demo/dg2-register-fields-windows-2026-11-03.json.gz`）.

### Loaded resident instructions match across native DG2 and L0

A separate A770 C0 / Windows 32.0.101.9034 diagnostic reads back all **1,892
code sections / 77,438,144 bytes** from the shared **1,242-module** native pack.
Every byte matches between native DG2, L0 and the original ELF text sections.
This closes the loaded-code comparison for this pack; CPU-side binary export
alone was not the evidence used.

Native adds a private readback immediately after its unchanged production
upload and uses the existing staging transfer. L0 obtains loaded code addresses
through `zeModuleGetFunctionPointer` and reads them with the existing compiled
`orca_copy` kernel. A standard multi-indirect target with a **zero launch
count** keeps each target ISA in the same command list's residency set. L0
records 1,892 predicated target dispatches but requests no mathematical work;
the 1,892 reader dispatches perform the copies. Full output guards and unchanged
count/group controls are checked on every successful read.

The initial ordinary-copy probe terminates with process access violation
`0xC0000005`. A copy-only GPU reader passes 14 sections before reporting
`ZE_RESULT_ERROR_DEVICE_LOST` at the first large isolated code allocation.
Adding the zero-count residency target passes that case, a 15-section
calibration and the complete pack. All failed attempts are retained; this
calibration does not establish the failed GPU access's fault address or prove
a production runtime defect. All diagnostic processes have exited.

An independent ELF parser verifies both complete readback sequences, all
module hashes, current v7 source hashes and **3,970 evidence files**, including
the actual code dumps. This is standalone load/residency validation, not a new
live-model capture or performance measurement. No inference requests, copy/lock
census or Linux GPU tests are added. Production runtime source, binaries,
configuration and model policy stay unchanged. The v7 adoption repeat remains
2.78% below L0; the later exact-length trial's unchanged v7 control is 3.94%
below L0. This readback adds no timing result. Packet/cache state, memory
traffic and scheduling still need investigation.
[Resident-code results and reproduction](../orca-l0/demo/dg2-resident-isa-windows-2026-11-03.json)
and complete readbacks, sources and failed attempts（本地归档：`../orca-l0/demo/dg2-resident-isa-windows-2026-11-03.json.gz`）.

### Initial preemption matches; Windows device/context timeout flags differ

Two separate startup captures begin before runtime construction and each check
three exact 4,097-byte GPU copies per backend, including full output guards.
Both complete captures have zero packet-capture failures. L0 writes preemption
register `0x2580 = 0x60002` once; native v7 writes that same value on each of
three replays. Both enable MMIO remapping. The earlier absence of L0 register
writes inside Decode did not establish a different initial setting.

The installed L0 path creates its device and all three contexts with flags
`0x4`, whereas native v7 uses `0`. The Windows SDK identifies `0x4` as
`DisableGpuTimeout` for both device and context creation. L0 uses two compute
contexts on node 5 and a copy context on node 2; native uses two compute
contexts on node 5. All five 144-byte private context metadata blocks match
after masking bytes 0–7 (pointer) and 12–15 (process ID). The full-matching
candidate's two masked blocks also match. These observations do not establish
effective hardware preemption or a desktop fix.

Two isolated Windows-only trials change `src/wddm/runtime.cpp`: first context
creation alone, then both device and context creation. Each passes 15 CTests,
six real GPU numerical groups and a separate three-copy startup calibration.
Each clean timing round uses L0 / native / candidate / native / L0, ten
warmups and ten retained requests per process. All **200 requests** match the
complete natural76 output, stop condition and 57/57 drafts, with no prefix
reuse. Both reuse the same 80 frozen Host objects and 1,242 numerical zebins;
min-p remains .5, MTP 4, slots 5,771 and requested reserve 1,536 MiB.

| Separate Short round | Native bracket tok/s | Candidate tok/s | L0 bracket tok/s | Candidate change | Native / L0 drift |
| --- | ---: | ---: | ---: | ---: | ---: |
| Context flag only | 46.734 | 46.573 | 48.472 | -0.34% | -0.30% / -0.07% |
| Device and context flags | 46.002 | 46.404 | 47.850 | +0.87% | +0.78% / +2.69% |

Brackets use the geometric mean of before/after process medians. TTFT changes
**-0.03% / -1.85%** respectively. The full-matching candidate remains **3.02%
below L0** in its round. The modest positive observation needs repetition;
especially with 2.69% L0 drift, it does not establish a stable gain. Neither
trial is adopted. These absolute rates are not combined with previous rounds.

Clean decode excludes setup, initialization, preparation and prefill; TTFT
is separate. Observer hooks, GPU timestamps and inner diagnostic counters are
absent from timing executables. Clocks remain unlocked. The earlier v2 startup
probe passed its numerical copies but logged 12 capture failures because its
decoder lacked `MI_FLUSH_DW`; the corrected captures and the failed attempt
are all preserved. An earlier library-path preflight ran no GPU work.

An independent verifier checks source/module/library/executable hashes,
masked metadata, every request, medians, geometric controls and ratios across
**369 evidence files**. No new copy/lock census or physical traffic measurement
is added. Current v7 source, production executables/configuration, MTP policy
and immutable Linux handoffs remain unchanged; Linux GPU tests are pending.
[Startup/flag results and reproduction](../orca-l0/demo/dg2-initial-state-windows-2026-11-03.json)
and captures, exact sources, model logs and failed attempts（本地归档：`../orca-l0/demo/dg2-initial-state-windows-2026-11-03.json.gz`）.

### Reversing the timeout-flag bracket does not confirm a stable gain

A second **100-request** clean Short round uses the exact same three binaries,
with no recompilation. It reverses the bracket nesting to native / L0 /
candidate / L0 / native. Each process again discards ten warmups and retains
ten requests; all complete outputs, stop conditions and 57/57 drafts match.
The functional checks of these unchanged binaries are reused, not rerun.

| Device + context flags | First round | Reversed repeat |
| --- | ---: | ---: |
| Native bracket tok/s | 46.002 | 46.582 |
| Candidate tok/s | 46.404 | 46.719 |
| L0 bracket tok/s | 47.850 | 48.727 |
| Candidate change against native | +0.87% | +0.29% |
| Native control drift | +0.78% | +0.75% |
| L0 control drift | +2.69% | -0.15% |
| TTFT change | -1.85% | -0.50% |

The repeat's candidate remains **4.12% slower than L0**. Its 0.29% observation
is smaller than native control drift, so matching these creation flags still
establishes no stable gain and is not adopted. This result does not demonstrate
that creation flags have zero effect. Current v7 runtime/production settings,
model/MTP policy and Linux handoffs remain unchanged. Both rounds exclude
preparation, initialization and prefill from decode and have no inner counters,
GPU timestamps or observer hooks. Clocks remain unlocked.
[Unchanged-binary repeat and independent calculations](../orca-l0/demo/dg2-timeout-repeat-windows-2026-11-03.json)
and 30 verified evidence files, including the original capture archive（本地归档：`../orca-l0/demo/dg2-timeout-repeat-windows-2026-11-03.json.gz`）.

### Loaded frequency samples match across L0 and native v7

A separate read-only Sysman process observes L0 / native v7 / L0 while each
model process completes ten warmups and ten retained Short requests. All
**60 outputs and 57/57 drafts match**. The model binaries and all 1,242
mathematical zebins are unchanged. No clock or power limit is set.

Samples are selected only when their complete query falls inside a retained
request's host first-to-last token dequeue window:

| Driver-reported frequency | L0 before | Native v7 | L0 after |
| --- | ---: | ---: | ---: |
| GPU-domain samples | 136 | 146 | 139 |
| GPU actual min / median / max, MHz | 2400 / 2400 / 2400 | 2400 / 2400 / 2400 | 2400 / 2400 / 2400 |
| Memory-domain samples | 136 | 146 | 139 |
| Memory actual min / median / max, MHz | 16000 / 16000 / 16000 | 16000 / 16000 / 16000 | 16000 / 16000 / 16000 |
| Samples with nonzero throttle-reason flags | 0 | 0 | 0 |

The nominal 100-ms sampling interval measures **109.92 / 108.84 / 110.22 ms**
at the median using QPC. GPU-state query medians are **60.65 / 78.05 / 62.80
microseconds**. Both paths report the same requested frequency and TDP-limited
frequency as well. These observations provide no sampled clock difference to
explain the throughput gap; brief changes between queries remain unobserved.

This is diagnostic evidence, not another clean speed result: the observer can
perturb execution, and host delivery windows include message/scheduling delay
and omit the first token. It does not measure utilization or physical traffic.
The initial Python availability probe used the wrong boolean width for frequency
properties; that properties result is rejected. A C++ SDK check verifies the
correct 8-bit `ze_bool_t`, structure sizes and offsets. The final sampler uses
SDK structures directly and verifies vendor/device 0x8086/0x56a0. The separate
initial sampler compile failure ran no model requests. Both failed attempts
are retained among **54 verified evidence files**. Runtime/production source,
configuration, MTP policy and Linux handoffs remain unchanged.
[Loaded-frequency observations and scope](../orca-l0/demo/dg2-loaded-frequency-windows-2026-11-03.json)
and SDK checks, exact sampler, raw QPC samples and model logs（本地归档：`../orca-l0/demo/dg2-loaded-frequency-windows-2026-11-03.json.gz`）.

### Installed Windows driver does not expose Sysman PCI byte counters

A standalone read-only SDK probe identifies the same A770 and successfully
queries PCI properties. It reports **no bandwidth, packet or replay counters**.
`zesDevicePciGetStats` returns **0x78000003**,
`ZE_RESULT_ERROR_UNSUPPORTED_FEATURE`. This establishes unavailable counters,
not zero PCIe traffic. No model request or performance measurement is added.
Actual bus traffic, including imported-host kernel reads, remains unmeasured.
The response, source, commands and hashes are cached for this device/driver;
repeating the same capability query is unnecessary until that boundary changes.
Linux counter availability is not tested.
[Cached availability result](../orca-l0/demo/dg2-pci-counter-availability-windows-2026-11-03.json)
and 11 verified evidence files（本地归档：`../orca-l0/demo/dg2-pci-counter-availability-windows-2026-11-03.json.gz`）.

### Native adapter CPU-copy spans are small in the fixed Short request

A separate diagnostic rebuilds all 80 frozen Host objects, replacing only four
adapter `memcpy` sites with counting callbacks. It links the unchanged adopted
v7 runtime and uses the same 1,242 mathematical zebins. All **15 Short requests**
match the full 76 output tokens, natural stop and 57/57 accepted/offered drafts,
with no prompt reuse. Counters cover the original internal Decode boundaries,
excluding preparation, initialization and prefill.

After ten initial requests, the last five report the following per request:

| Adapter CPU-copy site | Calls | Bytes | Median summed copy spans |
| --- | ---: | ---: | ---: |
| Producer snapshot | 19 | 1,867,776 | 301.4 us |
| Owner upload staging | 19 | 1,867,776 | 339.8 us |
| Recorded upload refresh | 0 | 0 | 0 us |
| Staged readback publication | 0 | 0 | 0 us |

Each nonzero copy is **96 KiB**. The median of each request's combined spans is
**645.7 us**, ranging **622.4–672.5 us**, for **3,735,552 copied bytes** per
76-token request. Combined median is computed from per-request sums, rather
than adding the two site medians. One of the first ten requests has 18 copies
at each upload site; the other fourteen have 19. All records are retained.
Across all 15, combined spans range **562.9–704.4 us**.

The observed `memcpy` spans are small relative to the previously measured
Decode duration; they provide no evidence of a multi-percent copy-time saving.
This does not bound cache effects or surrounding `cpu_access`/ownership costs.
QPC spans include measurement overhead, exclude counter updates and surrounding
`cpu_access`, and can overlap across threads. Out-of-line callbacks can change
inlining; these diagnostic model timings establish no throughput improvement.
The concurrency/guard self-check passes. No L0 driver-internal CPU-copy census
or physical-traffic measurement is claimed. Zero counts at two sites describe
this fixture, not an absence of GPU readbacks or all runtime CPU copies.
Current D2D/application-lock parity remains the separately verified **134/134**
and **130/130** result. Runtime source, production executable/configuration,
model/MTP policy and Linux handoffs stay unchanged.
[All copy records, scope and reproduction](../orca-l0/demo/dg2-cpu-staging-windows-2026-11-03.json)
and 293 verified evidence files（本地归档：`../orca-l0/demo/dg2-cpu-staging-windows-2026-11-03.json.gz`）
preserve the staged headers, helper, compiler commands, original/generated main
and complete model outputs.

### Owner handoff and CPU-access wall spans

A separate v7 diagnostic timestamps each synchronous owner call at publication,
owner entry, owner exit and caller resumption. Release/acquire completion
publishes these diagnostic fields before the caller reads them. Four additional
runtime scopes measure CPU access, submission, completion collection and
immutable completion waiting. All 80 frozen Host objects and the staged runtime
are rebuilt; mathematical modules and numerical/model policy stay fixed.

All **15 Short requests** match the full 76 tokens, natural stop, 57/57 drafts
and zero reuse. After ten initial requests, the last five give these per-request
medians:

| Measured wall-clock span | Calls | Summed spans, ms |
| --- | ---: | ---: |
| Publish to owner entry | 136 | 4.8125 |
| Owner body | 136 | 10.6938 |
| Owner exit to caller resumption | 136 | 2.8290 |
| `Runtime::cpu_access` | 19 | 0.0278 |
| `Runtime::submit_recording` | 136 | 3.3490 |
| `Runtime::completed_queue` | 136 | 0.0347 |
| `NativeCompletion::wait` | 135 | 1579.6079 |

The first three components partition each measured handoff. The median of
per-request **queue plus return** totals is **7.6685 ms**, with a
**7.5198–7.9157 ms** range; the full publish-to-resume total is **18.5218 ms**,
ranging **17.9063–18.7838 ms**. These combined medians are computed from actual
per-request sums. Runtime submission/access/collection scopes are nested in
owner bodies and must not be added to them. All 15 records, including the
initial completion-wait range up to 3742.6462 ms, remain available.

CPU access and owner queue/return spans are small relative to completion waits.
This narrows the next investigation to GPU execution and completion delivery;
it does not identify pure GPU compute time or isolate notification latency.
Wait spans include GPU work, OS scheduling, cached early returns and diagnostic
overhead. They are wall time, not CPU use. Handoff sums can overlap across
producers, and source probes can change inlining and scheduling; no clean
throughput improvement or removable critical-path bound is inferred.

The seven-counter concurrency/reset self-check passes. The existing
eight-producer executor check uses the **exact instrumented class** and passes
**32,332 handoffs**, nested calls, exceptions, reference/move-only returns and
four teardowns. All source, module, library/executable and raw protocol hashes
are independently checked. Only timing fields/scopes are added to staged
sources. Production runtime/adapter, executable/configuration, MTP policy and
Linux handoffs remain unchanged; native still has no established throughput
win. Earlier partial-state and GRF-only packet trials retain their measured
results and should not be repeated unchanged merely because host costs are now
better resolved.
[Owner/runtime records and scope](../orca-l0/demo/dg2-host-cost-windows-2026-11-03.json)
and 345 verified evidence files（本地归档：`../orca-l0/demo/dg2-host-cost-windows-2026-11-03.json.gz`）
include the exact class, helper, runtime modifications, concurrent check,
compiler/link commands, all outputs and per-request components.

### GPU tail marker to completion-wait exit

A separate diagnostic changes the existing final `PIPE_CONTROL` to a timestamp
post-sync, preserving its flush bits and packet count. Each closed list has a
guarded host-backed marker. A matching queue sequence publishes the marker to
the completion observer; stale or missing matches are reported explicitly.
Only the frozen main Host object is rebuilt for request hooks; the other 79
objects are reused. Production v7 sources and all 1,242 mathematical zebins
remain unchanged.

Native `D3DKMTQueryClockCalibration` supplies GPU/QPC pairs on compute node 5.
Eight gated replays across two queues validate projected tail times after host
gate signals and before host resumption, with marker overwrite and full guard
checks. The GPU counter has 36 valid bits. Its measured rate is about
117–119 ppm above the nominal 19.2 MHz, so conversion fits an affine map from
each request's before/after calibration pairs rather than using the nominal
rate for absolute timestamps. Query start/end windows give projected lower
and upper estimates under that model; they are not confidence intervals or
guaranteed hardware accuracy. Signed-wrap and synthetic projection checks pass.
The initial adapter-discovery failure is retained; corrected discovery uses
the runtime's hardware filter. This capability result is cached.

All **15 Short requests** match the complete 76 tokens, natural stop, 57/57
drafts and zero reuse, under the same balanced policy and expert budget.
All **2,023 waits** have valid matching markers and intact guards. Request 6
has 133 waits; the other fourteen have 135. Decode excludes preparation,
initialization and prefill. After ten initial requests, the last five give:

| Per 76-token request | Median | Observed range |
| --- | ---: | ---: |
| Total completion-wait spans | 1604.7809 ms | 1594.4641–1616.6185 ms |
| Wait portion after GPU tail, point estimate | 8.08335 ms | 7.75568–14.81612 ms |
| Before-request calibration query | 20.9 us | 19.5–21.4 us |
| After-request calibration query | 19.7 us | 19.3–20.6 us |

The after-tail lower/upper estimates have medians **7.54575 / 9.43948 ms**.
These are separate medians, not an interval around the median request.
Request 11's higher **14.81612 ms** point estimate is retained. Each wait
contributes `clip(wait_end - projected_tail, 0, wait_end - wait_start)`, excluding
caller work between an old tail marker and entering the wait. Summing the full
tail-to-exit gap would overcount cached early returns.

The after-tail portion includes the GPU/driver fence epilogue, interrupt
delivery, OS scheduling and observation overhead. It does not isolate pure
notification latency or CPU use. Wait sums can overlap and do not bound
removable critical-path time. There is no L0 completion-delivery comparison,
and the diagnostic allocations, writes and logging prevent a clean speed
claim. This evidence narrows attention toward work and scheduling before the
tail; it does not establish pure GPU compute time or explain the whole gap.
Production executable/configuration, MTP policy and Linux handoffs remain
unchanged. Native still has no established throughput win.
[All records, clock model and limits](../orca-l0/demo/dg2-completion-delivery-windows-2026-11-03.json)
and 100 verified evidence files（本地归档：`../orca-l0/demo/dg2-completion-delivery-windows-2026-11-03.json.gz`）
preserve both discovery attempts, staged sources, guards, calibration checks,
compiler/link commands and complete model outputs.

### Compiler launch state: a correctness fix, no established speedup

The remaining IDD denorm-field difference has a concrete compiler consequence.
The installed L0 capture uses **FTZ (0)**, whereas the previous native encoder
uses **SETBYKERNEL (1)**. IGC's reference `CEncoder::initCR` assumes denorm bits
start clear and uses OR instructions to enable the retention modes a kernel
requests. A launch that starts with FP32 retention enabled can therefore keep
retention even when the compiled kernel requests flushing. FTZ here describes
the launch state; the kernel can enable denorm retention in its body.

A read-only disassembly census uses the installed driver's IGA library on the
frozen collection: **1,242 zebins / 1,892 text sections / 77,438,144 ISA bytes**.
Every section has an initial retention setup OR after its selected entry:
1,882 use hexadecimal `0x4C0` and ten use its decimal equivalent 1216. Later
rounding/mode changes are retained in the records. Build options request
correctly rounded FP32 divide/sqrt and contain no denorm-flush option. Reference
IGC/NEO sources are content-hashed context, not claimed identical to the
installed compiler; the disassembly and GPU probes use the installed driver.

The same standalone FP32/FP16 probe is compiled with and without
`-cl-denorms-are-zero`, then run through L0, old native and a single-field FTZ
candidate. Each run checks 768 arithmetic values, three overwritten replays
per precision, signed normal/subnormal boundaries and full input/output guards.
The old native path matches the retaining module but differs from L0 on
**60 FP32 rows** of the explicit-flush module. FP16 has no old/native mismatch.
The candidate matches L0 bit for bit in both modules. The compilation option
also changes FP16 results; the historical `flush32` label names the fixture,
not an FP16 exclusion. This is a compiler-contract bug, not evidence of wrong
current Qwen output or a broad PPL result.

The clean L0/native/candidate/native/L0 Short bracket discards ten warmups and
retains ten requests per process. All **100 requests** match all 76 tokens,
natural stop, 57/57 drafts and zero reuse, with 5,771 slots, 1,536-MiB reserve,
step/last min-p 0.5, cumulative floor zero and MTP4. Mathematical zebins, copies,
cache policy, ownership and all other packet fields remain fixed.

| Clean Short measurement | Native control | FTZ launch | L0 control |
| --- | ---: | ---: | ---: |
| Decode tok/s | 47.049 | 47.130 | 48.628 |
| TTFT ms | 899.146 | 898.338 | 882.228 |

Native control drift is **+1.08%** and L0 drift **+0.12%**. The observed decode
change of **+0.17%** and TTFT change of **-0.09%** establish no stable speedup;
native remains **3.08% below L0**. Decode excludes preparation, initialization
and prefill. No GPU timestamps, inner timing probes or frequency observers are
enabled in this model comparison.
[Trial, numerical scope and independent checks](../orca-l0/demo/dg2-denorm-contract-windows-2026-11-03.json)
and 178 verified evidence files（本地归档：`../orca-l0/demo/dg2-denorm-contract-windows-2026-11-03.json.gz`）.

Source adoption **v8** changes the launch field, its packet assertion and a
small permanent regression. `tests/denorm.cl`, baked once retaining and once
flushing, tests 192 signed normal/subnormal/underflow/zero products per mode
across two queues and six overwritten replays with full guards. The old
runtime/header control passes retention and fails explicit flushing; the fixed
runtime passes both. Windows 15 checks and Linux three CPU checks pass. The
runtime encoding matches the model-tested candidate except explanatory
comments; six numerical groups and the 100 model requests retain their trial
evidence. No Linux GPU result is claimed.

Two rejected test attempts are preserved. The first used default-queue submit
for a list bound to the second queue. The second mixed a checker containing new
inline packet definitions with an old library, invalidating the old control.
The final control compiles the old runtime and old headers together and passes
its old-state CPU check before demonstrating the expected GPU failure. The
earlier public-API probes and model binaries do not mix these internal headers.

After baking the two regression modules with the archived compiler commands:

```text
orca-l0-dg2-check --gpu-denorm retain.zebin retain
orca-l0-dg2-check --gpu-denorm flush.zebin flush
```

The fix is adopted for compiler correctness. Production executables/configuration,
MTP policy and immutable Linux handoffs stay unchanged; native still has no
established throughput win. The previous D2D/lock census remains separately
scoped to v7; the copy/owner code is unchanged.
[Adopted source and regression](../orca-l0/demo/dg2-denorm-fix-windows-2026-11-03.json)
and 254 verified evidence files（本地归档：`../orca-l0/demo/dg2-denorm-fix-windows-2026-11-03.json.gz`）
include both rejected attempts, consistent controls, exact sources and commands.

### Working-set and mixed-kernel controls; repeated preemption is not the cause

The previous real-GDN diagnostic reused one input/state region. A new factorial
holds 16 SIMD16/Q8-tail GDN launches fixed while varying host/local placement,
T=1/4, one/16 independent data slots, and presence of a small `advance` kernel
after every GDN. Two fixture phases overwrite all inputs; slot seeds also
differ. The GDN zebin remains the current 256-GRF, 3,136-byte-SLM module with
no declared scratch. The added `advance` module uses 128 GRFs and SIMD32.
It changes kernel/GRF/SLM/ISA state and adds real work and barriers, so its
effect cannot be attributed solely to register-mode switching.

| Local-data GPU interval, 16 GDN launches | L0 µs | Native v8 µs |
| --- | ---: | ---: |
| T=1, one slot, GDN only | 161.185 | 159.180 |
| T=1, 16 slots, GDN only | 330.228 | 345.833 |
| T=1, one slot, alternating advance | 190.951 | 373.815 |
| T=1, 16 slots, alternating advance | 361.992 | 534.023 |
| T=4, one slot, GDN only | 311.894 | 308.542 |
| T=4, 16 slots, GDN only | 505.827 | 520.833 |
| T=4, one slot, alternating advance | 339.121 | 528.112 |
| T=4, 16 slots, alternating advance | 557.930 | 759.401 |

Requested working sets grow from 3,276,800 to 52,428,800 bytes at T=1 and
3,538,944 to 56,623,104 bytes at T=4. Large unmixed local-data cases are
4.73%/2.97% longer natively. The small mixed T=1 case is instead **95.77%
longer**, with L0 control drift -0.05%. Host placement also exposes a mixed
small-workset penalty, while its multi-millisecond large-workset cost hides
much of that relative difference. These observations separate two useful
directions: workset behavior and mixed-kernel state/cache processing. They do
not apportion the production-model gap or measure physical memory traffic.

An isolated staged candidate tests only the repeated preemption write:
`MI_LOAD_REGISTER_IMM`, register `0x2580`, value `0x60002`, MMIO remap enabled.
On the existing calibrated C0 profile, a valid recorded list keeps that
constant mode across kernel/heap changes. Fresh/invalid lists, untracked
recording, profile transitions and conservative fallback retain initialization.
All barriers, invalidations, pipeline/SCM/CFE/double-SBA fields, dispatch policy,
FTZ state, ISA and ownership remain unchanged. A packet regression checks
fresh, repeated and returning GRF transitions, invalidation of tracked state,
untracked recording and profile transitions; the source verifier confirms
that the runtime edit changes only this LRI emission.

The five-arm L0/native/candidate/native/L0 bracket relinks the exact previous
public-API probe object with the candidate library. It reproduces small mixed
T=1 GPU times of **190.739 / 374.192 / 375.026 µs** for L0/native/candidate.
Candidate time is **+0.22%**, native drift +0.02%; the repeated write does not
explain the large gap. At T=4 the corresponding times are
341.549 / 536.331 / 537.682 µs. The candidate is **not adopted**, and no
full-model trial is justified by this result. Earlier broad GRF-only
suppression results retain their separate negative evidence.

Across the baseline and candidate brackets, all **256 case runs**, **7,680
retained samples**, 2,560 warmups and 74,552 conditioning replays pass. FP32
has an independent scalar reference, maximum error **7.749e-7**. Packed Q8 has
finite checks and cross-control byte parity, rather than an independent CPU
quantization oracle. All eight T/slot/phase output fixtures match bit for bit
across backends, mixed/unmixed and host/local. Guarded counters validate every
extra advance launch, including conditioning and warmups; full state/input,
padding and timestamp guards are checked. Candidate Windows 15 tests, six GPU
numerical groups and both denorm regressions pass. No Linux GPU claim follows.

Setup, transfers, initialization and validation are outside the timed replay.
GPU intervals use the calibrated 19.2-MHz/36-bit CS clock and contain the fixed
timestamp commands; submit-and-wait wall intervals remain separate. Summaries
are medians of the two fixture-phase medians, with geometric bracket controls
computed within each phase. Clocks remain unlocked.

Source remains v8. Production executables/configuration and immutable Linux
handoffs are unchanged. The **134/134 D2D operations, 4,554,752 bytes and
130/130 imported mutex acquisitions** remain the separate v7 census with
unchanged copy/owner source; they were not recounted here. No end-to-end native
throughput win is established.
[Independent raw-sample checks and exact sources](../orca-l0/demo/dg2-gdn-worksets-windows-2026-11-03.json)
and 421 files, 108 content-addressed evidence blobs（本地归档：`../orca-l0/demo/dg2-gdn-worksets-windows-2026-11-03.json.gz`）
include original numerical dumps, module metadata, hashes and all commands.

### Separating instruction invalidation from redundant SBA programming

Two new staged v8 variants retain the frozen working-set probe object,
mathematical modules, operands and all numerical/guard checks. Each is limited
to pure GRF changes within a valid list on the calibrated C0 profile. New or
invalid lists, untracked recording, profile changes, changed heaps/MOCS and
conservative fallback retain the full original programming.

The first clears only the leading `PIPE_CONTROL` instruction-cache
invalidation bit. State/constant/texture invalidations, stall/HDC/untyped/DC
synchronization and every LRI, pipeline, SCM, CFE and double-SBA packet remain.
Its packet regression checks that every other synchronization bit matches the
original. This directly isolates a bit that the earlier broader narrow-GRF
trial changed together with several packets.

The second omits only the two identical `STATE_BASE_ADDRESS` packets on those
pure GRF changes. It retains the complete leading invalidation and every
other packet. Its packet regression compares the encoded stream against the
original with exactly those two SBA packets removed and the required SCM GRF
bit changed. Coupled MOCS/heap changes and fresh/fallback cases keep both SBA
packets. Neither trial changes the SBA cache policy or scratch-surface policy.

| Local small-workset mixed interval | Native control µs | Candidate µs | GPU time change |
| --- | ---: | ---: | ---: |
| T=1, clear instruction invalidation only | 374.271 | 373.659 | -0.16% |
| T=4, clear instruction invalidation only | 534.176 | 535.742 | +0.29% |
| T=1, omit repeated double-SBA only | 374.199 | 337.799 | -9.73% |
| T=4, omit repeated double-SBA only | 532.028 | 495.885 | -6.79% |

Both use L0/native/candidate/native/L0 brackets and medians of two fixture
phase medians. In the SBA T=1/T=4 rows, native control drifts -0.11%/-0.23%.
Large local mixed worksets improve 7.13%/2.96%, while unmixed cases have little
consistent change. Small host-backed mixed worksets also improve 5.51%/4.50%.
The isolated address-state programming has a measurable cost; clearing only
instruction invalidation does not explain the large mixed-kernel gap. The
remaining SBA-candidate T=1 time is still much longer than its L0 control,
**192.276 µs**. These are GPU intervals, not end-to-end decode measurements.

Across these two trials and the frozen baseline, **416 case runs**, **12,480
retained samples** and 4,160 warmups pass, with maximum scalar-reference error
**7.749e-7**. All eight FP32/packed-Q8 T/slot/phase fixtures match byte for byte
across controls, host/local and mixed/unmixed cases. Full input/state/padding,
timestamp and advance-counter checks remain. Each candidate passes Windows
15 tests, six numerical GPU groups and both denorm regressions. Archived source
comparisons independently confirm the exact runtime edits.
[Component results and limits](../orca-l0/demo/dg2-transition-parts-windows-2026-11-03.json)
and 680 evidence files, 142 unique SHA256 blobs（本地归档：`../orca-l0/demo/dg2-transition-parts-windows-2026-11-03.json.gz`）.

Because the SBA variant improves an actual mixed-kernel workload, it proceeds
to a clean full-model Short bracket. The same 80 frozen Host objects are
relinked with its library; 1,242 math zebins, v8 launch/dispatch/cache state,
ownership, copies and policy remain fixed. All 100 requests match complete
76-token natural output, 57/57 drafts, stop and zero reuse, with 5,771 slots,
1,536-MiB reserve, MTP4, step/last min-p 0.5 and cumulative floor zero. Each
process discards ten warmups and retains ten requests. No GPU timestamp,
inner timing probe or telemetry is enabled. Decode excludes preparation,
initialization and prefill; TTFT remains separate.

| Full Short measurement | Native control | SBA candidate | L0 control |
| --- | ---: | ---: | ---: |
| Decode tok/s | 46.386 | 46.254 | 47.880 |
| TTFT ms | 909.710 | 908.492 | 893.932 |
| Whole request ms | 2501.145 | 2505.929 | 2436.160 |

Observed decode changes **-0.28%**, with native control drift **+0.16%** and L0
drift **-0.70%**. TTFT changes -0.13%, versus native drift -0.19%. This does
not establish a model speedup or a stable regression. Native remains 3.40%
below L0 in this bracket. Linux compilation and three CPU tests pass; Linux
GPU testing remains deferred. The local microbenchmark benefit is too narrow
to establish the cause of the full-model gap.

Neither candidate is adopted. Root source remains v8; production executable,
configuration, concurrent MTP work and immutable Linux handoffs are unchanged.
The separate 134/134 D2D and 130/130 imported-lock census has not been rerun.
[Short parameters and independently checked raw protocols](../orca-l0/demo/dg2-sba-transition-short-windows-2026-11-03.json)
and 172 evidence files（本地归档：`../orca-l0/demo/dg2-sba-transition-short-windows-2026-11-03.json.gz`）.

A reversed SBA/native/SBA standalone bracket reuses exactly the same binaries
without recompilation. Local small mixed T=1 gives **372.878 / 335.937 µs**
for native/candidate, **-9.91%**, with candidate control drift -0.02%. T=4 gives
**526.849 / 491.543 µs**, **-6.70%**, with drift -0.03%. All 96 case runs,
2,880 retained samples and 960 warmups pass scalar-reference, full guards and
advance-counter checks; output bytes match the previous eight fixtures.
This confirms a repeatable local SBA effect. It does not overturn the clean
Short result or justify adopting the candidate as a model-throughput fix.
[Reversed probe and independent raw-sample verification](../orca-l0/demo/dg2-sba-transition-repeat-windows-2026-11-03.json)
and 111 evidence files（本地归档：`../orca-l0/demo/dg2-sba-transition-repeat-windows-2026-11-03.json.gz`）.

### Remaining GRF-transition components: isolated controls

Four further staged v8 variants use the same frozen public-API workset probe
object and mathematical zebins. Each changes only pure C0 GRF transitions;
fresh, invalid, untracked, coupled and fallback states keep full programming.
Removing CFE or pipeline selection separately has almost no effect. Clearing
state/constant/texture invalidations, or all four invalidations including
instruction cache, still leaves most of the mixed-kernel gap:

| Local small mixed interval | Native control µs | Candidate µs | GPU time change |
| --- | ---: | ---: | ---: |
| T=1, omit CFE only | 374.036 | 373.906 | -0.03% |
| T=4, omit CFE only | 527.077 | 527.357 | +0.05% |
| T=1, omit pipeline selection only | 373.926 | 373.698 | -0.06% |
| T=4, omit pipeline selection only | 527.044 | 527.878 | +0.16% |
| T=1, clear state/constant/texture invalidations only | 373.782 | 370.938 | -0.76% |
| T=4, clear state/constant/texture invalidations only | 527.044 | 524.779 | -0.43% |
| T=1, clear all four invalidations only | 373.763 | 363.411 | -2.77% |
| T=4, clear all four invalidations only | 526.608 | 517.734 | -1.69% |

Adjacent native controls and common outer L0 controls bracket each variant;
values are medians of two fixture-phase medians. Native drift stays within
0.25% for these rows. Common L0 T=1/T=4 intervals are 189.551/338.372 µs in
the first three trials and 189.375/338.151 µs in the four-invalidation trial.
These are calibrated GPU replay intervals, not full-model throughput.

All **448 case runs, 13,440 retained samples and 4,480 warmups** pass the
scalar FP32 reference, full state/input/padding/timestamp guards and advance
marker checks. The same eight output fixtures remain byte-identical. Q8 has
finite checks and cross-path byte parity, not an independent CPU quantization
oracle. Each of the four candidates passes 15 Windows tests, six numerical
GPU groups and both denorm regressions. A staged checker correction uses the
existing host gate to hold work pending during negative in-flight/timeout
assertions; a GPU operation may otherwise finish before the assertion. The
original failed assertion and an ambiguous source-transform attempt remain
archived. Root checker/runtime source remains unchanged.

No candidate is adopted and no new full-model run follows from these small
effects. The 134/134 D2D and 130/130 imported-lock figures remain the separate
v7 census; copy/owner sources are unchanged. Linux GPU testing remains deferred.
[Three component controls](../orca-l0/demo/dg2-transition-remaining-windows-2026-11-03.json)
and 689 evidence files（本地归档：`../orca-l0/demo/dg2-transition-remaining-windows-2026-11-03.json.gz`）;
[four-invalidation control](../orca-l0/demo/dg2-all-invalidation-windows-2026-11-03.json)
and 401 evidence files（本地归档：`../orca-l0/demo/dg2-all-invalidation-windows-2026-11-03.json.gz`）
include exact source edits, raw samples and independent verification.

### Actual warmed mixed-GDN commands: matching work, different state packets

A process-local observer now captures the exact small T=1/local/one-slot
mixed probe, after conditioning and ten warmups. Each path submits once and
executes 16 GDN kernels interleaved with 16 advance kernels. Primary and
secondary batches are followed with bounded reads; both captures have zero
unknown, truncated or missing ranges. Observer timings are excluded from
performance evidence.

| Actual captured work | Native v8 | Installed L0 |
| --- | ---: | ---: |
| KMT submissions | 1 | 1 |
| Captured ranges | 1 | 3 |
| Executed-range command bytes | 13,796 | 7,020 |
| COMPUTE_WALKER | 32 | 32 |
| STATE_COMPUTE_MODE | 32 | 32 |
| CFE_STATE | 32 | 0 |
| STATE_BASE_ADDRESS | 64 | 1 |
| PIPELINE_SELECT / preemption LRI | 32 / 32 | 0 / 0 |
| PIPE_CONTROL | 68 | 69 |

Typed decoding confirms the same alternating GRF modes (16 normal and 16
large), group counts, local sizes, SIMD widths, execution masks, required SLM,
hardware threads per group and barrier counts. GDN has 48 groups of
128×4×1 work items, SIMD16, 32 hardware threads per group and 4-KiB rounded
SLM. Advance has one 64×1×1 group, SIMD32, two hardware threads and zero SLM.
Denorm and thread-group-dispatch fields agree. This rules out different work
geometry as the cause of this exact probe's gap.

Remaining fields include GDN preferred SLM MAX versus 16 KiB, SBA L1 WBP
versus WB and advance indirect-data length 64 versus 32 bytes. The preferred
SLM and SBA cache policies already have separate negative full-model trials;
this capture does not overturn those results. Packet counts alone do not
explain the gap: the isolated controls above remove different components with
very different effects. Command bytes are not physical PCIe/VRAM traffic.

All **64 numerical case runs** across both instrumented processes pass scalar
FP32 reference, full guards and advance-counter checks; eight FP32/Q8 fixtures
match the frozen workset baseline. Q8 checks remain finite/byte parity, not an
independent quantization oracle. The first attempt stopped at compilation;
the second completed native capture but its decoder-output assertion rejected
CRLF line endings. The corrected third attempt reuses that exact native
capture, source and executable, then runs L0. Both failed attempts are retained.
An offline typed pass checks every raw packet byte and all 32 walker geometries.

No runtime source is adopted, no fresh model throughput or D2D/lock census is
claimed, and no Linux GPU test runs. Source remains v8. Both new captures,
numerical outputs, source hashes, decoder logs and failure records are in
[the capture report](../orca-l0/demo/dg2-gdn-mixed-packets-windows-2026-11-03.json)
and 209 content-addressed evidence files（本地归档：`../orca-l0/demo/dg2-gdn-mixed-packets-windows-2026-11-03.json.gz`）.

### SBA and invalidation interact in the mixed-GDN probe

The exact command capture motivates a combined control rather than another
unchanged full-model packet-suppression trial. On pure C0 GRF transitions,
the new staged candidate omits redundant double-SBA **and** clears the four
state/constant/texture/instruction invalidation bits. It retains every
stall/HDC/untyped/DC field, LRI, pipeline, SCM and CFE command. Fresh, invalid,
untracked, coupled and fallback transitions keep the complete v8 path.

A seven-arm L0/native/SBA-only/combined/SBA-only/native/L0 sequence reuses the
exact frozen public-API probe object, operands and zebins. The SBA-only binary
is reused from its earlier trial. This directly tests the interaction:

| Local small mixed GPU interval | Native control µs | SBA-only control µs | Combined µs | L0 control µs |
| --- | ---: | ---: | ---: | ---: |
| T=1 | 374.537 | 337.161 | 185.781 | 190.924 |
| T=4 | 529.427 | 491.927 | 333.203 | 339.902 |

The combined interval falls **50.40%/37.06%** versus native and
**44.90%/32.27%** versus SBA-only. It is **2.69%/1.97% shorter than L0** on
these small local cases. Native control drift is -0.40%/-0.28%; SBA-only drift
is +0.15%/-0.01%; outer L0 drift is -0.11%/+0.02%. The individual costs are
not additive: clearing all four invalidations alone previously saved only
2.77%/1.69%. These controls establish an interaction, without identifying a
specific hardware cache-stall mechanism.

With 16 independent local slots, combined T=1/T=4 intervals fall to
**372.396/569.583 µs**, versus native **537.111/756.547 µs** and L0
**362.558/557.923 µs**. The remaining 2.71%/2.09% gap on those larger working
sets stays separate from the small-workset result. Summary values and
percentages are medians of two fixture-phase medians/percentages.

All **224 guarded case runs, 6,720 retained samples and 2,240 warmups** pass
the scalar FP32 reference, full guards and advance-marker counters. The same
eight FP32/Q8 fixture outputs match all controls byte for byte; Q8 remains a
finite/parity check rather than an independent quantization oracle. Fifteen
Windows tests, six GPU numerical groups and both denorm regressions pass.
Exact source and packet regressions verify that the two selected components
are the only runtime changes. Source is not adopted at this point; a clean
full-model Short comparison is required before claiming a decode benefit.
[Combined and incremental comparisons](../orca-l0/demo/dg2-sba-invalidation-windows-2026-11-03.json)
and 476 independently verified evidence files（本地归档：`../orca-l0/demo/dg2-sba-invalidation-windows-2026-11-03.json.gz`）.

The reversed combined/L0/combined probe uses the exact same executables,
with no recompilation. Small local mixed T=1 is **185.534 versus 189.661 µs**
(-2.18% relative to L0), and T=4 is **332.962 versus 338.932 µs** (-1.76%).
Candidate before/after drift is +0.22%/+0.25%. All 96 guarded cases and 2,880
retained samples pass, with the same eight fixture outputs. The small local
GPU advantage repeats; it does not establish full-model throughput.
[Reverse ordering](../orca-l0/demo/dg2-sba-invalidation-repeat-windows-2026-11-03.json)
and 113 evidence files（本地归档：`../orca-l0/demo/dg2-sba-invalidation-repeat-windows-2026-11-03.json.gz`）.

A clean L0/native/combined/native/L0 Short comparison relinks the same 80 Host
objects with only the new runtime library. All 100 requests match the complete
76-token natural output, 57/57 drafts, stop and zero reuse. The same 1,242 math
zebins, 5,771 slots, 1,536-MiB reserve, MTP4, step/last min-p 0.5 and cumulative
floor zero remain. Each process discards ten warmups and retains ten requests;
no GPU timestamps, inner timing probes or telemetry run. Decode excludes
preparation, initialization and prefill. Linux compilation and three CPU
checks pass; Linux GPU testing remains deferred.

| First clean Short bracket | Native control | Combined | L0 control |
| --- | ---: | ---: | ---: |
| Decode tok/s | 46.382 | 46.749 | 48.172 |
| TTFT ms | 906.809 | 906.223 | 890.239 |
| Whole request ms | 2501.507 | 2482.243 | 2425.192 |

Decode changes **+0.79%** against native control drift **+0.25%**; L0 control
drift is -0.40%. TTFT changes -0.06% against native drift -0.41%. The candidate
remains **2.95% below L0**. The model benefit is much smaller than the local
mixed-kernel gain. This first bracket prompted the reversed full-model
comparison below before adoption; no fresh D2D/lock census follows from this run.
[Exact Short protocols and parameters](../orca-l0/demo/dg2-sba-invalidation-short-windows-2026-11-03.json)
and 178 evidence files（本地归档：`../orca-l0/demo/dg2-sba-invalidation-short-windows-2026-11-03.json.gz`）.

The reversed full-model L0/combined/native/combined/L0 bracket reuses the same
executables without recompilation. Another 100 requests match complete natural
output, draft counts and stop under the same fixed policy. Its results are:

| Reversed clean Short bracket | Native center | Combined controls | L0 controls |
| --- | ---: | ---: | ---: |
| Decode tok/s | 46.911 | 47.103 | 48.495 |
| TTFT ms | 902.401 | 899.461 | 883.991 |
| Whole request ms | 2478.263 | 2464.855 | 2407.385 |

Combined decode is **+0.41%**, with combined-control drift **-0.19%** and L0
drift -0.50%. TTFT changes -0.33%, with combined drift +0.16%. The second
opposite ordering also observes a small decode improvement, but native remains
**2.87% below L0**. Control drifts are observations, not confidence intervals;
rates from different rounds are not combined into one comparison.
[Reversed model protocols](../orca-l0/demo/dg2-sba-invalidation-short-repeat-windows-2026-11-03.json)
and 171 evidence files（本地归档：`../orca-l0/demo/dg2-sba-invalidation-short-repeat-windows-2026-11-03.json.gz`）.

The combined fix is now adopted as **source v9** after the two opposite model
brackets and the repeated real-kernel checks. Only `src/drm/encoding.hpp` and
`tests/drm_compute_check.cpp` change. The checker also adopts the already-tested
host-gated negative assertions, so completed GPU work is not incorrectly
assumed pending. Fifteen Windows checks, eight numerical GPU groups and three
Linux CPU checks cover the exact adopted source. All 12 current production
source hashes match the measured version. Full programming is retained on
initial/invalid/untracked/coupled/fallback states; Linux GPU execution remains
deferred. Mathematical ISA, copy/ownership code, model policy, production
executables/configuration and immutable Linux bundles are preserved. The
134/134 D2D and 130/130 imported-lock figures remain the independent historical
census with nine unchanged copy/owner sources, not new measurements.
[v9 adoption manifest and measured limits](../orca-l0/demo/dg2-sba-invalidation-main-windows-2026-11-03.json)
and 151 adoption evidence files（本地归档：`../orca-l0/demo/dg2-sba-invalidation-main-windows-2026-11-03.json.gz`）.

### Isolated co-aligned copy path: counts stay equal, copying improves

The v10 source adds one optional `orca_copy_coaligned` export. It is selected
only when source and destination have the same nonzero alignment modulo 16;
its byte prefix is bounded by the requested size. There is still one copy
kernel per operation. The existing `orca_copy`, fill and pitched-copy execution
bodies are byte-identical through their end-of-thread instruction. Their raw
ISA hashes differ in eight post-EOT marker bytes; the archive retains the
unmodified binaries and complete IGA disassemblies rather than calling their
raw hashes identical. Group geometry, SIMD16, 128 GRFs, zero SLM/private/spill,
barriers, ownership and all 1,242 mathematical zebins stay fixed.

| Same warmed Short fixture | Native v10, fresh | L0 |
| --- | ---: | ---: |
| D2D operations / bytes | 134 / 4,554,752 | 134 / 4,554,752 (earlier payload reference) |
| H2D operations / bytes | 659 / 968,318,976 | 659 / 968,318,976 (earlier payload reference) |
| Imported mutex acquisitions | 130 | 130 (fresh) |
| Runtime mutex / shared SRW / exclusive SRW | 0 / 0 / 0 | 0 / 0 / 0 (fresh) |
| Foreground / background I/O acquisitions | 40 / 90 | 40 / 90 (fresh) |

The foreground calls resolve to PLE issue/collect; the 90 background calls
correspond to 15 keepalive reads at six calls each. Keepalive is not disabled.
Its timing can change the total on another run. Observer guards, driver-private
or inlined locks, contention and physical memory traffic are outside the census.
Native still has 40,778 walkers and 136 submission API calls in this capture.
Instrumentation is confined to separate diagnostic executables.

| D2D GPU interval, 16 independent outputs | Old native | Isolated copy | L0 |
| --- | ---: | ---: | ---: |
| 96 KiB, aligned | 26.621 us | 26.445 us | 33.339 us |
| 96 KiB, both +1 byte | 125.820 us | 40.938 us | 36.751 us |
| 1 MiB, both +1 byte | 212.038 us | 91.992 us | 88.112 us |
| 4,194,321 bytes, both +1 byte | 784.255 us | 337.253 us | 312.259 us |

Values are medians of two overwritten fixture-phase medians. Native and L0
controls bracket the candidate; they are geometric controls, not confidence
intervals. Two rejected shared-kernel implementations also appear in the raw
archive: both improved unaligned copies but slowed small aligned D2D by
9.04%-11.59%, so neither was adopted. The final path avoids that cost.
All four matrices total 1,296 guarded case runs / 38,880 retained samples.
Correctness additionally covers 460 recordings / 920 replays per memory-module
check, every equal byte offset 0..15, short sizes, unequal-offset fallbacks,
full source/destination/padding guards, replayed changed inputs and legacy packs.

| Clean Short, fixed policy | Native control | v10 | L0 |
| --- | ---: | ---: | ---: |
| Decode | 46.759 tok/s | 46.659 tok/s | 48.171 tok/s |
| TTFT | 902.922 ms | 903.628 ms | 889.056 ms |

Decode excludes preparation/init/prefill. Control drift is +0.117% native /
+0.038% L0. The observed v10 decode change is -0.215%; this is a copy-path
improvement without an established full-model gain. All 100 clean plus 33
counter requests pass 76 exact tokens, 57/57 drafts, stop and zero prefix reuse.
Policy stays at 5,771 slots, reserve 1,536 MiB, step/last min-p 0.5, cumulative
zero and MTP4. No inner timers, GPU timestamps or telemetry run in the model
comparison. Windows seven checks and Linux three CPU checks pass; Linux GPU
execution remains deferred.

Adoption updates exactly five runtime/checker files under the shared experiment
lock. Existing executables, configuration, MTP work, root README and immutable
Linux bundles are preserved. Rebuild and bake the new five-export memory module
to enable the added path; older modules keep previous behavior.
[All samples and ISA proof](../orca-l0/demo/dg2-copy-throughput-windows-2026-11-03.json)
and 333 archived evidence files（本地归档：`../orca-l0/demo/dg2-copy-throughput-windows-2026-11-03.json.gz`）,
[model/counter results](../orca-l0/demo/dg2-copy-prefix-short-windows-2026-11-03.json)
and 103 archived evidence files（本地归档：`../orca-l0/demo/dg2-copy-prefix-short-windows-2026-11-03.json.gz`）,
[v10 source manifest](../orca-l0/demo/dg2-copy-prefix-main-windows-2026-11-03.json)
and 29 adoption files（本地归档：`../orca-l0/demo/dg2-copy-prefix-main-windows-2026-11-03.json.gz`）.
