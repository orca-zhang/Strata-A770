"""Recompute the independent rerun without replacing the earlier failed trial."""
import copy
import importlib.util
import json
from pathlib import Path
import statistics
import sys

R = Path(__file__).resolve().parents[4]
H = R / "build-sycl-a770-batch32-rerun-20261025"
F = R / "build-sycl-a770-batch32-production-20261025"
OUT = Path(__file__).with_name("results.json")
spec = importlib.util.spec_from_file_location("_frozen_batch32", F / "run.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
read, sha = m.read, m.p.sha


def hashes(values, root):
    for name, digest in values.items():
        assert sha(root / name) == digest, name


def analyze():
    experiment = read(H / "experiment.json")
    assert experiment["normal_priority_before_services"]
    assert experiment["historical_failure_unresolved"] and not experiment["automatic_deployment"]
    hashes(experiment["source_sha256"], R)
    built = read(F / "build.json")
    assert built["complete"] and built["check"] == {"checks": 464}
    hashes(built["source_sha256"], R)
    hashes(built["output_sha256"], F)
    # Also verify the accepted libraries/objects retained by both executables.
    inherited = read(R / "build-sycl-a770-production-xmx-v2-20261025/build.json")
    hashes({k: v for k, v in inherited["source_sha256"].items() if Path(k).suffix in (".obj", ".lib")}, R)
    result_path = H / "measure/results.json"
    finished = result_path.exists()
    data = read(result_path if finished else H / "measure/partial.json")
    assert data["complete"] == finished
    hashes(data["source_sha256"], R)
    if finished:
        hashes(data["artifact_sha256"], H / "measure")
    original = read(F / "measure/partial.json")
    assert data["cases"] == original["cases"] and data["expected"] == original["expected"]
    assert not original["complete"] and original["runs"][1]["rows"][-1].get("error")
    cfg = read(H / "production-config-before.json")
    assert (H / "production-config-before.json").read_bytes() == (R / "strata-iq2_xs.json").read_bytes()
    assert [run["label"] for run in data["runs"]] == ["A1", "B", "A2", "auto"][:len(data["runs"])]
    summaries, samples, bad = [], [], []
    warmups = [("tool255", True)] * 3 + [("tool258", True), ("long1k", True), ("long2k", True)]
    formal = [("long1k", False), ("long2k", False), ("tool258", False)] * 3
    for run in data["runs"]:
        label = run["label"]
        assert run["server_exit_code"] == 0 and run["threshold"] == 1024
        assert run["candidate"] == (label in ("B", "auto"))
        schedule = [("tool255", True), ("tool258", False), ("long1k", False), ("long2k", False)] if label == "auto" else warmups + formal
        assert [(row["case"], row["warmup"]) for row in run["rows"]] == schedule[:len(run["rows"])]
        assert not finished or len(run["rows"]) == len(schedule)
        expected = copy.deepcopy(cfg)
        if label != "auto":
            expected["args"][expected["args"].index("--expert-cache") + 1] = "5500"
            assert run["cache"][0] == "5771"
        assert int(run["cache"][0]) == run["metrics"]["engine"]["expert_slots"]
        if run["candidate"]:
            expected["exe"] = str(F / "bin/strata-unified-all-operators.exe")
            expected["env"]["STRATA_SYCL_BIN"] = str(Path(expected["exe"]).relative_to(R))
        expected["log"] = str(H / "measure" / label / "engine.log")
        assert read(H / "measure" / label / "config.json") == expected
        for row in run["rows"]:
            if row.get("error"):
                bad.append((label, row["case"], row["warmup"]))
            else:
                assert row["valid"]
                m.validate(row["case"], row, data["expected"].get(row["case"]))
                if label == "auto" and not row["warmup"]:
                    control = next(r for arm in data["runs"] if arm["label"] == "B" for r in arm["rows"] if r["case"] == row["case"] and not r["warmup"])
                    assert m.normalized(row["reply"]) == m.normalized(control["reply"])
            samples.append({"run": label, "case": row["case"], "warmup": row["warmup"], "valid": bool(row.get("valid")),
                "wall_ms": row["wall_ms"], "timings": row.get("timings"), "stop_reason": row.get("reply", {}).get("stop_reason")})
        for name in ("tool258", "long1k", "long2k"):
            rows = [row for row in run["rows"] if row["case"] == name and not row["warmup"] and row.get("valid")]
            if not rows:
                continue
            summaries.append({"run": label, "case": name, "requests": len(rows), "input_tokens": rows[0]["timings"]["prompt_n"],
                "output_tokens": sorted({row["timings"]["predicted_n"] for row in rows}),
                "http_ms": statistics.median(row["wall_ms"] for row in rows),
                "http_range_ms": [min(row["wall_ms"] for row in rows), max(row["wall_ms"] for row in rows)],
                "prefill_tps": statistics.median(row["timings"]["prompt_n"] * 1000 / row["timings"]["prompt_ms"] for row in rows),
                "decode_tps": statistics.median(row["timings"]["predicted_n"] * 1000 / row["timings"]["predicted_ms"] for row in rows)})
    evaluation = None
    if finished:
        assert not bad
        evaluation = m.evaluate(data)
        assert all(data[k] == v for k, v in evaluation.items())
        assert len(data["runs"]) == (4 if data["adoption_eligible"] else 3)
        assert data["auto_validation_passed"] == data["adoption_eligible"]
    else:
        assert len(bad) == 1 and data["runs"][-1]["rows"][-1].get("error")
    files = [Path(__file__).resolve(), H / "run_all.py", H / "experiment.json", F / "build.json",
             F / "measure/partial.json", result_path if finished else H / "measure/partial.json"]
    return {"date": "2026-10-25", "independent_rerun": True, "complete": finished,
        "settings": {"gpu": "Arc A77016GB", "cpu": "E5-2696v4", "ram_gib": 256, "pcie": "3.0x16", "os": "Windows", "model": "IQ2_XS",
            "workspace_mib": 32, "kv": "FP16", "mtp": 4, "short_read": 1024, "fixed_cache_slots": 5771},
        "requests": len(samples), "valid_requests": sum(row["valid"] for row in samples), "failed_requests": bad,
        "evaluation": evaluation, "summaries": summaries, "samples": samples,
        "reused_operator_checks": 464, "operator_checks_rerun": False,
        "historical_failure_preserved": True, "historical_failure_unresolved": True,
        "adopted": False, "production_unchanged": True,
        "limits": ["Only this independent run; original failed trial remains failed.", "The legacy adoption_eligible field means this run's timing/reply gates only; unresolved historical failure still prevents adoption.", "23/25-token retrieval decode is descriptive;276-token tool decode is the control.", "Automatic-cache requests, if present, are compatibility checks only."],
        "evidence_sha256": {f.relative_to(R).as_posix(): sha(f) for f in files}}


if __name__ == "__main__":
    report = analyze()
    if "--check" in sys.argv:
        assert report == read(OUT)
    else:
        assert not OUT.exists(), "Use --check for published evidence"
        m.p.save(OUT, report)
    print("PASS independent rerun: schedule, requests, replies, metrics, hashes and unchanged production")
