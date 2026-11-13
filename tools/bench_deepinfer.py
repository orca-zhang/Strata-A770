#!/usr/bin/env python3
"""Serial DeepInfer validation/measurement, using isolated baseline/candidate CMake builds.

python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023 --phase checks
Use a fresh --out for every repetition. No production executable/config is changed.
"""
import argparse
import contextlib
import hashlib
import json
import math
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
SCRIPT_SOURCE = Path(__file__).read_bytes()
FLAGS = ("STRATA_NO_BATCH_KV_STEP", "STRATA_MTP_COST_WINDOW", "STRATA_MTP_CHAIN_SUBMIT", "STRATA_QSA_FUSED_KV_STORE")
MODES = {"baseline": {FLAGS[0]: "1"}, "batch": {FLAGS[0]: "0"},
         "cost": {FLAGS[0]: "1", FLAGS[1]: "1"},
         "fused": {FLAGS[0]: "1", FLAGS[3]: "1"},
         "chain": {FLAGS[0]: "1", FLAGS[2]: "1"},
         "combined": {FLAGS[0]: "0", FLAGS[1]: "1", FLAGS[2]: "1", FLAGS[3]: "1"}}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for data in iter(lambda: f.read(1024*1024), b""):
            h.update(data)
    return h.hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding="utf-8")


def busy(ignore_pid=0, include_compiler=True):
    if os.name == "nt":
        names = "strata.*|icx.*|clang.*|llama.*" if include_compiler else "strata.*|llama.*"
        p = subprocess.run(["powershell", "-NoProfile", "-Command",
                            "Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(" + names + ")\\.exe$' -and $_.ProcessId -ne " + str(int(ignore_pid)) + " } | Select-Object Name,ProcessId | ConvertTo-Json"],
                           capture_output=True, text=True, check=True, timeout=30)
        if p.stdout.strip():
            raise RuntimeError("GPU/compiler busy: " + p.stdout)


@contextlib.contextmanager
def gpu_lock(allow_compiler=False):
    # Share the workspace's experiment lock with concurrent A770 measurements.
    path = ROOT / "gpu-experiment.lock"
    owner = f"{os.getpid()} deepinfer"
    last = None
    while True:
        try:
            with path.open("x") as f:
                f.write(owner)
            break
        except FileExistsError:
            try:
                state = path.read_text()
            except (FileNotFoundError, PermissionError):
                time.sleep(0.05)
                continue
            if state != last:
                print("WAIT", state, flush=True)
                last = state
            time.sleep(0.05)
    try:
        # Reserve the shared slot before waiting for an older process/compiler to finish.
        while True:
            try:
                busy(include_compiler=not allow_compiler)
                break
            except RuntimeError as e:
                if str(e) != last:
                    print("WAIT", str(e), flush=True)
                    last = str(e)
                time.sleep(1)
        yield
    finally:
        while True:
            try:
                if path.read_text() != owner:
                    break
                path.unlink()
                break
            except FileNotFoundError:
                break
            except PermissionError:
                time.sleep(0.05)


def environment(cfg, mode="baseline", reproducible=False):
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("STRATA_", "SYCL_", "UR_", "ONEAPI_", "ZES_"))}
    env.update(cfg.get("env", {}))
    for k in FLAGS:
        env.pop(k, None)
    # Profiling changes synchronization; measurements must leave it disabled.
    for k in ("STRATA_VERIFY_PROFILE", "STRATA_DECODE_TIMING", "STRATA_STATE_HASH", "STRATA_STATE_HASH_GDN"):
        env.pop(k, None)
    env.update(MODES[mode])
    env["STRATA_MTP_CUM_MIN_P"] = "0"  # Keep the concurrent confidence-threshold experiment out of this comparison.
    if reproducible:
        env["STRATA_IQ_MT_MIN"] = "1"
    env["PATH"] = os.pathsep.join(cfg.get("lib_dirs", [])) + os.pathsep + env.get("PATH", "")
    return env


def paired(a, b, c):
    base = math.sqrt(a*c)
    return {"baseline": base, "candidate": b, "throughput_change_pct": 100*(base/b-1),
            "baseline_drift_pct": 100*(c/a-1)}


class Engine:
    def __init__(self, exe, args, env, stem):
        busy(include_compiler=env.get("STRATA_DECODE_TIMING") == "1")
        self.log = stem.with_suffix(".stderr").open("x", encoding="utf-8")
        self.protocol = stem.with_suffix(".protocol").open("x", encoding="utf-8")
        self.lines = queue.Queue()
        self.process_started = time.perf_counter()
        self.p = subprocess.Popen([str(exe), "--serve", *args], cwd=ROOT, env=env,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
                                  text=True, encoding="utf-8", errors="replace", bufsize=1)
        self.timing = env.get("STRATA_DECODE_TIMING") == "1"
        def read():
            for line in self.p.stdout:
                stamp = time.perf_counter()
                self.protocol.write(line)
                self.protocol.flush()
                self.lines.put((stamp, line.strip()))
            self.lines.put((time.perf_counter(), None))
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.info = ""
        while True:
            stamp, line = self.next()
            if line.startswith("INFO "):
                self.info = line
            if line.startswith("READY "):
                self.ready_ms = 1000*(stamp-self.process_started)
                break

    def next(self):
        stamp, line = self.lines.get(timeout=1200)
        if line is None or line.startswith("ERR "):
            raise RuntimeError("engine failed: " + str(line))
        return stamp, line

    def send(self, text):
        self.p.stdin.write(text + "\n")
        self.p.stdin.flush()

    def gen(self, ids, n=128, keys="", cancel=False):
        if self.timing:
            busy(self.p.pid)
        started = time.perf_counter()
        self.send(f"GEN {n} {keys} " + ",".join(map(str, ids)))
        tokens, ttft, resume = [], None, 0
        first_delivery_tokens, last_delivery_ms = 0, None
        while True:
            stamp, line = self.next()
            if line.startswith(("T ", "TB ")):
                if ttft is None:
                    ttft = 1000*(stamp-started)
                    first_delivery_tokens = len(line.split()) - (1 if line.startswith("T ") else 2)
                last_delivery_ms = 1000*(stamp-started)
                tokens.extend(map(int, line.split()[1:] if line.startswith("T ") else line.split()[2:]))
                if cancel:
                    self.send("STOP")
                    cancel = False
            elif line.startswith("RESUME "):
                resume = int(line.split()[1])
            elif line.startswith("DONE "):
                if self.timing:
                    busy(self.p.pid)
                p = line.split()
                assert len(tokens) == int(p[1])
                delivery_span_ms = None if ttft is None else last_delivery_ms-ttft
                return {"tokens": tokens, "generated": int(p[1]), "prompt_tokens": int(p[2]),
                        "prompt_ms": float(p[3]), "decode_ms": float(p[4]), "finish": p[5],
                        "accepted": int(p[6]), "offered": int(p[7]), "reused": int(p[8]),
                        "resume": resume, "ttft_ms": ttft, "done": line,
                        "request_ms": 1000*(stamp-started), "first_delivery_tokens": first_delivery_tokens,
                        "last_delivery_ms": last_delivery_ms, "delivery_span_ms": delivery_span_ms,
                        "post_first_delivery_tok_s": 1000*(len(tokens)-first_delivery_tokens)/delivery_span_ms
                            if delivery_span_ms is not None and delivery_span_ms > 0 else None}

    def close(self):
        try:
            if self.p.poll() is None:
                with contextlib.suppress(OSError):
                    self.send("QUIT")
            self.p.wait(timeout=60)
            if self.p.returncode:
                raise RuntimeError(f"engine exit {self.p.returncode}")
        finally:
            if self.p.poll() is None:
                self.p.kill()  # only the child created by this measurement
                self.p.wait()
            self.reader.join(timeout=5)
            self.log.close()
            self.protocol.close()


def prompts(cfg):
    # Reuse the repository's measured 2K/8K inputs; no tokenization or dependency change.
    path = ROOT / "bench/results/2026-10-13-a770-iq2s-single-group-adopt/screen.in"
    large = []
    for line in path.read_text().splitlines():
        if line.startswith("GEN "):
            ids = list(map(int, line.split(" ", 2)[2].split(",")))
            if ids not in large:
                large.append(ids)
    assert list(map(len, large)) == [2048, 2048, 8000]
    sys.path.insert(0, str(ROOT))
    from strata_tokenizer import Tokenizer
    from serve.frontend import ChatTemplate, openai_to_messages
    directory = Path(cfg["tokenizer"])
    vocab = json.loads((directory/"vocab.json").read_text(encoding="utf-8"))
    tokens = [None]*len(vocab)
    for token, number in vocab.items():
        tokens[number] = token
    tokenizer = Tokenizer(tokens, (directory/"merges.txt").read_text(encoding="utf-8").splitlines(),
                          json.loads((directory/"token_type.json").read_text(encoding="utf-8")))
    template = ChatTemplate(directory/"chat_template.jinja")
    messages, tools, kwargs = openai_to_messages({"messages": [{"role": "user", "content": "Explain why the sky is blue in three sentences."}], "reasoning_effort": "none"})
    short = tokenizer.encode(template.render(messages, tools, **kwargs), parse_special=True)
    messages, tools, kwargs = openai_to_messages({"messages": [{"role": "user", "content": "请用三句话解释天空为什么是蓝色的。"}], "reasoning_effort": "none"})
    chinese = tokenizer.encode(template.render(messages, tools, **kwargs), parse_special=True)
    return dict(zip(("Short", "Code2K", "Prose2K", "Long8K", "Chinese"), [short, *large, chinese]))


def model_args(cfg, reproducible=False, threshold=.5, cache_mib=4000):
    args = cfg["args"].copy()
    changes = {"--expert-cache": str(cache_mib), "--spec-min-p": str(threshold)}
    if reproducible:
        changes.update({"--adapt-swaps": "0", "--pcie-frac": "0"})
    for k, v in changes.items():
        args[args.index(k)+1] = v
    return args


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-root", type=Path, required=True)
    ap.add_argument("--phase", choices=["checks", "operators", "model", "functional", "self-test"], required=True)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--config", type=Path)
    ap.add_argument("--modes", default="baseline,batch,baseline,cost,baseline,fused,baseline,combined,baseline")
    ap.add_argument("--threshold", type=float, default=.5)
    ap.add_argument("--kv", choices=["fp16", "int8", "q4_0", "k8v4"], help="Isolated test format; production config is untouched")
    ap.add_argument("--max-tokens", type=int, default=128)
    ap.add_argument("--warmup", type=int, default=2, help="Warmups per model timing case")
    ap.add_argument("--retained", type=int, default=3, help="Retained requests per model timing case")
    ap.add_argument("--cases", default="Short,Code2K,Prose2K,Long8K")
    ap.add_argument("--expert-cache", type=int, default=4000)
    ap.add_argument("--greedy-only", action="store_true", help="Run other functional checks when FP64 sampling is unavailable")
    args = ap.parse_args()
    if args.max_tokens < 48 or args.warmup < 0 or args.retained < 1:
        ap.error("max-tokens must be >=48, warmup >=0, retained >=1")
    if args.phase == "self-test":
        assert paired(100, 50, 100)["throughput_change_pct"] == 100
        assert paired(100, 100, 100)["baseline_drift_pct"] == 0
        assert environment({"env": {FLAGS[1]: "1"}}, "baseline").get(FLAGS[1]) is None
        print("PASS benchmark self-test")
        return
    with gpu_lock(allow_compiler=args.phase == "functional"):
        measure(args)


def measure(args):
    build = args.build_root.resolve()
    cfg = json.loads((args.config or build/"config-original.json").read_text(encoding="utf-8-sig"))
    out = args.out or build/args.phase
    out.mkdir(parents=True, exist_ok=False)
    (out/"bench_deepinfer.py").write_bytes(SCRIPT_SOURCE)
    data = {"complete": False, "phase": args.phase, "config": cfg, "threshold": args.threshold,
            "script_sha256": hashlib.sha256(SCRIPT_SOURCE).hexdigest(), "source_manifest_sha256": sha(build/"source-manifest.json"),
            "candidate_manifest_sha256": sha(build/"candidate-source-manifest.json"),
            "kv": args.kv or "fp16", "max_tokens": args.max_tokens, "modes": args.modes,
            "warmups_per_case": args.warmup, "retained_per_case": args.retained,
            "summary": "actual request with median decode milliseconds per generated token",
            "cumulative_confidence_floor": 0,
            "runs": [], "comparisons": []}
    manifest = json.loads((build/"candidate-source-manifest.json").read_text(encoding="utf-8"))
    data["candidate_source_sha256"] = {name: value["sha256"] for name, value in manifest.items()
                                       if name.startswith(("src/", "include/", "sycl/"))}
    data["benchmark_source_sha256"] = sha(ROOT/"sycl/src/kernels/kv_steps_parity.cpp")
    data["sampling_requested"] = not args.greedy_only
    data["build_sha256"] = {str(p.relative_to(build)): sha(p) for side in ("baseline", "candidate")
                            for p in (build/side).rglob("*") if p.suffix in (".lib", ".obj")}
    for side in ("baseline", "candidate"):
        data[side+"_sha256"] = {str(p.relative_to(build)): sha(p) for p in (build/side).glob("*.exe")}
    target = out/"results.json"
    save(target, data)
    if args.phase == "checks":
        cases = [("draft_policy_test", []), ("kv_steps_parity", []), ("rope_parity", []),
                 ("kv_q8_parity", []), ("kv_q4_parity", []),
                 ("qsa_parity", ["--indexer-commit-check"]), ("conversation_snapshot_test", [])]
        for i, (exe, extra) in enumerate(cases + [("kv_steps_parity", []), ("qsa_parity", ["--indexer-commit-check"])]):
            busy()
            env = environment(cfg)
            env["STRATA_ROPE_TABLE"] = "1" if i >= len(cases) else "0"
            print("CHECK", i, exe, env["STRATA_ROPE_TABLE"], flush=True)
            p = subprocess.run([str(build/"candidate"/(exe+".exe")), *extra], cwd=ROOT, env=env,
                               capture_output=True, text=True, timeout=1200)
            (out/f"{i}-{exe}.log").write_text(p.stdout+p.stderr, encoding="utf-8")
            data["runs"].append({"exe": exe, "args": extra, "rope_table": env["STRATA_ROPE_TABLE"], "exit": p.returncode})
            save(target, data)
            p.check_returncode()
    elif args.phase == "operators":
        for family, pair in (("KV", (0, 1)), ("fused", (2, 3)), ("indexer", (4, 5))):
            runs = []
            for i, mode in enumerate((pair[0], pair[1], pair[0], pair[1], pair[0])):
                busy()
                p = subprocess.run([str(build/"candidate/kv_steps_parity.exe"), "--bench", str(mode)],
                                   cwd=ROOT, env=environment(cfg), capture_output=True, text=True, timeout=600)
                (out/f"{family}-{i}-{mode}.log").write_text(p.stdout+p.stderr, encoding="utf-8")
                busy()
                p.check_returncode()
                rows = [json.loads(x) for x in re.findall(r"^RESULT (.+)$", p.stdout, re.M)]
                assert rows
                runs.append(rows)
                data["runs"].append({"family": family, "mode": mode, "rows": rows})
                save(target, data)
                print("OPERATOR", family, i, flush=True)
            for j in (1, 3):
                for a, b, c in zip(runs[j-1], runs[j], runs[j+1]):
                    row = {"family": family, "repeat": j, "format": b["format"], "T": b["T"], **paired(a["us"], b["us"], c["us"])}
                    row["gate"] = row["throughput_change_pct"] >= 3 and abs(row["baseline_drift_pct"]) <= 3
                    data["comparisons"].append(row)
            save(target, data)
    else:
        all_inputs = prompts(cfg)
        inputs = {name: all_inputs[name] for name in args.cases.split(",")}
        # The existing window-only prompt path needs no batched-prefill primer.
        window_prompt = int(cfg["args"][cfg["args"].index("--short-read")+1]) >= 8192
        primer = all_inputs["Short" if window_prompt or len(inputs) == 1 else "Code2K"]
        data["prompts"] = inputs
        modes = args.modes.split(",")
        assert all(x in MODES for x in modes) and 0 <= args.threshold <= 1
        reference = None
        for i, mode in enumerate(modes):
            reproducible = args.phase == "functional"
            argv = model_args(cfg, reproducible, args.threshold, args.expert_cache)
            if args.kv:
                if "--kv" in argv:
                    argv[argv.index("--kv")+1] = args.kv
                else:
                    argv += ["--kv", args.kv]
            # Frozen executable checks source parity; subsequent controls use the same candidate binary.
            side = "baseline" if i == 0 and mode == "baseline" else "candidate"
            print("MODEL", i, mode, side, flush=True)
            env = environment(cfg, mode, reproducible)
            # Existing host timers are already collected unconditionally; this only prints their per-request totals.
            if not reproducible:
                env["STRATA_DECODE_TIMING"] = "1"
            else:
                env["STRATA_SPEC_COUPLED"] = "1"
            engine = Engine(build/side/"strata.exe", argv, env, out/f"{i}-{mode}")
            try:
                slots = int(re.search(r"\bexpert_slots=(\d+)", engine.info)[1])
                assert slots == data.setdefault("expert_slots", slots), engine.info
                warmup = engine.gen(primer, 16)
                rows, samples, case_warmups = {}, {}, {}
                for name, ids in inputs.items():
                    case_warmups[name] = [engine.gen(ids, args.max_tokens) for _ in range(0 if reproducible else args.warmup)]
                    samples[name] = [engine.gen(ids, args.max_tokens) for _ in range(1 if reproducible else args.retained)]
                    rows[name] = sorted(samples[name], key=lambda r: r["decode_ms"]/r["generated"])[len(samples[name])//2]
                    print("CASE", mode, name, "done", flush=True)
                functional = {}
                if reproducible:
                    ids = all_inputs["Short"]
                    sampling = "temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128"
                    if not args.greedy_only:
                        functional["sampled"] = engine.gen(ids, 48, sampling)
                        functional["sampled_repeat"] = engine.gen(ids, 48, sampling)
                        assert functional["sampled"]["tokens"] == functional["sampled_repeat"]["tokens"]
                    functional["cancel"] = engine.gen(ids, 128, cancel=True)
                    assert functional["cancel"]["finish"] == "cancel"
                    functional["recovery"] = engine.gen(ids, 16)
                    assert functional["recovery"]["tokens"] == rows["Short"]["tokens"][:16]
                checked = {name: row["tokens"] for name, row in rows.items()}
                if reproducible:
                    if "sampled" in functional:
                        checked["sampled"] = functional["sampled"]["tokens"]
                    if reference is None:
                        reference = checked
                    assert reference == checked, f"generated output differs in {mode}"
                run = {"mode": mode, "build": side, "args": argv, "runtime_info": engine.info,
                       "warmup": warmup, "case_warmups": case_warmups, "samples": samples,
                       "rows": rows, "functional": functional}
                data["runs"].append(run)
                save(target, data)
                print("MODEL RESULT", mode, [(name, round(1000*r["generated"]/r["decode_ms"], 3)) for name, r in rows.items()], flush=True)
            finally:
                engine.close()
            data["runs"][-1]["stage_timings"] = [line for line in (out/f"{i}-{mode}.stderr").read_text(encoding="utf-8").splitlines()
                                                    if line.startswith("strata decode timing:")]
            if reproducible and mode in ("baseline", "combined"):
                cache_args = argv.copy()
                cache_args[cache_args.index("--prompt-cache")+1] = "128"
                cache_args += ["--conversation-cache-mib", "2048", "--conversation-cache-slots", "2"]
                cache_engine = Engine(build/side/"strata.exe", cache_args, {**env, "STRATA_SNAPSHOT_VERIFY": "1"}, out/f"{i}-{mode}-resume")
                try:
                    cache_engine.gen(primer, 16)
                    ref = cache_engine.gen(all_inputs["Short"], 48)
                    head = cache_engine.gen(all_inputs["Short"], 16)
                    assert head["tokens"] == ref["tokens"][:16]
                    cache_engine.gen(all_inputs["Code2K"][:23], 16)
                    parked = cache_engine.gen(all_inputs["Short"]+ref["tokens"][:16], 16)
                    assert parked["reused"] > 0 and parked["tokens"] == ref["tokens"][16:32]
                    live = cache_engine.gen(all_inputs["Short"]+ref["tokens"][:32], 16)
                    assert live["reused"] > 0 and live["tokens"] == ref["tokens"][32:48]
                    data["runs"][-1]["functional"].update({"parked_resume": parked, "live_resume": live})
                finally:
                    cache_engine.close()
                stop_id = rows["Short"]["tokens"][3]
                eos_engine = Engine(build/side/"strata.exe", argv+["--eos-ids", str(stop_id)], env, out/f"{i}-{mode}-eos")
                try:
                    eos = eos_engine.gen(all_inputs["Short"], 16)
                    count = rows["Short"]["tokens"].index(stop_id)+1
                    assert eos["finish"] == "stop" and eos["tokens"] == rows["Short"]["tokens"][:count]
                    data["runs"][-1]["functional"]["forced_eos"] = eos
                finally:
                    eos_engine.close()
            save(target, data)
        if not reproducible:
            for i in range(1, len(data["runs"])-1):
                a, b, c = data["runs"][i-1:i+2]
                if a["mode"] != "baseline" or c["mode"] != "baseline" or b["mode"] == "baseline":
                    continue
                for name in inputs:
                    row = {"mode": b["mode"], "case": name, "metrics": {}}
                    for key in ("decode_ms", "prompt_ms", "ttft_ms"):
                        values = [r["rows"][name][key] / (r["rows"][name]["generated"] if key == "decode_ms" else 1)
                                  for r in (a, b, c)]
                        row["metrics"]["decode_ms_per_token" if key == "decode_ms" else key] = paired(*values)
                    row["outputs_equal"] = a["rows"][name]["tokens"] == b["rows"][name]["tokens"] == c["rows"][name]["tokens"]
                    data["comparisons"].append(row)
    data["complete"] = True
    save(target, data)
    print("PASS", args.phase, target, flush=True)


if __name__ == "__main__":
    main()
