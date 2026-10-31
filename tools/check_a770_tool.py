"""Replay the IQ2_XS 258-token tool request that crossed short-read=256.

Use a dedicated local server with --prompt-cache 0 and --short-read 1024:
    python tools/check_a770_tool.py --url http://127.0.0.1:8096
"""
import argparse
import json
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    args = parser.parse_args()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def call(path, body=None):
        request = urllib.request.Request(args.url.rstrip("/") + path,
            data=json.dumps(body).encode() if body is not None else None,
            headers={"Content-Type": "application/json", "Connection": "close"})
        with opener.open(request, timeout=60) as response:
            assert response.status == 200
            return json.load(response)

    request = {"model": "qwen3.8-flash-next-iq2_xs", "temperature": 0,
        "thinking": {"type": "disabled"}, "max_tokens": 384, "stream": False,
        "messages": [{"role": "user", "content": "record memory locality."}],
        "tools": [{"name": "record", "input_schema": {"type": "object",
            "properties": {"value": {"type": "string"}}}}],
        "tool_choice": {"type": "tool", "name": "record"}}
    started = time.perf_counter()
    reply = call("/v1/messages", request)
    wall = time.perf_counter() - started
    assert reply["type"] == "message" and reply["stop_reason"] == "tool_use", reply
    calls = [block for block in reply["content"] if block["type"] == "tool_use"]
    assert len(calls) == 1 and calls[0]["name"] == "record", reply
    assert isinstance(calls[0]["input"]["value"], str) and calls[0]["input"]["value"], reply
    timings = call("/v1/status")["last_timings"]
    # ponytail: dedicated server required; status describes its most recent request.
    assert timings["cache_n"] == 0, "disable the prompt cache for this check"
    assert reply["usage"]["input_tokens"] == timings["prompt_n"] == 258
    assert reply["usage"]["output_tokens"] == timings["predicted_n"] > 0
    assert timings["prompt_ms"] > 0 and timings["predicted_ms"] > 0
    print(json.dumps({"result": "PASS", "http_s": wall,
        "input_tokens": timings["prompt_n"], "output_tokens": timings["predicted_n"],
        "prefill_tps": timings["prompt_n"] * 1000 / timings["prompt_ms"],
        "decode_tps": timings["predicted_n"] * 1000 / timings["predicted_ms"]}, indent=2))


if __name__ == "__main__":
    main()
