"""Five independent CLI processes, A/B/A/B/A; no trace/profiling; same input and frozen cache."""
import importlib.util
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import time

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-mtp-summary-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('runner', H/'run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def main(out):
    runner.verify()
    checks = json.loads((H/'checks-final-queue/results.json').read_text())
    assert checks['complete']
    expected = next(row['tokens'] for row in checks['cli'] if row['name'] == 'candidate-cli-1-two')
    out.mkdir(parents=True, exist_ok=False)
    cfg = json.loads((H/'config.json').read_text())
    env = b.environment(cfg, reproducible=True)
    env['STRATA_SPEC_COUPLED'] = '1'
    for key in ('STRATA_TRACE', 'STRATA_PREFILL_SYNC', 'STRATA_PREFILL_TIMING', 'STRATA_DECODE_TIMING',
                'STRATA_DRAFT_TIMING', 'STRATA_VERIFY_PROFILE'):
        env.pop(key, None)
    args = b.model_args(cfg, reproducible=True, cache_mib=5500)+['--kv', 'int8', '--stop-eos', '--tokens', '271,91', '--max-new', '6']
    args[args.index('--prefill')+1] = '1'
    data = {'complete': False, 'scope': 'Cold native CLI with a two-token prompt; prompt/TTFT only, not service decode or a four-scenario model speed claim.',
            'config': cfg, 'expected': expected, 'runs': [], 'comparisons': [],
            'warmup': 'Both engine paths already exercised by functional checks in independent processes; first result of every process is measured (CLI cold prompt).',
            'environment': {k: v for k, v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'ONEAPI_', 'UR_', 'ZES_'))},
            'exe_sha256': checks['exe_sha256']}
    def save():
        b.save(out/'results.json', data)
    save()
    for i, side in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
        name = f'{i}-{side}'
        exe = (P if side == 'baseline' else H)/'candidate/strata.exe'
        assert b.sha(exe) == data['exe_sha256'][side]
        command = [str(exe), *args]
        print('MEASURE', name, flush=True)
        started = time.perf_counter()
        with (out/(name+'.stdout')).open('xb') as stdout, (out/(name+'.stderr')).open('xb') as stderr:
            result = subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=240)
        wall = time.perf_counter()-started
        text = (out/(name+'.stdout')).read_text()
        stderr = (out/(name+'.stderr')).read_text()
        tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
        prompt_ms, ttft_ms = map(float, re.search(r'^prefill\s+1 tokens in ([\d.]+) ms.*time to first token ([\d.]+) ms', text, re.M).groups())
        decode_ms = float(re.search(r'^decode\s+6 tokens in ([\d.]+) ms', text, re.M)[1])
        rounds, window, accepted, offered = map(int, re.search(r'^speculation\s+(\d+) rounds of (\d+), drafts accepted (\d+) of (\d+)', text, re.M).groups())
        row = {'name': name, 'side': side, 'command': command, 'returncode': result.returncode,
               'tokens': tokens, 'prompt_ms': prompt_ms, 'ttft_ms': ttft_ms, 'decode_ms': decode_ms, 'wall_s': wall,
               'cache': list(re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', stderr).groups()),
               'rounds': rounds, 'window': window, 'accepted': accepted, 'offered': offered}
        data['runs'].append(row)
        save()
        result.check_returncode()
        assert tokens == expected and row['cache'] == ['5771', '7.74'], row
        assert all(math.isfinite(x) and x > 0 for x in (prompt_ms, ttft_ms, decode_ms, wall))
        print('RESULT', name, 'prompt', prompt_ms, 'TTFT', ttft_ms, 'decode', decode_ms, flush=True)
    for i in (1, 3):
        for metric in ('prompt_ms', 'ttft_ms', 'wall_s', 'decode_ms'):
            a, v, c = [data['runs'][k][metric] for k in (i-1, i, i+1)]
            row = {'candidate_run': i, 'metric': metric, **b.paired(a, v, c), 'latency_reduction_pct': 100*(1-v/math.sqrt(a*c))}
            row['stable'] = abs(row['baseline_drift_pct']) <= 3
            row['passes_1pct'] = row['stable'] and row['throughput_change_pct'] >= 1
            data['comparisons'].append(row)
    data['complete'] = True
    data['raw_sha256'] = {p.name: b.sha(p) for p in out.iterdir() if p.suffix in ('.stdout', '.stderr')}
    save()
    runner.verify()
    print('PASS serial CLI measurement; inspect drift before claiming gains', flush=True)


if __name__ == '__main__':
    with b.gpu_lock():
        main(Path(sys.argv[1]) if len(sys.argv) > 1 else H/'measure-final')
