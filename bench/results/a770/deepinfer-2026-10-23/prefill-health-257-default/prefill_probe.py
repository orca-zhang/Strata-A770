"""Bounded baseline prefill diagnostics; each invocation owns the GPU lock."""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--out', type=Path, required=True)
ap.add_argument('--variant', choices=['default', 'regular', 'inline', 'sync', 'ring2', 'v1'], default='default')
ap.add_argument('--length', type=int, default=257)
ap.add_argument('--chunk', type=int, default=256)
ap.add_argument('--seconds', type=int, default=120)
ap.add_argument('--short-primer', action='store_true')
a = ap.parse_args()
assert 2 <= a.length <= 8000 and a.chunk > 0 and a.seconds > 0
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
exe = H / 'sampler-fp32/baseline/strata.exe'
expected = json.loads((H / 'sampler-fp32/baseline/build.json').read_text())['exe_sha256']
assert b.sha(exe) == expected
argv = b.model_args(cfg, False, .5, 5500)
argv[argv.index('--prefill') + 1] = str(a.chunk)
argv[argv.index('--short-read') + 1] = '0'
argv += ['--kv', 'int8']
env = b.environment(cfg)
extra = {'STRATA_TRACE': '1', 'STRATA_WATCHDOG_S': '35'}
extra.update({'default': {}, 'regular': {'SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS': '0'},
              'inline': {'STRATA_PREFILL_ISSUER': '0'}, 'sync': {'STRATA_PREFILL_SYNC': '1'},
              'ring2': {'STRATA_PREFILL_RING': '2'}, 'v1': {'UR_L0_USE_LEVEL_ZERO_V2': '0'}}[a.variant])
env.update(extra)
inputs = b.prompts(cfg)
requests = []
if a.short_primer:
    requests.append('GEN 16 ' + ','.join(map(str, inputs['Short'])))
ids = (inputs['Code2K'] if a.length <= 2048 else inputs['Long8K'])[:a.length]
requests.append('GEN 16 ' + ','.join(map(str, ids)))
text = '\n'.join([*requests, 'QUIT', ''])
with b.gpu_lock():
    a.out.mkdir(parents=True, exist_ok=False)
    (a.out / 'prefill_probe.py').write_bytes(Path(__file__).read_bytes())
    (a.out / 'requests.in').write_text(text)
    data = {'complete': False, 'diagnostic_only': True, 'variant': a.variant,
            'exe': str(exe), 'exe_sha256': expected, 'argv': argv, 'env': env,
            'script_sha256': b.sha(Path(__file__)), 'timeout_seconds': a.seconds,
            'prompt_length': a.length, 'short_primer': a.short_primer}
    # Store only the configured GPU settings; inherited system environment can contain secrets.
    data['env'] = {k: v for k, v in env.items() if k.startswith(('STRATA_', 'UR_', 'SYCL_', 'ONEAPI_', 'ZES_'))}
    b.save(a.out / 'results.json', data)
    t0 = time.perf_counter()
    print('START', a.variant, a.length, a.chunk, flush=True)
    with (a.out / 'baseline.protocol').open('x') as stdout, (a.out / 'baseline.stderr').open('x') as stderr:
        try:
            p = subprocess.run([str(exe), '--serve', *argv], cwd=R, env=env,
                               input=text, text=True, stdout=stdout, stderr=stderr, timeout=a.seconds)
            data.update(returncode=p.returncode, timeout=False)
        except subprocess.TimeoutExpired:
            data['timeout'] = True
    lines = (a.out / 'baseline.protocol').read_text().splitlines()
    logs = (a.out / 'baseline.stderr').read_text().splitlines()
    data.update(complete=True, wall_seconds=time.perf_counter()-t0,
                info=[s for s in lines if s.startswith('INFO ')],
                done=[s for s in lines if s.startswith('DONE ')],
                last_log=logs[-25:], protocol_sha256=b.sha(a.out/'baseline.protocol'),
                stderr_sha256=b.sha(a.out/'baseline.stderr'))
    b.save(a.out / 'results.json', data)
    print(json.dumps({k: data.get(k) for k in ('variant', 'returncode', 'timeout', 'wall_seconds', 'done', 'last_log')}, indent=2), flush=True)
