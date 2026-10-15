"""Offline Q2_0 plane repack and ternary study; never modifies a model or engine.

python tools/q2_layout_probe.py --self-test
python tools/q2_layout_probe.py --gguf MODEL.gguf --out RESULTS
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from _paths import add_gguf_py
from canonical_xcheck import from_s2, to_s2
from gguf_reader import GGUFFile


def repack(raw):
    """Keep packed code bytes and FP16 scale bits, in separate contiguous planes."""
    if not raw or len(raw) % 18:
        raise ValueError('Q2_0 needs nonempty complete 18-byte blocks')
    a = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 18)
    return a[:, 2:].copy(), a[:, :2].copy().view('<u2').reshape(-1)


def restore(codes, scales):
    a = np.empty((len(scales), 18), dtype=np.uint8)
    a[:, :2] = scales.view(np.uint8).reshape(-1, 2)
    a[:, 2:] = codes
    return a.tobytes()


def ternary64(codes, scales):
    # ponytail: fit the existing four-level weights; this cannot assess original BF16 model accuracy.
    symbols = codes.astype(np.int8) - 1
    nz = np.count_nonzero(symbols, axis=1)
    factor = np.abs(symbols).sum(axis=1) / np.maximum(nz, 1)
    d = (scales * factor).astype(np.float16)
    if not np.isfinite(d).all():
        raise ValueError('Ternary scale overflows FP16')
    q = np.clip(symbols, -1, 1)
    # Custom 64-element blocks, not GGUF TQ1_0: 13 base-3 bytes + the original scale precision.
    trits = np.pad(q.astype(np.int16) + 1, ((0, 0), (0, 1)), constant_values=1)
    packed = (trits.reshape(-1, 13, 5) * np.array([1, 3, 9, 27, 81])).sum(axis=2).astype(np.uint8)
    return packed, d


def decode_ternary64(packed, scales):
    q = (packed[:, :, None].astype(np.int16) // np.array([1, 3, 9, 27, 81])) % 3 - 1
    return q.reshape(-1, 65)[:, :64].astype(np.float32) * scales.astype(np.float32)[:, None]


def self_test():
    rng = np.random.default_rng(7)
    a = rng.integers(0, 256, (9, 18), dtype=np.uint8)
    d = np.array([0., -0., 1., -1., 2., -2., .125, -.125, .001], dtype=np.float16)
    a[:, :2] = d.view(np.uint8).reshape(-1, 2)
    a[2, 2:] = 0xE4  # {-1, 0, 1, 2} in every byte; exercises the fourth level.
    raw = a.tobytes()
    cp, sp = repack(raw)
    assert restore(cp, sp) == raw
    c, s = to_s2(raw)
    decoded = (cp[:, :, None] >> np.array([0, 2, 4, 6])) & 3
    assert np.array_equal(decoded.reshape(-1, 64), c)
    assert np.array_equal(sp, d.view('<u2'))
    packed, scale = ternary64(c, s)
    got = decode_ternary64(packed, scale)
    expected = np.clip(c.astype(np.int8) - 1, -1, 1).astype(np.float32) * scale.astype(np.float32)[:, None]
    assert np.array_equal(got.view(np.uint32), expected.view(np.uint32))
    assert packed.nbytes + scale.nbytes == len(c) * 15
    assert np.isclose(float(scale[2]), 4 / 3, atol=.001)
    for bad in (b'', b'\x00', raw[:-1]):
        try:
            repack(bad)
        except ValueError:
            pass
        else:
            raise AssertionError('Malformed block accepted')
    print('PASS: exact plane roundtrip, all four codes, signed scales/zero, ternary packing, malformed inputs')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('--gguf', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--experts', default='0,127,255,511')
    args = ap.parse_args()
    if args.self_test:
        self_test(); return
    if not args.gguf or not args.out:
        ap.error('--gguf and --out are required')
    ids = [int(v) for v in args.experts.split(',')]
    if not ids or len(ids) != len(set(ids)) or min(ids) < 0:
        ap.error('Expert IDs must be distinct nonnegative integers')
    if args.out.exists():
        ap.error('Use a new output directory to preserve previous results')
    g = GGUFFile(args.gguf)
    tensors = [t for t in g.tensors if 'ffn_down_exps' in t.name]
    if not tensors:
        ap.error('No routed down tensors in this shard')
    for t in tensors:
        if t.type_id != 42 or len(t.shape) != 3 or t.shape[0] % 64 or max(ids) >= t.shape[2]:
            ap.error(f'Unsupported tensor shape/type or expert ID: {t.name}')
        if g.data_start + t.offset + t.expected_bytes() > args.gguf.stat().st_size:
            ap.error(f'Truncated tensor: {t.name}')
    add_gguf_py()
    from gguf import GGMLQuantizationType as Q, quants
    args.out.mkdir(parents=True)
    rows = []
    total_counts = np.zeros(4, dtype=np.int64)
    sums = dict(reference_energy=0., ternary64_error=0., standard_tq_padded_error=0.)
    with args.gguf.open('rb') as f:
        for t in tensors:
            k, nout, nexpert = t.shape
            expert_bytes = t.expected_bytes() // nexpert
            for eid in ids:
                f.seek(g.data_start + t.offset + eid * expert_bytes)
                raw = f.read(expert_bytes)
                if len(raw) != expert_bytes:
                    raise ValueError(f'Truncated expert {t.name}/{eid}')
                cp, sp = repack(raw)
                assert restore(cp, sp) == raw
                c, s = to_s2(raw)
                decoded = ((cp[:, :, None] >> np.array([0, 2, 4, 6])) & 3).reshape(-1, 64)
                assert np.array_equal(decoded, c)
                assert np.array_equal(sp, s.astype(np.float16).view('<u2'))
                w = from_s2((c, s)).reshape(nout, k)
                if not np.isfinite(w).all():
                    raise ValueError(f'Nonfinite weight {t.name}/{eid}')
                packed, d = ternary64(c, s)
                tw = decode_ternary64(packed, d).reshape(nout, k)
                padded = np.pad(w, ((0, 0), (0, (-k) % 256)))
                tq = quants.quantize(padded, Q.TQ2_0)
                tqw = quants.dequantize(tq, Q.TQ2_0)[:, :k]
                energy = float(np.sum(w.astype(np.float64) ** 2))
                err = float(np.sum((w.astype(np.float64) - tw) ** 2))
                tqerr = float(np.sum((w.astype(np.float64) - tqw) ** 2))
                counts = np.bincount(c.reshape(-1), minlength=4)
                total_counts += counts
                sums['reference_energy'] += energy
                sums['ternary64_error'] += err
                sums['standard_tq_padded_error'] += tqerr
                row = dict(tensor=t.name, expert=eid, raw_sha256=hashlib.sha256(raw).hexdigest(),
                           code_counts=counts.tolist(), blocks_with_fourth_level=int(np.any(c == 3, axis=1).sum()),
                           blocks=len(c), raw_bytes=len(raw), repack_bytes=cp.nbytes + sp.nbytes,
                           ternary64_bytes=packed.nbytes + d.nbytes, standard_tq2_padded_bytes=tq.nbytes,
                           standard_tq1_padded_bytes=nout * (padded.shape[1] // 256) * 54,
                           reference_energy=energy, ternary64_error=err, standard_tq_padded_error=tqerr)
                rows.append(row)
                if len(rows) == 1:
                    tq1 = quants.quantize(padded, Q.TQ1_0)
                    assert np.array_equal(quants.dequantize(tq1, Q.TQ1_0), quants.dequantize(tq, Q.TQ2_0))
                    np.savez(args.out / 'first-expert.npz', raw=np.frombuffer(raw, dtype=np.uint8),
                             q2_codes=cp, q2_scale_bits=sp, ternary64_codes=packed, ternary64_scales=d)
                if len(rows) % 16 == 0:
                    print('samples', len(rows), '/', len(tensors) * len(ids), flush=True)
    denom = sums['reference_energy']
    result = dict(source=str(args.gguf.resolve()), source_file_bytes=args.gguf.stat().st_size,
                  samples=len(rows), layers=len(tensors), expert_ids=ids, code_counts=total_counts.tolist(),
                  ternary64_relative_weight_l2=math.sqrt(sums['ternary64_error'] / denom) if denom else 0.,
                  standard_tq_padded_relative_weight_l2=math.sqrt(sums['standard_tq_padded_error'] / denom) if denom else 0.,
                  sums=sums, scope='offline weights vs existing Q2_0; no GPU timing, BF16 oracle or model quality evaluation',
                  repack='packed 2-bit code plane + unchanged FP16 scale bits; exact original-byte roundtrip', rows=rows)
    (args.out / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('rows', 'sums')}, indent=2))


if __name__ == '__main__':
    main()
