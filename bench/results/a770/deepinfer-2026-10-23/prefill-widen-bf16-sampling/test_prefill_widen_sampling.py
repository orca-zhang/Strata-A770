"""Short batch-prefill sampling parity with the same FP32 sampler and fixed seed/penalties."""
import argparse
import json
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b
ap = argparse.ArgumentParser()
ap.add_argument('--build-root',type=Path,default=H/'prefill-widen-output-stage')
ap.add_argument('--out',type=Path,default=H/'prefill-widen-output-stage-sampling')
ap.add_argument('--bf16',action='store_true')
a = ap.parse_args()
B = a.build_root.resolve()
F = a.out.resolve()
exe = B/'model/strata.exe'
build = json.loads((B/'build.json').read_text())
assert build['complete'] and b.sha(exe)==build['exe_sha256']['model']
cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
argv = b.model_args(cfg,False,.5,5500)
argv[argv.index('--prefill')+1]='256'
argv[argv.index('--short-read')+1]='0'
argv[argv.index('--prompt-cache')+1]='128'
argv += ['--kv','int8']
ids = b.prompts(cfg)['Short']
keys = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128'
with b.gpu_lock():
    F.mkdir(exist_ok=False)
    (F/'test_prefill_widen_sampling.py').write_bytes(Path(__file__).read_bytes())
    data = {'complete':False,'functional_only':True,'bf16':a.bf16,'exe_sha256':b.sha(exe),'build_sha256':b.sha(B/'build.json'),
            'argv':argv,'sampling':keys,'input_tokens':ids,'runs':[]}
    b.save(F/'results.json',data)
    for enabled in (False,True):
        b.busy()
        env = b.environment(cfg,reproducible=True)
        env['STRATA_PREFILL_GPU_WIDEN']='1' if a.bf16 else str(int(enabled))
        env['STRATA_PREFILL_BF16_GPU_WIDEN']=str(int(enabled)) if a.bf16 else '0'
        engine = b.Engine(exe,argv,env,F/str(int(enabled)))
        row = {'widen':enabled,'info':engine.info,'requests':[],
               'env':{k:v for k,v in env.items() if k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}}
        data['runs'].append(row)
        b.save(F/'results.json',data)
        try:
            assert 'expert_slots=5771 ' in engine.info and 'expert_cache_mib=7922 ' in engine.info
            for repeat in range(2):
                print('SAMPLE',enabled,repeat,flush=True)
                row['requests'].append(engine.gen(ids,48,keys))
                b.save(F/'results.json',data)
        finally:
            engine.close()
    tokens = [s['tokens'] for r in data['runs'] for s in r['requests']]
    data['outputs_equal'] = all(t==tokens[0] for t in tokens)
    data['nondegenerate_outputs'] = all(any(t!=0 for t in seq) for seq in tokens)
    data['functional_acceptance'] = data['outputs_equal'] and data['nondegenerate_outputs']
    data['complete']=True
    b.save(F/'results.json',data)
    print('PASS' if data['functional_acceptance'] else 'FAIL',flush=True)
    if not data['functional_acceptance']:
        raise RuntimeError('Sampling differs between baseline/candidate or repeated requests')
