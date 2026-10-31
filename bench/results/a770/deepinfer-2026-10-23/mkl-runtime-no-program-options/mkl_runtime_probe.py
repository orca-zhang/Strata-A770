"""Repeat the frozen GEMM probe with one runtime setting changed; no rebuild."""
import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H.parent/'tools'))
import bench_deepinfer as b

ap = argparse.ArgumentParser()
ap.add_argument('--out', required=True, type=Path)
ap.add_argument('--variant', required=True, choices=('baseline', 'no-program-options', 'verbose', 'trace'))
a = ap.parse_args()
exe = H/'mkl-scalar-probe-final/strata-mkl-scalar-probe.exe'
cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
env = b.environment(cfg)
if a.variant == 'no-program-options':
    env.pop('SYCL_PROGRAM_COMPILE_OPTIONS', None)
elif a.variant == 'verbose':
    env['MKL_VERBOSE'] = '1'
elif a.variant == 'trace':
    env['SYCL_UR_TRACE'] = '2'
with b.gpu_lock():
    a.out.mkdir(exist_ok=False)
    (a.out/'mkl_runtime_probe.py').write_bytes(Path(__file__).read_bytes())
    result = {'complete': False, 'diagnostic_only': True, 'variant': a.variant,
              'exe_sha256': b.sha(exe), 'argv': [str(exe)], 'timeout_seconds': 180,
              'env': {k: v for k, v in env.items() if k.startswith(('STRATA_', 'UR_', 'SYCL_', 'ONEAPI_', 'ZES_', 'MKL_'))}}
    b.save(a.out/'results.json', result)
    t0 = time.perf_counter()
    print('RUN', a.variant, flush=True)
    with (a.out/'stdout.log').open('x') as out, (a.out/'stderr.log').open('x') as err:
        try:
            p = subprocess.run([str(exe)], cwd=a.out, env=env, stdout=out, stderr=err, timeout=180)
            result.update(returncode=p.returncode, timeout=False)
        except subprocess.TimeoutExpired:
            result['timeout'] = True
    output = (a.out/'stdout.log').read_text(errors='replace')
    result.update(complete=True, wall_seconds=time.perf_counter()-t0,
                  runs=[json.loads(s) for s in re.findall(r'^GEMM (.+)$', output, re.M)],
                  queries=[json.loads(s) for s in re.findall(r'^QUERY (.+)$', output, re.M)],
                  pass_check=bool(re.search(r'^PASS$', output, re.M)))
    b.save(a.out/'results.json', result)
    print(json.dumps(result, indent=2), flush=True)
    if not result['pass_check'] or result.get('returncode') != 0:
        raise RuntimeError('Probe did not pass; inspect saved logs')
