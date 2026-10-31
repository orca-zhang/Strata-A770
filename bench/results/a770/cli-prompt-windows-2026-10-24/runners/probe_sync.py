"""Replay the minimal reproducer with existing phase synchronization enabled."""
import json
from pathlib import Path
import subprocess
import sys
import time

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b

with b.gpu_lock():
    out = H/'probe-sync'
    out.mkdir(exist_ok=False)
    reference = json.loads((H/'probe/results.json').read_text())
    assert reference['complete'] and reference['timed_out']
    cfg = json.loads((H/'config.json').read_text())
    env = b.environment(cfg, reproducible=True)
    env.update(reference['environment'])
    env['STRATA_PREFILL_SYNC'] = '1'
    data = {'complete': False, 'command': reference['command'], 'timeout_s': 120,
            'environment': reference['environment'] | {'STRATA_PREFILL_SYNC': '1'},
            'scope': reference['scope']}
    b.save(out/'results.json', data)
    print('RUN phase-sync two-token prefill1', flush=True)
    started = time.perf_counter()
    with (out/'baseline.stdout').open('xb') as stdout, (out/'baseline.stderr').open('xb') as stderr:
        try:
            result = subprocess.run(data['command'], cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=120)
            data.update(returncode=result.returncode, timed_out=False)
        except subprocess.TimeoutExpired:
            data.update(returncode=None, timed_out=True)
    data.update(complete=True, elapsed_s=time.perf_counter()-started)
    data['raw_sha256'] = {p.name: b.sha(p) for p in out.glob('baseline.*')}
    b.save(out/'results.json', data)
    print('RESULT', data['timed_out'], data['returncode'], flush=True)
