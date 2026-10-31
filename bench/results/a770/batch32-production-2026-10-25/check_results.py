"""Audit the rejected batch32 trial and its separate correctness reproduction."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

R = Path(__file__).resolve().parents[4]
F = R / "build-sycl-a770-batch32-production-20261025"
D = R / "build-sycl-a770-batch32-diagnose-20261025"
OUT = Path(__file__).with_name("results.json")
read = lambda path: json.loads(path.read_text(encoding="utf-8"))
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()


def load_measure():
    spec = importlib.util.spec_from_file_location("_rejected_batch32", F / "run.py")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def verify_hashes(data, root):
    for name, digest in data.items():
        assert sha(root / name) == digest, name


def analyze():
    m = load_measure()
    build, failed = read(F / "build.json"), read(F / "measure/partial.json")
    assert build["complete"] and build["check"] == {"checks": 464}
    verify_hashes(build["source_sha256"], R)
    verify_hashes(build["output_sha256"], F)
    verify_hashes(failed["source_sha256"], R)
    assert not failed["complete"] and not (F / "measure/results.json").exists()
    assert [run["label"] for run in failed["runs"]] == ["A1", "B"]
    warmups = ["tool255"] * 3 + ["tool258", "long1k", "long2k"]
    schedule = warmups + ["long1k", "long2k", "tool258"] * 3
    for run, expected in zip(failed["runs"], (schedule, warmups)):
        assert run["server_exit_code"] == 0 and run["cache"][0] == "5771"
        assert [row["case"] for row in run["rows"]] == expected
    rows = [row for run in failed["runs"] for row in run["rows"]]
    assert len(rows) == 21
    bad = [row for row in rows if row.get("error")]
    assert len(bad) == 1 and bad[0] is failed["runs"][1]["rows"][-1]
    for row in rows:
        if row is bad[0]:
            continue
        assert row["valid"] and not row.get("error")
        m.validate(row["case"], row, failed["expected"].get(row["case"]))
    broken = bad[0]
    reply = broken["reply"]
    text = "".join(block["text"] for block in reply["content"] if block["type"] == "text")
    assert text == "!" * 128 and reply["stop_reason"] == "max_tokens"
    assert broken["case"] == "long2k" and broken["warmup"]
    assert broken["timings"]["prompt_n"] == 2069 and broken["timings"]["predicted_n"] == 128
    # ponytail: the existing HTTP validator is also the regression check for collapse/partial/wrong replies.
    correct = next(row for row in rows if row["case"] == "long2k" and row.get("valid"))
    for kind in ("collapse", "stop", "answer"):
        sample = copy.deepcopy(broken if kind == "collapse" else correct)
        if kind == "stop":
            sample["reply"]["stop_reason"] = "max_tokens"
        if kind == "answer":
            sample["reply"]["content"] = [{"type": "text", "text": "{}"}]
        try:
            m.validate("long2k", sample, failed["expected"]["long2k"])
        except (AssertionError, ValueError):
            continue
        raise AssertionError("validator accepted " + kind)
    control_build, diagnosis = read(D / "build.json"), read(D / "measure/results.json")
    assert control_build["complete"] and diagnosis["complete"]
    verify_hashes(control_build["source_sha256"], R)
    verify_hashes(control_build["output_sha256"], D)
    verify_hashes(diagnosis["source_sha256"], R)
    verify_hashes(diagnosis["artifact_sha256"], D / "measure")
    assert sha(D / "prefill.cpp") == sha(R / "build-sycl-a770-xmx-batch-production-20261025/prefill.cpp")
    assert diagnosis["cases"] == failed["cases"] and diagnosis["expected"] == failed["expected"]
    assert [run["label"] for run in diagnosis["runs"]] == ["recompiled8", "original32"]
    status = []
    for run in diagnosis["runs"]:
        assert run["server_exit_code"] == 0 and run["cache"][0] == "5771"
        assert [row["case"] for row in run["rows"]] == warmups[:len(run["rows"])]
        assert len(run["rows"]) == 6 or run["rows"][-1].get("error")
        for row in run["rows"]:
            if row.get("error"):
                continue
            assert row["valid"] and row["normalized_reply_equal"]
            m.validate(row["case"], row, diagnosis["expected"].get(row["case"]))
        status.append(all(row.get("valid") and not row.get("error") for row in run["rows"]) and len(run["rows"]) == 6)
    outcome = ("batch32_failure_reproduced_after_recompiled8_passed" if status == [True, False] else
               "not_reproduced_in_six_request_replay" if all(status) else "recompiled8_also_failed_root_cause_unresolved")
    before = F / "production-config-before.json"
    assert before.read_bytes() == (R / "strata-iq2_xs.json").read_bytes() == (D / "measure/production-config-before.json").read_bytes()
    samples = lambda data: [{"run": run["label"], "case": row["case"], "valid": bool(row.get("valid")),
        "wall_ms": row["wall_ms"], "timings": row.get("timings"), "stop_reason": row.get("reply", {}).get("stop_reason")}
        for run in data["runs"] for row in run["rows"]]
    paths = [F / "build.json", F / "check.log", F / "integration.patch", F / "measure/partial.json",
             D / "build.json", D / "measure/results.json", R / "strata-iq2_xs.json", Path(__file__).resolve()]
    return {"candidate": "prefill expert wait interval8 to32; unchanged HC/GEMM arithmetic and32MiB workspace",
        "hardware": "Intel Arc A77016GB, E5-2696v4, PCIe3.0x16, Windows, DPC++2025.3.2/oneMKL2025.3",
        "adopted": False, "production_unchanged": True, "operator_checks": 464,
        "original_http": {"requests": 21, "valid": 20, "invalid": 1, "candidate_formal_samples": 0,
            "failure": "2K warmup returned128 exclamation marks and max_tokens instead of complete JSON", "samples": samples(failed)},
        "correctness_reproduction": {"outcome": outcome, "recompiled8_passed": status[0], "original32_passed": status[1], "samples": samples(diagnosis)},
        "performance_conclusion": "No valid paired HTTP/prefill/decode benefit. Failed trial is preserved; correctness replay never substitutes for performance samples.",
        "evidence_sha256": {path.relative_to(R).as_posix(): sha(path) for path in paths}}


if __name__ == "__main__":
    report = analyze()
    if "--check" in sys.argv:
        assert report == read(OUT)
    else:
        assert not OUT.exists(), "Use --check for published evidence"
        OUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("PASS rejected trial, correctness replay, response validator, hashes and unchanged production")
