"""Experimental FP16/BF16 matrix -> A770 native U4 DPAS weights (no Q2 input).

python tools/u4s4_pack.py weights.bf16 weights.u4 --type bf16 --rows 320 --cols 10240 --digits 2
python tools/u4s4_pack.py up.bf16 up.u4 --type bf16 --rows 10240 --cols 320 --radix
python tools/u4s4_pack.py --selftest
Payload: digit-major B DWORDs, then digit-major FP32 group scales. Header: <8s6I.
One digit is W4; two digits are W4 plus W4 residual (8 payload bits/weight).
v001 is consumed by u4s4_projection_check --native, not by the model loader.
--radix writes v002: two U4 digits with ratio16 and one signed FP32 high scale per group
(8.5 payload+scale bits/weight). v001 readers reject it; a v002 consumer is required.
Add --radix-positive to reproduce maxabs/119 and q[-119,119] for the up95/affine candidates.
"""
import argparse
import os
from pathlib import Path
import struct

import numpy as np

MAGIC = b'U4S4v001'
RADIX_MAGIC = b'U4S4v002'


def quantize(raw, kind, rows, cols, digits, radix=False, radix_positive=False):
    if kind not in ('fp16', 'bf16') or digits not in (1, 2):
        raise ValueError('expected fp16/bf16 and 1/2 digits')
    if radix and digits != 2:
        raise ValueError('radix requires two U4 digits')
    if radix_positive and not radix:
        raise ValueError('positive radix scale requires radix')
    if not (0 < rows <= 16384 and rows % 8 == 0 and 0 < cols <= 16384 and cols % 64 == 0
            and rows * cols <= 16777216):
        raise ValueError('N multiple of 8, K multiple of 64; each <=16384 and N*K <=16M')
    if len(raw) != rows * cols * 2:
        raise ValueError('matrix byte size does not match N*K*2')
    bits = np.frombuffer(raw, dtype='<u2')
    values = (bits.view('<f2').astype(np.float32) if kind == 'fp16'
              else (bits.astype(np.uint32) << 16).view(np.float32))
    if not np.isfinite(values).all():
        raise ValueError('nonfinite source weights')
    values = values.reshape(rows // 8, 8, cols // 64, 64).transpose(0, 2, 1, 3).copy()
    if radix:
        positive = np.maximum(np.max(values, axis=-1), np.float32(0))
        negative = np.maximum(-np.min(values, axis=-1), np.float32(0))
        forward = np.maximum(positive / np.float32(119), negative / np.float32(136))
        reverse = np.maximum(positive / np.float32(136), negative / np.float32(119))
        base = np.where(forward <= reverse, np.maximum(forward, np.float32(2**-126)),
                        -np.maximum(reverse, np.float32(2**-126)))
        base = np.where((positive > 0) | (negative > 0), base, np.float32(1))
        if radix_positive:
            maximum = np.maximum(positive, negative)
            base = np.where(maximum > 0, np.maximum(maximum / np.float32(119), np.float32(2**-126)), np.float32(1))
        quant = np.clip(np.rint(values / base[..., None]), -119 if radix_positive else -136, 119).astype(np.int32)
        high = (quant + 8) // 16
        words = []
        for digit in (high, quant - 16 * high):
            code = (digit + 8).astype(np.uint32).reshape(rows // 8, cols // 64, 8, 8, 8)
            packed = np.bitwise_or.reduce(code << (np.arange(8, dtype=np.uint32) * 4), axis=-1)
            words.append(packed.transpose(0, 1, 3, 2).astype('<u4').tobytes())
        return (struct.pack('<8s6I', RADIX_MAGIC, rows, cols, 2, int(kind == 'bf16'), 64, 0)
                + b''.join(words) + (base * np.float32(16)).astype('<f4').tobytes())
    words, scales = [], []
    for _ in range(digits):
        maximum = np.max(np.abs(values), axis=-1)
        scale = np.where(maximum > 0, np.maximum(maximum / np.float32(7), np.float32(2**-126)), np.float32(1))
        quant = np.clip(np.rint(values / scale[..., None]), -7, 7).astype(np.int32)
        code = (quant + 8).astype(np.uint32).reshape(rows // 8, cols // 64, 8, 8, 8)
        packed = np.bitwise_or.reduce(code << (np.arange(8, dtype=np.uint32) * 4), axis=-1)
        words.append(packed.transpose(0, 1, 3, 2).astype('<u4').tobytes())
        scales.append(scale.astype('<f4').tobytes())
        # q has <=3 bits and both inputs are FP32: double gives the exact product/difference before FP32 rounding.
        values = (values.astype(np.float64) - quant.astype(np.float64) * scale[..., None].astype(np.float64)).astype(np.float32)
    return struct.pack('<8s6I', MAGIC, rows, cols, digits, int(kind == 'bf16'), 64, 0) + b''.join(words + scales)


def selftest():
    # Independent scalar decoder checks native B addressing, RNE, scales and the residual.
    for kind in ('fp16', 'bf16'):
        a = np.array([((i * 13) % 39 - 19) / 8 for i in range(8 * 128)], dtype=np.float32)
        raw = a.astype('<f2').tobytes() if kind == 'fp16' else (a.view(np.uint32) >> 16).astype('<u2').tobytes()
        for digits in (1, 2):
            data = quantize(raw, kind, 8, 128, digits)
            words = np.frombuffer(data, '<u4', count=digits * 128, offset=32)
            scales = np.frombuffer(data, '<f4', offset=32 + digits * 128 * 4)
            for row in range(8):
                for g in range(2):
                    values = list(map(float, a.reshape(8, 128)[row, g*64:g*64+64]))
                    for d in range(digits):
                        scale = float(np.float32(max(map(abs, values)) / 7)) or 1.0
                        assert scale == scales[d * 16 + g * 8 + row]
                        for k in range(64):
                            q = round(float(np.float32(values[k] / scale)))
                            w = words[d * 128 + g * 64 + (k // 8) * 8 + row]
                            assert ((int(w) >> ((k % 8) * 4)) & 15) - 8 == q
                            values[k] = float(np.float32(values[k] - q * scale))
            assert np.isfinite(scales).all()
        # Independent scalar v002 decoder: both scale signs, full digit range and RNE half ties.
        a = np.tile(np.arange(-32, 32, dtype=np.float32), (8, 2))
        a[:, :6] = [-136, 119, -.5, .5, -1.5, 1.5]
        a[:, 64:70] = [136, -119, -.5, .5, -1.5, 1.5]
        raw = a.astype('<f2').tobytes() if kind == 'fp16' else (a.view(np.uint32) >> 16).astype('<u2').tobytes()
        data = quantize(raw, kind, 8, 128, 2, radix=True)
        assert struct.unpack('<8s6I', data[:32]) == (RADIX_MAGIC, 8, 128, 2, int(kind == 'bf16'), 64, 0)
        assert len(data) == 32 + 8 * 128 + 8 * 2 * 4
        words = np.frombuffer(data, '<u4', count=256, offset=32)
        scales = np.frombuffer(data, '<f4', offset=32 + 256 * 4)
        assert (scales[:8] == 16).all() and (scales[8:] == -16).all()
        for row in range(8):
            for g in range(2):
                base = float(scales[g*8+row]) / 16
                for col in range(64):
                    decoded = []
                    for digit in range(2):
                        word = int(words[digit*128+g*64+(col//8)*8+row])
                        decoded.append(((word >> ((col%8)*4)) & 15)-8)
                    assert decoded[0]*16+decoded[1] == round(float(a[row,g*64+col])/base)
        data = quantize(raw, kind, 8, 128, 2, radix=True, radix_positive=True)
        words = np.frombuffer(data, '<u4', count=256, offset=32)
        scales = np.frombuffer(data, '<f4', offset=32 + 256 * 4)
        for row in range(8):
            for g in range(2):
                expected = np.float32(max(map(abs, a[row,g*64:g*64+64])) / np.float32(119))
                assert scales[g*8+row] == expected * np.float32(16)
                for col in range(64):
                    decoded = [((int(words[d*128+g*64+(col//8)*8+row]) >> ((col%8)*4)) & 15)-8 for d in range(2)]
                    assert decoded[0]*16+decoded[1] == max(-119, min(119, round(float(np.float32(a[row,g*64+col]/expected)))))
    for bad in (float('nan'), float('inf'), -float('inf')):
        for radix in (False, True):
            try: quantize(np.full(512, bad, dtype='<f2').tobytes(), 'fp16', 8, 64, 2, radix)
            except ValueError: pass
            else: raise AssertionError('nonfinite accepted')
    for raw in (bytes(1024), np.full(512, 0x0080, dtype='<u2').tobytes()):
        for radix in (False, True):
            data = quantize(raw, 'bf16', 8, 64, 2, radix)
            assert np.isfinite(np.frombuffer(data, '<f4', offset=32 + 2 * 64 * 4)).all()
    for rows, cols, digits, raw in ((0, 64, 1, b''), (8, 63, 1, b''), (8, 64, 3, bytes(1024)), (8, 64, 1, b'')):
        try: quantize(raw, 'fp16', rows, cols, digits)
        except ValueError: pass
        else: raise AssertionError('invalid dimensions/payload accepted')
    try: quantize(bytes(1024), 'bf16', 8, 64, 1, radix=True)
    except ValueError: pass
    else: raise AssertionError('one-digit radix accepted')
    try: quantize(bytes(1024), 'bf16', 8, 64, 2, radix_positive=True)
    except ValueError: pass
    else: raise AssertionError('positive scale without radix accepted')
    print('PASS native U4 v001/v002 layouts, FP16/BF16, signed radix/RNE/residual and nonfinite rejection')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', nargs='?', type=Path);p.add_argument('output', nargs='?', type=Path)
    p.add_argument('--type', choices=('fp16', 'bf16'));p.add_argument('--rows', type=int);p.add_argument('--cols', type=int)
    p.add_argument('--digits', type=int, choices=(1, 2), default=2);p.add_argument('--selftest', action='store_true')
    p.add_argument('--radix', action='store_true', help='v002 signed radix16, two digits and one high-scale plane')
    p.add_argument('--radix-positive', action='store_true', help='with --radix: original maxabs/119 positive scale, q[-119,119]')
    args = p.parse_args()
    if args.selftest: selftest();return
    if any(v is None for v in (args.source, args.output, args.type, args.rows, args.cols)):
        p.error('source, output, --type, --rows and --cols are required')
    if args.radix_positive and not args.radix:
        p.error('--radix-positive requires --radix')
    if args.source.resolve() == args.output.resolve() or args.output.exists():
        p.error('output must be a new file')
    if args.source.stat().st_size != args.rows * args.cols * 2:
        p.error('source size must be rows*cols*2 bytes')
    data = quantize(args.source.read_bytes(), args.type, args.rows, args.cols, args.digits, args.radix, args.radix_positive)
    # ponytail: one matrix per sidecar; container-wide conversion waits for model-quality validation.
    # Exclusive creation preserves existing models. Remove an incomplete new sidecar on write failure.
    with args.output.open('xb') as f:
        try: f.write(data);f.flush();os.fsync(f.fileno())
        except BaseException:
            f.close();args.output.unlink();raise
    print(f'Wrote {args.output}: {len(data)} bytes, {args.digits} U4 plane(s)')


if __name__ == '__main__': main()
