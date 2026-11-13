"""Run with python tools/test_mtp_rt.py; no GPU or model files required."""
from pathlib import Path
import hashlib
import json
import sys
import tempfile
import numpy as np

import mtp_rt
import mtp_pack
from mtp_rt import blob_of, H, FF, BLOB
from mtp_pack import QUANT, dequant, q2_0


def main():
    exact = np.tile(np.array([-1, 0, 1, 2], dtype=np.float32), 16)
    assert q2_0(exact).tobytes() == np.float16(1).tobytes() + bytes([228] * 16)
    negative = np.tile(np.array([-2, -1, 0, 1], dtype=np.float32), 16)
    assert np.array_equal(dequant('q2_0', q2_0(negative, np.ones(64)), 64), negative)
    weights = np.random.default_rng(44).normal(size=(16, 256)).astype(np.float32)
    importance = np.geomspace(1e-4, 1e4, 256).astype(np.float32)
    calibrated = q2_0(weights, importance)
    original = q2_0(weights)
    uniform = q2_0(weights, np.ones(256, dtype=np.float32))
    error = lambda blob: ((dequant('q2_0', blob, weights.size).reshape(weights.shape) - weights) ** 2 *
                          importance).reshape(-1, 64).sum(axis=1)
    assert calibrated.size == original.size and (error(calibrated) <= error(original) + 1e-5).all()
    assert error(calibrated).sum() < error(uniform).sum()
    assert np.array_equal(q2_0(weights, np.zeros(256)), original)
    for bad in (np.ones(255), np.full(256, -1), np.full(256, np.nan), np.full(256, np.inf)):
        try:
            q2_0(weights, bad)
        except ValueError:
            pass
        else:
            raise AssertionError('invalid calibration moments accepted')
    gu = np.arange(2 * FF * H // 64 * 18, dtype=np.uint8).reshape(2 * FF, H // 64, 18)
    gu[FF:] ^= 0x5A
    down = np.full((H, FF // 64, 18), 173, dtype=np.uint8)
    blob = blob_of(gu.reshape(2 * FF, -1), down.reshape(H, -1))
    # Read the split planes as the existing Q2 kernel does: interleaved gate/up, then down.
    values = 3 * H * FF // 64
    codes = np.frombuffer(blob[:values * 16], dtype=np.uint8).reshape(-1, 16)
    scales = np.frombuffer(blob[values * 16:], dtype=np.uint8).reshape(-1, 2)
    restored = np.concatenate((scales, codes), axis=1)
    inter = restored[:2 * FF * (H // 64)].reshape(2 * FF, H // 64, 18)
    assert len(blob) == BLOB
    assert np.array_equal(inter[::2], gu[:FF]) and np.array_equal(inter[1::2], gu[FF:])
    assert np.array_equal(restored[2 * FF * (H // 64):].reshape(down.shape), down)
    for kind, size in (("q5_0", 22), ("q8_0", 34)):
        gu = np.arange(2 * FF * (H // 32 * size), dtype=np.uint8).reshape(2 * FF, -1)
        gu[FF:] ^= 0x5A
        down = np.full((H, FF // 32 * size), 173, dtype=np.uint8)
        blob = blob_of(gu, down, kind=kind)
        gate_bytes = FF * H // 32 * size
        assert len(blob) == 3 * gate_bytes
        assert blob[:gate_bytes] == gu[:FF].tobytes()
        assert blob[gate_bytes:2 * gate_bytes] == gu[FF:].tobytes()
        assert blob[2 * gate_bytes:] == down.tobytes()
        for bad_gu, bad_down in ((gu[:, :-1], down), (gu, down[:-1]), (gu.astype(np.int16), down)):
            try:
                blob_of(bad_gu, bad_down, kind=kind)
            except ValueError:
                pass
            else:
                raise AssertionError("malformed native expert rows accepted")
    x = np.random.default_rng(42).normal(size=(4, 640)).astype(np.float32)
    fn, _, _, _ = QUANT["q5_0"]
    got = dequant("q5_0", fn(x), x.size).reshape(x.shape)
    assert np.sqrt(np.mean((got - x) ** 2)) < 0.08
    # Exercise the actual GGUF reader/runtime writer with small geometry, including the Gemma norm offset.
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp)
        rng = np.random.default_rng(43)
        gu = fn(rng.normal(size=(2, 64, 64)).astype(np.float32)).reshape(2, 64, 44)
        dn = fn(rng.normal(size=(2, 64, 32)).astype(np.float32)).reshape(2, 64, 22)
        writer = mtp_rt.gguf.GGUFWriter(str(path / "toy.gguf"), "qwen4exp-mtp")
        qtype = QUANT["q5_0"][1]
        for name, data in (("gate_up_proj", gu), ("down_proj", dn)):
            writer.add_tensor("mtp.layers.0.mlp.experts." + name, data, raw_shape=list(data.shape), raw_dtype=qtype)
        writer.add_tensor("mtp.pre_fc_norm_embedding.weight", np.zeros(64, dtype=np.float32))
        writer.write_header_to_file()
        writer.write_kv_data_to_file()
        writer.write_tensors_to_file()
        writer.close()
        before = (mtp_rt.H, mtp_rt.FF, mtp_rt.NE, sys.argv)
        try:
            mtp_rt.H, mtp_rt.FF, mtp_rt.NE = 64, 32, 2
            sys.argv = ["mtp_rt.py", "--gguf", str(path / "toy.gguf"), "--out", str(path / "rt")]
            assert mtp_rt.main() == 0
            assert (path / "rt/dense.bin").read_bytes() == np.ones(64, dtype=np.float32).tobytes()
            assert (path / "rt/experts.q5_0.bin").read_bytes() == b"".join(
                gu[e].tobytes() + dn[e].tobytes() for e in range(2))
        finally:
            mtp_rt.H, mtp_rt.FF, mtp_rt.NE, sys.argv = before
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp)
        rng = np.random.default_rng(45)
        manifest, expected = [], {}
        moments = {'gate_up_proj': np.tile(np.geomspace(.01, 100, 128), (2, 1)),
                   'down_proj': np.tile(np.geomspace(100, .01, 64), (2, 1))}
        for key, shape in (('gate_up_proj', (2, 128, 128)), ('down_proj', (2, 128, 64))):
            raw = (rng.normal(size=shape).astype(np.float32).view(np.uint32) >> 16).astype('<u2')
            tensor = path / (key + '.bin')
            tensor.write_bytes(raw.tobytes())
            name = 'mtp.layers.0.mlp.experts.' + key
            manifest.append(dict(name=name, file=tensor.name, shape=list(shape),
                                 sha256=hashlib.sha256(tensor.read_bytes()).hexdigest()))
            expected[name] = b''.join(q2_0((raw[e].astype(np.uint32) << 16).view(np.float32),
                                         moments[key][e]).tobytes() for e in range(2))
        (path / 'mtp-manifest.json').write_text(json.dumps(manifest), encoding='utf8')
        np.savez(path / 'moments.npz', **moments)
        before = sys.argv
        try:
            sys.argv = ['mtp_pack.py', '--src', str(path), '--experts', 'q2_0', '--q2-imatrix',
                        str(path / 'moments.npz'), '--out', str(path / 'calibrated.gguf')]
            assert mtp_pack.main() == 0
            reader = mtp_pack.gguf.GGUFReader(str(path / 'calibrated.gguf'))
            assert {t.name: t.data.tobytes() for t in reader.tensors} == expected
            del reader
        finally:
            sys.argv = before
    print("PASS MTP Q2_0/Q5_0/Q8_0 layouts, calibrated Q2 objective, input rejection and Q5 GGUF conversion")


if __name__ == "__main__":
    main()
