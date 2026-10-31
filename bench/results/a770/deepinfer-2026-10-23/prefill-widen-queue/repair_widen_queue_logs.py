"""Recheck two overwritten parity logs with the same candidate binary; timing is not repeated."""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b

F = H / 'prefill-widen-queue'
state = json.loads((F / 'results.json').read_text())
manifest = json.loads((F / 'build/build.json').read_text())
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
exe = F / 'build/parity/strata.exe'
assert b.sha(exe) == manifest['exe_sha256']['parity']
shutil.copyfile(F / 'large-normal.log', F / 'check-large-normal.log')
shutil.copyfile(Path(__file__), F / 'repair_widen_queue_logs.py')
shutil.copyfile(H / 'check_widen_queue.py', F / 'replay_check_widen_queue.py')
repair = {'complete': False, 'original_results_sha256': b.sha(F / 'results.json'),
          'candidate_exe_sha256': b.sha(exe),
          'reason': 'large-1.log and large-4.log correctness logs were overwritten by benchmark logs; original structured summaries preserved, raw parity rechecked.',
          'replay_change': 'Prefix correctness log names with check-; computation and measurement unchanged.', 'runs': []}
with b.gpu_lock():
    for arena in ('1', '4'):
        b.busy()
        env = b.environment(cfg)
        env.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1',
                   STRATA_PREFILL_HALF_OUTPUT='0', STRATA_PREFILL_HALF_BF16='0', STRATA_PREFILL_HALF_PROFILE='0',
                   STRATA_GEMM_SHARED_ARENA=arena)
        label = 'check-large-' + arena
        print('RECHECK', label, flush=True)
        p = subprocess.run([str(exe), '--large'], cwd=R, env=env, capture_output=True, text=True, timeout=240)
        (F / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8')
        summary = [json.loads(v) for v in re.findall(r'^SUMMARY (.+)$', p.stdout, re.M)]
        repair['runs'].append({'label': label, 'arena': arena, 'returncode': p.returncode, 'summary': summary})
        b.save(F / 'logging-repair.json', repair)
        p.check_returncode()
        assert summary == [{'checks': 270, 'mismatched_cases': 0}]
for path, digest in state['source_sha256'].items(): assert b.sha(R / path) == digest, path
for path, digest in state['production_sha256'].items(): assert b.sha(path) == digest, path
assert b.sha(F / 'results.json') == repair['original_results_sha256']
repair['complete'] = True
b.save(F / 'logging-repair.json', repair)
print('PASS 540 repeated parity checks; original measurements unchanged', flush=True)
