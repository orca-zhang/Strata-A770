"""Verify the BF16 experiment and publish source/logs, never binaries."""
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

build = read('prefill-widen-bf16-final/build.json')
assert build['complete']
assert build['sources'] == {src:b.sha(R/src) for src in build['sources']}
for name, value in build['exe_sha256'].items():
    assert b.sha(H/'prefill-widen-bf16-final'/name/'strata.exe') == value
before = read('bf16-widen-before/manifest.json')['sources']
for src in ('include/strata/prefill/gemm.hpp', 'sycl/src/prefill/prefill.cpp', 'sycl/src/kernels/cuda/sampler.dp.cpp'):
    assert b.sha(R/src) == before[src]
current = (R/'sycl/src/prefill/gemm.dp.cpp').read_text()
prior = (H/'bf16-widen-before/sycl/src/prefill/gemm.dp.cpp').read_text()
f16 = lambda s:s[s.index('void Gemm::f16('):]
assert f16(current) == f16(prior)
dependencies = read('reused-dependencies.json')
assert len(dependencies) == 96
assert all(b.sha(H/'baseline'/name) == value for name,value in dependencies.items())
assert build['sampler_obj_sha256'] == b.sha(H/'sampler-fp32/sampler.obj')
cfg = read('config-original.json')
production = {'exe':b.sha(cfg['exe']), 'config':b.sha(R/'strata-iq2_xs.json')}
assert production == {'exe':'8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122',
                      'config':'1a755372f0e6c90c0bfa2af865d67b51f8df49c10aa1613b041b08da8184ca88'}
assert (R/'strata-iq2_xs.json').read_bytes() == (H/'config-original.json').read_bytes()
assert read('prefill-widen-bf16-final/results.json')['complete']
prototype = []
for line in (H/'prefill-widen-bf16/1-check.log').read_text().splitlines():
    if line.startswith('PARITY '):
        row = json.loads(line[7:])
        if row['different']:
            prototype.append(row)
assert len(prototype)==3 and all(s['t']==s['n']==1 and s['beta']==0 for s in prototype)
accept = []
for suffix in ('operators','operators-confirm'):
    result = read('prefill-widen-bf16-'+suffix+'/results.json')
    assert result['complete'] and result['dtype']=='bf16' and len(result['runs'])==5
    pairs = [s for s in result['comparisons'] if s['bf16'] and s['optimized'] and s['meets_screen']]
    assert pairs
    accept.append({'directory':'prefill-widen-bf16-'+suffix,'valid_pairs':pairs})
for suffix in ('arena','model','sampling'):
    result = read('prefill-widen-bf16-'+suffix+'/results.json')
    assert result['complete'] and result['functional_acceptance']
    if suffix != 'arena':
        requests = [s for row in result['runs'] for s in row['requests']]
        assert len({(s['accepted'],s['offered']) for s in requests})==1
result = {'complete':True,'date':datetime.datetime.now().isoformat(timespec='seconds'),
          'source_sha256':build['sources'],'build_sha256':b.sha(H/'prefill-widen-bf16-final/build.json'),
          'extra_source_sha256':{s:b.sha(R/s) for s in ('sycl/CMakeLists.txt','docs/DEEPINFER_OPTIMIZATIONS.md','tools/bench_deepinfer.py')},
          'unchanged_dependency_objects_libraries':96,'f16_native_functions_unchanged':True,
          'scalar_dot_prototype_mismatches':prototype,
          'production_sha256':production,'production_unchanged':True,'operator_confirmation':accept,
          'default_switch':'off','switch':'STRATA_PREFILL_BF16_GPU_WIDEN=1',
          'restriction':'BF16 ldy == N, inputs fit existing workspace; preserve original interleaved-output path',
          'model_speed_claim':False}
F = H/'prefill-widen-bf16-final-check'
F.mkdir(exist_ok=True)
(F/'finish_bf16.py').write_bytes(Path(__file__).read_bytes())
b.save(F/'results.json',result)
P = R/'bench/results/a770/deepinfer-2026-10-23'
for src in [*H.glob('prefill-widen-bf16*'), H/'bf16-widen-before']:
    for item in src.rglob('*'):
        if item.is_file() and item.suffix in ('.json','.log','.cpp','.hpp','.py','.stderr','.protocol'):
            dst = P/src.name/item.relative_to(src)
            dst.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(item,dst)
for name in ('finish_bf16.py','check_bf16.py','check_bf16_arena.py','check_lock_retry.py','build_prefill_widen.py','test_prefill_widen.py','test_prefill_widen_sampling.py','test_gemm_arena.py'):
    shutil.copyfile(H/name,P/name)
shutil.copyfile(R/'tools/bench_deepinfer.py',P/'prefill-widen-bf16-final-check/bench_deepinfer.py')
print(json.dumps(result,indent=2))
