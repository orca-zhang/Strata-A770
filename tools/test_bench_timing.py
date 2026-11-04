"""CPU-only protocol check for engine timing versus streamed delivery timing."""
import math
from unittest.mock import patch

from bench_deepinfer import Engine


def request(lines):
    engine = Engine.__new__(Engine)
    engine.timing = False
    engine.send = lambda line: None
    engine.next = iter(lines).__next__
    with patch("bench_deepinfer.time.perf_counter", return_value=10.0):
        return engine.gen([1], 3)


row = request([(10.020, "TB 2 10 11"), (10.050, "T 12"),
               (10.060, "DONE 3 1 12.0 40.0 length 0 0 0")])
assert row["tokens"] == [10, 11, 12] and row["first_delivery_tokens"] == 2
assert row["prompt_ms"] == 12.0 and row["decode_ms"] == 40.0
assert math.isclose(row["ttft_ms"], 20.0)
assert math.isclose(row["last_delivery_ms"], 50.0)
assert math.isclose(row["delivery_span_ms"], 30.0)
assert math.isclose(row["request_ms"], 60.0)
# Two tokens arrived together first: only one token belongs to the later span.
assert math.isclose(row["post_first_delivery_tok_s"], 1000.0 / 30.0)

row = request([(10.020, "T 10"), (10.025, "DONE 1 1 12.0 8.0 stop 0 0 0")])
assert row["first_delivery_tokens"] == 1 and row["delivery_span_ms"] == 0.0
assert row["post_first_delivery_tok_s"] is None

row = request([(10.010, "DONE 0 1 10.0 0.0 cancel 0 0 0")])
assert row["tokens"] == [] and row["first_delivery_tokens"] == 0
assert row["ttft_ms"] is row["last_delivery_ms"] is row["delivery_span_ms"] is None
assert row["post_first_delivery_tok_s"] is None
print("PASS timing boundaries: engine counters unchanged, streamed bursts, single delivery and zero-token cancellation")
