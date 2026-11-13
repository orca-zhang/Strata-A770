# Orca L0 and Orca Direct

**Orca L0 is recommended. Orca Direct is WIP and an alternative, not the preferred path.**

**Orca Direct** is the name of the native DG2 runtime (`Orca::DG2`): it encodes
GPU commands and submits through Windows D3DKMT or Linux DRM. **Orca L0** retains
the Level Zero path. Source paths, CMake targets and namespaces keep their existing
names.

The [early-commit control failure](../bench/results/a770/orca-direct-early-commit-2026-11-08.md)
retains an abnormal baseline and its watchdog termination. The candidate builds
but has zero GPU requests, so there is no speed or adoption conclusion. An
offline dequantization wait snapshot does not establish a cause; a separate
frozen CPU probe passes all 2,112 checks. Orca L0 remains recommended.

The [adaptive-executor comparison](../bench/results/a770/orca-direct-adapt-future-2026-11-08.md)
passes 102 requests / 7,836 output tokens. The installed `std::async` executor
reuses threads/TLS, but opposed Short throughput changes of +0.377% / -1.097%
fail the model gain gate. This isolated 5243-slot candidate is not adopted;
Orca L0 remains recommended and Linux GPU testing remains pending.

The [Host-stage attribution](../bench/results/a770/orca-direct-decode-stages-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero trace losses. Of 7.018 ms
in empty GPU/queue intervals outside Owner and wait scopes, 3.460 ms falls
in post-verify routing, acceptance and adaptive-thread preparation. Both
active contexts are compute contexts: verify uses one, and the other appears
during draft. This diagnostic identifies the next isolation target; it is
not a normal speed result or a default change.

The [completion-wait diagnostic](../bench/results/a770/orca-direct-wait-trace-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero trace losses. The traced
decode has 128 completion-observer waits, 99 registrations and 101 OS waits;
`wait_sequence` and `wait_fence` are not entered. Two OS timeouts are normal
50 ms slices. Empty GPU/queue intervals outside Owner calls include 9.434 ms
inside OS waits and 7.326 ms outside marked waits. These are call-interval
overlaps, not measured wakeup latency or recoverable speed; the independent
5243-slot diagnostic changes no default.

The [actual decode-owner diagnostic](../bench/results/a770/orca-direct-owner-handoff-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero trace losses. The traced
decode contains 128 cross-thread calls, all at one Owner's replay callsite.
Handoff delays total 9.403 ms, overlapping 4.897 ms of the 17.902 ms without
owned DMA or outstanding packets; 2.176 ms falls in Owner bodies and 10.829 ms
outside Owner intervals. The three Direct windows use two active GPU contexts.
This is attribution evidence, not normal performance or a CallerOwner adoption;
the next investigation must also cover intervals outside the Owner and within DMA.

The [idle-owner CPU diagnostic](../bench/results/a770/orca-direct-owner-idle-2026-11-08.md)
passes 2,112 return/ownership checks in six alternating worker/caller windows.
After measured idle intervals of about 16/31–32/63 ms, worker invoke medians
are 22.55–52.05 us versus caller's 1.60–3.10 us; hot medians are 0.90–1.50
versus 0.20 us. This CPU study does not measure model handoffs; the previous
CallerOwner model experiment did not establish a stable model gain.

The [decode-boundary ETW diagnostic](../bench/results/a770/orca-direct-etw-phases-2026-11-08.md)
passes 36 requests / 2,868 output tokens; two Host markers bracket the unchanged
decode timer within 0.051 ms. Both backends have 128 decode DMA segments.
Against the arithmetic mean of the two L0 controls, Direct's marked decode
takes 25.823 ms more: 14.551 ms within DMA intervals and 11.272 ms between them.
Gaps with no outstanding queue packet total 14.089 ms versus L0's 2.867/2.483 ms;
gaps with outstanding packets remain similar. This isolates decode from observed
prefill, without proving a specific Host handoff, context or kernel cause. The
5243-slot frozen profile and instrumented timings remain diagnostic only.

The [Short-request ETW diagnostic](../bench/results/a770/orca-direct-etw-scheduler-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero lost trace events/buffers and
complete owned queue/DMA pairs. Direct uses 153 work-DMA segments versus 240
for matched L0. Median submit-to-first-DMA wait is 35.2 us versus 64.15/71.55 us,
but gaps without this process's DMA total 22.478 versus 10.299/11.316 ms.
This covers the whole Short request, including preparation/prefill; it does not
isolate decode or establish a normal speed result or driver-queue cause.
The [checker correction](../bench/results/a770/orca-direct-etw-scheduler-check-2026-11-08.md)
preserves the original JSON-key comparison failure. Correct offline command:
`python -X utf8 build-orca-etw-scheduler-20261108/check.py`.

The [equal-Q2-source diagnostic](../bench/results/a770/orca-direct-policy-parity-2026-11-08.md)
matches all 60 requests / 4,780 output tokens at 5,243 slots / 7,201 MiB.
The matched L0 Verifier/shared-expert source texts equal Direct's. Warm Short
decode changes from 50.505 to 50.822 tok/s for L0 (+0.628%); Direct reaches
49.918 tok/s (-1.779% versus matched L0). Control drift passes. This single
nested ordering uses cache budget5000 and disables short-route heat for all;
it is separate from the 5771-slot anchor and current spec6 production. Other
adapters/dynamic work are not proved equal, and no production change is adopted.

The [whole-window replay functional check](../bench/results/a770/orca-direct-whole-window-functional-2026-11-08.md)
passes 20 Short requests / 1,400 output tokens and 20 extra real T4 window replays.
Every replay restores pre-window PLE, QSA and GDN state, then checks all state,
history snapshots, logits and sampled IDs byte for byte. Restoring PLE alone
first failed the QSA check; that failure remains. The full-reset matched attempt
stopped at READY with 5757 versus required 5771 slots. The separate functional
run uses 5748/5771 slots, so it establishes no new speed comparison or Short
cause. No production code, deployment or Linux GPU status changes.

The [current-qualified runtime comparison](../bench/results/a770/orca-direct-qualified-contexts-r2-2026-11-08.md)
uses the 19-check-qualified pure native object and corrected L0 global-residency
accounting, with the original frozen HC128/MTP4/min-p0.5/reserve1536 history.
All 84 requests / 7,272 output tokens match. Direct Short decode throughput is
1.686%/2.058% below L0; repeated 8K changes -0.442%/+0.466%. Short TTFT takes
9.732%/10.186% less time and whole requests 2.215%/2.147% less time.
Quality, capacity and control-drift gates pass; the no-regression gate fails.
The first attempt failed linking libircmt.lib before any engine started; only
the recorded oneAPI link environment was restored. This is separate from current
spec6 production. Deployment and the recommended L0 path are unchanged;
Linux GPU validation remains pending. Offline verification uses
`python -X utf8 build-orca-qualified-contexts-r2-20261108/publish.py check`;
the report's copied command points to the first failed attempt instead.

The [first attempt](../bench/results/a770/orca-direct-qualified-contexts-2026-11-08.md)
is preserved with its link-environment failure and zero GPU requests. It did not
reach capacity, numerical or performance checks; its generic gate wording does
not describe the actual failure.

The [largest-window-first capture experiment](../bench/results/a770/orca-direct-warm-order-2026-11-08.md)
reduces frozen native thread storage from 344 to 332 MiB, without removing either
old private pool. Eight complete model requests and 12 matched 5771-slot Short
requests pass output/counter checks. The Short forward bracket observes decode
throughput -0.213%, with control-time drift +0.780%; this small delta may be
noise and establishes no speed gain. The candidate is not adopted or retried;
runtime/defaults remain unchanged and Linux GPU validation is pending.

The [within-submission HC ordering audit](../bench/results/a770/orca-direct-hc-ordering-paths-2026-11-08.md)
follows all 129 captured L0 batch-chain jumps. In 32 up tails, two PIPE_CONTROL
commands occur after the captured-buffer boundary; the comparable norm paths
also retain CS stalls. The previously tested duplicate-tail removal established
no gain and is not repeated. This offline evidence adds no model-speed claim
or runtime/default change.

The [idle-only thread reclamation check](../bench/results/a770/orca-direct-thread-trim-2026-11-08.md)
validates explicit `Runtime::trim_thread_memory()`: all current queue bindings
and retained lists are protected. Host/device lifecycle checks, 19 existing
CTests and eight frozen HC128/MTP4/min-p0.5 model requests pass. Startup recapture
plus trimming reclaims 73.5 MiB, leaving 270.625 MiB of thread pools and 0.125 MiB
of scratch descriptors. Actual capacity is 5707 slots; this is no new speed
result or pass of the original 5771-slot conditions. Linux compilation/CPU checks
pass, with Linux GPU pending. Recapture and model auto-trim remain off by default.

The [pool reference audit](../bench/results/a770/orca-direct-thread-pools-2026-11-08.md)
finds all eight pools retained by recorded lists: 221.5 MiB is currently bound,
and 122.5 MiB belongs to older bindings. A separate [READY recapture observation](../bench/results/a770/orca-direct-pool-recapture-2026-11-08.md)
removes references to three old pools totaling 73.375 MiB; another 49.125 MiB
remains referenced. Nothing was freed and total thread storage remains 344 MiB.
These diagnostics use 5413/5586 slots, send no GEN and establish no speed gain.

The [READY memory inventory](../bench/results/a770/orca-direct-memory-inventory-2026-11-08.md)
attributes 344 MiB of 351.875 MiB native internal local storage to eight
scratch/private pools; parameter mirrors use 2 MiB. Five complete ownership
partitions and normalized-source/archive checks pass. This is a READY-only,
5569-slot frozen-profile diagnostic, without a matched L0 category comparison,
new speed claim or pass of the original 5771-slot performance conditions.

The [current native-runtime model check](../bench/results/a770/orca-direct-native-import-model-2026-11-08.md)
passes eight complete protocols with frozen HC128/MTP4/min-p0.5 math and Host
objects, loading no SYCL/UR/Level Zero/IGC/GMM DLLs. A separate two-request live
allocation audit records 1143 successful native creates and zero caller-page or
standard-allocation calls; these requests do not use the newly completed external
import/large-Host paths. The original missing destructor summary remains recorded.
Actual slots are 5577/5567, so these functional checks do not pass the original
5771-slot performance conditions or establish a speed/PPL gain. Exact archived
source/protocol bytes pass verification; deployment and Linux GPU status are unchanged.

The [Windows memory query correction](../bench/results/a770/orca-direct-memory-budget-2026-11-08.md)
also constrains L0 by live global residency when its Sysman path is unavailable.
Six query-fault/recovery checks and eight exact model protocols pass. This source
change can reduce slots under memory pressure: its strict 5771-slot attempt failed
at 5734, and the separate functional run used 5673. It establishes no new speed
gain; deployed executables and configuration are unchanged.

The [native Host import candidate](../bench/results/a770/orca-direct-native-import-2026-11-08.md)
removes the two remaining Standard ExistingHeap paths in an isolated current-source
build. Eight driver calibrations through 4 GiB + 64 KiB pass full caller-content
and ownership checks; seven eligible descriptors match its C++ serializer exactly.
All 19 CTests and dynamic-token semantics now pass, including full 4 GiB + 64 KiB
owned/imported contents, GPU boundary reads/writes and five injected import
failures. The exact checked source is adopted into Windows Direct, removing its
remaining StandardAllocation paths. Original failures remain; deployment is
unchanged and this has no new model-speed claim.

The [native Host allocation diagnostic](../bench/results/a770/orca-direct-native-host-2026-11-08.md)
reduces Direct's first 2K TTFT by 15.155%/17.419% in 84 matched request protocols,
with no stable warm Short decode gain. A separate untimed 84-request comparison
finds Direct 2.254%/2.077% below L0 in Short decode and 1.359%/0.308% below on
repeated 8K, while Short TTFT takes 10.588%/10.430% less time. Output/counter and
control-drift checks pass; the overall no-regression gate fails. These are fixed
HC128/MTP4/5771-slot frozen-runtime results. Windows Direct now adopts the
qualified Host source path; a fresh build passes all 18 registered CTests,
including ownership/failure cleanup and 920 GPU transfer replays. Current-source
model performance is not established; deployment is unchanged and Linux GPU is
untested.

See the [complete parallel pipeline diagram](../docs/ORCA_PARALLEL_PIPELINE.md)
for request flow, layer dependencies, CPU/BCS/compute overlap and buffer reuse
rules. It distinguishes explicit backend options from current default support.

Native DG2 argument descriptors now use a compiler-sized `std::array` on the
stack. Pointer/count and initializer-list append calls consume their arguments
synchronously; the existing vector entry points remain available. This removes
the temporary descriptor-vector allocation, not all recording allocations.
The shared C++17 core passes Windows CPU checks, 12 native compiler GPU pipeline
replays and 64 Strata HC checks with rebuilt/frozen Host objects mixed. A fresh
Linux/WSL core build passes four CPU checks and its dependency audit. Full-engine
performance and Linux GPU execution are not established by these initial checks; deployed
EXEs are unchanged. See the [validation scope](../docs/ORCA_NATIVE_BACKEND_STATUS.md).

The subsequent [all-49-Host model comparison](../bench/results/a770/orca-direct-argument-array-2026-11-06.md)
passes 64 HC checks and all 100 complete request protocols with the original
engine core, math pack and fixed 5771-slot/MTP4/min-p0.5 policy. Short decode
changes only +0.472% against old Direct, failing the preset 1% gain gate.
Against L0, new Direct decode is 2.961%/2.272%/1.690% lower on Short/repeated
2K/repeated 8K, while repeated 2K/8K TTFT takes 13.419%/6.566% less time.
Control drift passes; longer-context positions run once per arm. This does
not establish a necessary TTFT/decode tradeoff: the earlier same-backend HC256
comparison improves both. READY plus first TTFT is essentially unchanged
(+0.106% versus old Direct); no cold-start gain is established. Deployment and
the recommended L0 path remain unchanged; Linux GPU is still untested.

The isolated [cached-completion dependency trial](../bench/results/a770/orca-direct-completed-dependencies-2026-11-06.md)
removes 24 redundant driver waits in the standalone BCS demo, retaining all 12
pending waits, exact outputs and guards. All 100 model protocols match, but
Short gains only +0.350% and L0 control drift reaches 4.331%, failing preset
gates; initial long-request regressions are retained. No stable model gain is
established, so this candidate is not adopted. Its Linux core builds and passes
four CPU checks; Linux GPU remains untested. Driver-call reduction alone is
insufficient evidence of faster decode.

The [current U4/HC window diagnostic](../bench/results/a770/orca-direct-current-window-2026-11-06.md)
matches 90 complete Short protocols and 2,610 windows in two opposed backend
orders. Direct GPU window spans are 2.513%/2.613% longer than L0; Host
submit-to-return spans are 2.874%/2.866% longer, with outer drift below 0.15%
for these primary metrics. Host window-input preparation is only about
1.21/0.69 ms per request. This directs further work toward current-window
execution/dependencies and outside-window MTP/commit, rather than preparation
alone. Ordered markers/readback perturb scheduling; these are diagnostic
spans, not clean throughput or exclusive kernel time. Deployment is unchanged.

An [offline Host split of the same 90 requests](../bench/results/a770/orca-direct-current-host-split-2026-11-06.md)
puts about 33 ms of Direct's extra time in the Verifier-call region, 11--12 ms
in draft preparation/execution/waiting, and 3 ms in commit/output. This uses
additive arithmetic controls and rounded existing logs, with no new GPU run;
the regions include GPU work and must not be described as pure CPU overhead.

The [current-pack HC1-to-route/device-plan follow-up](../bench/results/a770/orca-direct-current-route-phase-2026-11-06.md)
matches 90 more protocols and all 2,610 windows. Direct takes 6.327%/6.278%
longer in this region in opposed orders, with phase-control drift below 0.12%.
The interval also includes router/Top-K/quantization/device-plan dependencies;
96 additional ordered markers per window perturb scheduling. It identifies a
region for further work, not pure HC1 time or a new throughput result. The
runtime default and deployment remain unchanged.

The [current-U4/HC compact-GRF check](../bench/results/a770/orca-direct-current-grf-u4-2026-11-06.md)
removes three unchanged packets at each of 1,202 recorded GRF-only transitions;
both censuses retain 14,879 kernel appends and matching normalized state/length
histograms. All four census and 150 clean model protocols match, with 28 native
CTests, 128 HC checks and five Linux/WSL CPU checks passing. Opposed clean
Short decode changes are only -0.007%/-0.053%, below the preset +1% gain gate.
The candidate stays isolated/default-off. This removes constant packets, not
the mode transitions or their stalls; Linux GPU and the Direct 8K issue remain
unverified by this experiment.

The [bounded CPU-activity isolation](../bench/results/a770/orca-direct-cpu-activity-2026-11-06.md)
keeps the accepted Direct EXE, event waits and current math pack unchanged.
All 150 Short protocols match in opposed orders; decode changes +0.632%/+0.377%
and TTFT -0.228%/-0.886%, passing control drift but failing the preset 1% decode
gain gate. A below-normal helper on CPU43 adds 2.122--2.141 CPU seconds per
complete request, with no engine-affinity or system-power changes. No C-state,
uncore or energy cause is established. The helper is not adopted; runtime and
deployment defaults remain unchanged, with no new long-context or Linux GPU claim.

The [generic FP16 M16 projection trial](../bench/results/a770/orca-generic-projection-m16-2026-11-04.md)
now extends register reuse to aligned ordinary projections, benefiting both
transports. All 102 model requests match. Same-EXE off/on/off comparisons reduce
Direct 2K/8K TTFT by **33.46%/43.55%**, and L0 by **26.43%/38.94%**.
With identical M16 math, Direct TTFT is **20.90%/11.96% shorter than L0**;
Short remains **58.30 Direct / 59.37 L0 tok/s**. Long requests run once per
process; these are finite observations, not stable cross-day measurements.
The math gain is shared, not a backend-only improvement. Native source enables
M16 by default for eligible FP16 shapes; `STRATA_ORCA_XMX_WIDE=0` restores
the generic M4 path.
Rebuild the backend and XMX image pack together. Production EXEs/configs are
unchanged; Linux GPU and the intermittent Direct 8K fault remain open.

The [Short bounded-wait comparison](../bench/results/a770/orca-direct-hybrid-wait-2026-11-04.md)
matches 210 requests across two opposed windows. Neither 8-us nor 32-us spin
improves throughput, so default event waiting stays unchanged. Direct remains
2.13–4.40% below L0 in decode, while using about **97.92% less whole-request
process CPU time** on this fixed profile. CPU accounting excludes startup and
warmup and includes prefill/decode. Separate 30-request diagnostics attribute
most L0 CPU to `zeCommandListHostSynchronize`; this is not an energy result or
a vendor-internal stack trace.

The [large-projection allocation screen](../bench/results/a770/orca-direct-projection-alignment-2026-11-04.md)
keeps Host/math/synchronization fixed while testing calibrated 2-MiB descriptors
and aligned GPU VA. Both builds pass 28 checks and 80 changing-input allocation
replays; opposed timing brackets pass 180 samples and 120 data checks, but do
not establish a stable improvement. The policy is not adopted; opposed Direct
times remain 3.659%/5.309% longer than L0 on the two synthetic projection shapes.
The Linux CPU checker now explicitly runs all four checks, including shared
BCS encoding; a fresh WSL build and dependency audit pass. This is not Linux
GPU validation or a new whole-model performance result.

The [source adoption](../bench/results/a770/orca-source-adoption-2026-11-04.md)
integrates Host eager/device-recorded parameters, shared BCS encoding and M16
FP16 pair tiles. Fresh Direct and L0 builds each pass 840 pair checks and match
all six cold-history 2K/2K/8K model requests. This does not close the earlier
intermittent Direct 8K failure. The real repository Linux/i915 source builds
and passes four CPU CTests. Direct remains
experimental, `ORCA_DG2_BCS_COPY=1` explicitly enables BCS, and **Linux GPU is
untested**. Production configs and deployed EXEs are unchanged. The performance
comparisons below identify their own frozen EXEs; adoption correctness checks
alone do not establish performance.

The [graph-tail proof trial](../bench/results/a770/orca-direct-replay-prefix-2026-11-04.md)
adds a separate isolated adapter candidate on these source builds. All 85
Short/2K/8K requests and the [75-request opposed Short repeat](../bench/results/a770/orca-direct-replay-prefix-repeat-2026-11-04.md)
match. Reusing the completed tail avoids redundant old inline-tag queries;
new-generation tag resets and fences remain. Same-EXE Short gains are
**+0.95%/+0.13%**, while Direct stays **0.93%/2.45% below L0**. The repeat's
gain is smaller than its control drift, so no stable model gain is established
and this candidate is not adopted. Forward 2K/8K TTFT is **10.71%/2.11% lower
than current-source L0**, with one long request per process; those observations
do not establish long-context repeatability or close the intermittent 8K issue.

The [current-source wait isolation](../bench/results/a770/orca-direct-replay-prefix-wait-2026-11-04.md)
keeps those prefix-proof Host objects and changes only the native observer wait.
All 75 Short requests match; event/poll/L0 decode is **56.61/57.39/58.32 tok/s**.
Active polling gains **1.37%** over events, yet remains **1.60% below L0**;
event/poll control drift is **+0.76%/+0.29%**. Fourteen runtime CTests and the
kernel/engine/BCS queue checks pass. This is a CPU-consuming diagnostic upper
bound; default waiting stays unchanged. No new long-context result follows.

The isolated [Point allocation batch trial](../bench/results/a770/orca-direct-point-batch-2026-11-04.md)
groups at most 16 inline event objects per shared owner, without waiting for
work to accumulate. All 85 model requests match; generation, strong/weak
lifetime and older unpublished readback checks pass. Same-EXE Short decode
changes **−0.57%**, against **+1.14%** off-control drift: no model gain is
established, so this candidate is not adopted. Unmodified Direct observes
**8.01%/0.68% shorter 2K/8K TTFT** than current-source L0 in this bracket;
each long case runs once per process. These are finite observations, not
long-context steady-state results, and the intermittent 8K issue remains open.

The separate [prefill submission census](../bench/results/a770/orca-direct-prefill-submit-census-2026-11-04.md)
matches all **34 requests** across 68 prefill/decode spans. On the adopted
sources, Direct/L0 2K prefill submits **67,669/184,527** times; 8K submits
**81,510/225,908** times: **63.33%/63.92% fewer** for Direct. Hot Short decode
has **97 submits and 112 observable `_Mtx_lock` calls on both paths**. This
observer has no per-call clocks or packet capture, but still perturbs execution;
its durations are not performance results. The smaller clean 8K time advantage
cannot be explained by the submission-count advantage disappearing. Compute,
memory and expert-pipeline time shares remain to be measured.

The [dependency-only prefill follow-up](../bench/results/a770/orca-direct-dependency-only-2026-11-04.md)
adopts three source files byte-identical to its tested candidate. Sixteen prefill
sites append their wait without exporting an unused eager completion. All 85
clean and 34 diagnostic model requests match; 69 adapter-dependent Host TUs and
the kernel/engine/BCS queue checks pass. Same-EXE 2K/8K prefill takes
**5.35%/2.08% less time**, against **−0.44%/−0.02%** off-control drift;
its separate census cuts submits **67,669→39,671 / 81,510→47,638**
(**41.37%/41.56% fewer**). Versus common-math L0, prefill takes
**14.15%/3.44% less time**. Short decode is
**56.77 versus 56.85 off and 58.10 L0 tok/s**: no decode gain is established.
Long cases run once per process. `STRATA_ORCA_DG2_DEPENDENCY_ONLY=0` restores
the previous eager boundary policy. Direct stays opt-in, deployed EXEs are
unchanged, Linux GPU is untested, and the intermittent Direct 8K issue stays open.

The isolated [small-primary/secondary-batch trial](../bench/results/a770/orca-direct-secondary-batch-2026-11-04.md)
reuses those Host objects and math while changing only WDDM command submission.
All 85 clean requests match. Short on/off/L0 gives **56.94/57.11/58.31 tok/s**:
**−0.30% versus off**, with **+0.71%** off-control drift, and **2.35% below L0**.
2K/8K prefill changes **+1.74%/+0.41%** versus off; long cases run once per
process. No full-model gain is established, so this candidate is not adopted.
Thirty separate diagnostic Short requests match; hot decode primary command
bytes fall **7,198,104→1,552**, while submits/mutex calls stay **97/112**.
The original sealed body was already reused without a per-replay command copy;
smaller submitted primary lengths alone are not performance evidence.

The isolated [seven-arm combination trial](../bench/results/a770/orca-direct-combined-2026-11-04.md)
matches all **119 requests**. Combined Direct observes **8.92%/6.97% shorter
2K/8K prefill than original L0**, or **7.84%/1.55% shorter with the same wide
mathematical pack**. Short decode remains **1.90% below original L0**.
Each long case runs once per process; control drift is reported, without a
steady-state claim. That timed EXE is the isolated candidate; the source adoption
above has separate checks. The intermittent 8K correctness issue remains open.

The same report includes a separate portable BCS source check: shared packet
bodies pass the Windows GPU regression; Linux/i915 compiles under WSL and
passes CPU checks, including 2,268 packet-output cases on each OS. This source
is separate from the timed EXE and predates the adoption above. **Linux GPU remains untested**,
including the Windows-calibrated BCS MOCS/cache policy.

The isolated [current-frontend BCS integration](../bench/results/a770/orca-direct-current-bcs-2026-11-04.md)
routes three explicit copy roles to Copy node2 while compute stays on node5.
Same-EXE off/on/off observations give **7.32%/2.89% shorter 2K/8K prefill** and
**+0.98% Short decode**. All 51 model requests match; 1,632 fills, 12 dependency
chains and 16 outputs across two compute/two BCS logical queues pass. Logical
queues share the nodes; no physical parallel-utilization claim is established.
Long cases run once per process. This Windows candidate is not adopted;
This frozen snapshot has no Linux BCS; portable source checks are listed above.
The intermittent Direct 8K correctness issue remains open.

The isolated [real-model FP16 pair tile follow-up](../bench/results/a770/orca-fp16-pair-tiles-2026-11-04.md)
observes **3.77%/10.09% shorter 2K/8K prefill** versus Direct's previous pair
kernel, with the same dynamic-parameter policy. All 51 model requests match;
Direct/L0 pass 1,680 pair output/guard checks. Each long case has one request
per process; Short is unchanged, and no L0 model-performance arm is included.
The candidate is not adopted; the intermittent Direct 8K failure remains open.

The isolated [dynamic-parameter follow-up](../bench/results/a770/orca-direct-dynamic-parameters-2026-11-04.md)
observes **5.77%/2.77% shorter 2K/8K prefill** against Direct itself, with all
51 model requests matching. Long cases have one request per process; Short has
no measured gain. Placement replay checks pass, but the earlier intermittent
8K failure remains unresolved. This candidate is not adopted and does not
establish a win over L0.

The independent [XMX SLM/prefetch demo](../bench/results/a770/orca-xmx-slm-async-2026-11-04.md)
finds larger register-reuse tiles promising for larger batches, while tested
SLM/double-buffer versions lose and small batches can regress. It passes
760 output/guard checks; these are standalone kernel results, not model
throughput or TTFT gains. A runnable source and three rounds of raw data are
provided. Production kernels/defaults are unchanged; Linux GPU is untested.

The earlier [Short/2K/8K comparison](../bench/results/a770/orca-direct-current-contexts-2026-11-04.md)
finds Direct decode **1.83%/1.51%/2.21% below L0**, with **6.62%/7.75% longer
2K/8K prefill** under the same fixed anchor. All 69 clean requests match;
a separate shortened-history control exposes an **unresolved Direct 8K output
failure**. Diagnostics point to many synchronous parameter uploads for further
investigation; no new performance candidate is adopted. Bitmap opportunities
are source-level candidates without measured throughput gains. This MTP-only
protocol differs from the earlier Short audit and is not a daily-default or
Linux GPU result.

The preceding [idle Short audit](../bench/results/a770/orca-direct-idle-audit-2026-11-04.md)
finds Direct **1.80%/2.13% below L0** in opposed current-frontend brackets.
Hot logical copies match: **134 D2D / 4,554,752 bytes**, zero explicit H2D/D2H
and zero adaptive refill. Observed EXE-import mutex calls are lower in Direct;
these counts exclude private/inlined/driver locks. Experimental active observer
polling improves Direct over its event wait by **1.83%/0.94%**, but a fresh
L0 bracket still leaves polling Direct **2.12% behind**. All 269 requests match
tokens/drafts; the polling candidate stays isolated and defaults are unchanged.
This is the fixed Short anchor, not daily-default or Linux GPU performance.

The preceding [whole-frontend rebuild](../bench/results/a770/orca-direct-current-frontend-2026-11-04.md)
uses 48 matching ABIs, 1,269 byte-identical numerical SPIR-V modules and one
fresh native collection for both transports. It finds and fixes a source-policy
mismatch: Direct's cache margin was still 0.5; both Orca paths now use the existing
1.5 margin. Matched forward/reverse results are **56.50/58.96** and
**58.80/59.73 Direct/L0 tok/s**: Direct remains **4.18%/1.56% below L0**,
with -1.28%/-0.40% flank drift. All 180 outputs/drafts match and six GPU checks
pass. Both rounds are retained; a native-over-L0 throughput win is not established.
This is the fixed Short anchor, not daily-default or Linux GPU performance.

An earlier [complete current-runtime rebuild](../bench/results/a770/orca-direct-current-runtime-2026-11-04.md)
on Windows A770 gives **56.53 Direct / 55.88 old Direct / 57.76 L0 tok/s** under
the same fixed Short policy: **+1.17%** over the old library, **2.12% below L0**.
It includes current ISA padding, early code slabs and scratch reuse, preserving
the same Host objects and mathematical pack. All 86 complete outputs match;
Windows checks and device-local numerical replay pass. This is not a whole
frontend rebuild, daily-default, long-context or Linux GPU measurement.

The earlier [fixed-short remeasurement](../bench/results/a770/orca-direct-anchor-2026-11-04.md)
records the older Direct at **49.30 tok/s**, versus **56.24 tok/s** for L0.
Its profiles had different expert replacement margins; the follow-ups below
retain their own frozen build and policy scope.

The [fixed-anchor cache/copy ablation](../bench/results/a770/orca-direct-copy-margin-2026-11-04.md)
finds a host-policy mismatch: L0 uses expert replacement margin **1.5**, Direct
**0.5**. Changing only that constant raises Direct **50.52 to 55.72 tok/s
(+10.29%)**, with 0.05% control drift. The later matched-margin BCS/EU/L0 trial
measures **54.86/54.48/56.58 tok/s**: BCS adds 0.69%, but Direct remains 3.04%
below L0. All 292 outputs match; hot diagnostic refill falls from 640 expert
copies / 843.55 MiB per request to zero, matching L0. This is a separate frozen
256/0.95 Short profile, not the later 1024/0.5 numerical-source trials or a
daily-default change. Raw evidence and reproduction configurations are linked.

The [daily-parameter check](../bench/results/a770/orca-direct-daily-margin-2026-11-04.md)
reverses that margin gain: at 1024/0.5 tuning and cumulative draft threshold 0.5,
Direct falls **44.88 to 41.02 tok/s (-8.62%)** when raising its margin to 1.5.
Matched-margin L0 is **42.03 tok/s**; default margins stay unchanged. These 60
requests use a fixed 5,771-slot capacity, after an automatic-allocation failure.
The WDDM source now recovers completed nonlocal-residency rejection by synchronously
destroying every allocation part before allowing a smaller retry. Deterministic
failure checks and 920 GPU transfer replays pass; uncertain driver/cleanup failures
remain terminal. This source fix requires a rebuild; deployed binaries are unchanged.

The L0 library is a small inference compute runtime using Intel's Level Zero C API directly.
Linux is the primary deployment target; Windows is a compatibility target.
The host library uses C++17 and, by default, `ze_loader`; it does not link SYCL, Unified
Runtime, oneMKL or the OpenCL runtime. `ocloc` is needed only to build the check
kernels. The Intel GPU driver and its compiler are still required.

`orca-l0/` builds on its own and can be copied into a separate repository.
Its CMake target is `Orca::L0`, and its C++ namespace is `orca::l0`.
The source is covered by the included MIT license.

## Native binaries and lower driver layers

`ORCA_L0_DIRECT_DRIVER=ON` binds Intel's vendor DDI tables once at initialization
and removes the `ze_loader` dependency. Linux loads `libze_intel_gpu.so.1`;
Windows requires an absolute vendor DLL path in `ORCA_L0_DRIVER`.
This still uses Intel's Level Zero user-mode driver.

`orca-l0-native-pack SOURCE NEW_OUTPUT [--modules trace.tsv | --compatible]` bakes existing
SPIR-V packs into device-specific zebin, preserving their original compiler
options. Build the tool with `cmake --build BUILD --target orca-l0-native-pack`.
Set `ORCA_L0_MODULE_TRACE=trace.tsv` while exercising representative workloads
to record the modules and flags actually loaded. A selected bake retains
unselected variants as SPIR-V. The native device identity is checked before
loading; rebuild after a device/driver change and build Linux packs on Linux.
`--compatible` preserves images that require unavailable FP64, and SG16
cooperative-matrix images on DG2, with reasons in `retained-spirv.tsv`.
It changes no arithmetic and still fails on other compiler errors. Omit it
for a strict bake. The tool's `--self-check` validates capability parsing,
ordinary SG16 versus matrix requirements and malformed inputs without a GPU.

Windows A770 compatible baking now produces 1,243 zebin images across 48
packs. It explicitly retains 18 FP64 images and six SG16 matrix images;
these variants cannot be compiled for the tested DG2 path. The IQ pack's
252 supported variants and all 611 GEMV variants bake successfully.
Full-model checks of the expanded pack loaded 150 native modules, including
exact Short/Code2K/Prose2K/Long8K output parity and no SPIR-V fallback in those
workloads. The bracketed rerun measured **51.539 / 51.529 / 51.599 tok/s**
for existing Orca / direct DDI + expanded zebin / existing Orca: effectively
unchanged (candidate −0.078%, control drift +0.115%). These are decode times,
excluding preparation/loading/prefill; this compares against existing Orca,
not the original SYCL stack. [Parameters, hashes and checks](demo/native-windows-2026-10-31.json)
are recorded; one round establishes **no stable speedup** from these lower layers.
See [the exact status and limits](../docs/ORCA_NATIVE_BACKEND_STATUS.md).

Windows also has an experimental direct D3DKMT execution check:

The latest adopted native baseline is **v14**, reusing this queue's already allocated scratch
pools for no-scratch kernels. Private memory remains demand-bound. Independent
warmed Short captures show scratch-base changes **98 to 3**, removing **95 full
state preambles / 190 SBA commands**, with the same walker shapes and mathematical
order. Opposite timing orderings give **+0.364% / +0.701%** decode versus v13,
near control drift; the first candidate is **0.967% below matched L0**, whose
control drift is -4.204%. No stable native or TTFT win is established. All 171
outputs match; Windows checks and Linux build/three CPU checks pass. Linux GPU
remains untested. [V14 evidence](demo/dg2-scratch-retain-windows-2026-11-03.json)
and [exact source adoption](demo/dg2-scratch-retain-main-windows-2026-11-03.json).

A separate C0 single-SBA candidate cuts **278 to 139 SBA commands** per warmed
Short, with all other packet counts and mathematical walker order unchanged.
Opposite orders give **+0.279% / -0.393% decode** versus v14; TTFT also changes
direction. It stays **isolated**, with no established throughput or TTFT gain.
All 171 outputs, Windows checks and Linux build/three CPU checks pass; Linux
GPU remains untested. [Single-SBA evidence](demo/dg2-single-sba-windows-2026-11-03.json).

A further isolated close-time compaction trial removes **1,863 duplicate GPU
dependencies** per warmed Short, keeping 136 submissions and 40,778 walkers.
Opposite orders give **-1.650% / +1.926% decode** versus frozen v14; TTFT also
reverses. It is not adopted. All 171 outputs, Windows checks and Linux build /
three CPU checks pass; Linux GPU remains untested. Concurrent live local-ID
caching is excluded. [Compaction evidence](demo/dg2-barrier-compact-windows-2026-11-03.json).

Same-process ABBA/BAAB further tests dependency compaction with shared model
allocations and paired requests: **-0.238% / -0.431% median paired decode**,
with no gain or adoption. All 132 new outputs and 30 Windows checks pass.
Actual captures verify both selected streams; 97 mathematical lists match
byte-for-byte after compaction, while 39 memory-only lists have dynamic pointers.
This diagnostic excludes concurrent live local-ID caching and adds no Linux
GPU or matched-L0 claim. [Paired evidence](demo/dg2-barrier-abba-windows-2026-11-03.json).

A separate same-process trial overlaps only independent consecutive GPU copies.
Its two paired decode medians are **+0.219% / +0.559%**; 132 exact outputs and
30 Windows checks pass. Actual commands omit **620 dependencies** while retaining
136 submissions / 40,778 walkers, with every captured executed byte independently
verified. This small dual-buffer result stays isolated; it establishes no stable
production or native-over-L0 win, and adds no Linux validation. Concurrent live
local-ID/MTP changes are excluded. [Copy overlap evidence](demo/dg2-copy-parallel-windows-2026-11-03.json).

A further native-cache experiment passes **5,760 samples / 342 full-buffer
checks**, but shows no consistent model-sized H2D improvement. Bypassing L1 or
using L1 streaming with L3 cached changes only decoded native load-cache bits.
Full non-temporal reads help some batched D2D cases but
also change compiler scheduling. **No default change or full-model speedup is
claimed.** Linux remains untested. [Cache-policy evidence](demo/dg2-copy-cache-windows-2026-11-03.json).

Reducing copy-grid caps also gives no consistent model-sized H2D gain.
Two dedicated null controls pass alongside those tests: even replaying the
same list under both labels gives **+0.494% / +3.635%** in the two phases of
one H2D case. These observations qualify small deltas, including the earlier
cache results; they cannot all be isolated causal cache costs. All **7,680 new
samples / 456 full-buffer checks** pass. Defaults stay unchanged, with no new
full-model or native-over-L0 claim. [Grid and null controls](demo/dg2-copy-groups-windows-2026-11-03.json).

An isolated Windows BCS extension now bypasses EU copy kernels with raw
`XY_BLOCK_COPY_BLT` on Copy node 2. An installed-driver capture and 16-way
matrix isolate **MOCS 2** as the correction for the original first-byte failure;
all eight corrected configurations pass **73 full-buffer checks each**.
Reusable batches, GPU queue dependencies, host gates and guards pass another
**7,680 samples / 456 full-buffer checks / 24 cross-engine checks**.
At 1,510,400 bytes × 32 H2D, observed gains versus native EU copies are
**+1.307% / +2.521%** with ordered lists and **+1.129% / +4.197%** with explicit
dependencies. Matching same-list nulls and mixed D2D results limit interpretation.
These are standalone frozen-v14 Windows stages, with **no full-model or
native-over-L0 win, production adoption or Linux GPU claim**.
[BLT correctness, timings and null controls](demo/dg2-blitter-windows-2026-11-03.json).

Actual frozen-model Short integration now observes **+0.460% / +0.773% decode
versus matched L0**, and **+1.093% / +1.724% versus native EU copies**, in
opposite orderings. All **261 complete outputs** match. Ten clean process
arms each discard ten warmups and retain ten requests; decode excludes
initialization, graph preparation and prefill. TTFT is not improved. Reverse
L0 control drift is -1.329%, so a stable native backend advantage remains
unproved. Actual commands move **640 EU copy kernels to raw BLT**, retaining
136 submissions. EU/BCS logical bytes and copy counts match in all eleven
paired diagnostic requests; uncaptured L0/EU/BCS requests each record **130
application mutex acquisitions and zero SRW acquisitions**. Capture-request
lock counts are excluded because capture changes execution and worker timing.
Queue 8 was audited as copy-only; that experiment's fixed-ordinal switch is
diagnostic. The follow-up below wires the adapter API. Production defaults and sources
are unchanged, and Linux GPU remains untested.
[Short measurements, raw packets and reproducible checks](demo/dg2-bcs-short-windows-2026-11-03.json).

The next frozen Windows candidate honors explicit `copy_engine` roles, including
both single-GPU adaptive-refill callsites, with all **80 Host translation units**
rebuilt consistently and no fixed queue ordinal. Raw BCS fills pass **1,632
checks** without EU kernels. The two clean orderings measure **49.888 / 49.827
tok/s**, **+2.172% / +1.064% versus native EU copies** and **+1.505% / +0.244%
versus matched L0**. L0 drift is -2.918% / -0.717%, and TTFT changes direction;
a stable native-over-L0 advantage remains unproved. All **256 complete outputs**
match. Actual commands replace 640 EU copy walkers with raw BLT, retaining 136
submissions and matching all eleven logical-payload pairs. Uncaptured EU/BCS
requests record **130/124 imported mutex acquisitions**, zero SRW acquisitions;
I/O worker timing affects totals, so this does not prove lock removal. Decode
excludes preparation/init/prefill. Production sources/defaults and Linux GPU
validation are outside this stage.
[Copy-role API and native fills](demo/dg2-copy-api-windows-2026-11-03.json)
and 1,539 archived evidence files（本地归档：`demo/dg2-copy-api-windows-2026-11-03.json.gz`）.

A further isolated trial moves each ordering flush after the disjoint BLT
rectangles of one logical copy. Actual **MI_FLUSH_DW counts fall 1,320 to 680**,
with identical transfers, 136 submissions and 40,138 walkers. Opposite Short
orderings give **-0.100% / -0.029% decode versus previous BCS**, so this change
is **not adopted**. The reverse apparent **+1.663% versus L0** is below its
**+2.718% control drift**; TTFT also changes direction. All **250 complete
outputs**, **1,632 fill checks**, **7,680 standalone samples / 456 guarded
cases / 48 cross-engine checks** pass. This removes commands without an
established model gain; no production or Linux GPU change is claimed.
[Flush experiment and all results](demo/dg2-blt-flush-windows-2026-11-03.json)
and 1,081 archived evidence files（本地归档：`demo/dg2-blt-flush-windows-2026-11-03.json.gz`）.

A calibrated CS/BCS diagnostic now pairs every submitted list with its completed
span. Both engines pass gated clock/ordering checks at **19.2 MHz**, using only
their common lower 32 bits. All **88 calibration spans** and **11 exact Short
outputs** pass. The final request has **116 CS + 20 BCS submissions**: BCS spans
total **104.867 ms**, with **99.646 ms intersecting CS (95.022%)**, **5.221 ms
outside captured CS**, and **29.303 ms outside both captured span sets**.
These envelopes include waits and context scheduling; they do not establish
continuous engine activity, removable critical-path time or clean throughput.
The isolated diagnostic main exports after GPU drain, before intentional
`_Exit`; live sources/defaults and Linux GPU validation are outside its scope.
[Timeline report](demo/dg2-bcs-timeline-windows-2026-11-03.json),
706 evidence files（本地归档：`demo/dg2-bcs-timeline-windows-2026-11-03.json.gz`）
and [execution-envelope plot](demo/dg2-bcs-timeline-windows-2026-11-03.svg).

A further coarse module-family profile accounts for **40,138 kernel calls**
across **116 CS lists** in its final Short. IQ kernels, fused GR and native
MMVQ occupy **633.826 / 349.656 / 346.560 ms**, or **77.022%** of summed CS
envelopes. All 11 exact outputs and integer block partitions pass. Its
**27,636 added timestamp markers** perturb caches and scheduling, so these
are attribution targets, not clean rates or pure kernel costs. No source
adoption, native-over-L0 win or Linux GPU claim is added.
[Module-family blocks](demo/dg2-kernel-blocks-windows-2026-11-03.json)
and 452 evidence files（本地归档：`demo/dg2-kernel-blocks-windows-2026-11-03.json.gz`）.

Direct walker post-sync profiling further resolves **40,138 kernels**, with
all **11 exact Shorts**, **88 calibration spans** and **1,196 standalone
timestamp packets** passing. The largest individual summed envelopes are IQ
`gu_pair_wg128` **285.406 ms**, GR Q8 up **200.750 ms** and IQ down
`native_down_port<42,4,16,true,256>` **174.845 ms**. GR Q8 up declares
**40,192 private / 4,096 scratch bytes per hardware thread**, an optimization
lead requiring separate validation. Kernel intervals overlap; sums are not
wall-time partitions or utilization, and raw context ticks are not active EU
cycles. Post-sync writes/flushes still perturb execution even without added
per-kernel timestamp commands. This adds attribution, not a clean speed claim.
[Hardware kernel report](demo/dg2-kernel-timestamps-windows-2026-11-03.json)
and 464 evidence files（本地归档：`demo/dg2-kernel-timestamps-windows-2026-11-03.json.gz`）.

An isolated Q8-up candidate narrows captured arguments **1,256 to 728 bytes**
and compiler private memory **40,192 to 23,296 bytes (-42.038%)**, with
unchanged SIMD32/128 GRFs and reported spill. All **64 T=1..8 byte-output
checks and 180 fresh Shorts** pass. With identical candidate math on both
backends, native beats L0 **49.231 / 48.143 tok/s (+2.261%)** in forward order,
then **48.857 / 48.660 tok/s (+0.405%)** in a reverse L0/native/L0 bracket.
Reverse L0 drift is +0.250%, and TTFT changes direction. This supports a small
observed Short advantage, not a stable 2% claim. Decode excludes preparation,
initialization and prefill; ten warmups per process are discarded. The frozen
older norm baseline and fixed MTP4/step-last0.5/cumulative0 policy are explicit.
No production/default adoption or Linux GPU result is added.
[Q8-up controls and reproduction](demo/dg2-hc-up-capture-windows-2026-11-04.json)
and 463 evidence files（本地归档：`demo/dg2-hc-up-capture-windows-2026-11-04.json.gz`）.

A follow-up uses **capacity1/4 captures with eight-row fallback**. Private
requirements fall from 23,296 to **3,584 / 12,032 bytes**; scratch stays 4,096
bytes. All **64 byte-output/guard checks and 180 exact Shorts** pass, with
all 1,242 existing model zebins retained. Incremental native/L0 decode changes
are **-0.340% / -1.875%**, alongside **-0.341% / +2.445%** control drift.
The same new math gives native **+0.323%** versus L0 forward, then **50.145 /
50.140 tok/s (+0.010%)** in reverse, below **+0.251%** drift. No stable gain
or default adoption is established. The preceding frozen policy and decode
timing exclusions apply.
[Capacity measurements and reproduction](demo/dg2-hc-up-capacity-windows-2026-11-04.json)
and 484 evidence files（本地归档：`demo/dg2-hc-up-capacity-windows-2026-11-04.json.gz`）.

A separate **IQ paired-GU format split** reduces selected entry code from
61,824 to **38,464 / 23,936 bytes**, retaining SIMD16/128 GRFs/zero spill and
the original 4/2/1 batching. All **384 canonical old/new cases and 180 exact
Shorts** pass. Native/L0 candidate decode is **-0.069% forward**, then
**50.258 / 49.920 tok/s (+0.676%)** in reverse, with +0.312% L0 drift.
The forward native/L0 incremental ablations are **-0.565% / +0.070%**.
This establishes no stable gain or default adoption. All original 1,242
model zebins remain; total pack size grows by two images. The frozen policy,
decode timing exclusions and unlocked-clock limits above apply.
[Format-split measurements and reproduction](demo/dg2-iq-pair-format-windows-2026-11-04.json)
and 476 evidence files（本地归档：`demo/dg2-iq-pair-format-windows-2026-11-04.json.gz`）.

A separate paired-GU GPU census finds **1,323 length-three groups out of
17,882** in the last Short. A three-entry tail could avoid **6.889% of logical
weight-decoding traversals** versus 4/2/1; this is not a speed or physical
traffic measurement. All 192 predicted histograms, 192 canonical cases and
11 exact Shorts pass. The extra diagnostic dispatches and waits are excluded
from performance comparisons. Original math images and policy stay fixed.
[Group counts and reproduction](demo/dg2-iq-group-hist-windows-2026-11-04.json)
and 192 evidence files（本地归档：`demo/dg2-iq-group-hist-windows-2026-11-04.json.gz`）.

The subsequent **three-entry tail reuse (E3)** observes native **49.571 to
49.815 tok/s (+0.492%)**, with +0.078% native-control drift; L0 changes -0.082%.
Same E3 math leaves native **0.170% / 0.176% below L0** in forward/reverse
orderings. The reverse bracket does not repeat the incremental E3 ablation.
All **384 canonical cases and 180 exact Shorts** pass. Both entries retain
128 GRFs and zero spill, while selected code grows to 52,096 / 32,384 bytes.
E3 stays isolated and defaults off; no stable incremental or backend win is
established. Frozen policy, decode timing exclusions and unlocked-clock limits
above apply; the diagnostic histogram is absent from performance arms.
[E3 measurements and reproduction](demo/dg2-iq-pair-e3-windows-2026-11-04.json)
and 485 evidence files（本地归档：`demo/dg2-iq-pair-e3-windows-2026-11-04.json.gz`）.

A further **shared E3/E4 body** reduces selected text **52,096 to 38,784 bytes**
and **32,384 to 24,192 bytes**, retaining 128 GRFs, zero spill and the same
4/3/2/1 grouping. Two Short backend orderings observe native decode leads:
**50.276 / 49.990 tok/s (+0.572%)** forward and **50.315 / 50.125 (+0.377%)**
reverse, with reverse L0 drift -0.119%. Native's incremental gain over E3 is
+0.511% in one bracket (native drift +0.208%); the reverse does not repeat that
ablation. TTFT remains 0.055% / 0.397% longer than L0. All **384 canonical
cases and 180 exact Shorts** pass, including capacity-ending three-entry tails.
The new switch is cached once/defaults off; the candidate stays isolated.
Frozen policy, timing exclusions and unlocked-clock limits above apply.
[Shared-body results and reproduction](demo/dg2-iq-pair-compact-windows-2026-11-04.json)
and 499 evidence files（本地归档：`demo/dg2-iq-pair-compact-windows-2026-11-04.json.gz`）.

A separate native **compact/E3/compact** incremental repeat observes
**50.267 versus 50.127 tok/s (+0.279%)**, with shared-body drift **+0.017%**.
Together with the earlier +0.511%, both incremental orderings observe a small
decode improvement over E3. All **60 additional Shorts** match exact tokens and
draft counts. TTFT changes **+0.071%**, so no TTFT benefit is established.
This repeat uses the exact previous executables and matching packs; it does
not compare native against L0 or change defaults. Timing exclusions, fixed
policy and unlocked-clock limits remain the same.
[Independent repeat and verification](demo/dg2-iq-pair-compact-increment-windows-2026-11-04.json)
and 19 evidence files（本地归档：`demo/dg2-iq-pair-compact-increment-windows-2026-11-04.json.gz`）.

The shared body is now **integrated as an opt-in** in the numerical source.
Rebuild Host objects and images, then set **`ORCA_IQ_PAIR_COMPACT=1`**; unset/0
keeps the original 4/2/1 path. The direct original-GU/shared-body comparison
measures **50.234 / 50.255 tok/s (+0.041%) native** and **49.940 / 50.003
(+0.127%) L0**, with control drift -0.109% / -0.089%. This establishes no
overall speed gain, so defaults stay unchanged. Same-new-math native/L0 is
**+0.502% decode**, with native TTFT **0.584% longer**, in one ordering.
All **120 fresh Shorts**, **384 canonical buffers**, the expanded existing
checker's **1,152 comparisons**, and **252 generic-fallback comparisons** pass.
Native/SYCL retain 252/261 old ABIs and add two entries; four current-header
SYCL Host/device compiles pass. The two new machine-code texts exactly match
the preceding prototype. Only the IQ source and existing checker are adopted;
runtime sources, production executables/configuration and Linux bundles remain
unchanged. Linux GPU is untested.
[Adopted-source validation and reproduction](demo/iq-compact-source-windows-2026-11-04.json)
and 528 evidence files（本地归档：`demo/iq-compact-source-windows-2026-11-04.json.gz`）.

V14 retains **v13**, preallocating Windows bootstrap/math code
slabs before tensors. This removes **268 instruction-window changes and full
state preambles** per warmed Short, with **1,004 to 468 SBA commands** and
unchanged walkers/submissions. Opposite Short orderings give **+0.473% /
+0.224%** decode versus v12 controls, close to drift; the first candidate is
still **0.637% below matched L0**. All 171 clean/diagnostic outputs match.
Linux allocation behavior is unchanged. [V13 measurements](demo/dg2-early-isa-windows-2026-11-03.json)
and [exact source adoption](demo/dg2-early-isa-main-windows-2026-11-03.json).

V13 retains **v12**, reducing Windows DG2 ISA tail padding from
4,096 to 512 bytes. Opposite Short orderings observe **+1.267% / +0.607%**
decode versus native controls. The first bracket remains **0.686% below matched
L0** (49.196 / 49.536 tok/s); a native backend win is still unproven. All 160
performance requests match complete output and drafts. Linux keeps 4,096-byte
padding pending GPU testing. [Padding evidence](demo/dg2-isa-padding-windows-2026-11-03.json)
and [v12 source checks](demo/dg2-isa-padding-main-windows-2026-11-03.json).

V12 retains v11's batched local-parameter mirrors for the calibrated Windows
A770 C0 backend. Production binaries/configuration are preserved. Linux parameters
remain host-backed and await GPU testing. It also retains v10's optional
co-aligned copy kernel.
It peels at most 15 bytes, caches module support at binding and selects the
kernel at recording; aligned copies keep their original execution body.
Unaligned D2D GPU time falls **56.62%-67.46%** versus the old native path in the
three tested sizes. Aligned 96-KiB D2D is **20.68% faster than L0** in the same
standalone bracket; this is not a full-model gain. A serial rerun of both
backends completes 22 exact Short requests: all 11 paired logical payload
censuses match, with **134 D2D / 4,554,752 bytes** in each final warmed request.
Separate verified application mutex probes are **130/130**, with **0/0 runtime
mutex/SRW acquisitions**. Background I/O remains enabled; its lock totals can
change with request duration.
The historical v10 Short comparison is **46.759 / 46.659 / 48.171 tok/s** for
native control / v10 / L0: v10 is -0.21% versus native (control drift +0.12%)
and still **3.14% below L0**. All 133 clean/counter outputs, Windows seven
checks, Linux three CPU checks and 1,840 candidate/legacy-module transfer
replays pass. Linux GPU is untested. Source is adopted; production binaries
and configuration are preserved. A new five-export memory pack enables the
path; old packs retain their prior behavior.
[Copy samples and ISA](demo/dg2-copy-throughput-windows-2026-11-03.json),
[model/counter results](demo/dg2-copy-prefix-short-windows-2026-11-03.json),
[same-session copy parity and separate lock reference](demo/dg2-copy-lock-parity-windows-2026-11-03.json),
[historical v10 source](demo/dg2-copy-prefix-main-windows-2026-11-03.json),
[historical v11 source](demo/dg2-local-parameters-current-main-windows-2026-11-03.json),
[historical v12 source](demo/dg2-isa-padding-main-windows-2026-11-03.json),
[historical v13 source](demo/dg2-early-isa-main-windows-2026-11-03.json),
[frozen v14 source](demo/dg2-scratch-retain-main-windows-2026-11-03.json).
Read-only frozen-evidence check:
`python build-orca-native-lower-20261031/publish_dg2_scratch_retain.py --verify`.
Concurrent local-ID payload-cache edits are outside these frozen comparisons.
Earlier exact-live-source verifiers, including v14, are historical after those edits;
their frozen evidence stays intact.

A new diagnostic compares GDN through both runtimes on the **same local data
pages and GPU virtual address**. Small T=1 worksets still favor native; 16
independent slots retain a **3.59% / 3.68% longer** native GPU interval in two
pure-GDN bracket orderings. Data allocation/address differences alone do not
remove the gap. All 384 guarded cases, 11,520 retained samples and 122
cross-runtime update checks pass. Native internal allocations/contexts still
differ, and T=4 controls show more variation. This is a standalone diagnostic,
not a new model speed result or production import API. Source stays V10.
[Same-pages and same-address results](demo/dg2-shared-pages-windows-2026-11-03.json)
include the resolved handle-access failure and all raw evidence.

A current-V10 readback also follows the actual entry addresses of 16 completed
GDN launches per backend. The complete **30,592-byte GPU-resident text matches
byte for byte** between native, L0 and zebin, with the same 192-byte prologue
skip. All 64 GDN cases, independent reader checks and seven Windows checks
pass. This extends the earlier loaded-pack proof to an executed fixture;
it adds no performance claim and leaves the diagnostic API outside production.
[Executed-address evidence](demo/dg2-executed-isa-windows-2026-11-03.json).

Further controls fix the native queue, code allocations and shared data
pages/VA. Combining L0's L1/SLM field values gives no gain: T=1 large pure GDN
takes **0.89% / 0.71% longer** in two opposite orderings. Separately, clearing
only extra native timestamp flush bits saves **0.21% / 0.16%** in that case;
native still takes **3.32% longer than L0**. All ordinary/queue-tail flushes
remain. The 448 guarded case runs pass, with exact packet-change proofs and
frozen output hashes. Neither diagnostic changes production source or model
speed; marker-free clean Short remains 46.659 versus 48.171 tok/s.
[Cache/SLM cross](demo/dg2-cache-slm-cross-windows-2026-11-03.json),
[timestamp calibration](demo/dg2-timestamp-scope-windows-2026-11-03.json).

A context crossover executes the **identical native batch, code allocation,
parameter heap and data pages/VA** on a real L0 context, retaining its original
submission metadata and completion tail. Large pure T=1 GDN still takes
**3.24% / 3.34% longer** than ordinary L0 in opposite bracket orders. Separately,
clearing all four initial invalidation bits after a complete prime saves only
**0.013% / 0.132%**, leaving a **3.27% / 3.18%** gap. Neither control explains
the remaining large-workset difference. All 448 guarded cases / 13,440 samples
pass with the original scalar oracle and frozen output hashes. Every arm has
one GRF128 normalization kernel after its end timestamp, excluded from the
measured GPU interval. Source stays V10; model throughput and the separate
134/134 D2D, 130/130 imported-lock census are unchanged.
[Exact scope and 746 archived evidence files](demo/dg2-context-cross-windows-2026-11-03.json).

A parameter-placement follow-up explains most of that large GDN fixture gap.
Keeping the original parameter offsets and reuse, but reading byte-identical
parameters from local VRAM, reduces large pure T=1 GPU time **2.92% / 2.85%**
in opposite orders; the remaining L0 gap is **0.28% / 0.37%**. Mixed T=1 becomes
**1.57% / 1.56% faster than L0**, while small pure GDN stays within 0.03% of
native controls. Only general SBA base and walker indirect offsets change;
code allocation, shared data pages/VA, context and all other packets remain.
All 576 guarded cases / 17,280 samples pass. This standalone result motivated
the current-V10 model trial below; it does not itself establish model
throughput or change production defaults.
[Parameter controls and 837 evidence files](demo/dg2-parameter-placement-windows-2026-11-03.json).

Rebasing the existing batched mirrors onto frozen V10 gives a repeated
**+0.982% / +0.984% clean Short decode gain** versus a matched host-parameter
build in two opposite bracket orders. Native local parameters reach
**46.947 / 47.020 tok/s**, compared with **47.671 / 47.702 tok/s for L0**;
native remains **1.52% / 1.43% slower**. All 200 complete outputs and draft
counts match. Both variants pass 15 Windows checks and eight GPU lifecycle
groups each; Linux compiles with host parameters and passes three CPU checks.
New parameter ranges upload by slab at close, with no immutable replay
uploads. This isolates the local-parameter policy between the new builds,
not the combined patch against unchanged V10. TTFT changes direction between
the two runs; no stable prefill gain or full-model win is established.
An additional 60-request bracket against **unchanged V10** measures
**46.409 vs 47.036 tok/s (+1.350%)**, with -0.697% control drift. The patch is
now adopted as V11; production binaries/configuration are preserved.

Separate fresh payload and lock probes confirm **134/134 D2D operations**,
**4,554,752 bytes** and **130/130 imported application mutex acquisitions**,
all I/O, with **zero runtime mutex/SRW acquisitions**. All eleven logical
payload pairs match; keepalive stays enabled. These are application counters,
not physical bus transactions or private driver locks. All **304 complete
requests** pass across the three clean brackets and counter runs.
[First bracket and 309 source/check evidence files](demo/dg2-local-parameters-current-windows-2026-11-03.json),
[follow-up brackets and fresh counters](demo/dg2-local-parameters-current-repeat-windows-2026-11-03.json),
[V11 adoption](demo/dg2-local-parameters-current-main-windows-2026-11-03.json).

A subsequent **100-request command-buffer placement control** finds no gain
from mirroring large command lists into local VRAM. V11 host commands / local
commands / L0 measure **47.507 / 47.374 / 48.172 tok/s**. The candidate's
**-0.279%** decode change is within **-0.471%** native control drift; TTFT is
894.888 / 897.092 / 891.801 ms. All full outputs and draft counts match.
Fifteen Windows checks, 24 guarded placement replays on two queues, 64 inflight
recording submissions and three numerical groups pass. No replay uploads are
added, but close needs an extra mirror/upload. **The candidate is not adopted**;
V11 defaults and Linux remain unchanged. These are unlocked-clock Short
observations, with preparation/init/prefill excluded from decode.
[Results and scope](demo/dg2-local-commands-windows-2026-11-03.json),
raw protocols, runner and frozen candidate（本地归档：`demo/dg2-local-commands-windows-2026-11-03.json.gz`）.

A separate **100-request tail-barrier control** also establishes no gain.
V11 / candidate / L0 measure **47.522 / 47.359 / 48.357 tok/s**, with TTFT
894.906 / 897.651 / 886.212 ms. Removing the adapter's duplicate tail barrier
changes decode **-0.343%**, with +0.301% native control drift. All outputs and
draft counts match; 80 Host objects are rebuilt, Windows 15 checks, both actual
adapter batch limits and Linux three CPU checks pass. **The candidate is not
adopted**; V11 remains current and Linux GPU testing is deferred. Decode
excludes preparation/init/prefill. No new operator overlap or copy/lock count
is claimed. [Result](demo/dg2-tail-barrier-windows-2026-11-03.json) and
364 verified evidence files（本地归档：`demo/dg2-tail-barrier-windows-2026-11-03.json.gz`）
retain the exact changes, build/check commands and raw protocols.

A **100-request shared/routed stage trial** exposes three independent
intervals while retaining their internal dependencies and driver barriers.
Its format gate covers 43/48 layers. Both batch settings pass **576 exact
full-buffer/guard comparisons** at T=1/4/8 with changing inputs/plans and graph
replay. Serial / interleaved / L0 measure **47.526 / 47.766 / 48.423 tok/s**;
TTFT is 894.353 / 892.212 / 886.939 ms. The observed **+0.505%** decode change
is below the **+0.741%** native control drift, so no speedup is established
and **the candidate is not adopted**. Native still trails L0 by **1.36%**.
Both native modes use one executable/pack, the same 5,771 slots, 1,536 MiB
reserve, MTP4 and 0.5 draft thresholds; every response matches all 76 tokens
and 57/57 drafts. Decode excludes preparation/init/prefill, with no inner
timers or GPU telemetry. Eight changed ISA hashes contain only post-EOT
`mov null` immediate differences; computation/resource metadata is unchanged.
Linux GPU and physical concurrent execution are unverified.
[Results](demo/dg2-expert-overlap-windows-2026-11-03.json) and
478 checked evidence files（本地归档：`demo/dg2-expert-overlap-windows-2026-11-03.json.gz`）.

A matching-stage follow-up pairs shared/routed projections, activations and
Down stages. In its first clean bracket, serial / interleaved / matching / L0
measure **47.684 / 48.157 / 48.576 / 48.626 tok/s**. Matching improves native
serial **1.871%**, versus +0.141% serial control drift; native remains **0.102%
below L0**. Both batch settings pass **864 exact full-buffer/guard checks**.
All 260 expert exports retain the preceding trial's ISA and resource/capture
metadata; only two Host objects are rebuilt. The isolated candidate covers
43/48 format-eligible layers at T<=4; **V11 defaults remain unchanged**.
An unchanged-binary reverse bracket gives **46.799 serial / 47.936 matching /
47.296 L0 tok/s**. The matching point estimate is +2.429% versus serial and
+1.354% versus L0, but matching/L0 controls drift -3.128%/-4.711%.
This does **not** establish a repeatable native win; the candidate remains
experimental. The user subsequently reported other application use as a
possible confound; resource contention was not measured. Raw evidence is kept.
All **240 requests** across both brackets match every output
token, stop and 57/57 drafts. Settings remain 5,771 slots, 1,536-MiB reserve,
MTP4, step/last min-p 0.5, cumulative zero and no prefix reuse. Decode excludes
preparation/init/prefill; no inner timers or GPU telemetry run in the brackets.
[Both results and limitations](demo/dg2-expert-fork-windows-2026-11-03.json),
79 verified evidence files（本地归档：`demo/dg2-expert-fork-windows-2026-11-03.json.gz`）.

Adding router-GEMV/input-quantization overlap now yields two Short brackets
ahead of the existing L0 baseline, after the user confirmed an idle desktop:

| Same fixed policy | Optimized native | Existing L0 | Native gain |
| --- | ---: | ---: | ---: |
| Five-arm bracket | 47.870 tok/s | 47.263 tok/s | +1.283% |
| Reverse bracket | 47.735 tok/s | 46.616 tok/s | +2.400% |

The router/quantization increment is **+0.842%** over matching-only native
in the first bracket. Native/L0 control drift is -0.265%/+0.326%; reverse
native drift is +0.143%. Sample dispersion and every retained result are
archived; these are observations, not confidence intervals or fixed gains.
All **160 model requests**, **1,280 full-buffer comparisons** and **1,920 GPU
tail epochs** pass. A subset scalar GEMV oracle has maximum error 2.235e-8;
quantization uses cross-schedule byte parity. One Host object changes and the
exact preceding device pack is reused. Decode excludes prep/init/prefill,
TTFT stays separate, and no prefill gain is established.

The L0 baseline lacks both new schedules, so this compares complete
implementations rather than isolating API-layer cost. The matched-schedule
control below resolves this comparison; current source integration follows it.
**V11 defaults remain unchanged**;
the candidate is experimental and Linux GPU execution is untested.
[Both brackets, parameters and limits](demo/dg2-router-quant-windows-2026-11-03.json),
66 verified evidence files（本地归档：`demo/dg2-router-quant-windows-2026-11-03.json.gz`）.

With **identical expert/router schedules**, native DG2 is still slower than
L0 in both clean Short brackets on Windows A770 C0, driver 32.0.101.9034:

| Same device pack and fixed policy | L0 matched schedule | Native matched schedule | Native versus L0 |
| --- | ---: | ---: | ---: |
| Five arms, 100 requests | 48.466 tok/s | 47.643 tok/s | **-1.697%** |
| Reverse, 60 requests | 48.323 tok/s | 47.636 tok/s | **-1.423%** |

The L0 control rebuilds all 80 Host objects consistently, without recompiling
device code. Its new schedule improves **47.212 → 48.466 tok/s (+2.655%)** in
the first bracket. The earlier whole-version native wins therefore do not
establish a speed advantage from bypassing Level Zero. L0 original/matched
control drift is -0.304%/-0.385%; reverse native drift is +0.326%. These are
complete-backend comparisons, not measurements of one API call's cost.
All **160 exact model requests**, **2,144 full-buffer comparisons** and
**2,952 GPU tail epochs** pass. Decode excludes preparation/init/prefill;
TTFT is separate, and no inner timers or GPU telemetry run during the model
brackets. Sample dispersion, exact parameters and raw requests are retained.
This historical experiment left the schedules experimental; production binaries
and Linux handoffs were unchanged. Linux GPU execution is still untested.
[Matched results and limits](demo/l0-matched-schedule-windows-2026-11-03.json),
645 verified evidence files（本地归档：`demo/l0-matched-schedule-windows-2026-11-03.json.gz`）.

The schedules are now **integrated and enabled by default in engine source**
for audited single-group windows of at most four rows, on both Orca L0 and DG2.
Other paths retain the original schedule. `STRATA_VERIFY_EXPERT_FORK=0` disables
matched expert stages; `STRATA_VERIFY_ROUTER_QUANT=0` disables router/quantization
overlap. Ordinary SYCL and CUDA/HIP public signatures are preserved. The three
ordinary SYCL files compile, and a manual `strata-orca-expert-schedule-check`
target passes **1,728 full-buffer comparisons / 2,064 GPU tail epochs** across
both backends with batching off/on.

| Integrated source acceptance | L0 original | L0 default | DG2 default | DG2 versus matched L0 |
| --- | ---: | ---: | ---: | ---: |
| Five arms, 100 exact Short requests | 47.382 tok/s | 47.917 tok/s | 47.942 tok/s | +0.052% |
| Reverse, 60 exact Short requests | Not measured | 48.710 tok/s | 48.342 tok/s | -0.756% |

The first L0 off/on bracket gains **1.129%**; its original/default control
drift is +0.393%/-0.198%. Reverse DG2 control drift is **+1.946%**, so the small
backend differences do not establish a stable native win. All 160 complete
76-token outputs, natural stops and 57/57 drafts match. Only the three current
scheduling files are rebuilt per backend; the other 77 Host objects, equal-work
main, V11 runtime and device pack stay frozen. This preserves ongoing MTP work
and is not a full latest-engine benchmark. Policy stays 5,771 slots, 1,536-MiB
reserve, MTP4, step/last min-p .5 and cumulative zero. Decode excludes
preparation/init/prefill; TTFT is separate. First-bracket original/default L0
TTFT is 903.769/910.874 ms; no prefill improvement is established. Source defaults
are adopted, while production executables/configuration and immutable Linux
handoffs are preserved. Linux GPU testing is pending.
[Integration details and dispersion](demo/expert-schedule-integration-windows-2026-11-03.json),
258 verified evidence files（本地归档：`demo/expert-schedule-integration-windows-2026-11-03.json.gz`）.

The next runtime-only comparison reduces zeroed ISA tail padding, keeping the
existing slabs, mathematical code and both matched engine schedules fixed:

| Clean Short | Native 4-KiB padding | Native 512-B padding | Matched L0 | Native padding gain |
| --- | ---: | ---: | ---: | ---: |
| First, bracketed controls | 48.580 tok/s | 49.196 tok/s | 49.536 tok/s | +1.267% |
| Reverse, compact controls | 48.809 tok/s | 49.105 tok/s | Not measured | +0.607% |

First native/L0 control drift is +1.047%/+0.477%; reverse compact drift is
+0.052%. The remaining first-bracket L0 deficit is **0.686%**. TTFT is
899.012/890.048/886.718 ms for native control/compact/L0 in the first bracket,
890.600/888.173 ms for reverse control/compact; no stable TTFT improvement is
established. Each arm uses ten warmups plus ten retained requests. All 160
outputs match 76 tokens, natural stop, 57/57 drafts and zero reuse, with the
same 5,771 slots, 1,536-MiB reserve, MTP4 and min-p .5/.5/cumulative zero.
Decode excludes prep/init/prefill, with no inner timers or GPU telemetry.
Clocks are unlocked; different rounds' rates and gains are not combined.

Two separate diagnostic requests load the same **109 kernels**. Captured ISA
bytes fall **3,960,576 to 3,569,920**, while retained storage stays **5 MiB**
(4 MiB local VRAM). This saves 390,656 bytes inside the existing slabs and
changes code placement; no released VRAM or cache-miss cause is established.
Pinned NEO DG2 source uses 512-byte default padding; the installed driver
policy is not asserted identical. The two diagnostic timings are excluded.

Both variants pass 15 Windows checks each and a combined 1,728 full-buffer
comparisons / 2,064 tail epochs. The adopted Windows-only **v12** branch matches
the measured candidate, while Linux keeps the 4-KiB control. The exact adopted
stage additionally passes 15 Windows checks, 864 full-buffer comparisons /
1,032 tail epochs, one exact Short smoke request and three Linux CPU checks.
Linux GPU validation remains deferred. Production binaries/configuration,
MTP work and immutable Linux handoffs are preserved.
[Padding results and footprint](demo/dg2-isa-padding-windows-2026-11-03.json),
243 verified evidence files（本地归档：`demo/dg2-isa-padding-windows-2026-11-03.json.gz`）,
[single-source v12 adoption](demo/dg2-isa-padding-main-windows-2026-11-03.json).

A fresh diagnostic captures the eleventh exact Short decode with **v12 and
matched schedules on both backends**. All 22 requests match full output and
draft counts; both command captures have zero decoding failures. Native/L0
execute **136/795 KMT submissions**, **40,778/40,932 walkers**,
**1,004/214 SBA commands** and **34,550/34,281 PIPE_CONTROL commands**.
An independent typed decoder reproduces all counts. Copy/fill implementations
differ, and the observer perturbs execution; no captured timing is a speed
measurement or a fresh payload/lock census.

Native code occupies two serialized 4-GiB instruction windows, with **268
within-file instruction-only base changes**, while captured L0 uses one
instruction base. This motivates an isolated early-code-allocation experiment,
without establishing a causal performance cost. This capture precedes the v13
adoption below. [Fresh packet counts and address fields](demo/dg2-v12-packets-windows-2026-11-03.json),
1,197 verified raw/source/protocol records（本地归档：`demo/dg2-v12-packets-windows-2026-11-03.json.gz`）.

The follow-up now adopts **v13 early code slabs**, changing only
`src/drm/recording.inl`. Windows local ISA reserves 1 MiB for bootstrap code
and 4 MiB for mathematical code before tensors; the existing growth path
handles larger programs. Linux and host-ISA behavior stay unchanged.

| Independent Decode capture | v12 | Early slabs | Matched L0 |
| --- | ---: | ---: | ---: |
| Instruction-only base changes | 268 | **0** | Not inferred across secondary files |
| SBA commands | 1,004 | **468** | 214 |
| CFE / preemption / pipeline / compute-mode programs | 2,894 each | **2,626 each** | Different programming strategy |
| PIPE_CONTROL commands | 34,550 | **34,282** | 34,281 |
| Walkers / KMT submissions | 40,778 / 136 | 40,778 / 136 | 40,932 / 795 |

All candidate commands decode without failure. The 268 removed full preambles
include 536 SBA commands, reducing executed bytes 7,418,376 to 7,351,912.
The capture is diagnostic only; these are not timing, physical-traffic or
new copy/application-lock measurements.

| Clean Short | v12 control | Early slabs | Matched L0 | Candidate versus v12 |
| --- | ---: | ---: | ---: | ---: |
| First | 49.190 tok/s | 49.423 tok/s | 49.740 tok/s | +0.473% |
| Reverse | 49.218 tok/s | 49.328 tok/s | Not measured | +0.224% |

Native control drift is +0.204% in the first bracket; reverse candidate drift
is +0.260%. These small point estimates do not establish a guaranteed gain.
Native remains **0.637% below L0** in the first bracket. TTFT is
891.459/883.625/882.228 ms for first control/candidate/L0 and
885.181/885.610 ms for reverse control/candidate; improvement does not repeat.
A CPU-only archive publication overlaps the first bracket; reverse timing
has no parallel archive/compiler job. Clocks are unlocked.

All **160 performance plus 11 separate diagnostic requests** match 76 complete
tokens, natural stop, 57/57 drafts and zero reuse. Policy stays 5,771 slots,
reserve 1,536 MiB, MTP4, min-p .5/.5 and cumulative zero. Each arm uses ten
warmups and ten retained requests. Decode excludes prep/init/prefill; no inner
timers or GPU telemetry run in performance arms. The exact adopted source
passes 15 Windows checks, 864 full-buffer comparisons / 1,032 tail epochs,
Linux compilation and three CPU checks. Linux GPU remains deferred.

The policy reserves **5 MiB upfront**, including 4 MiB local code; small
programs can retain unused space. Allocation placement and tensor addresses
may change. There is no new full-model retained-code or automatic-cache-size
measurement; the benchmark fixes expert slots explicitly. Production binaries,
configuration, MTP work and immutable Linux handoffs are preserved.
[Timing, packet deltas and caveats](demo/dg2-early-isa-windows-2026-11-03.json),
242 checked evidence files（本地归档：`demo/dg2-early-isa-windows-2026-11-03.json.gz`）,
[v13 source and 12 current hashes](demo/dg2-early-isa-main-windows-2026-11-03.json).

The preceding **v9** adds the measured C0 SBA/invalidation
interaction fix to v8. Two opposite clean Short brackets observe **+0.79% and
+0.41%** decode versus v8 (control drifts +0.25%/-0.19%); native remains
**2.95%/2.87% below L0**. The real mixed-GDN GPU probe improves 50.40% at T=1,
with a repeated small-local advantage over L0 of 2.69%/2.18%. Windows 15 checks,
Linux three CPU checks, eight GPU numerical groups and all 200 full model
outputs pass. Only encoder/checker source is adopted; Linux GPU testing is
deferred and existing binaries/configuration are preserved.
[V9 source and limitations](demo/dg2-sba-invalidation-main-windows-2026-11-03.json).
Its historical verifier expects v9 live source; use the current v13 check above.

**Known desktop issue (2026-11-02):** the user reported cursor flickering while
the native DG2 backend ran and confirmed recovery after it stopped. Short-only
experiments resumed with the user's authorization. The passing numerical checks
below do not establish desktop coexistence. Keep production L0 for normal use.
A staged low-priority context prototype now passes 15 exact model requests,
but has no confirmed desktop fix. Its Short throughput is 38.710 tok/s versus
38.712 for normal native DG2 and 47.734 for production Orca L0. Native uses
about 0.03 CPU cores during decode versus 1.02 for L0; increased CPU use does
not explain this result. CPU accounting observes host message delivery, and
cannot separate GPU execution from completion notification delay.
[75-request Short results](demo/dg2-short-priority-cpu-windows-2026-11-02.json) and
exact runner, sources and logs（本地归档：`demo/dg2-short-priority-cpu-windows-2026-11-02.json.gz`）.
The earlier interrupted heap-window experiment completed **zero candidate
model requests** and remains an incomplete historical comparison.
The later completed Short-only native/candidate/native bracket gives
**38.604 → 40.462 tok/s (+4.81%)** from sharing eligible heap-base windows,
with -0.49% control drift and all 45 outputs exact. TTFT drops 4.27%.
This standalone binary remains a historical control for the combined policy
below; it does not establish a desktop fix or Linux GPU performance.
[Shared-window results](demo/dg2-short-heap-window-windows-2026-11-02.json)
and runner, sources and logs（本地归档：`demo/dg2-short-heap-window-windows-2026-11-02.json.gz`）.
A separate 75-request bracket uses **the same 1,242 zebin inputs** in both
backends. Freshly built current-source L0 reaches **48.705 tok/s**, native DG2
**39.289 tok/s (-19.33%)**, with +0.04% control drift and exact output parity.
Switching that L0 from SPIR-V to zebin changes decode by only -0.09%.
This localizes the remaining gap beyond the input binary format; command/state,
memory access and completion costs remain candidates. This L0 control is neither
the frozen production executable nor original SYCL.
[Same-module results](demo/dg2-short-shared-modules-windows-2026-11-02.json)
and runners, build records and failed attempts（本地归档：`demo/dg2-short-shared-modules-windows-2026-11-02.json.gz`）.
Full polling of WDDM monitored fences gives **39.206 → 39.674 tok/s (+1.20%)**
in a separate 45-request exact-output bracket, with -0.04% decode control drift.
Process CPU rises from about **0.028 to 1.028 cores**. This single bracket does
not establish a stable small gain or recover the main native gap. The candidate
remains diagnostic-only; inline tag waits are unchanged. GPU state and memory
access remain the next priorities. Independent experiment gains cannot be added.
[Fence-polling results](demo/dg2-short-fence-polling-windows-2026-11-02.json)
and source, functional checks and model logs（本地归档：`demo/dg2-short-fence-polling-windows-2026-11-02.json.gz`）.
**Main native DG2 now defaults to shared heap windows plus within-list scratch
retention.** A new native/window/combined/window/native bracket measures
**39.165 → 42.309 tok/s (+8.03%)** net, with -0.23% control drift, 75 exact
Short28/natural76 outputs and unchanged 57/57 drafts. The scratch increment
alone is +6.57% against shared-window controls; TTFT falls **9.61%** net.
These gains are measured together, not added from separate experiments.
The actual main sources pass **21 Windows CTests, three Linux CPU CTests**,
the repaired legacy benchmark's CPU check and an additional exact model smoke.
The production L0 executable/configuration stays unchanged; Linux GPU and broad
PPL remain unvalidated.
[Matched measurements](demo/dg2-short-heap-scratch-windows-2026-11-02.json),
full experiment（本地归档：`demo/dg2-short-heap-scratch-windows-2026-11-02.json.gz`）,
[main adoption](demo/dg2-state-reuse-main-windows-2026-11-02.json) and
main-source checks（本地归档：`demo/dg2-state-reuse-main-windows-2026-11-02.json.gz`）.
The v3 Linux continuation exports 2,003 files with this policy; the exported
source passes three Linux GCC CPU checks and three ELF dependency audits.
[Linux continuation verification](demo/dg2-state-reuse-linux-handoff-cpu-2026-11-02.json)
and manifest, runner and notes（本地归档：`demo/dg2-state-reuse-linux-handoff-cpu-2026-11-02.json.gz`）
preserve its exact source boundary. Real Linux GPU validation remains pending.
**Main native DG2 also keeps cached parameters within the current capture
window.** Two independent Short brackets measure **42.771 to 44.305 tok/s
(+3.59%)** and **42.764 to 44.390 (+3.80%)**, with +0.18%/-0.29% control drift
and all 90 outputs exact. Whole-request time falls about 2.1%; TTFT is effectively
unchanged. A normalized-state diagnostic reduces T=4 parameter-base transitions
from 516 to one and full state blocks from 591 to 131 across the same 1773
dispatches. Counts depend on allocation layout and do not measure GPU time.
Copying an old immutable parameter cache into the current window happens only
during recording, with no extra replay work or numerical/barrier change.
Actual main passes 21 Windows checks, three Linux CPU checks (including runtime
compilation) and another exact model request. Production L0 remains unchanged;
v4 preserves that source checkpoint. Independent percentages cannot be added.
[Two-bracket results](demo/dg2-parameter-window-windows-2026-11-02.json),
full experiment（本地归档：`demo/dg2-parameter-window-windows-2026-11-02.json.gz`）,
[main adoption](demo/dg2-parameter-window-main-windows-2026-11-02.json) and
main-source checks（本地归档：`demo/dg2-parameter-window-main-windows-2026-11-02.json.gz`）.
The v4 Linux export contains 2,007 files and passes three exported-source CPU
checks and three ELF dependency audits; real Linux GPU validation remains pending.
[Linux export report](demo/dg2-parameter-window-linux-handoff-cpu-2026-11-02.json)
and manifest, exporter and notes（本地归档：`demo/dg2-parameter-window-linux-handoff-cpu-2026-11-02.json.gz`）.

A fresh same-1242-zebin Short bracket measures **48.674 tok/s for current-source
Orca L0 versus 44.507 for native DG2 (-8.56%)**, with **+0.32%** control drift.
Both host frontends were rebuilt; 45 release requests and 45 separate diagnostic
requests all match exactly. Parameters retain 5771 slots, min-p=.5 and reserve
1536 MiB, without prefix reuse. Decode excludes initialization/preparation/prefill.
This control is not original SYCL or frozen production.
[Current comparison and stage measurements](demo/dg2-profiler-short-windows-2026-11-02.json)
and source bytes, every request, checks and failed attempts（本地归档：`demo/dg2-profiler-short-windows-2026-11-02.json.gz`）
preserve the evidence. Earlier CPU accounting measured about 0.027 native CPU
cores versus L0's 1.024; saturation alone does not explain the gap. The earlier
[GRF-only experiment](demo/dg2-grf-only-windows-2026-11-02.json) remains unadopted.

The Strata CS profiler now works on both transports. The diagnostic difference
is largest in GDN HC/router, recurrent update and projection intervals; resident
expert compute differs by only about 1–1.5%. Markers add barriers and disable
some independent scheduling: profiler throughput falls 4.80% on L0 and 9.78%
on native, so stage differences are priorities for investigation, not a precise
decomposition of the release gap. A CPU-only check also confirms MSVC rounds
the inline wait's `sleep_for(25us)` to at least 1ms; its idle-process median was
15.51ms. Its effect inside model execution remains unmeasured; defaults are unchanged.

A later isolated **layer-boundary control** retains independent projections and
reduces profiler overhead to **0.79% / 1.38% (L0/native)**. The uninstrumented
Short repeat measures **48.671 / 44.888 tok/s (-7.77%)**, with +0.69% control
drift and 90 exact requests across release/diagnostic brackets. Decode-only GPU
intervals are 64.465 / 71.960ms per window; about 80% of their difference is in
GDN layer spans. Host verifier time outside those intervals is 0.945 / 0.740ms,
which does not support completion wakeup as the dominant loss in that bracket.
GPU intervals include stalls/dependencies, and remaining unequal marker cost
prevents exact release attribution. Main sources and production pins are unchanged.
[Layer control and scope](demo/dg2-layer-profiler-short-windows-2026-11-02.json)
and exact runners, source evidence and discarded attempts（本地归档：`demo/dg2-layer-profiler-short-windows-2026-11-02.json.gz`）.

A further **three-phase control** narrows about 90% of the instrumented GPU
difference to mixer and HC/router/shared stages; experts/tail account for
about 10%. The fresh release repeat is **48.759 L0 / 44.795 native tok/s
(-8.13%)**, with -0.17% control drift. The separate diagnostic bracket adds
1.06% / 2.37% throughput overhead, so its phase shares prioritize investigation
and do not establish exact release attribution. All 90 outputs match; numerical
host objects and the same 1242 zebins are unchanged. Decode excludes loading,
preparation and prefill. Main sources and production pins are unchanged.
[Three-phase measurements and scope](demo/dg2-coarse-profiler-short-windows-2026-11-02.json)
and reproducible source/command/request evidence（本地归档：`demo/dg2-coarse-profiler-short-windows-2026-11-02.json.gz`）.

Two further isolated **GRF transition controls** pass all 75 exact Short
requests, with 14 Windows checks (two CPU and twelve GPU), three Linux CPU checks and five real GPU
numerical groups per variant. Removing unchanged state packets measures
**+0.22%**; narrowing the remaining invalidation adds only **+0.33%**, versus
**+0.98%** control drift. Neither is adopted. A cached NEO source review makes
indirect parameter placement a focused next question: native parameter slabs
are host-backed, while NEO's indirect heap can use local memory. Installed L0
placement and its effect remain unverified. Main and production are unchanged.
[GRF controls and limits](demo/dg2-grf-cache-short-windows-2026-11-02.json)
and all source/check/request evidence and cached placement references（本地归档：`demo/dg2-grf-cache-short-windows-2026-11-02.json.gz`）.

An isolated **batched local-parameter implementation** now passes the original
Windows checks, Linux CPU checks, real numerical checks, 17,000 capture rollover,
900 reclamation and 64 in-flight recording submissions. New parameter ranges
upload by slab at list close; replay performs no upload. A nested 75-request
same-zebin Short bracket measures **44.226 host / 44.429 local tok/s (+0.46%)**;
main controls are 43.971 tok/s. The small one-bracket difference does not
establish a repeatable gain or explain the L0/native gap, so defaults are
unchanged. A buffer-pool interaction found by the original plateau checks was
fixed before model measurement; both failed and corrected sources are retained.
[Placement results and scope](demo/dg2-local-parameters-short-windows-2026-11-02.json)
and all sources, checks and requests（本地归档：`demo/dg2-local-parameters-short-windows-2026-11-02.json.gz`）.
A separate [read-only WDDM probe](demo/dg2-perfdata-capability-windows-2026-11-02.json)
returns zero idle engine-frequency fields on nodes0/5 despite successful query
status; it does not establish loaded clock rates or a throttling cause.

Two new **same-zebin chain diagnostics** give the opposite direction from the
whole model: 400 dependent `advance` kernels on 64 local floats take
**249.84/275.96us native versus 363.79/364.32us L0** on the calibrated CS clock
(-31.32%/-24.25%). Host replay improves only 11.57%/2.70%, while native host
submission takes longer. These are synthetic kernel-chain times, not model
throughput gains. All 2880 replays finish; 72 case outputs and full guards
match. The pinned Short configuration bypasses per-layer CPU planning, and
the existing cold graphs have no API event waits or graph transfers. The
remaining model gap needs workload-specific investigation, especially the
mixer/HC stages; this does not prove host cache policy or all packet settings
equivalent. [Multi-angle results](demo/dg2-multi-angle-diagnostic-windows-2026-11-02.json)
and runnable probes, raw samples, source pins and graph accounting（本地归档：`demo/dg2-multi-angle-diagnostic-windows-2026-11-02.json.gz`）.

A **nine-phase Short investigation** now completes90 exact requests using an
isolated1GiB native allocation-part candidate: **47.094L0/43.192native tok/s
(-8.287%)**, with-0.913% L0 control drift. Smaller parts resolve the observed
7.74GiB cache startup failure while retaining5771 slots and residency checks;
mapping before residency alone does not. Instrumented spans prioritize HC and
input/output projections, but markers perturb native(-4.958%) more than
L0(-1.285%), so their deltas are not exact release attribution. A concurrent
MTP source edit is preserved and resolved against immutable inputs; executed
binary/object pins and all raw outputs/tables are checked separately. This
candidate changes no default or Linuxv6 file. [Measured scope and provenance](demo/dg2-one-gib-subphase-short-windows-2026-11-02.json)
and all sources, checks, failures and requests（本地归档：`demo/dg2-one-gib-subphase-short-windows-2026-11-02.json.gz`）.

**The later main-source validation adopts the 1 GiB WDDM allocation-part
limit.** Total capacity, contiguous GPU virtual addresses and strict local
residency checks stay unchanged. All 22 Windows CTests and three Linux CPU
CTests pass; an exact model smoke retains 5,771 expert slots, 76 output tokens
and 57/57 drafts. The large-allocation check writes seven distinct regions
across a 5 GiB allocation before reading any back, covering every part boundary
and detecting address aliases. This is a Windows reliability fix, with no
throughput claim or Linux GPU result; immutable Linux v6 remains unchanged.
[Main adoption and checks](demo/dg2-one-gib-main-windows-2026-11-02.json)
and runnable evidence（本地归档：`demo/dg2-one-gib-main-windows-2026-11-02.json.gz`）.

A separate **compile-time removal of verifier/MTP profiling and accounting
clocks** gives 43.513 / 43.668 / 43.157 tok/s for native baseline/candidate/
baseline. The candidate's +0.769% is smaller than the 0.818% baseline drift;
this bracket establishes no stable gain. All 45 outputs and draft counts
match. Synchronization locks, algorithmic timing, request clocks and safety
deadlines remain, so this does not exclude lock contention. The candidate is
not adopted. [Timer-removal results](demo/dg2-no-profile-short-windows-2026-11-02.json)
and sources, audit and raw requests（本地归档：`demo/dg2-no-profile-short-windows-2026-11-02.json.gz`）.

An independent **model-shape HC + Q8 control also runs without GPU timestamps
or inner-operator clocks**. Each replay contains 48 HC reads, with only three
host clock reads around the whole replay. For T=1, native DG2 takes
**16.33%-26.43% longer than bracketed L0** across one/eight weight banks and
pending-residual OFF/ON; T=4 differences are 2.71%-7.57%, with some near control
drift. The stamped and unstamped brackets produce **48 byte-identical complete
outputs** across 1,440 replays. Thus the HC difference persists without GPU
markers; this does not establish a whole-model speedup, identify its exact
cause or exclude all synchronization costs. Production defaults stay unchanged.
[HC controls, geometry and all samples](demo/dg2-hc-shape-windows-2026-11-02.json)
and runnable sources, commands and canonical output bytes（本地归档：`demo/dg2-hc-shape-windows-2026-11-02.json.gz`）.

A later [actual-launch trace](demo/dg2-hc-launch-trace-windows-2026-11-02.json)
verifies all 1,536 HC kernel names and dispatch dimensions match L0, with
16 identical complete outputs; 1,520 launches reuse all normalized state.
Large private-memory requirements are real in the selected kernels. An
eleven-signature `const&` candidate fails to reduce private/spill metadata and
is not adopted; it has no GPU performance claim.
[Resource comparison and frozen-input checks](demo/dg2-hc-const-reference-windows-2026-11-02.json).

The frozen L0/native Short comparison also passes an
[effective draft-threshold audit](demo/dg2-draft-threshold-audit-windows-2026-11-02.json):
both use step 0.5, cumulative floor disabled and last threshold inherited at
0.5. All 90 output/draft counts and 45 profiled window counts match. A stale
`STRATA_MTP_LAST_DRAFT_MIN_P=0.6` configuration key is read by neither executable.

A later [private-pool allocation control](demo/dg2-hc-private-pools-windows-2026-11-02.json)
passes 48 full-output comparisons across two native A/B/A brackets. Per-kernel
pools do not establish a broad repeatable gain and add 235.75 MiB of derived
capacity for the selected HC kernels. Main keeps queue-wide private reuse.

A narrower **HC norm capture** reduces compiler private memory from 40,192
to 12,544 bytes per hardware thread, with all 32 T=1..8 shape outputs exact.
Two HC replay brackets show modest changes. Two full native Short brackets
then improve decode **+0.72% / +0.66%**, with control drift +0.27% / -0.28%;
all 90 requests keep the same 76 tokens and 57/57 drafts. These tests explicitly
fix cumulative floor at zero, step/last min-p at 0.5, 5,771 slots and reserve
1,536 MiB, using the archived configuration after a separate live MTP update.
The source change is now adopted after L0 all-T/output checks and fresh Linux
device/host compilation. The complete rebuilt pack passes 64 output cases.
Its first native full-pack Short bracket gives **-0.81%**, with +0.37% control
drift; the complete-pack L0 bracket gives **48.465 to 48.609 tok/s (+0.30%)**
against +0.63% control drift, with all 45 exact outputs. The earlier selected-image
gains do not establish a normal-rebuild speedup. The production executable has not
been replaced. A 68.79% private-byte
reduction is not a corresponding throughput or model-memory gain.
[HC resources, all-T parity and replay samples](demo/dg2-hc-norm-capture-windows-2026-11-02.json),
[Short bracket](demo/dg2-norm-capture-short-windows-2026-11-02.json) and
[Short confirmation](demo/dg2-norm-capture-short-confirm-windows-2026-11-02.json),
[complete-pack L0 bracket and evidence](demo/l0-norm-capture-fullpack-short-windows-2026-11-02.json).

A separate decode transfer audit finds identical explicit payloads in all
eleven L0/native request pairs, but **1,445 native D2D row copies versus 134
L0 region/linear-copy operations** for the same 4,554,752 bytes per 76-token
request. The frozen comparison also uses identical instruction bytes from
1,242 zebins. Counter-build timings are not speed results, and L0's hidden
copy kernels/blits are uncounted. An isolated one-kernel 2D-copy candidate passes
13 layouts / 78 changing-input region-versus-row replays and adapter Host fallback
checks. Its uninstrumented Short bracket gives **44.306 control / 44.194 candidate
tok/s (-0.25%)**, with -0.30% control drift. The matched-policy L0 control is
**48.541 tok/s**: native remains 8.96% slower. All 60 requests match the same
76 tokens and 57/57 drafts. That isolated trial establishes no speedup; the later
combined batching change below adopts the checked copy implementation. Live
module snapshots confirm native loads GDI32 without the Intel
UMD/Level Zero DLLs, while L0 loads ze_loader, ze_intel_gpu64 and GMM components;
neither snapshot contains SYCL/UR. Loaded modules do not measure call frequency.
[Transfer counts and exact outputs](demo/dg2-decode-transfers-windows-2026-11-02.json),
[host assembly and DLL imports](demo/dg2-transfer-static-assembly-windows-2026-11-02.json),
[2D-copy checks, Short results and loaded libraries](demo/dg2-copy2d-short-windows-2026-11-02.json).

A process-local allocation probe checks 15 size/alignment combinations against
the vendor driver. Its public `pageSize=65536` describes allocation alignment,
not a measured physical GPU PTE size. An isolated candidate calibrates two
resource fields and 2-MiB virtual alignment, preserving exact physical bytes
with separate 64-KiB tails. The expanded trial covers **98.00%** of cumulative
device allocation bytes, passes 15 Windows checks and five numerical GPU groups
per build, and matches all 60 Short requests. Decode gives **44.076 control /
44.395 candidate / 48.426 L0 tok/s**. The +0.72% change is close to -0.62% control
drift and leaves native 8.32% below L0; the candidate is not adopted. These data
do not establish a physical page-size or bandwidth explanation.
[Calibration, both alignment brackets and coverage](demo/dg2-allocation-alignment-windows-2026-11-02.json)
and sources, allocation blobs, checks and raw requests（本地归档：`demo/dg2-allocation-alignment-windows-2026-11-02.json.gz`）.

The initial process-local dynamic call census, before copy/locking batching, compares the same layer:
after ten warmups, native makes **136 KMT submissions versus L0's 795**, and
**2,153 application mutex calls versus 130**. The corresponding lock/unlock
inclusive probe totals are 0.493 / 0.052 ms. L0 makes 97 graph replay calls,
659 memory-copy appends and 39 barrier appends; mathematical-kernel appends
and argument-setting calls are zero throughout all eleven Decode requests.
All 22 output/draft checks pass. These are instrumented counts, not throughput
results or pure DLL overhead. Fewer submissions already exist in native;
they do not by themselves explain the measured Short gap. Driver internal
locks, direct fence polling and physical PCIe traffic remain outside this census.
[Actual selected DLL/KMT calls](demo/dg2-dynamic-calls-windows-2026-11-02.json)
and runnable observer, exact boundaries, raw logs and hashes（本地归档：`demo/dg2-dynamic-calls-windows-2026-11-02.json.gz`）.

An additional 22 exact requests attribute every observed successful submission
to its actual WDDM context node. Both backends submit only to **node 5, Compute**
throughout Decode. L0 creates node 2 (`DXGK_ENGINE_TYPE_COPY`), but no observed
Decode submission targets it; a copy-engine-overlap explanation is unproven.
Final-request primary command lengths total **8,171,544 bytes native / 143,936
bytes L0**. These omit secondary lists, so their ratio is not a total command
volume or speedup. Metadata is read once per adapter/node and cached.
[Engine-node attribution and primary command lengths](demo/dg2-submit-nodes-windows-2026-11-02.json)
and observer, cached metadata, SDK enum and all requests（本地归档：`demo/dg2-submit-nodes-windows-2026-11-02.json.gz`）.

Following the actual secondary batch-buffer jumps now gives **8,171,308 native /
7,381,108 L0 decoded command bytes**, with zero capture failures. Native emits
2,913 CFE states and 5,826 base-address states versus L0's 97 and 218. Capture
strongly disturbs execution; these are command counts, not speed measurements.
The earlier primary-only ratio is not a total command-volume comparison.
[Primary/secondary packets and bounded parser](demo/dg2-packets-windows-2026-11-02.json)
(raw batches and sources（本地归档：`demo/dg2-packets-windows-2026-11-02.json.gz`）).
Actual mutex return addresses also identify 659 per-copy context acquisitions
and 310 pairs of context/point acquisitions in completion collection. Staging
pool allocation/release accounts for only 19 pairs.
[Dynamic mutex callers](demo/dg2-lock-sites-windows-2026-11-02.json)
(same-link map and raw evidence（本地归档：`demo/dg2-lock-sites-windows-2026-11-02.json.gz`）).

The combined copy/locking changes are now adopted in the native DG2 source.
The batched ownership Short confirmation keeps step/last min-p 0.5, cumulative floor zero,
MTP4, 5,771 expert slots, 1,536-MiB reserve and the same 1,242 numerical zebins.
All 126 requests across separate performance, payload and lock probes match
the same 76 tokens and 57/57 drafts without reuse.

| Last warmed Short request | Native before | Native after | L0 |
| --- | ---: | ---: | ---: |
| D2D operations | 1,445 | 134 | 134 |
| Application mutex acquisitions | 2,147 | 136 | 124 |
| Application shared SRW acquisitions | 135 | 0 | 0 |
| Application exclusive SRW acquisitions | 0 | 0 | 0 |
| Total observed application acquisitions | 2,282 | 136 | 124 |

D2D payload stays **4,554,752 bytes**; H2D stays 659 operations and
968,318,976 bytes. Total observed application locking falls **94.04%**, but
does not exactly equal L0: the remaining difference is in CPU I/O. These counts include direct application SRW imports
used by the statically linked MSVC shared-mutex wrappers, exclude the probe's
own lock, and do not measure driver/private locks or contention. CPU I/O counts
can vary with scheduling. Every candidate mutex caller resolves to PLE/file
I/O; a separate matching-map census sees 130 I/O calls and **zero native runtime
mutex calls**, replacing the preceding version's 136 runtime acquisitions.
The all-lock probe sees 136 candidate I/O calls versus L0's 124. Original native
has 2,017 runtime mutex calls and 130 I/O calls here. I/O count variation is not
a runtime optimization.

A separate matched-policy probe confirms **zero business SSD reads** during
both warmed decodes. Both have 40 foreground I/O acquisitions; native has 15
background keepalive reads and L0 has 14. Six acquisitions per keepalive give
**130 versus 124** total. The original 100-ms keepalive stays enabled.
[Counters, caller maps and command-field controls](demo/dg2-current-fields-windows-2026-11-02.json)
and 1,432 verified evidence files（本地归档：`demo/dg2-current-fields-windows-2026-11-02.json.gz`）.

The uninstrumented before/candidate/after bracket gives **44.294 / 44.061
tok/s** for native control/candidate, against **48.665 tok/s** for L0.
Here the native control is the preceding adopted 128-KiB snapshot version;
the operation-count table compares the original native source with current source.
The -0.53% change, with +0.27% control drift, does not establish a speedup;
native remains **9.46% slower**. TTFT is **928.653 / 929.200 / 880.203 ms**
for control/candidate/L0. Decode excludes preparation, initialization and prefill.

The later **C0 dispatch-policy source update** keeps these copy/ownership paths
and matches the observed L0 overdispatch/thread-group settings. Two clean Short
controls improve native decode **+2.38% / +2.81%**; the calibrated repeat gives
**43.803 → 45.034 tok/s**, versus **48.621 tok/s** for L0, with +0.57% native
control drift. TTFT changes **933.226 → 924.809 ms (-0.90%)**. Native is still
**7.38% slower than L0**. Only A770 C0 under the calibrated Windows driver opts
in; Linux retains its previous fields. Fifteen Windows checks, six GPU
numerical groups, three Linux CPU checks and 60 exact model requests pass.
L1 policy, denorm mode, SLM and mathematical zebins stay unchanged. These are
source changes; the normal executable/configuration is not replaced.
[Calibrated repeat and adopted source hashes](demo/dg2-overdispatch-c0-windows-2026-11-02.json)
and 133 verified evidence files（本地归档：`demo/dg2-overdispatch-c0-windows-2026-11-02.json.gz`）.

A separate occupancy-based SLM control matches all **40,932 actual L0 walker
preferences**, unlike the earlier minimum-per-group experiment. Yet Short
changes **45.056 → 45.021 tok/s (-0.08%)**, within -0.51% control drift; all
60 outputs and runtime/numerical checks pass. It is not adopted: source keeps
SLM MAX and the existing C0 dispatch improvement.
[SLM controls and topology-inference limits](demo/dg2-slm-occupancy-windows-2026-11-02.json).

The latest **scratch-surface L1 writeback update** is also adopted on calibrated
A770 C0 Windows. Actual L0 descriptors use L1 WB; changing that field alone
improves native Short **+3.91%** in the isolated control and **+3.37%** in the
gated repeat. The latter measures **44.222 → 45.714 tok/s**, versus **47.024
tok/s** for L0, with **+0.31%** native control drift. TTFT falls **943.649 →
927.864 ms (-1.67%)**. Native is still **2.78% slower than L0** in this round.
The scratch coherence field remains unchanged: changing it alone gives only
+0.10%, and combining it with writeback does not improve decode over writeback
alone. SBA cache, SLM MAX, denorm mode and mathematical ISA remain unchanged.

Fifteen Windows tests, six GPU numerical groups and three Linux CPU checks
pass; all 60 repeat requests match 76 tokens and 57/57 drafts under the same
balanced policy and expert budget. Linux retains its prior scratch fields and
has no GPU performance claim. Source adoption-v7 preserves the copy/ownership
implementation; its clean performance run is separate from the earlier
operation/lock census. Normal executables and configuration are not replaced.
[Actual descriptors and three isolated controls](demo/dg2-scratch-cache-windows-2026-11-02.json),
[gated repeat and adopted source hashes](demo/dg2-scratch-cache-c0-windows-2026-11-02.json)
and 133 verified repeat evidence files（本地归档：`demo/dg2-scratch-cache-c0-windows-2026-11-02.json.gz`）.

A separate **v7 census** now confirms the current native payload and remeasures
both backends' imported application locks:

| Warmed 76-token Short | Native v7 | L0 reference |
| --- | ---: | ---: |
| D2D operations | 134 | 134 |
| D2D payload bytes | 4,554,752 | 4,554,752 |
| Observed application mutex acquisitions | 130 | 130 |
| Runtime mutex acquisitions | 0 | 0 |
| Shared / exclusive SRW acquisitions | 0 / 0 | 0 / 0 |
| Foreground I/O acquisitions | 40 | 40 |
| Background I/O acquisitions | 90 | 90 |

Both lock probes observe 15 keepalive reads, with six acquisitions per read,
and zero business SSD reads. Keepalive stays at 100 ms. This establishes total
count parity in this probe; background timing can change totals in later runs.
The census covers imported application mutex/SRW calls, excluding the probe's
own guard; private/inlined/driver locks and contention remain unmeasured.
Native D2D is freshly counted; L0 payload uses the earlier identical-fixture
reference. Copy/fill function bodies, owner/Queue headers, memory source and
ABI are also independently checked unchanged from v5. All 33 diagnostic
requests match 76 tokens and 57/57 drafts. Diagnostic executables contribute
no throughput claims.
[Current counters and scope](demo/dg2-v7-copy-lock-census-windows-2026-11-02.json)
and 97 verified evidence files（本地归档：`demo/dg2-v7-copy-lock-census-windows-2026-11-02.json.gz`）.

Changing only the offline copy/fill module to SIMD32 gives +2.26% in the first
Short trial, but only **+0.35%** in the larger repeat, against +0.29% native
control drift. The repeat retains 15 requests per arm after ten warmups:
native SIMD16 **45.577**, SIMD32 **45.739**, L0 **47.083 tok/s**. SIMD32 remains
**2.85% slower than L0**; no stable gain is established and SIMD16 stays in
place. All 200 requests across both trials match 76 tokens and 57/57 drafts.
Decode excludes preparation, initialization and prefill; TTFT is separate and
clocks are unlocked. These clean timing runs add no new copy/lock census.
[Two brackets, checks and reproduction](demo/dg2-copy-width-windows-2026-11-02.json)
and 120 verified evidence files（本地归档：`demo/dg2-copy-width-windows-2026-11-02.json.gz`）.

An isolated exact-length control also separates the walker's logical parameter
length from its unchanged 64-byte backing alignment. It passes eleven added CPU
cases, 15 Windows tests, six GPU numerical groups and three Linux CPU checks.
All 80 Short outputs match, but decode changes **45.552 to 45.437 tok/s
(-0.25%)**, against -0.31% native control drift; TTFT increases 0.69%. No gain is
established and this candidate is not adopted. The same round's L0 control is
47.421 tok/s. Decode excludes preparation, initialization and prefill, with
unlocked clocks. [Parameters, scope and reproduction](demo/dg2-payload-length-windows-2026-11-02.json)
and 115 checked evidence files（本地归档：`demo/dg2-payload-length-windows-2026-11-02.json.gz`）.

CPU analysis of the earlier v5 capture also finds 2,894 native writes to
preemption register `0x2580` and no L0 LRI in the observed Decode range. This
does not reveal L0's retained initialization state. Pinned NEO source shows
kernel-binary export reads a CPU heap, while uploaded ISA may use a separate
patched segment; export equality alone cannot prove resident-ISA equality.
[Cached-register results and limits](demo/dg2-register-fields-windows-2026-11-03.json)
and 20 checked evidence files（本地归档：`demo/dg2-register-fields-windows-2026-11-03.json.gz`）.

A subsequent real GPU readback compares **1,892 loaded code sections /
77,438,144 bytes** from the same **1,242 modules**. Native DG2, L0 and original
ELF text match byte for byte. Native uses its existing upload/readback paths;
L0 uses the existing copy kernel and zero-count multi-indirect targets to keep
the target code resident. These targets record dispatches but request no
mathematical work. Full readback guards and unchanged controls pass.
The earlier ordinary-copy access violation and copy-only reader's device-lost
failure are preserved, along with the successful residency calibration.
All **3,970 evidence files**, actual code dumps, input/module hashes and v7
source hashes are independently verified. This standalone check adds no model
request, copy/lock census or speed claim. Production code/configuration stay
unchanged. Native remains below L0: 2.78% in the v7 adoption repeat and 3.94%
for the unchanged v7 control in the later exact-length trial.
[Resident instructions and scope](demo/dg2-resident-isa-windows-2026-11-03.json)
and readbacks, exact sources and reproduction（本地归档：`demo/dg2-resident-isa-windows-2026-11-03.json.gz`）.

A subsequent startup capture confirms **the same initial preemption value**,
`0x2580 = 0x60002`, in L0 and native v7. The installed L0 creates both device
and contexts with `DisableGpuTimeout=1`; native leaves both clear. All five
masked 144-byte private context metadata blocks match. Separate probes verify
exact copies and full guards; these observations do not establish effective
hardware preemption or a desktop fix.

Two isolated five-arm Short rounds test context-only and device-plus-context
flag matching, with ten warmups and ten retained requests per process. Each
passes 15 CTests and six GPU numerical groups; all **200 model requests** match
76 output tokens and 57/57 drafts. Context-only changes native throughput by
**-0.34%**. Matching both changes **46.002 to 46.404 tok/s (+0.87%)**, with
**+0.78% native / +2.69% L0 control drift** and **-1.85% TTFT**. Its L0 bracket
is **47.850 tok/s**: the candidate remains 3.02% slower. The positive observation
needs repetition and establishes no stable gain; neither trial is adopted.
Timing has no observer or inner diagnostic counters. Decode excludes setup,
initialization, preparation and prefill; clocks are unlocked. Production v7
source, executable/configuration and Linux handoffs stay unchanged.
[Initial state, both comparisons and reproduction](demo/dg2-initial-state-windows-2026-11-03.json)
and 369 independently checked evidence files（本地归档：`demo/dg2-initial-state-windows-2026-11-03.json.gz`）.

The exact same binaries complete another **100 clean requests** with reversed
native / L0 / candidate / L0 / native ordering. Native/candidate/L0 brackets
are **46.582 / 46.719 / 48.727 tok/s**: the flag candidate changes **+0.29%**,
versus **+0.75% native / -0.15% L0 drift**, and remains **4.12% below L0**.
TTFT changes -0.50%; all outputs and drafts match. This repeat does not confirm
a stable gain; flags remain unadopted. Existing numerical checks are reused
because the binaries are unchanged. [Repeated comparison and scope](demo/dg2-timeout-repeat-windows-2026-11-03.json)
and 30 checked evidence files（本地归档：`demo/dg2-timeout-repeat-windows-2026-11-03.json.gz`）.

A separate **60-request** L0 / native v7 / L0 diagnostic samples frequency
through read-only Sysman, with exact output/draft parity and unchanged binaries.
All **136 / 146 / 139** retained GPU samples report **2400 MHz**; corresponding
memory samples report **16000 MHz**, with no throttle-reason flags. Actual
median sampling intervals are **109.92 / 108.84 / 110.22 ms**. No clock/power
setting is changed. This provides no sampled frequency difference to explain
the gap; brief changes between samples remain unobserved. Observer overhead
and host delivery windows prevent treating diagnostic timings as clean speed
results. [Frequency observations](demo/dg2-loaded-frequency-windows-2026-11-03.json)
and 54 checked evidence files（本地归档：`demo/dg2-loaded-frequency-windows-2026-11-03.json.gz`）
preserve raw samples, SDK ABI calibration and failed attempts.

The installed Windows driver advertises **no Sysman PCI byte counters** and
returns `ZE_RESULT_ERROR_UNSUPPORTED_FEATURE` from `zesDevicePciGetStats`.
This is a cached capability failure, not a physical-traffic measurement.
Imported-host traffic remains open; Linux counter availability is untested.
[Actual response and scope](demo/dg2-pci-counter-availability-windows-2026-11-03.json)
and 11 checked evidence files（本地归档：`demo/dg2-pci-counter-availability-windows-2026-11-03.json.gz`）.

A four-site native adapter CPU-copy diagnostic completes **15 exact Short
requests**, with unchanged v7 runtime/math modules and 57/57 drafts. In each
of the last five requests, producer snapshots and owner upload staging each
perform **19 copies / 1,867,776 bytes**, all 96 KiB; recorded upload refresh and
staged CPU readback publication each count zero in this fixture. The median
of combined copy spans is **645.7 us per entire 76-token request**, ranging
**622.4–672.5 us**, with **3,735,552 copied bytes**. One initial request has
18 copies at each upload site; all 15 records are published. These spans are
small relative to prior Decode measurements and provide no evidence of a
multi-percent copy-time saving. QPC overhead is included; surrounding
`cpu_access`, counter updates, cache effects and L0 driver-internal copies
are unmeasured. Callbacks change inlining, so diagnostic model timings are
not clean speed evidence. Production source/configuration stay unchanged;
D2D and application-lock parity retain their separate census evidence.
[CPU-copy records and scope](demo/dg2-cpu-staging-windows-2026-11-03.json)
and 293 verified evidence files（本地归档：`demo/dg2-cpu-staging-windows-2026-11-03.json.gz`）.

A separate owner/runtime diagnostic passes **15 exact Short requests** and an
eight-producer check of the exact instrumented executor (**32,332 handoffs**).
The last five requests each have 136 owner handoffs. Median queue-plus-return
spans sum to **7.6685 ms per 76-token request**; full publish-to-resume spans
sum to **18.5218 ms**. The nested owner-body median is 10.6938 ms, including
136 runtime submissions totaling 3.3490 ms; 19 CPU-access calls total only
**0.0278 ms**. The 135 completion waits total **1579.6079 ms** at the median.
Combined medians use per-request sums; nested scopes are not added to parents.
These observations focus further investigation on GPU execution and completion
delivery. Wait spans include GPU work and OS scheduling and do not isolate
notification latency or CPU usage. Diagnostic code changes scheduling/inlining,
so these are not clean speed results or guaranteed removable costs. Production
source/configuration stay unchanged; a native throughput win is still open.
[Handoff/runtime records and limits](demo/dg2-host-cost-windows-2026-11-03.json)
and 345 verified evidence files（本地归档：`demo/dg2-host-cost-windows-2026-11-03.json.gz`）.

A GPU-tail diagnostic then matches **all 2,023 waits in 15 exact Short
requests**, with unchanged v7 production sources, mathematical modules and
57/57 drafts. It adds timestamp post-sync to the existing final `PIPE_CONTROL`,
preserving flush bits and packet count. Eight gated replays validate clock
alignment and guarded markers across two queues. Per-request GPU/QPC affine
calibration handles the 36-bit wrap and an observed **117–119 ppm** difference
from nominal counter rate. In the last five requests, total wait spans have a
**1604.7809 ms** median; the portion inside waits after their GPU tails has an
**8.08335 ms** point median and **7.75568–14.81612 ms** range. The higher request
is retained. Lower/upper query-window estimates have separate medians
**7.54575 / 9.43948 ms** under the affine model, not statistical confidence
intervals or guaranteed hardware accuracy. Before/after calibration queries
take median **20.9 / 19.7 us**. Caller work before wait entry is excluded.
These spans include fence epilogue, interrupt delivery and OS scheduling;
they are not pure notification latency, CPU use or guaranteed removable time.
No L0 completion-delivery comparison or clean speed claim is made. Work and
scheduling before the tail remain the main investigation; production
executable/configuration and Linux handoffs stay unchanged.
[Tail/wait records and clock-model limits](demo/dg2-completion-delivery-windows-2026-11-03.json)
and 100 verified evidence files（本地归档：`demo/dg2-completion-delivery-windows-2026-11-03.json.gz`）.

The latest native source adoption, **v8**, fixes the compiler's launch-state
contract. IGC enables a kernel's denorm-retention bits using OR instructions
and assumes they start clear. Previous native SETBYKERNEL dispatch could keep
FP32 retention in an explicitly flushing kernel. The field now starts at
**FTZ (0)**, matching installed L0; body CR0 instructions still select each
kernel's mode. All **1,892 text sections in 1,242 frozen mathematical zebins**
initialize retention after their selected entry, so this does not turn the
current model's arithmetic into denorm flushing.

Standalone FP32/FP16 probes compare preserving and explicitly flushing modules
on all three paths, with 768 values per run and overwritten replays/full
guards. Old native differs from L0 on **60 FP32 rows** of the flush module;
the fix matches L0 bit for bit in both modules. This is a compiler-contract
correction, not evidence of incorrect current model output or a broad PPL
result. A permanent two-queue regression passes both modes with the fix and
rejects the consistent old runtime/header control for explicit flushing.
Windows **15 checks** and Linux **three CPU checks** pass; Linux GPU is untested.

The clean 100-request Short bracket gives **47.049 / 47.130 / 48.628 tok/s**
for native / fix / L0, with identical 76 tokens and 57/57 drafts. The observed
**+0.17%** is below **+1.08%** native control drift; L0 drift is +0.12%.
There is no established speedup: native remains **3.08% below L0** in this round.
Decode excludes preparation/init/prefill; balanced policy, expert budget and
all mathematical ISA remain fixed. V8 is adopted for correctness. Copy/owner
code, production executable/configuration and immutable Linux handoffs stay
unchanged; earlier D2D/lock counters remain separate v7 evidence.
[Compiler/ISA audit and clean trial](demo/dg2-denorm-contract-windows-2026-11-03.json),
178 trial evidence files（本地归档：`demo/dg2-denorm-contract-windows-2026-11-03.json.gz`）,
[v8 source adoption and regression](demo/dg2-denorm-fix-windows-2026-11-03.json)
and 254 adoption evidence files（本地归档：`demo/dg2-denorm-fix-windows-2026-11-03.json.gz`）
include exact build commands and both rejected test-harness attempts. Use the
baked regression modules with `orca-l0-dg2-check --gpu-denorm retain.zebin retain`
and `orca-l0-dg2-check --gpu-denorm flush.zebin flush`.

A subsequent **100-request SBA L1 WB trial on v8** measures **46.172 / 46.203 /
48.001 tok/s** for native control / candidate / L0. Its **+0.07%** change is
below **+0.44%** native control drift, so the candidate is not adopted; it
remains **3.75% below L0**. TTFT is **908.370 / 906.494 / 891.190 ms**.
All requests match 76 tokens and 57/57 drafts; Windows 15 checks, six GPU
numerical groups, both denorm regressions and Linux three CPU checks pass.
Decode excludes preparation/init/prefill. Scratch WB, current dispatch/FTZ,
policy, 80 Host objects and 1,242 mathematical zebins are fixed. Nine unchanged
copy/owner source hashes connect v8 to the separate **134/134 D2D** and
**130/130 application mutex** census; this is not a new counter run.
[Results and frozen baseline](demo/dg2-sba-wb-v8-windows-2026-11-03.json)
and 147 verified evidence files（本地归档：`demo/dg2-sba-wb-v8-windows-2026-11-03.json.gz`）
preserve both source versions, raw protocols and commands. The source stays v8.

A **real GDN kernel diagnostic** now repeats both L0/native/L0 and
native/L0/native brackets with the same binaries. For default SIMD16, local
data and 16 dependent launches, native GPU time is **1.85%-2.64% lower at T=1**
and **1.37%-1.39% lower at T=4**. Single-launch local GPU time is instead
3.15%-4.59% longer; host data does not show the same stable advantage.
Submit-and-wait wall time remains longer even where native GPU time improves.
This is an isolated verification workload, with unchanged state and reused
inputs, rather than a full model or mixed-kernel/large-working-set comparison.
It establishes no end-to-end native win and changes no default.

Both frozen GDN/Q8-tail zebins use 256 GRFs and 3,136 bytes of SLM. Current
SIMD16 has no declared spill; SIMD32 is the 4-KiB-scratch control. All **192
case runs** pass scalar-reference and full input/state/padding/timestamp guard
checks; 32 FP32/packed-Q8 fixtures match bit for bit across all six arms.
Maximum scalar-reference error is **6.333e-7**, with **5,760 retained samples**.
[Two-bracket results and limits](demo/dg2-gdn-kernel-windows-2026-11-03.json)
and 243 evidence files（本地归档：`demo/dg2-gdn-kernel-windows-2026-11-03.json.gz`）
preserve raw samples, numerical output dumps, sources and exact commands.

A follow-up **working-set and mixed-kernel diagnostic** finds a larger,
repeatable gap. With local data, T=1 and 16 GDN launches, changing from one
reused slot to 16 independent slots changes L0/native GPU time from
**161.185 / 159.180 µs** to **330.228 / 345.833 µs**. Alternating each GDN
with a guarded 128-GRF `advance` kernel instead gives **190.951 / 373.815 µs**
on the small working set. A fresh bracket reproduces **190.739 / 374.192 µs**.
The added kernel also changes SLM/ISA state and adds work/barriers; this does
not isolate GRF switching or establish the full-model cause.

Removing only repeated preemption-register writes leaves the latter case at
**375.026 µs**; the candidate is not adopted. All **256 case runs** pass scalar
reference and full guards, with identical GDN FP32/packed-Q8 outputs across
host/local, mixed/unmixed and backend controls. This is a standalone diagnostic,
with no new model-throughput or D2D/lock census. The existing measured parity
remains **134/134 D2D operations, 4,554,752 bytes and 130/130 imported mutex
acquisitions**, from separate v7 evidence; those source paths are unchanged.
[Factorial results and isolated register test](demo/dg2-gdn-worksets-windows-2026-11-03.json)
and 421 evidence files, deduplicated into 108 blobs（本地归档：`demo/dg2-gdn-worksets-windows-2026-11-03.json.gz`）
preserve raw samples, outputs, source versions and commands. Source stays v8.

Two narrower follow-ups separate **instruction-cache invalidation** from
**repeated address-state programming** on pure C0 GRF changes. Clearing only
the instruction invalidation bit leaves the small local mixed T=1 interval
essentially unchanged: **374.271 / 373.659 µs** for control/candidate. Omitting
only the two redundant SBA packets reduces **374.199 to 337.799 µs (-9.73%)**;
T=4 falls **532.028 to 495.885 µs (-6.79%)**. All other packets and full
invalidations remain in the SBA candidate. All **416 guarded cases** pass.
[Separate component trials](demo/dg2-transition-parts-windows-2026-11-03.json)
and 680 files, 142 deduplicated blobs（本地归档：`demo/dg2-transition-parts-windows-2026-11-03.json.gz`）.

That local benefit does **not** establish faster model decode. The fresh
100-request Short bracket measures **46.386 / 46.254 / 47.880 tok/s** for
native control / SBA candidate / L0, with native control drift +0.16% and L0
drift -0.70%. TTFT is **909.710 / 908.492 / 893.932 ms**. All requests match
76 tokens, 57/57 drafts and natural stop at the fixed balanced policy; decode
excludes preparation/init/prefill. Windows 15 checks, six numerical groups,
both denorm regressions and Linux three CPU checks pass. The candidate is
not adopted; source, production executable/config and Linux handoffs stay fixed.
[Raw model protocols and parameters](demo/dg2-sba-transition-short-windows-2026-11-03.json)
and 172 verified evidence files（本地归档：`demo/dg2-sba-transition-short-windows-2026-11-03.json.gz`）.

A reverse SBA/native/SBA probe, reusing the same executables, reproduces the
local mixed benefit: **372.878 to 335.937 µs (-9.91%)** at T=1 and
**526.849 to 491.543 µs (-6.70%)** at T=4. Candidate drift is -0.02%/-0.03%.
All 96 guarded cases and their FP32/Q8 output bytes pass. This confirms the
local effect; the full-model result above remains the adoption decision.
[Reverse probe and raw outputs](demo/dg2-sba-transition-repeat-windows-2026-11-03.json)
and 111 evidence files（本地归档：`demo/dg2-sba-transition-repeat-windows-2026-11-03.json.gz`）.

Further isolated C0 GRF-transition trials leave most of the mixed-kernel gap.
On the same local small T=1 probe, removing only CFE changes GPU time
**374.036 → 373.906 µs (-0.03%)**; removing only pipeline selection gives
**373.926 → 373.698 µs (-0.06%)**. Clearing only state/constant/texture
invalidations gives **373.782 → 370.938 µs (-0.76%)**. Clearing all four,
including instruction invalidation, gives **373.763 → 363.411 µs (-2.77%)**,
versus **189.375 µs** for L0. T=4 improves 1.69% in that last trial. Adjacent
native-control drift is below 0.25%; these remain GPU microbenchmark intervals.
All **448 guarded case runs** pass; each of four staged candidates passes
15 Windows checks, six numerical GPU groups and both denorm checks. A staged
host-gated checker removes a race in pending-work negative assertions; the
original failed attempts are preserved. No source is adopted or model speedup
claimed. D2D/lock parity remains the separate historical census above.
[Three isolated controls](demo/dg2-transition-remaining-windows-2026-11-03.json)
and 689 evidence files（本地归档：`demo/dg2-transition-remaining-windows-2026-11-03.json.gz`）;
[four-invalidation control](demo/dg2-all-invalidation-windows-2026-11-03.json)
and 401 evidence files（本地归档：`demo/dg2-all-invalidation-windows-2026-11-03.json.gz`）.

The **actual warmed mixed-GDN replay** is now captured on both paths, rather
than inferred from full-model traces. Both make one KMT submission and execute
32 walkers with identical GRF switches, geometry, SIMD masks, required SLM,
hardware-thread counts and denorm mode. Native/L0 have **64/1 SBA, 32/0 CFE
and 68/69 PIPE_CONTROL** packets, totaling **13,796/7,020 command bytes**.
GDN preferred SLM and advance payload padding still differ; the earlier
negative full-model SLM/cache controls remain valid. All 64 guarded numerical
cases match the frozen fixtures. Captured timings are instrumented and make
no speed claim; these counts are not a new full-model D2D/lock census. Source
remains v8, with no further adoption or Linux GPU claim.
[Actual packet fields and limitations](demo/dg2-gdn-mixed-packets-windows-2026-11-03.json)
and 209 evidence files, including both preserved failed attempts（本地归档：`demo/dg2-gdn-mixed-packets-windows-2026-11-03.json.gz`）.

A subsequent **SBA/invalidation interaction** trial closes the small mixed-GDN
gap. Omitting redundant double-SBA and clearing four invalidation bits together
gives T=1 **374.537 → 185.781 µs (-50.40%)**, versus **190.924 µs** for L0.
The reused SBA-only control is **337.161 µs**; clearing the invalidations alone
previously saved only 2.77%. T=4 gives **529.427 → 333.203 µs (-37.06%)**,
versus **339.902 µs** for L0. A seven-arm bracket checks native, SBA-only and
outer L0 drift. All **224 guarded case runs**, 15 Windows checks and eight
GPU numerical groups pass. Larger local working sets remain 2.71%/2.09%
longer than L0. This is a GPU microbenchmark result; full-model benefit and
source adoption require the separate clean Short comparison.
[Interaction results and scope](demo/dg2-sba-invalidation-windows-2026-11-03.json)
and 476 evidence files（本地归档：`demo/dg2-sba-invalidation-windows-2026-11-03.json.gz`）.

The reversed combined/L0/combined standalone bracket repeats the small local
advantage: **185.534/189.661 µs at T=1 (-2.18%)** and **332.962/338.932 µs
at T=4 (-1.76%)**, with candidate drift below 0.25%. All 96 guarded cases pass.
[Reverse results](demo/dg2-sba-invalidation-repeat-windows-2026-11-03.json)
and 113 evidence files（本地归档：`demo/dg2-sba-invalidation-repeat-windows-2026-11-03.json.gz`）.

The first clean full Short bracket gives **46.382 / 46.749 / 48.172 tok/s**
for native/combined/L0: **+0.79%** versus native, control drift +0.25%, still
**2.95% below L0**. TTFT is **906.809 / 906.223 / 890.239 ms**. All 100 full
outputs and 57/57 drafts match the same balanced policy, expert budget and
1,242 math zebins. Linux three CPU checks pass. Decode excludes preparation,
initialization and prefill; no timing probes or telemetry run. This first bracket
prompted the reverse comparison below; a native end-to-end win is not established.
[Short result and limits](demo/dg2-sba-invalidation-short-windows-2026-11-03.json)
and 178 evidence files（本地归档：`demo/dg2-sba-invalidation-short-windows-2026-11-03.json.gz`）.

The reversed full-model L0/combined/native/combined/L0 bracket gives **46.911 /
47.103 / 48.495 tok/s** for native/combined/L0: **+0.41%** versus native,
combined-control drift -0.19%, still **2.87% below L0**. TTFT is **902.401 /
899.461 / 883.991 ms**. Another 100 exact outputs and 57/57 drafts pass with
the same binaries and policy. Both opposite orders observe small decode gains;
control drift does not supply a statistical confidence interval.
[Reverse model result](demo/dg2-sba-invalidation-short-repeat-windows-2026-11-03.json)
and 171 evidence files（本地归档：`demo/dg2-sba-invalidation-short-repeat-windows-2026-11-03.json.gz`）.

This combination is now **adopted as source v9**, changing only the encoder and
its checker. Pure C0 GRF transitions omit redundant double-SBA and four
invalidations together; stall/HDC/untyped/DC and all other packets remain.
Initial/invalid/untracked/coupled/fallback states keep full programming. The
checker adopts deterministic host-gated pending/timeout assertions. All 12
current production-source hashes match the validated source; 15 Windows,
three Linux CPU and eight numerical GPU groups pass. Existing executables,
configuration and immutable Linux bundles are preserved. No fresh D2D/lock
census or native full-model throughput win is claimed.
[v9 source adoption](demo/dg2-sba-invalidation-main-windows-2026-11-03.json)
and 151 adoption evidence files（本地归档：`demo/dg2-sba-invalidation-main-windows-2026-11-03.json.gz`）.

Registered copies use bounded producer batches, and pitched regions use one
GPU dispatch. Existing command boundaries flush tails immediately; no timer or
wait-to-fill is added. Eager ordinary Host inputs up to 128 KiB use cached
producer snapshots, covering the measured 96-KiB uploads. Larger inputs retain
immediate staging. The extra CPU snapshot reuses capacity; GPU copy bytes stay
unchanged. Immutable completion points avoid duplicate context
acquisitions, retain upload storage until safe collection, and preserve staged
CPU readbacks. Recorded ordinary Host inputs still refresh from their original
addresses on every replay. Completion observers now retain their Windows
device/fence mappings or a duplicated Linux DRM fd/timeline. Their waits need
no lifetime mutex; atomic teardown cancellation remains checked. The shared
native recorder is now owned by a dedicated consumer. Producers publish calls
through a lock-free MPSC exchange and block on atomic completion words, while
GPU fence waits remain outside the consumer. Producer copy batching and
immutable checkpoint fast paths avoid per-copy handoffs; completion words are
separated from reference counts by cache lines. Cold pools and unresolved host
events retain their own synchronization.
Ordinary Host inputs use the snapshot only while its registry epoch is
current. A device allocation added during a pending batch therefore goes
through current-registry lookup rather than being dereferenced by the CPU.

All 80 Host objects are rebuilt against the changed adapter layout. Validation includes
an eight-producer, 32,000-call CPU check with nested calls, exception propagation
and four teardowns, plus 15 Windows native checks, six numerical GPU groups, eight two-queue teardown
races with retained completions from older runtimes,
old/new memory-module checks and the complete Queue suite. The adopted
[`orca_queue_check.cpp`](../sycl/orca_queue_check.cpp) checks 137-copy tails,
changing pitched data and padding, stack-source lifetime, a newly allocated
device source during a pending batch, concurrent finishers,
96-/128-KiB upload boundaries and overwritten or destroyed ordinary Host inputs,
cross-thread queue switches and graph replay across two consumers; batch limits
64 and 1 both pass. Linux passes four CPU checks and dependency/error-path
checks; Linux GPU execution remains unvalidated. Production executables and
configuration files were not replaced.
[Batched ownership confirmation](demo/dg2-owner-batched-windows-2026-11-02.json) and
703 independently verified evidence files with sources, commands and outputs（本地归档：`demo/dg2-owner-batched-windows-2026-11-02.json.gz`）.
The [preceding 128-KiB confirmation](demo/dg2-host-snapshots128-windows-2026-11-02.json)
preserves the 272-acquisition result.
The [preceding completion-ownership confirmation](demo/dg2-completion-leases-windows-2026-11-02.json)
preserves the 291-acquisition result. The unchanged Linux runtime source keeps
its four CPU-check results; the Linux adapter and GPU path still need validation.
The [earlier batching confirmation](demo/dg2-copy-lock-batching-confirm-windows-2026-11-02.json)
preserves the preceding 426-acquisition measurement.

A follow-up owner-readiness change was **rejected**: all 126 requests match,
but locking stays at **426**, and decode is **43.097 control / 43.081 candidate /
47.056 L0 tok/s** (-0.04%, +0.19% control drift). Matching linker maps resolve
all **135 shared acquisitions** to `NativeCompletion::wait`, rather than
owner-readiness queries. This trial was not adopted; the completion-ownership
change above removes those lifetime locks instead.
[Rejected trial and shared-wait stacks](demo/dg2-owner-query-rejected-windows-2026-11-02.json)
and sources, matching maps and raw evidence（本地归档：`demo/dg2-owner-query-rejected-windows-2026-11-02.json.gz`）.

A separate 64-KiB Host-snapshot trial was also rejected: native runtime
acquisitions stay at 155, with **44.140 control / 44.127 candidate / 48.596 L0
tok/s** (-0.03%, -0.29% control drift). Copy-path tracing identifies all 19
remaining copy acquisitions as **96-KiB ordinary Host-to-Device uploads**
with a current registry snapshot; they exceed that trial's cache limit.
[137 exact requests and path evidence](demo/dg2-host-snapshots-rejected-windows-2026-11-02.json)
and 599 evidence files（本地归档：`demo/dg2-host-snapshots-rejected-windows-2026-11-02.json.gz`）.

**Main native Runtime now records raw CS timestamps** through
`write_global_timestamp(registered_gpu_address)`, with ordered writes and range
checks. Two queues/eight gated replays validate the Windows clock scale from a
separately cached 19.2-MHz/36-bit driver calibration. Actual main passes 21
Windows checks, three Linux CPU checks and another exact model smoke. This
adds no Level Zero dependency or default inference-time markers. The Strata
adapter now passes another **36** gated timestamp samples across host/device
targets, eager/recorded/relaxed modes and batch limits 64/1. Calibrated DG2
profiling uses `STRATA_ORCA_DG2_TIMESTAMP_HZ` and `STRATA_ORCA_DG2_TIMESTAMP_BITS`,
cached once per Runtime; the fractional ns/tick period is retained. Linux GPU
calibration remains pending.
[Calibration](demo/dg2-timestamp-windows-2026-11-02.json) and
[main-source adoption](demo/dg2-timestamp-main-windows-2026-11-02.json)
include source hashes and full check archives alongside their reports.
A separate smaller nonzero SLM-partition experiment regresses Short
**44.212 to 41.039 tok/s (-7.18%)**, with all 45 outputs exact, so main retains
MAX. [SLM results](demo/dg2-slm-data-windows-2026-11-02.json) preserve that check.
The latest source-only Linux continuation is **v6**, with three
exported-source CPU checks and three ELF dependency audits.
[Linux export verification](demo/dg2-profiler-linux-handoff-cpu-2026-11-02.json)
records its boundary; fresh Linux GPU checks and clock calibration remain pending.

[Incident report](demo/dg2-desktop-incident-windows-2026-11-02.json) and
sources, raw logs and CPU-only check（本地归档：`demo/dg2-desktop-incident-windows-2026-11-02.json.gz`）.

```powershell
cmake -S orca-l0 -B build-orca-wddm -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DORCA_L0_BUILD_RUNTIME=OFF -DORCA_L0_BUILD_WDDM_CHECKS=ON
cmake --build build-orca-wddm
ctest --test-dir build-orca-wddm --output-on-failure
```

On A770/32.0.101.9034 it passes allocation, residency, GPU VA mapping,
native compute-context creation, command submission, monitored-fence waits
and cleanup without loading Level Zero, Intel UMD, GMM, SYCL, UR or the GPU
compiler. Three cycles check 216 exact GPU stores in total. Timeout remains
enabled; the check changes no display or system settings.

With a **ze_info 1.73 zebin baked from `tests/inference.cl` on this driver**,
it also checks 216 multi-group vector increments and 216 recorded
RMSNorm → GEMV → SwiGLU → vector-increment pipelines against a CPU reference.
The pipeline changes its parameter buffer before every replay. All other
input, code, payload and guard bytes are checked for unintended writes.

```powershell
# Add the existing, driver-baked image to the build above:
cmake -S orca-l0 -B build-orca-wddm `
  -DORCA_L0_WDDM_ZEBIN=C:/path/to/image_0.zebin
cmake --build build-orca-wddm
ctest --test-dir build-orca-wddm --output-on-failure
# Or exercise the two workloads separately:
./build-orca-wddm/orca-l0-wddm-memory-check.exe --compute C:/path/to/image_0.zebin
./build-orca-wddm/orca-l0-wddm-memory-check.exe --pipeline C:/path/to/image_0.zebin
```

To bake the image, build `orca-l0-native-pack` with the Level Zero SDK,
place the compiled `inference.spv` in a fresh source folder with an `index.txt`
containing `inference.spv<TAB>advance`, and run `orca-l0-native-pack SOURCE NEW_OUTPUT`.
The module contains all four entry points. Baking uses Intel's compiler/UMD;
the separate WDDM execution process has neither dependency. Older AOT images
with ze_info 1.62 are rejected, rather than silently interpreted as 1.73.

The private Windows ABI is calibrated for **PCI 0x56a0 / driver 32.0.101.9034**;
other devices/drivers are rejected before context creation. The native layout
is 144 bytes for context data and 184 bytes for the submit header, different
from Intel NEO's public fallback. The shipped helper's field translation and
the installed driver's calls were checked in isolated diagnostic processes;
the execution check does not load that helper.

This proves UMD-free execution of these kernels on this Windows machine.
Full Strata integration and the broader kernel ABI remain open. Explicit
device-local allocation now passes the correctness checks below; earlier
performance measurements still use system-memory heaps.
The [initial WDDM report](demo/wddm-windows-2026-10-31.json) records
correctness before the following performance comparison.

The matched Windows benchmark builds the same harness against existing Orca
L0/direct vendor DDI as its Intel UMD control. Both arms use the same zebin,
inputs, geometry and CPU/guard checks, with resident system-memory data heaps.
Three A0/B/A1 rounds retain 512 samples after 128 warmups per process;
setup, recording, parameter updates and correctness checks are excluded.

| Workload / wait | Existing Orca L0 completion | Direct WDDM completion | Paired latency change | Stable pairs |
| --- | ---: | ---: | ---: | ---: |
| One kernel / native | 68.4 µs | 98.4 µs | +43.9% | 2/3 |
| Four-kernel pipeline / native | 86.2 µs | 125.6 µs | +45.7% | 1/3 |
| 400 dependent kernels / native | 486.1 µs | 450.8 µs | −7.6% | 2/3 |
| One kernel / poll | 70.8 µs | 67.3 µs | −5.5% | 0/3 |
| Four-kernel pipeline / poll | 87.7 µs | 93.2 µs | +5.5% | 0/3 |
| **400 dependent kernels / poll** | **499.4 µs** | **386.7 µs** | **−23.3%** | **3/3** |

Only the last row meets the three-stable-pair rule (control drift ≤3%);
its maximum drift is 1.48%. Other rows are observations with insufficient
stable pairs, including the short-job regressions with native waiting.
Percentages are medians of paired ratios; times are separately aggregated.
CPU submission measured 13.4 → 8.7 µs for the polling chain, but submission
drift fails the rule, so a stable CPU submission gain is not established.

The candidate reuses identical GPU state, uses the verified DG2 L3 write-back
policy, keeps GPU dependency barriers and flushes for host visibility once
at the end of the list. The initial conservative encoder was much slower
(5.52 ms for the polling chain); state reuse alone did not fix it. Cache
policy and flush placement were necessary. Separate allocations, ISA/cache
placement, command encoding and wait implementations differ between arms;
this does not isolate API overhead. Polling occupies a CPU thread.
**This is a dispatch microbenchmark, not a full-model token-throughput gain.**
[Parameters, hashes, all six cases and optimization history](demo/wddm-performance-windows-2026-10-31.json)
include raw samples（本地归档：`demo/wddm-performance-windows-2026-10-31.json.gz`）.

To reproduce on the calibrated Windows host, initialize the compiler
environment, bake the image as above, and use a fresh build/output directory:

```powershell
cmake -S orca-l0 -B build-orca-wddm-perf -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DORCA_L0_BUILD_RUNTIME=ON -DORCA_L0_DIRECT_DRIVER=ON `
  -DORCA_L0_BUILD_GPU_CHECKS=OFF -DORCA_L0_BUILD_WDDM_CHECKS=ON `
  -DORCA_L0_SDK="C:/Program Files/LevelZeroSDK/1.33.1" `
  -DORCA_L0_WDDM_ZEBIN=C:/path/to/image_0.zebin
cmake --build build-orca-wddm-perf --target orca-l0-wddm-memory-check orca-l0-wddm-baseline-check
python tools/bench_orca_wddm.py `
  --baseline build-orca-wddm-perf/orca-l0-wddm-baseline-check.exe `
  --candidate build-orca-wddm-perf/orca-l0-wddm-memory-check.exe `
  --image C:/path/to/image_0.zebin --out build-orca-wddm-measurement --rounds 3 `
  --driver C:/path/to/installed/ze_intel_gpu64.dll
```

The script serializes GPU experiments through Strata's shared lock and saves
every retained sample and each process's stdout/stderr. The measured compiler
was IntelLLVM 2025.3.2; the report records the exact build arguments and hashes.
Use the installed vendor DLL, the same driver and image, and no concurrent
GPU experiments. GPU clocks were not fixed; recheck the paired drift rather
than assuming the same speed on another run or machine.

The reusable Windows backend now exposes `Orca::DG2` and
`#include <orca_l0/dg2.hpp>` (`orca::l0::dg2::Runtime`). Build it independently
of the Level Zero SDK/runtime:

```powershell
cmake -S orca-l0 -B build-orca-dg2 -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DORCA_L0_BUILD_RUNTIME=OFF -DORCA_L0_BUILD_WDDM_BACKEND=ON `
  -DORCA_L0_WDDM_ZEBIN=C:/path/to/image_0.zebin `
  -DORCA_L0_DG2_MEMORY_ZEBIN=C:/path/to/memory-native/image_0.zebin
cmake --build build-orca-dg2
ctest --test-dir build-orca-dg2 --output-on-failure
./build-orca-dg2/orca-l0-dg2-check.exe --inspect-pack C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --strata-gate C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --strata-gemv C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --strata-device C:/path/to/strata-native-pack C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-transfers C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --strata-scratch C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --strata-private C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --strata-atomics C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --strata-xmx C:/path/to/strata-native-pack
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-3d C:/path/to/dispatch3d-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-captures C:/path/to/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-semisync C:/path/to/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-memory C:/path/to/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-multilist C:/path/to/image_0.zebin C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-reclaim C:/path/to/image_0.zebin C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-multiqueue C:/path/to/image_0.zebin C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-host-gate C:/path/to/image_0.zebin C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-observers C:/path/to/image_0.zebin C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --gpu-recording C:/path/to/image_0.zebin C:/path/to/memory-native/image_0.zebin
./build-orca-dg2/orca-l0-dg2-check.exe --strata-multiqueue-memory C:/path/to/strata-native-pack
```

`Runtime` owns ordered native queues, resident allocations, cached ISA/module data,
an active recording and immutable frozen lists. Each queue has its own completion sequence.
It provides `allocate`, `allocate_device`,
`import_host`, `set_memory_module`, `copy`, `fill`, `append`,
`close`, `finish`, `submit`, `ready`, `completed`, `wait`, `reset`, checked `cpu_access` and the explicit
idle-only `check_scratch_guards` diagnostic.
`Argument::local(bytes)` supplies dynamic shared memory. Recording copies
argument bytes; use stable parameter buffers for values that change on replay.
The array overload of `append` accepts local and group dimensions in hardware
**x,y,z** order; SYCL `range<3>` uses the reverse z,y,x order. For example, a
17-by-72 BF16 XMX product with eight subgroups uses local `{64,1,1}` and groups
`{1,2,3}`. Local thread count must be a power of two up to 1,024, total work
items must fit `uint32_t`, and a kernel's required local tuple must match.
The original 1D overload remains shorthand for `{x,1,1}`.
Kernels from the same module path share global/constant storage. Keep module
files unchanged for the runtime's lifetime. Submission allocates nothing and
does no per-kernel parsing, lookup or system call.

`finish()` closes and freezes the current recording, returns a `dg2::Commands`
handle and starts an empty recording on the default queue. Multiple frozen lists share
allocations; each list is bound to the queue selected before recording. Record stable regions once, submit them without a host
wait between regions, and join at the final consuming boundary:

```cpp
record_first_region(runtime);
auto first = runtime.finish();
record_next_region(runtime);
auto next = runtime.finish();
runtime.submit(first);
const auto tail = runtime.submit(next);
runtime.wait(next);
const auto through = runtime.completed(); // At least tail; earlier queue work is complete.
```

Every submission returns a monotonically increasing queue sequence. One
`completed()` query collects progress for all earlier submissions on that
**same ordered queue**. Windows reads its mapped monitored fence; Linux uses
`DRM_IOCTL_SYNCOBJ_QUERY`. Linux submissions signal an i915 timeline syncobj;
blocking joins use `SYNCOBJ_TIMELINE_WAIT` with an absolute monotonic deadline.
The kernel must report `I915_PARAM_HAS_EXEC_TIMELINE_FENCES`. This replaces
BO-wide `GEM_WAIT`, which can include unrelated later uses of a shared BO.
Native Linux GPU execution is still unvalidated.

`begin(queue, false)` selects caller-managed dependencies within the next
recording. Independent kernels may proceed without an automatic trailing
dependency flush; insert `barrier()` before a later kernel consumes their
outputs. The default `begin(queue)` retains per-kernel ordering. Explicit
barriers, completion writes and required state invalidations/workarounds are
preserved in both modes, and frozen lists retain their recorded commands.
Strata's DG2 adapter honors requested relaxed graphs by default;
`STRATA_ORCA_DG2_RELAXED=0` retains the preceding conservative behavior for
matched checks. Only `0` or `1` is accepted, cached before GPU initialization.

`signal_value(gpu_address, uint64_value)` records a flushing `PIPE_CONTROL`
with a 64-bit immediate write. `wait_value(gpu_address, uint32_value)` records
an in-list GPU equality wait. Both validate the registered allocation/range
and alignment; addresses and values are captured during recording. These
memory points can complete before the list's queue fence. Observing a tag
does **not** retire the list or permit recycling its other resources.
The caller retains tags and all waiters, gives independently updated tags
separate cache lines, and resets a tag only when its previous users are done.
Native queue-fence wait/retirement remains unchanged. The shared Linux/Windows
check is `orca-l0-dg2-check --gpu-inline-events inference.zebin memory.zebin`
(`orca-l0-drm-compute-check` on Linux), using modules baked for that machine.
Windows A770 passed 16 replays with a prefix signal and cross-queue consumer
completing before a blocked tail, one producer submission, unchanged queue
progress until the tail, exact snapshots and full guards. Linux GPU execution
of these methods remains unvalidated.

For a CPU/GPU pipeline, submit ready work immediately, prepare independent CPU
data, collect the completed prefix, and block only on the first result needed
by the consumer. Do not wait to fill a completion batch or delay a ready tail.
Windows owner waits briefly spin before using a native event; observer waits
use native events directly. Querying one queue already covers its earlier
submissions. Cross-queue batches are available through the owner API below;
Linux GPU validation and eventfd notification remain open.
Completion batching does not combine kernels into a GEMM.
In the [completion-mode demo](demo/dg2-completion-modes-windows-2026-11-01.json),
CPU preparation before prefix collection reduced combined-work latency 5.6%
and calling-thread cycles 68.2%; prefix collection alone increased latency
6.7%. These are demo results, not model throughput.

`completed(queues, sequences, count)` queries up to 16 queue handles without
waiting or allocating heap memory. Duplicate handles share one query and result.
Every handle is validated before querying, and errors leave the caller's output
unchanged. A zero-length batch accepts null pointers. Windows reads mapped
monitored fences; Linux packs unique pending timelines into one
`DRM_IOCTL_SYNCOBJ_QUERY` and skips fully completed queues using owner caches.
Results are per-queue completed prefixes, not an atomic snapshot across queues.
This owner-only API does not execute callbacks. Strata's native collector now
uses it as described below. Worker threads retain `Completion::ready/wait` observation.

```cpp
std::array<orca::l0::dg2::Queue, 3> queues{producer, consumer, independent};
std::array<uint64_t, 3> through{};
runtime.completed(queues.data(), through.data(), queues.size());
// Consume each queue's ready prefix; wait only at the next consuming boundary.
```

The Windows A770/32.0.101.9034 batch checkpoint passes **19/19** CTests,
including **48 three-queue snapshots** with 16 entries, duplicate handles,
gated tails, completed prefixes and unchanged output guards on rejection.
It also reruns 64 coordinate replays and the eight-replay 5-GiB buffer check.
The updated Linux GCC build/CPU CTest passes **4/4** without a GPU node;
Linux GPU queries remain unvalidated. At this checkpoint the batch API was not
yet wired into Strata's collector; this correctness check claims no model speedup.
[Batch-query report](demo/dg2-query-batch-windows-2026-11-01.json) and
exact source/image delta, logs and runners（本地归档：`demo/dg2-query-batch-windows-2026-11-01.json.gz`）
retain the implementation and checks. Production settings and executable are unchanged.

An isolated Windows observer-wait experiment tried up to **128 short probes**
with deadline/teardown checks before the existing TLS event fallback. The real
Strata old-native/candidate/old-native comparison used ten warmups and five
retained requests per process at the same 5771-slot cache. Decode medians were
**16.54 / 16.60 / 16.52 tok/s**: **+0.43%**, with **−0.12%** control drift.
All 45 requests matched the same 76 output tokens and 57/57 accepted drafts.
This single bracket does not establish a material gain sufficient to change
the default observer policy; the original event wait was restored exactly.
Candidate and restored Windows CTests both pass **19/19**; both builds also
pass the 5-GiB-buffer regression. Linux sources are unchanged from the batch checkpoint;
no additional Linux GPU validation or original-L0 comparison is claimed.
[Observer-wait report](demo/dg2-strata-observer-hybrid-windows-2026-11-01.json) and
candidate source, exact model requests and both validation runs（本地归档：`demo/dg2-strata-observer-hybrid-windows-2026-11-01.json.gz`）
retain the experiment. The broader native-DG2 model regression remains open.

The subsequent Strata revision integrates owner batch queries into its native
collector. It looks ahead at most **16 pending entries**, deduplicates queues
on the stack and reuses each completed-prefix snapshot. Larger queue sets use
successive batches. Collection stops at the first incomplete entry, without
waiting for a tail or for a batch to fill. A real GPU regression checks **18
retained graph queues**, two waves and **72 staged readbacks**, including fresh
guards and delivery before a separately gated tail. Windows CTest passes
**19/19**, with 64 coordinate replays and eight 5-GiB-buffer replays.

Native service startup now prepares verifier window/commit graphs and ordinary
MTP round/prefill/step graphs before `READY`, under the existing default-on
`STRATA_WARM_GRAPHS`; `0` retains lazy preparation. Request-dependent work and
some dynamic MTP variants remain. Preparation does not execute dummy inference
or warm the expert cache. The timing harness keeps engine counters and separately
measures process-to-READY, request-to-DONE and delivery after the first token
burst; a CPU-only protocol check covers multiple, single and empty deliveries.

The five-process Windows timing audit retains five requests after ten warmups
per process. All **75 requests** match the same 28-token input, natural 76-token
output, **57/57** accepted drafts and 5771-slot cache. Original-L0 before/after
decode medians are **46.41 / 47.12 tok/s**; prepared native DG2 is **16.27 tok/s**,
**−65.20%** against the geometric control baseline, with **+1.54%** control drift.
Excluding the first delivery still gives **47.20 / 47.91** versus **16.61 tok/s**,
**−65.06%**. Prompt medians are **874.5 / 858.4** versus **1913.9 ms**, and TTFT
medians **921.2 / 903.0** versus **2074.0 ms**. These host observations are not GPU
timestamps. Initialization placement and the first-output boundary do not
explain the substantial warmed regression.

The lazy-native controls bracket prepared DG2 at **15.80 / 16.51 tok/s**.
Their **+4.45%** drift exceeds the audit's 3% stability bound, so the prepared
arm's **+0.74%** nominal steady-state change does not establish a speedup.
First-request TTFT is **2596.7 / 2588.0** versus **2257.1 ms**; graph preparation
itself takes **494.2 ms** before READY. These first requests follow module
audits with retained OS caches; pre-recording also changes allocation/capture
order. The changed Strata frontend remains Linux-uncompiled and GPU-unvalidated;
raw `orca-l0/` sources match the preceding Linux CPU checkpoint exactly.
[Service/timing report](demo/dg2-service-preparation-windows-2026-11-01.json) and
source/image delta, all requests, checks and cached Intel/Microsoft references（本地归档：`demo/dg2-service-preparation-windows-2026-11-01.json.gz`）
preserve this checkpoint. See the [backend investigation](../docs/ORCA_NATIVE_BACKEND_STATUS.md)
for the remaining ISA placement and barrier/queue investigation. The following
checkpoint measures local scratch/private placement separately.

`Runtime::set_memory_module(path, local_thread_memory=true)` now places new
scratch/private pools in local VRAM. The memory module's copy/fill kernels must
have no scratch/private requirements, preventing initialization recursion.
Initialization uses a reserved native queue that progresses while another queue
is gated. An explicit guard diagnostic reads back only the leading/trailing
guards through one **192-KiB** staging buffer, preserving both open and closed
caller recordings. At this checkpoint ISA, indirect parameters, module data and
command buffers still used system memory; the later ISA revision below measures
code placement separately. Strata's experimental DG2 backend defaults to local
pools; `STRATA_ORCA_DG2_THREAD_MEMORY=host` selects the system-memory control,
and `device` explicitly selects local pools.

Windows A770/32.0.101.9034 passes **19/19** CTests plus three-queue checks in both
placements: **24 Cvec and 24 Q3_K results per placement**, with changing inputs,
exact outputs, all guards, first-use initialization before a blocked queue and
the first guard check on a closed list. The Linux raw GCC build/CPU CTest passes
**4/4**; Linux frontend/GPU execution remains unvalidated. All **1257 native
zebin images are byte-identical** to the service-preparation checkpoint; the
24 retained unsupported SPIR-V variants remain unsupported.

The fixed Cvec demo uses `n=4096`, `hc=257`, scratch2048, device input/output and
a GPU fill in each replay. In its second host/device/host bracket, median
submission-plus-wait times are **603.90 / 207.60 / 596.15 µs**: **−65.40%**
latency, with **−1.28%** control drift. Its first bracket shows −62.50% with
−5.43% drift; both are retained. Each process discards ten warmups and retains
50 replays. This isolated result is not a model-throughput claim.

Two complete model brackets use one native executable and the same images,
28-token prompt, natural 76-token output, **5771 slots**, balanced parameters,
zero prefix reuse and **57/57** accepted drafts. All **90 requests** match;
each process discards ten warmups and retains five requests. The repeat bracket's
host/device/host decode medians are **16.75 / 28.53 / 16.67 tok/s**, **+70.75%**
against the geometric control baseline, with **−0.46%** control drift. Prompt
medians are **1865.2 / 1150.5 / 1892.3 ms** (**−38.76%**), and TTFT medians
**2019.3 / 1235.2 / 2046.9 ms** (**−39.25%**). Excluding the first token delivery
still gives **+70.38%**. Preparation/prefill remain outside engine decode timing.
The first bracket gives +70.85% with −4.47% control drift, which prompted the
complete repeat. All timings are host observations with unlocked clocks.
[Placement report](demo/dg2-thread-memory-windows-2026-11-01.json) and
sources, images, both model brackets, demo, checks and failed attempts（本地归档：`demo/dg2-thread-memory-windows-2026-11-01.json.gz`）
preserve the measurements. This establishes one substantial native bottleneck;
it does not establish parity with original L0 or broad PPL/long-context behavior.

The initial auto-cache smoke reached **6580 slots** but left only **1165 MiB**
after graph preparation, below the requested **1536 MiB**. A follow-up budgets
an additional **512 MiB** before native auto sizing and its existing admission
check, calibrated from the observed later local-pool cost on this A770/profile.
This applies to auto sizing with local pools; explicit slot counts and host-pool
mode retain their budget. The requested reserve stays 1536 MiB. The revised
auto smoke opens **6177 slots**, reports **1697 MiB** free after preparation,
and matches all 76 output tokens. A fixed-cache regression retains **5771 slots**,
reports **2236 MiB** free and matches the same output. Windows CTest remains
**19/19**. Raw Linux sources match the preceding 4/4 CPU checkpoint exactly;
the changed Strata frontend remains Linux-uncompiled.
[Auto-budget report](demo/dg2-pool-reserve-windows-2026-11-01.json) and
source delta, exact runners and checks（本地归档：`demo/dg2-pool-reserve-windows-2026-11-01.json.gz`）
retain this correction. These two cold smoke requests are correctness/budget
checks, not new throughput measurements; 28.53 tok/s refers to its exact parent
revision and fixed conditions above. Startup availability is a sampled OS
figure, not continuously measured physical headroom. The 512-MiB allowance is
calibrated for the current model/queues; growing them requires preallocating or
querying those pools before cache sizing.

The following revision adds independent local ISA placement through
`Runtime::set_memory_module(path, local_thread_memory=true, local_isa=false)`.
Strata's experimental DG2 backend enables both placements by default;
`STRATA_ORCA_DG2_ISA_MEMORY=host|device` changes only code placement, while
`STRATA_ORCA_DG2_THREAD_MEMORY` retains its separate control. Ordinary kernel
code uses immutable **1-MiB local slabs**, uploaded once through a reusable host
staging slab with zeroed instruction-prefetch padding. The bound copy/fill
module keeps host-backed code to bootstrap uploads. The reserved upload queue
preserves the caller's recording and indirect heap, including while another
queue is gated. At this ISA checkpoint module data, parameters and command
buffers retained their prior backing; the following state revision moves module
data separately.

The Windows A770/32.0.101.9034 five-process comparison fixes **5771 expert
slots**, the same 28-token input, natural **76-token output**, balanced
parameters, no prefix reuse and **57/57** accepted drafts. Each process discards
ten warmups and retains five requests; all **75 requests** match:

| Arm, execution order | Decode tok/s | Prompt ms | TTFT ms |
| --- | ---: | ---: | ---: |
| Original L0, before | 49.013 | 826.7 | 870.4 |
| Native DG2, host ISA | 28.287 | 1162.8 | 1247.9 |
| Native DG2, local ISA | **35.983** | **1017.5** | **1079.8** |
| Native DG2, host ISA, after | 28.329 | 1166.2 | 1251.6 |
| Original L0, after | 48.584 | 830.3 | 874.1 |

Local ISA improves native decode by **27.11%**, with **+0.15%** native-control
drift; prompt time falls **12.62%** and TTFT **13.60%**. Excluding the first
delivery still gives **+26.84%** throughput. Against the original-L0 geometric
baseline of **48.798 tok/s**, native decode remains **26.26% slower**; L0 controls
drift **-0.88%**. Preparation, loading and prefill are excluded from engine
decode; timings are host observations, clocks are unlocked and OS caches remain
populated. This is one fixed-prompt comparison, without broad PPL/long-context
or native GPU-timestamp validation.

Windows CTest passes **19/19**, alongside 64 coordinate and eight 5-GiB-buffer
replays. Explicit local-ISA checks cover both host/device scratch-private
placements, three queues, **48 Cvec and 48 Q3_K results**, full guards and gated
first use. The existing **900-submission** reclamation check also passes with
local ISA and a stable allocation plateau. Its initial host-mode failure,
caused by unnecessarily separating bootstrap code when ISA was host-backed,
and the correction remain in the archive. All **1257 zebin images** remain
byte-identical to the previous checkpoint; 24 unsupported SPIR-V variants
remain. The updated raw Linux GCC build/CPU CTest passes **4/4**; Linux frontend
and GPU execution remain unvalidated.

Native auto sizing additionally retains **192 MiB** for local ISA slab packing
on this 81.4-MiB padded-image pack; the existing local-thread allowance remains
512 MiB. The requested reserve stays **1536 MiB**. The auto smoke opens **6008
slots**, reports **1857 MiB** free after preparation and matches the same output
and drafts. This is a sampled startup availability check and a calibrated
allowance, requiring revision if packs or queue first-use patterns grow.
[Local-ISA report](demo/dg2-local-isa-windows-2026-11-01.json) and
source/image deltas, exact runners, five-arm requests, Linux checks and earlier attempts（本地归档：`demo/dg2-local-isa-windows-2026-11-01.json.gz`）
record the parameters and source/executable/image hashes for reproduction under
the same conditions. The production profile and executable are unchanged.

The following revision also places ordinary module globals/constants in local
VRAM whenever the bound Runtime has an internal upload queue. The bound memory
module remains host-backed for bootstrap. Scratch descriptors follow
`local_thread_memory`. All paths reuse the ISA staging slab and reserved queue;
module data is initialized once, and GPU changes persist across kernels and
queue replays. No public Runtime API or auto-cache budget changed. Disabling
both local placements retains the host-backed control.

The new `tests/module_data.cl` check exercises initialized/zero-valued globals
and volatile constants across two kernels/queues, **16 changing updates and 17
readbacks per placement**, with exact values, full output guards and stable
allocations. Run `orca-l0-dg2-check --gpu-module-data module-data.zebin
memory.zebin` with modules baked for the current driver; Linux builds the same
check as `orca-l0-drm-compute-check`. At that checkpoint Windows CTest passed **19/19**,
alongside local-ISA/thread-memory, 900-submission reclamation, 64 coordinate and
eight 5-GiB-buffer checks. Updated raw Linux GCC/CPU CTest passes **4/4**;
Linux frontend/GPU execution remains unvalidated. All **1257 numerical zebins**
are byte-identical to the preceding ISA checkpoint.

Two complete five-process model brackets retain the same **5771 slots**, 28-token
input, natural **76-token output**, **57/57 drafts**, balanced parameters and no
prefix reuse. Each process discards ten warmups and retains five requests; all
**150 requests** match. The final native-parent/candidate/native-parent decode
medians are **36.426 / 36.409 / 36.351 tok/s**: **+0.06%** against the geometric
control baseline, with **-0.21%** control drift. Prompt time changes **+0.31%**
and TTFT **+0.25%**. This is effectively unchanged, without evidence of a stable
model-speed gain. Original L0 controls are **48.624 / 48.596 tok/s**
(**-0.06%** drift); native remains **25.10% slower**. Decode excludes preparation,
loading and prefill; these are host observations with unlocked clocks and
populated OS caches.

The first bracket gives nominal **+1.34%** with **+2.16%** native-control drift;
it is retained. Its prototype used a 4-KiB descriptor host buffer; the final
implementation stages **256 bytes** and zero-pads to the same 4-KiB GPU image.
Compiler disassembly confirms that the prototype's dispatch stack reservation
falls **4888 → 1128 bytes**, removing its Windows `__chkstk` call. No isolated
model benefit is attributed to that stack change.

The auto-cache smoke opens **5877 slots**, reports **1830 MiB** free after
preparation against **1536 MiB** requested and matches the output/drafts.
Slot counts follow current OS availability; the model brackets independently
fix 5771 slots. Startup availability is not continuous physical headroom.
[Local-state report](demo/dg2-local-state-windows-2026-11-01.json) and
source/image deltas, both brackets, module fixture, disassembly and exact runners（本地归档：`demo/dg2-local-state-windows-2026-11-01.json.gz`）
retain the reproduction parameters and hashes. Production remains unchanged;
native GPU timestamps, broader PPL/long-context validation, parameter storage,
state programming and barriers remain investigation areas.

A subsequent original-L0/native/original-L0 context bracket preserves those
local-state sources, binaries and model images. All **69 requests** match token
IDs and draft acceptance, with the same 5771 slots and balanced parameters.
Decode excludes loading, preparation and prefill:

| Input / output | Original L0 geometric baseline | Native DG2 | Decode change | Original / native prefill |
| --- | ---: | ---: | ---: | ---: |
| Short28 / natural76 | 48.117 tok/s | 35.370 tok/s | -26.49% | 844.5 / 1049.9 ms |
| Code2K / 64 | 26.419 tok/s | 21.473 tok/s | -18.72%, inconclusive | 28107.8 / 27678.2 ms |
| Long8K (8000 tokens) / 64 | 28.642 tok/s | 21.546 tok/s | -24.77% | 75878.7 / 78466.5 ms |

Short and 8K decode controls drift **+0.46%** each; 2K drifts **+9.42%**, outside
the 3% bound. Long cases retain three requests after one warmup, with continuing
expert-cache adaptation; short retains five after ten warmups. Contents and MTP
acceptance differ across cases, so they do not isolate context length alone.
On 8K, TTFT increases **3.44%** and total request latency **4.17%**: long prefill
dilutes the request-level difference without removing the decode gap. These
are host observations, not GPU timestamps or a converged long-context plateau.
[Context report](demo/dg2-context-windows-2026-11-01.json) and
all requests, logs, exact runner and pinned NEO sources（本地归档：`demo/dg2-context-windows-2026-11-01.json.gz`）
preserve this checkpoint, which predates the eager-batch revision below.

SYCL, UR and NEO already implement graph/dependency shortcuts, command-list
batching, in-list GPU events and state/resource management. Optional completion
batching and Direct Submission rings are separate mechanisms. Source defaults
and the installed driver's actual behavior must be distinguished; see the
[pinned-source explanation](../docs/ORCA_NATIVE_BACKEND_STATUS.md#what-sycl-ur-and-neo-already-do).
Command batching does not automatically fuse arbitrary GEMVs into GEMM.

Strata's experimental DG2 adapter now counts **kernel, copy and fill** operations
together in bounded eager batches. The default remains **64 operations**;
`STRATA_ORCA_DG2_BATCH_OPS=1..4096` is parsed once per Runtime, with invalid
settings rejected before GPU initialization. Signals, waits, CPU readbacks and
queue switches flush their tails without a batching thread or timer. Recorded
graphs retain their explicit signal/dependency segments. Contiguous 2D copies
become one flat transfer; strided 2D readbacks share bounded row batches and
flush at the end. This adds no arithmetic fusion or change to the reusable
`Orca::DG2` API.

The embedded `strata-orca-queue-check IMAGE_DIRECTORY --batch-bench` replays
**601 dependent fill/copy/kernel operations**, with changing inputs and complete
guards, taking **10 / 3 / 1 submissions** at limits 64 / 256 / 1024. Timing covers
host encoding, submission and GPU wait, excluding setup and guard verification;
it is not GPU timestamps or model throughput. The full check also covers
padded/contiguous 2D transfers. Windows CTest passes **19/19**; limits 1/64/4096
and eight rejected settings pass independently. The old adapter counted only
kernels toward 64, so 10 → 3 is a comparison between limits of the new binary,
not a measured speedup over the old runtime.

Across the retained prototype rounds, larger limits fail to establish a
consistent latency advantage. In the final bracket, 256 versus 64 gives
**3.401 versus 3.141 ms**, **8.25% slower**, with **-1.07%** control drift.
The 1024 comparison is also retained but misses the 3% drift bound. The 64-op
default is a conservative latency choice, not a universal optimum.

The matched old-native/new-native/old-native model bracket measures
**35.998 / 35.947 / 36.103 tok/s**, **-0.29%** against the geometric control
baseline, with **+0.29%** control drift. Prompt/TTFT/request time change
**+0.06% / +0.06% / +0.01%**: effectively unchanged, with no established model
speedup. Ten warmups and five retained requests per process give **45 exact
28-token/natural76 requests**, all **57/57 drafts**, with 5771 slots, balanced
parameters and no prefix reuse. Decode excludes preparation/loading/prefill.
All **1242 non-queue model zebins** and the other non-manifest model files are
byte-identical to the context checkpoint. The 46 changed model manifests differ
only in recorded compilation time, with filenames, flags and sizes checked.
This compares two native DG2 revisions, not native DG2 versus original L0 or
SYCL. At this checkpoint, recorded signal segments remained separate submissions;
the in-list event revision below changes eligible GPU signals.
[Batch report](demo/dg2-auto-batch-windows-2026-11-01.json) and
source/image deltas, every micro bracket, model requests, logs and seven cached upstream sources（本地归档：`demo/dg2-auto-batch-windows-2026-11-01.json.gz`）
retain all attempts and hashes. The shared build helper was captured after
validation; model/micro runners were saved per run. Raw Linux sources still
match the preceding 4/4 CPU checkpoint; the changed Strata adapter/frontend,
Linux GPU execution, broad PPL and post-change long contexts remain unvalidated.

The subsequent DG2 event revision keeps eligible recorded GPU signals and waits
inside a command list. It uses pooled 64-byte-separated tags, flushing
`PIPE_CONTROL` writes and `MI_SEMAPHORE_WAIT`; staged CPU readbacks and required
host/queue transitions retain their submission boundaries. Queue fences still
retire resources: observing an intermediate tag never retires its unfinished
tail. `STRATA_ORCA_DG2_INLINE_EVENTS=0|1` is cached once, defaulting to **1**.
Partial-tag waits use a short spin/sleep fallback; queue-tail waits retain OS
fence waiting. This does not establish a hardware-only wakeup latency.

Windows passes **20/20 CTests**, including 16 raw and 16 adapter gated-tail
replays that check cross-queue prefix completion, immutable old event points,
guard bytes and delayed resource retirement. Linux passes **4/4 raw CPU
checks**; the changed Strata frontend and Linux GPU execution remain untested.
The `--event-bench` queue check records 200 dependent kernels and 200 signals.
One candidate binary, events off/on/off, takes **200 / 1 / 200 submissions**
and **9.914 / 0.622 / 9.972 ms** median replay/submit/wait time: **-93.75%**
against the geometric control, with **+0.59%** control drift. There are 72
retained samples, with exact outputs and complete guards; this is an
event-heavy microbenchmark, not a model-throughput result.

Two old-native/new-native/old-native model brackets retain **90 exact requests**,
with unchanged 1242 model zebins and the same short-anchor/balanced parameters.
The first bracket shows **+0.53%** decode, with late deterioration and invalid
TTFT/request controls. The rerun measures a geometric control of **35.534 tok/s**
versus **35.326 tok/s**, **-0.58%**, with **+1.13%** control drift; TTFT changes
**+0.58%**. These checks establish no meaningful model speedup. Neither bracket
compares against original SYCL or original L0. Preparation and prefill remain
outside decode timing; new long-context and broad PPL claims remain open.
[In-list event report](demo/dg2-inline-events-windows-2026-11-01.json) and
both brackets, exact runners, source/image deltas, checks and logs（本地归档：`demo/dg2-inline-events-windows-2026-11-01.json.gz`）
preserve the evidence, including the corrected invalid-address preflight.

An additional short-context phase diagnostic compares production Orca L0
through Intel UMD against native DG2, bracketed L0/native/L0. It enables
`STRATA_DECODE_TIMING=1` and `STRATA_VERIFY_NODES=1`; the only product-source
change adds capture-time graph counts under the existing diagnostic flag.
Windows remains **20/20 CTests**, and 45 requests preserve every output token
and 57/57 drafts. These host timings include diagnostic logging and do not
replace release-throughput measurements or GPU timestamps.

| Per decode window | L0 geometric control | Native DG2 |
| --- | ---: | ---: |
| Total window | 80.584 ms | 107.990 ms |
| Verifier | 66.684 ms | 92.510 ms |
| Draft | 12.985 ms | 15.050 ms |
| Commit/emit | 0.130 ms | 0.170 ms |
| Verifier submission call | 0.06695 ms | 0.03130 ms |

Each request has 20 windows, averaging 3.85 input rows and 3.8 output tokens per
window. Diagnostic decode is **47.157 / 35.188 tok/s**, **-25.38%**, with
**+1.19%** L0 control drift. Submission overlaps device progress and must not
be added to the other rows. Independent rounded medians are approximate phase
accounting. The verifier adds **25.826 ms** of the **27.406 ms** window increase;
this localizes the main difference to submission-through-completion behavior,
without separating GPU work, memory stalls and completion observation.

All six T=1..6 verifier graphs have **one segment and zero signals, queued
event waits, staged readbacks or host uploads**. T=4 contains 1769 kernel nodes
and 1557 barrier nodes. These counts cover the verifier graph, not all request
submissions. Across all 15 matched requests per arm, expert VRAM-hit counts
match exactly; the retained median is **91.74%**. Thus event segmentation,
waiting for eager batches to fill and different expert residency do not explain
this verifier comparison.

A concrete source difference at that checkpoint: L0 honored requested relaxed
recording; the DG2 adapter ignored `Ordering` and emitted a flushing barrier
after every kernel, including work marked independent by the upper queue. Whether
some of those barriers can be removed safely and recover model performance
needs an isolated check. TG dispatch and preferred-SLM policy differences
remain separate candidates. No workaround was removed by this diagnostic.
[Phase localization report](demo/dg2-phase-localization-windows-2026-11-01.json) and
all requests, exact runners, source delta and logs（本地归档：`demo/dg2-phase-localization-windows-2026-11-01.json.gz`）
retain the evidence. Numerical-image identity was checked between the native
event and diagnostic revisions; the L0 control uses its preserved production
image pack. Linux frontend/GPU, broad PPL and new long contexts remain open.
Production configuration and executable remain unchanged.

The next revision honors requested relaxed ordering in the native adapter.
`Runtime::begin(queue, false)` omits the automatic trailing dependency flush;
the default raw API remains ordered. Explicit joins, queue completion flushes,
leading invalidations and the repeated-SBA workaround remain. Eager operations
stay ordered. `STRATA_ORCA_DG2_RELAXED=0|1` is cached before GPU initialization,
defaulting to **1**. This does not measure simultaneous kernel occupancy.

Two same-binary off/on/off model brackets retain **90 exact requests** with
unchanged numerical kernels and the same short-anchor parameters. The fixed
revision measures **35.469 geometric control / 36.087 tok/s**, **+1.74%**, with
**+0.12%** control drift. TTFT falls **0.95%** and request time **1.67%**.
The prototype also measured **+1.74%**, with **-0.33%** drift. This small
improvement does not explain the earlier roughly 25% native-versus-L0 gap;
these two brackets compare native ordering modes, not L0 or original SYCL.

Windows remains **20/20 CTests**, both ordering modes pass dependency/replay
checks, eight malformed settings are rejected, and shared Linux CPU checks
remain **4/4**. A rejected transfer during empty relaxed capture exposed an
ownership bug: the recorder survived the empty capture. A failing-before /
passing-after regression now checks release and subsequent eager dependencies.
The prototype result and failed regression are preserved separately.

An entry-point-matched production/native SPIR-V audit also finds **1266/1266**
equal compiler options, **1258** byte-identical images and eight differences
confined to `OpName` debug names. All non-debug instruction words match. This
does not prove identical JIT-generated ISA or GPU state.
[Ordering report](demo/dg2-relaxed-ordering-windows-2026-11-01.json) and
both brackets, source/image deltas, identity audit, exact runners and logs（本地归档：`demo/dg2-relaxed-ordering-windows-2026-11-01.json.gz`）
preserve the evidence. Post-change long contexts, broad PPL and Linux GPU
execution remain unvalidated; production is unchanged.

A subsequent isolated preferred-SLM check sets **0KB** only when static plus
dynamic SLM usage is zero; nonzero usage keeps **MAX**. Windows passes **20/20**
and Linux CPU **4/4**, including zero/static/dynamic packet checks. All kernel
images and ABI files remain byte-identical. The native/control/native bracket
retains 45 exact requests and measures **35.944 geometric control / 35.988
tok/s**, **+0.12%**, with **-0.40%** drift: no established performance gain.
This aligns one field with NEO's zero-SLM branch; physical allocation and
occupancy were not measured. [SLM report](demo/dg2-zero-slm-windows-2026-11-01.json)
and complete checks, deltas, pinned references and model bracket（本地归档：`demo/dg2-zero-slm-windows-2026-11-01.json.gz`）
preserve the result. Nonzero-SLM occupancy/revision tuning remains separate.

The next isolated change fixes explicit GPU dependency barriers, which still
used the default host-visible `DCFlushEnable=true` after automatic dispatch
barriers had adopted the calibrated device policy. `Runtime::barrier()` now
uses the same policy: **false on the tested Windows DG2 backend**, unchanged
**true on Linux**. CS stall, HDC/untyped flushing, required state workarounds,
event-tag flushing and queue-tail host flushing remain. Pinned NEO's ordinary
compute-barrier flags also default DC flush to false; this is source evidence,
not an installed-driver packet trace.

Windows **20/20 CTests** and Linux raw CPU **4/4** pass, with identical kernel
images and ABI files. A preserved-native/candidate/preserved-native bracket
measures **36.435 / 38.563 / 36.357 tok/s**: **+5.95%** against the geometric
control, **-0.22%** control drift. All **45 natural76 requests** and **57/57
drafts** match, with the same balanced parameters, 5771 slots, ten warmups and
five retained per arm. Decode excludes preparation/loading/prefill. TTFT falls
**2.86%**, prefill **2.85%** and request time **4.50%**. This identifies one
concrete source of native overhead; it does not establish a complete explanation
of the earlier L0 gap. These are separate native-revision brackets; their gains
must not be added or advertised as a new original-SYCL comparison.
[Dependency-flush report](demo/dg2-dependency-flush-windows-2026-11-01.json) and
all checks, source delta, cached NEO references and exact model requests（本地归档：`demo/dg2-dependency-flush-windows-2026-11-01.json.gz`）
retain the evidence. Broad PPL and Linux GPU performance remain unmeasured;
production configuration and executable are unchanged.

The updated native revision now has a fresh **81-request** original-L0 /
native-DG2 / original-L0 context bracket, with matched outputs and drafts:

| Input / generated tokens | L0 geometric control | Native DG2 | Decode change | Control drift |
| --- | ---: | ---: | ---: | ---: |
| Short28 / 76 | 47.060 tok/s | 38.699 tok/s | -17.77% | -1.17% |
| Code2K / 64 | 30.613 tok/s | 25.601 tok/s | -16.37%* | -3.45%* |
| Long8K / 64 | 29.713 tok/s | 24.077 tok/s | -18.97% | -0.78% |

*The 2K decode control exceeds the 3% drift bound; its precise regression is
not established. Long cases discard three warmups and retain three requests;
expert-cache adaptation still raises retained throughput, so these are bounded
curves, not converged plateaus. The short case discards ten and retains five.
All arms use 5771 slots, balanced min-p=0.5 and requested 1536-MiB reserve,
with no prefix reuse. Decode excludes preparation/loading/prefill.

On 8K, prefill / TTFT / whole-request time increase **3.81% / 3.83% / 4.34%**.
The decode gap persists at long context; whole-request percentages alone hide
it behind prefill. FP16 expert-pair work matches across all arms: **9159 pairs
and 22 leftovers** on 2K, **9794 and 24** on 8K. Prompt contents and MTP acceptance
differ across cases, so this does not isolate context length as a sole cause.
Historical runs used different warmup counts and are not isolated measurements
of the combined runtime changes. [Latest context report](demo/dg2-context-latest-windows-2026-11-01.json)
and all curves, exact runner, pairing checks and logs（本地归档：`demo/dg2-context-latest-windows-2026-11-01.json.gz`）
preserve the comparison; this is production Orca L0 versus native DG2, not
original SYCL versus Orca.

A staged capture diagnostic now attributes global-state changes across all
six verifier graphs. T=4 has **1773 dispatches / 1259 full state blocks**,
including **806 ISA-base, 784 parameter-base, 522 scratch-heap and 120 GRF
changes** (overlapping counts). Five other tracked fields never change.
The existing encoder still repeats every state group, even when only heap
bases change. This is a concrete optimization target, not a measured timing
cause: tracing is disabled in performance runs. Follow-up adjustments use
Short28 only, preserving required synchronization and workarounds.
[Capture attribution](demo/dg2-state-trace-windows-2026-11-01.json) and
full diagnostic evidence（本地归档：`demo/dg2-state-trace-windows-2026-11-01.json.gz`）
record the exact source and correctness checks.

The first short-only isolated test suppresses unchanged state groups while
keeping all invalidating barriers and workarounds. Windows native checks pass
**14/14**, Linux CPU **3/3**, plus real device numerical checks and **45 exact
natural76 requests / 57/57 drafts**. Decode measures **38.695 → 38.013 tok/s**
against the geometric native control (**-1.76%**, control drift **-1.54%**).
This round shows no gain; the change remains staged, without changing defaults.
It does not test reducing heap-induced invalidation frequency.
[Isolated result](demo/dg2-partial-state-windows-2026-11-01.json) and
all checks and model requests（本地归档：`demo/dg2-partial-state-windows-2026-11-01.json.gz`）
include the unsuccessful initial legacy-harness build as well.

Copied handles refer to the same list and its latest submission. Invalid,
foreign and destroyed-runtime handles are rejected. An in-flight list cannot
be resubmitted; completing a later sequence retires earlier sequences. The
original implicit `close/submit/wait/reset` API remains available. Runtime methods
require one host owner, including `Runtime::ready/wait/completed`. Independent
`Completion::ready/wait` observations and `HostGate::signal` may use worker threads.
Recording and fresh allocation may proceed while other frozen lists execute.
Checked CPU mappings still require every queue to be observed complete.
Explicit list discard releases frozen lists and their
capture leases; backing allocations remain pooled until Runtime destruction,
as described below. Public allocation release is available as documented below;
full-engine integration and internal-pool trimming remain unfinished.

`default_queue()` and `create_queue()` return independent native queues that
share this Runtime's allocations. Select one with `begin(queue)` before recording;
it requires an empty open healthy recording. Creating a new queue still requires
all queues observed idle. Queues retain
their own scratch/private pools, context and completion fence/timeline until
Runtime destruction. They currently select the same calibrated compute engine;
creating several queues does not establish simultaneous execution or use of a
dedicated copy engine.

```cpp
auto producer = runtime.default_queue();
auto consumer = runtime.create_queue();
record_upload(runtime);
auto upload = runtime.finish();
runtime.begin(consumer);
record_compute(runtime);
auto compute = runtime.finish();
auto uploaded = runtime.submit(upload, producer);
auto computed = runtime.submit(compute, consumer, {uploaded}); // GPU-side dependency.
prepare_independent_host_data();
runtime.wait(computed); // Join only where its result is needed.
runtime.wait(uploaded); // Publish producer progress too, before checked CPU access.
```

The queue-aware `submit` returns an immutable `Completion`: its queue/sequence
remains valid after list replay, discard and slot reuse. `wait(completion)` and
`ready(completion)` target that exact point; `completed(queue)` collects its
queue's completed prefix. `wait(commands)` instead targets the list's latest
submission. A list cannot switch queues. The original `submit()` and
`submit(commands)` overloads target the default queue only.

GPU dependencies must be already-submitted points from this Runtime, with at most
16 dependencies per submission, counting recorded host gates before deduplication.
They are validated before submission, foreign queues are
deduplicated using their latest requested point, and same-queue dependencies
need no extra wait. Windows enqueues monitored-fence waits through D3DKMT;
Linux attaches timeline waits/signals to the execbuffer. Linux contexts share
an explicit VM and require `I915_PARAM_HAS_EXEC_ASYNC`; residency objects use
`EXEC_OBJECT_ASYNC`, so callers must supply actual cross-queue dependencies.
Shared mutable module globals also need ordering. Consumer completion does not
update the producer's cached progress: observe each queue before checked CPU access.
Recording and fresh allocation do not require a global drain. Runtime calls still require one host owner;
`HostGate::signal()` and independent `Completion::ready/wait` calls can run on
callback threads. The complete callback worker remains open.

Windows A770/32.0.101.9034 passed **61 submissions** across three queues,
including 12 GPU fork/join waves, local-VRAM snapshots, later producer tails,
queue-specific sequences, duplicate dependencies, immutable old completion
points, list recycling, invalid/foreign handles and full guards. Each wave
submits all five lists before any host wait. A separate check passed **24
submissions** containing 24 actual Cvec scratch and 24 Q3_K private-memory
executions with different queue inputs, exact analytic outputs and full
input/output/pool guards. It also checks allocation growth for distinct writable
thread pools: serialization alone could hide pool aliasing. These checks prove
no backlog depth, concurrent kernel overlap or new model-throughput gain.
[Multiqueue report](demo/dg2-multiqueue-windows-2026-11-01.json) and
exact sources, logs and images（本地归档：`demo/dg2-multiqueue-windows-2026-11-01.json.gz`）
record this checkpoint; full Strata adaptation and Linux GPU validation remain open.

`create_gate()` returns an unsignaled, one-shot `HostGate`. Record
`depend_on(gate)` before kernels/copies, on the selected queue. A frozen list
retains each distinct gate until discard; a gated list may contain no kernels.
Duplicate gates are ignored, and queue selection cannot change after a gate is
recorded. Creation and recording require an open healthy recording. Gates cannot be
reset; replays keep the original signal state. `reset()` drops unsubmitted waits.

```cpp
auto gate = runtime.create_gate();
runtime.begin(compute_queue);
runtime.depend_on(gate);
record_work_after_callback(runtime);
auto tail = runtime.finish();
auto point = runtime.submit(tail, compute_queue);
// A worker with its own gate copy releases it after its host work, including on failure.
gate.signal();
runtime.wait(point);
```

`signal()` is idempotent and uses a mutex per gate to protect release/teardown;
it does not mutate Runtime recording, cached progress or retirement ledgers.
Finish callback threads before destroying the Runtime. Destroyed-runtime gate
signals are rejected. Teardown attempts to release abandoned gates before
draining contexts; this is cancellation, not successful callback execution.
The Runtime does not execute or propagate callback bodies itself yet.

Windows gates use a separate monitored fence, a D3DKMT GPU queue wait and
`D3DKMTSignalSynchronizationObjectFromCpu`. Linux i915 cannot enqueue an
execbuffer wait on an unestablished future syncobj point; its gate instead
records the pinned NEO `MI_SEMAPHORE_WAIT` packet on a dedicated host word.
Signal stores one and flushes that WB cache line, avoiding a deadlocking
`GEM_SET_DOMAIN` call on a pending BO. This is **GPU semaphore polling**, with
no CPU poll loop. Linux preemption, other-queue progress and coherence still
need real GPU validation. One 4 KiB page per live gate is retained for reuse
after its last handle/list expires; pages stay allocated until Runtime teardown.

Windows A770/32.0.101.9034 passed **69 host-gate submissions**, including 16
three-queue waves where an earlier prefix copy and independent work complete
while the producer tail remains blocked. Alternating releases prove both
gates are required; a callback thread releases the remaining gate, and the
owner joins at completion. Actual OS sleep occurrence is not measured.
Checks also cover a 16-gate marker retained after external handle
release, marker replays, implicit reset, idempotence, invalid/foreign/late/
overflow gates, abandoned-gate teardown and exact full-buffer guards.
Windows CTest passed **13/13**, and updated Linux GCC build/CPU CTest **4/4**.
[Host-gate report](demo/dg2-host-gate-windows-2026-11-01.json) and
sources, logs and images（本地归档：`demo/dg2-host-gate-windows-2026-11-01.json.gz`）
record this correctness check. Full Strata callbacks still need callback-body
execution/error propagation and recording/resource-lifetime adaptation.

`Completion::ready()` and `Completion::wait(timeout_ns)` independently observe
the immutable submitted point from worker/watchdog threads. Keep a separate
handle copy on each thread. `wait()` returns **false** on timeout; invalid points,
timeouts above `INT64_MAX` and new calls on closing/destroyed runtimes throw.
These methods never update Runtime's owner progress or retire resources.
`Runtime::wait(point)` still throws on timeout and publishes owner progress;
`Runtime::ready/wait/completed` still require the submitting owner.

Each queue has a separate monotonic observer cache. A confirmed completed prefix
answers older point queries without another native query or observer lock.
Windows reads the monitored fence, then blocks through D3DKMT/Windows events
when needed. One event is created lazily per observing thread and reused across
queues; each wait registers once and rechecks its exact fence after wakeups.
Linux uses timeline query/wait ioctls. Blocking observer waits use slices of up
to 50 ms to check teardown cancellation; this is not a CPU busy-poll loop.
Runtime teardown cancels observers and waits for active native observations
before destroying their fence/device/fd. An in-progress observation can complete
or report closing. Checked CPU buffer access still needs all queues observed
complete; recording and fresh allocation may proceed independently. Finish callback
bodies before teardown.

Windows A770/32.0.101.9034 passed **50 observer submissions** across twelve
two-queue callback pipelines, plus invalid/destroyed-point checks. The check
covers earlier prefix progress before a gated tail, concurrent worker/watchdog
handles, zero/finite waits, cross-queue event reuse, a 60 ms gated wait, owner-only
progress publication/discard, immutable recycled points and a teardown race.
Every output, unchanged input and complete allocation guard is checked.
Windows CTest passed **14/14** and current-source Linux GCC build/CPU CTest
**4/4**. Linux GPU execution and actual OS sleep/wakeup overlap remain unmeasured.
This adds no full-model performance claim; the native Strata adapter remains open.
[Observer report](demo/dg2-observers-windows-2026-11-01.json) and
exact sources, raw logs, runners and images（本地归档：`demo/dg2-observers-windows-2026-11-01.json.gz`）
preserve this correctness checkpoint separately from the completion-mode timings.

The dynamic-recording revision removes the global drain before `append`, `copy`,
`fill`, `begin`, `close`, `finish`, fresh allocation/import and gate creation.
The caller remains the single recording/submission owner. An implicit submitted
recording must complete before `finish`/`reset` can reuse it. Frozen lists still
require their own observed completion before replay or discard. Queue creation,
checked public CPU access and scratch diagnostics still require all queues idle.
Memory-module binding remains an initialization operation.

Internal ISA, capture, command, scratch/private, module-data and gate storage
track their last submitted use per queue. Replay updates existing ledger entries
without an allocation or per-kernel native call. New captures rotate away from
in-flight slabs; completed pool leases remain reusable. Linux residency includes
the internal BOs actually referenced by that list, plus conservative user buffers;
unused pool storage does not acquire an unrelated gated request's busy fence.
Only owner-confirmed idle internal storage can enter `GEM_SET_DOMAIN` for a write.
User-buffer dependencies and shared module-global ordering remain the caller's job.
Linux residency is captured when closing a list: rebuild it before indirectly
using a user allocation introduced after that close. This checkpoint preceded
the public allocation release and large logical Windows buffers described below.

Windows A770/32.0.101.9034 passed **64 dynamic-recording submissions** across
sixteen waves. Each holds a producer gate closed while recording/submitting
independent work, then records a later gated producer tail and a prefix consumer.
The independent work completes before the producer gate is released; the prefix
consumer completes before the later tail gate is released. Checks include fresh
allocation/gate creation while pending, first-use ISA, changed scalar and 128-byte
pattern captures, queue-ledger growth, discarded-list reuse, exact outputs,
unchanged input and full guards. A five-second watchdog releases gates on failure.
After warmup this fixture retained **4,915,200 bytes / 4.6875 MiB** across the
remaining waves; this is backing storage, not measured physical VRAM residency.
Windows CTest passed **15/15** plus actual Strata kernel regressions. Current-source
Linux GCC build/CPU CTest passed **4/4**; Linux GPU behavior remains unvalidated.
No new model throughput, recording latency or simultaneous-kernel claim is made.
[Dynamic-recording report](demo/dg2-recording-windows-2026-11-01.json) and
exact sources, raw logs, runners and images（本地归档：`demo/dg2-recording-windows-2026-11-01.json.gz`）
retain this checkpoint, before the Strata queue adapter below.

The real `sycl/orca_queue.hpp` now also runs over `Orca::DG2` when compiled with
`STRATA_ORCA_DG2=1`. It reuses Strata's exact events, deferred operators, graph
replay, SPSC callback worker and callback error propagation. The native adapter
is in `sycl/orca_dg2_backend.hpp`; the production engine still selects L0.
Device-only kernels coalesce up to 64 launches. Host-visible copy/fill operations
submit promptly, so a CPU-polled completion word cannot remain in the recorder.
Ordinary host transfers use pooled staging; captured uploads refresh their source
bytes before replay. Graphs retain their native recording queue and its
scratch/private storage. Another stream in the same Runtime can replay them
after the recorder is destroyed; GPU dependencies join both queue transitions.
The previous replay must complete before reuse.

Prefer asynchronous submission, useful independent CPU preparation, then collect
only the results needed next. The adapter reads one queue progress snapshot per
collection and retires the whole completed prefix, including staged readbacks.
A zero/finite wait that times out still collects this prefix; it never waits for
the tail to finish or for a batch to fill. Windows uses a mapped monitored fence
and blocking event waits; Linux uses syncobj timeline query/wait ioctls. The SPSC
worker sleeps with C++ atomic wait when its ring is empty. No dedicated polling
thread is added. Unregistered host readbacks need an event/stream observation or
a subsequent submission to perform their CPU copyback; unattended polling of
such a destination is not a supported progress mechanism.

The 2026-11-01 Windows A770 check passed **16/16** CTests, including the actual
Strata queue adapter: exact cross-queue events, 128 cross-thread handoffs,
2048 eight-slot staging jobs, 64 ten-MiB uploads, graph replay and callback errors.
A gated-tail regression publishes **16 completed staged readbacks** while the
tail remains blocked. The original L0 Queue is checked separately. Linux GCC
build/CPU CTest passed **4/4**; WSL lacks both a GPU node and the oneAPI frontend,
so it does not validate the adapter or native Linux GPU execution.
[Adapter report](demo/dg2-adapter-windows-2026-11-01.json) and
exact sources, logs, runners and images（本地归档：`demo/dg2-adapter-windows-2026-11-01.json.gz`）
retain this checkpoint. Later revisions below add engine bootstrap, large
logical buffers and bounded model checks. Pool trimming, timestamps and broad
model correctness/performance remain open. Recording/submission/collection
share one adapter mutex; hardware capabilities remain a calibrated A770 subset.

The same checkpoint reran the raw completion-mode demo in three processes:
16 ordered lists x 25 kernels plus identical independent CPU preparation,
13,824 retained samples and 276,480 submissions including warmups. Compared
with adjacent polling controls (geometric mean; at most 3% control drift):

| Completion policy | Combined latency change | Calling-thread cycles change |
| --- | ---: | ---: |
| Wait for the tail | +10.36% | -76.13% |
| Collect prefix; block on first pending point | +8.16% | -60.87% |
| Prepare on CPU first; then collect prefix | **-6.89%** | **-61.83%** |

Values are medians of three qualifying pairs. GPU clocks are not locked and
mode order is fixed. Thread cycles are not CPU utilization or total driver work.
This measures scheduling choices in the raw runtime, not an adapter A/B,
GEMM fusion or full-model tok/s/TTFT. Serial per-list waiting had no qualifying
stable pair. The adapter report/archive above retain every sample and source.

The native Strata numerical frontend now builds with
`-DSTRATA_ORCA_DG2=ON -DSTRATA_ORCA_BUILD_ENGINE=OFF`. Its default eight packs
include the router and native RoPE registry needed by the prefill helpers.
Waiting on a later event now publishes all earlier staged readbacks from that
same stream. A shared sequence ledger keeps these readbacks alive for retained
completion observers; delivery does not depend on another submission or a
whole-stream wait. The existing event/timeline waiting mechanism is reused.

On Windows A770/32.0.101.9034, this checkpoint passes **15/15** CTests:
36 FP16/BF16 XMX shapes, 96 GEMM API cases, 294 FP16 expert-pair output checks,
matrix batching, first-use constant initialization during capture, and the real
Queue adapter. Its new regression checks **64 fresh host destinations using only
event waits**; the gated-tail test still delivers 16 earlier readbacks. The
original L0 numerical/queue/timestamp path passes **3/3** separately.
The earlier 48-pack build compiled but failed before this readback fix; it is
not a passing full-pack numerical checkpoint. The default pack collection
retains one unsupported SPIR-V variant; selecting it on DG2 fails explicitly.
Full ten-format IQ parity, Linux frontend/GPU execution and full-model validation
remain open. These correctness checks establish no new performance gain.
[Numerical report](demo/dg2-strata-numerical-windows-2026-11-01.json) and
exact sources, images, logs, failures and runners（本地归档：`demo/dg2-strata-numerical-windows-2026-11-01.json.gz`）
retain this checkpoint.

The subsequent graph/wait correctness check passes **15/15** native Windows
CTests, **3/3** original L0 checks and **4/4** Linux raw-backend CPU checks.
A graph recorded on a destroyed queue replays 32 times from another stream,
with refreshed uploads, ordered input/output and staged host readbacks.
Eighteen events from one producer coalesce to its latest completion point;
the gated-tail check still delivers 16 earlier readbacks immediately.
At this checkpoint collection queries remained per queue. The newer owner
batch API and Strata collector integration are documented above; eventfd
notification remains open. These changes establish no new performance gain.
[Graph/wait report](demo/dg2-strata-graph-waits-windows-2026-11-01.json) and
exact source/image delta, checks and runners（本地归档：`demo/dg2-strata-graph-waits-windows-2026-11-01.json.gz`）
extend the numerical archive above. Linux frontend/GPU and full-model validation
remain open.

The next checkpoint builds the full Strata engine over native DG2 and passes
**19/19** Windows CTests, **3/3** original L0 checks and **4/4** Linux raw CPU
checks. Ten IQ formats pass their existing dequantization/matvec tolerances for
`ncols=1..8`; dequantization is exact, while matvec is not bit-exact. Engine
bootstrap, a touched 256-MiB allocation and OS memory budgets, eight aligned
borrowed-page import/release cycles, 4,680 snapshot checks and 167 session-file
checks pass. Reusable host gates keep the same GPU address through 32 graph
reset/replays, including signaled capture and rejection of an in-flight reset.
These gates let the verifier record CPU dependencies without a polling worker.

This builds 48 kernel packs; the collection retains 24 unsupported SPIR-V files,
which native DG2 rejects if selected. Full-model outputs, PPL, tok/s and TTFT
were still unvalidated at that checkpoint. Later revisions below add large
logical Windows buffers and bounded model checks. GPU timestamps, Linux
frontend/GPU execution and pool trimming remain open.
The small 4-KiB host arena test takes the staging fallback; direct host imports
require caller-owned 64-KiB-aligned ranges.
[Engine bootstrap report](demo/dg2-strata-engine-windows-2026-11-01.json) and
exact source/image delta, checks, failed attempts and runners（本地归档：`demo/dg2-strata-engine-windows-2026-11-01.json.gz`）
extend the graph/wait archive above. No new performance gain is claimed.

The first bounded model launch exposed an additional restriction: real GR
operators use workgroups whose sizes are not powers of two. The encoder now
prepares SIMD8/16/32 local-ID payloads during recording when the fixed hardware
walk order cannot generate them; supported shapes keep hardware-generated IDs.
CPU layout checks, **64 exact GPU coordinate replays**, **19/19** native Windows
CTests and **4/4** Linux raw CPU checks pass after this fix.

At that checkpoint, the diagnostic model used 132 actual expert slots and a
complete 33.02-GiB host mirror. It loaded the model and captured T=1..6 decode
graphs, then failed commit submission with `NTSTATUS=0xc01e0200`.
[Dispatch report](demo/dg2-strata-dispatch-windows-2026-11-01.json) and
source/image delta, checks and all model attempts（本地归档：`demo/dg2-strata-dispatch-windows-2026-11-01.json.gz`）
retain this failure as well as the passing component checks.

The subsequent fix translates expert pointers into GPU virtual addresses before
serializing integer pointer tables. A 32-replay queue check covers host/interior,
existing GPU and null aliases, rejects unregistered host storage, and checks every
input value. The generated verifier also converts its legacy unsigned infinite
teardown wait into the native signed timeout range; the earlier attempt completed
two rounds but terminated during that cleanup wait.

Native Windows CTest passes **19/19**, native IQ checks pass for ten formats and
`ncols=1..8`, and original L0 compatibility passes **3/3**. The bounded model now
exits normally and produces **`271, 2`**, matching the unchanged production L0
executable with the same one-token prompt, 132-slot cache and complete host mirror.
In these single, unpaired two-token launches, reported decode time was **429.0 ms
native / 241.8 ms original L0**. This diagnostic showed no speedup; it includes the
first decode round and does not establish production throughput, TTFT or PPL.
Full-cache, long-context and native Linux GPU validation remain open. The raw
Linux sources are unchanged from the preceding CPU checkpoint; no new Linux run
is claimed. [Pointer/model report](demo/dg2-strata-pointer-windows-2026-11-01.json)
and source/image delta, checks, model logs and failed/debug attempts（本地归档：`demo/dg2-strata-pointer-windows-2026-11-01.json.gz`）
retain the exact settings and evidence. Production settings remain unchanged.

The next revision removes the 4-GiB logical Windows device-buffer limit using
contiguous GPU VA mappings backed by several resident allocations. Native
Windows CTest passes **19/19**; the separate large-buffer check passes eight
5-GiB-plus-64-KiB replays. Linux GCC build/CPU CTest passes **4/4**, without GPU access.
The actual model now runs with `--expert-cache auto`, **6478 slots / 8.68 GiB**,
prefill/context settings of 8192 and the requested 1536-MiB reserve. That
auto-cache smoke still uses a one-token prompt and two output tokens.
A separate **5771-slot / 7.74-GiB** test matches all 128 command-line output
tokens against unchanged production L0.

The persistent service comparison uses the same 28-token input, 5771 slots,
balanced parameters, ten warmups and five retained requests per process.
All **45 requests** produce the same natural 76-token output and **57/57**
accepted/offered drafts, without prefix reuse. Medians on A770/32.0.101.9034:

| Backend | Decode tok/s | Prompt ms | TTFT ms |
| --- | ---: | ---: | ---: |
| Original L0 before | 48.78 | 835.6 | 879.1 |
| Native DG2 | 16.66 | 1854.7 | 2010.8 |
| Original L0 after | 48.71 | 837.8 | 882.2 |

Native decode throughput is **65.82% lower**, with **−0.13%** control drift.
Decode uses native `DONE decode_ms`, excluding preparation, prefill and loading.
This is one fixed-prompt comparison; balanced decay/short-read settings differ
from the published 50-tok/s anchor. It establishes a substantial regression for
this workload, not its cause. Native DG2 remains experimental. Long-context
correctness, PPL, broader performance and Linux GPU execution remain open.
No continuous external measurement of the requested VRAM reserve is claimed.
The [large-buffer/model report](demo/dg2-strata-large-device-windows-2026-11-01.json)
and source/image delta, exact runners and raw logs（本地归档：`demo/dg2-strata-large-device-windows-2026-11-01.json.gz`）
retain the two-warmup screening separately from the ten-warmup comparison.

An isolated follow-up tested a thread-group dispatch policy of two workgroups
for one hardware thread, and one workgroup otherwise, against the original
grouping of eight. Ten warmups and five retained requests in each native
old/new/old process measured **15.26 / 14.92 / 15.25 tok/s**, or **−2.21%**
with **−0.06%** control drift. All 45 natural76 outputs and 57/57 draft counts
matched, and the zebin bytes were identical. This single bracket showed no
gain, so the original grouping was restored. The
[candidate report](demo/dg2-strata-thread-group-windows-2026-11-01.json) and
sources, runners and raw logs（本地归档：`demo/dg2-strata-thread-group-windows-2026-11-01.json.gz`）
retain the rejected experiment. These numbers compare two DG2 versions;
the original L0 comparison above remains separate.
Instruction-cache invalidation is also retained: Xe HPG's multi-CCS
non-pipelined-state workaround can require it, and the current backend does
not reliably discover when it can be omitted.

`dg2::Runtime::release(allocation)` now releases public mapped/device-local
allocations and imported mappings. All queues must have owner-observed
completion, and the current recording must be empty/open. Discard frozen lists
that use the allocation first. The caller's handle is cleared; copied, stale,
foreign or modified handles are rejected using a retained allocation identity.
Internal staging/ISA/parameter/batch/scratch pools cannot be released this way.
Their backing storage still stays with the Runtime for reuse.

Windows releases the GPU VA and native allocation before returning owned host
pages. Allocation destruction must be synchronous: the first check found that
asynchronous destruction left pages pinned and `VirtualFree` returned error 87.
Runtime teardown now also uses synchronous allocation destruction. Imported
pages remain owned by the caller. Linux closes the GEM handle, unmaps owned CPU
storage and removes conservative residency entries from unrelated frozen lists.
The caller must not replay a list using freed memory.

`memory_info()` queries OS availability without L0/Sysman. Windows takes the
minimum of the `D3DKMTQueryVideoMemoryInfo` process remainder, capacity minus
tracked local allocations, and global local-segment availability from
`D3DKMTQueryStatistics`. Adapter/segment identity is cached; each memory check
refreshes residency, excludes aperture/system segments and validates counters.
Both `process_budget` and `global_availability` are true on this path. Failed
queries report errors; failed allocation diagnostics preserve the original
residency error and rollback. Kernel dispatch and successful allocations do not
add statistics queries. Linux caches the region-query layout and reads current
i915 region availability; unprivileged kernel policy can hide other processes'
usage. Both paths cap availability by capacity minus this Runtime's local
allocations. The adapter exposes `deallocate()` and
`memory_info()`; its device capacity comes from this query. Transfer bounds use
the requested logical size, excluding physical allocation padding. Host-event
success is published after the native signal succeeds.

The [2026-11-06 Windows A770 global-counter check](../bench/results/a770/orca-direct-global-memory-2026-11-06.md)
calibrates against independent Sysman readings and a foreign 256-MiB allocation.
Old/new runtime controls, 17 residency rejection/retry cycles, three global-query
faults and seven complete model protocols pass at 5771 slots and 1536-MiB reserve.
This is an accounting correction, without a new speed or Linux GPU claim; the
earlier intermittent residency and 8K failures remain open.

The 2026-11-01 Windows A770 check passed **17/17** CTests and all Strata kernel
regressions. The new **76-submission** check covers 64 host/local-VRAM
roundtrip/release cycles, three unrelated frozen-list replays, a gated release
rejection and eight borrowed-page reimports. Exact outputs/guards and stale,
foreign, modified, internal and double-release rejection pass. Retained backing
storage after the repeated cycles stays at **2,162,688 bytes / 2.0625 MiB**;
this is not physical VRAM residency. The actual Strata adapter separately checks
one-byte logical bounds and a physical backing-byte decrease after free.
Linux GCC build/CPU CTest passed **4/4**. Native Linux GPU behavior and full-model
integration remain unvalidated; no new performance gain is claimed.
[Allocation-release report](demo/dg2-allocation-release-windows-2026-11-01.json) and
exact sources, logs, runners and images（本地归档：`demo/dg2-allocation-release-windows-2026-11-01.json.gz`）
retain this checkpoint.

The Windows A770 check submits 40 frozen lists without intervening host waits,
repeats twelve waves with changing inputs, and validates every intermediate
snapshot and full prefix/tail padding in shared local VRAM. Alias replay and
implicit-list transitions bring this to **482 submissions**. It also probes
zero-timeout ownership and rejects invalid/foreign/stale handles. Hardware
backlog depth and whether the zero-timeout probe actually times out are not measured.
The compiler check runs another **96 submissions** across eight frozen regions,
covering ordinary/ESIMD 3D launches, large/normal GRF transitions and local memory.
Windows CTest passed **10/10**; Linux shared-source build/CPU CTest passed **4/4**.
Transfer, GEMV/XMX, scratch/private/atomic and 17,000-capture regressions passed.
[Multilist report](demo/dg2-multilist-windows-2026-11-01.json) and
exact sources, logs and images（本地归档：`demo/dg2-multilist-windows-2026-11-01.json.gz`）
establish correctness, with no new model-throughput or wait-mode speed claim.

`discard(commands)` now returns a completed or never-submitted frozen list to
the internal pools and invalidates **all copied handles**, including after its
slot is reused. Observe its completion with `wait`, `ready` or `completed`
before discarding; an in-flight list is rejected. A completed prefix can be
discarded while later sequences remain pending. Checked CPU mappings and new
recording still require every queue to be observed complete.

Command buffers use a best-fit pool. Argument slabs have shared internal leases
held by every list that uses them; the kernel's last-argument cache uses a weak
lease. Releasing one list therefore preserves captures used by another frozen
list, while a recycled slab cannot satisfy an expired cached capture. Implicit
`reset()` also releases parameter leases. Linux residency lists retain their
handles and place the actual executing batch last, including recycled batches.
**Backing allocations stay owned by the Runtime for reuse**; discard does not
return GPU memory to the OS. Shared ISA, module storage and scratch/private
pools retain Runtime lifetime. A general allocation free/trim API remains open.

The Windows A770/32.0.101.9034 `--gpu-reclaim inference.zebin memory.zebin` check
passed **900 submissions**: four waves of 96 frozen lists / 18,432 changing
capture instances, a surviving older frozen capture, completed-prefix discard,
then 512 wide-fill/scalar re-recordings with implicit resets. It checks stale,
foreign, invalid and recycled handles, unchanged scalar recapture after weak
lease expiry, never-submitted discard, exact GPU results and every padding byte.
After the second wave, `allocated_bytes()` remained **10,682,368 bytes
(10.1875 MiB)** through the later waves and all 512 re-recordings. This metric
counts retained GPU allocations/imports, including system-memory heaps and idle
pools; it is not physical VRAM residency, RSS or an OS budget query. Larger live
sets or new shapes can grow the pools. Windows CTest passed **11/11**, Linux
shared-source build/CPU CTest **4/4**; Linux GPU behavior remains unvalidated.
[Reclamation report](demo/dg2-reclamation-windows-2026-11-01.json) and
exact sources, checks and images（本地归档：`demo/dg2-reclamation-windows-2026-11-01.json.gz`）
record this lifecycle change without a new model-throughput claim.

GPU pointers are `Allocation::gpu`; CPU mappings are `Allocation::data`.
They are distinct addresses. `allocate` provides system-memory heaps and
rounds each Windows allocation to 64 KiB. The prototype's 256 MiB
per-allocation cap has been removed; an allocation larger than 320 MiB has
passed GPU checks. Larger requests still depend on driver and residency limits.
`import_host(data, bytes)` borrows committed write-back read/write host pages
without copying their contents. Both address and size must be 64 KiB aligned;
overlapping imports are rejected. Use the returned GPU VA, retain the original
mapping until all GPU users and the runtime end, and never free or remap it
while in flight. `Allocation::imported` identifies borrowed storage; the runtime
releases its GPU handle but does not free these host pages. Importing is not
device-local placement. Linux uses `GEM_USERPTR`; its GPU behavior remains
unvalidated. Linux-owned system-memory BOs are rounded to 4 KiB.
`allocate_device(bytes)` explicitly allocates local VRAM, rounds to 64 KiB,
and returns `data == nullptr`, `device_local == true`. CPU access is rejected;
upload/download through GPU kernels and use `gpu` for kernel arguments.
The calibrated Windows descriptor requests linear, uncompressed, local-only,
non-lockable storage, and every allocation must report GPU-memory residency.
Each physical part is limited to `0xffff0000` bytes (below 4 GiB). Larger
logical buffers reserve one contiguous GPU VA range and map multiple resident
parts into it. A 5-GiB-plus-64-KiB buffer passed eight replays across both ends,
the physical-part boundary and 4 GiB, including guards, release accounting and
stale-handle rejection. See the [large-buffer/model report](demo/dg2-strata-large-device-windows-2026-11-01.json).
Linux uses device-only
`GEM_CREATE_EXT_MEMORY_REGIONS` with a 2 MiB aligned/padded GPU VA span to avoid
mixing DG2 4K/64K PTEs in one PDE; this placement remains GPU-unvalidated.
All allocations and captured argument storage live until runtime destruction;
frequent re-recording with different captures can grow memory use. A timeout
keeps the list in flight; finish it before reset, CPU access or another submit.

Native `copy(destination_gpu_va, source_gpu_va, bytes)` records H2D, D2D, D2H
or H2H work without a Level Zero handle. It checks both entire ranges, accepts
arbitrary byte counts and offsets, and rejects partial overlap; exact self-copy
and zero-length operations are no-ops. `fill(destination_gpu_va, pattern,
pattern_bytes, bytes)` repeats a captured power-of-two pattern of 1..128 bytes,
including a partial final pattern. Changing the original pattern after recording
does not change a replay. Common 1..16-byte patterns use kernel arguments;
larger patterns share the immutable capture heap. Both use aligned 16-byte
vector bodies and byte access for unaligned addresses and tails. The measured
native memory kernels use SIMD16/128 GRFs, with no scratch/private memory.
The 256-group cap is a bounded launch policy, not a measured bandwidth optimum.

Bind a driver-baked [memory.cl](src/drm/memory.cl) once with
`runtime.set_memory_module("memory-native/image_0.zebin")`. Baking needs the
vendor compiler; the execution process does not. For a fresh offline-build
directory, using matched oneAPI `clang`/`llvm-spirv` and the SDK-enabled
`orca-l0-native-pack` tool:

```sh
mkdir -p memory-source
clang -cc1 -triple spir64-unknown-unknown -x cl -cl-std=CL2.0 \
  -finclude-default-header -O2 -emit-llvm-bc orca-l0/src/drm/memory.cl -o memory.bc
llvm-spirv -spirv-max-version=1.2 -spirv-ext=-all memory.bc -o memory-source/memory.spv
printf 'memory.spv\torca_copy\nmemory.spv\torca_fill\nmemory.spv\torca_fill_wide\nmemory.spv\torca_copy_2d\n' > memory-source/index.txt
orca-l0-native-pack memory-source memory-native
```

`copy_2d(destination, destination_pitch, source, source_pitch, width, height)`
validates the complete disjoint pitched spans and records one memory kernel,
including unaligned rows and byte tails. Re-bake all four exports to enable it.
Existing three-export modules retain row-copy fallback; capability is cached
once at module binding rather than queried for each transfer.

Bake Linux zebin on Linux. The optional CMake setting
`ORCA_L0_DG2_MEMORY_ZEBIN` registers the transfer GPU check; omit it for a
CPU-only build or when the memory module is unavailable. Build-time commands
and the exact Windows image are also preserved in the transfer report archive.

On Windows/A770, the reusable backend passed the existing pipeline/400-kernel
checks and actual Strata native kernels: 24 GDN-gate/copy pipeline replays
(703 elements, maximum absolute error 9.54e-7), and 48 BF16 GEMV replays
(19×1027, local64/local256 with dynamic SLM, maximum absolute error 0 on these
exactly representable inputs). Input data and output guards are checked.
The execution process loaded no Level Zero, Intel UMD, GMM or GPU compiler.
Metadata inspection now accepts **1,243/1,243** native exports; this is not GPU
validation of all 1,243. The earlier reader misclassified ten ESIMD images:
their product family is unspecified, while their core correctly identifies
Xe HPG. The compiler-binding revision below accepts this form and supplies
their SIMD1 packed local IDs; specified different product/core families are
still rejected.
Global-atomic metadata and EU-fusion requirements are now supported. Earlier
censuses accepted 640 before scratch/private support, then 1,210 before these
execution flags were implemented.
[Earlier reusable-backend evidence](demo/dg2-runtime-windows-2026-10-31.json)
records the original 640-export census. The latest scratch/private checks and
shared-heap comparison are described below.

The following reusable `Runtime` measurement predates scratch/private support
and shared recording heaps. It used that same harness on Windows
A770/driver 32.0.101.9034. The initial three A0/B/A1 rounds were followed by
three additional polling-chain rounds because the initial chain had only two
stable pairs. All **63 processes / 32,256 retained samples** passed their
CPU-reference and guard checks; all attempts are retained.

| Workload / wait | Existing Orca L0 | Reusable DG2 | Paired completion change | Stable pairs |
| --- | ---: | ---: | ---: | ---: |
| One kernel / native | 73.0 µs | 101.7 µs | +39.3% | 0/3 |
| Four-kernel pipeline / native | 100.8 µs | 159.4 µs | +47.3% | 1/3 |
| 400 dependent kernels / native | 496.4 µs | 459.5 µs | −6.0% | 1/3 |
| One kernel / poll | 71.1 µs | 69.8 µs | −1.6% | 2/3 |
| Four-kernel pipeline / poll | 92.9 µs | 106.3 µs | +11.5% | 0/3 |
| **400 dependent kernels / poll** | **496.2 µs** | **382.3 µs** | **−23.4%** | **5/6** |

Only the last row meets the three-stable-pair rule. Its five stable pairs
have a median reduction of **23.6%**, with reductions of 21.3–27.2%; the
maximum control drift across all six attempts is 4.43%. Percentages use
paired ratios; the displayed times are separately aggregated medians.
CPU submission for that row measured 14.7 → 9.2 µs, but only two of six
submission pairs were stable, so no stable CPU submission gain is claimed.
Short native-wait regressions remain an unresolved observation. This compares
against existing Orca L0/direct DDI with Intel UMD, using system-memory data;
it does not isolate API overhead or establish a full-model throughput gain.
These workloads exercise no scratch/private memory.
[Reusable-backend performance](demo/dg2-performance-windows-2026-10-31.json)
includes all samples, logs and the measured source snapshot（本地归档：`demo/dg2-performance-windows-2026-10-31.json.gz`）.

To run this variant, add `-DORCA_L0_BUILD_WDDM_BACKEND=ON` to the matched
benchmark configuration above, build target `orca-l0-dg2-bench`, and use it
as `--candidate` with `--candidate-backend dg2`. The archived source snapshot
identifies the exact harness used for these numbers. Full Strata execution
through this backend and its token-throughput comparison remain open.

An additional instrumented wait-policy run passed **108 processes / 55,296
retained samples**, using the same A770, driver, image, inputs and guard checks.
`QueryThreadCycleTime` records the calling thread's CPU execution cost around
each timed window. It counts cycles, not CPU microseconds or whole-system use;
driver/background threads are excluded. CPU counters are outside wall-clock
timing, and their interval includes timer/counter overhead.

| DG2 wait policy | One kernel | Four-kernel pipeline | 400-kernel chain | Chain CPU cycles, poll = 100% |
| --- | ---: | ---: | ---: | ---: |
| Existing wait: 128 spins, then event | 93.8 µs | 121.2 µs | 457.3 µs | 11.1% |
| 50 µs spin budget, then existing wait | 64.9 µs | 127.8 µs | 459.9 µs | 21.0% |
| 100 µs spin budget, then existing wait | 66.3 µs | 93.0 µs | 468.8 µs | 33.1% |
| Continuous bounded polling | 65.4 µs | 92.6 µs | 399.3 µs | 100% |

These are policy observations from separate processes; small differences are
not a directly bracketed policy comparison. Fixed 50/100 µs spinning spends
extra CPU on the long chain without improving its event-wait latency. A
100 µs budget approaches polling latency on the short pipeline, with similar
CPU cost there. Prefer event waiting for longer work and a bounded spin budget
for latency-sensitive short work; choose budgets from cached per-list timings,
rather than treating 100 µs as a universal optimum. At that measurement stage,
the runtime had one completion fence for all 400 kernels. The multilist revision
now exposes batch completion progress; the complete Strata scheduler remains
engine work. Collect ready notifications together and submit ready tail work
immediately.

Against existing Orca L0/direct DDI in this instrumented run, the event-wait
chain reduced completion latency **8.1%** and calling-thread cycles **91.1%**;
both have three stable pairs. Its CPU-cycle control drift is at most 1.97%.
The old L0 native-wait control consumed about 1.10 million calling-thread
cycles per chain versus 0.097 million for DG2 event waiting. Polling still
reduced chain latency **20.9%** with three stable pairs, while the four-kernel
polling pipeline regressed **12.9%**, also with three stable pairs. Mixed-kernel
state programming remains an optimization target. This is a separate harness
version with CPU counters; its samples are not pooled with the earlier run.
[Wait-policy results](demo/dg2-wait-modes-windows-2026-10-31.json)
and complete evidence/source snapshot（本地归档：`demo/dg2-wait-modes-windows-2026-10-31.json.gz`）
record all cases. Strata's production wait policy remains unchanged.

With the DG2/control benchmark targets built, select modes explicitly:

```powershell
python tools/bench_orca_wddm.py `
  --baseline build-orca-wddm-perf/orca-l0-wddm-baseline-check.exe `
  --candidate build-orca-wddm-perf/orca-l0-dg2-bench.exe --candidate-backend dg2 `
  --image C:/path/to/image_0.zebin --out build-orca-wait-measurement --rounds 3 `
  --driver C:/path/to/installed/ze_intel_gpu64.dll `
  --wait-mode native --wait-mode hybrid50 --wait-mode hybrid100 --wait-mode poll
```

`--workload chain400` restricts a repeat to the chain. The 50/100 µs budgets
are checked every 32 polls; scheduling can extend their wall time. The
`native_wait_calls` diagnostic counts fallback invocation, not confirmed OS
blocking or context switches. Full-model throughput and native Linux GPU
validation remain open.

The scratch/private revision added direct GPU checks:
12 actual Cvec replays use 2,048 scratch bytes per hardware thread (maximum
absolute error 1.19e-7); 16 actual Q3_K-to-BF16 replays use 512 private bytes per
hardware thread and match an analytic BF16 reference exactly. Both vary their
inputs and check the full padded data regions and 64 KiB guards on both sides
of each thread-memory pool. Two scratch slots are metadata-supported; slot 0
has GPU validation. These checks do not establish parity for every accepted
export.

ISA and immutable parameter captures now share 1 MiB recording slabs with
64-byte alignment, reducing global-state changes between different kernels.
The GPU rollover check verifies 17,000 distinct scalar captures across a slab
boundary. In that measured revision captures remained owned until destruction.
The newer `discard`/`reset` pooling described above reuses retired parameter
slabs while preserving captures referenced by other frozen lists.

Against the **previous reusable DG2 runtime**, 27 processes / 13,824 retained
samples passed. The four-kernel polling pipeline's median paired latency fell
**6.1%** across six attempts. Three met the 3% control-drift rule; those reductions
were 6.0–6.5%. The 400-kernel chain changed less than 1%, establishing no material
additional gain. The latest pipeline comparison against existing Orca L0 had
zero stable pairs; it does not establish that the earlier L0 gap is eliminated.
This is neither an original-SYCL comparison nor a full-model throughput result.
[Latest report](demo/dg2-scratch-arena-windows-2026-10-31.json) and
complete checks, samples and exact sources（本地归档：`demo/dg2-scratch-arena-windows-2026-10-31.json.gz`）
preserve all attempts. To bracket two DG2 revisions with the existing harness,
select `--baseline-backend dg2 --candidate-backend dg2`.

For normal execution, submit a batch asynchronously, prepare independent work
on the CPU, and join at the point that needs its GPU result:

```cpp
runtime.submit();                     // All recorded kernels share a tail fence.
prepare_next_request_in_host_memory(); // Separate storage; no live GPU buffer access.
if (!runtime.ready()) runtime.wait(); // One explicit query, then bounded blocking.
runtime.cpu_access(output, false);    // The result can now be read.
```

On Windows `ready()` reads the mapped monitored fence. `wait()` makes a short
128-probe spin and then uses a Windows event; a caller does not need a continuous
poll loop. On the current Linux transport `ready()` queries the timeline once;
calling `wait()` directly at a mandatory join avoids that extra query. Native
Linux GPU execution remains unvalidated.

One ordered list needs only its tail fence to cover all kernels and their
recorded dependencies. This reduces CPU completion queries, while retaining GPU
dependency barriers. Cross-queue dependencies and CPU reads of intermediate
results need their own synchronization points. The DG2 API now supports multiple
frozen lists, independent native queues and per-queue batch progress through `completed(queue)`.
The complete engine request scheduler remains open. Combining compatible
requests into larger GEMMs remains an engine scheduling feature. Submit ready
tail work immediately rather than waiting for another request to fill a batch.

For normal inference, use this semisynchronous pattern with one completion
point per dependent batch. Independent host preparation can overlap GPU work;
join only when the result is consumed. A required join can call `wait()`
directly. Completion batching collects finished requests; it does not require
waiting for a batch to fill or merging incompatible operators. Multiqueue
execution uses a completion sequence per queue, not one global sequence across
independent queues. Continuous polling remains a measured latency/CPU tradeoff,
not the default requirement.

| Completion strategy | Appropriate use | Current DG2 support |
| --- | --- | --- |
| Submit, prepare independent CPU data, join at consumption | Normal inference with useful host work | Available; never modify buffers still used by the GPU. |
| Short spin followed by an event | Short Windows work with no host preparation left | Existing `wait()`; longer work blocks. |
| Blocking wait | Mandatory Linux join | Existing `wait()`; avoids repeated `ready()` ioctls. |
| Collect completions through a queue sequence | Multiple submitted lists | `completed()`; one query retires earlier submissions on that same ordered queue. |
| Worker observes an immutable point | Callback release or watchdog | `Completion::ready/wait`; caches a completed prefix without publishing owner retirement. |

Batch completion collection does not itself combine operators into a GEMM.
Linux now uses syncobj timeline waits per queue. Eventfd notification
and a collector spanning independent queues remain unimplemented and unmeasured.
Independent queues require independent completion progress. Do not wait for a
completion batch to fill before releasing finished work.

`--gpu-semisync image_0.zebin` demonstrates the pattern with 400 dependent GPU
kernels and independent CPU dequantization of 262,144 host values. It compares
serial/overlap/serial with 16 warmups and 64 retained samples per phase, checks
every CPU/GPU result and the full output allocation, and performs one explicit
completion query per replay. Setup, recording, initialization and correctness
checks are excluded; **CPU preparation is included** because the metric is
combined work, not GPU decode latency. It adds no new runtime API or dependency.

On Windows A770/driver 32.0.101.9034, three serial/overlap/serial rounds passed
720 replays / 576 retained samples. Combined-work latency fell **10.0%** by the
median paired ratio (individual rounds 9.5%, 10.0%, 13.4%); all three pairs met
the 3% drift rule, with maximum absolute drift 1.8%. Windows CTest passed 4/4,
and the updated Linux CPU build/CTest passed 4/4. This independent host work
does not depend on the current GPU result; the result does not establish that
the next autoregressive token can always be prepared in advance.
[Semisynchronous results](demo/dg2-semisync-windows-2026-10-31.json) and
all samples, checks and source snapshots（本地归档：`demo/dg2-semisync-windows-2026-10-31.json.gz`）
make the metric and ordering explicit. Production settings remain unchanged.

The newer `--gpu-completion-modes image_0.zebin` check uses **16 frozen lists
of 25 dependent kernels** and the same independent 262,144-value CPU work in
every mode. Each alternative is bracketed by polling controls. Three fresh
processes use 128 warmups and 512 retained samples per phase: **13,824 retained
samples / 276,480 submissions**, with exact CPU/GPU results and full padding
checks. Windows A770/32.0.101.9034 results relative to adjacent polling controls:

| Completion mode | Combined-work latency | Calling-thread cycles | Stable pairs |
| --- | ---: | ---: | ---: |
| Submit all, wait for tail, prepare CPU data | +10.3% | −77.8% | 3/3 |
| Submit all, collect completed prefixes, prepare CPU data | +6.7% | −67.6% | 3/3 |
| Submit all, prepare CPU data, collect completed prefixes | **−5.6%** | **−68.2%** | **3/3** |

These are paired medians with a 3% control-drift limit; cycle comparisons also
require cycle drift <=3%. Serial submit/wait per list has no stable pair, so
no percentage is established for that arm. CPU cycles describe calling-thread
execution cost, not CPU percentage, power or background driver work. The total
includes CPU preparation; it is not pure GPU time or model decode throughput.

The prefix collector reads `completed()` once, consumes all submitted tickets
at or below that sequence, then waits for the first unfinished ticket if needed.
It never waits for new requests to fill a batch. This fixture still needs a
median 15–16 explicit queries and 14–15 wait calls per wave; one query can
describe a complete prefix, but cannot predict work that has not finished.
Windows queries read mapped memory. Wait-call counts include the existing short
spin and do not establish how often the thread actually slept. A tail-only join
needs one wait call when intermediate CPU results are unnecessary.

Prefer useful independent host work before a required join; select a tail join
or prefix collection according to actual result consumers. Completion collection
does not free frozen resources or relax the current all-queues-idle CPU-access rule.
The APIs and runtime defaults are unchanged. Windows CTest passed **10/10** and
the updated Linux build/CPU CTest passed **4/4**; native Linux GPU validation and
the complete Strata adapter remain open. Run the check through
`tools.bench_deepinfer.gpu_lock()`, as with other GPU measurements.
[Completion-mode report](demo/dg2-completion-modes-windows-2026-11-01.json) and
all samples, checks, runnable measurement script and exact sources（本地归档：`demo/dg2-completion-modes-windows-2026-11-01.json.gz`）
retain both the initial short exploratory run and the final run separately.

The large-allocation/import check passed eight Windows A770/32.0.101.9034 GPU
replays with a >320 MiB allocation and a borrowed 128 KiB mapping. It checks
four separated GPU VA subranges on both sides of 256 MiB, every remaining
allocation byte as a guard, changed inputs, invalid/overlapping/read-only
imports, in-flight CPU-access rejection and a caller mapping that survives
runtime teardown. Windows CTest passed 5/5; XMX, GEMV, scratch/private and
atomic regressions passed. Linux CPU CTest passed 4/4. These are correctness
checks; no device-local bandwidth or full-model throughput gain is claimed.
[Memory report](demo/dg2-host-memory-windows-2026-10-31.json) and
logs and exact sources（本地归档：`demo/dg2-host-memory-windows-2026-10-31.json.gz`）
preserve the current revision separately from earlier performance results.

The earlier `--strata-device PACK` check used explicit local VRAM on
Windows A770/32.0.101.9034. It passed **12 complete H2D → D2D → D2H replays**:
four each at 65,536, 16,842,752 and 335,609,856 bytes, with changed inputs and
exact full-buffer comparisons. Actual local-memory BF16 GEMV passed **48
replays** and FP16 M4/BF16 M8 XMX GEMM passed **144 replays**, with exact CPU
agreement on the dyadic fixtures, padded strides/tails, input/output guards
and mixed system/local allocations. Windows CTest passed **5/5**, and Linux
build/CPU CTest passed **4/4**; system-memory XMX/GEMV/scratch/private/atomic
regressions also passed. Execution loads no Level Zero, Intel UMD, GMM or
compiler. Calibration uses the installed driver separately and is specific
to this device/driver. These checks establish neither bandwidth nor a
full-model speedup. That revision used a float4 transfer fixture, superseded
by the public byte-copy API in the following checks. Full Strata integration
remains unfinished.
[Device-memory report](demo/dg2-device-memory-windows-2026-10-31.json) and
logs, calibration details and exact sources（本地归档：`demo/dg2-device-memory-windows-2026-10-31.json.gz`）
retain this revision independently of earlier system-memory performance runs.

The latest native transfer revision passed **276 replays / 138 recordings**
on Windows A770/32.0.101.9034: H2D/D2D/D2H/H2H, 0..2,097,189 bytes, six aligned/
unaligned offset pairs, captured 1..128-byte fill patterns, exact full-buffer
and guard comparisons, changed inputs and invalid range/overlap rejection.
Actual local GEMV48/XMX144 and 12 local roundtrips through 335,609,856 bytes
now use this public copy API; the old float4 helper is removed. System-memory
regressions and 17,000 immutable scalar captures across a 1 MiB heap rollover
also passed. Windows CTest passed **6/6** and the actual Linux build/CPU CTest
passed **4/4**. Linux GPU validation is still pending. Successful error checks
now take `string_view`, avoiding temporary error-string allocations; the checks
themselves remain enabled. This is correctness evidence, without a new bandwidth
or full-model throughput measurement.
[Transfer report](demo/dg2-transfers-windows-2026-10-31.json) and
all logs, failed attempts, source snapshot and baked memory image（本地归档：`demo/dg2-transfers-windows-2026-10-31.json.gz`）
identify the exact revision. Multiqueue/events, general allocation release and
the full Strata adapter remain open; frozen-list pooling is described above.

The native execution ABI now also covers contended global atomics, EU-fusion
disable requirements and three-dimensional dispatch. On the same Windows
A770/32.0.101.9034, actual Strata FP32/FP16/BF16 finite-count reductions and a
dependent doorbell passed 24 replays over 8,195 elements, including all-finite,
all-bad and mixed signed Inf/NaN inputs. Repeated atomic additions and every
input/output padding byte are checked exactly. The atomic instructions/fences
are already in the ISA; the metadata flag requires no extra DG2 hardware state.

Actual Strata **FP16 M4 / BF16 M8 XMX GEMMs** passed 144 replays with exact CPU
agreement on the dyadic fixtures. Coverage includes 128/256 GRFs, one/eight
subgroups, T/N/K = 8/64/32, 17/72/48 and 5/8/16, padded input strides, partial
row/column tiles and three-dimensional group Y/Z. Non-DPAS doorbells bracket
each GEMM to check live systolic, EU-fusion and GRF state transitions in one
list. The EU-fusion bit is programmed with Xe HPG's inverted meaning and
participates in the state-cache key. This is kernel parity, not a full Strata
prefill performance result or general numerical equivalence for every input.

`tests/dispatch3d.cl` independently checks local/group/global coordinates and
hidden sizes. Its native image passed 32 replays with nontrivial local and group
X/Y/Z dimensions, partial/full SIMD, prefix guards and full output padding.
Both public dimension overloads share the same recorder and submit path; no
per-kernel work was added to submission. Source, build/bake commands, image
hashes, checks and remaining limits are in the
[atomic/XMX/3D report](demo/dg2-atomics-xmx-windows-2026-10-31.json) and
complete evidence/source archive（本地归档：`demo/dg2-atomics-xmx-windows-2026-10-31.json.gz`）.
The latest Windows CTest and shared Linux CPU CTest each passed 4/4; native
Linux GPU execution and concurrent CPU/GPU doorbell handoff remain unvalidated.
Device-local large allocations, concurrent queues and full-model integration
remain unfinished. Production binaries/settings are unchanged.

An opt-in Linux DG2/i915 backend, `Orca::DG2` (`orca::l0::dg2`), submits
Orca-encoded compute commands directly through DRM ioctls. The older
`orca_l0/drm.hpp` names remain aliases. It has no Level
Zero, Intel user-mode driver or `libdrm` linkage. Its first implementation
supports system-memory BOs, a bounded ze_info 1.73 ABI, stateless pointer/value
arguments, hardware local IDs, inline/indirect payloads, 128/256 GRFs, SLM and
one barrier per kernel. An ordered list records multiple kernels and replays
with one submission and preallocated residency entries. Unsupported ABI
features are rejected; this is **not yet a full Strata backend**.

```sh
cmake -S orca-l0 -B build-orca-dg2 -DCMAKE_BUILD_TYPE=Release \
  -DORCA_L0_BUILD_RUNTIME=OFF -DORCA_L0_BUILD_DRM_CHECKS=ON
cmake --build build-orca-dg2
ctest --test-dir build-orca-dg2 --output-on-failure
# On native DG2/i915, using a zebin baked from tests/inference.cl on that Linux driver:
./build-orca-dg2/orca-l0-drm-compute-check --inspect /path/to/image_0.zebin
./build-orca-dg2/orca-l0-drm-compute-check /dev/dri/renderD128 /path/to/image_0.zebin
```

The compute check records RMSNorm → GEMV → SwiGLU → vector increment and
checks 72 replays against a CPU reference with changing input parameters.
Only compilation, parser rejection and command-encoding CPU checks have run
in Linux/WSL; native Linux GPU execution and performance remain unvalidated.
The selected Intel-generated command definitions retain their MIT license
and pinned source revision in `third_party/intel-dg2/`.

From the Strata checkout, the [Linux check runner](../tools/check_orca_native_linux.py)
builds the raw backend, runs three explicit CPU checks and audits dependencies
without opening a GPU context:

```sh
python3 tools/check_orca_native_linux.py --out build-native-linux-cpu-NEW
# With the Linux oneAPI environment initialized, also compile the full engine:
python3 tools/check_orca_native_linux.py --frontend \
  --out build-native-linux-frontend-NEW --ggml-dir /path/to/pinned/llama.cpp
```

The exported source now compiles the **full Linux frontend** with oneAPI
2025.3.2, generates 48 kernel packs and passes **3/3 CPU checks** plus four ELF
dependency audits. The binaries link none of SYCL, UR, Level Zero, Intel GPU
UMD/compiler or libdrm, and have no missing libraries in the checked environment.
The public runner also passes a Make-free GCC build and the same three CPU
checks, selecting installed Ninja when no generator is specified. The private
userland needed procfs for dependency inspection; checks use a temporary private
mount namespace that leaves no procfs mount outside it. Neither Strata nor GPU
CTests ran. Use fresh output directories and bake fresh native images with the
Linux driver before later GPU validation.
[Full frontend/CPU report](demo/dg2-linux-frontend-cpu-2026-11-02.json) and
commands, runner and all attempts（本地归档：`demo/dg2-linux-frontend-cpu-2026-11-02.json.gz`）.
The earlier raw-source handoff and missing-compiler preflight remain archived:
[CPU report](demo/dg2-linux-handoff-cpu-2026-11-02.json) and
exact runner, manifest and logs（本地归档：`demo/dg2-linux-handoff-cpu-2026-11-02.json.gz`）.

## Build and check

Linux, with Level Zero development files, an Intel GPU driver and `ocloc`:

```sh
cmake -S orca-l0 -B build-orca-l0 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-orca-l0
ctest --test-dir build-orca-l0 --output-on-failure
```

Windows, from an x64 Visual Studio developer shell:

```powershell
cmake -S orca-l0 -B build-orca-l0 -G Ninja -DCMAKE_BUILD_TYPE=Release `
  '-DORCA_L0_SDK=C:/Program Files/LevelZeroSDK/1.33.1' `
  '-DORCA_L0_OCLOC=C:/Program Files (x86)/Intel/oneAPI/ocloc/2025.3/bin/ocloc.exe'
cmake --build build-orca-l0
ctest --test-dir build-orca-l0 --output-on-failure
```

`ORCA_L0_DEVICE` defaults to `dg2` for the A770 check. Set it to an `ocloc`
target supported by your installation for another GPU. The checks load both
the generated SPIR-V and a device-specific native binary through
`zeModuleCreate` (`ZE_MODULE_FORMAT_NATIVE` for the latter).
Use `ORCA_L0_BUILD_CHECKS=OFF` to build only the host library, without `ocloc`.
Use `ORCA_L0_BUILD_GPU_CHECKS=OFF` to compile/link the check harness and run
the CPU ring check on a machine without a GPU or `ocloc`.
The library can be consumed with `add_subdirectory(orca-l0)` and
`target_link_libraries(your_target PRIVATE Orca::L0)`.

The ring also has a standalone CPU check, with no Intel tools, loader or GPU:

```sh
cmake -S orca-l0 -B build-orca-ring -G Ninja -DORCA_L0_BUILD_RUNTIME=OFF
cmake --build build-orca-ring
ctest --test-dir build-orca-ring --output-on-failure
```

The Linux GitHub workflow builds and links the check harness against Ubuntu
24.04's Level Zero development files and runs the CPU ring check. It does not claim
GPU coverage on a hosted runner. When extracting this directory into a
repository, also copy `.github/workflows/orca-l0.yml` and adjust its source
paths. Native Linux GPU parity and full-model performance need an Intel GPU
runner; Windows measurements cannot substitute for them.

Coordinate GPU checks and builds with any existing benchmarks. In the Strata
workspace, use the existing `tools.bench_deepinfer.gpu_lock` lease.

`replay(immediate_or_queue, count, closed_lists)` submits a ready array of native
command lists in one driver call. It allocates no memory and performs no host
wait. The original single-list overloads remain available. Keep the lists and
their resources alive until native completion, and express dependencies between
lists explicitly. Independent lists with disjoint outputs need no added
dependency. This entry point does not perform model-request aggregation.

`Runtime::properties()`, `driver_properties()` and `extensions()` return
immutable snapshots owned by that Runtime. Driver properties and extension
enumeration run at construction, including the capability list used for large
allocations. Create a new Runtime to rediscover a device/driver. Kernel ABI and
module lookups remain cached by `compiler::Program`; completion queries still
read current driver state.

The optional `orca-l0-submit-check` target compares queue/fence/immediate/event
completion, DDI function-table calls, and batched versus individual submissions:

```sh
cmake --build build-orca-l0 --target orca-l0-submit-check
./build-orca-l0/orca-l0-submit-check build-orca-l0/inference.spv
./build-orca-l0/orca-l0-submit-check build-orca-l0/inference.bin native
```

The supported API passed three fresh Windows A770 processes on 2026-10-31:
2916 exact GPU-tail windows and 1728 complete GPU batches, including invalid
input rejection, distinct inputs and output guards. Two processes used SPIR-V;
one loaded the native binary directly. For 32 independent one-kernel lists,
individual appends with one final wait took 267.6--288.9 us of host submission
time; one batch append took 30.5--30.6 us. Complete execution/readback windows
fell from 1633.8--1689.3 us to 383.35--384.9 us. The paired window speedups were
4.33x/4.42x; the native-binary process measured 5.00x. With 16 kernels per list,
the three process speedups were 1.83x--2.15x. These are standalone ready-list
batches, not full-model throughput. The 400-kernel single-list DDI comparison
changed complete windows by less than 0.5%; small-list samples varied more.
Every sample and the prior raw-C-API prototype are retained in the
[API submission report](demo/api-submission-a770-windows-2026-10-31.json).
Linux GPU results and a hardware concurrency timeline remain pending.

`ORCA_L0_POISON_ALLOCATIONS=1` fills each new device, host and shared allocation
with `0xff` bytes before returning it. This is a NaN pattern for the common
floating-point types and exposes reads before initialization. Device fills use
a separate immediate list and wait once per allocation; it adds no work to
later kernel submissions. The diagnostic is off by default and invalidates
timing measurements. `orca-l0-poison` checks every byte, including an odd-sized
tail, for all three memory kinds and reruns the inference check with it enabled.

## Reusing compiled GPU kernels

The optional `compiler/` project uses the oneAPI compiler and SYCL headers
to compile existing device code. Its host executable submits directly through
Orca and links without SYCL, UR or oneMKL. It requires C++20; the base runtime
still requires only C++17.

```sh
cmake -S orca-l0/compiler -B build-orca-compiler -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=icpx
cmake --build build-orca-compiler
ctest --test-dir build-orca-compiler --output-on-failure
```

Use the Windows compiler and SDK paths from the examples above on Windows.
`compiler/build.py` emits a kernel image pack and index through device-only
compilation, `sycl-post-link` and `llvm-spirv`. `compiler::Program` loads those
images with Level Zero and binds arguments using the compiler's capture
descriptors. Multiple translation-unit packs can be added to one program.
`compiler::Stream` provides native in-order eager submission and recording/replay
on the same context, allocations and stream.

For explicitly scheduled graphs, `runtime.commands(Ordering::relaxed)` and
`stream.begin_recording(Ordering::relaxed)` request a regular Level Zero list
with `ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING`. The caller supplies barriers or
event dependencies before a command reads another command's outputs, and before
scratch or state is overwritten. Ordinary stream recording remains in order.
The driver decides whether ready kernels execute concurrently; the flag does
not create another compute engine or make unknown future inputs available.

`build.py --large-grf-kernel='<kernel-name-glob>'` records the Level Zero
`-ze-opt-large-register-file` option for matching ordinary kernels, preserving
their SPIR-V bytes. The pattern must match at least one export. ESIMD and
kernels declaring a GRF property reject this override. Existing GRF properties
continue to supply their own module options. Hardware behavior and performance
need checks on the target GPU; larger register files can reduce occupancy.

This is a compiler adapter for Strata's kernels, with pointers, standard-layout
captures and linear local scratch. It is not a general implementation of the
SYCL host API. The compiler check exercises scalar/aggregate captures, an
indirect host pointer, dynamic local scratch, ordinary and ESIMD kernels and
three native graph replays. It also checks that the process has not loaded
SYCL, UR or oneMKL. These checks passed on Windows/A770 with IntelLLVM 2025.3.2
and driver 32.0.101.9034 on 2026-10-29; native Linux GPU coverage is pending.

The independent DG2 binding is `#include <orca_l0/compiler/dg2.hpp>`.
`dg2::Program` indexes packs, and the same `compiler::parallel_for` spelling
records directly into a `dg2::Runtime`, without Level Zero kernel handles.
Compiler descriptors supply capture offsets/sizes; native metadata identifies
pointer fields inside aggregates. Resident CPU mappings, interior pointers and
GPU VAs are translated while recording; ordinary integers keep their exact bits.
Dynamic/empty local accessors share their compiler ABI with the original L0 path.
`nd_range<1..3>` and `range<1>` support ordinary and ESIMD kernels. Range group
sizes use cached required-size metadata or a dividing power of two up to 256;
this heuristic is not claimed to be optimal. SIMD1 packed xyz IDs are captured
once per recording in 32-byte thread records, with the matching walker and
prologue entry. Replay adds no parameter translation, parsing or allocation.

Build the compiler check separately with the oneAPI environment initialized;
provide the already baked memory module described above. The offline native
packer uses the vendor driver/compiler; the resulting check does not:

```powershell
cmake -S orca-l0/compiler -B build-orca-dg2-compiler -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  '-DCMAKE_CXX_COMPILER=C:/Program Files (x86)/Intel/oneAPI/compiler/2025.3/bin/icx.exe' `
  -DORCA_L0_BUILD_RUNTIME=OFF -DORCA_L0_BUILD_WDDM_BACKEND=ON `
  "-DORCA_L0_DG2_COMPILER_PACK=$PWD/build-orca-dg2-compiler/native" `
  -DORCA_L0_DG2_MEMORY_ZEBIN=C:/path/to/memory-native/image_0.zebin
cmake --build build-orca-dg2-compiler
& C:/path/to/orca-l0-native-pack.exe build-orca-dg2-compiler/images build-orca-dg2-compiler/native
ctest --test-dir build-orca-dg2-compiler --output-on-failure
```

Run builds and GPU checks under `tools.bench_deepinfer.gpu_lock()` in this
workspace. Pack entries used at launch must be baked zebin with their options
already applied. Indexed SPIR-V for unused variants does not enable a runtime
compiler fallback. Program pack addition rejects malformed/duplicate entries
without partially modifying its index.

The 2026-11-01 report records Windows A770 CTest **9/9**, including 12 mixed
pipeline replays with ordinary/ESIMD 3D groups, local VRAM float/integer outputs,
changing inputs and full guards. It checks that an address-shaped scalar is not
translated. Transfers276, local roundtrip12/GEMV48/XMX144, system-memory
regressions and 17,000 immutable captures passed. The original L0 compiler
control passed **4/4**; Linux shared-code build/CPU CTest passed **4/4**.
WSL lacks the oneAPI frontend, so neither the Linux compiler-binding build nor
native Linux GPU execution is validated. Full Strata queue/device/event
integration and general allocation release remain unfinished; frozen-list
pooling is available as described above. There is no new model-speed claim.
[Compiler-binding report](demo/dg2-compiler-windows-2026-11-01.json) and
logs, exact sources, portable inputs and Windows native images（本地归档：`demo/dg2-compiler-windows-2026-11-01.json.gz`）
preserve passing checks and failed attempts separately from earlier reports.

The Strata numerical-source integration is in `../sycl/orca/`. The earlier
verifier-only bridge remains a separate, hybrid path described in
[demo/STRATA.md](demo/STRATA.md).

## Standalone performance demo

The comparison executable lives in `demo/`. Only this executable links SYCL
to provide a baseline; the runtime library keeps its Level Zero-only dependency.
Build it separately with a SYCL compiler (Linux example, after initializing
the compiler environment):

```sh
cmake -S orca-l0/demo -B build-orca-l0-demo -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=icpx
cmake --build build-orca-l0-demo
./build-orca-l0-demo/orca-l0-demo ./build-orca-l0-demo/inference.spv 15 > dispatch.csv
```

On Windows use `icx`, the `.exe` suffix and the same Level Zero/ocloc CMake
paths shown above. When extracting the runtime as a repository, replace
`-S orca-l0` with `-S .` and `-S orca-l0/demo` with `-S demo`.

The demo imports the **same compiled native module** into SYCL, keeping the
same device, context, allocations, kernel arguments and launch geometry.
It compares SYCL eager submission, SYCL command-graph replay and Orca L0
replay for a chain of 400 dependent vector-increment kernels. Cases use
64 elements and 1,048,576 elements to expose different ratios of submission
work to GPU/memory work. This is a dispatch experiment, not a model replica.

Each timed step includes input transfer, all kernels, result transfer and
completion wait. Every output is checked exactly against a CPU reference
outside the timed region. Three warmups precede retained samples. Each native
sample is bracketed by graph baseline samples (A0/B/A1); all raw times are
printed as CSV. Summary pairing requires adjacent baseline drift <=3% and
at least five stable pairs. Both positive and negative changes are reported;
insufficient stable pairs yield `inconclusive`. Report graph-relative results
as the primary comparison when evaluating an engine that already uses graphs.
Run fresh processes to assess repeatability, without other GPU jobs or CPU
compilation. This demo does not establish a Strata token-throughput change.
It measures direct submission; the optional ring transport is exercised by
the separate runtime check rather than included in these timings.
The measured Windows A770 results and all retained samples are linked in
[demo/RESULTS.md](demo/RESULTS.md).

The experimental Strata verifier integration and its real-model checks are
described in [demo/STRATA.md](demo/STRATA.md). That hybrid path keeps SYCL
kernel compilation and graph recording, then submits the borrowed Level Zero
list through Orca. It does not remove SYCL from the engine.

## Execution contract

`Runtime` selects an Intel GPU by index across the installed Level Zero
drivers. It owns its context, compute queue, allocations, modules, kernels,
events and command lists. Resources remain alive until the runtime drains
its queues and destroys them. Stop producers and join the execution thread
before destroying the runtime. Signal outstanding host gates first.

The free `replay` and `wait` functions also accept borrowed native queues and
closed command lists. They neither destroy these handles nor synchronize an
external runtime's pending submissions automatically. The caller must flush
that runtime first, keep every owner alive, and use native completion before
resuming external submissions. An exported graph may need additional private
event-reset or copy lists; one handle alone does not establish replay safety.

`Arena` slices one device allocation using checked absolute alignment and
capacity arithmetic. Allocate weights, scratch and parameter buffers during
setup. Allocation bookkeeping is outside submission. Allocations are virtual
device memory; this API does not promise contiguous physical pages or
per-allocation reclamation.

Device allocations explicitly establish residency before returning. SPIR-V
modules disable stateful buffer addressing so kernels can access arenas above
4 GiB, matching UR's relaxed-allocation mode. Native/AOT images must already
have been compiled for the allocation sizes they will access.

`arguments(kernel, values...)` passes explicit C ABI values to
`zeKernelSetArgumentValue`. Pass pointers as pointer values and match scalar
widths, struct layout and local sizes to the compiled kernel. OpenCL C entry
points avoid SYCL's compiler-generated closure ABI. Porting an existing
SYCL/ESIMD kernel requires inspecting its ABI or giving it an explicit native
entry point; loading an arbitrary extracted image is not enough.

Regular `Commands` lists are recorded, closed, submitted and waited on.
`launch` captures the arguments at append time. Changing a kernel handle's
arguments after recording does **not** update a closed list. Put changing
values in stable device parameter buffers and upload them before each replay,
or reset and record a list after its previous execution completes. Indirect
device pointer tables also require the relevant `zeKernelSetIndirectAccess`
flags and correct residency; the runtime never enables all indirect memory
implicitly.

Memory dependencies are explicit. Put `barrier()` between dependent kernels
and copies, or use events with signal/wait dependencies. Immediate lists allow
dynamic work and copies; their appends execute immediately and cannot be
closed or replayed. An immediate copy list selects a dedicated copy engine
when available, otherwise the compute engine. For cross-engine transfers,
signal an event after the copy and wait on it before the consuming kernels.

One fence belongs to each regular list. `submit()` rejects an unclosed or
already in-flight list. `wait(timeout_ns)` returns false on timeout while
preserving in-flight ownership. A successful `wait()` or `ready()` allows the
next submission or reset. Never recycle inputs, staging buffers or events
while their GPU users are still in flight. Host-visible events use host
memory scope. Host event signaling still goes through the driver; it does
not bypass WDDM scheduling.

## Ring submission

`Ring<T, Capacity>` borrows the sequence publication and consumer gating idea
from Disruptor. It is a bounded SPSC queue with acquire/release publication,
cache-line-separated cursors, power-of-two indexing and no allocation in
`try_push`/`try_pop`. Full and empty return false; the caller chooses its
backpressure and idle policy. The target must have lock-free word atomics.

Use one ring for requests and another for completions. A single execution
thread owns Level Zero submissions and kernel mutations. A popped request
only frees a CPU ring slot: its GPU buffers remain owned until a completion
is published after a successful GPU wait. The runtime check exercises this
with 24 real GPU command-list replays and sequence-tagged completions.
Each ring has exactly one producer and one consumer. Multiple callers need
separate SPSC channels or a separately designed multi-producer sequencer.
The driver itself can still allocate or lock internally.

## Validation and scope

The CPU check tests empty/full handling, backpressure, slot wraparound and
200,000 ordered request/completion handoffs across two threads. It uses
explicit checks that remain enabled in release builds.

The GPU check compares RMSNorm → gate/up GEMV → SwiGLU against a CPU
reference, with non-multiple dimensions and three dynamic parameter updates.
It also checks module errors, arena overflow/alignment, host gates, timeouts,
in-flight reuse rejection, list reset, captured arguments and a chain of
400 dependent kernels. It separately prints recording time, mean CPU
submission time and mean submission-plus-wait time after warmup. Submission
includes fence reset. These numbers describe this microbenchmark only.

The native Strata engine integration is in [`sycl/orca`](../sycl/orca/README.md).
It runs the existing numerical kernels through native queues, command lists,
memory and events; its README records component and full-model validation.
The SYCL reference build remains available for comparisons. MoE routes,
cache misses, sequence lengths and CPU-produced results prevent treating every
decode as one fixed 48-layer list. Record stable regions and connect them with
explicit dependencies.

The proposed +8–15% decode gain and sub-10 µs submission are hypotheses.
Existing SYCL command graphs already amortize much dispatch work. Measure
CPU time exposed on the critical path against the same model, accepted
tokens, context, memory budget, GPU clocks and driver. A synthetic dispatch
improvement does not establish an end-to-end token-throughput gain.
