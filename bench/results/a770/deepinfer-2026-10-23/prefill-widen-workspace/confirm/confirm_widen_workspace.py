"""Confirm large-workspace GEMMs and batch-prefill output with frozen references."""
import contextlib
import json
import re
import runpy
import shutil
import sys
import subprocess
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b

P = H / 'prefill-widen-workspace-v2'
F = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else H / 'prefill-widen-workspace-confirm'
screen = json.loads((P / 'results.json').read_text())
assert screen['complete']
F.mkdir(exist_ok=False)
(F / 'confirm_widen_workspace.py').write_bytes(Path(__file__).read_bytes())
shutil.copytree(P / 'source', F / 'source')
gemm = F / 'source/gemm.dp.cpp'
s = gemm.read_text(encoding='utf-8')
old = 'std::string("prefill gemm: workspace of 32 MiB: ")'
assert s.count(old) == 1
gemm.write_text(s.replace(old, 'std::string("prefill gemm: workspace of ") + std::to_string(ws >> 20) + " MiB: "'), encoding='utf-8')
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
data = {'complete': False, 'screen_sha256': b.sha(P / 'results.json'), 'runs': [], 'comparisons': [],
        'greedy': [], 'sampling': [], 'model_diagnostic_only': True,
        'note': 'No model speed claim. Same frozen dependencies and FP32 sampler. Native half-output and DeepInfer experiments disabled.'}
b.save(F / 'results.json', data)

def env(reproducible=False):
    e = b.environment(cfg, reproducible=reproducible)
    e.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1',
             STRATA_PREFILL_HALF_OUTPUT='0', STRATA_PREFILL_HALF_BF16='0', STRATA_PREFILL_HALF_PROFILE='0')
    return e

with b.gpu_lock():
    with patch.object(b, 'gpu_lock', contextlib.nullcontext):
        sys.argv = [str(H / 'build_prefill_widen.py'), '--source-build', str(F / 'source'),
                    '--reuse', str(P / 'build'), '--checks', '1015', '--out', str(F / 'build')]
        runpy.run_path(sys.argv[0], run_name='__main__')
    C = F / 'build'
    data['candidate_build_sha256'] = b.sha(C / 'build.json')
    for i, candidate in enumerate((False, True, False, True, False)):
        b.busy()
        exe = (C if candidate else P / 'baseline') / 'parity/strata.exe'
        print('CONFIRM', i, candidate, flush=True)
        p = subprocess.run([str(exe), '--bench', '--large'], cwd=R, env=env(), capture_output=True, text=True, timeout=240)
        (F / f'operator-{i}.log').write_text(p.stdout + p.stderr, encoding='utf-8')
        row = {'candidate': candidate, 'exe_sha256': b.sha(exe), 'returncode': p.returncode,
               'benches': [json.loads(v) for v in re.findall(r'^BENCH (.+)$', p.stdout, re.M)],
               'batches': [json.loads(v) for v in re.findall(r'^BATCH (.+)$', p.stdout, re.M)],
               'summary': [json.loads(v) for v in re.findall(r'^SUMMARY (.+)$', p.stdout, re.M)]}
        data['runs'].append(row)
        b.save(F / 'results.json', data)
        p.check_returncode()
        assert row['summary'] == [{'checks':12,'mismatched_cases':0}]
        assert len(row['benches']) == 4 and len(row['batches']) == 20
    for i in (1,3):
        for bf16 in (False,True):
            for n in (4096,12288):
                times = [next(v['total_us'] for v in data['runs'][j]['benches'] if v['bf16']==bf16 and v['n']==n) for j in (i-1,i,i+1)]
                pair = b.paired(*times)
                data['comparisons'].append({'candidate_run':i,'bf16':bf16,'n':n,**pair,
                                            'gate':pair['throughput_change_pct']>=3 and abs(pair['baseline_drift_pct'])<=3})
    for bf16 in (False,True):
        for n in (4096,12288):
            assert any(v['gate'] for v in data['comparisons'] if v['bf16']==bf16 and v['n']==n)
    b.save(F / 'results.json', data)

    refs = []
    for case in ('combined-Short', 'Code2K-256', 'Prose2K-512'):
        refpath = H / 'prefill-widen-f16-stride/model-checks' / case / 'results.json'
        reference = json.loads(refpath.read_text())
        row = reference['runs'][0]
        assert all(r['requests'][0]['tokens']==row['requests'][0]['tokens'] for r in reference['runs'])
        refs.append((case, refpath, row))
    argv = refs[0][2]['argv']
    assert all(row['argv']==argv for _,_,row in refs)
    selected = env()
    selected['STRATA_TRACE'] = '1'
    engine = b.Engine(C / 'model/strata.exe', argv, selected, F / 'greedy')
    data['greedy_args'] = argv
    data['greedy_info'] = engine.info
    data['greedy_env'] = {k:v for k,v in selected.items() if k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
    try:
        assert 'expert_slots=4191 ' in engine.info and 'expert_cache_mib=5760 ' in engine.info
        for case, refpath, row in refs:
            print('MODEL', case, flush=True)
            expected = row['requests'][0]
            result = engine.gen(expected['input_tokens'], 8)
            check = {'case':case, 'reference_sha256':b.sha(refpath), 'input_tokens':expected['input_tokens'],
                     **result, 'outputs_equal_reference':result['tokens']==expected['tokens'],
                     'counts_equal_reference':(result['accepted'],result['offered'])==(expected['accepted'],expected['offered'])}
            data['greedy'].append(check)
            b.save(F / 'results.json', data)
            assert result['generated']==8 and result['reused']==0 and check['outputs_equal_reference'] and check['counts_equal_reference']
    finally:
        engine.close()

    refpath = H / 'prefill-widen-bf16-sampling/results.json'
    reference = json.loads(refpath.read_text())
    expected = reference['runs'][0]['requests'][0]['tokens']
    assert all(r['tokens']==expected for row in reference['runs'] for r in row['requests'])
    data['sampling_reference_sha256'] = b.sha(refpath)
    data['sampling_args'] = reference['argv']
    data['sampling_keys'] = reference['sampling']
    ids = reference['input_tokens']
    selected = env(True)
    engine = b.Engine(C / 'model/strata.exe', reference['argv'], selected, F / 'sampling')
    data['sampling_info'] = engine.info
    data['sampling_env'] = {k:v for k,v in selected.items() if k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
    try:
        assert 'expert_slots=5771 ' in engine.info and 'expert_cache_mib=7922 ' in engine.info
        for i in range(2):
            print('SAMPLE', i, flush=True)
            result = engine.gen(ids, 48, reference['sampling'])
            data['sampling'].append({'input_tokens':ids, **result, 'outputs_equal_reference':result['tokens']==expected})
            b.save(F / 'results.json', data)
            assert result['generated']==48 and result['tokens']==expected
        assert data['sampling'][0]['reused']==0 and data['sampling'][1]['reused']==16
    finally:
        engine.close()
    b.busy()
for path,digest in screen['source_sha256'].items(): assert b.sha(R/path)==digest,path
for path,digest in screen['production_sha256'].items(): assert b.sha(Path(path))==digest,path
for path,digest in screen['dependencies'].items(): assert b.sha(H/'baseline'/path)==digest,path
data.update(complete=True, production_unchanged=True, workspace_sources_unchanged=True,
            functional_acceptance=True, unchanged_dependencies=len(screen['dependencies']))
b.save(F / 'results.json', data)
print('PASS independent operator confirmation, three bounded batch inputs, seeded sampling and replay', flush=True)
