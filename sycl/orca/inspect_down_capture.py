"""Inspect an opt-in Down snapshot and freeze a CPU64-checked native replay fixture.

python sycl/orca/inspect_down_capture.py down.bin new-fixture-directory
python sycl/orca/inspect_down_capture.py --self-check
Uses the workspace's existing NumPy installation; never executes GPU work.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile

import numpy as np


def inspect(capture, fixture):
    data = capture.read_bytes()
    if len(data) < 48:
        raise ValueError("truncated Down capture header")
    t, n, k, layer, expert, position = struct.unpack_from("<6q", data)
    if not (0 < t <= 8192 and n == 2560 and k == 640 and
            0 <= layer < 256 and 0 <= expert < 512 and position >= 0):
        raise ValueError("invalid Down capture metadata")
    if len(data) != 48 + t * k * 2 + n * k * 2 + t * n * 4:
        raise ValueError("wrong-sized Down capture payload")
    offset = 48
    x = np.frombuffer(data, dtype="<f2", count=t * k, offset=offset).reshape(t, k)
    offset += t * k * 2
    w = np.frombuffer(data, dtype="<f2", count=n * k, offset=offset).reshape(n, k)
    offset += n * k * 2
    y = np.frombuffer(data, dtype="<f4", count=t * n, offset=offset).reshape(t, n)
    report = dict(complete=False, performance_measurement=False, full_model_stability_established=False,
                  linux_gpu_validated=False, capture_sha256=hashlib.sha256(data).hexdigest(),
                  shape=dict(T=t, N=n, K=k), layer=layer, expert=expert, position=position,
                  nonfinite={name: int((~np.isfinite(a)).sum()) for name, a in (("x", x), ("w", w), ("y", y))},
                  numpy_version=np.__version__, snapshot_scope="Immediately after Down on the compute stream, before known DQ/scratch reuse; not a sample during GEMM execution.")
    fixture.mkdir()  # Require a new directory; preserve earlier evidence.
    xpad = np.zeros(((t + 7) // 8 * 8, k), dtype="<f2")
    xpad[:t] = x
    xpad.tofile(fixture / "x.f16")
    w.tofile(fixture / "weights.f16")
    y.tofile(fixture / "captured.f32")
    (fixture / "shape.txt").write_text(f"{t} {n} {k}\n", encoding="ascii")
    if not report["nonfinite"]["x"] and not report["nonfinite"]["w"]:
        xd, wd = x.astype(np.float64), w.astype(np.float64)
        exact = xd @ wd.T
        expected = exact.astype("<f4")
        u = 2.0 ** -24
        gamma = k * u / (1 - k * u)
        bound = np.nextafter((gamma * (np.abs(xd) @ np.abs(wd).T) +
                             np.abs(exact - expected.astype(np.float64)) + 2e-6).astype("<f4"), np.float32(np.inf))
        bad = ~np.isfinite(y) | (np.abs(y.astype(np.float64) - expected.astype(np.float64)) > bound)
        coords = np.argwhere(bad)
        expected.tofile(fixture / "expected.f32")
        bound.tofile(fixture / "bound.f32")
        report.update(cpu64_checked=True, outside_bound_finite=int((bad & np.isfinite(y)).sum()),
                      bad_cells=int(bad.sum()), bad_tiles_8x8=len({(int(r) // 8, int(c) // 8) for r, c in coords}),
                      bad_coordinates_first512=coords[:512].tolist(), reference_abs_max=float(np.abs(exact).max()),
                      bound="gamma(K)*sum(abs(x*w)) + CPU64-to-FP32 rounding error + 2e-6, FP32 rounded upwards", gamma=gamma)
    else:
        report.update(cpu64_checked=False, replay_ready=False)
    report["replay_ready"] = report["cpu64_checked"]
    report["fixture_sha256"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in fixture.iterdir()}
    report["complete"] = True
    (fixture / "inspection.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def self_check():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        x, w, y = np.ones((1, 640), dtype="<f2"), np.full((2560, 640), .5, dtype="<f2"), np.full((1, 2560), 320, dtype="<f4")
        y[0, -1] = np.nan
        capture = root / "down.bin"
        capture.write_bytes(struct.pack("<6q", 1, 2560, 640, 7, 23, 1024) + x.tobytes() + w.tobytes() + y.tobytes())
        result = inspect(capture, root / "fixture")
        assert result["nonfinite"] == dict(x=0, w=0, y=1)
        assert result["bad_cells"] == 1 and result["outside_bound_finite"] == 0
        assert result["bad_coordinates_first512"] == [[0, 2559]]
        assert np.all(np.fromfile(root / "fixture/expected.f32", dtype="<f4") == 320)
        capture.write_bytes(capture.read_bytes()[:-1])
        try:
            inspect(capture, root / "bad")
        except ValueError:
            pass
        else:
            raise AssertionError("truncated snapshot was accepted")
    print("PASS Down CPU64 fixture: known product, injected NaN, coordinates, truncated payload")


if __name__ == "__main__":
    if sys.argv[1:] == ["--self-check"]:
        self_check()
    elif len(sys.argv) == 3:
        print(json.dumps(inspect(Path(sys.argv[1]), Path(sys.argv[2])), indent=2))
    else:
        sys.exit(__doc__)
