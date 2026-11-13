# Orca native backend: what has actually been replaced

Status on 2026-11-08. Measurements below identify their frozen profiles.

**Orca L0 is the recommended path. Orca Direct remains WIP and an alternative.**
Current matched Short brackets still put L0 ahead of Direct; each comparison
below identifies its source, executable and fixed policy.

The [Windows call-path diagram](../README.md#runtime-call-paths) is retained
separately from performance plots: SYCL uses UR and Level Zero/Intel UMD;
Orca L0 uses Level Zero/Intel UMD directly; Orca Direct encodes its own GPU
commands. All three continue through D3DKMT and the Windows kernel driver.
The default remains Orca L0; Direct is WIP. Offline kernel compilation is
separate from these execution paths.

The [early-commit control failure](../bench/results/a770/orca-direct-early-commit-2026-11-08.md)
does not establish a performance result. The candidate builds, but has zero GPU
requests: the unchanged control completes one exact 128-token Code2K request
with abnormal 474,695.8 ms prefill / 21,997.0 ms decode, then the predefined
600-second watchdog terminates its second request. One offline snapshot observes
dequantization dispatch waiting in `WaitOnAddress`; non-atomic thread capture
does not prove deadlock or a cause. A separate frozen CPU probe passes 2,112
checks with idle worker medians of 23.00–26.85 us. All failures are retained;
the candidate remains unvalidated and the default remains Orca L0.

The [adaptive-executor comparison](../bench/results/a770/orca-direct-adapt-future-2026-11-08.md)
passes 102 requests / 7,836 output tokens across six fresh processes. Replacing
per-window thread creation with the installed `std::async` executor produces
Short throughput changes of +0.377% / -1.097% in opposed brackets, with decode
control drift -0.368% / +2.100%. The CPU check confirms thread/TLS reuse, but
the model gain gate fails; it is not adopted. Both sides retain identical
disabled ETW instrumentation and the separate 5243-slot profile. This is
neither an L0 comparison nor a canonical-capacity or Linux performance result.

The [Host-stage attribution](../bench/results/a770/orca-direct-decode-stages-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero trace losses and 0.356%
control decode drift. Across 29 traced rounds, empty GPU/queue intervals
outside Owner and wait scopes total 7.018 ms: 3.460 ms in post-verify routing,
acceptance and adaptive-thread preparation, 1.657 ms within verify, and
1.084 ms in finalization. Per-window adaptive-thread creation is a candidate,
not yet an isolated cost. Both active contexts are compute contexts; only
one executes during verify, while the other appears during draft. Asynchronous
work can cross Host boundaries; this 5243-slot diagnostic changes no default.

The [completion-wait diagnostic](../bench/results/a770/orca-direct-wait-trace-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero trace losses. The traced
decode enters `NativeCompletion::wait` 128 times, with 99 registrations and
101 OS waits; neither `wait_sequence` nor `wait_fence` is entered. Two OS
timeouts are normal 50 ms wait slices, not failed requests. Of 17.086 ms
without owned DMA, outstanding packets or Owner calls, 9.434 ms is inside
OS waits, 0.326 ms in other completion-wait code and 7.326 ms outside marked
wait scopes. Wait duration includes GPU work, not just wakeup latency.
Control drift is about 3.8%; this 5243-slot diagnostic establishes neither
normal throughput nor recoverable time and changes no default.

The [actual decode-owner diagnostic](../bench/results/a770/orca-direct-owner-handoff-2026-11-08.md)
passes 36 requests / 2,868 output tokens with zero trace losses. The traced
decode contains 128 cross-thread calls, all at one Owner's replay callsite.
Publish-to-run and end-to-return intervals total 9.403 ms, overlapping 4.897 ms
of the 17.902 ms without owned DMA or outstanding packets; 2.176 ms falls in
Owner bodies and 10.829 ms outside Owner intervals. All three Direct windows
use two active GPU contexts. This narrows attribution; it is neither a normal
speed result nor a CallerOwner adoption gate, and does not prove recoverable time.

The [idle-owner CPU diagnostic](../bench/results/a770/orca-direct-owner-idle-2026-11-08.md)
passes 2,112 return/ownership checks in six alternating worker/caller windows.
After measured idle intervals of about 16/31–32/63 ms, worker invoke medians
are 22.55–52.05 us versus caller's 1.60–3.10 us; hot medians are 0.90–1.50
versus 0.20 us. This does not measure actual model handoffs or GPU work;
the previous CallerOwner model experiment still has no established gain.

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

The [idle-only reclamation qualification](../bench/results/a770/orca-direct-thread-trim-2026-11-08.md)
passes Host/device lifecycle checks, all 19 existing CTests, Linux compilation/CPU
checks, and eight complete frozen HC128/MTP4/min-p0.5 model protocols. The explicit
`Runtime::trim_thread_memory()` protects every current queue binding and retained
list. Isolated startup recapture plus trim frees 73.5 MiB: thread pools fall from
344 to 270.625 MiB, descriptors from 0.25 to 0.125 MiB. Actual capacity is 5707
slots, without a pass of the original 5771-slot performance conditions or a new
speed/PPL result. The initial Host-rounding expectation and link-directory
failures remain recorded. Recapture/model auto-trim remain off by default;
deployed EXEs are unchanged and Linux GPU validation remains pending.

The [pool reference audit](../bench/results/a770/orca-direct-thread-pools-2026-11-08.md)
finds 122.5 MiB of older pools still retained by recorded lists, alongside
221.5 MiB currently bound. A separate [READY-only recapture observation](../bench/results/a770/orca-direct-pool-recapture-2026-11-08.md)
leaves three old pools (73.375 MiB) unreferenced; one older 49.125 MiB pool is
still retained. No memory was freed: all eight pools still total 344 MiB.
Actual slots are 5413/5586, without GEN, a new performance claim or a pass of
the original 5771-slot performance conditions. Recapture is not default-enabled.

The [READY memory ownership inventory](../bench/results/a770/orca-direct-memory-inventory-2026-11-08.md)
finds 351.875 MiB of native internal local storage: eight scratch/private pools
account for 344 MiB; parameter mirrors use 2 MiB, ISA 4 MiB, module data 1.625 MiB
and scratch descriptors 0.25 MiB. All five handle/byte partitions and exact-source
normalization/archive checks pass. This frozen HC128/MTP4/min-p0.5 diagnostic
sends no GEN and uses 5569 slots; it neither passes the original 5771-slot
performance conditions nor measures the corresponding L0 categories. It narrows
the memory investigation without establishing a decode cause or speed gain.

The [current native-runtime model qualification](../bench/results/a770/orca-direct-native-import-model-2026-11-08.md)
now passes eight complete 2K/8K/Short/Prose2K protocols with the 19-check-qualified
runtime and frozen HC128/MTP4/min-p0.5 math/Host policy. No SYCL, UR, Level Zero,
IGC or GMM DLL is loaded. The functional run used 5577 slots. Its destructor-only
allocation summary was skipped by the model's `_Exit`; that observation gap is
retained. Two separate 2K/8K diagnostic requests use live flushed counters and
match exactly at 5567 slots: 1143 successful native allocation calls, zero caller-
page or StandardAllocation calls. Thus these model requests do not exercise the
new external-import or larger-than-2-GiB owned-Host paths; those capabilities do
not explain the previous decode gap. The exact-source/protocol/archive check
passes. These are functional and allocation-path checks, without a new speed/PPL
claim or a pass of the original 5771-slot performance conditions. Deployment and
Linux GPU status are unchanged.

The [Windows memory-accounting diagnostic](../bench/results/a770/orca-direct-memory-budget-2026-11-08.md)
finds that both frozen L0 controls return `have_sysman=0`, leaving INFO constrained
only by DXGI process-budget headroom. Their INFO exceeds independent global
queries by about 623--644 MiB; Direct's global counter agrees with the independent
query. The exact checked Windows L0 Host source now also applies the global
residency limit, caching topology and refreshing residency at capacity checks.
Six query-fault/recovery checks and eight complete 2K/8K/Short/Prose2K protocols
pass. The strict 5771-slot attempt instead obtained 5734 slots and remains failed;
the separate functional run obtained 5673. These are not new same-slot performance
results. Reserve1536 is still configured; it does not guarantee that much remains
after later graph allocations. Source is adopted; deployed EXEs, numerical kernels,
configuration and Linux GPU status are unchanged.

The [native caller-page import qualification](../bench/results/a770/orca-direct-native-import-2026-11-08.md)
now has eight successful driver calibrations through 4 GiB + 64 KiB, with full
CPU contents/base lookup/caller ownership and no GPU command submissions. Seven
eligible descriptors match the actual C++ serializer exactly, including 2 GiB;
two larger vendor descriptors remain outside its per-part scope. An isolated
current-source candidate removes both remaining Standard ExistingHeap paths,
using at most 2 GiB parts for imports and larger owned Host spans. All 19 CTests
and dynamic-token semantic checks now pass, including full 4 GiB + 64 KiB owned/
imported contents, GPU boundary reads/writes and five injected import failures.
The exact checked source is adopted into Windows Direct; every unchanged source
file also matches the checked snapshot. Original preparation/reference-layout/
log-decoding failures remain. No deployment or model-speed claim changes. The
frozen --stream-experts model has no external PinnedArena, so import calibration
alone cannot explain its decode gap.

The [driver-owned Host allocation diagnostic](../bench/results/a770/orca-direct-native-host-2026-11-08.md)
matches all 84 request protocols with fixed HC128/MTP4/min-p0.5/5771 slots.
Against the same Direct backend's standard allocation, first 2K TTFT takes
15.155%/17.419% less time in opposed orders. Warm Short decode time changes
+0.044%/−0.969%, so no stable decode gain is established. Endpoint drift and
regression gates pass; the allocation component itself exceeds the 3% drift
limit and is only diagnostic. The candidate passes 2 GiB contents, six injected
failure paths and 920 GPU transfer replays. A separate 84-request untimed L0
comparison now passes complete output/counter and control-drift checks (maximum
2.232%). Short decode is 2.254%/2.077% below L0, narrowing to 1.359%/0.308% on
repeated 8K. Short TTFT takes 10.588%/10.430% less time and whole requests
2.237%/2.152% less time; the overall no-regression gate fails because decode
remains behind. This uses the frozen runtime and fixed 5771-slot policy, not
current spec6 production. The current-source model attempt stopped at READY
with 5763 versus L0's 5771 slots; that failure remains. The qualified Host path
is now adopted into Windows Direct source. A fresh build passes all 18 registered
CTests, including full Host contents/ownership, six injected failures and 920 GPU
transfer replays. These checks do not establish current-source model performance.
Deployment is unchanged and Linux GPU execution remains untested.

The [short-prompt route-heat ablation](../bench/results/a770/orca-direct-short-route-heat-2026-11-07.md)
reuses the completed prompt windows' mapped expert IDs, with the existing
0.125 heat weight and at most four predecode LFU batches. All 120 Short
protocols match. Opposed warm brackets improve Direct decode
**+1.008%/+1.540%**, TTFT time **-13.497%/-13.450%**, and directly timed
whole-request time **-5.068%/-6.279%**. Decode excludes prompt/cache preparation;
those costs remain in TTFT and whole-request time. Warm decode control drift
is +0.137%/-0.196%; the frozen performance gates pass. Cold reverse decode
effect is smaller than its control drift, so cold-start acceleration remains
unproven. Direct reaches 50.942/50.974 tok/s; outer L0 controls are
51.192/51.241 tok/s, so this still does not establish an overall Direct lead.

After that isolated measurement, eight additional off/on
2K/8K/Prose2K/Short protocols match exactly. The
[subsequent source adoption](../sycl/orca/direct-short-route-heat-adoption-a770-windows-2026-11-07.json)
enables this preparation by default for Direct; L0 retains its disabled
default. `STRATA_SHORT_ROUTE_HEAT=0` rolls back. A request with no new prompt
rows skips collection and preparation. The frozen generated frontend equals
the tested candidate after normalizing only that default/empty-read block;
the same patch commutes with current frontend generation. Both exact-byte
archives pass verification. The benchmark's unchanged-production statement
records its sampling stage; this paragraph records the later source adoption.
Deployed EXEs, configuration and README headlines remain unchanged; rebuild
to use the source default. Evidence remains HC128/MTP4/min-p0.5/5771 slots,
not the separate spec6/calibrated production profile. No broad PPL, Linux
GPU or historical intermittent 8K-fault resolution is claimed.

The [routing and expert-slot diagnostic](../bench/results/a770/orca-direct-q2-residency-2026-11-07.md)
checks 70 complete protocols, 2,300 windows and 3,219,840 routed entries with
the same frozen HC128/MTP4/min-p0.5/5771-slot profile. For matching complete
request histories, all 35 L0/Direct request pairs have identical expert IDs,
Host slot indices and planned VRAM-or-mirror classes. Long-request history
changes the later Short cache state equally on both backends: each warmed
Short has 1,190 mirror entries instead of 1,476 after Short-only warmup, a
19.4% reduction. This is a Host-table diagnostic, not a measurement of actual
PCIe traffic, physical residency, device-cache hits or performance. It does
not explain the backend timing gap. All original failures and exact-byte
evidence are retained; source, deployed EXEs and defaults are unchanged.

The [matched-HC128 context comparison](../bench/results/a770/orca-direct-q2-contexts-2026-11-07.md)
uses the qualified/adopted Direct Q2_0 fork and the same numerical pack on both
backends. All 120 protocols match across six fresh processes and opposed
2K/2K/8K/8K/Prose2K/Short histories. Direct Short decode is **-0.260%/-2.572%**
versus L0; repeated 8K is **+1.352%/-1.273%**. Thus repeated 8K improves the
relative gap by 1.612/1.298 percentage points versus Short in these two fixed
histories, but forward control drift exceeds the 3% limit and reaches 4.375%
across metrics. This does not establish a repeatable context-length effect or
identify API dispatch, cache state or locks as its cause. Repeated 2K TTFT
uses **18.628%/17.320% less time**, repeated 8K **9.823%/8.177% less**; first
2K instead uses **3.638%/3.432% more**. Decode excludes startup/preparation/
prefill; all timing, drift failures and raw protocols are retained. Source,
artifact and cached asset identities plus the exact-byte text archive pass
verification. The frozen profile is MTP4/min-p0.5/5771 actual slots, not the
separate current spec6/calibrated policy. No deployed executable, configuration
or README headline changes; historical intermittent 8K faults, broad PPL and
Linux GPU validation remain open.

The [Q2_0 shared-down fork qualification](../bench/results/a770/orca-direct-q2-fork-2026-11-07.md)
adds the five previously excluded layers to Direct's existing full-fork path
under its unchanged one-group, at-most-four-row conditions. Both native safety
probes pass 2,304 complete input/output/scratch/guard comparisons over T1--8
and changing routed plans; the full fork itself remains limited to T1--4.
All 120 same-HC128/MTP4 Short model protocols match exactly. Direct decode
improves +1.743%/+0.851% in opposed brackets, versus -0.700%/-0.239%
control drift; TTFT time changes -0.282%/-0.436% and whole-request time
-1.283%/-0.155%. Candidate decode is 49.508/49.233 tok/s; outer L0 is
50.190 tok/s with -0.201% drift, so Direct still does not lead L0 overall.
The [source adoption proof](../sycl/orca/direct-q2-fork-adoption-a770-windows-2026-11-07.json)
uses the existing `STRATA_ORCA_DG2` compile macro: the entire generated Direct
Verifier equals the tested candidate, while the L0 Verifier equals its old
baseline. The shared phase5 wrapper equals both safety-tested versions.
No new runtime flag, numerical kernel, scratch layout or public ABI is added.
This source change is adopted for Direct; L0's Verifier policy, deployed EXEs,
configuration and README headline remain unchanged. The frozen profile is not
the separate current spec6/calibrated policy. Both text archives pass byte
round-trip checks; there is no new long-context, PPL or Linux GPU claim.

The [real HC-to-router phase-order diagnostic](../bench/results/a770/orca-direct-hc-router-2026-11-07.md)
captures all 48 real T4 layers, with the complete 70-token model protocol exact
and all HC capture bytes identical to the preceding real-HC check. The actual
model uses the early router/quant fork in 43 layers; layers 6, 7, 11, 22 and 30
fall back. The standalone graphs retain a three-way router/quant fork for all
cases, so those five fallback schedules are not reproduced. All 48 work
protocols, 1,032,192 replay launches and 144 independent FP64/BF16/Q8/TopK plus
full-buffer/guard case checks pass. Grouping independent HC cases before their
router stages reduces Host replay/wait time by 13.188--19.109% across the three
processes. This reordering is diagnostic: it omits real cross-layer recurrence
and changes memory/cache/state/dependency history together. Direct time versus
L0 is +0.683%/-0.644% for interleaved 384/1536-kernel lists and
-2.272%/-1.910% for grouped lists; every apparent Direct advantage is smaller
than its corresponding L0 control drift, which reaches 7.120%. All samples and
both failed capture revisions are retained without a performance rerun. The
third capture records data after BF16/native-Q8 preparation; non-fork shared
scratch is internally reused, so its replay oracle uses captured native-Q8
bytes, separately checked against the replay quantizer. The fresh offline
protocol/pin audit and exact-byte text archive pass. No stable model-speed,
cache-specific cause, production-default or Linux GPU claim follows.

The [same-ZEBIN hardware-counter comparison](../bench/results/a770/orca-direct-gpu-counters-2026-11-07.md)
finishes 75 matching Short protocols in L0/Direct/L0 order, with the same HC128
native pack on both frozen full-current engines. Clean decode is 50.229 versus
48.920 tok/s, Direct -2.606%; closing/opening L0 decode drift is -0.390%.
Device-wide whole-request counters show Direct memory reads -0.482%, writes
+2.556%, average EU active core cycles -0.647% and stall core cycles +3.498%.
These counters include prefill, desktop activity and collection-control boundaries;
they do not isolate decode, PCIe traffic or a root cause. All three new GPU timers
pass validation. An earlier interrupted 50-request run retains two impossible
Direct elapsed-timer bins, invalidating that run's frequency estimate; it cannot
establish downclocking. Its samples and the earlier readiness failure remain
separate. No product runtime, default, deployed EXE or Linux GPU claim changes.

The [timestamp-free HC norm counters](../bench/results/a770/orca-direct-hc-counters-2026-11-07.md)
keep the same frozen full-current headers/runtimes and HC128 native pack, with
T4, residuals and 48 disjoint banks of checked synthetic input. All 30 work
protocols and 12,976,128 norm launches pass complete output/source/guard checks.
At 192 norms per replay, Direct Host replay plus per-list wait is 8.237 versus
7.913 microseconds/norm, +4.097%; closing/opening L0 drift is +0.061%.
Device-wide counters show reads -1.028%, writes -1.549%, average EU active
core cycles -0.697% and stall core cycles +1.368%; all three timers validate.
These counters include validation and control boundaries and cannot isolate a
kernel or establish the cause. Host times are neither pure GPU spans nor model
tok/s, and cannot be subtracted from earlier endpoint-timestamp measurements.
The offline publisher's initial same-name module import failure is preserved;
loading the unchanged parser by exact path fixes reporting without a GPU rerun.
All source/protocol hashes and exact-byte archive round-trips pass; production
defaults, the deployed engine and Linux GPU status remain unchanged.

The [paired 192/768 replay check](../bench/results/a770/orca-direct-hc-batching-2026-11-07.md)
and its [opposed-order repeat](../bench/results/a770/orca-direct-hc-batching-reverse-2026-11-07.md)
keep total work fixed at 196,608 norms per measured sample, using the same two
EXEs and mathematical pack. Direct Host time is +3.975%/+3.552% at 192 norms
per list, but -4.017%/-1.417% at 768; increasing list length reduces replay/wait
boundaries fourfold without waiting for extra work or fusing kernels.
The reverse 768 control drift is +2.060%, larger than its 1.417% time advantage;
these observations do not establish a stable model-speed gain or a single cause.
All 120 work protocols pass complete output/source/guard checks. Mixed-size
device-wide counters cannot distinguish the two sizes. Two impossible elapsed
timer bins also occur in the opening forward L0 arm, invalidating its frequency
estimate; clean Host times do not use those counters. The prior
[caller-owner check](../bench/results/a770/orca-direct-caller-owner-2026-11-05.md)
did not establish model acceleration, so boundary sensitivity alone does not
identify thread handoff as the cause. Production defaults remain unchanged.
Both archives pass exact-byte round-trip verification, preserving all samples
and the reverse advantage's control-drift limitation.

The [timestamp-free full HC chain](../bench/results/a770/orca-direct-hc-chain-2026-11-07.md)
links the frozen model Host wrappers and common HC128 native pack, interleaving
norm/down-sliced/reduce/up-Q8 at T4 with residuals and one hot weight group.
Direct replay plus per-list wait is -0.088% at 192 kernels/list and -3.446%
at 768; outer L0 time drift is +0.225%/-0.383%. All 60 work protocols,
6,488,064 native launches and independent FP64/Q8/full-source/output/guard
checks pass. The offline publisher first rejects mismatched counter grids;
its revision preserves every exported bin and records Direct's missing final
read/Busy bin, disabling the read-byte comparison. The Direct elapsed timer
also fails validation, so its average frequency is invalid. No GPU rerun or
repair of raw counters occurs; Host times do not use those counters.

The [same-process 1/48-weight-group check](../bench/results/a770/orca-direct-hc-chain-banks-2026-11-07.md)
allocates all 48 independent cloned BF16 weight groups in every process,
then changes only the groups touched by each graph and the list length.
Requested weight buffers plus guards total 603.785 MiB; this is not measured
physical VRAM usage or a DRAM-bank mapping. The four graphs share input/output
addresses, math and allocation layout, with equal total work per sample.

| Touched weight groups | Kernels/list | Direct Host time versus L0 | Closing/opening L0 time drift |
| --- | ---: | ---: | ---: |
| 1 | 192 | -0.272% | -0.239% |
| 1 | 768 | -3.731% | -0.209% |
| 48 | 192 | +0.899% | -0.083% |
| 48 | 768 | -1.924% | -0.119% |

Expanding the touched weights increases Direct time by 34.331%/35.613%,
versus 32.668--33.174% across the L0 controls. The larger workset reverses
the small-list advantage and reduces the long-list advantage; it still does
not reproduce the full model's 2.606% throughput deficit. These are one
L0/Direct/L0 ordering, not confidence intervals or opposed-order confirmation.
All 120 work protocols and 12,976,128 native launches pass the independent
FP64/Q8 and complete source/output/guard checks. Counters mix all four cases
and full 48-group validation readbacks; they cannot attribute per-case memory
traffic or a cache-level cause. The opening L0 elapsed timer is invalid and
retained. Both full-HC archives pass exact-byte round-trip checks. Synthetic
cloned weights omit real layer intermediates and cross-layer input recurrence;
real-model surrounding operators and required synchronization remain to be
isolated. Production defaults, deployed EXEs and Linux GPU status are unchanged.

The [residual input/output aliasing control](../bench/results/a770/orca-direct-hc-alias-2026-11-07.md)
addresses another model/probe difference: Verifier uses `R == R_out`, whereas
the earlier HC probes read a separate R buffer. All four new graphs touch the
same 48 weight groups and write the same output addresses. Both modes retain
`apply=true` but use a zero residual addend, keeping repeated R values fixed.
At 192 kernels/list, Direct time is +0.282% with separate input and +0.157%
with aliased input; at 768, it is -2.394%/-2.524%. Maximum outer L0 drift is
0.502%. Within Direct, aliasing changes time by -0.004%/+0.125%; L0 controls
change by -0.220% to +0.464%. This control does not show an enlarged Direct
deficit from address aliasing alone. It does not verify nonzero recurrent data
dependencies or rule them out in the model. All 120 protocols, 12,976,128
native launches and independent FP64/Q8/full-source/output/guard checks pass;
all outputs hash to `e4947135518a19d0`. The opening L0 elapsed timer is invalid
and retained; Host timing is independent of that counter. The archive passes
exact-byte round-trip verification, with no production or Linux GPU change.

The [nonzero two-state follow-up](../bench/results/a770/orca-direct-hc-alias-recurrence-2026-11-07.md)
alternates residual addends of +2^-10/-2^-10 with zero injection logits and
exact dyadic R0/R1 values. Separate-input and in-place graphs perform the same
math at each step; both finish at R0 after each even-length list. This adds
a bounded nonzero read-after-write dependency without accumulating numerical
drift. It still uses synthetic cloned weights rather than real model history.

| Nonzero residual input | Kernels/list | Direct Host time versus L0 | Closing/opening L0 time drift |
| --- | ---: | ---: | ---: |
| Separate R0/R1 | 192 | +0.825% | -0.358% |
| Separate R0/R1 | 768 | -1.569% | -0.371% |
| In-place R | 192 | +0.901% | -0.788% |
| In-place R | 768 | -1.503% | -0.377% |

Within Direct, in-place time changes +0.022%/-0.283%; the L0 controls range
from -0.353% to +0.163%. This one L0/Direct/L0 ordering does not show the
nonzero in-place dependency enlarging the backend gap; it does not exclude
arbitrary model recurrence or surrounding operators as contributors. All
120 work protocols, 12,976,128 native launches and independent FP64/Q8 plus
full input/output/guard checks pass. The complete output again hashes to
`e4947135518a19d0`, with maximum CPU-reference absolute error 4.173e-7.
Direct's elapsed timer is invalid and retained, so no frequency claim follows.
The completed publication passes a fresh offline exact-byte/archive/protocol
check without a GPU rerun. No production or Linux GPU change is adopted.

The [real-model 48-layer HC1 replay](../bench/results/a770/orca-direct-hc-real-2026-11-07.md)
records one actual T4 window's inputs, weights and complete outputs from the
frozen full-current diagnostic Direct engine. Its entire 70-token Short protocol
still matches the reference, including draft counts, stop and zero reuse/resume.
All 48 private capture files total **663,379,968 bytes**; weight payloads stay
local, with only hashes in the public evidence. Recording adds diagnostic copy
nodes and is not a throughput measurement.

Independent L0/Direct/L0 processes replay all 48 real cases with the common
HC128 mathematical pack, frozen Host wrappers/runtimes and Direct residency
object. They read immutable R0 and write disjoint R1, omitting the original
model's in-place cross-layer recurrence and surrounding operators. Every full
output matches the model capture bitwise. All **144** independent case checks
pass the original FP64 tolerance, separate/fused Q8 and complete source/output/
guard checks; maximum CPU absolute error is 4.598477e-5 in every arm. Output hash
is `b71f656ec4f65bce`. No numerical kernel is rebuilt.

| Real HC graph | L0 Host us/read | Direct Host us/read | Direct time change | L0 time drift |
| --- | ---: | ---: | ---: | ---: |
| 192 kernels/list | 75.724288 | 76.975643 | +1.653% | -0.065% |
| 768 kernels/list | 72.976021 | 72.742578 | -0.320% | +0.660% |

All **30** work protocols and **663,552** replay kernel launches pass; that count
excludes startup numerical checks and memory operations. Each cell has
four retained 6,144-read samples per process, with two warmups; reset, full
validation and readback stay outside the Host replay/per-list-wait timer.
There are no GPU timestamps or profiler. The smaller long-list advantage is
below observed control drift and does not establish a stable speed gain.
This real-data fixture still does not reproduce the whole model's complete
deficit. Data and allocation layout both differ from the seeded fixture, so
the cross-run difference cannot be attributed solely to real numerical values.
Surrounding operators, allocation layout and arbitrary model recurrence remain
to be isolated. These Host times are neither pure GPU spans nor model tok/s.
The initial post-build byte comparison fails on generated CRLF versus LF;
both probes had already compiled/linked. The original script/failure remains,
and normalized text plus actual compiled-byte/library/header hashes verify
without rebuilding or a GPU rescue rerun. Exact-byte text archive checks pass;
production defaults, deployed EXEs, README scores, PPL and Linux GPU status
remain unchanged. Historical Direct 8K/nonfinite/residency issues stay open.

The [same-math real-model Router/Quant isolation](../bench/results/a770/orca-direct-router-serial-2026-11-07.md)
keeps early quantization in place and changes only its one `independent`
wrapper into a sequential lambda. All mathematical kernel calls, other forks,
Host libraries, the Direct residency object and the common HC128 native pack
are preserved. Eight fresh-process arms are fixed in advance:
L0/parallel/serial/parallel/serial/parallel/serial/L0. Each completes 15 Short
requests, discards ten warmups and retains five; all **120** complete token,
draft-count, stop and reuse/resume protocols match and are independently parsed.
No profiler, GPU timestamps or inner timing switches run in this comparison.

| Opposed Direct bracket | Serial decode change | Serial TTFT time change | Serial request time change | Control decode drift |
| --- | ---: | ---: | ---: | ---: |
| parallel/serial/parallel | -1.436% | +1.232% | +1.815% | +0.142% |
| serial/parallel/serial | -0.489% | +0.277% | +0.660% | +0.085% |

Net decode excludes preparation, initialization and prefill. Serial is slower
in both orderings, so this concurrency fork is retained; removing it does not
close Direct's deficit. The smaller reverse TTFT change is below its 1.217%
control drift, so it does not establish a stable TTFT difference. Outer L0
decode drift is +0.820%; those outer controls span the entire run, rather than
pairing locally with each Direct arm. All samples and both orders are retained;
control drift is not a confidence interval. The initial build-input audit failure
occurs before compilation/GPU and is preserved separately. The first offline
publisher assumes nine DONE fields and rejects the frozen engine's sixteen-field
protocol before publishing; the corrected parser checks all appended counters
and the full prompt read. Both failed scripts and records remain archived,
without a GPU rerun; shared pins and the exact-byte archive round-trip pass.
No production default,
deployed EXE, main README result, PPL or Linux GPU status changes. This fixed
Short screen does not close arbitrary model recurrence or historical Direct
8K/nonfinite/residency issues.

The [preparation-default adoption](../bench/results/a770/orca-preparation-defaults-2026-11-06.md)
enables known MTP position preparation and persistent-mirror service refill
batching in Orca L0 source. Rebuild to apply; `STRATA_MTP_PRESTAGE=0` and
`STRATA_ORCA_MIRROR_REFILL=0` independently restore the previous paths.
The earlier small measurements remain separate from whole-version speed claims.
This source change does not replace the deployed HC SG16 executable or change
draft confidence thresholds.
The L0 rebuild matches all eight complete default/rollback protocols across
fresh 2K, repeat 2K, 8K and Short histories.
Direct keeps these switches opt-in after the combined check exited on nonfinite
activations in its third request, 8K prefill; that failure is retained separately.
The [bounded four-arm follow-up](../bench/results/a770/orca-direct-nonfinite-factors-2026-11-06.md)
matches all 16 Direct protocols across both switches off, each alone and both on.
Six Direct/L0 real-NaN cases also verify input/hidden classification and reset.
The historical failure did not recur; its cause remains unresolved, with no new
performance claim or change to Direct defaults.

The local L0 configuration also enables [accepted-row-only MTP catch-up](../bench/results/a770/orca-mtp-accepted-only-adoption-2026-11-06.md)
after the user's small-gain adoption request. Code2K decode observations are
+0.964%/+0.499%, Prose2K +0.767%/+0.102% in opposed orders; the original
reverse gain gate remains failed. All 42 measurement and 18 mixed-8K/HTTP
protocols match. This config-only change takes effect next launch, retains
0.5 confidence thresholds and does not claim improved TTFT for every workload.

The [timestamp visibility correction](../bench/results/a770/orca-timestamp-visibility-2026-11-06.md)
now lets ordered timestamps follow the queue's device/host visibility policy;
completion signals keep their host flush. Both native gate checks and 5,040
full norm replays pass, along with Linux DRM compilation and CPU encoding checks.
Dense-marker overhead changes substantially, but 54 of 144 comparisons exceed
the 3% drift reference. This improves the diagnostic path without a model-speed
claim, production-EXE replacement or Linux GPU validation.

The [sparse real-model HC check](../bench/results/a770/orca-direct-hc-sparse-2026-11-06.md)
uses the corrected timestamp runtime and only GDN layer 0/QSA layer 47: twelve
ordered marks per window instead of the earlier 242. All 90 protocols and
2,610 windows match. The selected norm intervals remain 9.266%/9.511% longer
than L0; whole-window GPU spans are 2.269%/1.533% longer in opposed orders.
These two layers do not represent all 48, and changing both marker density
and visibility prevents a single-factor subtraction from the old dense data.
The earlier roughly 95% norm gap must not be treated as pure kernel overhead.

The [ordered-timestamp wrapper cleanup](../bench/results/a770/orca-ordered-stamp-2026-11-06.md)
now omits Direct's duplicate pre-timestamp barrier. The native timestamp keeps
its CS stall/HDC/untyped flush, final host completion and subsequent ordering;
L0 retains its explicit barrier. All 5,040 replays match, with identical other
compiled headers. Dense-marker representative intervals shrink about 3--5%,
but seven of 144 GPU comparisons exceed the 3% drift reference. This source
adoption takes effect after rebuilding and leaves normal timestamp-free decode
unchanged. There is no new model-speed or Linux GPU claim.

The [full HC walker audit](../bench/results/a770/orca-direct-hc-walker-audit-2026-11-06.md)
checks retained same-math captures at every actual command boundary, including
160 norm and 80 fused-Q8 up walkers per backend. After explicit address/inline
exclusions and the separately reported payload-length/SLM differences, the
remaining control difference is walker post-sync: L0 embeds a completion-counter
write with dataport pipeline/subslice flushes; Direct uses subsequent barriers
and list completion instead. This establishes a synchronization-mechanism
difference, without new GPU execution or a latency/performance attribution.

The [walker-flush isolation](../bench/results/a770/orca-direct-walker-flush-2026-11-06.md)
puts the observed dataport flushes inside Direct walkers, retaining CS stalls
and conservative Host/state/timestamp completion. Packed queue-private cache
lines fix the first candidate's allocation-plateau failure without weakening
that assertion. Both policies pass 28 native checks and 128 full HC cases;
480 selected actual walkers verify activation and unchanged other controls.
All 90 clean Short protocols match, but opposed net decode changes are
-0.275%/-1.434%, with maximum control drift 3.470% against the 3% reference.
The candidate is not adopted. These Direct-only results do not establish
the synchronization mechanism as the cause of the Direct/L0 gap, and add no
long-context, PPL or Linux GPU claim. Both earlier failures remain recorded.

The [complete pipeline diagram](ORCA_PARALLEL_PIPELINE.md) maps request flow,
per-layer dependencies and CPU/BCS/compute staging. It marks conditional overlap,
explicit copy roles, logical versus physical queues and buffer ownership.

The [full current-source comparison](../bench/results/a770/orca-direct-full-current-2026-11-06.md)
rebuilds both backends and matches all 120 complete protocols and draft counters,
with current shared tail clipping and Direct128/L0256 defaults. Direct repeat-2K
TTFT is 16.630%/13.509% shorter in opposed orders; repeat-8K is 7.154%/6.765%
shorter. Short net decode remains 51.192/51.384 tok/s versus L0's
52.493/52.755 (-2.479%/-2.598%). First-2K TTFT is instead 8.011%/4.573% longer.
Maximum control drift is 5.866%, exceeding the preset 3% gate. These are retained
observations, not a stable all-workload speedup. Repeat-8K decode gaps are
-1.374%/-2.964%; the gap does not consistently shrink relative to Short.
The initial attempt failed before Direct READY on local residency of the
7.74-GiB cache. Its failure remains archived and unresolved. The completed run
uses a diagnostic-only runtime relink; 17 controlled allocation rejections and
retries pass, without changing residency policy, cache size or reserve.
Both full builds pass their PLE CPU checks and 64 HC cases per backend.
No production EXE/config or Linux GPU status changes follow from this test.

The [native global-memory accounting check](../bench/results/a770/orca-direct-global-memory-2026-11-06.md)
now bounds Windows Direct availability by local-segment residency as well as
the process budget and tracked allocations. Adapter/segment identity is cached;
only memory checks refresh residency. Independent Sysman calibration observes a
775.113-MiB process-budget overestimate in an idle observer. Old-runtime negative
controls and the new allocation/error checks pass, followed by seven exact
2K/8K/Short model protocols at the same 5771 slots and 1536-MiB reserve.
There are no added queries in kernel dispatch or successful allocation paths.
This corrects accounting without a new speed claim, production deployment,
Linux GPU result or closure of the earlier residency/8K failures.

The [current Short state census](../bench/results/a770/orca-direct-state-domains-2026-11-06.md)
weights encoded commands by actual successful submissions, including frozen-list
replays. Both exact-output requests execute 67,271 walkers, with 3,934 GRF-only,
50 parameter-base-only and 28 scratch-base-only transitions each. No within-list
systolic transition or invariant-SBA rewrite occurs, excluding that proposed
redundancy for these requests. The trace includes whole-request work and intrusive
logging; it is not net-decode timing or a new speed result. The original publisher
retains two JSON-key comparison failures; the [supplemental audit](../bench/results/a770/orca-direct-state-domains-2026-11-06-audit.md)
passes raw protocols, packet/list invariants and exact-archive checks without
rerunning GPU. Runtime defaults and deployed binaries remain unchanged.

The [startup window-recapture experiment](../bench/results/a770/orca-direct-scratch-recapture-2026-11-06.md)
keeps the grown scratch pools and records Verifier windows again before READY.
Four exact diagnostic protocols reduce scratch-base changes from 28 to 7 and
parameter-base changes from 50 to 0 per whole request, with 67,271 walkers
unchanged. The changed recording layout prevents attributing this to scratch
alone. All 90 clean model protocols match at 5771 slots and 1536-MiB reserve,
but opposed-order net decode changes are -0.352%/-0.582%; TTFT changes are
-1.751%/+0.497%. Control drift passes the preset 3% gate; the gain gate fails.
This is not a small positive result, so the candidate remains isolated.
READY is reported separately; no default, deployment or Linux GPU status changes.

The [physical-context census of the frozen full builds](../bench/results/a770/orca-direct-queue-topology-2026-11-06.md)
matches all 14 calibrated 2K/8K/Prose2K/Short protocols and all 28 phase totals.
Net decode uses one compute plus one copy context on L0, versus three compute
plus one copy context on Direct, in every tested context length. Whole-process
creation totals are four (2 compute/2 copy) and nine (6 compute/3 copy),
respectively; those totals include contexts that are inactive in net decode.
L0 host synchronization can itself submit commands. The first seven exact L0
protocols retain a failed API-attribution check; the corrected observer adds
synchronization/reset/teardown scopes without changing math or runtime policy.
The report's API-handle column counts distinct (API, first-argument handle)
identities, not unique handles or application Streams. Intrusive logging and
the census lock prevent speed attribution. This establishes a scheduling
difference that persists at 8K, not its latency cost or a reason to merge contexts
without first proving HostGate progress, scratch/fence ownership and dependencies.

The service source now defaults to preparing the known next-window PLE row
after commit submission and before MTP. The next gather still collects its
reader ticket before consumption; `STRATA_ORCA_PLE_ROW0=0` restores the old
order. This reuses private reader buffers and its worker, without mapped GPU
input writes. The [source adoption evidence](../bench/results/a770/orca-ple-row0-source-2026-11-05.md)
retains the initial Direct 8K all-zero failure and subsequent same-EXE controls.
All 24 follow-up protocols match on Direct/L0 with the switch unset, 0 and 1,
each starting a fresh 2K→2K→8K→Short history; the initial failure is retained.
The [consumer follow-up](../bench/results/a770/orca-ple-prefix-2026-11-05.md)
reuses an ordered matching prefetch prefix and submits only the suffix before
collecting all writers. One prefetched token followed by four tokens now needs
64 rather than 80 logical row requests; physical I/O was not measured.
The extended CPU selftest passes 336 prefix/batch cases per reader implementation,
and the native CPU-only CMake/CTest target passes. All 16 rebuilt U4 Host/retained-
library protocols match across Direct/L0 through cold Code2K→8K→Prose2K→Short
histories, using HC128. That consumer check did not cover HC256; the later
full-source comparison above covers current Direct128/L0256 defaults.
Rebuilding is required; deployment is unchanged and the intermittent fault remains open.
No stable model-speed gain is claimed for this scheduling change.

The [earlier HC128 endpoint diagnostic](../bench/results/a770/orca-direct-hc1-span-2026-11-05.md)
passes 360 complete output/guard/span checks. T1 fused-Q8 Direct GPU spans
were about 19% longer than L0, locating a difference inside the GPU list too.
A [one-image 256-GRF ablation](../bench/results/a770/orca-hc1-q8-grf-2026-11-05.md)
removes reported spills, retains the same payload ABI and passes 960 output/
guard checks. Component replay improves on both backends, while global private
memory remains. Component results require separate model checks; neither
result closes the Direct 8K fault.

The [subsequent clean Short model screen](../bench/results/a770/orca-hc1-q8-grf-model-2026-11-05.md)
matches 120 protocols in opposed 128/256-GRF orders, using the same EXE per
backend. Direct improves **57.936→59.650 tok/s (+2.959%)** and L0 improves
**59.575→60.708 (+1.901%)**; 128-GRF control drift is −0.168%/+0.031%.
This is frozen generic M16 Short28, without the new PLE prefetch; the gains
cannot be credited to scheduling or API removal alone. That experiment used an
isolated image; matched 256-GRF Direct still trails L0.

The [later current-U4 adoption checks](../bench/results/a770/orca-hc1-q8-grf-u4-adoption-2026-11-05.md)
pass all 18 mixed-history/actual HTTP requests and two final-path smoke requests;
the Windows A770 primary L0 config selected permanent HC256 images at that
stage. The EXE, native fallback, profiles and RAM guard were retained.
The [matched U4/M16 context experiment](../bench/results/a770/orca-hc1-production-contexts-2026-11-05.md)
then passes all 120 complete protocols. HC128→HC256 improves Direct Short net
decode **45.494→47.119 tok/s (+3.571%)** and TTFT
**766.087→754.491 ms (−1.514%)** together; L0 Short improves **+2.479% / −1.485%**.
With HC256 on both backends, Direct Code2K/8K TTFT is **11.039%/7.462% shorter**
than L0 while net decode remains **2.211%/0.627% lower**; Short remains
**2.524% lower**. Shorter TTFT does not establish that prefill optimizations
caused the decode gap. Long cases have one retained sample per process;
all samples and control drift are archived. The targeted GRF flag is now a
source build default. This context experiment did not replace deployed EXEs
or configs; shared kernel gains cannot be attributed entirely to API removal.

The [post-HC256 endpoint replay](../bench/results/a770/orca-hc1-post256-span-2026-11-05.md)
passes 720 complete output/guard/span checks and 72 CPU references. T1/input0
fused GPU spans are now **4.917%/5.424% shorter** on Direct in opposed orders;
ordinary HC remains **4.699%/4.303% longer**. The old fused-chain +19% attribution
therefore does not apply to this HC256 replay. The ordinary HC and submission
boundaries need mapping to real-model decode; hot independent buffers and
endpoint spans do not isolate the entire residual. Linux GPU validation and
the intermittent Direct 8K fault remain open.

The [ordinary-HC register screen](../bench/results/a770/orca-hc-plain-grf-2026-11-05.md)
changes only SG16 ordinary up from 128 to 256 GRF, retaining fused HC256.
All 720 complete output/guard/span checks and 72 CPU references pass. Direct T1
ordinary-chain wall throughput improves **1.81–5.23%**, while T4 loses
**7.55–8.71%** and L0 regresses. The preset joint gate fails, so this policy
is not adopted. Both images have zero spills and the same 1,256-byte private
allocation per SIMT thread; more registers do not ensure a faster chain.
These component results do not replace model decode or TTFT measurements.

The [narrow-capture model check](../bench/results/a770/orca-hc-up-capture-2026-11-05.md)
passes 108 complete protocols but changes L0/Direct Short net decode by
−0.879%/−1.208%; TTFT also lengthens, so the candidate remains isolated.
The [Q8 split/subgroup follow-up](../bench/results/a770/orca-hc-q8-sg16-2026-11-05.md)
retains those failures and separately records SG16 bitwise checks, the failed
joint component gate, and the matched full model. Shared kernel gains do not
establish gains from API removal; those isolated experiments did not change production defaults.

The subsequent [local HC SG16 adoption](A770_PERFORMANCE.md#hc-sg16-default)
now selects the complete matching narrow-capture EXE/images tuple on next start.
Opposed HC256/SG16 brackets match all 42 protocols and improve Code2K/Prose2K
net decode throughput by **5.60–5.61%/5.97–6.15%**. Permanent-path first/repeated
8K, mixed histories and actual HTTP checks match all 18 generation requests;
169 one-second candidate samples have a minimum **1336.047 MiB** free VRAM.
The initial control-side memory failure remains archived. Only the primary
EXE/images fields change; HC256 and native fallbacks, U4N16 data, profiles and
the RAM guard remain available. This deployed tuple uses the frozen U4 Host
and generic libraries from the isolated model experiment; the current-tree
build measurements below retain their own executable and pack provenance.

The later source adoption selects narrow captures together with SG16 fused Q8,
preserving all eight token slots and the fused kernel's 256-GRF option. The
complete current-tree Windows L0 and Direct builds now each pass PLE CPU CTest
and all 64 HC eager/replay output/source/guard checks against CPU FP64 and separate-Q8
references, using the retained matching HC pack for that build-time check.
The portable `strata-orca-hc-check` target is documented in the
[frontend README](../sycl/orca/README.md); the Linux frontend checker builds and
audits it without GPU execution. Static PE audits find the expected Level Zero
loader on L0 and no Level Zero/SYCL/UR/oneMKL direct imports on Direct; dynamic
or transitive DLL dependencies are outside this check. Matched current-tree
L0/Direct/L0 testing now passes all 60 full protocols and the 3% control-drift
gate, using one fresh 1,254-ZEBIN pack with 24 unsupported SPIR-V variants.
Warm Short is **51.228 L0 / 49.480 Direct tok/s** (−3.411% Direct); repeated
Code2K/8K decode gaps narrow to **−1.298%/−1.181%**, with TTFT
**16.496%/7.772% shorter**. Cold first Code2K TTFT is **4.971% longer** instead.
Each long-request position has one sample per arm, and the old Direct 8K fault
remains open. This comparison does not deploy executables or change the launch
configuration; earlier isolated speed results remain separate measurements.
The [frozen-build report](../bench/results/a770/orca-current-tree-2026-11-05.md)
and its JSON/text archive are now published and pass the source, protocol,
comparison and archive checks, subject to the asset-identity limitation below.

A CPU audit of all 60 recorded protocols also compares delivery after the
first token, excluding that token from the throughput numerator. Direct
post-first delivery throughput is **1.354% lower** for repeated Code2K,
**1.218% lower** for repeated 8K, and **3.436% lower** for retained Short.
These agree with the engine decode gaps, so the observed gap is not explained
solely by moving the first-token timing boundary. The timestamps are from
the Host protocol reader, not GPU or HTTP timestamps; this audit adds no new
GPU samples. Repeated Code2K nevertheless completes in **19.183 s L0 /
16.652 s Direct (13.194% less time)**. TTFT and engine decode overlap at
the first window; whole-request time is measured directly, not their sum.
The root cause of the remaining backend gap is still unconfirmed; the earlier
same-backend HC256 Short comparison already improves both TTFT and decode.

These are matched transport comparisons, not evidence of a necessary TTFT/decode
tradeoff. Prefill's larger batched matrix work and decode's small dependent
launches have different costs; improving one does not remove the other's
bottleneck. Cache carryover or concurrent prefetch could couple them, but this
cohort does not isolate either cause. Preparing known data after submission and
waiting at consumption remains the scheduling rule: preserve buffer ownership,
avoid waiting to fill a tail batch, and check both TTFT and net decode. Moving
initialization before READY must also retain startup-to-first-delivery timing;
shorter request TTFT alone is insufficient evidence of less total work.

The current Short comparison is also a counterexample to a simple tradeoff:
Direct TTFT is **0.370% longer**, while net decode throughput is **3.411% lower**.
Its decode loss therefore does not require a TTFT improvement. The native
M16 selector in `sycl/orca/prepare.py` already restricts wide FP16 projection
tiles to aligned **T >= 128**; decode T1..8 keeps the small-shape path. Keep
phase-specific shapes and dispatch policies rather than delaying ready decode
work to form prefill-sized batches. This source boundary does not identify
the remaining backend cause; the matched HC128-to-HC256 result above is the
measured example of improving both metrics on the same backend.

On 2026-11-06 the native DG2 source removes a remaining temporary parameter-
descriptor allocation: `compiler::append` uses the compiler-known capture
count for a stack `std::array`, passing a borrowed pointer/count into the
shared Windows/Linux append implementation. Initializer-list callers also
avoid constructing a descriptor vector. The existing out-of-line vector
entry points remain for older Host objects. Values are serialized before
append returns; this adds no TLS buffer, shared argument storage, dependency
or C++ standard upgrade to the C++17 runtime. Other recording allocations
remain; recorded-graph replay largely bypasses this path, so this change
does not explain or claim to fix the warm Short gap.

The Windows source build passes the extended CPU argument checks: scalar and
captured-pointer payload equality, dynamic SLM alignment, zero arguments,
null/nonzero lists and wrong-count rejection. The fresh compiler check passes
12 GPU pipeline replays over eight frozen lists with changed data, aggregate
pointer/scalar captures, local memory, 3D/SYCL/ESIMD/range kernels and guards.
Rebuilding the Strata `fused_gr` Host TU and linking the new runtime alongside
the other frozen Host objects passes all 64 portable HC checks with the original
math pack and bitwise original outputs. This covers new stack captures plus
the retained vector ABI; it is not a fully rebuilt model measurement.
A fresh Linux/WSL GCC build passes all four CPU checks and dependency audit;
neither the Linux compiler frontend nor Linux GPU executes in this cohort.
Exact pre-edit source copies extend the existing resolver without changing
historical receipts. No deployed EXE, launch profile, model policy or numerical
image changes were made by these initial checks; they did not establish a
TTFT/decode performance result.
Local evidence: `build-orca-argument-array-20261106/{windows-r1,strata-hc-r1,linux-r1}`.
The normal native CPU check remains runnable after rebuilding:
`orca-l0-dg2-check --self-check` on Windows or
`orca-l0-drm-compute-check --self-check` on Linux.

The subsequent [full Host/model comparison](../bench/results/a770/orca-direct-argument-array-2026-11-06.md)
rebuilds all 49 kernel Host units and the native runtime, retaining the original
engine core, numerical pack, assets and policy. All 64 HC checks and **100/100
complete model protocols** pass in L0 → old Direct → new Direct → old Direct →
L0 order. Each process runs Code2K twice, 8K twice, Prose2K, then Short ×15;
Short discards ten warmups and retains five. Actual expert slots remain 5771,
VRAM reserve 1536 MiB, MTP4/min-p0.5 and U4/min16/M16. The largest absolute
L0/Direct control drifts are 1.946%/1.263%, within the preset 3% limit.

| Retained case | L0 / new Direct decode tok/s | Direct decode change | Direct TTFT time change | Direct whole-request time change |
| --- | ---: | ---: | ---: | ---: |
| Repeated Code2K | 36.950 / 36.110 | −2.272% | −13.419% | −10.441% |
| Repeated 8K | 34.331 / 33.751 | −1.690% | −6.566% | −5.742% |
| Short | 52.789 / 51.226 | −2.961% | +0.522% | +2.332% |

All changes in this table compare new Direct with L0, not production SYCL.
Against old Direct, Short decode changes only **+0.472%**, failing the preset
1% gain gate; descriptor allocation removal does not establish a stable model
speedup. Quality and same-backend nonregression gates pass, while the gate for
decode exceeding L0 in every case fails. Longer-context decode gaps narrow,
but workloads/draft trajectories differ and capped long requests retain the
existing backend tail-work distinction. Post-first delivery gaps agree with
decode, so a first-token timing-boundary shift does not explain the result.
Each long position runs once per arm; control drift is not a confidence interval.

Old/new Direct READY is 30.214/30.722 s and first Code2K TTFT is 26.709/26.261 s.
Their sequential sum is 56.922/56.983 s (+0.106%); startup through the first
complete request changes +0.069%. These exclude controller gaps and establish
no cold-start gain or causal initialization shift. Whole-request time is
measured directly; TTFT and overlapping engine decode are not added. The
published JSON/text archive passes protocol, build/source and integrity checks;
asset identity uses the reported stat-cached SHA256 checks. Deployment is
unchanged; the intermittent Direct 8K fault and Linux GPU remain open.

The subsequent [cached-dependency trial](../bench/results/a770/orca-direct-completed-dependencies-2026-11-06.md)
isolates two native runtime filters, one per platform, without changing Host
objects, core, math, ABI or state layout. Only cross-queue points already
confirmed by owner/immutable-observer monotonic caches can bypass the driver
wait; pending points and Host gates remain. In the standalone BCS demo, the
old/new binaries each pass 36 complete snapshots over 12 waves: cached-prefix
wait calls fall from **24 to zero**, while all 12 pending-tail waits remain.
Observer/owner caches, duplicate/invalid dependencies and full guards are
checked. Another 920 transfer replays and 64 HC checks pass; the isolated
Linux/WSL build passes four CPU checks and dependency audit, without GPU use.

The fixed L0/old/candidate/old/L0 model cohort matches **100/100 protocols**.
Short changes **50.832→51.009 tok/s (+0.350%)**, below the preset 1% gain gate;
max absolute L0/Direct control drift is **4.331%/1.670%**, failing the 3% gate.
The candidate also retains longer first-2K/first-8K samples; nonregression fails.
No stable model speedup or causal regression is established, and the candidate
is **not adopted** into production source or deployment. The observed repeated
8K backend gap of −0.279% comes with failed control drift and does not supersede
the accepted argument-array cohort's −1.690%. Demo call counts are not model
counts. Compiler flags match; initial compiler-mode/checker-capacity failures
and the localized-log archive failure are retained. The report and raw text
archive retain the failed performance gates; Linux GPU and the old 8K fault
remain open.

The [current U4/HC whole-window diagnostic](../bench/results/a770/orca-direct-current-window-2026-11-06.md)
replaces the old generic-M16 diagnostic inputs with the frozen current pack
and accepted array-argument Direct Host/runtime. Only one Verifier TU per
backend is rebuilt; actual dependencies cover 60/62 repository headers,
including 11 generated headers per backend derived from frozen originals.
The first diagnostic attempt dereferenced its queue before initialization
and exited before READY; its source/logs remain. Moving the diagnostic init
after queue creation and adding a CPU order assertion resolves that probe bug.

Both opposed orders match all **90 request protocols and 2,610 windows**,
including the same position/actual-T schedule, guards/raw ticks and Host sums.
Each natural 70-token Short request has 29 windows, not the old pack's 20.
Direct GPU window spans take **2.513%/2.613%** longer than L0; Host
submit-to-return takes **2.874%/2.866%** longer. Primary-metric outer drift
stays below 0.15%, passing the preset 3% gate. Host input preparation is only
about 1.21/0.69 ms per request; its own drift exceeds 3%, so no stable gain is
claimed for this small component. Preparation alone cannot explain the
remaining difference. GPU spans include execution, memory/dependency gaps
and marker ordering; Host return includes diagnostic readback. Do not subtract
clock domains to infer wake latency, or call these clean tok/s measurements.
Current-pack window phases and outside-window MTP/commit remain to be split;
the old M16 HC/router ranking is not current attribution. No deployment,
Linux GPU, PPL or 8K reliability claim is added.

The [Host-only reanalysis of those same logs](../bench/results/a770/orca-direct-current-host-split-2026-11-06.md)
uses arithmetic retained-request/outer-control means so stage differences
remain additive. In the two orders, Direct's extra 48.400/47.180 ms per
diagnostic request consists of 33.640/32.654 ms in the Verifier-call region,
11.687/11.397 ms in the draft region and 2.987/2.987 ms in commit/emit;
the remaining Host difference is 0.086/0.142 ms. This rechecks all 90 raw
protocols and log frames without GPU work. Printed stage precision bounds
each reconstructed phase to +/-0.145 ms per request; DONE precision is
+/-0.05 ms. Draft includes PLE/history preparation and waiting, and commit
includes adaptive-thread startup/output. These Host regions include GPU
work and overlap on the GPU, so they are neither exclusive kernel time nor
pure CPU costs. They identify both regions for investigation without changing
the original diagnostic or performance gates.

The [current-pack HC1-to-route/device-plan diagnostic](../bench/results/a770/orca-direct-current-route-phase-2026-11-06.md)
matches another 90 complete protocols and 2,610 windows in opposed orders.
The 48-layer region takes **217.801/217.722 ms** on Direct versus
**204.840/204.861 ms** on L0, or **6.327%/6.278%** longer; phase-control
drift is +0.045%/-0.113%. Whole-window GPU spans are 3.484%/2.974% longer,
with primary GPU/Host control gates passing. This region includes HC1,
router projection, Top-K, quantization and device-plan dependencies. Adding
96 ordered markers per window changes scheduling, so it neither isolates HC1
nor supplies clean throughput or an additive contribution to the parent run.
No runtime policy is adopted. The initial publication import failure is
retained; an explicit-path publisher and independent check preserve the
frozen measurements without another GPU run.

The [matched GRF128 HC1/router split](../bench/results/a770/orca-direct-hc-route-split-2026-11-06.md)
then holds the fused-Q8 GRF128 pack constant on both backends. All 90 complete
protocols and 2,610 windows match in opposed orders. Direct's HC1 interval takes
**10.706%/9.639%** longer, versus **2.891%/2.279%** for router/device-plan;
HC1 T4 takes **11.727%/10.834%** longer, compared with **7.623%/6.958%** for T1.
Primary whole-window/Host outer drift stays below 0.65%, passing the preset 3%
gate; aggregate HC1 and router phase controls also stay below 0.5%. The T1
subgroup can exceed 0.5%. This narrows the next
investigation to HC1 memory/code generation rather than Host input preparation.
It does not isolate a particular HC1 kernel or prove private-memory policy is
the cause. Three ordered marks per layer introduce 146 boundaries per window;
the intervals include dependency gaps and marker ordering. Neither these
percentages nor differences from the earlier 98-marker probe are clean model
speedups or additive causal contributions. No production change follows from
this diagnostic; the default remains Direct128/L0256, and the old 8K fault and
Linux GPU validation remain open.

The [isolated getter128 build/output check](../bench/results/a770/orca-hc-q8-getter128-2026-11-06.md)
combines the previously audited six Q8-only row selections with 128 GRF.
Declared private memory falls 9,728→0 bytes per SIMD16 hardware thread, with
zero spill; ELF text instead grows 27,392→27,456 bytes. The 256 rebake is byte
identical to the measured getter256 image, and getter128/256 payloads match.
Both frozen backends pass 256 complete generic128/getter128 output/CPU/source/
guard eager/replay cases with identical 64 full-output hashes. This covers
production entries 2/3, not an additional image 9 run. Earlier getter256 failed
gates remain retained. Compiler resources and operator checks alone do not
establish model speed.

The [clean Direct Short model trial](../bench/results/a770/orca-hc-q8-getter128-model-2026-11-06.md)
then matches all 90 complete protocols, using the same EXE and 128 GRF on both
arms with profiling disabled. In the stable first bracket, getter128 changes
net decode by **-0.364%**, TTFT by **-0.105%**, and whole-request time by
**+0.303%**. The reverse bracket reports -3.152% decode, but its getter control
drifts -4.289% in decode and +5.970% in TTFT; it does not establish a regression
of that size. The original drift, gain and time gates remain failed. Even with
the user's acceptance of small positive gains, this candidate has not shown
one, so it stays isolated without a source/default/deployment change or rescue
repeats. The adopted Direct GRF128 policy above remains in place. No new long
context, PPL, Linux GPU or 8K reliability claim follows from this Short trial.

The [current-U4/HC compact-GRF follow-up](../bench/results/a770/orca-direct-current-grf-u4-2026-11-06.md)
tests the earlier invariant-packet candidate with the accepted array Host and
current math pack. Both capture censuses contain 14,879 appended kernels and
1,202 GRF-only transitions; each eligible append loses exactly three packets /
40 bytes, with the complete normalized kernel/state/length histogram matching.
This is capture-side accounting, not replay-weighted per-token work.
All four census and 150 clean Short protocols match. Both opposed clean
brackets pass control drift, but net decode changes only **-0.007%/-0.053%**,
failing the preset +1% gate; the candidate remains isolated and default-off.
Each process runs only Short requests; absolute rates must not be compared
with the earlier all-Host cohort's long-context-then-Short request history.
The two policies pass 28 native CTests and 128 HC checks in total; the same
Linux/WSL source passes five CPU checks and four dependency audits, without
Linux GPU execution. Removing constant packets retains actual GRF mode changes
and their synchronization, so it does not rule out optimizing those transitions.
The initial queued cancellation and audit-build compiler-flag failure remain
archived. No new long-context, PPL or reliability claim is added.

The [adjacent-barrier follow-up](../bench/results/a770/orca-direct-grf-barrier-2026-11-06.md)
instead keeps the GRF state programming and removes only a second identical
24-byte PIPE_CONTROL at a tracked command boundary. Both capture censuses
contain 1,097 eligible pairs among 14,879 appended kernels; this is not
replay-weighted per-token accounting. All four census and 150 clean Short
protocols match, alongside 28 native CTests, 128 HC checks and five Linux/WSL
CPU checks with four dependency audits. Opposed-order decode changes are
**-0.050%/+0.160%**, with control drift and TTFT gates passing but the preset
two-order 1% decode-gain gate failing. The candidate stays isolated and
default-off. The first build's CPU packet-expectation failure is retained;
the corrected build changes that assertion, with byte-identical runtime code.
No long-context, PPL, Linux GPU or new reliability claim is added.

The [current SG16 HC Q8 register comparison](../bench/results/a770/orca-hc-sg16-grf-2026-11-06.md)
removes the large-register option from only the fused image. Both 128/256-GRF
builds report SIMD16, zero spill and 9,728 private bytes per hardware thread;
the payload ABI is identical, but machine code also changes, so occupancy alone
is not isolated. All 256 full HC checks and 180 complete Short protocols match.
Opposed-order Direct decode improves **+1.142%/+0.743%**, with TTFT
**-0.237%/-0.385%** and measured request time **-0.803%/-0.686%**;
L0 decode changes **+0.070%/+0.059%**. Both backends pass quality, control-drift
and time-nonregression gates, but fail the preset two-order 1% decode-gain gate.
The original two-order 1% gate remains failed. Under the user's later acceptance
of small positive gains, the [source-default adoption](../bench/results/a770/orca-hc-sg16-grf-adoption-2026-11-06.md)
now omits this override for Direct (128 GRF on the measured A770); L0 retains
256 GRF. Both generated build recipes and source equivalence to the measured
kernel pass. Rebuild/rebake is required; deployed executables and configs are
unchanged. No additional repeats were used to obtain a pass. These observations do not
establish a necessary TTFT/decode tradeoff or close the Direct backend gap.
No new PPL, Linux GPU or reliability result is added.

The [subsequent five-arm context check](../bench/results/a770/orca-hc-sg16-grf-contexts-2026-11-06.md)
matches all 100 complete protocols with the same Direct EXE and frozen 5771-slot
policy. Direct128 versus256 net decode changes **+1.320% Short, +3.010% repeated2K,
+2.440% repeated8K**; TTFT changes **-1.126%/-2.368%/-2.032%** respectively.
The original 3% control-drift gate fails: repeated8K L0 decode drift is +5.145%,
and Direct Short decode drift is -3.211%. These are supporting observations,
not a stable speedup claim or a replacement for the original opposed-order
Short evidence. Direct128 still trails L0 Short by 1.359%; its observed
repeated2K/8K advantages are 1.591%/0.719%, with single long positions per arm.
All-case superiority is not established. No rescue repeats or deployment
changes were made; the earlier Direct8K fault remains open.

The [fused HC Q8 preferred-SLM trial](../bench/results/a770/orca-direct-hc-slm-2026-11-06.md)
isolates the remaining target allocation-preference difference from L0. Both
settings pass 28 native CTests, 72 complete packet comparisons and 128 full HC
checks in total; real captures retain every non-target policy and command count.
All 90 clean Short protocols match. Opposed-order net decode changes are
**+0.057%/+0.065%**, against **+0.257%/+0.587%** control drift; TTFT changes
**+0.314%/+0.026%**, and whole-request time **+0.042%/-0.034%**. The original
gain gate remains failed. This establishes no model-speed benefit and does not
explain the backend gap. The candidate stays isolated and default-off; the
user-authorized Direct128/L0256 source defaults remain. No deployment or Linux
GPU validation follows from this trial.

The [fused HC Q8 indirect-length trial](../bench/results/a770/orca-direct-hc-indirect-2026-11-06.md)
then changes just the target walker length **832→800 bytes** to match L0,
preserving its 832-byte backing allocation, parameter bytes/padding, SBA
window, SLM and mathematical images. Both settings pass 28 native CTests,
144 complete normalized CPU packet comparisons and 128 full HC cases in total;
actual captures verify 160 target walkers with other decoded fields/counts
unchanged. All 90 clean Short protocols match. Opposed-order net decode changes
are **-3.427%/+0.007%**, with **-0.050%/-0.357%** decode control drift; TTFT
changes **+2.835%/+0.866%** and whole-request time **+3.427%/+0.358%**. The
original drift gate passes, but the gain and request-nonregression gates fail.
The results establish neither a consistent benefit nor a stable 3.427% loss;
the candidate remains isolated and default-off. Existing component-capture
logs also put both backends' compute submissions on node5 (Compute), without
fresh hardware queries or a full-model engine-utilization measurement. No
deployment, new long-context or Linux GPU result follows.

Scope clarification for both field trials: their selectors use kernel
metadata/shape, not a Verifier stage identity. `gr_read_group` can pass
`q8_mixed` in either half, and `fused_gr_read_multi` selects the fused Q8 entry
when those inputs are present. Matching HC0 and HC1 calls can therefore both
be affected; the HC checker's historical `HC1.*` labels do not establish an
HC1-only ablation. The frozen SLM report and its archive remain unchanged.
This does not change the separate HC1/router interval comparison, whose
markers use actual Verifier stage boundaries.

The [actual-HC1 internal diagnostic](../bench/results/a770/orca-direct-hc-internal-2026-11-06.md)
then uses the existing optional math-Host stamps after norm and down, enabled
only for Verifier `half == 1`. Both backends retain the identical GRF128 pack,
normal independent projections and shared/routed scheduling, with the legacy
per-layer profiler disabled. Only Verifier is rebuilt; 242 ordered marks per
window and a 1,952-byte readback perturb scheduling. All **90 complete protocols
and 2,610 windows** match, with raw ticks partitioned into norm/down/up/router,
48 layers, 36 GDN/12 QSA, valid guards, ordering, wrap and identical actual-T
sequences. Whole-window/Host-wall control drift stays below **0.20%**.

| Opposed order | Direct norm ms/request | L0 norm ms/request | Direct norm change | Down change | Up change | Whole HC1 change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Direct→L0→Direct | 33.2342 | 17.0291 | +95.162% | -2.586% | +5.842% | +13.785% |
| L0→Direct→L0 | 33.3434 | 16.9858 | +96.302% | -2.866% | +5.011% | +13.660% |

The marked norm interval is the next investigation target, not proof of a
95% pure-kernel regression. Existing [empty-marker calibration](../bench/results/a770/orca-direct-timestamp-cost-2026-11-05.md)
shows unequal timestamp/barrier costs; those cannot be subtracted as a fixed
correction under model load. A no-intermediate-marker norm replay is needed
before changing runtime policies. Separate medians are not additive, and
different marker densities cannot be subtracted for precise attribution.
Defaults/deployment remain unchanged; no new clean throughput, long-context,
PPL or Linux GPU evidence, and the historical Direct8K fault remains open.

The [independent norm replay](../bench/results/a770/orca-direct-hc-norm-replay-2026-11-06.md)
uses that same baked norm entry and generated **392-byte capture ABI**, without
recompiling mathematical code. Each list has 128/512 consecutive norm launches
and only two endpoints. T1/4/8, residual off/on and two inputs cover 72 CPU
reference/eager/source/guard cases and **1,440 complete replay checks**, with
1,008 retained samples and matching hashes across all six processes. Both
backend orders and every case show Direct GPU time **13.029%–16.669% lower**,
with maximum absolute GPU control drift **0.181%**.

For residual-on T4/input0/512 reads, Direct/L0 GPU time is
**3.882904/4.488322 µs** forward (**-13.489%**) and
**3.882650/4.486694 µs** reverse (**-13.463%**). Corresponding Host replay/wait
time changes **-9.130%/-10.401%**; short 128-read Host samples are noisier and
do not establish a general Host advantage. Static PE imports contain no
SYCL/UR/OpenCL; Direct has no ze_loader import, while L0 retains it. This is
not an enumeration of dynamically loaded modules.

The single-weight/single-buffer hot replay therefore does not reproduce the
marked model's norm slowdown. It moves the next investigation toward layer
working sets, surrounding state transitions/dependencies and probe boundaries;
it does not establish which factor causes the model gap, or a new end-to-end
gain. All named generated descriptors remain exact; the unused anonymous fill
specialization is excluded for plain Host compilation. Four build/audit
failures occurred before GPU execution and retain their original sources,
commands and logs. Source defaults remain Direct128/L0256; deployed executables
and configs are unchanged, with no new long-context, PPL or Linux GPU claim.

The [independent buffer-bank follow-up](../bench/results/a770/orca-direct-hc-norm-banks-2026-11-06.md)
then rotates **1/48/192** distinct data/parameter-address sets, keeping the same
native norm zebin, 392-byte ABI, local256, frozen runtime and two endpoints.
Residual-on T1/4/8, two inputs and 192/768 launches cover **108 full CPU/eager/
source/guard cases, 2,160 replay checks and 1,512 retained samples**. All six
processes produce matching complete output hashes; actual compiled headers and
static PE imports also pass independent publication checks.

For T4/input0/768 launches, one bank gives Direct/L0 **3.912593/4.614190 µs**
forward and **3.878581/4.513386 µs** reverse, time changes
**-15.205%/-14.065%**. With 48 banks and an **18.756-MiB logical active
footprint**, these become **6.712188/7.245144 µs** and
**6.791585/7.076637 µs**, or **-7.356%/-4.028%**. These four controls stay
within the earlier 3% drift reference. Both backends slow as the independent
working set grows, while Direct's advantage shrinks. This does not reproduce
the marked model's approximately doubled norm interval, and logical bytes are
not measured physical traffic, cache misses or bandwidth.

All **72 GPU comparisons** remain in the evidence: Direct time changes span
**-19.876% to +3.096%**, maximum absolute control drift is **11.970%**, and
**23/72** exceed that earlier 3% reference. Some cases change direction; the
T4/input0/192-bank/768 forward control itself drifts **-4.108%** and cannot
establish a stable paired estimate. This reference is diagnostic, not a newly
passed performance gate; no repeat is added to obtain a pass. Independent bank
rotation cannot exclude the model's real cache history. Surrounding kernel
state/dependency changes and probe boundaries remain the next isolation.
Source defaults remain Direct128/L0256; deployed executables/configs are
unchanged. There is no new full-model, PPL or Linux GPU measurement, and the
historical Direct8K intermittent failure stays open.

The [ordered-marker density control](../bench/results/a770/orca-direct-hc-norm-density-2026-11-06.md)
then keeps that same native norm, ABI, data and 1/48-bank layouts while changing
only the ordered timestamp period: **0** for two endpoints, **8** for one mark
per eight norms, and **1** for one per norm. With 768 launches these produce
2/97/769 marks. All modes use a fixed 771-slot timestamp buffer; every inactive
slot remains a guard. Each raw marker is retained and independently checked for
count, valid range, order, wrap and interval sum. **72 CPU/eager/source/guard
cases, 4,320 full replay checks and 3,024 retained samples** pass. Complete
reference hashes also match every corresponding shape in the bank trial.

For T4/one-bank/input0/768, the opposed-order GPU results are:

| Ordered marker period | Direct/L0 µs per norm, forward | Direct/L0, reverse | Direct time change, forward/reverse |
| --- | ---: | ---: | ---: |
| Endpoints only | 3.877496/4.482286 | 3.878038/4.483235 | -13.493%/-13.499% |
| Every 8 norms | 4.485835/4.595269 | 4.468926/4.600352 | -2.381%/-2.857% |
| Every norm | 8.881304/5.522597 | 8.825005/5.469885 | +60.818%/+61.338% |

All six listed controls stay within **2.906%** drift. The same mathematical
work can therefore change backend ranking when ordered instrumentation changes.
This establishes substantial probe disturbance in this component, not an exact
explanation of the earlier model's +95% interval. Every-8 marks also alter some
larger-bank cases substantially; subtracting an empty-marker constant or simply
assuming sparse probes are harmless would be unjustified.

All **144 GPU comparisons** are retained: Direct time changes span
**-14.707% to +66.986%**, maximum control drift is **8.259%**, and **10/144**
exceed the earlier 3% diagnostic reference. This is not a new passed performance
gate. The next model comparison should use no inner markers and coarse window
endpoints, then isolate surrounding state/dependencies at matched probe density.
No runtime/default/deployment change or new model-throughput, PPL, long-request
or Linux GPU claim follows; the historical Direct8K failure stays open.

The [fixed-T HC follow-up](../bench/results/a770/orca-hc-fixed-t-2026-11-06.md)
specializes T1/T4 inside the same SG16/256-GRF kernel, preserving the deployed
capture ABI, other native images, dispatch count and model policy. Both the initial
by-value and corrected const-reference helpers pass 256 full HC checks each.
The first doubles declared private space to 19,456 bytes and is retained without
model sampling; the correction reports 9,856 bytes versus baseline 9,728, zero
spill, but grows text from 27,840 to 90,880 bytes. Exact IGA disassembly is archived;
static instruction counts include all branches/padding and do not measure execution.
The const-reference frontend also changes ordinary-up images 2/9; these are not
deployed. The first publication audit incorrectly required only one frontend
change and failed before writing results; its source/failure remain archived.
All 180 fixed Short protocols match. Opposed-order decode changes are
**+0.032%/-0.413% Direct** and **-0.887%/-0.441% L0**; control-drift and time
nonregression gates pass, but neither backend passes the preset 1% gain gate.
The candidate stays isolated with defaults unchanged and no added repeats or
long-context run. This rejects this implementation, not shape-specific tuning
in general, and does not establish a TTFT/decode tradeoff.

The [separate T1/T4 entry follow-up](../bench/results/a770/orca-hc-split-t-2026-11-06.md)
reduces capture sizes from 616 to 112/328 bytes and declared private space from
9,728 to 1,792/5,248 bytes per hardware thread. T1 text shrinks to 24,768 bytes,
while T4 grows to 42,240; both retain SIMD16/256GRF/zero spill. All original 25
Host parameter descriptions and deployed native images remain identical. Four
frontend modules differ only in eight numeric OpName type suffixes, with all
other instructions and IDs identical; the initial byte-check failure is retained.
The matching Host builds pass 256 full HC checks and 180 complete Short protocols.
Opposed-order decode changes are **-0.025%/-0.469% Direct** and
**-2.275%/+0.061% L0**. These fail the original gain gate and establish no
consistent positive gain under the later user-authorized small-gain policy.
The candidate remains isolated. Resource savings alone do not prove a speedup;
no additional repetitions, long-context runs or source-default changes follow.

The [current-pack CPU-activity isolation](../bench/results/a770/orca-direct-cpu-activity-2026-11-06.md)
retains the accepted array Direct EXE, event waits, numerical pack and model
settings. After READY, a bounded helper either blocks or runs on logical CPU43
at BELOW_NORMAL priority. Two fixed opposed five-arm orders match all 150
Short protocols. Decode changes **+0.632%/+0.377%**, TTFT **-0.228%/-0.886%**;
control drift passes, while the preset two-window 1% decode-gain gate fails.
The helper adds **2.122--2.141 CPU seconds per complete retained request**,
sampled separately from engine CPU; this includes prefill and decode, not energy.
Engine affinity, Windows power settings and production defaults remain unchanged.
There is no C-state/uncore measurement or causal claim, and no adopted busy helper.
This Short-only result does not close the Direct decode gap or its 8K issue,
and adds no long-context, PPL or Linux GPU verification.

Fresh Windows sampling uses new directories and the same frozen assets,
compiler, pack, policy and request history. This command is documented, not
executed to obtain a passing result; existing output directories are rejected:

```powershell
python -X utf8 -c "import sys; sys.path.insert(0,'build-orca-current-grf-20261106'); import run as n; n.STAGE=n.OUT/'build-repro-r1'; sys.path.insert(0,str(n.OUT)); import model as m; m.BUILT=n.STAGE; m.STAGE=n.OUT/'model-repro-r1'; m.PLAN=n.OUT/'model-prepared-repro-r1.json'; n.run(); m.prepare(); m.run()"
```

The generated Host frontend also applies its remaining-output MTP draft cap
only outside `STRATA_ORCA_DG2`. Both arms retain min-p 0.5/cumulative zero and
match full protocols, but that alone does not establish identical discarded
tail work on length-capped requests. Warm Short ends naturally at 70 of the
128-token limit, so the cap stays at the full three drafts and cannot explain
its backend gap. Aligning Direct's tail cap is a separate prepared Host
candidate. Its isolated Host TU now compiles and links against the retained
Direct libraries. A source check permits exactly the two guard removals,
retains the first KV-maintenance round and leaves thresholds unchanged.
The baseline/candidate/baseline cohort now completes all 60 normal requests
plus 15 Short requests at output budgets 1/2/3/4/8, with complete matching
output and draft protocols. Repeated Code2K/8K net decode throughput changes
**+0.931%/+0.343%**; Short changes **+0.142%** and never activates a smaller cap.
All cases meet the preset 3% timing-nonregression limit, but cold-first Code2K
TTFT control drift reaches **4.426%**, failing the 3% whole-cohort drift gate.
The candidate remains isolated and is not adopted. This finite bracket does
not establish a stable speed improvement or close the original Direct 8K fault.
The single opposite-order candidate/baseline/candidate cohort also completes
all 75 service protocols and four CLI checks at budgets 3 and 8, covering the
second patched call site. Those cold CLI processes are correctness checks,
not performance samples. Repeated Code2K/8K decode changes are
**-0.512%/+0.136%**, and Short changes **-0.797%**. Quality and timing
nonregression pass, but first-Code2K TTFT control drift is **+5.068%**, so
the unchanged 3% drift gate fails again. Both failed cohorts are retained;
the candidate remains isolated, with no further retry to pass. The runner's
raw-protocol, CLI, arithmetic, source and asset checks pass. Local evidence:
`build-orca-hc-up-capture-20261105/native-tail-budget-r1/model-reverse-r1/results.json`.

Later independent GGUF-cache edits changed two source files after this
cohort was built. Exact original versions have been recovered against the
recorded digests, preserving the retained EXEs and newer workspace sources.
Subsequent startup-mirror work also changes `sycl/src/program/generate.cpp`;
its exact original has likewise been recovered from the retained pre-edit
copy (SHA-256 `fa7efbb299fb20411d9d798677f4ba5b0b0f0b9eb6bb45603869a599e3f2c18a`).
The publication and first tail-candidate build reject that drift before
publication or compilation; both failures are retained. The source resolver
now records that original as well, without restoring it over the new code.
The original cohort did not snapshot model/MTP/U4 assets or external Direct
`memory.zebin`; its source/EXE/math-pack hashes must not be called a complete
model-asset freeze. A later 18,816-file asset snapshot is complete for new
cohorts and does not retrospectively prove the old files' contents.

A separate isolated U4-preload Host candidate completes all 60 L0 off/on/off
protocols, including an independent raw-token/DONE and startup-sum audit.
First Code2K TTFT changes 26.750 to 15.506 s, while READY changes 26.810 to
38.209 s; startup-to-first-token is 53.560/53.715 s (+0.289%, control drift
-2.558%). This establishes preparation moving earlier, not less total waiting.
Direct then matches 20 off requests and two on Code2K requests, but rejects
the first on 8K prefill with `U4 expert nonfinite activation`, exit 1. All 82
completed matching protocols and the failure are retained; the 120-request
acceptance is incomplete and preloading has not been adopted. This experiment
uses the original current math pack. The cause is not established.

A subsequent bounded Direct diagnostic retains preload=1 and adds flag
reporting only after the existing nonfinite rejection. Its fresh
Code2K→Code2K→8K process matches all three protocols and exits normally,
so no failing input/hidden flags are captured. This is diagnostic collection,
not a stability or performance pass; the original rejection and adoption
failure remain open.

A single subsequent `gdn-all` collection reuses the same post-rejection
diagnostic EXE with preload enabled, adding the existing GDN stage scans.
Code2K→Code2K→8K again matches all three full protocols and exits zero;
the recorded 2047/2047/7999-row reports contain zero bad residual halves,
no GDN nonfinite-stage reports and no U4 rejection flags. A CPU reparse
checks the raw tokens/DONE fields against references, configured mode,
all three reports and retained log hashes. Evidence:
`build-orca-hc-up-capture-20261105/preload-nonfinite-r1/model-gdn-all-r1`.
The scans change scheduling: their times are not performance results, and
this non-reproduction neither identifies the earlier fault nor enables
startup preloading. The failed preload acceptance remains retained.

A later isolated U4 bank-history diagnostic now passes serial SYCL, L0 and
Direct execution through **2047→2047→7999 input tokens**, changing input and
row IDs while retaining the same cache. It uses 32 real experts from layers
0/15/31/47, two eight-slot banks, and 12 batches per history with one to eight
experts, varied row counts and padding. Native runs use production copy-ahead
order; the serial SYCL reference waits after every batch. Both native backends
bitwise match all **94,372,224 final GU/Down float elements per backend** across
the three histories, including output gaps/guards, with input/ID and packed
guards checked. The Direct log confirms `ORCA_COPY_ENGINE enabled=1`; physical
engine overlap is not measured. All eight compile/link/GPU commands exit zero.
The runner's source/artifact check passes, as does an independent CPU audit
of the recorded counts, copy-engine log, source/log hashes and output scope.
Asset checks use the explicitly labelled cached-SHA identity mode.

This is a subset diagnostic, not a full-model or VRAM-pressure reproduction.
Output rows overlap between batches: the full arrays verify each history's
final state, not every overwritten intermediate result. The original Direct
8K failures remain open, and no production source or default changes.
Local evidence and CPU recheck:
`build-orca-hc-up-capture-20261105/u4-bank-history-r1/run-r1/results.json` and
`python -X utf8 build-orca-hc-up-capture-20261105/u4_bank_history_decision.py --check`.

The follow-up disjoint-output check closes that intermediate-output coverage
gap without changing the two-bank weight lifecycle or copy-ahead order.
All 61 expert operations per history retain separate GU/Down rows (6,946 active
rows within capacity 8,192); the three histories again pass on serial SYCL,
L0 and Direct. Each native backend bitwise matches 94,372,224 final float
elements, now retaining every batch's expert results. The same input/ID,
gap, guard, packed-buffer and nonfinite checks pass; Direct's copy-engine
log is confirmed. All eight commands and the runner check pass, followed by
the independent CPU audit with `u4_bank_history_decision.py --disjoint --check`.
Evidence is in `build-orca-hc-up-capture-20261105/u4-bank-disjoint-r1`.
This still uses only 32 experts and does not reproduce full-model pressure,
prove physical overlap, establish a speedup or close the original 8K fault.

A separate literal-index HC Q8 candidate removes its compiler-reported
9,728-byte private allocation per SIMD16 thread (608 bytes per SIMT thread),
retaining 256 GRF and zero spills. Explicit Host descriptors remain unchanged;
the deleted implicit private-base payload is audited separately. Current-pack
baseline/candidate checks on both backends pass all 256 complete HC output,
CPU-reference, input and guard checks, with bitwise matching outputs. The first
strict-payload-equality failure is retained. It is not adopted, and this is
not a model speed or total-VRAM reduction claim.
The next timing attempt compiles both probes and completes its three L0
arms, then Direct rejects missing diagnostic clock calibration before GPU
work. That failed attempt is retained. A new complete six-arm bracket reuses
those EXEs with the existing 19.2-MHz/36-bit Direct calibration; its samples
are kept separate from the partial L0 bracket. All six new probe processes
complete, but the preset joint performance screen fails. For example,
L0 T1/fused-Q8/input1 wall time grows **1629.300→1707.300 µs** (4.787% longer),
with control drift 0.000%; its GPU span grows **1553.854→1622.656 µs**
(4.428% longer), with drift -0.047%. Direct's same wall time is 0.177% longer,
while its two T4 fused wall times are 0.623%/1.033% shorter.
Some other controls drift beyond 3%; these are finite component observations,
not steady-state or model throughput. Removing private memory alone does not
establish a performance improvement. The 120-request full-model candidate
cohort is not started after this failed screen; the prepared plan is retained.

A separate retained by-value row getter is now checked against the fresh
current pack. Its old native metadata exactly matches the current baseline,
and its explicit compiler ABI matches except debug file/line information.
The new image has zero private memory and spills, while its ELF text is
**26,624 versus 27,840 baseline bytes**, rather than the literal candidate's
34,624 bytes. Both backends pass **256 current-pack portable HC checks**;
a new six-arm bracket also passes **720 complete output/guard/span checks
and 72 CPU references**, reusing the fixed probes and clock calibration.
Direct fused replay throughput changes **+2.289% to +6.980% at T1** and
**+2.312% to +6.800% at T4** across the two inputs. These are component wall
rates, not model tok/s. The joint component screen still fails: some control
drifts exceed 3%, some other cases regress, and one L0 T1 fused wall gain is
only 0.497%. The separate current-model cohort now completes all **120/120**
protocols with the original model gates unchanged and that component failure
retained. Both asset checks pass in the explicitly labelled stat-cached SHA256
mode; no new full asset rehash is claimed. Its runner's complete artifact check
and an independent CPU raw-protocol/timing/gate audit pass.

| Same-backend Short bracket | Baseline / getter decode tok/s | Throughput change | Decode control drift | Baseline / getter TTFT ms | TTFT change | TTFT control drift |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| L0 | 52.244 / 52.854 | +1.169% | +1.156% | 720.405 / 722.722 | +0.322% | -1.391% |
| Direct | 50.989 / 51.702 | +1.400% | -0.226% | 724.832 / 724.752 | -0.011% | +1.101% |

Repeated Code2K/8K decode throughput changes **-0.199%/+0.340%** on L0 and
**+0.524%/+1.516%** on Direct. These are finite same-backend comparisons, not
a new matched cross-backend speedup. The quality, time-nonregression,
Short-gain and Short-TTFT gates pass, but the original whole-cohort drift gate
fails: L0 first-Code2K TTFT control drift is **-28.352%**, and Direct is
**+3.205%**, both beyond 3%. The apparent first-load TTFT reduction must not be
credited to the getter. Component and model failures remain recorded; the
candidate stays isolated and no production source/image is adopted. The local
decision receipt is `build-orca-hc-up-capture-20261105/current-getter-model-r1/decision-r1.json`;
`current_getter_decision.py --check` independently checks its 120 raw protocols
and decision arithmetic without a GPU run or full asset rehash.

The system reboot
at 21:05 interrupted its first zero-sample lock wait and the reverse-tail
waiter. Both original result files are preserved byte-for-byte in the local
`build-orca-hc-up-capture-20261105/reboot-recovery-r1` directory; neither had
started a GPU arm. Fresh processes completed the same prepared plans.
The recovery audit separately records a failed PowerShell lock reservation;
recovery only moved metadata directories, and the GPU runners acquire their
own shared lock.

A scoped-source follow-up applies the same by-value getter at six Q8-only
access sites. The isolated model pack changes only fused-Q8 image 3, whereas
the original broad template edit could also change ordinary HC images.
The new transformation preserves the ordinary branches and its inverse
restores every original source byte; repeated transformation is rejected.
The corrected isolated compile now checks all 25 native images: image 3's
loadable sections, relocations, compatibility note and native metadata match
the measured getter; the other 24 match the baseline. Explicit compiler Host
ABI matches except debug filename/line information. Its independent retained
compile/ABI/section/hash check passes. This adds no GPU or model-speed samples;
the original failed model/component gates remain and the source is not adopted.
The first isolated compile (`current-getter-scoped-r1/build-r1`) fails before
GPU execution because its runner omits the existing native `transform`
step; the original runner, compiler errors and failure audit are retained.
The runner also had an unexecuted nine-image assertion; the actual source
index contains 25 images. The successful fresh `build-r2` uses the native
transformation, requires its original-source output to equal the measured
generated source, and compares all 25 images' executable sections/relocations
and native metadata. Its target Q8 reference is the measured getter image;
the other images must match the baseline. Compilation/pack loading uses the
shared experiment lock. Recheck the retained result with
`python -X utf8 build-orca-hc-up-capture-20261105/current_getter_scoped_build.py check`.

A separate ordinary-HC follow-up reuses the retained broad getter SPIR-V for
images 2 and 9, preserving baseline fused-Q8 image 3. Both ordinary images keep
SIMD16/128 GRFs and zero spills/SLM, while reported private memory falls from
**9,728 to 0 bytes per SIMD16 hardware thread** (608 bytes per SIMT thread).
Their ELF text falls **40,000 to 16,384 bytes**. Exact native metadata comparison
allows only four compiler-owned private-memory removals; six mutated-metadata
checks are rejected. The original bake's field-parser rejection is retained:
its regex swallowed root-level metadata after the private-memory block vanished.
The independent audit passes without rebaking or changing that failed result.

The added full upstream self-check then fails on the **unchanged L0 baseline**
with `ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY` at kernel append, before any
candidate execution. This failure is retained in `ordinary-getter-check-r1`;
it does not establish a candidate regression or an out-of-memory root cause.
The subsequent production-entry check reuses the existing portable binaries:
**256 complete output/CPU/source/guard cases** pass on baseline/candidate and
both backends. Its six-arm baseline/candidate/baseline endpoint bracket passes
**720 complete replay checks and 72 CPU references**, with the fixed probes and
19.2-MHz/36-bit Direct calibration. These production-entry checks execute image
2 and unchanged fused-Q8 image 3; **image 9 remains GPU-unvalidated**.

| Ordinary HC chain component wall throughput | T1 change | T4 change |
| --- | ---: | ---: |
| L0, two inputs and pending-write off/on | -5.682% to +6.602% | +6.288% to +9.681% |
| Direct, same cases | +2.514% to +6.030% | +7.341% to +11.618% |

All ordinary-chain endpoint GPU rates improve in this bracket, but the preset
joint wall-time screen **fails**: L0 T1/no-write/input1 regresses 5.682%, and
unchanged fused-Q8 controls also have wall regressions beyond 1%. Control drift
stays within 3%; it is not a confidence interval. These finite component gains
are not model tok/s or TTFT gains, and do not identify the remaining transport
gap. No production source/image is adopted and no full-model cohort is run.
Local evidence and independent checks:
`python -X utf8 build-orca-hc-up-capture-20261105/ordinary_getter_audit.py --check`
and `python -X utf8 build-orca-hc-up-capture-20261105/ordinary_getter_followup.py check`.

An isolated Host-dispatch follow-up now covers ordinary **image 9** too. It
keeps the production norm/down path and its initialized partial workspace,
dispatching only the final ordinary up through the existing `launch_multi`
entry. Both backends and baseline/candidate pass another **256 complete
output/CPU/source/guard eager-and-replay cases**, T1..8 with pending writes
off/on and changed inputs. Packs that omit image 9 fail at that exact kernel
lookup on both backends before any HC output, proving which entry was requested.
The mathematical images are reused; this is a test-only Host dispatch, not
validation of the complete upstream norm/down/split/staged pipeline or a fix
for its retained baseline memory error. The original production-entry report
still correctly limits its own coverage to images 2/3. Recheck locally with
`ordinary_entry9_check.py check` and `ordinary_entry9_dispatch.py check`, both
under `build-orca-hc-up-capture-20261105`.
The ordinary-only source candidate changes four access sites and preserves
all six Q8 sites and eight token slots; its inverse restores the original
source bytes. `ordinary_getter_scoped.py check` verifies the prepared edit,
but its scoped compilation and image-equivalence acceptance remain pending;
the production source is not changed by that preparation.

The first ordinary-only full-model cohort now completes **120/120 exact
protocols**, all six processes exiting zero. It uses the same current EXE per
backend, off/on/off images 2/9, unchanged fused-Q8 image 3, actual 5,771 slots,
1,536-MiB reserve, U4/min16/M16, MTP4/min-p0.5 and the same
2K→2K→8K→8K→Prose2K→15 Short history. Short drops ten warmups and retains five;
each long position is sampled once per arm. Model timing has no inner probes
or GPU timestamps. Both before/after asset checks pass in the explicitly
labelled stat-cached SHA256 mode, without rehashing the 89.8-GiB assets.

| Ordinary-only model bracket | Baseline / candidate Short tok/s | Decode change | Decode control drift | Short TTFT change | TTFT control drift |
| --- | ---: | ---: | ---: | ---: | ---: |
| L0 | 51.942 / 52.545 | +1.161% | +2.890% | -1.446% | -3.371% |
| Direct | 50.955 / 50.865 | -0.178% | +0.080% | -0.711% | -0.681% |

Repeated Code2K/8K net decode changes **-3.132%/+1.470% on L0** and
**-0.247%/+0.278% on Direct**. L0 repeated-8K decode control drift is +3.728%,
so that position cannot establish a steady gain. L0 first/repeated-2K TTFT is
5.592%/5.733% longer and first-8K decode throughput is 3.894% lower in this
finite cohort. The original quality and Short-TTFT gates pass; whole-cohort
control drift, all-case nonregression and both-backend minimum-Short-gain
gates **fail**. The failed component screen remains recorded too. Less private
memory and faster component GPU execution do not establish a model speedup;
no source/default/image is adopted and no repeat is added to obtain a pass.
This also does not isolate the remaining transport gap or close the intermittent
8K fault. The completed runner verifies all raw protocols, timing arithmetic
and frozen dependencies. Local results and recheck:
`build-orca-hc-up-capture-20261105/ordinary-getter-model-r1/model-r1/results.json`
and `python -X utf8 build-orca-hc-up-capture-20261105/ordinary_getter_model.py check`.

The independent `ordinary_getter_decision.py` audit now also passes, including
its separate `--check`: all 120 raw protocols, selection, original gates and
post-first-token delivery arithmetic are reconstructed without new GPU work.
For Direct, repeated Code2K/8K post-first delivery changes **-0.243%/+0.272%**
and Short **-0.149%** with the ordinary-only getter. These finite differences
do not establish a stable win. The original failed gates are retained in
`ordinary-getter-model-r1/decision-r1.json`.

The first **combined ordinary-plus-Q8 getter** check completes on 2026-11-06.
The ten-site source candidate retains all eight token slots; inverse edits
restore the original bytes. Applying the native preparation transform exactly
matches the previously retained source/25 SPIR-V images and compiler Host ABI
apart from debug location. Rebaking all 25 images and comparing executable
sections, relocations and native metadata changes only images **2/3/9**;
each equals its separately audited ordinary/Q8 candidate. No new compilation
or production source adoption is implied.

Using one isolated complete pack, the original production checker and the
image-9 Host entry pass **512 complete output/CPU/source/guard eager-and-replay
cases** across T1..8, changed inputs, pending writes off/on, both backends and
baseline/candidate. The reused six-arm endpoint probes additionally pass
**720 complete replay checks and 72 CPU references**. The original upstream
norm/down memory failure is retained; these production-workspace checks do
not validate that full alternative pipeline or fix the intermittent 8K fault.

| Combined component wall throughput change | T1 | T4 |
| --- | ---: | ---: |
| L0 ordinary HC | +5.330% to +12.699% | +4.901% to +8.267% |
| L0 fused Q8 HC | +3.907% to +5.097% | +3.889% to +5.509% |
| Direct ordinary HC | +3.399% to +7.031% | +8.422% to +15.151% |
| Direct fused Q8 HC | +3.415% to +4.117% | +6.479% to +7.317% |

All 12 actual timing cases per backend exceed the preset 1% component gain
threshold, but the joint screen **fails control drift**: three L0 T1 cases
exceed 3%, reaching **13.140%** absolute drift. Direct's maximum absolute drift
is **2.749%**. These are component brackets, not model decode or TTFT results;
the L0 values with excessive drift do not establish stable gains. No combined
model cohort, retry to obtain a pass, default change or performance adoption
follows this failed joint screen.

The historical combined runner mistakenly names timing mode 3 in its minimum-
gain loop: modes 0..3 belong to functional tests, while timing has only 0..2.
Its earlier failed drift predicate short-circuits that loop, yielding False.
The original receipt and source remain unchanged. A separate audit reparses
all raw outputs, CPU checks and timing rows, verifies exactly 12 timing keys
per backend, and evaluates all corrected gates; the joint result remains
**False**. It retains the separate ordinary-only and Q8-only failed model gates.
Local evidence is `hc-all-getters-r1/bake-r1/results.json` and
`hc-all-getters-r1/check-r1/{results,audit-r1}.json` under
`build-orca-hc-up-capture-20261105`. Recheck under the shared experiment lock:
`python -X utf8 build-orca-hc-up-capture-20261105/hc_all_getters_audit.py --check`.

The [generic FP16 M16 projection comparison](../bench/results/a770/orca-generic-projection-m16-2026-11-04.md)
extends register reuse beyond paired experts. Each backend uses one EXE and
math pack, switching M16 off/on/off; **102 requests match** complete output,
draft and finish state. Direct 2K/8K TTFT falls from **23.676/67.963 s to
15.754/38.362 s** (**33.46%/43.55% less time**); L0 falls from
**27.071/71.359 s to 19.917/43.575 s** (**26.43%/38.94% less time**).
With the same M16 math, Direct TTFT is **20.90%/11.96% shorter than L0**.
Forward/opposed standalone projections also improve on both transports by
about 68–70%, while Direct still takes 1.94–3.95% longer than L0 with M16.
Short decode remains **58.30 Direct / 59.37 L0 tok/s**; no stable Short gain
is established. Long cases run once per process, so this is a finite same-day
window. These shared math gains are not backend-only gains. The source
defaults `STRATA_ORCA_XMX_WIDE` to 1 for aligned FP16 M≥128, retaining M4
tails/fallbacks and BF16. Rebuild the native backend and its XMX image pack
together. Fresh default builds pass 168 edge checks, 240 canonical FP16/BF16
shapes, eight changed-input replays and 1,680 pair checks across both backends.
Prepared fresh-process Direct/L0/Direct 2K→2K→8K histories also match all nine
requests, with no zero diagnostic triggered; finite retries do not close the fault.
Deployed EXEs/configs are unchanged. Linux GPU and the intermittent
Direct 8K fault remain open.

The [full GEMM API long-history check](../bench/results/a770/orca-long-gemm-api-2026-11-05.md)
adds 2047→2047→7999 rows and reverse retained-list replay with changed inputs
and weights. All **96 Direct/L0 M4/M16 cases** pass, checking **867,309,960
active outputs**, complete sources and caller-buffer guards, including FP16,
BF16, Q8 dequantization and beta=0/1. This is correctness evidence, without
timing; workspace rebinding, cache-slot loans and real-model multi-queue state
remain outside its scope. It does not close the intermittent 8K fault.
**Frozen-report errata:** “逐位数值相等” means exact elementwise numerical
equality, not signed-zero bit identity. Preparation preceded the experiment
lock; compilation, GPU work, heavy hashes and publication held it. The original
report/archive are preserved and their independent publication check passes.

The [subsequent workspace-rebinding check](../bench/results/a770/orca-rebind-gemm-2026-11-05.md)
adds three caller arenas, pending auto-batched requests, rebind completion
queries and reverse retained-list replay while the current binding points
elsewhere. All **120 Direct/L0 M4/M16 cases** pass, checking **873,501,576
active outputs**, complete sources/guards and every element of inactive
scratch/workspace arenas. Across four processes, **240 rebind queries** and
**32 Host merged-request counts** (including recording) match expectations.
This checks ownership under changed workspace addresses; it adds no timing
claim and does not execute real ExpertCache loans or residency handoff.
The intermittent model 8K fault remains open.

The [stable-mirror expert refill screen](../bench/results/a770/orca-mirror-refill-2026-11-05.md)
then checks real cache-slot loans. The migrated `fill_slot_queued` waits after
every copy; the isolated service candidate reuses asynchronous `fill_slot`
only for persistent GGUF USM mirrors, preserving transient-source waits,
the blocking override, 256-copy drains and final residency publication.
Six diagnostic requests byte-check **8,016 slots / 11,442,329,600 bytes**
and six complete **24,576-entry** device residency tables. Another 18
off/on/off requests without readback instrumentation match outputs/drafts/finish
state; all 24 protocols pass. Direct 8K TTFT changes **−1.788%**, with
off-control drift +0.444%; L0 changes **−0.316%**, with drift +0.037%.
The two 2K positions do not consistently improve. Each position runs once
per arm, and enabled timing arms retain one refill-count log per request.
That initial candidate stayed isolated/default-off; deployed EXEs/configs and ordinary
generate/elastic-refill paths are unchanged. Refill belongs to prefill/TTFT,
not decode. These are frozen generic-M16 comparisons, not production U4
results; actual copy/preparation overlap, unmirrored or multi-device GPU
execution and Linux GPU remain unmeasured. Passed retries do not close the
original intermittent 8K fault.

The [persistent-mirror service branch](../bench/results/a770/orca-mirror-refill-source-2026-11-05.md)
was subsequently integrated into
`sycl/src/program/generate.cpp`, initially using `STRATA_ORCA_MIRROR_REFILL=1`
after rebuilding, with unset/0 retaining the original behavior. Freshly generated
complete Host TUs for Direct/L0 each pass default and opt-in 2K→2K→8K→Short
histories: **16 protocols match** with no refill audit/count logs. This is
source integration, not a new performance result or a complete production-U4
rebuild. Deployed EXEs/configs and CLI generate are unchanged.
L0 source now enables it by default under the [later adoption](../bench/results/a770/orca-preparation-defaults-2026-11-06.md);
explicit 0 restores the previous path, and Direct remains opt-in. The existing
Intel `ExpertCache::open_segmented` explicitly rejects elastic VRAM, so that
refill branch is currently unreachable. The [pipeline document](ORCA_PARALLEL_PIPELINE.md)
also identifies the 1,024-callback ring, 512-blob source ring, default
16-buffer Stager and 64-copy descriptor array. Fully mirrored experts bypass
the two Host expert-data rings; GPU routed staging still uses eight slots
and FP16 dequantization two slots. The DG2 owner uses an MPSC linked list
rather than a fixed-capacity ring. These source boundaries do not measure
GPU occupancy or slot-reuse waits.
Its independent publication check now passes. The first check was mistakenly
queued before publication and acquired the lock first, so it failed on the
missing public JSON; a subsequent check after publication verifies every
recorded file and hash. The model/source verification itself passed on both
attempts; the failed command is preserved in the local experiment notes.

The [copy-descriptor census](../bench/results/a770/orca-copy-capacity-census-2026-11-05.md)
then matches all eight requests across two 2K→2K→8K→Short histories on Windows A770. Both
default and asynchronous mirror refill issue **54,068 Host copy calls**
with identical byte arguments. Default peaks at **32/64**, with **zero**
capacity flushes; asynchronous refill reaches **64/64**, with **62** capacity
flushes (40 at the 8K boundary). Counters cover Host calls before DONE,
not physical transfers or recorded GPU replays. All eight boundaries balance;
cleanup is excluded because the service uses `_Exit(0)`. The report preserves
both diagnostic failures and the independent audit. No timing benefit or GPU
staging-depth conclusion follows; this uses frozen generic M16, not production U4.
The original publication check subsequently fails because concurrent U4
adoption updates the primary launcher JSON. A separate
[archive audit](../sycl/orca/copy-capacity-census-a770-windows-2026-11-05-audit.json)
and independent recheck pass all **1,003 files**, protocols and boundaries,
recording that launcher-only difference. Frozen argv/env do not read that
JSON; the original publication and failed check remain preserved.

The [routed prefetch-depth screen](../bench/results/a770/orca-routed-ring-depth-2026-11-05.md)
then compares **8→16→8** while reserving 16 GPU slots in both arms, keeping
routed experts and borrowed-slot layout fixed. All **12 protocols** and
per-request FP16-pair work counts match. Depth16 TTFT changes by
**−0.490%/+0.670% for the two 2K positions, +0.505% for 8K**, and −0.806%
for Short. One bracketed window establishes no stable benefit; the default
remains eight slots. This isolates prefetch depth, not the full production
allocation change, GPU occupancy or slot-reuse waiting. Frozen M16 dependencies
are used; no production-U4, L0 or Short net-decode gain follows. The 42-file
publication check passes; Linux GPU and the intermittent 8K fault remain open.

The [Host descriptor-depth comparison](../bench/results/a770/orca-copy-depth-2026-11-05.md)
then runs **64→256→64**, reserving 256 descriptors in both arms while retaining
64-operation GPU batches and the same frozen M16 math/runtime. Asynchronous
mirror refill is enabled throughout. All 74 TUs, eight GPU checker runs and
12 complete model protocols pass independent auditing; a separate checker
asserts all 521 copy counts, full-capacity flushes and tails for both limits.
Relative to the two 64 controls, 256-item TTFT changes by **+1.780%/+0.450%**
for the two 2K positions, **+0.352%** for 8K and **+0.082%** for Short.
No speedup is established; the production default stays 64. This finite window
does not measure model high-water marks, per-flush blocking, production U4,
L0 or steady Short throughput. The original final verifier exits 1 after an
incorrect nested-list comparison; its source and error remain immutable.
Independent auditing validates raw DONE fields/tokens, work counts, all 90
successful commands and hashes without rerunning the GPU. The 1,028-file
publication check passes. A [fresh-attempt runner snapshot](../sycl/orca/copy-depth-reproduction-a770-windows-2026-11-05.json)
corrects only that assertion, with syntax and three guard checks; it makes no
new GPU-run claim. See the pipeline notes for the corrected rerun commands.

The [original-binary zero-output reprobe](../bench/results/a770/orca-legacy-zero-reprobe-2026-11-05.md)
runs two fresh historical M4 Direct processes, each directly 2K→2K→8K:
all **six complete protocols match**. The original EXE, initial expert profile
and all 1,516 math-directory hashes match the retained failure evidence; the
memory module also matches the earlier history fingerprint. Only the existing
post-zero diagnostic is enabled, and no diagnostic GPU readback is triggered.
Its enabled Host condition may still perturb timing. The original 64-zero-token,
0/0-draft failure remains archived and unresolved; these retries establish
neither a fix, failure rate, ring-capacity cause nor a performance result.
Preparation-only r1 and executed r2 are retained separately; the 54-file
publication recheck passes. Production files
are unchanged by this reprobe; Linux GPU and the Short backend gap remain open.

The [bounded-wait and CPU attribution screen](../bench/results/a770/orca-direct-hybrid-wait-2026-11-04.md)
keeps current Host/math fixed and changes one WDDM runtime file. Two opposed
Short windows match all **210 requests**. Neither 8-us nor 32-us spin before
the existing event wait establishes a throughput gain; the candidate is not
adopted. Direct event decode remains **2.13–4.40% below L0** in these windows.
Whole-request process CPU, excluding startup/warmup but including prefill and
decode, is **45.29/46.77 ms Direct versus 2175.00/2259.38 ms L0**, about
**97.92% less CPU time** for Direct in both windows. This is not an energy
measurement. A separate import observer matches **30 requests/60 phases**;
**97.92% of retained L0 process CPU** falls inside observed
`zeCommandListHostSynchronize` calls. The diagnostic uses the original default
runtime libraries, not the spin candidate; its durations are not speed results.
Vendor-internal spinning, syscalls and locks are not separately attributed.
Defaults, deployed EXEs and the Linux wait path remain unchanged.

The [L1 WBP/WB Short screen](../bench/results/a770/orca-direct-l1-short-2026-11-04.md)
independently rebuilds the isolated cache-policy runtime and retains current
Host/math. Both policies pass 28 named CTests, 240 shapes and 1,680 pair checks
in total; two opposed model windows match all **150 requests**. WB changes
decode throughput by **+0.34%/+0.14%**, below WBP control-drift magnitudes
**1.03%/0.62%**. No stable gain is established; WBP remains the default.
Default Direct is still **1.83%/2.09% below L0** in those windows. This tests a
global SBA field, not every cache or dispatch difference. CPU accounting has
large control drift and does not justify choosing WB. An initial CTest replay
rewrote two generated bookkeeping files in an older cached build, invalidating
its full live-hash check; original source/library/sample bytes are unchanged.
The report preserves that incident and uses only the subsequent independent
build for its model results. No new Linux GPU or long-context result follows.

The [Short GPU phase diagnostic](../bench/results/a770/orca-direct-short-phases-2026-11-05.md)
retains normal shared/routed interleaving and independent projections rather
than using legacy profiling predicates that disable them. Direct/L0/Direct
matches all **45 requests**, including exact per-window T schedules. Existing
Host staging logs put window input preparation near **1.20/0.80 ms per request**
for Direct/L0. GDN/QSA hc1-router-plan spans are **4.09%/4.43% longer** on
Direct, totaling **9.00 ms per request** of additional marked time; these are
split by the finer diagnostic below. The full marked-span difference is **+1.13%**, with
Direct control drift **-1.35%**, so it is not a stable overall speed conclusion.
Markers add barriers and per-window readback/logging; spans include dependency
gaps and exclude inter-window draft/commit/Host preparation. They are not
exclusive kernel times or clean throughput. Both initial diagnostic failures
(output-path replacement and integer-versus-floating clock validation) are
preserved; the report uses only the corrected independent build/run. Defaults
and deployed EXEs remain unchanged; Linux GPU and intermittent Direct 8K stay open.

The [finer HC1/router/plan diagnostic](../bench/results/a770/orca-direct-short-router-phases-2026-11-05.md)
matches another **45 requests**, retaining normal scheduling and exact T
sequences with 386 timestamps per window. HC1 and router-plus-quantization
contribute **14.99/10.49 ms per request** of additional marked time. Increasing
marker density changes the observed differences; full-span difference is
**+4.06%** with Direct control drift **-4.99%**. These are localization hints,
not established exclusive-kernel or model-speed differences. A separate
[empty timestamp calibration](../bench/results/a770/orca-direct-timestamp-cost-2026-11-05.md)
passes **120 raw/guard/replay checks** across four marker counts. At 512 dense
markers, median GPU time per ordered interval is about **1.145 us Direct /
0.552 us L0**. This quantifies instrumentation cost in an empty context; it
cannot be subtracted from actual model phases or explain the entire gap.
The subsequent replays below remove per-operator markers. No default or
deployment changes follow from these diagnostics.

The [ready-input router replay](../bench/results/a770/orca-direct-router-replay-2026-11-05.md)
uses the original runtimes and the same math pack, with 48 operation groups per
list, T=1/4 and two changed inputs. Direct/L0/Direct and L0/Direct/L0 match all
**720 full-output/guard checks**, retaining **504 wall samples** and 72 matching
output checksums. In T=4 projection-plus-quantization independent groups,
Direct takes **5.59–8.78% more list wall time** across the two orders. Upload,
recording, poison, readback and validation are outside timing; submission,
completion notification and wakeup remain inside. Repeated same-weight hot
cache, fixed mode order and the T=1 interface-only fork limit extrapolation.
These are not exclusive kernel timings or a full-model result.

The [ready-input HC1 replay](../bench/results/a770/orca-direct-hc1-replay-2026-11-05.md)
matches **360 full-output/guard checks**, with FP64 CPU reference maximum
absolute error **7.62e-7** and bitwise fused-Q8/native-quantizer agreement.
T=1 pending-write-plus-Q8 takes **19.18–21.67% more list wall time** on Direct;
T=4 takes **1.56–2.37% less**. The other modes differ by about 0.91–3.23%.
This single Direct/L0/Direct bracket has no GPU timestamps. Inputs and R_out
are separate to make all 48 reads stable, unlike the model's usual in-place
writeback. The shape-dependent difference persists with ready inputs but still
includes submission/completion/wakeup; it does not attribute all model loss to
HC1 or establish a runtime fix. Linux GPU and intermittent Direct 8K remain open.

The [FFN Q8 reuse pilot](../bench/results/a770/orca-ffn-q8-reuse-2026-11-05.md)
then reuses HC1's completed Q8 input and runs only the existing BF16 conversion
at two preparation sites. A single-group readiness guard retains all original
fallbacks; each backend rebuilds one Host TU, with no math/runtime change.
Direct off/on/off and L0 off/on/off match all **90 exact-protocol requests**.
Direct decode changes **58.457→57.971 tok/s (-0.831%)**, with zero observed
control drift; L0 changes **59.566→59.838 (+0.457%)**, with +0.157% drift.
No consistent benefit across transports is established, so this remains an
isolated default-off candidate. It preserves kernel count and is not a new
preparation thread or production change. Long-context/PPL/Linux results are
not available for this candidate; input readiness alone does not remove the
remaining device/submission/completion differences.

The [two-endpoint router diagnostic](../bench/results/a770/orca-direct-router-span-2026-11-05.md)
adds only a first/last GPU timestamp to each recorded list. All **360 output,
guard and raw-tick checks** pass, with 252 retained wall/GPU pairs. T=4
projection-plus-quantization independent groups take **643.411/642.187 us GPU**
on Direct versus **658.646/658.854 us** on L0, or **2.31/2.53% less device span**.
The same lists take **798.170/793.245 us wall** versus **741.500/759.500 us**,
or **7.64/4.44% more wall time**. Direct GPU control drift is +0.219/+0.276%;
wall drift is +1.744/+0.721%. This prioritizes submission/completion/wakeup
diagnosis for this probe. It does not establish full-model speed: the two
markers add ordering, device spans include dependency gaps, weights repeat
in hot cache, mode order is fixed and this is one Direct/L0/Direct bracket.
The failed r1 compile is preserved; r2 and all four
new report archives pass independent publication checks.

The [opposed endpoint and Host submit/wait diagnostic](../bench/results/a770/orca-direct-submit-wait-2026-11-05.md)
extends this to **1,440 output/guard/raw-tick checks**, including 720 exact
per-sample submit-plus-wait wall checks. The original EXEs in L0/Direct/L0
again show T=4 Direct GPU spans **2.34–2.46% shorter** but wall time
**4.73–10.38% longer**. Adding one Host clock reading, both backend orders
show T=4 Direct submission at **31.4–46.1 us versus 21.2–24.2 us L0**;
the final wait differs too. Wait includes outstanding GPU work and cannot be
called pure wakeup latency. GPU spans include dependencies and marker ordering;
there is no absolute CPU/GPU clock correlation for subtracting those domains.
Ready inputs and recorded lists exclude late preparation from this local
comparison, motivating separate submission-handoff and completion-path trials.
The archive explicitly audits a concurrent launcher-JSON promotion while
requiring all historical executable/runtime/math dependencies unchanged;
saved argv/env bypass that launcher. No model/default/Linux GPU claim follows.

The [caller-consumed owner pilot](../bench/results/a770/orca-direct-caller-owner-2026-11-05.md)
then removes the dedicated worker handoff while retaining MPSC publication,
FIFO batches, serialized native mutations and nested TLS ownership. Windows
and Linux/WSL each pass **98,304 CPU concurrency checks**. The extra Linux
ASan/UBSan link fails because its runtimes are missing; that failure is retained
and is not a sanitizer pass. Rebuilding all 69 affected Host TUs and five
other header users avoids mixed layouts: **74 compilations**, three GPU
checkers and 120 guarded router/tick/Host checks pass. The unchanged native
library/math pack is then used for worker/caller/worker and L0/caller/L0.
All **90 full model protocols match**. Caller decode is **58.669 versus
58.583 tok/s** for worker controls (**+0.146%**, control drift **-0.599%**),
and **58.597 versus 59.871 tok/s L0 (-2.128%)** in the second bracket.
Hot sequential CPU calls fall from 1,368/1,090 ns controls to 59 ns, but do not
establish a model gain. The candidate remains isolated; no default, deployed
EXE or production source is changed. These use the earlier frozen pack and
saved policy, not the concurrently promoted production U4 configuration.
Long-context/PPL/Linux GPU and the intermittent Direct 8K fault remain open.

The [low-density real-model window diagnostic](../bench/results/a770/orca-direct-window-span-2026-11-05.md)
records only two ordered GPU endpoints per Verifier window plus a 32-byte
readback published by the existing final wait. Legacy per-layer profiling is
disabled internally, preserving normal independent projections and expert
interleaving. Direct/L0/Direct matches **45 requests and 900 windows**, with
300 retained windows, identical T schedules, full guards/raw ticks and exact
per-window Host sums. Median per-request GPU-window totals are **1132.582 ms
Direct versus 1120.277 ms L0 (+1.098%)**; Host submit-to-return totals are
**1140.052 versus 1123.091 ms (+1.510%)**. Direct GPU/wall control drift is
**+0.203%/+0.419%**. Host submission totals **1.812/0.916 ms**, while input
staging totals **1.181/0.748 ms**. Device execution/dependency spans also differ;
the gap is not confined to final notification. These marked intervals include
ordering and diagnostic readback, exclude commit/MTP/inter-window work, and
establish no clean throughput or pure wakeup latency. No absolute CPU/GPU clock
correlation or cross-domain subtraction is used. The earlier frozen pack and
policy remain fixed; production defaults and deployed binaries are unchanged.

The [current-v14 GRF state screen](../bench/results/a770/orca-direct-current-grf-2026-11-05.md)
rechecks invariant CFE/preemption/pipeline packets with the later ISA and scratch
reuse already present. One Direct EXE switches the isolated process-cached policy;
28 named CTests, 240 numerical shapes, 1,680 pair checks and **150 complete model
protocols** pass. Opposed Short windows change decode **+0.139%/+0.204%**;
the second full-policy control drifts **−0.490%**. Compact Direct still trails its
respective L0 reference by **2.075%/2.466%**. No default change is justified;
the candidate stays isolated. Runtime source matches current v14, while Host/math
remain frozen generic M16 rather than production U4. The pre-model path-hashing
failure and corrected runner are preserved; no new long-context, PPL or Linux GPU
result follows, and the intermittent Direct 8K fault remains open.

The [MTP independent Q8 projection screen](../bench/results/a770/orca-mtp-ready-projections-2026-11-05.md)
groups ready-input Q/K/V operations, joining before normalization, KV append
and quantization-scratch reuse. KV-only paths still omit Q. One MTP Host TU
per backend changes, retaining math/runtime/header layout and min-p 0.5.
Two opposed backend/flag orders match **180 complete model protocols**.
Direct decode changes **+0.273%/-0.223%**, with two-sided policy drift
**+0.177%/+0.293%**; L0 changes **+0.280%/+0.268%**. Direct does not establish
a stable gain or a smaller backend gap, so the candidate remains isolated
and disabled by default. This checks the model trajectories, not every
intermediate tensor. Common graphs are already cached before READY;
in-flight mapped metadata must not be overwritten while preparing later
steps. These trials retain the older frozen M16 pack and saved policy,
not the concurrently promoted production U4 pack. No production source,
deployed binary, Linux GPU or long-context result changes.

The [MTP norm/RoPE fusion screen](../bench/results/a770/orca-mtp-norm-rope-2026-11-05.md)
reuses the existing fused kernel: K stays in place and Q reads interleaved
Q/gate directly, removing its split copy. Both backends pass **192 complete
bitwise output/source/guard cases** with changed-input graph replay across
HD128/256, four row counts, three analytic RoPE scalings and two layouts.
Two opposed model orders match **180 complete protocols**, retaining min-p
0.5 and the older frozen M16 pack. Direct decode changes **+0.443%/+0.127%**,
against two-sided policy drift **-0.421%/+0.501%**; L0 changes
**+0.252%/-0.083%**. No stable model gain or smaller backend gap is established.
The isolated candidate stays default-off; production MTP source and deployed
EXEs are unchanged. The focused probe does not cover table lookup, multimodal
MRoPE or every model intermediate. Linux GPU and long-context checks remain open.

The [MTP single-dispatch greedy/probability pilot](../bench/results/a770/orca-mtp-fused-head-2026-11-05.md)
joins the existing reductions with a global/local workgroup barrier. It still
scans the vocabulary twice, preserving finite probability bits, NaN behavior,
argmax ties and min-p 0.5. One extra image and one MTP Host TU per backend
pass **216 functional cases, 288 timed graph checks and 90 complete model
protocols**. Ready-input T=4 list wall time falls **2.96-4.27%**, including
submission/completion/wakeup and repeated same-input hot-cache effects.
Model Direct decode changes **58.435 to 58.601 tok/s (+0.285%)**, against
**-0.261%** off-control drift; L0 changes **+0.004%**, against **+0.212%** drift.
This single window establishes no stable model gain or Direct win. The actual
zebin metadata remains 128 GRF/SIMD32 with 384 bytes of SLM, not measured
occupancy or bandwidth. The candidate stays isolated and default-off. Its
initial final verifier failed on tuple/list JSON representation after all
GPU checks passed; an independent raw-log/hash audit passes, preserving the
original error and artifacts. The older frozen M16 profile is used; production
U4, long-context, PPL and Linux GPU results are not updated by this pilot.

The [MTP metadata prestaging screen](../bench/results/a770/orca-mtp-prestage-2026-11-05.md)
prepares known future positions before the first round submission and reuses
its publication fence; GPU copies and result waits remain unchanged. All
**90 complete model protocols match**, with min-p 0.5. Direct decode changes
**58.412 to 58.543 tok/s (+0.223%)**; L0 changes **-0.024%**. This bounded
window establishes no stable gain; the initial candidate stayed isolated and
default-off. The [later source adoption](../bench/results/a770/orca-preparation-defaults-2026-11-06.md)
enables it on L0 after the user accepts smaller improvements, with an explicit0
rollback; Direct remains opt-in. Commit may still run during preparation, but overlap duration
was not measured. The round reads `2*T` mapped rows, so those slots must be
published before submission. Production U4 and the Direct 8K issue remain outside this screen.

The [next-window PLE row0 read-ahead pilot](../bench/results/a770/orca-ple-row0-2026-11-05.md)
uses the existing I/O worker to read 16 known rows before MTP starts. The two
predecessors come from accepted input history, since `ss.ple_prev` advances only
after commit finishes. No preparation thread or mapped GPU-input write is added.
Direct and L0 off/on/off match **90 complete protocols**, with **19 actual
prefetches per enabled request**. Net decode changes **+0.165%/+0.071%**;
this single repeated-Short window establishes no stable gain. The candidate
stays isolated and default-off; actual I/O/GPU overlap duration and cold-read
benefits were not measured. It retains the older frozen M16 profile and min-p
0.5. The initial preparation target error and original EOS-count assertion
failure are preserved. A separate publication audit checks every raw protocol,
count, command, statistic, configuration and archive hash; EOS truncated the
last accepted window, invalidating the original count formula. Production U4,
long-context/PPL/Linux GPU and the intermittent 8K issue are not updated by it.

The isolated [early-route preparation trial](../bench/results/a770/orca-direct-early-route-2026-11-04.md)
moves the route-ID readback ahead of independent shared-expert work. The CPU
waits for that earlier event, then groups rows and prepares expert staging
while later shared work can remain in flight. Same-queue ordering and buffer
ownership stay intact. One prefill Host TU per backend changes; math/runtime
objects are reused. Direct/L0 pass **48 event checks** with deliberately
blocked shared tails, and all **68 model requests match**. The same-EXE Direct
off/on/off comparison changes 2K/8K prefill by **+0.416%/-0.266%**, against
off-control drift **+0.587%/-0.639%**; Short decode changes **-0.284%**.
No stable model gain is established, so the candidate remains isolated with
`STRATA_PREFILL_EARLY_ROUTE` defaulting to 0. L0-on checks output safety only
and is not a performance control. Cached request IDs, exact references and
configs are prepared before waiting for the shared GPU lock and reused by
the test. Long cases run once per process; no PPL/Linux GPU or closure of
the intermittent Direct 8K issue follows.

The subsequent [large-M4 preferred-SLM screen](../bench/results/a770/orca-direct-projection-slm-2026-11-04.md)
keeps the Host objects, mathematical pack, payload placement and dependencies
fixed. Only calibrated C0 generic M4 grids with at least 4,096 groups opt into
the previously checked occupancy rule; the measured 1-KiB-per-group shapes
change preferred SLM from MAX to 16 KiB. Fourteen CTests and 14 packet checks
pass, and only the preferred-partition bits differ. Forward/opposed brackets
provide **180 clean wall samples, 168 data checks and 112 separate traces**.
The first screen's final assertion assumed SYCL dimension order instead of
native XYZ; it remains failed and an independent validator checks its raw
samples and actual `(1,96,512/2000)` groups, `(64,1,1)` local dimensions.
M2047/M7999 on/off times change **-1.967%/-2.020%** forward, but only
**+0.042%/-0.944%** opposed. Forward M2047 control drift is -3.489%; opposed
on controls drift +0.453%/+0.275%. The adjustment does not establish a stable
gain across these shapes or remove the gap: opposed on times remain
**5.212%/5.611% longer than L0**. The candidate stays isolated and off by
default. These are synthetic launch-plus-wait samples, not new model or
exclusive-kernel timings; no production/Linux GPU/PPL/8K-closure claim follows.

The [actual projection commands and L1 trial](../bench/results/a770/orca-direct-projection-packets-cache-2026-11-04.md)
now capture initialization and both synthetic shapes with the same frozen
runtime/math. Bounded capture has zero failures and passes **24 data checks**;
all **56 M4 walkers** are identified separately from memory/initialization
kernels. Their decoded geometry, SIMD, execution mask, indirect length,
hardware local IDs, walk order, required SLM and barrier fields match. Preferred
SLM remains the sole difference among the selected per-projection fields.
The full command streams confirm SBA L1 WBP=0 for Direct versus WB=2 for L0;
MOCS is 6 in both. Aggregate state counts include memory kernels and are not
exclusive-kernel or physical-traffic measurements. A header-only isolated
candidate changes just this process-wide L1 policy, reusing the Host probe and
math objects. Fourteen CTests, both policy encodings and invalid-selector
rejection pass. Forward/opposed brackets pass **180 clean wall samples and
120 data checks**. WB changes M2047/M7999 time by **+2.450%/+0.349%** forward
and **+1.292%/+0.645%** opposed; it does not improve these projections and is
not adopted. Opposed on-control drift is +2.640%/+1.559%, so the small 8K effect
is not established as stable. The recorded command fields narrow the search
but do not explain the remaining allocation/context/dispatch difference.
There is no new model, Linux GPU, PPL or intermittent-8K-closure result.

The [large-M4 allocation/VA screen](../bench/results/a770/orca-direct-projection-alignment-2026-11-04.md)
then changes only two frozen WDDM source files, retaining the same Host objects,
math, parameter placement and synchronization. It tests the previously calibrated
2-MiB bulk descriptors plus aligned GPU VA without increasing physical bytes;
tails and the 1-GiB part limit remain. Both builds pass **28 CTests and 80
changing-input allocation replays**, including boundaries, guards and release
accounting. Forward/opposed brackets pass **180 clean samples and 120 data
checks**. M2047/M7999 on/off time changes **+0.472%/+0.018%** forward and
**-0.388%/+0.361%** opposed: no stable improvement is established, and the
policy is not adopted. Opposed on times remain **3.659%/5.309% longer than L0**,
with on-control drift **-0.318%/-0.100%** and L0 drift **-0.045%/-0.023%**.
This does not support alignment alone explaining the projection gap; descriptors
and VA layout change together, and physical PTE sizes are not measured. A first
build lost `/EHsc` in its script and stopped before GPU work; the corrected
fresh build retains the frozen compiler defaults and preserves the failure.
The Linux CPU checker also adds the shared BCS entry point: a separate WSL
build passes four CPU checks, including 2,268 BLT cases, plus four dependency
audits. That checks the current Linux/i915 source, not the Windows candidate;
no Linux GPU, full-model/PPL or intermittent-8K closure result follows.

The isolated [prefill phase timeline](../bench/results/a770/orca-direct-prefill-phases-2026-11-04.md)
now measures the stage shares left open by the submission census below. One
prefill Host TU per backend is rebuilt; all other Host/runtime objects and the
mathematical pack stay fixed. All **51 requests match**, and six long-request
timelines pass raw timestamp, guard, stage-count and interval-sum checks. Against
the arithmetic mean of the flanking L0 runs, Direct's gather/expert intervals
save **4.614/5.802 seconds** at 2K/8K, while its three GDN/QSA projection
intervals add **0.620/2.747 seconds**. Those projections occupy **36.33%/54.02%**
of L0's timeline, versus **45.23%/59.93%** for Direct. The larger projection
share and added projection time offset more expert-chain savings at 8K.
Host grouping takes only tens of milliseconds; this does not exclude waits
inside the expert chain. The markers add barriers and enable two existing
prefill waits; intervals include GPU work and possible Host/copy/dependency
gaps, rather than exclusive kernel time. **Diagnostic durations are not new
end-to-end performance results.** The internal GEMM/dequant/parameter/cache/
scheduling cause was not isolated in that experiment; its frozen generic FP16
used M4, while M16 covered only paired experts. The later generic M16 trial
above addresses this math path. That diagnostic changed no source policy or deployed EXE; no Linux
GPU/PPL result or closure of the intermittent Direct 8K issue follows.

The [large-XMX parameter isolation](../bench/results/a770/orca-direct-projection-parameters-2026-11-04.md)
then keeps the generic M4 mathematical kernel and all existing Host objects
fixed, rebuilding only private runtime state and the placement condition.
Fourteen CTests, 840 pair checks and engine/BCS queue checks pass. Five clean
processes provide **90 samples and 60 data checks** on representative M2047/
M7999, N6144, K2560 shapes with changing output pointers. Separate traces
confirm both Host/device placements and pass another 24 data checks. Device
parameters change launch-plus-wait time by **-1.32%/+0.80%**, against off-control
drift **-1.46%/-0.35%**; no stable gain is established and the candidate is not
adopted. A separate L0/Direct/L0 timestamp probe passes **36 data checks and
84 guarded intervals**, retaining 54 raw sample intervals. Direct's GPU marker
intervals remain **5.25%/6.83% longer**, with L0 drift **+0.087%/+0.008%**.
The gap therefore also appears between GPU markers, rather than only in the
Host wall clock. Marker intervals still include possible dispatch/dependency
gaps; these are diagnostic observations, not exclusive kernel timings or new
model results. The source of the remaining cache/resource/dispatch difference
is not isolated. These representative shapes do not cover all real model
projections; no model/PPL/Linux GPU or intermittent-8K closure claim follows.

The [source adoption](../bench/results/a770/orca-source-adoption-2026-11-04.md)
integrates Host eager/device-recorded parameters, shared BCS encoding and M16
FP16 pair tiles. All 80 Direct Host TUs are rebuilt; L0 rebuilds the two changed
TUs after checking the other generated sources and actual dependencies. Both
pass 840 pair output checks. Fresh-process 2K/2K/8K checks on both source builds
match all six requests, including tokens, drafts and finish state; successful
retries do not close the earlier intermittent 8K failure. The real repository
Linux/i915 source builds and passes four CPU CTests. Direct stays experimental, BCS requires
`ORCA_DG2_BCS_COPY=1`, and **Linux GPU remains untested**. Deployed EXEs and
production configs are unchanged; this adoption is not a new performance run.

The isolated [graph-tail proof trial](../bench/results/a770/orca-direct-replay-prefix-2026-11-04.md)
uses those source builds with identical mathematics. It preserves new tag resets
and fences while reusing the completed segment tail to finish old inline points.
Both safety screens pass 12,800 old-point checks; the extended screen includes
800 points depending on earlier unpublished readbacks. All 69 adapter-dependent
Host TUs are rebuilt, and all 85 model requests plus a
[75-request opposed Short repeat](../bench/results/a770/orca-direct-replay-prefix-repeat-2026-11-04.md)
match. Same-EXE Short decode gains are **+0.95%/+0.13%**; Direct remains
**0.93%/2.45% below L0**. The repeat gain is smaller than the observed control
drift, so the candidate remains isolated without an established stable model
gain. Forward 2K/8K TTFT is **10.71%/2.11% lower than current-source L0**,
but each long case runs only once per process. No long-context repeatability,
new PPL or Linux GPU conclusion follows, and the intermittent Direct 8K issue
remains open. Independent optimization percentages are not added together.

The [current-source wait isolation](../bench/results/a770/orca-direct-replay-prefix-wait-2026-11-04.md)
then reuses the same prefix-proof Host objects and changes only the native
completion observer from OS events to active polling. Fourteen runtime CTests,
840 pair checks and engine/BCS=0/1 queue checks pass; all **75 model requests
match**. Short event/poll/L0 decode is **56.61/57.39/58.32 tok/s**. Polling is
**+1.37% over events**, but still **1.60% below L0**, with event/poll flank drift
**+0.76%/+0.29%**. This supports a partial wait-path cost, not an attribution of
the entire remaining gap. Active polling is a CPU-consuming diagnostic upper
bound; CPU usage/power are not measured and default waiting stays unchanged.
No new long-context, PPL or Linux GPU result follows.

The isolated [Point allocation batch trial](../bench/results/a770/orca-direct-point-batch-2026-11-04.md)
groups at most 16 inline event objects under one shared owner. It creates new
objects on every replay, preserves old generations and does not wait to fill
a batch. Both policies pass lifetime and unpublished-readback checks; all 69
adapter-dependent Host TUs are rebuilt and all **85 model requests match**.
Short decode is **56.51 tok/s versus 56.83 off and 58.16 L0**. The same-EXE
change is **−0.57%**, against **+1.14%** off-control drift; no full-model gain
is established and the candidate is not adopted. Unmodified Direct's 2K/8K
prefill is **8.03%/0.69% shorter** than current-source L0, with TTFT
**8.01%/0.68% shorter**. Each long case runs once per process; this is not
steady-state or cross-day evidence. No PPL or Linux GPU claim follows, and
the intermittent Direct 8K issue remains open.

The separate [prefill submission census](../bench/results/a770/orca-direct-prefill-submit-census-2026-11-04.md)
uses adopted-source Host/runtime objects and an observer without per-call
clocks or packet capture. All **34 requests match**, with 68 prefill/decode
spans retained. Direct/L0 2K prefill submits **67,669/184,527** times; 8K
submits **81,510/225,908** times: Direct has **63.33%/63.92% fewer** submits.
Its Copy node2 counts are **28,002/33,876**, versus L0 **56,140/67,890**.
The last hot Short decode has **97 submits and 112 observable `_Mtx_lock`
calls on both paths**. CPU fence registration and GPU queue waits serve
different purposes; unobserved private locks are outside the census. Atomic
counters and diagnostic SRW locks still perturb execution, so these durations
are not performance evidence. The similar prefill count reductions do not
explain the smaller clean 8K time gain; compute, memory and expert-pipeline
time shares remain unmeasured. Each long case runs once per process, and the
earlier intermittent 8K issue stays open.

The [dependency-only prefill follow-up](../bench/results/a770/orca-direct-dependency-only-2026-11-04.md)
is now adopted in three source files byte-identical to the tested candidate;
repository generator output also matches. It removes unused eager completion
exports at 16 prefill wait sites, preserving recorded/exported events and actual
submission boundaries. All 69 adapter-dependent Host TUs are rebuilt, GPU
kernel/engine/BCS queue checks pass, and **85 clean plus 34 diagnostic requests
match**. Same-EXE 2K/8K prefill is **5.35%/2.08% shorter**, with off-control
drift **−0.44%/−0.02%**; versus common-math L0 it is **14.15%/3.44% shorter**,
with TTFT **14.11%/3.43% shorter**. Short decode is **56.77 tok/s**, versus
**56.85 off and 58.10 L0**, leaving Direct **2.28% below L0**. Long cases run
once per process; no steady-state or cross-day claim follows. Independent
2K/8K prefill counts fall **67,669→39,671 / 81,510→47,638**
(**41.37%/41.56% fewer**); visible application mutex counts show no consistent
decrease. Hot Short decode still has 97 submits and 112 visible mutex calls.
Diagnostic durations are excluded from the performance conclusion. Clean
2K/8K prefill saves about **1.35/1.49 seconds** against off: similar absolute
savings occupy a smaller share of the longer 8K duration. The count advantage
persists; separate compute/memory/expert time shares remain unmeasured. The new policy
defaults to 1 inside opt-in Direct; `STRATA_ORCA_DG2_DEPENDENCY_ONLY=0` restores
the previous policy. Deployed EXEs/configs are unchanged. Linux GPU, PPL and the
intermittent Direct 8K correctness issue remain open; L0 stays recommended.

The isolated [small-primary/secondary-batch trial](../bench/results/a770/orca-direct-secondary-batch-2026-11-04.md)
keeps the dependency-only Host policy and mathematical pack fixed. Fourteen
runtime CTests, 840 pair checks and engine/BCS=0/1 queue checks pass; all **85
clean model requests match**. Short on/off/L0 is **56.94/57.11/58.31 tok/s**,
or **−0.30% versus off and −2.35% versus L0**, with off-control drift **+0.71%**.
2K/8K prefill changes **+1.74%/+0.41%** versus off, against off-control drift
**−1.59%/−1.02%**. No model gain is established; the candidate remains isolated.
All **30 diagnostic Short requests match**, and every measured on-submit has
a 16-byte primary. Hot decode primary bytes fall **7,198,104→1,552**, while
submits and observable mutex calls remain **97/112**. Diagnostic durations
are not used for the performance claim.
The existing body copy happens at close, not at replay. Smaller primary command
lengths do not prove reduced tensor transfers or a speedup, and this experiment
does not exclude every KMD cost. Long cases run once per process, Linux GPU is
untested, and the earlier Direct 8K correctness issue remains open.

The bounded [cold-history and NaN-scratch checks](../bench/results/a770/orca-direct-cold-history-2026-11-04.md)
retain the original 8K failure alongside **15 new clean requests**: old Direct,
current dependency-only on/off/on and current L0 each start with 2K/2K/8K,
without Short warmup. All match complete outputs and draft counts. Another
**six diagnostic requests** poison the shared attention/MoE scratch before
every half on current L0 and Direct; all match, with **576 residual scan points
and zero non-finite halves**. The existing post-zero logits/residual diagnostic
never triggers. Poisoning and scans alter scheduling and cover only that
scratch/residual path; these finite successes **do not close or identify the
cause of the original intermittent issue**. Frozen executables and mathematical
packs are reused, no runtime source or default changes, and no new performance,
PPL or Linux GPU conclusion follows. The report preserves all 21 new requests
and the original failure; L0 remains recommended.

The isolated [seven-arm combination trial](../bench/results/a770/orca-direct-combined-2026-11-04.md)
combines Host eager parameters, BCS copy roles and M16 pair tiles, with all
**119 requests matching**. Against original L0, 2K/8K prefill takes
**8.92%/6.97% less time**; with the identical wide mathematical pack in L0,
it takes **7.84%/1.55% less time**. Short decode remains **1.90% below L0**.
Long cases run once per process, with control drift reported; this does not
establish steady-state or cross-day repeatability. This is the frozen isolated
EXE; source adoption above has separate checks. The intermittent shortened-history
Direct 8K correctness issue stays open.

Separate portable BCS source checks in that report share the calibrated packet
bodies and add cached i915 compute/copy engine selection. The Windows rebuild
passes 1,632 fills, 12 chains and 16 multi-queue outputs; WSL Linux/i915 builds
and passes its three CPU checks. Both OSes pass 2,268 CPU packet-output cases.
That portable-source check predates the source adoption above and is separate
from the timed binary.
**Linux GPU remains untested**, including BCS MOCS/cache visibility.

The isolated [current-frontend BCS integration](../bench/results/a770/orca-direct-current-bcs-2026-11-04.md)
rebuilds all 80 Host TUs, including 69 adapter users, and routes prefill,
verification and expert-refill copy roles to Copy node2 while compute stays
on node5. Same-EXE BCS off/on/off gives **7.32%/2.89% shorter 2K/8K prefill**
and **+0.98% Short decode**. All 51 model requests match, plus 1,632 guarded
fills, 12 cross-engine chains and 16 outputs across two compute/two BCS
logical queues. Multiple logical queues share those nodes; simultaneous
physical-engine utilization is not measured. Long cases run once per process;
no L0 model-performance arm is included. This Windows candidate is isolated,
the earlier 8K issue remains open, and this frozen snapshot has no Linux BCS.
The separate portable source above has only compile/CPU validation on Linux.

The [real-model FP16 pair tile follow-up](../bench/results/a770/orca-fp16-pair-tiles-2026-11-04.md)
keeps the dynamic-parameter policy fixed and changes only native_gemm. Larger
register-reuse tiles handle full 16-row prefixes for experts with at least 128
rows; original M4 handles smaller experts and tails. All 51 model requests
match, with **3.77%/10.09% shorter 2K/8K prefill** versus Direct's previous pair.
Long cases have one ordinal-matched request per process; no steady-state or
L0 model-performance win is established. Direct/L0 pass **1,680 pair checks**.
Short has no measured gain. The candidate is isolated; the intermittent 8K
correctness issue remains unresolved. Its unchanged baseline selects CCS0;
the separate BCS candidate above adds explicit copy roles. Earlier isolated BCS
results are not proof of this frontend's active simultaneous engine utilization.

The [dynamic-parameter follow-up](../bench/results/a770/orca-direct-dynamic-parameters-2026-11-04.md)
keeps eager parameters on the host and retains device mirrors for recorded graphs.
All 51 clean model requests match, plus 12 cold-history requests and 32 dedicated
placement/replay submissions. Observed 2K/8K prefill time falls **5.77%/2.77%**
against Direct itself; each long case has one request per process, Short has no
measured gain, and the earlier intermittent 8K failure remains unresolved.
The candidate is isolated and establishes no win over L0.

An independent [XMX SLM/prefetch screen](../bench/results/a770/orca-xmx-slm-async-2026-11-04.md)
passes **760 output/guard checks**. Larger register-reuse tiles improve kernel
throughput for M160 GU/Down and M2048 shapes by **1.95–1.97x/1.32–1.37x/2.49–2.54x**
over its M4 baseline across three runs, but regress small GU batches. The tested
SLM/double-buffer variants lose; L2 prefetch is not a general win. These are
standalone SYCL kernel measurements, not Orca or model speedups. Production
kernels and defaults are unchanged; no Linux GPU result is established.

The earlier [current-frontend context comparison](../bench/results/a770/orca-direct-current-contexts-2026-11-04.md)
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
