"""Serial minimal check for workspace/output slices of the same USM allocation."""
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
ap.add_argument('--build-root',type=Path,required=True)
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--offset-gib',type=int,choices=(0,4),default=0)
a = ap.parse_args()
exe = a.build_root.resolve()/'parity/strata.exe'
build = json.loads((a.build_root/'build.json').read_text())
assert build['complete'] and b.sha(exe)==build['exe_sha256']['parity']
cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
with b.gpu_lock():
    a.out.mkdir(exist_ok=False)
    (a.out/'test_gemm_arena.py').write_bytes(Path(__file__).read_bytes())
    result = {'complete':False,'diagnostic_only':True,'exe_sha256':b.sha(exe),'build_sha256':b.sha(a.build_root/'build.json'),'runs':[]}
    b.save(a.out/'results.json',result)
    for enabled in (False,True):
        b.busy()
        env = b.environment(cfg)
        env.update(STRATA_PREFILL_GPU_WIDEN=str(int(enabled)),STRATA_GEMM_SHARED_ARENA='4' if a.offset_gib else '1')
        p = subprocess.run([str(exe)],cwd=R,env=env,capture_output=True,text=True,timeout=180)
        (a.out/f'{int(enabled)}.log').write_text(p.stdout+p.stderr)
        summary = [json.loads(s) for s in re.findall(r'^SUMMARY (.+)$',p.stdout,re.M)]
        result['runs'].append({'widen':enabled,'shared_arena':True,'offset_gib':a.offset_gib,'returncode':p.returncode,'summary':summary,
                               'env':{k:v for k,v in env.items() if k.startswith(('STRATA_','UR_','SYCL_','ONEAPI_','ZES_'))}})
        b.save(a.out/'results.json',result)
        print('RUN',enabled,p.returncode,summary,flush=True)
    result['complete']=True
    result['functional_acceptance']=all(r['returncode']==0 and r['summary']==[{'checks':255,'mismatched_cases':0}] for r in result['runs'])
    b.save(a.out/'results.json',result)
    if not result['functional_acceptance']:
        raise RuntimeError('Arena parity did not pass')
