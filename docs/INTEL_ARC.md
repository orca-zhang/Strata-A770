# Intel Arc (experimental)

The native **Orca Level Zero** backend is documented in [sycl/orca/README.md](../sycl/orca/README.md).
Build it from the root with `-DSTRATA_ENABLE_ORCA=ON`, then use `python setup.py --backend orca`.
The Intel installer defaults to Orca and discovers a local native build. Root setup also chooses Orca
when a supported discrete Arc is available and no NVIDIA or AMD card can be used. Explicit backend
and CUDA-toolkit choices take precedence; `--backend sycl` selects the SYCL baseline described below.
Existing Intel profiles are activated through the selected runtime before startup. Switching backends
backs up the previous config and keeps its engine arguments and sampling settings; see the native README
for saved custom builds and Linux container-path migration.
Restored-reserve1536 Windows A770 measurements: 5896 expert slots/8093 MiB; Short 24.34 tok/s, Code2K TTFT 28.728s, Long8K 77.195s. Sixteen 64-token requests, twelve retained samples, minimum 1 Hz sampled free VRAM 947.051 MiB. Same executable/images as the earlier reserve3072 run; separate time periods and automatic cache sizes, no causal reserve speedup or fresh SYCL/quality-equivalence claim. The 1024 MiB headroom check failed; the raw failure is preserved, and these are timing observations only. See the [latest samples and conditions](../bench/results/a770/orca-reserve1536-2026-10-31.md).

The same current build reached 38.54 tok/s on continuous Sky23/64 requests using the full production profile (auto 5949 slots, ten warmups/five retained). Historical fixed5771-slot settings reached 38.42. All30 outputs and37/50 draft counts matched the mixed Short. These controls separate continuous repeated-input behavior from the mixed-request table; cache/adaptation/PCIe contributions are not isolated. See the [workload diagnostic](../bench/results/a770/orca-short-workloads-2026-10-31.md).

Subsequent quality checks use full-distribution raw PPL as the gate (aggregate <= +1%, each case <= +3%); conditional PPL is diagnostic only. The historical 1022-target result fails this policy. The tool now scores fixed assistant continuations with complete chat headers; see the [quality policy and reproduction checks](../bench/results/a770/orca-quality-policy-2026-10-31.md).

The new 502-target diagnostic completed with valid scores for the frozen FP16-pair Orca build, while the previous SYCL binary produced NaNs on 134 targets in two 8K cases. The overall quality gate failed; no aggregate PPL is reported for the valid subset. Raw and conditional PPL changes have the same direction in the five comparable cases. This uses newer matched settings and does not establish quality equivalence for the historical speed samples.

A separately validated local SYCL recovery entry now uses the frozen M4/256-GRF executable: `run-iq2_xs-sycl.bat` and `strata-iq2_xs-sycl.json`. It passed two cold rounds of all 502 assistant targets with score TSVs identical to the frozen Orca reference, plus four HTTP checks and six mixed 2K/8K history requests. The 580 physical free-memory samples had a minimum of 3762.17969 MiB. This entry uses prefill2048, 3000 requested expert slots and reserve3072; the main Orca profile was not edited by this recovery. The old `.sycl-backup` is a rejected historical archive. These checks provide limited quality coverage and no new speed measurement; see the [recovery evidence and runnable checks](../bench/results/a770/sycl-recovery-2026-10-31.md).

A separate RAM-cache candidate restores the requested checkpoint directly, avoiding a live-state upload followed by a rewind. On Windows A770 an independent repeat confirmed 8K cache-hit TTFT reductions of 4.34%/4.64%, with baseline drift of −0.10%/−0.86%. The 2K result remains unconfirmed because its second baseline drift was 3.98%; decode and whole-request throughput did not reach the 1% gain threshold. The repeat passed 4680 snapshot checks and 40 full 2K/8K requests with outputs and MTP workloads identical to the first run. The local opt-in `strata-iq2_xs-ram-cache.json` also repeated four alternating HTTP requests with distinct answers and two actual RAM restores, using 2048 MiB/four conversation slots, reserve3072 and the production adaptation settings. The main profile remains unchanged; see the [independent repeat](../bench/results/a770/snapshot-target-restore-reconfirm-2026-10-31.md) and [first-run evidence and launch command](../bench/results/a770/snapshot-target-restore-2026-10-31.md).

A separate same-source control completed 72 requests with 4608 identical generated tokens and draft workloads: decode +2.89--5.56%, whole request +2.54--3.87%, with capacity-4 decode baseline drift up to 2.44%. This supplementary control includes native scheduling and GEMM integration; it does not isolate API entry-point cost.
The local IQ2_XS main profile now selects the checked FP16-pair build with
automatic expert caching, 8192-token prefill, MTP capacity 4, `spec-min-p=0.5`
and reserve1536, restored at the user's request for the next engine start.
The earlier reserve3072 OFF/ON/AUTO check passed 11 requests with 704 identical output tokens;
the lowest 1 Hz physical free-memory sample was 1675.168 MiB. The original SYCL
config remains backed up; existing services were not restarted. The restored profile has now been measured with 5896 slots/8093 MiB; see the latest measurements above.
See the [adoption evidence](../sycl/orca/prefill-pair-default-a770-windows-2026-10-31.json). Native HTTP, session and batch checks
are documented in the native README. Linux compilation and GPU behavior, and
Orca performance on other Arc models, remain unverified. Cross-platform optimization research is in
[ORCA_CROSS_PLATFORM_RESEARCH.md](ORCA_CROSS_PLATFORM_RESEARCH.md).

## SYCL port background and earlier validation

The following sections retain the upstream SYCL port's history, requirements
and dated tests. For the current Orca build and support scope, use the native
README linked above; the historical platform limitations below apply to those
earlier SYCL tests.

Strata 0.1.39 includes an **experimental Intel Arc engine**: Strata's own engine ported to SYCL (Intel oneAPI).
maxfridbe wrote it in [#423](https://github.com/Niko1221/Strata/pull/423), with fixes from the people testing it.
The code is in `sycl/`, and the port's own notes, measurements and maintenance procedure are in
[docs/INTEL.md](INTEL.md). This page covers what you need, how to build it, and what has been tested.
For ArcInt / OpenVINO comparisons, see [ARCINT_COMPARISON.md](ARCINT_COMPARISON.md).

**Experimental means:** the Strata maintainers have no Intel GPU. We compile the port and run its kernel tests, but
we have not run it on an Arc. Every result on Arc hardware below comes from the community. The NVIDIA and AMD
engines are unchanged: the Intel build is a separate CMake target, off by default.

There is **no ready-made Intel engine** in the release zips. You build it from source on Linux.

## What has been run, and by whom

| | Hardware | Result | Reported in |
|---|---|---|---|
| Port author | Arc Pro B70 32 GB, Ubuntu 24.04 | Coder IQ1_M: 70-78 tok/s decode, ~790 tok/s prompt; IQ2_XS: 51-64 tok/s; 256K context measured | #423, [INTEL.md](INTEL.md) |
| Community | 2x Arc Pro B70, `--layer-split` | Flash-Next IQ3_XXS 66 tok/s decode, 394 tok/s prompt (with the `stage_room` fix that is now in 0.1.39) | #423 |
| Community | Arc Pro B50 16 GB | Coder IQ1_M ~23 tok/s, IQ2_XS ~25-27 tok/s, up to 128K | #423 |
| Community | Arc B580 12 GB, WSL2 | IQ2_XS ~21 tok/s, Coder ~15 tok/s; **device loss also seen** | #423 |
| Community | 2x Arc Pro B60 24 GB, `--layer-split` | Coder IQ1_M: 56.6 tok/s decode, 428 tok/s prompt (2,129 tokens); Flash-Next IQ2_XS: 58.6-61.3 tok/s decode, 436 tok/s prompt; all experts in VRAM; 8K context | [bench/results/2026-10-04-community-2x-arc-pro-b60](../bench/results/2026-10-04-community-2x-arc-pro-b60/README.md) |
| Community | one Arc Pro B60 24 GB | Coder IQ1_M with 4,042 of 12,288 experts mirrored in RAM: 11.9 tok/s decode (needs the ring-wait fix from the same report) | same |
| Strata maintainers | no Arc | compile check and kernel tests on a CPU device only (below) | this release |

The 0.1.39 port re-migrates 0.1.38's port onto the 0.1.39 engine sources (the #606 NaN fix, the #649 verify
trace, the new prompt paths). 0.1.39's `sycl/` did not compile against 0.1.39's own engine sources (#784: the
`ThreadAffinity` type of #626 and the layer range of `NativeDense::load` from #559), and its ring waits never saw a
slow layer as still running (#866, #867). Both are fixed in 0.1.40, and the two B60 reports below were measured with
exactly those two fixes on top of 0.1.39. **No one has run an unpatched 0.1.39 port on an Arc**, and the other rows were
measured on earlier versions.

The two B60 rows ran `6f32ec0` plus two small `sycl/` fixes (the compile fix and the ring-wait fix), AOT `bmg-g21`, on Ubuntu 24.04 with
`xe`, Level Zero V2, NEO 26.09.37435.12 and oneAPI 2026.1.1, without Docker. Host: Ryzen 5 5600, 64 GB RAM, PCIe 3.0 x8 per card.
Full flags, per-request timings and engine logs are in the report linked in the table.

## What was tested here (0.1.39)

- **Compiles:** Ubuntu 24.04 (WSL2), Intel oneAPI DPC++ 2026.1.1 + oneMKL 2026.1. The whole `sycl/` project
  builds with 0 errors: the `strata` engine plus all 157 targets (the kernel parity tests and benches). The build is
  SPIR-V (JIT). The AOT build (`STRATA_SYCL_AOT`) was not built here, because it needs `ocloc`.
- **Kernel parity tests on a CPU** (`ONEAPI_DEVICE_SELECTOR=opencl:cpu`, Intel's OpenCL CPU runtime, AMD Ryzen 5 7600):
  14 of 25 pass. That is the same set that PR #423's own 0.1.38 port passes on that device: the failures are tight
  float tolerances on the CPU's math (rel 3e-6 against a 1e-6 limit), model fixtures that are not present, and two
  tests that time out on a CPU. This is a check that the kernels compile and compute, not a test of an Arc.
- **Setup:** `--backend sycl` warns, then hands over to `sycl/setup_intel.py` on Linux (unit tests:
  `tools/test_setup_sycl.py`).
- **Unchanged:** the CUDA engine's greedy output, checked byte-identical against the gated 0.1.39 build (Q2_0 and Coder).
  The HIP build is not affected (the option is off by default).

**Not tested by anyone yet:** Windows (no native build path; see below), Arc on WSL2 for the 0.1.39 port, the Alchemist
A-series, integrated Arc GPUs (the B390 / Panther Lake in #515; the shared-memory planning does not exist yet), and
images (not wired on Intel).

## What you need

- **Linux**, e.g. Ubuntu 24.04, with Intel's GPU driver (the `xe` or `i915` kernel driver plus the compute runtime /
  Level Zero; on Ubuntu, `intel-opencl-icd libze1 libze-intel-gpu1`, or Intel's
  [client GPU guide](https://dgpu-docs.intel.com/driver/client/overview.html)).
- **Intel oneAPI**: the DPC++ compiler (`icpx`, 2025.3 or newer; 2026.1 is what was built here) and **oneMKL**. About 5 GB.
- `cmake` 3.24+, `ninja`, `git` (the build fetches ggml unless you point `STRATA_GGML_DIR` at a llama.cpp checkout),
  Python 3.
- For `setup --backend sycl` today: **Docker**. `sycl/setup_intel.py` runs the engine in the `strata-sycl-dev`
  image built from `sycl/tools/Dockerfile`. Note that the Dockerfile starts from a community llama.cpp SYCL image
  (`ghcr.io/snailium/...`), not an Intel or Strata image.
- VRAM: the port keeps the experts on the card (`--stream-experts`). A 32 GB card holds the Coder IQ1_M or IQ2_XS.
  Smaller cards mirror part of the experts in RAM and are slower.

## Build (Linux)

Install oneAPI from Intel's apt repository (this is what was used here):

```sh
wget -qO- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
  | sudo gpg --dearmor -o /usr/share/keyrings/oneapi-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" \
  | sudo tee /etc/apt/sources.list.d/oneAPI.list
sudo apt update
sudo apt install intel-oneapi-compiler-dpcpp-cpp intel-oneapi-mkl-devel ninja-build cmake git
```

Then build the engine. Either command works: the first goes through the top-level CMake option, the second
configures the `sycl/` project directly.

```sh
source /opt/intel/oneapi/setvars.sh
cmake -S . -B build-sycl -G Ninja -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx -DSTRATA_ENABLE_SYCL=ON
#   or: cmake -S sycl -B build-sycl -G Ninja -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icpx
cmake --build build-sycl --target strata
```

Options:

- `-DSTRATA_SYCL_AOT=bmg-g31` (Arc Pro B70) or `bmg-g21` (B580 / B570 / Pro B60) compiles the GPU code ahead of
  time. This needs `ocloc` (Intel's `intel-ocloc` package). Without it, the first start JIT-compiles every kernel,
  which takes about 47 s.
- `-DSTRATA_SYCL_PARITY=OFF` skips the kernel tests (on by default). Run them with
  `ctest --test-dir build-sycl` on the card.

`setup_intel.py` looks for `build-sycl-aot/strata` or `build-sycl/strata` in the checkout. With the
top-level option the engine is at `build-sycl/sycl/strata`, so either use `-S sycl` or set `STRATA_SYCL_BIN`.

## Setup and running

```sh
./setup.sh --backend sycl [setup's usual options, e.g. --model IQ2_XS --context 32768]
```

This prints the experimental warning and continues with `sycl/setup_intel.py`. That script finds the Arc in sysfs,
uses the SYCL engine you built, and writes the config and `run-<model>.sh`. It still downloads and packs the model
the usual way. To run the engine by hand (no Docker), see "How to run it by hand" in [INTEL.md](INTEL.md).

Things that matter on an Arc (details in INTEL.md):

- **Do not ask for more VRAM than the card has.** On the `xe` driver, an allocation past VRAM can push buffers into
  RAM until the machine runs out of memory and stalls. Leave about 1.5 GB free.
- `SYCL_CACHE_PERSISTENT=0`: the persistent JIT cache crashed on Xe2 during the first compile.
- Two cards: `ONEAPI_DEVICE_SELECTOR=level_zero:*` (the image pins `level_zero:0`; `strata-sycl.sh` now passes the
  variable through) and `--layer-split`.
- **Arc Pro B60:** the PCI id is `8086:e211` (`lspci -nn`, the kernel's `xe` id list files it with the BMG-G21 cards), `sycl-ls` prints
  `Intel(R) Arc(TM) Pro B60 Graphics 20.1.0`, and `ocloc ids bmg-g21` prints 20.1.0, so `-DSTRATA_SYCL_AOT=bmg-g21` is the right target.
  `setup_intel.py` knows both B60 ids, `e211` and `e221` (both seen on B60 cards). On a `--layer-split` the startup line "N experts are neither in VRAM nor mirrored"
  counts the other card's layers; the lines `100% of the experts resident` that follow are the ones to read. Set `STRATA_MIRROR_MIB=0` there,
  or the pinned mirror is allocated and not used.
- **`setvars.sh` and `set -u`:** `source /opt/intel/oneapi/setvars.sh` in a shell with `set -u` stops at `OCL_ICD_FILENAMES: unbound variable`
  (oneAPI 2026.1.1). Source it before `set -u`, or run `set +u` around it.
- **Without Docker:** `sycl/serve/strata-sycl.sh` needs Docker. Natively, `source setvars.sh`, export `SYCL_CACHE_PERSISTENT=0`,
  `ZES_ENABLE_SYSMAN=1` and `STRATA_VERIFY_DEVICE_PLAN=1`, then run `build-sycl-aot/strata` or `sycl/serve/server_intel.py` with an `exe` that does this.
  A normal user is not in the `render` group on Ubuntu, and then `sycl-ls` shows only the CPU device.
- `STRATA_VERIFY_NO_HOST=1` (set by `strata-sycl.sh`) is only valid when every expert is in VRAM. On smaller cards
  that path is the one that has hung, and #667 found the likely reason: the GPU does not see the CPU's flag
  stores without a system fence.

## Windows (experimental)

`setup --backend sycl` on Windows now detects the Intel Arc (display adapters + the
display-class registry's 64-bit VRAM size) and hands over to `sycl/setup_intel.py`, which runs the
native Windows build (`build-sycl-aot\\strata.exe` via `sycl/serve/strata-sycl.bat`, no Docker).
Build it with Intel oneAPI (`icx`/`icpx` 2026.1 + oneMKL, e.g. conda-forge `dpcpp_win-64`) and
`sycl\\tools\\build.bat`. `sycl/CMakeLists.txt` accepts IntelLLVM flags on Windows; MSVC `cl`
is not supported. The Monitor tab names the card from the same detection; util/temp/power are
not read on Windows yet. Needs VS Build Tools (the MSVC linker `icx` invokes) and an Intel
graphics driver with Level Zero (`ze_intel_gpu.dll`, for `level_zero:0`; without it `sycl-ls`
lists only `opencl:gpu`). WSL2 with an Arc has been used by one tester (the B580 row above),
but setup cannot detect the card there, because it reads `/sys/class/drm`, which WSL2 does not have.

Measured on Windows (Arc Pro B70 32 GB, driver 32.0.101.8976, i9-9900, 64 GB RAM, conda-forge
`dpcpp_win-64` 2026.1.1, OpenCL backend, `STRATA_VERIFY_EAGER=1`): the `strata` engine builds with
0 errors; `quantize_act_parity --selftest` and the sampler checks pass byte-exact; the Coder IQ1_M
(12,288/12,288 experts resident, `--stream-experts`, 32K context) chats through the OpenAI API at
31-33 tok/s pure decode and up to 78.7 tok/s over a 155-token answer with MTP drafts (Linux B70: 78.2).
Warm prompt re-reads reach 123-128 tok/s; cold first reads go through the plain-SYCL GEMM fallback
(oneMKL SYCL BLAS has no OpenCL Xe2 backend) at single-digit tok/s. Partly-resident models
(IQ3_XXS: 17,687/24,576) fall back per missing expert and need a RAM mirror (`STRATA_MIRROR_MIB`)
or all-resident sizing; eager replay refuses non-device-planned runs loudly instead of running them
wrong. Needs an admin account for VS Build Tools and the Intel driver; the conda + VS + NuGet/PyPI
oneMKL pieces install per-user.

## A770 throughput-first optimizations

On Windows, Arc A770 driver 32.0.101.9034 and oneAPI 2025.3.2 / Level Zero,
the retained HC combination (owned-stream static3 normalization, eight-read
ordered reduction, and 512-thread SG16 sliced down) improves complete-HC
throughput by **10.418%** geometric mean over T1-T6. Three adjacent A/B/A
triples give +9.632%, +10.941% and +9.974%; every T improves by at least 9.767%.
Default/direct/norm-fallback parity, changed graph replay and null injection pass.

The native max/min Router change previously measured **45.653%** operator
throughput gain, but only **0.005559%** fixed-slot model Decode gain. IQ2_S
GU16 physical SG16 previously measured **16.672%** VRAM operator gain; its
**0.952894%** formal model result used slightly different automatic cache
sizes. These gains are separate measurements, not additive Decode gains.
`STRATA_GU_IQ2S_SG16=0` retains the SG32 expert path; other formats/lane
settings remain SG32. No speed claim is made for other Arc devices.

At the user's request, verified operator throughput gains are retained even
without 1% whole-model gain. Earlier failed-gate records are not rewritten.
See [the adoption record](../bench/results/2026-10-13-a770-throughput-adopt/README.md)
for exact outputs, combined model measurements, source-only adoption and checks.
The combined fixed-slot model check measures +0.074737% Decode geometric gain,
with all 36 outputs exact. Baseline Decode drift reaches 1.728%, so this does
not establish a meaningful model speedup. The user confirmed the complete-HC
throughput gain as sufficient for source adoption; this run does not replace
the live engine.

A follow-up retains WG512/SG16 but skips inactive tail subgroups and removes
their per-row load/store masks. Complete-HC throughput improves a further
**1.563%** against that adopted baseline; adjacent A/B/A triples give +1.441%,
+1.874% and +1.233%, with T1-T6 gains from +0.640% to +2.692%. SG8 alone
(-3.531%) and SG8 with live rows (-0.758%) are rejected. Arithmetic order,
default/direct/norm-fallback parity, graph replay and null-injection checks
are retained. This is source-only adoption, not a measured model or cumulative
speedup; see [the follow-up record](../bench/results/2026-10-13-a770-hc-wg512-live/README.md).

Changing only the sliced-down reduce launch from SG32 to SG16 improves
complete-HC throughput by **7.039%** against that live-row baseline. With
5000 calls and 1000 ms warm-up per T, three adjacent A/B/A triples give
+6.992%, +6.816% and +7.279%; T1-T6 improve by +5.328% to +8.752%.
Maximum adjacent baseline drift is 2.828%. Packing actual-T partials alone
is slower (-1.011%); packing plus SG16 gives only +3.054%, so only the
one-line subgroup change is retained. Default/direct/norm-fallback parity,
graph replay, null-injection outputs and emitted subgroup requirements pass.
This is source-only adoption, not a deployed, cumulative or whole-model
speedup; see [the reduce record](../bench/results/2026-10-13-a770-hc-packed-partials/README.md).

A separate production integration recompiles the live-row baseline and
reduce-SG16 candidate against the frozen production headers. Complete-HC
throughput improves **6.414%** geometric mean; three adjacent A/B/A triples
give +6.307%, +6.799% and +6.230%, with every T1-T6 improving by +5.484% to
+7.860%. Default/direct/norm-fallback parity, graph replay, null injection,
six exact model outputs, API comparisons and reload checks pass. The HC
package is now deployed together with the existing IQ2_S SG16/singleton
object. This is an operator measurement against the live-row source
baseline; cumulative or whole-model gains are not claimed. See
[the runtime record](../bench/results/2026-10-13-a770-hc-reduce16-runtime/README.md).

A subsequent source-only change widens ordered reduce prefetch from eight
to sixteen reads and reduces its work-group from 128 to 64 threads, retaining
SG16. Unprofiled same-T A/B/A with persistent workers measures **+3.489%**
complete-HC throughput against the adopted prefetch8/WG128/SG16 baseline.
Three triple means are +3.342%, +4.136% and +3.139%; every T1-T6 improves,
with maximum adjacent baseline drift of 2.773%. All 117 sample checksums
match. The whole-T sweep still drifts 7.385% and remains diagnostic; earlier
failed-drift decisions are unchanged. No deployment, cumulative or model
speedup is claimed. See
[the paired-worker record](../bench/results/2026-10-13-a770-hc-paired-worker/README.md).

Up now uses SG16's fixed column ownership to visit the four HC streams
directly, retaining the original dot products, summation order, local memory
and both barriers. Against the adopted prefetch16/WG64/SG16 baseline,
same-T A/B/A screening measures **+1.205%** complete-HC throughput; an
independent confirmation measures **+1.492%**. Confirmation triple gains
are +1.482%, +1.092% and +1.465%, all T1-T6 improve, and maximum adjacent
baseline drift is 2.786%. All 162 timed checksums, default/direct/norm-fallback
parity and null/graph checks match. The separate unroll4 arm screens at
+1.100% and is not selected. This is source-only adoption, not a deployed,
cumulative or whole-model speedup; see
[the fixed-stream record](../bench/results/2026-10-13-a770-hc-up-fixed-streams/README.md).

`STRATA_SYCL_HC_BATCH_SUBMIT` is an experimental SYCL compile option,
default **OFF**. It uses oneAPI eventless submission for the default HC
norm/down/reduce/up launches without changing kernel math or adding a
runtime dispatch branch. It targets continuous batches on A770, not
wait-after-each latency. Add `-DSTRATA_SYCL_HC_BATCH_SUBMIT=ON` to the normal
SYCL CMake configure command and rebuild; use `=OFF` and rebuild to restore
the existing submission path. It is not an environment variable. No CPU
expert scheduling or CUDA/HIP code changes are involved, and no model
Decode gain or improvement on other devices/toolchains is claimed. See
[the switch validation](../bench/results/2026-10-14-a770-hc-cpu-submit-switch/README.md)
and [the earlier scheduling experiments](../bench/results/2026-10-14-a770-hc-cpu-submit/README.md).
The earlier all-eventless candidate confirmed **+32.144%** complete-HC
queued throughput on the A770 with oneAPI 2025.3.2. The guarded ON/OFF
screen measures +30.182% with 1.558% maximum baseline drift; its independent
confirmation overlaps another GPU worker and drifts 14.898%, so it is
diagnostic, not a new confirmed gain. Wait-after-each does not pass. Both
builds pass default/direct/norm-fallback parity, changed graph replay and
byte-identical null checks. Default settings and live binaries stay unchanged.

The native oneAPI runtime parameter `UR_L0_BATCH_SIZE=64` is a separate
optional continuous-submission experiment, not a new scheduler. On the
A770 with oneAPI 2025.3.2, the same default-OFF eventful HC executable
confirms **+2.836%** throughput after a +2.812% screen; all T1-T6 improve
and maximum confirmation drift is 2.595%. Wait-after-each fails, with
81.271% maximum baseline drift, so no latency gain is claimed. Both
eventful/eventless builds pass default/direct/norm-fallback parity and
byte-identical null/changing-input graph checks, but their combined
performance is not measured. This environment parameter is process-wide,
not HC-only: set it before an experimental SYCL process starts, and unset
it and restart to restore runtime dynamic batching. Defaults and live
configuration stay unchanged; no whole-model Decode or cumulative gain
is claimed. See [the native batching record](../bench/results/2026-10-14-a770-hc-static-batch/README.md)
and [the CPU runtime profile](../bench/results/2026-10-14-a770-hc-cpu-runtime/README.md).

A separate independent HC graph test does not support removing verify's
ignored post-launch queue query. The wait-each screen measures **-2.108%**
throughput and **+2.153%** latency with 2.340% maximum drift; two of three
epochs regress, so the engine query and waits remain unchanged. Its CPU
profile places 91.893% sampled time in the driver/Level Zero loader,
including a 69.886% driver leaf under `urQueueFinish`, not in expert-pool
scheduling. The continuous-graph branch fails large drift and is aborted
with all published rows retained; no queued or model gain is claimed. See
[the graph query record](../bench/results/2026-10-14-a770-hc-graph-query-wait/README.md).

`STRATA_SYCL_VERIFY_GRAPH_EVENT_WAIT` is a separate experimental SYCL
compile option, default **OFF**. Only non-eager `STRATA_VERIFY_NO_HOST=1`
graph replay waits on its graph-execution event instead of the whole
compute queue; host/eager, copy, commit and watchdog waits are unchanged.
On the Windows A770/oneAPI 2025.3.2, the independent HC wait-each test
confirms **+0.724%** throughput and **0.718%** lower latency after a
+0.868% screen, with 2.859% maximum confirmation drift. These are reciprocal
metrics of one cadence, not queued or model Decode gains. Both ON/OFF
verifier translation units compile and 256 native guard checks pass;
the full engine is not relinked or deployed. Add
`-DSTRATA_SYCL_VERIFY_GRAPH_EVENT_WAIT=ON` to an experimental SYCL configure
command and rebuild; use `=OFF` and rebuild to restore the original path.
No combination with earlier eventless/batch64 options is measured, and
driver busy-wait elimination is not claimed. See
[the graph completion record](../bench/results/2026-10-14-a770-hc-graph-event-wait/README.md).

A subsequent native `event_mode(low_power)` completion-barrier test on
the same Windows A770/oneAPI 2025.3.2 is rejected: independent HC wait-each
throughput changes **-1.997%**, latency grows **2.038%**, and maximum
baseline drift is 2.640%. Native trace shows all 16 low-power flags are
forwarded, but the empty in-order barrier returns the existing graph
event, without creating an interrupt event. An explicit-dependency
partial-barrier follow-up passes graph/readback checks but fails native
activation, so it is not timed. All 256 graph/readback checks pass; no
engine/default, CUDA/HIP or deployment changes and no queued/model gain
are claimed. See [the barrier record](../bench/results/2026-10-15-a770-hc-low-power-wait/README.md)
and [the explicit-dependency check](../bench/results/2026-10-15-a770-hc-low-power-explicit/README.md).

An independent approximate expert-pruning prototype retains each token's
Top-9 or Top-8 execution-plan entries while preserving K=10 destination
strides and renormalizing retained weights. On Windows A770/oneAPI
2025.3.2, synthetic IQ2_S/Q2_0 expert plus zero-fill/combine throughput
confirms **+8.222%** / **+16.779%** across T1/T4 VRAM/pinned-RAM shapes,
with 1.818% / 1.197% maximum baseline drift. It is not equal-output
inference: maximum synthetic layer-combine relative L2 differences are
8.204% / 14.203%. Router/planner/shared-expert projection, actual model
quality, MTP acceptance and full-model Decode are not measured. Keep only
the standalone prototype; engine/default, CUDA/HIP and live binaries are
unchanged. See [the rank-pruning record](../bench/results/2026-10-15-a770-expert-rank-prune-v3/README.md).

A later isolated IQ2_XS full-model experiment on the same Windows A770/oneAPI
2025.3.2 keeps 5771 actual cache slots in six processes (10/9/8/8/9/10).
Top9/Top8 generated Decode throughput changes are **+2.506% / +4.859%**
across constructed Code2K/Prose2K/Long8K prompts. They are not uniform:
Top9 Prose changes -7.183%, and Top8 Code -4.498%. Fixed-token observations
are +5.954%/+12.064%, but the Code baseline drifts 3.912%, above the declared
3% gate, so the fixed-work aggregate is not confirmed. On 502 frozen
model-generated reference tokens, PPL increases 0.293%/0.676%; MTP-only
acceptance falls from 72.464% to 67.763%/64.333%, and generated text changes.
This small corpus is not a broad accuracy test. Keep the isolated approximate
candidate only; production defaults and API remain unchanged. See
[the full-model record](../bench/results/2026-10-15-a770-expert-prune-model-v3/README.md).

A subsequent isolated A770/oneAPI 2025.3.2 IQ2_XS SER specialization fuses
selection with device planning and skips omitted rows in combine, without
extra normal-path GPU submissions or retained-count CPU readbacks. Six
workers use the same 5771 actual cache slots. Fixed9 (`9,1`) and adaptive
`8,0.30` generated throughput changes are **+3.599% / +0.353%**; fixed-work
changes are +5.723%/+6.018%, with both 3% drift gates passing (maximum
0.306% fixed, 0.239% generated). Adaptive's routing-only threshold was
locked before timing. PPL on 502 reference tokens rises 0.293%/0.595%,
and MTP-only acceptance is 67.763%/70.741% versus 72.464% baseline.
Prose regresses -6.201%/-5.781%; outputs change and the small corpus is
not a broad quality test. Retain the default-OFF isolated candidate only;
do not recommend this adaptive threshold over fixed9. Native rank/weight,
GPU-reference combine and snapshot checks pass, and default/fixed9 scores
match the earlier policies exactly. No historical gains are added or
attributed solely to fusion. See
[the fused SER record](../bench/results/2026-10-15-a770-ser-v3/README.md).

A later isolated Windows A770/oneAPI 2025.3.2 IQ2_XS experiment profiles actual
expert-output perturbations on three new prompts, then freezes 24 layers to
retain Top8 and leaves the other 24 at Top10. The local squared-perturbation
proxy is 9.433% lower than global Top9, but reference PPL on 502 tokens rises
0.614% versus Top9's 0.293%. Raw fixed-work/generated throughput observations
are +6.038%/+5.474%, **not confirmed gains**: another session's `gdn_parity.exe`
overlaps the reverse Top9 worker, and baseline drift reaches 3.811% fixed and
6.650% generated, failing both declared 3% gates. All six samples are retained;
the contaminated Top9 aggregate is not used to declare a winner. Native and
offline audits pass, with identical repeated scores/IDs and 5771 actual cache
slots in every worker. Keep the profile tool/default-OFF isolated candidate;
do not recommend this mask or change production/CUDA/HIP. See
[the layer-output record](../bench/results/2026-10-15-a770-ser-layer-output/README.md).

A fresh six-worker retest reuses that sealed binary, configuration, corpus and
layer mask without rebuilding or recalibration. Layer8 **fixed-work throughput
improves 6.889%** (24.887 to 26.601 tok/s geometric mean); fixed9 improves
4.130%. No tracked competitors occur, all workers keep 5771 actual slots,
and maximum fixed-work baseline drift is 2.170%, passing the original 3% gate.
Generated changes of +7.480%/+4.381% for layer8/fixed9 remain **unconfirmed**:
Prose baseline elapsed time drifts 5.294%, so the generated timing gate fails.
Layer8's 0.614% reference-PPL penalty remains; scores and generated IDs match
the preceding cohort exactly. Keep the default-OFF isolated option, not a
default-policy change. The archived-config audit passes; it explicitly handles
another session's later shared-config update without restoring that file.
See [the independent retest](../bench/results/2026-10-15-a770-ser-layer-confirm/README.md).

IQ2_S GU16/SG16 now uses the existing aligned Multi<1> dot path when a whole
expert group has one entry. On the A770 with oneAPI 2025.3, same-process
A/B/A over 48 distinct 2560 x 640 IQ2_S/Q2_0 layers measures **+4.718%** T1
and **+0.585%** T4 VRAM expert-operator throughput, or **+2.631%** geometric
mean. Mapped-weight T1/T4 changes are +0.100%/-0.031%; these are separate
measurements, not cumulative or whole-model gains. All 57 format/shape cases
pass bitwise parity, and all 54 formal model outputs match. The other two
singleton implementations overlap this change and are not stacked.
`STRATA_GU_IQ2S_SINGLE_GROUP=0` restores the earlier dot path; the source
defaults on and the measured runtime requires explicit `=1`. The user now
accepts measured operator gains without a whole-model performance gate;
earlier decisions stay unchanged. See
[the additive adoption record](../bench/results/2026-10-13-a770-iq2s-single-group-adopt/README.md)
for runtime checks and deployment identity.

The accepted paired IQ2_S GU path now specializes input width 2560 on A770,
keeping its dot/accumulation order and the generic path for other widths.
On Windows A770, driver 32.0.101.9034, oneAPI 2025.3/Level Zero, independent
confirmation on 2026-10-15 measures **+0.832%** complete 48-layer synthetic
expert-chain VRAM throughput versus the already paired path: T1 **+0.647%**,
T4 **+1.019%**. Pinned-host changes are -0.166%/+0.074%, and maximum baseline
drift is 0.810%. Bitwise operator checks, emitted SG16 contracts and all 48
generated token IDs across three private-model prompts match. This is a
source-only adoption, not whole-model tok/s or an additional SER gain; no
production executable/configuration is replaced. See
[the fixed-width record](../bench/results/2026-10-15-a770-iq2s-gu-fixed-width/README.md).

The SYCL MTP drafter also has an opt-in native Q8_0 expert path:
`STRATA_MTP_Q8_EXPERTS=1` with a Q8 runtime selected by `--mtp`. On Windows
A770 16GB/oneAPI 2025.3.2, IQ2_XS, same-binary A/B/A at 4,717 cache slots
and the current probability thresholds measures **71.59% to 72.51%** MTP
acceptance across nine 128-token cases; all 1,152 output IDs match. With both
thresholds zero, acceptance is **52.79% to 55.61%**. Extra expert storage is
**1,875 MiB**, four cases regress in each comparison, and Q8 defaults off.
The current MTP translation unit compiles; runtime measurements use an isolated
executable with the deployed operators and matching ABI. Production configuration
is retained. See [the MTP record and packing steps](../bench/results/2026-10-15-a770-mtp-q8-experts/README.md).

## Reporting a problem

Open an issue with: the card, the driver version, `sycl-ls` output, the oneAPI version, the model and flags, and the
engine's stderr. Results from real cards are what move this from experimental to supported.
