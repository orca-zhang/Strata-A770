"""Real Strata A/B/A check of the verifier bridge or a separate native executable.

python tools/bench_orca_strata.py --exe <experimental.exe> --config <config.json> --out <new-directory>
Without --candidate-exe, the executable needs STRATA_SYCL_ORCA_L0=ON.
With --candidate-exe, compare a SYCL baseline against fully native Orca.
Use --mtp-only to disable the suffix policy's timing-dependent window choices.
Uses the shared GPU lease; timings are invalid if outputs/workload differ.
"""
import argparse
import json
import math
from pathlib import Path
import statistics
import time

import bench_deepinfer as bench


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--candidate-exe", type=Path, help="Fully native Orca executable")
    ap.add_argument("--images", type=Path, help="Native kernel images; pin and audit these files during the comparison")
    ap.add_argument("--config", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--spec", default="1,4")
    ap.add_argument("--cases", default="Short,Code2K")
    ap.add_argument("--tokens", type=int, default=64)
    ap.add_argument("--retained", type=int, default=3)
    ap.add_argument("--expert-cache", type=int, default=6000)
    ap.add_argument("--mtp-only", action="store_true", help="Disable suffix lookup and timing-based MTP window selection")
    args = ap.parse_args()
    widths = [int(x) for x in args.spec.split(",")]
    if not widths or len(set(widths)) != len(widths) or any(x < 1 or x > 8 for x in widths) or args.tokens < 2 or args.retained < 1:
        ap.error("spec must be 1..8, tokens >= 2 and retained >= 1")
    if args.images and (not args.candidate_exe or not args.images.is_dir()):
        ap.error("--images requires --candidate-exe and an existing image directory")
    cfg = json.loads(args.config.read_text(encoding="utf-8"))
    inputs = bench.prompts(cfg)
    cases = args.cases.split(",")
    if len(set(cases)) != len(cases) or any(x not in inputs for x in cases):
        ap.error("unknown prompt case")
    args.out.mkdir(parents=True, exist_ok=False)
    result = dict(complete=False, exe=str(args.exe.resolve()), exe_sha256=bench.sha(args.exe),
                  config_sha256=bench.sha(args.config), input_ids={name: inputs[name] for name in cases},
                  tokens=args.tokens, retained=args.retained, mtp_only=args.mtp_only, runs=[], comparisons=[], metrics=[])
    if args.candidate_exe:
        result.update(candidate_exe=str(args.candidate_exe.resolve()),
                      candidate_sha256=bench.sha(args.candidate_exe), backend="native")
    save = lambda: bench.save(args.out / "results.json", result)
    save()
    with bench.gpu_lock():
        if args.images:
            image_hashes = {p.relative_to(args.images).as_posix(): bench.sha(p)
                            for p in args.images.rglob('*') if p.is_file()}
            if not image_hashes:
                raise RuntimeError("empty native image directory")
            result.update(images=str(args.images.resolve()), image_sha256=image_hashes)
            save()
        for width in widths:
            launch = bench.model_args(cfg, cache_mib=args.expert_cache)
            # Native packs require a capacity >= 2. With no drafter or suffix lookup, every decode window is T=1.
            launch[launch.index("--spec") + 1] = str(max(2, width))
            if width == 1:
                if "--mtp" in launch:
                    at = launch.index("--mtp")
                    del launch[at:at + 2]
            if width == 1 or args.mtp_only:
                if "--suffix-draft" in launch:
                    launch[launch.index("--suffix-draft") + 1] = "0"
                else:
                    launch.extend(["--suffix-draft", "0"])
            reference = {}
            arms = []
            for label, active in (("A0", False), ("B", True), ("A1", False)):
                env = bench.environment(cfg)
                env.pop("STRATA_VERIFY_EAGER", None)
                env.pop("ORCA_L0_KERNEL_RESOURCES", None)
                env.pop("STRATA_DECODE_TIMING", None)
                env.pop("STRATA_VERIFY_PROFILE", None)
                if args.mtp_only:
                    env["STRATA_MTP_COST_WINDOW"] = "0"
                # Keep graph construction identical in both arms; Orca borrows this self-contained compute list.
                env.update(UR_L0_USE_IMMEDIATE_COMMANDLISTS="1", UR_L0_CMD_BUFFER_USE_IMMEDIATE_APPEND_PATH="1",
                           UR_L0_USE_DRIVER_INORDER_LISTS="1", UR_L0_USE_DRIVER_COUNTER_BASED_EVENTS="1")
                env["STRATA_ORCA_L0"] = str(int(active))
                executable = args.candidate_exe if active and args.candidate_exe else args.exe
                if active and args.candidate_exe:
                    env = {key: value for key, value in env.items()
                           if not key.startswith(("SYCL_", "UR_", "ONEAPI_"))}
                    if args.images:
                        env["STRATA_ORCA_IMAGES"] = str(args.images.resolve())
                stem = args.out / f"spec{width}-{label}"
                run = dict(label=label, orca=active, spec=width, exe=str(executable.resolve()), args=launch, rows=[])
                result["runs"].append(run)
                save()
                print("RUN", width, label, flush=True)
                engine = bench.Engine(executable.resolve(), launch, env, stem)
                try:
                    run["info"] = engine.info
                    # Same warmup and request history in all processes; compare every generated token.
                    for iteration in range(args.retained + 1):
                        for name in cases:
                            started = time.perf_counter()
                            row = engine.gen(inputs[name], args.tokens, keys="temperature=0")
                            row["wall_ms"] = (time.perf_counter() - started) * 1000
                            row.update(case=name, iteration=iteration, retained=iteration > 0)
                            run["rows"].append(row)
                            key = (iteration, name)
                            # These fixed text prompts have nonzero references; a zero stream
                            # is an invalid benchmark workload, not a numerical diagnosis.
                            if row["tokens"] and not any(row["tokens"]):
                                raise RuntimeError(f"all-zero benchmark output: spec={width} {label} {key}")
                            workload = {k: row[k] for k in ("tokens", "accepted", "offered", "reused", "finish")}
                            if label == "A0":
                                reference[key] = workload
                            elif workload != reference[key]:
                                raise RuntimeError(f"output/workload mismatch: spec={width} {label} {key}")
                            if row["generated"] < 2 or row["decode_ms"] <= 0:
                                raise RuntimeError(f"insufficient decode work: spec={width} {label} {key}")
                            if width == 1 and row["offered"] != 0:
                                raise RuntimeError("single-token case unexpectedly offered drafts")
                            save()
                finally:
                    engine.close()
                    run["exit_code"] = engine.p.returncode
                    save()
                log = stem.with_suffix(".stderr").read_text(encoding="utf-8")
                witness = "native Orca Level Zero" if args.candidate_exe else "Orca L0 native graph replay active"
                run["native_replay_witness"] = witness in log
                if run["native_replay_witness"] != active:
                    raise RuntimeError("requested runtime path did not match execution log")
                arms.append(run)
            for name in cases:
                medians = [statistics.median(row["decode_ms"] for row in arm["rows"]
                           if row["case"] == name and row["retained"]) for arm in arms]
                pair = bench.paired(*medians)
                result["comparisons"].append(dict(spec=width, case=name, median_decode_ms=medians, **pair))
                print("RESULT", result["comparisons"][-1], flush=True)
                for metric in ("prompt_ms", "decode_ms", "ttft_ms", "wall_ms"):
                    medians = [statistics.median(row[metric] for row in arm["rows"]
                               if row["case"] == name and row["retained"]) for arm in arms]
                    if any(value is None or not math.isfinite(value) or value < 0 or
                           (value == 0 and metric != "prompt_ms") for value in medians):
                        raise RuntimeError(f"invalid {metric} samples: {width} {name}")
                    measured = all(value > 0 for value in medians)
                    metric_row = dict(spec=width, case=name, metric=metric, median_ms=medians,
                                      comparison_valid=measured)
                    if measured:
                        metric_row.update(bench.paired(*medians))
                    else:
                        metric_row["reason"] = "zero-duration prefill; no percentage comparison"
                    result["metrics"].append(metric_row)
            save()
        result["exe_unchanged"] = bench.sha(args.exe) == result["exe_sha256"]
        if args.candidate_exe:
            result["candidate_unchanged"] = bench.sha(args.candidate_exe) == result["candidate_sha256"]
        if args.images:
            current = {p.relative_to(args.images).as_posix(): bench.sha(p)
                       for p in args.images.rglob('*') if p.is_file()}
            result["images_unchanged"] = current == image_hashes
        result["config_unchanged"] = bench.sha(args.config) == result["config_sha256"]
        save()
        if not all(value for key, value in result.items() if key.endswith("_unchanged")):
            raise RuntimeError("benchmark executable, images or config changed during the run")
    result["complete"] = True
    save()


if __name__ == "__main__":
    main()
