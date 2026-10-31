"""Run the remaining BF16 checks serially under one shared GPU reservation."""
import contextlib
import runpy
import sys
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b

build = str(H/'prefill-widen-bf16-final')
steps = [
    ('test_prefill_widen.py', ['--phase','model','--dtype','bf16','--switches','0,1',
        '--reference',str(H/'prefill-widen-model-short/results.json'),
        '--out',str(H/'prefill-widen-bf16-model')]),
    ('test_prefill_widen_sampling.py', ['--bf16','--out',str(H/'prefill-widen-bf16-sampling')]),
    ('test_prefill_widen.py', ['--phase','operators','--dtype','bf16','--out',str(H/'prefill-widen-bf16-operators')]),
    ('test_prefill_widen.py', ['--phase','operators','--dtype','bf16','--out',str(H/'prefill-widen-bf16-operators-confirm')]),
]
# All scripts run in this interpreter, so the outer reservation covers their GPU work.
with b.gpu_lock(), patch.object(b, 'gpu_lock', contextlib.nullcontext):
    F = H/'prefill-widen-bf16-sequence'
    F.mkdir(exist_ok=False)
    (F/'check_bf16.py').write_bytes(Path(__file__).read_bytes())
    data = {'complete':False,'steps':[]}
    for script, args in steps:
        sys.argv = [str(H/script),'--build-root',build,*args]
        print('STEP',script,*args,flush=True)
        b.save(F/'results.json',data)
        runpy.run_path(str(H/script),run_name='__main__')
        data['steps'].append(sys.argv.copy())
        b.save(F/'results.json',data)
    data['complete'] = True
    b.save(F/'results.json',data)
    print('PASS BF16 sequence',flush=True)
