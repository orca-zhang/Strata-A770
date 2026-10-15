#!/usr/bin/env python3
"""Fresh-prompt A770 measurements; start a dedicated server with --prompt-cache 0.

python tools/bench_a770.py --config strata-iq2_xs.json --out baseline.json
Run the same cases against each MTP/config/kernel variant, then compare the JSON.
"""
import argparse
import hashlib
import json
import math
import statistics
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def collect_stream(response, started):
    from serve.server import openai_collect

    ttft_ms, done = None, False

    def chunks():
        nonlocal ttft_ms, done
        # ponytail: Strata emits each JSON chunk on one SSE data line.
        for line in response:
            if not line.startswith(b"data:"):
                continue
            data = line[5:].strip()
            if data == b"[DONE]":
                done = True
                break
            chunk = json.loads(data)
            if "error" in chunk:
                raise ValueError(chunk["error"])
            delta = chunk["choices"][0]["delta"]
            if ttft_ms is None and delta.get("content"):
                ttft_ms = (time.monotonic() - started) * 1000
            yield chunk

    out = openai_collect(chunks())
    if not done or ttft_ms is None:
        raise ValueError("Incomplete stream or no visible token")
    return {**out, "ttft_ms": ttft_ms}


def checked_timings(response):
    if response["choices"][0]["finish_reason"] not in ("stop", "length"):
        raise ValueError("Request did not complete successfully")
    usage, timings = response["usage"], response["timings"]
    if usage.get("prompt_tokens_details", {}).get("cached_tokens", 0):
        raise ValueError("Prompt was cached; start the dedicated server with --prompt-cache 0")
    for key in ("prompt_per_second", "predicted_per_second"):
        if not math.isfinite(timings[key]) or timings[key] <= 0:
            raise ValueError(f"Invalid engine timing: {key}")
    if timings["prompt_n"] != usage["prompt_tokens"] or timings["predicted_n"] != usage["completion_tokens"]:
        raise ValueError("Timings and usage disagree")
    offered, accepted = timings.get("draft_n", 0), timings.get("draft_n_accepted", 0)
    if not 0 <= accepted <= offered:
        raise ValueError("Invalid draft counters")
    out = {**timings, "draft_acceptance": accepted / offered if offered else None}
    if "ttft_ms" in response:
        out["ttft_ms"] = response["ttft_ms"]
    return out


def self_test():
    response = {"choices": [{"finish_reason": "length"}],
                "usage": {"prompt_tokens": 2048, "completion_tokens": 128},
                "timings": {"prompt_n": 2048, "predicted_n": 128,
                            "prompt_per_second": 100., "predicted_per_second": 20.,
                            "draft_n": 12, "draft_n_accepted": 9}}
    assert checked_timings(response)["draft_acceptance"] == .75
    response["timings"]["draft_n"] = response["timings"]["draft_n_accepted"] = 0
    assert checked_timings(response)["draft_acceptance"] is None
    for change in ({"predicted_per_second": float("nan")}, {"prompt_n": 1}, {"draft_n_accepted": 1}):
        bad = {**response, "timings": {**response["timings"], **change}}
        try:
            checked_timings(bad)
        except ValueError:
            pass
        else:
            raise AssertionError(change)
    response["usage"]["prompt_tokens_details"] = {"cached_tokens": 4}
    try:
        checked_timings(response)
    except ValueError:
        pass
    else:
        raise AssertionError("Cached prefill was accepted")
    import io
    chunk = {"id": "test", "created": 0, "model": "test",
             "choices": [{"delta": {"content": "hello"}, "finish_reason": None}]}
    final = {**chunk, "choices": [{"delta": {}, "finish_reason": "stop"}],
             "usage": {"prompt_tokens": 1, "completion_tokens": 1}}
    data = b": keepalive\n\n" + b"data: " + json.dumps(chunk).encode() + b"\n\ndata: " + json.dumps(final).encode() + b"\n\ndata: [DONE]\n\n"
    out = collect_stream(io.BytesIO(data), time.monotonic())
    assert out["choices"][0]["message"]["content"] == "hello" and out["ttft_ms"] >= 0
    try:
        collect_stream(io.BytesIO(data.split(b"data: [DONE]")[0]), time.monotonic())
    except ValueError:
        pass
    else:
        raise AssertionError("Incomplete SSE stream was accepted")
    print("bench_a770 self-test: PASS")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--url", default="http://127.0.0.1:8096")
    ap.add_argument("--config", type=Path)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--max-tokens", type=int, default=128)
    ap.add_argument("--cases", default="short,code,prose,long")
    ap.add_argument("--reference", type=Path, help="Require identical greedy text to this earlier report")
    ap.add_argument("--spec-min-p", type=float, help="Per-request MTP confidence threshold (0..1)")
    ap.add_argument("--stream", action="store_true", help="Measure client TTFT from the first nonempty SSE content chunk")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return
    cases = args.cases.split(",")
    if not args.config or not args.out or args.runs < 1 or args.max_tokens < 2:
        ap.error("--config, --out, positive --runs and --max-tokens >= 2 are required")
    if any(case not in ("short", "code", "prose", "long", "gcd", "memory", "followup") for case in cases):
        ap.error("Unknown case")
    if args.spec_min_p is not None and not 0 <= args.spec_min_p <= 1:
        ap.error("--spec-min-p must be in 0..1")
    if args.out.exists():
        ap.error("Output exists; use a new file to preserve previous measurements")
    from strata_tokenizer import Tokenizer
    from serve.frontend import ChatTemplate, openai_to_messages

    config = json.loads(args.config.read_text(encoding="utf-8-sig"))
    directory = Path(config["tokenizer"])
    vocab = json.loads((directory / "vocab.json").read_text(encoding="utf-8"))
    tokens = [None] * len(vocab)
    for token, number in vocab.items():
        tokens[number] = token
    tokenizer = Tokenizer(tokens, (directory / "merges.txt").read_text(encoding="utf-8").splitlines(),
                          json.loads((directory / "token_type.json").read_text(encoding="utf-8")))
    template = ChatTemplate(directory / "chat_template.jinja")

    def request(messages):
        body = {"model": config["model_name"], "messages": messages, "temperature": 0,
                "reasoning_effort": "none", "max_tokens": args.max_tokens, "stream": args.stream}
        if args.spec_min_p is not None:
            body["strata_tune"] = {"spec_min_p": args.spec_min_p}
        return body

    def count(messages):
        msgs, tools, kwargs = openai_to_messages(request(messages))
        return len(tokenizer.encode(template.render(msgs, tools, **kwargs), parse_special=True))

    def call(path, body=None):
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(args.url.rstrip("/") + path, data, {"Content-Type": "application/json"})
        started = time.monotonic()
        with urllib.request.urlopen(req, timeout=1800) as response:
            if body and body.get("stream"):
                return collect_stream(response, started)
            return json.load(response)

    code = "\n".join(f"def task_{i}(x): return (x * {i % 97 + 1} + {i}) % 100003" for i in range(2000))
    prose = "\n".join(f"Section {i}: Rivers connect hills and fields. People record rainfall, compare seasons, and plan water use."
                      for i in range(2000))
    report = {"config": config, "max_tokens": args.max_tokens, "runs": args.runs,
              "metrics_before": call("/metrics"), "trials": [], "medians": {}}
    report["request_tune"] = request([]).get("strata_tune", {})
    references = {}
    if args.reference:
        previous = json.loads(args.reference.read_text(encoding="utf-8"))
        references = {(row["case"], row["run"]): row for row in previous["trials"]}
    executable = Path(config["exe"])
    if not executable.is_absolute():
        executable = Path(config.get("cwd", ".")) / executable
    report["executable_sha256"] = hashlib.sha256(executable.read_bytes()).hexdigest()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    for case in cases:
        for run in range(args.runs):
            prefix = f"Trial {run}, case {case}.\n"
            ending = "\nExplain the material above in detail, using at least 600 words."
            messages = [{"role": "user", "content": prefix +
                         "Write an iterative Python fibonacci(n) returning the nth Fibonacci number. Code only."}]
            if case == "gcd":
                messages[0]["content"] = prefix + "Write Python gcd(a, b) using Euclid's algorithm. Code only."
            elif case == "memory":
                messages = [{"role": "user", "content": prefix + "Remember my access code is MAPLE-742."},
                            {"role": "assistant", "content": "I will remember MAPLE-742."},
                            {"role": "user", "content": "What is my access code? Reply with the code only."}]
            elif case in ("code", "prose", "long", "followup"):
                filler = code if case == "code" else prose
                target = 2048 if case in ("code", "prose") else 8000
                if case == "followup":
                    ending = "\nRemember the code MAPLE-742."
                    messages.extend([{"role": "assistant", "content": "Recorded."},
                                     {"role": "user", "content": "What code did I ask you to remember? Code only."}])
                low, high = 0, len(filler)
                while low < high:
                    middle = (low + high + 1) // 2
                    messages[0]["content"] = prefix + filler[:middle] + ending
                    if count(messages) <= target:
                        low = middle
                    else:
                        high = middle - 1
                messages[0]["content"] = prefix + filler[:low] + ending
                if not target - 20 <= count(messages) <= target:
                    raise ValueError("Failed to construct target prompt length")
            expected = count(messages)
            if expected + args.max_tokens > report["metrics_before"]["engine"]["max_context"]:
                raise ValueError("Prompt plus output exceeds context")
            print(f"{case} {run + 1}/{args.runs}: {expected} prompt tokens", flush=True)
            started_at = time.time()
            started = time.monotonic()
            response = call("/v1/chat/completions", request(messages))
            timings = checked_timings(response)
            if timings["prompt_n"] != expected:
                raise ValueError("Server tokenization differs from the benchmark")
            report["trials"].append({"case": case, "run": run, "messages": messages,
                                     "started_at": started_at, "finished_at": time.time(),
                                     "wall_s": time.monotonic() - started, "response": response})
            rows = [checked_timings(row["response"]) for row in report["trials"] if row["case"] == case]
            report["medians"][case] = {
                key: statistics.median(row[key] for row in rows if row[key] is not None)
                if any(row[key] is not None for row in rows) else None
                for key in ("prompt_per_second", "predicted_per_second", "draft_acceptance") + (("ttft_ms",) if args.stream else ())}
            report["metrics_after"] = call("/metrics")
            match = True
            if args.reference:
                previous = references[(case, run)]
                match = (previous["messages"] == messages and
                         previous["response"]["choices"] == response["choices"])
                report["trials"][-1]["reference_match"] = match
            args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print(json.dumps(report["medians"][case]), flush=True)
            if not match:
                raise ValueError(f"Greedy output differs from reference: {case}, run {run}")


if __name__ == "__main__":
    main()
