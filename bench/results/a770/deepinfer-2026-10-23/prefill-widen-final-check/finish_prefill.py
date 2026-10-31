"""Record final source/dependency/production checks without touching an executable or configuration."""
import datetime
import hashlib
import json
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): digest.update(chunk)
    return digest.hexdigest()
def read(name): return json.loads((H/name).read_text(encoding='utf-8-sig'))

build=read('prefill-widen-output-stage/build.json')
assert build['complete']
sources={src:sha(R/src) for src in build['sources']}
assert sources==build['sources']
for name,value in build['exe_sha256'].items(): assert sha(H/'prefill-widen-output-stage'/name/'strata.exe')==value
original=read('prefill-widen/build.json')
assert build['reused_libraries']==original['reused_libraries']
assert build['sampler_obj_sha256']==original['sampler_obj_sha256']==sha(H/'sampler-fp32/sampler.obj')
dependencies=read('reused-dependencies.json')
assert len(dependencies)==96
assert all(sha(H/'baseline'/path)==value for path,value in dependencies.items())
for path,value in build['reused_libraries'].items(): assert sha(H/'baseline'/path)==value
now=(R/'sycl/src/prefill/gemm.dp.cpp').read_text()
old=(H/'source/sycl/src/prefill/gemm.dp.cpp').read_text()
bf16=lambda text:text[text.index('void Gemm::bf16('):text.index('void Gemm::f16(')]
assert bf16(now)==bf16(old)
cfg=read('config-original.json')
production={'exe':sha(cfg['exe']),'config':sha(R/'strata-iq2_xs.json')}
assert production=={'exe':'8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122',
                    'config':'1a755372f0e6c90c0bfa2af865d67b51f8df49c10aa1613b041b08da8184ca88'}
assert (R/'strata-iq2_xs.json').read_bytes()==(H/'config-original.json').read_bytes()
accepted=[]
for name in ('prefill-widen-output-stage-operators','prefill-widen-output-stage-operators-confirm'):
    result=read(name+'/results.json')
    assert result['complete'] and len(result['runs'])==5
    rows=[s for s in result['comparisons'] if not s['bf16'] and s['meets_screen']]
    assert rows
    accepted.append({'directory':name,'valid_fp16_pairs':rows})
for name in ('prefill-widen-output-stage-model','prefill-widen-output-stage-sampling','prefill-widen-output-stage-arena'):
    result=read(name+'/results.json')
    assert result['complete'] and result['functional_acceptance']
probe=read('mkl-output-stage-probe/results.json')
assert probe['complete'] and probe['returncode']==0 and probe['summary']==[{'checks':16,'mismatched_cases':0}]
result={'complete':True,'date':datetime.datetime.now().isoformat(timespec='seconds'),
        'source_sha256':sources,'build_sha256':sha(H/'prefill-widen-output-stage/build.json'),
        'extra_source_sha256':{src:sha(R/src) for src in ('sycl/CMakeLists.txt','docs/DEEPINFER_OPTIMIZATIONS.md','sycl/src/kernels/cuda/sampler.dp.cpp')},
        'unchanged_dependency_objects_libraries':96,'bf16_function_unchanged':True,
        'production_sha256':production,'production_unchanged':True,'operator_confirmation':accepted,
        'default_switch':'off','model_speed_claim':False}
F=H/'prefill-widen-final-check'
F.mkdir(exist_ok=True)
(F/'finish_prefill.py').write_bytes(Path(__file__).read_bytes())
(F/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
