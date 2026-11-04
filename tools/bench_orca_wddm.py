#!/usr/bin/env python3
"""Bracket the direct WDDM check with the same harness built against Orca L0.

Both executables use the same zebin, geometry, inputs and CPU/guard checks.
Build orca-l0-wddm-memory-check (or orca-l0-dg2-bench) and the baseline first.
The benchmark is serial and shares Strata's GPU experiment lock.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

from bench_deepinfer import ROOT, gpu_lock, save, sha


def run(exe, workload, image, wait, stem, env, baseline, candidate_backend="fixed"):
    args = [str(exe), "--bench", workload, str(image), wait]
    start = time.perf_counter()
    p = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, timeout=120)
    stem.with_suffix(".stdout").write_bytes(p.stdout)
    stem.with_suffix(".stderr").write_bytes(p.stderr)
    if p.returncode:
        raise RuntimeError(f"{stem.name}: exit {p.returncode}; see its stdout/stderr")
    output = p.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n")
    samples = []
    for match in re.finditer(r"^SAMPLE (\d+) ([\d.]+) ([\d.]+)(?: (\d+))?$", output, re.M):
        if int(match[1]) != len(samples):
            raise RuntimeError("missing, duplicate or reordered sample")
        pair = [float(match[2]), float(match[3])]
        if not all(math.isfinite(v) and v > 0 for v in pair) or pair[1] < pair[0]:
            raise RuntimeError("invalid timing sample")
        if match[4] is not None:
            cycles = int(match[4])
            if cycles <= 0:
                raise RuntimeError("invalid thread-cycle sample")
            pair.append(cycles)
        samples.append(pair)
    if len(samples) != 512 or "PASS 640 GPU" not in output:
        raise RuntimeError("incomplete workload or correctness checks")
    marker = ("PASS Level Zero baseline and cleanup" if baseline else
              "PASS reusable DG2 runtime and cleanup" if candidate_backend == "dg2" else
              "PASS WDDM allocation/residency/cleanup; no Level Zero/Intel UMD/compiler/GMM loaded")
    if marker not in output:
        raise RuntimeError("wrong backend or missing dependency/cleanup check")
    native_wait = re.search(r"native_wait_calls=(\d+)", output)
    return dict(args=args, seconds=time.perf_counter() - start,
                native_wait_calls=int(native_wait[1]) if native_wait else None,
                submit_median_us=statistics.median(x[0] for x in samples),
                complete_median_us=statistics.median(x[1] for x in samples), samples=samples)


def compare(a0, candidate, a1, metric):
    before, after = a0[metric], a1[metric]
    baseline = math.sqrt(before * after)
    return dict(baseline_us=baseline, candidate_us=candidate[metric],
                latency_change_pct=100 * (candidate[metric] / baseline - 1),
                control_drift_pct=100 * (after / before - 1))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--baseline", type=Path, required=True)
    p.add_argument("--candidate", type=Path, required=True)
    p.add_argument("--image", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--driver", help="Absolute installed ze_intel_gpu64.dll path for the direct-DDI control")
    p.add_argument("--rounds", type=int, default=3)
    p.add_argument("--candidate-backend", choices=("fixed", "dg2"), default="fixed")
    p.add_argument("--baseline-backend", choices=("l0", "dg2"), default="l0")
    p.add_argument("--wait-mode", action="append", choices=("native", "poll", "hybrid50", "hybrid100"))
    p.add_argument("--workload", action="append", choices=("increment", "pipeline", "chain400"))
    args = p.parse_args()
    if args.rounds < 1:
        p.error("--rounds must be positive")
    wait_modes = tuple(dict.fromkeys(args.wait_mode or ("native", "poll")))
    workloads = tuple(dict.fromkeys(args.workload or ("increment", "pipeline", "chain400")))
    for key in ("baseline", "candidate", "image"):
        path = getattr(args, key).resolve(strict=True)
        if not path.is_file():
            p.error(f"--{key} must be a file")
        setattr(args, key, path)
    args.out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    for key in ("ORCA_L0_MODULE_TRACE", "ORCA_L0_POISON_ALLOCATIONS"):
        env.pop(key, None)
    if args.driver:
        env["ORCA_L0_DRIVER"] = str(Path(args.driver).resolve(strict=True))
    report = dict(complete=False, rounds=args.rounds,
                  candidate_backend=args.candidate_backend,
                  baseline_backend=args.baseline_backend,
                  driver_path=env.get("ORCA_L0_DRIVER"),
                  driver_sha256=sha(env["ORCA_L0_DRIVER"]) if env.get("ORCA_L0_DRIVER") else None,
                  image_sha256=sha(args.image),
                  executable_sha256={k: sha(getattr(args, k)) for k in ("baseline", "candidate")},
                  source_sha256={name: sha(ROOT / name) for name in
                                 ("tools/bench_orca_wddm.py", "orca-l0/tests/wddm_memory_check.cpp", "orca-l0/src/drm/encoding.hpp",
                                  "orca-l0/src/wddm/runtime.cpp", "orca-l0/src/drm/recording.inl", "orca-l0/src/drm/arguments.hpp",
                                  "orca-l0/src/drm/zebin.hpp", "orca-l0/include/orca_l0/dg2.hpp", "orca-l0/CMakeLists.txt",
                                  "orca-l0/third_party/intel-dg2/packets.hpp", "orca-l0/src/runtime.cpp",
                                  "orca-l0/src/direct_driver.cpp", "orca-l0/include/orca_l0/runtime.hpp")},
                  warmups=128, retained=512, order=["baseline", "candidate", "baseline"],
                  wait_modes=wait_modes, workloads=workloads,
                  sample_columns=["submit_us", "complete_us", "thread_cycles (new harness only)"],
                  timing_excludes=["setup", "allocation", "module load", "recording", "parameter update", "CPU/guard checks"],
                  scope="Windows dispatch microbenchmarks; not full Strata token throughput",
                  caveats=["Both data heaps are resident CPU-visible system memory; allocations are separate.",
                           "ISA placement, cache policy, state programming and native wait implementation differ.",
                           ("Both DG2 arms use the same native wait implementation." if args.baseline_backend == "dg2" else
                            "Native wait uses an OS event on WDDM and the driver's fence wait on L0."),
                           "Hybrid budgets 50/100 us after submit, checked every 32 polls; OS scheduling can extend wall time.",
                           "Thread cycles include timer/counter overhead and are CPU execution cost, not wall time or a power measurement.",
                           "Poll uses pause loops with bounded completion checks on both backends."],
                  runs=[], pairs=[])
    report_path = args.out / "results.json"
    save(report_path, report)
    try:
        with gpu_lock():
            for round_index in range(args.rounds):
                for wait in wait_modes:
                    for workload in workloads:
                        group = []
                        for arm in ("a0", "b", "a1"):
                            label = f"r{round_index}-{wait}-{workload}-{arm}"
                            print("RUN", label, flush=True)
                            exe = args.candidate if arm == "b" else args.baseline
                            backend = args.candidate_backend if arm == "b" else args.baseline_backend
                            result = run(exe, workload, args.image, wait, args.out / label, env, backend == "l0", backend)
                            result.update(label=label, round=round_index, wait=wait, workload=workload, arm=arm)
                            report["runs"].append(result)
                            group.append(result)
                            save(report_path, report)
                        pair = dict(round=round_index, wait=wait, workload=workload)
                        for metric in ("submit_median_us", "complete_median_us"):
                            pair[metric] = compare(*group, metric)
                        report["pairs"].append(pair)
                        save(report_path, report)
            report["summary"] = []
            for wait in wait_modes:
                for workload in workloads:
                    pairs = [x for x in report["pairs"] if x["wait"] == wait and x["workload"] == workload]
                    summary = dict(wait=wait, workload=workload)
                    for metric in ("submit_median_us", "complete_median_us"):
                        values = [x[metric] for x in pairs]
                        stable = [x for x in values if abs(x["control_drift_pct"]) <= 3]
                        summary[metric] = dict(
                            baseline_us=statistics.median(x["baseline_us"] for x in values),
                            candidate_us=statistics.median(x["candidate_us"] for x in values),
                            latency_change_pct=statistics.median(x["latency_change_pct"] for x in values),
                            max_abs_control_drift_pct=max(abs(x["control_drift_pct"]) for x in values),
                            stable_pairs=len(stable),
                            stable_latency_change_pct=statistics.median(x["latency_change_pct"] for x in stable) if len(stable) >= 3 else None)
                    report["summary"].append(summary)
                    print("RESULT", json.dumps(summary), flush=True)
            report["complete"] = True
            save(report_path, report)
    except BaseException as error:
        report["error"] = repr(error)
        save(report_path, report)
        raise


if __name__ == "__main__":
    main()
