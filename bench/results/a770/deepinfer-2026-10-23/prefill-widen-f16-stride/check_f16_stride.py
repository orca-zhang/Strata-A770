"""Reproduce the padded-output regression, then check the minimal fallback fix serially."""
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
ap.add_argument('--out', type=Path, required=True)
ap.add_argument('--reuse', type=Path, required=True)
a = ap.parse_args()
F = a.out.resolve()
B = F/'build'

with b.gpu_lock(), patch.object(b, 'gpu_lock', contextlib.nullcontext):
    prior = json.loads((a.reuse/'results.json').read_text())
    assert len(prior['runs']) == 2
    assert prior['runs'][0]['returncode'] == 0
    assert prior['runs'][0]['summary'] == [{'checks':1015, 'mismatched_cases':0}]
    assert prior['runs'][1]['returncode'] == 2
    assert prior['runs'][1]['summary'][0]['checks'] == 1015
    assert prior['runs'][1]['summary'][0]['mismatched_cases'] > 0
    F.mkdir(exist_ok=False)
    (F/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    data = {'complete':False, 'functional_only':True, 'prior_failure_sha256':b.sha(a.reuse/'results.json'),
            'requested_expert_cache':4000, 'expert_slots':4191, 'expert_cache_mib':5760, 'steps':[]}
    b.save(F/'results.json', data)

    def run(name, args):
        sys.argv = [str(H/name), *map(str,args)]
        print('STEP', *sys.argv, flush=True)
        runpy.run_path(sys.argv[0], run_name='__main__')
        data['steps'].append(sys.argv.copy())
        b.save(F/'results.json', data)

    run('build_prefill_widen.py', ['--reuse',a.reuse,'--checks',1015,'--out',B])
    for offset in (0,4):
        run('test_gemm_arena.py', ['--build-root',B,'--offset-gib',offset,'--checks',1015,'--out',F/f'arena{offset}'])
    for directory in (B,F/'arena0',F/'arena4'):
        result = json.loads((directory/'results.json').read_text())
        assert result['complete'] and len(result['runs'])==2
        assert all(r['returncode']==0 and r['summary']==[{'checks':1015,'mismatched_cases':0}] for r in result['runs'])
    for case,length in [('Code2K',256),('Prose2K',512)]:
        out = F/f'{case}-{length}'
        common = ['--build-root',B,'--phase','model','--cases',case,'--prefix-tokens',length,
                  '--keep-tail-tokens',64,'--max-tokens',8,'--expert-cache',4000,'--expected-cache','4191,5760']
        reference = H/'prefill-widen-bf16-batch-chat-fixed-cache'/out.name/'results.json'
        run('test_prefill_widen.py', [*common,'--dtype','f16','--switches','0,1','--reference',reference,'--out',out])
        result = json.loads((out/'results.json').read_text())
        assert result['complete'] and result['functional_acceptance'] and result['outputs_equal_reference']
        assert all(s['generated']==8 and s['prompt_tokens']==length for r in result['runs'] for s in r['requests'])
        for i in (0,1):
            trace = (out/f'{i}.stderr').read_text(encoding='utf-8')
            batches = sorted(set((int(p),int(t)) for p,t in re.findall(r'prefill layer 0, position (\d+), rows (\d+),',trace)))
            assert batches==([(0,255)] if length==256 else [(0,256),(256,255)]), batches
    # Only Prose needs a fresh combination check: it exercises two prefill chunks.
    run('test_prefill_widen.py', [*common,'--dtype','bf16','--switches','1',
                                '--reference',out/'results.json','--out',F/'combined-Prose2K-512'])
    result = json.loads((F/'combined-Prose2K-512/results.json').read_text())
    assert result['complete'] and result['functional_acceptance'] and result['outputs_equal_reference']
    assert all(s['generated']==8 for r in result['runs'] for s in r['requests'])
    data.update(complete=True, functional_acceptance=True, checks_per_arm=1015, total_comparisons=6090,
                model_speed_claim=False, default_switch='off')
    b.save(F/'results.json', data)
    print('PASS FP16 stride fix', flush=True)
