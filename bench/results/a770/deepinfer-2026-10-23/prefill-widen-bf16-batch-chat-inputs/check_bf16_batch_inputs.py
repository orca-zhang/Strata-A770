"""Bounded true batch-prefill checks; compare BF16 off/on without changing production."""
import argparse
import contextlib
import json
import re
import runpy
import sys
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H.parent/'tools'))
import bench_deepinfer as b

ap = argparse.ArgumentParser()
ap.add_argument('--build-root', type=Path, default=H/'prefill-widen-bf16-batches')
ap.add_argument('--out', type=Path, required=True)
ap.add_argument('--keep-tail-tokens', type=int, default=64)
a = ap.parse_args()
assert 0 <= a.keep_tail_tokens < 256
F = a.out.resolve()
B = a.build_root.resolve()
steps = [('Code2K', 256), ('Prose2K', 512)]
with b.gpu_lock(), patch.object(b, 'gpu_lock', contextlib.nullcontext):
    for folder in ('prefill-widen-bf16-batches', 'prefill-widen-bf16-batches-arena0', 'prefill-widen-bf16-batches-arena4'):
        result = json.loads((H/folder/'results.json').read_text())
        assert result['complete'] and all(r['returncode']==0 and r['summary']==[{'checks':725,'mismatched_cases':0}] for r in result['runs'])
    F.mkdir(exist_ok=False)
    (F/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    data = {'complete':False, 'functional_only':True, 'keep_tail_tokens':a.keep_tail_tokens, 'steps':[]}
    b.save(F/'results.json', data)
    for case, length in steps:
        script = H/'test_prefill_widen.py'
        out = F/f'{case}-{length}'
        sys.argv = [str(script), '--build-root', str(B), '--phase', 'model', '--dtype', 'bf16',
                    '--switches', '0,1', '--cases', case, '--prefix-tokens', str(length),
                    '--keep-tail-tokens', str(a.keep_tail_tokens), '--max-tokens', '8', '--out', str(out)]
        print('STEP', *sys.argv, flush=True)
        runpy.run_path(str(script), run_name='__main__')
        result = json.loads((out/'results.json').read_text())
        assert result['complete'] and result['functional_acceptance']
        assert all(s['prompt_tokens']==length and len(s['input_tokens'])==length for r in result['runs'] for s in r['requests'])
        if a.keep_tail_tokens:
            original = b.prompts(json.loads((H/'config-original.json').read_text(encoding='utf-8-sig')))[case]
            assert all(s['input_tokens'][-a.keep_tail_tokens:]==original[-a.keep_tail_tokens:] and s['generated']==8 for r in result['runs'] for s in r['requests'])
        trace_batches = []
        for i in range(2):
            trace = (out/f'{i}.stderr').read_text(encoding='utf-8')
            batches = sorted(set((int(p), int(t)) for p,t in re.findall(r'prefill layer 0, position (\d+), rows (\d+),', trace)))
            assert (0, min(256, length-1)) in batches and (length==256 or (256, 255) in batches), batches
            trace_batches.append(batches)
        data['steps'].append({'case':case, 'input_length':length, 'argv':sys.argv.copy(),
                              'results_sha256':b.sha(out/'results.json'), 'trace_batches':trace_batches})
        b.save(F/'results.json', data)
    data.update(complete=True, functional_acceptance=True)
    b.save(F/'results.json', data)
    print('PASS bounded batch inputs', flush=True)
