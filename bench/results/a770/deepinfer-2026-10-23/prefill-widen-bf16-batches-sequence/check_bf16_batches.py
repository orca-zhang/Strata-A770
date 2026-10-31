"""Check expanded GEMM batches serially using the existing isolated build scripts."""
import contextlib
import runpy
import sys
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H.parent/'tools'))
import bench_deepinfer as b

F = H/'prefill-widen-bf16-batches-sequence'
B = H/'prefill-widen-bf16-batches'
steps = [
    ('build_prefill_widen.py', ['--reuse', str(H/'prefill-widen-bf16-final'), '--checks', '725', '--out', str(B)]),
    ('test_gemm_arena.py', ['--build-root', str(B), '--offset-gib', '0', '--checks', '725', '--out', str(H/'prefill-widen-bf16-batches-arena0')]),
    ('test_gemm_arena.py', ['--build-root', str(B), '--offset-gib', '4', '--checks', '725', '--out', str(H/'prefill-widen-bf16-batches-arena4')]),
]
with b.gpu_lock(), patch.object(b, 'gpu_lock', contextlib.nullcontext):
    F.mkdir(exist_ok=False)
    (F/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    data = {'complete': False, 'steps': []}
    b.save(F/'results.json', data)
    for name, args in steps:
        sys.argv = [str(H/name), *args]
        print('STEP', *sys.argv, flush=True)
        runpy.run_path(str(H/name), run_name='__main__')
        data['steps'].append(sys.argv.copy())
        b.save(F/'results.json', data)
    data['complete'] = True
    b.save(F/'results.json', data)
    print('PASS expanded batches', flush=True)
