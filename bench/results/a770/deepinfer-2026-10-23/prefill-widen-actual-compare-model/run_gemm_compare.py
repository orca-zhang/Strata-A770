"""Bounded comparison of real model GEMM inputs; deliberately exits before a full request."""
import json
import subprocess
import sys
import time
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b

F = H/'prefill-widen-actual-compare-model'
B = H/'prefill-widen-actual-compare'
cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
build = json.loads((B/'build.json').read_text())
exe = B/'model/strata.exe'
assert build['complete'] and b.sha(exe)==build['exe_sha256']['model']
argv = b.model_args(cfg,False,.5,5500)
argv[argv.index('--prefill')+1]='256'
argv[argv.index('--short-read')+1]='0'
argv += ['--kv','int8']
env = b.environment(cfg)
env.update(STRATA_PREFILL_GPU_WIDEN='1',STRATA_PREFILL_GEMM_COMPARE='1',STRATA_TRACE='1',STRATA_DBG_NAN='1')
ids = b.prompts(cfg)['Short']
request = 'GEN 16 '+','.join(map(str,ids))+'\nQUIT\n'
with b.gpu_lock():
    F.mkdir(exist_ok=False)
    (F/'run_gemm_compare.py').write_bytes(Path(__file__).read_bytes())
    (F/'requests.in').write_text(request)
    result = {'complete':False,'diagnostic_only':True,'exe_sha256':b.sha(exe),'argv':argv,
              'env':{k:v for k,v in env.items() if k.startswith(('STRATA_','UR_','SYCL_','ONEAPI_','ZES_'))},
              'timeout_seconds':180,'expected_exit_codes':{'86':'First mismatch or nonfinite result','87':'32 finite GEMMs matched'}}
    b.save(F/'results.json',result)
    started = time.perf_counter()
    with (F/'compare.protocol').open('x') as out, (F/'compare.stderr').open('x') as err:
        try:
            p = subprocess.run([str(exe),'--serve',*argv],cwd=R,env=env,input=request,text=True,
                               stdout=out,stderr=err,timeout=180)
            result.update(returncode=p.returncode,timeout=False)
        except subprocess.TimeoutExpired:
            result['timeout']=True
    result.update(complete=True,wall_seconds=time.perf_counter()-started,
                  protocol_sha256=b.sha(F/'compare.protocol'),stderr_sha256=b.sha(F/'compare.stderr'))
    result['comparisons']=[s for s in (F/'compare.stderr').read_text().splitlines() if s.startswith('GEMM_COMPARE')]
    b.save(F/'results.json',result)
    print(json.dumps(result,indent=2),flush=True)
