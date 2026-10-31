"""Verify and archive the FP16 padded-output fallback regression without binaries."""
import datetime
import json
import re
import shutil
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

F = H/'prefill-widen-f16-stride'
before = read(H/'f16-stride-before/manifest.json')
build = read(F/'build/build.json')
assert build['complete'] and build['sources']=={s:b.sha(R/s) for s in build['sources']}
assert all(b.sha(F/'build'/s/'strata.exe')==v for s,v in build['exe_sha256'].items())
unchanged = {s:v for s,v in before['sources'].items() if s not in ('sycl/src/prefill/gemm.dp.cpp','sycl/src/prefill/gemm_parity.cpp')}
assert all(b.sha(R/s)==v for s,v in unchanged.items())
assert len(before['dependencies'])==96 and all(b.sha(H/'baseline'/s)==v for s,v in before['dependencies'].items())
cfg = read(H/'config-original.json')
production = {'exe':b.sha(cfg['exe']),'config':b.sha(R/'strata-iq2_xs.json')}
assert production==before['production']
old = H/'prefill-widen-f16-stride-before-fix'
assert read(old/'build.json')['sources']['sycl/src/prefill/gemm.dp.cpp']==before['sources']['sycl/src/prefill/gemm.dp.cpp']
assert read(old/'build.json')['sources']['sycl/src/prefill/gemm_parity.cpp']==build['sources']['sycl/src/prefill/gemm_parity.cpp']
failed = read(old/'results.json')['runs']
assert [r['summary'] for r in failed]==[[{'checks':1015,'mismatched_cases':0}],[{'checks':1015,'mismatched_cases':82}]]
assert [r['returncode'] for r in failed]==[0,2]
rows = [json.loads(s[7:]) for s in (old/'1-check.log').read_text().splitlines() if s.startswith('PARITY ')]
assert len(rows)==1008 and all(r['different']==0 for r in rows)
bad = [r for r in rows if r['padding_different']]
assert len(bad)==82 and all(not r['bf16'] and r['beta']==0 and r['ldy']>r['n'] for r in bad)
assert {r['output_offset'] for r in bad}=={0,7}
parity = []
for directory,pattern in [(F/'build','*-check.log'),(F/'arena0','[01].log'),(F/'arena4','[01].log')]:
    result = read(directory/'results.json')
    assert result['complete'] and len(result['runs'])==2
    assert all(r['returncode']==0 and r['summary']==[{'checks':1015,'mismatched_cases':0}] for r in result['runs'])
    logs = sorted(directory.glob(pattern)); assert len(logs)==2
    for log in logs:
        rows = [json.loads(s[7:]) for s in log.read_text().splitlines() if s.startswith('PARITY ')]
        assert len(rows)==1008 and all(r['different']==r['padding_different']==0 for r in rows)
        assert {r['t'] for r in rows}=={1,4,6,22,64,256}
        assert len([r for r in rows if not r['bf16'] and r['output_offset']==7])==144
    parity.append({'directory':directory.relative_to(H).as_posix(),'checks_per_arm':1015,'result_sha256':b.sha(directory/'results.json')})
sequence = read(F/'model-checks/results.json'); assert sequence['complete'] and sequence['functional_acceptance']
interrupted = read(F/'results.json'); assert not interrupted['complete'] and len(interrupted['steps'])==3
assert not read(F/'Code2K-256/results.json')['complete']
outputs = []
for name in ('Short','Code2K-256','Prose2K-512','combined-Short'):
    result = read(F/'model-checks'/name/'results.json')
    assert result['complete'] and result['functional_acceptance'] and result.get('outputs_equal_reference',True)
    requests = [s for r in result['runs'] for s in r['requests']]
    assert all(s['generated']==8 and s['tokens']==requests[0]['tokens'] for s in requests)
    assert len({(s['accepted'],s['offered']) for s in requests})==1
    assert all('expert_slots=4191 ' in r['info'] and 'expert_cache_mib=5760 ' in r['info'] for r in result['runs'])
    if name in ('Code2K-256','Prose2K-512'):
        reference = read(H/'prefill-widen-bf16-batch-chat-fixed-cache'/name/'results.json')
        expected = reference['runs'][0]['requests'][0]
        assert requests[0]['input_tokens']==expected['input_tokens'] and requests[0]['tokens']==expected['tokens']
        assert (requests[0]['accepted'],requests[0]['offered'])==(expected['accepted'],expected['offered'])
        assert all('expert_slots=4191 ' in r['info'] and 'expert_cache_mib=5760 ' in r['info'] for r in reference['runs'])
    trace_batches = []
    for i in range(len(result['runs'])):
        trace = (F/'model-checks'/name/f'{i}.stderr').read_text(encoding='utf-8')
        batches = sorted(set((int(p),int(t)) for p,t in re.findall(r'prefill layer 0, position (\d+), rows (\d+),',trace)))
        assert batches == ([(0,256),(256,255)] if name=='Prose2K-512' else [(0,255)] if name=='Code2K-256' else [(0,22)])
        trace_batches.append(batches)
    outputs.append({'case':name,'arms':len(result['runs']),'generated':8,'input_tokens':len(requests[0]['input_tokens']),
                    'tokens':requests[0]['tokens'],'accepted':requests[0]['accepted'],'offered':requests[0]['offered'],
                    'trace_batches':trace_batches,
                    'result_sha256':b.sha(F/'model-checks'/name/'results.json')})
final = H/'prefill-widen-f16-stride-final-check'
final.mkdir(exist_ok=False)
data = {'complete':True,'date':datetime.datetime.now().isoformat(timespec='seconds'),
        'purpose':'Preserve original FP16 padded-output semantics by retaining the original oneMKL call for ldy != N',
        'regression_cases_before_fix':82,'regression_valid_matrix_differences':0,'regression_padding_float_differences':55772,
        'source_sha256':build['sources'],'unchanged_runtime_sources':unchanged,'build_sha256':b.sha(F/'build/build.json'),
        'binary_sha256':build['exe_sha256'],'unchanged_dependencies':96,'production_unchanged':True,'production_sha256':production,
        'parity':parity,'total_comparisons':6090,'model_outputs':outputs,'expert_slots':4191,'expert_cache_mib':5760,
        'initial_probe_excluded':'Its unchanged-padding assertion was invalid for the original oneMKL fallback; replaced with reference-byte equality',
        'interrupted_model_attempt_excluded':True,'model_comparisons':'Short same-binary FP16 off/on and combined; Code FP16 on and Prose combined compared to frozen references',
        'performance_measured':False,'model_speed_claim':False,'default_switch':'off','full_2k_8k_batch_validation':False,
        'docs_sha256':b.sha(R/'docs/DEEPINFER_OPTIMIZATIONS.md')}
assert sum(r['padding_different'] for r in bad)==data['regression_padding_float_differences']
b.save(final/'results.json',data)
P = R/'bench/results/a770/deepinfer-2026-10-23'
folders = [H/'prefill-widen-f16-stride-probe',old,F,final]
for folder in folders:
    assert not (P/folder.name).exists()
    for src in folder.rglob('*'):
        if src.is_file() and src.suffix in ('.json','.log','.cpp','.hpp','.py','.stderr','.protocol'):
            dst = P/folder.name/src.relative_to(folder)
            dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(src,dst)
for name in ('check_f16_stride.py','resume_f16_stride.py','finish_f16_stride.py','check_bf16_arena.py'):
    shutil.copyfile(H/name,P/'prefill-widen-f16-stride-final-check'/name)
shutil.copyfile(H/'f16-stride-before/manifest.json',P/'prefill-widen-f16-stride-final-check/baseline-manifest.json')
print(json.dumps(data,indent=2),flush=True)
