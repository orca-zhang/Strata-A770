"""Serial checks/timings for the isolated GPU-widening build."""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b

ap = argparse.ArgumentParser()
ap.add_argument('--out',required=True,type=Path)
ap.add_argument('--build-root',type=Path,default=H/'prefill-widen')
ap.add_argument('--phase',required=True,choices=('operators','model'))
ap.add_argument('--switches',default='0,1,0,1,0')
ap.add_argument('--cases',default='Short')
ap.add_argument('--reader',choices=('batch','window'),default='batch')
ap.add_argument('--debug-nan',action='store_true')
ap.add_argument('--reference',type=Path)
ap.add_argument('--dtype',choices=('f16','bf16'),default='f16')
ap.add_argument('--prefix-tokens',type=int,default=0,help='Limit the frozen input for a bounded batch-prefill check')
ap.add_argument('--max-tokens',type=int,default=48)
a = ap.parse_args()
assert a.prefix_tokens >= 0 and a.max_tokens > 0
switches = [int(s) for s in a.switches.split(',')]
assert all(s in (0,1) for s in switches)
cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
exe = a.build_root.resolve()/('parity' if a.phase=='operators' else 'model')/'strata.exe'
build = json.loads((a.build_root/'build.json').read_text())
assert build['complete'] and b.sha(exe) == build['exe_sha256']['parity' if a.phase=='operators' else 'model']
with b.gpu_lock():
    a.out.mkdir(exist_ok=False)
    (a.out/'test_prefill_widen.py').write_bytes(Path(__file__).read_bytes())
    data = {'complete':False,'phase':a.phase,'dtype':a.dtype,'exe_sha256':b.sha(exe),'build_sha256':b.sha(a.build_root/'build.json'),
            'runs':[],'note':'Both arms use the same binary, toolchain, BLAS DLL and all other flags; experimental switch defaults off.'}
    b.save(a.out/'results.json',data)
    for i,enabled in enumerate(switches):
        b.busy()
        env = b.environment(cfg)
        env['STRATA_PREFILL_GPU_WIDEN'] = str(enabled) if a.dtype=='f16' else '1'
        env['STRATA_PREFILL_BF16_GPU_WIDEN'] = str(enabled) if a.dtype=='bf16' else '0'
        print('RUN',a.phase,i,enabled,flush=True)
        row = {'widen':bool(enabled),'env':{k:v for k,v in env.items() if k.startswith(('STRATA_','UR_','SYCL_','ONEAPI_','ZES_'))}}
        if a.phase == 'operators':
            argv = [str(exe),'--bench']
            p = subprocess.run(argv,cwd=R,env=env,capture_output=True,text=True,timeout=180)
            (a.out/f'{i}.log').write_text(p.stdout+p.stderr)
            row.update(argv=argv,returncode=p.returncode,
                       batches=[json.loads(s) for s in re.findall(r'^BATCH (.+)$',p.stdout,re.M)],
                       benches=[json.loads(s) for s in re.findall(r'^BENCH (.+)$',p.stdout,re.M)],
                       summary=[json.loads(s) for s in re.findall(r'^SUMMARY (.+)$',p.stdout,re.M)])
            data['runs'].append(row)
            b.save(a.out/'results.json',data)
            p.check_returncode()
            assert row['summary'] == [{'checks':6,'mismatched_cases':0}]
            assert len(row['batches']) == 10 and len(row['benches']) == 2
        else:
            argv = b.model_args(cfg,False,.5,5500)
            argv[argv.index('--prefill')+1] = '256'
            argv[argv.index('--short-read')+1] = '8192' if a.reader=='window' else '0'
            argv += ['--kv','int8']
            # Diagnostic traces do not add GPU waits. These cold requests are not performance acceptance.
            env['STRATA_TRACE'] = '1'
            row['env']['STRATA_TRACE'] = '1'
            if a.debug_nan:
                env['STRATA_DBG_NAN'] = '1'
                row['env']['STRATA_DBG_NAN'] = '1'
            engine = b.Engine(exe,argv,env,a.out/f'{i}')
            row.update(argv=argv,info=engine.info,requests=[])
            data['runs'].append(row)
            b.save(a.out/'results.json',data)
            try:
                assert 'expert_slots=5771 ' in engine.info and 'expert_cache_mib=7922 ' in engine.info
                for name in a.cases.split(','):
                    print('MODEL',i,name,flush=True)
                    ids = b.prompts(cfg)[name]
                    if a.prefix_tokens:
                        assert a.prefix_tokens <= len(ids)
                        ids = ids[:a.prefix_tokens]
                    result = engine.gen(ids,a.max_tokens)
                    row['requests'].append({'case':name,'input_tokens':ids,**result})
                    b.save(a.out/'results.json',data)
            finally:
                engine.close()
        b.busy()
    if a.phase == 'operators':
        data['comparisons'] = []
        for i in range(1,len(switches)-1):
            if switches[i-1:i+2] != [0,1,0]: continue
            for dtype in (False,True):
                times = [next(s['total_us'] for s in data['runs'][j]['benches'] if s['bf16']==dtype) for j in (i-1,i,i+1)]
                change = b.paired(*times)
                optimized = dtype == (a.dtype=='bf16')
                data['comparisons'].append({'candidate_run':i,'bf16':dtype,'optimized':optimized,**change,
                                            'meets_screen':optimized and abs(change['baseline_drift_pct'])<=3 and change['throughput_change_pct']>=3})
    else:
        same = True
        for name in a.cases.split(','):
            tokens = [next(s['tokens'] for s in row['requests'] if s['case']==name) for row in data['runs']]
            same = same and all(t==tokens[0] for t in tokens)
        data['outputs_equal'] = same
        data['diagnostic_only'] = True
        data['nondegenerate_outputs'] = all(any(t!=0 for t in s['tokens']) for row in data['runs'] for s in row['requests'])
        if a.reference:
            reference = json.loads(a.reference.read_text())
            expected = {s['case']:s['tokens'] for row in reference['runs'] if not row['widen'] for s in row['requests']}
            data['reference_sha256'] = b.sha(a.reference)
            data['outputs_equal_reference'] = all(s['tokens']==expected[s['case']] for row in data['runs'] for s in row['requests'])
        data['functional_acceptance'] = same and data['nondegenerate_outputs'] and data.get('outputs_equal_reference',True)
    data['complete'] = True
    b.save(a.out/'results.json',data)
    print(json.dumps(data.get('comparisons',{k:data[k] for k in
        ('outputs_equal','nondegenerate_outputs','outputs_equal_reference','functional_acceptance') if k in data}),indent=2),flush=True)
    if a.phase == 'model' and not data['functional_acceptance']:
        raise RuntimeError('Model correctness did not pass; inspect saved outputs')
