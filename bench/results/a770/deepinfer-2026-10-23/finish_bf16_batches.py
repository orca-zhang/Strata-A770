"""Check and archive expanded BF16 validation; exclude binaries and unrelated workspace edits."""
import datetime
import json
import shutil
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b

def read(name):
    return json.loads((H/name).read_text(encoding='utf-8-sig'))

before = read('bf16-batches-before/manifest.json')
B = H/'prefill-widen-bf16-batches'
build = read('prefill-widen-bf16-batches/build.json')
assert build['complete'] and build['sources']=={s:b.sha(R/s) for s in build['sources']}
unchanged_sources = {s:v for s,v in before['sources'].items() if s!='sycl/src/prefill/gemm_parity.cpp'}
assert all(b.sha(R/s)==v for s,v in unchanged_sources.items())
assert all(b.sha(B/name/'strata.exe')==v for name,v in build['exe_sha256'].items())
assert build['exe_sha256']['model']==read('prefill-widen-bf16-final/build.json')['exe_sha256']['model']
deps = before['dependencies']
assert len(deps)==96 and all(b.sha(H/'baseline'/s)==v for s,v in deps.items())
cfg = read('config-original.json')
production = {'exe':b.sha(cfg['exe']), 'config':b.sha(R/'strata-iq2_xs.json')}
assert production==before['production']
parity = []
for name in ('prefill-widen-bf16-batches', 'prefill-widen-bf16-batches-arena0', 'prefill-widen-bf16-batches-arena4'):
    result = read(name+'/results.json')
    assert result['complete'] and len(result['runs'])==2
    assert all(r['returncode']==0 and r['summary']==[{'checks':725,'mismatched_cases':0}] for r in result['runs'])
    logs = sorted((H/name).glob('*check.log')) if name=='prefill-widen-bf16-batches' else sorted((H/name).glob('[01].log'))
    assert len(logs)==2
    for log in logs:
        rows = [json.loads(s[7:]) for s in log.read_text().splitlines() if s.startswith('PARITY ')]
        assert len(rows)==720 and {r['t'] for r in rows}=={1,4,6,22,64,256}
        assert all(r['different']==0 for r in rows)
    parity.append({'directory':name, 'checks_per_arm':725, 'sha256':b.sha(H/name/'results.json')})
assert read('prefill-widen-bf16-batches-sequence/results.json')['complete']
prototype = read('prefill-widen-bf16-batch-inputs/results.json')
assert prototype['complete']
M = H/'prefill-widen-bf16-batch-chat-fixed-cache'
model = json.loads((M/'results.json').read_text())
assert model['complete'] and model['functional_acceptance']
assert model['keep_tail_tokens']==64
assert model['expected_cache']=='4191,5760' and model['requested_expert_cache']==4000
window = json.loads((M/'window-reference/results.json').read_text())
assert window['complete'] and window['functional_acceptance'] and window['outputs_equal_reference']
assert b.sha(M/'window-reference/results.json')==model['window_reference_sha256']
assert all('expert_slots=4191 ' in r['info'] and 'expert_cache_mib=5760 ' in r['info'] for r in window['runs'])
outputs = []
for step in model['steps']:
    path = M/f"{step['case']}-{step['input_length']}"/'results.json'
    assert b.sha(path)==step['results_sha256']
    result = json.loads(path.read_text())
    assert result['complete'] and result['functional_acceptance']
    assert all('expert_slots=4191 ' in r['info'] and 'expert_cache_mib=5760 ' in r['info'] for r in result['runs'])
    requests = [s for r in result['runs'] for s in r['requests']]
    assert all(s['tokens']==requests[0]['tokens'] and s['generated']==8 for s in requests)
    assert all(s['finish']==requests[0]['finish'] and (s['generated']==8 or s['finish']=='stop') for s in requests)
    assert len({(s['accepted'],s['offered']) for s in requests})==1
    outputs.append({'case':step['case'], 'input_tokens':step['input_length'], 'output_limit':8,
                    'output_tokens':requests[0]['generated'], 'finish':requests[0]['finish'],
                    'tokens':requests[0]['tokens'], 'accepted':requests[0]['accepted'], 'offered':requests[0]['offered'],
                    'trace_batches':step['trace_batches'], 'results_sha256':step['results_sha256']})
F = H/'prefill-widen-bf16-batches-final-check'
F.mkdir(exist_ok=False)
result = {'complete':True, 'date':datetime.datetime.now().isoformat(timespec='seconds'),
          'purpose':'Expanded batch and bounded prompt correctness only; no new performance measurements',
          'source_sha256':build['sources'], 'unchanged_runtime_sources':unchanged_sources,
          'build_sha256':b.sha(B/'build.json'), 'model_binary_unchanged':True,
          'unchanged_dependency_objects_libraries':96, 'production_sha256':production,
          'production_unchanged':True, 'parity':parity, 'total_comparisons':4350, 'model_outputs':outputs,
          'input_construction':'First (length-64) tokens plus original final 64 tokens; preserve instruction and assistant template',
          'prefix_only_prototype_is_not_generation_acceptance':True,
          'prefix_only_prototype_sha256':b.sha(H/'prefill-widen-bf16-batch-inputs/results.json'),
          'window_reader_output_matches_batch':True,
          'window_reference_sha256':b.sha(M/'window-reference/results.json'),
          'rejected_cache_comparison_sha256':b.sha(H/'bf16-batch-chat-cache-rejection/results.json'),
          'expert_slots':4191, 'expert_cache_mib':5760, 'default_switch':'off',
          'full_2k_8k_batch_validation':False, 'model_speed_claim':False,
          'docs_sha256':b.sha(R/'docs/DEEPINFER_OPTIMIZATIONS.md')}
b.save(F/'results.json', result)
P = R/'bench/results/a770/deepinfer-2026-10-23'
folders = [H/name for name in ('prefill-widen-bf16-batches', 'prefill-widen-bf16-batches-arena0',
           'prefill-widen-bf16-batches-arena4', 'prefill-widen-bf16-batches-sequence',
           'prefill-widen-bf16-batch-inputs', 'prefill-widen-bf16-batch-chat-inputs',
           'prefill-widen-bf16-batch-chat-window-reference', 'prefill-widen-bf16-batch-chat-fixed-reference',
           'bf16-batch-chat-cache-rejection', 'prefill-widen-bf16-batch-chat-fixed-cache',
           'prefill-widen-bf16-batches-final-check')]
for folder in folders:
    assert not (P/folder.name).exists()
    for src in folder.rglob('*'):
        if src.is_file() and src.suffix in ('.json','.log','.cpp','.hpp','.py','.stderr','.protocol'):
            dst = P/folder.name/src.relative_to(folder)
            dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(src,dst)
for name in ('check_bf16_batches.py','check_bf16_batch_inputs.py','finish_bf16_batches.py'):
    shutil.copyfile(H/name,P/name)
shutil.copyfile(H/'bf16-batches-before/manifest.json',P/'prefill-widen-bf16-batches-final-check/baseline-manifest.json')
shutil.copyfile(R/'tools/bench_deepinfer.py',P/'prefill-widen-bf16-batches-final-check/bench_deepinfer.py')
shutil.copyfile(H/'check_bf16_arena.py',P/'prefill-widen-bf16-batches-final-check/check_bf16_arena.py')
print(json.dumps(result,indent=2),flush=True)
