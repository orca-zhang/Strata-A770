"""CPU screening of binary/ternary LUT weights against captured expert inputs.

Uses existing BF16 source/FP16 activation manifests; never changes a model.
Run --self-check without model files. See the dated report for a local command.
"""
import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path

import numpy as np

from canonical_xcheck import from_s2, to_s2
from gguf_reader import BLOCK_GEOMETRY, GGML_TYPES

ROOT = Path(__file__).resolve().parents[1]
PARTS = ("gate", "up", "down")


def quantize(w, mode):
    if mode not in ("binary", "ternary"):
        raise ValueError("expected binary or ternary")
    w = np.asarray(w, np.float32)
    if w.ndim != 2 or not w.size or w.shape[1] % 64 or not np.isfinite(w).all():
        raise ValueError("expected finite nonempty matrix with K divisible by 64")
    blocks = w.reshape(-1, 64)
    mag = np.abs(blocks)
    if mode == "binary":
        alpha = mag.mean(axis=1)
        symbols = np.where(blocks >= 0, 1, -1)
        digits, radix = 8, 2
    else:
        # Exact groupwise L2 fit before FP16 scale rounding; no activation fitting.
        sums = np.cumsum(np.sort(mag, axis=1)[:, ::-1], axis=1)
        keep = np.argmax(sums * sums / np.arange(1, 65), axis=1)
        alpha = sums[np.arange(len(blocks)), keep] / (keep + 1)
        symbols = np.sign(blocks) * (mag >= alpha[:, None] / 2)
        digits, radix = 5, 3
    with np.errstate(over="ignore"):
        scales = alpha.astype("<f2")
    if not np.isfinite(scales).all():
        raise ValueError("FP16 scale overflow")
    values = ((symbols + 1) // 2 if mode == "binary" else symbols + 1).astype(np.uint8)
    width = ((64 + digits - 1) // digits) * digits
    values = np.pad(values, ((0, 0), (0, width - 64)), constant_values=1)
    codes = (values.reshape(-1, width // digits, digits) * radix ** np.arange(digits)).sum(axis=2)
    return codes.astype(np.uint8), scales


def decode(codes, scales, mode, shape):
    digits, radix = (8, 2) if mode == "binary" else (5, 3)
    values = ((codes[..., None].astype(np.int16) // radix ** np.arange(digits)) % radix)
    symbols = (values * 2 - 1 if mode == "binary" else values - 1).reshape(len(scales), -1)[:, :64]
    return (symbols * scales.astype(np.float32)[:, None]).reshape(shape).astype(np.float32)


def lut_dot(x, codes, scales, mode, rows):
    """Independent activation-LUT dot check, not a CPU/GPU speed benchmark."""
    digits, radix = (4, 2) if mode == "binary" else (5, 3)
    if mode == "binary":
        codes = ((codes[..., None] >> np.array([0, 4])) & 15).reshape(len(scales), 16)
    patterns = np.array(list(itertools.product(range(radix), repeat=digits)), np.float32)[:, ::-1]
    patterns = patterns * 2 - 1 if mode == "binary" else patterns - 1
    groups, chunks = x.shape[1] // 64, codes.shape[1]
    padded = np.pad(x.reshape(len(x), groups, 64), ((0, 0), (0, 0), (0, chunks * digits - 64)))
    table = np.einsum("tgpc,vc->tgpv", padded.reshape(len(x), groups, chunks, digits), patterns)
    c = codes.reshape(rows, groups, chunks)
    s = scales.reshape(rows, groups).astype(np.float32)
    return np.stack([(table[:, np.arange(groups)[:, None], np.arange(chunks), c[r]].sum(axis=2) * s[r]).sum(axis=1)
                     for r in range(rows)], axis=1)


def relative_l2(ref, got):
    delta = got.astype(np.float64) - ref
    energy = np.sum(ref.astype(np.float64) ** 2)
    return float(np.sqrt(np.sum(delta * delta) / energy)) if energy else float(np.linalg.norm(delta))


def hidden(x, w):
    gate, up = x @ w["gate"].T, x @ w["up"].T
    # ponytail: isolate weight changes with FP32 dots; this does not emulate Q8/DPAS decode.
    h = (gate / (1 + np.exp(-np.clip(gate, -80, 80)))) * up
    return np.clip(h, -65504, 65504).astype(np.float16).astype(np.float32)


def self_check():
    rng = np.random.default_rng(17)
    x = rng.standard_normal((3, 128), dtype=np.float32)
    w = rng.standard_normal((7, 128), dtype=np.float32)
    w[0] = 0
    for mode, block_bytes in (("binary", 10), ("ternary", 15)):
        codes, scales = quantize(w, mode)
        got = decode(codes, scales, mode, w.shape)
        assert codes.nbytes + scales.nbytes == w.size // 64 * block_bytes
        assert np.isfinite(got).all() and not got[0].any()
        np.testing.assert_allclose(lut_dot(x, codes, scales, mode, len(w)), x @ got.T, atol=1e-5, rtol=2e-5)
        for bad in (np.zeros((1, 63)), np.full((1, 64), np.nan), np.full((1, 64), 1e10)):
            try:
                quantize(bad, mode)
            except ValueError:
                pass
            else:
                raise AssertionError("invalid weight accepted")
    # All 243 ternary words, including the padded 65th trit, independently decode.
    codes = np.repeat(np.arange(243, dtype=np.uint8)[:, None], 13, axis=1)
    scales = np.ones(243, np.float16)
    got = decode(codes, scales, "ternary", (243, 64))
    for i, row in enumerate(got):
        expected = [(i // 3 ** (j % 5)) % 3 - 1 for j in range(64)]
        np.testing.assert_array_equal(row, expected)
    # Exactly representable ternary weights must survive fitting and packing.
    w = np.tile(np.array([-2, 0, 2, 0], np.float32), (3, 32))
    c, s = quantize(w, "ternary")
    np.testing.assert_array_equal(decode(c, s, "ternary", w.shape), w)
    # Existing Q2_0 can use two binary LUT planes without another weight quantization:
    # c-1 = 0.5*(2*b0-1) + (2*b1-1) + 0.5.
    q2 = rng.integers(0, 4, (14, 64), dtype=np.uint8)
    scales = rng.uniform(-2, 2, 14).astype(np.float16)
    lo = np.packbits(q2 & 1, axis=1, bitorder="little")
    hi = np.packbits(q2 >> 1, axis=1, bitorder="little")
    result = 0.5 * lut_dot(x, lo, scales, "binary", 7) + lut_dot(x, hi, scales, "binary", 7)
    result += 0.5 * (x.reshape(3, 2, 64).sum(axis=2) @ scales.reshape(7, 2).astype(np.float32).T)
    ref = x @ ((q2.astype(np.float32) - 1) * scales.astype(np.float32)[:, None]).reshape(7, 128).T
    np.testing.assert_allclose(result, ref, atol=1e-5, rtol=2e-5)
    print("PASS: independent LUT dots, 243 codewords, zero/tails, fit, payload, invalid inputs, Q2 bitplane identity")


def run(args):
    from _paths import add_gguf_py
    add_gguf_py()
    import gguf

    weights = json.loads(args.weights.read_text(encoding="utf-8"))
    inputs = json.loads(args.inputs.read_text(encoding="utf-8"))
    if not weights["complete"] or not inputs["complete"]:
        raise ValueError("incomplete capture/source manifest")
    sources = {(s["layer"], s["expert"]): s for s in weights["samples"]}
    hashes = {}

    def read(path, expected=None):
        p = ROOT / path
        data = p.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if expected is not None and digest != expected:
            raise ValueError(f"SHA256 mismatch: {p}")
        hashes[str(p.relative_to(ROOT))] = digest
        return data

    records = []
    for request in inputs["rows"]:
        for item in request["inputs"]:
            sample = sources[item["layer"], item["expert"]]
            raw = read(item["file"], item["sha256"])
            if len(raw) < 24 or struct.unpack_from("<8s4I", raw) != (
                    b"U4ACT001", item["layer"], item["expert"], item["rows"], item["cols"]):
                raise ValueError("captured input header mismatch")
            x = np.frombuffer(raw, "<f2", offset=24).reshape(item["rows"], item["cols"])
            if not np.isfinite(x).all() or x.shape[1] != 2560:
                raise ValueError("invalid captured input")
            indices = np.linspace(0, len(x) - 1, min(args.rows, len(x)), dtype=int)
            x = x[indices].astype(np.float32)
            original = {}
            for part, shape in (("gate_up", (1280, 2560)), ("down", (2560, 640))):
                path = sample["original"][part]
                data = read(path, weights["files_sha256"][path])
                original[part] = (np.frombuffer(data, "<u2").astype(np.uint32) << 16).view(np.float32).reshape(shape)
            original["gate"], original["up"] = original.pop("gate_up").reshape(2, 640, 2560)
            raw = read(sample["native"], weights["files_sha256"][sample["native"]])
            if len(raw) != 1510400:
                raise ValueError("this capture expects IQ2_S GU / Q2_0 down")
            native = {}
            for i, part in enumerate(("gate", "up")):
                a = np.frombuffer(raw[i * 524800:(i + 1) * 524800], np.uint8)
                native[part] = gguf.dequantize(a, gguf.GGMLQuantizationType.IQ2_S).reshape(640, 2560)
            native["down"] = from_s2(to_s2(raw[1049600:])).reshape(2560, 640)
            ref = hidden(x, original) @ original["down"].T
            nh = hidden(x, native)
            baseline = nh @ native["down"].T
            record = dict(case=request["case"], layer=item["layer"], expert=item["expert"],
                          captured_rows=item["rows"], sampled_indices=indices.tolist(),
                          baseline_y_nrmse=relative_l2(ref, baseline), variants=[])
            for mode in ("binary", "ternary"):
                candidate, payload = {}, {}
                for part in PARTS:
                    c, s = quantize(original[part], mode)
                    candidate[part] = decode(c, s, mode, original[part].shape)
                    payload[part] = c.nbytes + s.nbytes
                    rows = 8
                    groups = original[part].shape[1] // 64
                    activation = x[:2] if part != "down" else nh[:2]
                    np.testing.assert_allclose(lut_dot(activation, c[:rows * groups], s[:rows * groups], mode, rows),
                                               activation @ candidate[part][:rows].T, atol=5e-5, rtol=2e-4)
                for replace in (("gate",), ("up",), ("down",), PARTS):
                    mixed = {p: candidate[p] if p in replace else native[p] for p in PARTS}
                    h = nh if replace == ("down",) else hidden(x, mixed)
                    y = h @ mixed["down"].T
                    if not np.isfinite(y).all():
                        raise ValueError("nonfinite candidate output")
                    size = sum(payload[p] if p in replace else (460800 if p == "down" else 524800) for p in PARTS)
                    record["variants"].append(dict(mode=mode, replace=list(replace), bytes=size,
                        bytes_saved_fraction=1 - size / len(raw), y_nrmse=relative_l2(ref, y),
                        delta_from_native_nrmse=relative_l2(baseline, y)))
            records.append(record)
            print(json.dumps(record, ensure_ascii=False), flush=True)
    return dict(complete=True, model_quality_tested=False, gpu_speed_tested=False, production_changed=False,
                rows_per_input=args.rows, numpy_version=np.__version__, records=records, files_sha256=hashes,
                manifests_sha256={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.weights, args.inputs)})


def inventory(path):
    rows = []
    for line in path.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        layer, gu, down, _, size = map(int, line.split()[:5])
        rows.append(dict(layer=layer, gu=GGML_TYPES[gu], down=GGML_TYPES[down], bytes=size))
    return dict(source=str(path), source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                layers=rows, experts_per_layer=512, original_bytes=sum(r["bytes"] * 512 for r in rows),
                geometry={n: BLOCK_GEOMETRY[n] for n in {r["gu"] for r in rows} | {r["down"] for r in rows}})


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--self-check", action="store_true")
    ap.add_argument("--weights", type=Path)
    ap.add_argument("--inputs", type=Path)
    ap.add_argument("--native-experts", type=Path)
    ap.add_argument("--rows", type=int, default=64)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()
    if args.self_check:
        self_check()
    else:
        if not args.weights or not args.inputs or not args.out or args.rows < 1:
            ap.error("--weights, --inputs, --out and positive --rows required")
        if args.out.exists():
            ap.error("output already exists; choose a new path")
        # ponytail: reuse the existing experiment lock; CPU BLAS can disturb GPU measurements.
        from bench_deepinfer import gpu_lock
        with gpu_lock():
            result = run(args)
        if args.native_experts:
            result["inventory"] = inventory(args.native_experts)
        result["script_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        args.out.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
