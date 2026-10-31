"""Keep the local Ninja build out of A770 benchmark intervals."""
import subprocess
import sys

from bench_deepinfer import gpu_lock


def build(args):
    # ponytail: reuse the benchmark lease; all cooperating builds share one slot.
    with gpu_lock():
        return subprocess.call(["ninja", *args])


def self_check():
    from contextlib import contextmanager
    from unittest.mock import patch

    events = []

    @contextmanager
    def lease():
        events.append("held")
        try:
            yield
        finally:
            events.append("released")

    def command(args):
        assert events[-1] == "held" and args == ["ninja", "strata", "-j2"]
        return 7

    with patch.dict(build.__globals__, gpu_lock=lease):
        with patch.object(subprocess, "call", command):
            assert build(["strata", "-j2"]) == 7
        with patch.object(subprocess, "call", side_effect=OSError("missing ninja")):
            try:
                build([])
            except OSError:
                pass
            else:
                raise AssertionError("build failure swallowed")
    assert events == ["held", "released", "held", "released"]
    print("PASS build holds shared lease, forwards arguments/status and releases on failure; no build/GPU")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-check"]:
        self_check()
    else:
        sys.exit(build(sys.argv[1:]))
