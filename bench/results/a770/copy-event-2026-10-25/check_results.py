"""Recompute the pinned DMA event HTTP screen and its read-only telemetry."""
import copy
import importlib.util
import json
from pathlib import Path
import statistics
import sys

R = Path(__file__).resolve().parents[4]
H = R / "build-sycl-a770-copy-event-20261025"
OUT = Path(__file__).with_name("results.json")
spec = importlib.util.spec_from_file_location("_copy_event_measure", H / "run.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
read, sha = m.read, m.p.sha


def hashes(values, root):
    for name, digest in values.items():
        assert sha(root / name) == digest, name


def telemetry(data):
    meta = read(H / "monitor.json")
    assert meta["read_only"] and meta["stopped_by_runner"] and meta["exit_code"] != 0
    lines = (H / "monitor.stdout").read_text().splitlines(keepends=True)
    if lines and not lines[-1].endswith("\n"):
        lines.pop()
    props = [dict(part.split("=", 1) for part in line.split()[1:]) for line in lines if line.startswith("FREQ_PROP ")]
    gpu = [p["index"] for p in props if p["error"] == "0" and p["type"] == "0"]
    memory = [p["index"] for p in props if p["error"] == "0" and p["type"] == "1"]
    assert len(gpu) == 1
    samples, current = [], None
    for line in lines:
        if line.startswith("SAMPLE "):
            fields = dict(part.split("=", 1) for part in line.split()[1:])
            current = {"ns": int(fields["host_ns"]), "temperatures": []}
        elif current is not None and line.startswith(("FREQ ", "TEMP ", "PCI ")):
            fields = dict(part.split("=", 1) for part in line.split()[1:])
            if line.startswith("FREQ ") and fields["index"] == gpu[0] and fields["error"] == "0":
                current.update(mhz=float(fields["actual_mhz"]), throttle=int(fields["throttle"]))
            elif line.startswith("FREQ ") and fields["index"] in memory and fields["error"] == "0":
                current["memory_mhz"] = float(fields["actual_mhz"])
            elif line.startswith("TEMP ") and fields["error"] == "0":
                current["temperatures"].append(float(fields["c"]))
            elif line.startswith("PCI ") and "mhz" in current:
                # ponytail: only complete native records; termination can interrupt the final record.
                assert meta["start_ns"] <= current["ns"] <= meta["end_ns"]
                samples.append(current)
                current = None
    result = []
    for run in data["runs"]:
        for row in run["rows"]:
            during = [s for s in samples if row["host_start_ns"] <= s["ns"] <= row["host_end_ns"]]
            assert during, (run["label"], row["case"], "no GPU sample during HTTP request")
            temperatures = [t for sample in during for t in sample["temperatures"]]
            memory_mhz = [s["memory_mhz"] for s in during if "memory_mhz" in s]
            result.append({"run": run["label"], "case": row["case"], "warmup": row["warmup"], "samples": len(during),
                "mhz_range": [min(s["mhz"] for s in during), max(s["mhz"] for s in during)],
                "memory_frequency_samples": len(memory_mhz),
                "memory_mhz_range": [min(memory_mhz), max(memory_mhz)] if memory_mhz else None,
                "throttle_observed": any(s["throttle"] != 0 for s in during),
                "temperature_c_range": [min(temperatures), max(temperatures)] if temperatures else None})
    return result


def analyze():
    built = read(H / "build.json")
    data = read(H / "measure/results.json")
    assert built["complete"] and built["check"] == {"checks": 48} and data["complete"]
    assert read(H / "queue-priority.json") == {"priority": 32, "before_build_and_services": True}
    hashes(built["source_sha256"], R)
    hashes(built["output_sha256"], H)
    hashes(data["source_sha256"], R)
    hashes(data["artifact_sha256"], H / "measure")
    baseline = R / "build-sycl-a770-xmx-batch-production-20261025"
    old_command = dict(read(baseline / "build.json")["commands"])["compile-prefill"]
    new_command = dict(built["commands"])["compile-prefill"]
    assert old_command.replace(str(baseline), "$OUT") == new_command.replace(str(H), "$OUT")
    assert (baseline / "include/strata/prefill/gemm.hpp").read_bytes() == (H / "include/strata/prefill/gemm.hpp").read_bytes()
    inherited = read(R / "build-sycl-a770-production-xmx-v2-20261025/build.json")
    hashes({k: v for k, v in inherited["source_sha256"].items() if Path(k).suffix in (".obj", ".lib")}, R)
    original = read(R / "build-sycl-a770-batch32-rerun-20261025/measure/results.json")
    assert data["cases"] == original["cases"] and data["expected"] == original["expected"]
    cfg = read(H / "production-config-before.json")
    assert (H / "production-config-before.json").read_bytes() == (R / "strata-iq2_xs.json").read_bytes()
    evaluation = m.evaluate(data)
    assert all(data[k] == value for k, value in evaluation.items())
    assert data["auto_validation_passed"] == data["adoption_eligible"]
    assert [run["label"] for run in data["runs"]] == ["A1", "B", "A2"] + (["auto"] if data["adoption_eligible"] else [])
    summaries, samples = [], []
    for run in data["runs"]:
        label = run["label"]
        assert run["server_exit_code"] == 0 and run["threshold"] == 1024
        assert run["candidate"] == (label in ("B", "auto"))
        assert int(run["cache"][0]) == run["metrics"]["engine"]["expert_slots"]
        expected = copy.deepcopy(cfg)
        if label != "auto":
            expected["args"][expected["args"].index("--expert-cache") + 1] = "5500"
            assert run["cache"][0] == "5771"
        if run["candidate"]:
            expected["exe"] = str(H / "bin/strata-unified-all-operators.exe")
            expected["env"]["STRATA_SYCL_BIN"] = str(Path(expected["exe"]).relative_to(R))
        expected["log"] = str(H / "measure" / label / "engine.log")
        assert read(H / "measure" / label / "config.json") == expected
        schedule = [("tool255", True), ("tool258", False), ("long1k", False), ("long2k", False)] if label == "auto" else \
            [("tool255", True)] * 3 + [("tool258", True), ("long1k", True), ("long2k", True)] + \
            [("long1k", False), ("long2k", False), ("tool258", False)] * 3
        assert [(row["case"], row["warmup"]) for row in run["rows"]] == schedule
        for row in run["rows"]:
            assert row["valid"] and not row.get("error") and row["host_end_ns"] > row["host_start_ns"]
            m.validate(row["case"], row, data["expected"].get(row["case"]))
            if label == "auto" and not row["warmup"]:
                control = next(r for arm in data["runs"] if arm["label"] == "B" for r in arm["rows"] if r["case"] == row["case"] and not r["warmup"])
                assert m.normalized(row["reply"]) == m.normalized(control["reply"])
            samples.append({"run": label, **{k: row[k] for k in ("case", "warmup", "wall_ms", "timings", "valid")}})
        for name in ("tool258", "long1k", "long2k"):
            rows = [row for row in run["rows"] if row["case"] == name and not row["warmup"]]
            summaries.append({"run": label, "case": name, "requests": len(rows), "input_tokens": rows[0]["timings"]["prompt_n"],
                "output_tokens": sorted({row["timings"]["predicted_n"] for row in rows}),
                "http_ms": statistics.median(row["wall_ms"] for row in rows),
                "http_range_ms": [min(row["wall_ms"] for row in rows), max(row["wall_ms"] for row in rows)],
                "prefill_tps": statistics.median(row["timings"]["prompt_n"] * 1000 / row["timings"]["prompt_ms"] for row in rows),
                "decode_tps": statistics.median(row["timings"]["predicted_n"] * 1000 / row["timings"]["predicted_ms"] for row in rows)})
    files = [Path(__file__).resolve(), H / "build.json", H / "measure/results.json", H / "integration.patch",
        H / "monitor.json", H / "monitor.stdout", H / "monitor.stderr", H / "queue-priority.json"]
    return {"date": "2026-10-25", "scope": built["scope"], "complete": True, "operator_checks": 48,
        "requests": len(samples), "valid_requests": len(samples), "summary": summaries, "samples": samples,
        "evaluation": evaluation, "auto_validation_passed": data["auto_validation_passed"], "telemetry": telemetry(data),
        "adopted": False, "production_unchanged": True,
        "settings": {"gpu": "Arc A770 16GB", "cpu": "Xeon E5-2696 v4", "ram_gib": 256, "pcie": "3.0x16",
            "os": "Windows", "model": "IQ2_XS", "workspace_mib": 32, "kv": "FP16", "mtp": 4,
            "expert_batch": 8, "short_read": 1024, "fixed_cache_slots": 5771, "prompt_cache": 0},
        "limits": ["One HTTP cohort; no retries or discarded observations.", "276-token tool decode is the control; retrieval decode is descriptive.",
            "Expert residency adapts throughout this fixed schedule; disabling prompt caching does not reset expert residency.",
            "Telemetry is sampled during complete HTTP requests, not exact GPU phase boundaries. Sampling can miss brief changes; overhead is not independently quantified.",
            "No GPU event timing, clock/power settings, or automatic deployment. Auto-cache samples, if present, establish compatibility only."],
        "evidence_sha256": {f.relative_to(R).as_posix(): sha(f) for f in files}}


if __name__ == "__main__":
    report = analyze()
    if "--check" in sys.argv:
        assert report == read(OUT)
    else:
        assert not OUT.exists(), "Use --check for published evidence"
        m.p.save(OUT, report)
    print("PASS two-queue checks, HTTP requests/replies/metrics, telemetry, hashes and unchanged production")
