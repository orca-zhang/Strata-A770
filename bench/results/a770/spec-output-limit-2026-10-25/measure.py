"""Independent A/B/A/B/A short-output screening; one predefined confirmation."""
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b


def main():
    import importlib.util
    spec = importlib.util.spec_from_file_location('window_checks', H/'check.py')
    checks = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checks)
    correct = json.loads((H/'checks/results.json').read_text())
    assert correct['complete']
    cfg, env, args = checks.setup()
    for key in ('STRATA_TRACE', 'STRATA_VERIFY_PROFILE', 'STRATA_DECODE_TIMING', 'STRATA_STATE_HASH', 'STRATA_SNAPSHOT_VERIFY'):
        env.pop(key, None)
    ids = correct['prompt']
    out = H/'measure'
    out.mkdir(exist_ok=False)
    data = {'complete': False, 'scope': 'Fresh Short prompt, 4/6/16 output tokens; not the four-scenario model matrix.',
            'primary_cap': 6, 'warmups_per_process': 2, 'retained_per_cap': 3, 'args': args,
            'environment': {k:v for k,v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))},
            'screen': [], 'confirm': []}
    def save():
        b.save(out/'results.json', data)
    save()
    def phase(name):
        runs = data[name]
        for i, side in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
            engine = b.Engine(checks.executable(side), args, env, out/f'{name}-{i}-{side}')
            run = {'side': side, 'runtime_info': engine.info, 'warmup': [], 'samples': {}, 'medians': {}}
            runs.append(run)
            assert 'expert_slots=4191' in engine.info and 'expert_cache_mib=5760' in engine.info, engine.info
            try:
                for _ in range(2):
                    run['warmup'].append(engine.gen(ids, 16))
                for cap in (4, 6, 16):
                    rows = [engine.gen(ids, cap) for _ in range(3)]
                    assert all(row['tokens'] == correct['reference']['0.5-False'][:cap]
                               and row['generated'] == cap and row['finish'] == 'length' and row['reused'] == 0 for row in rows)
                    run['samples'][str(cap)] = rows
                    run['medians'][str(cap)] = {key: statistics.median(row[key] for row in rows)
                                               for key in ('decode_ms', 'prompt_ms', 'ttft_ms')}
                    print('MEASURE', name, i, side, cap, run['medians'][str(cap)], flush=True)
                    save()
            finally:
                engine.close()
            save()
        pairs = []
        for i in (1, 3):
            for cap in (4, 6, 16):
                for metric in ('decode_ms', 'prompt_ms', 'ttft_ms'):
                    values = [runs[k]['medians'][str(cap)][metric] for k in (i-1, i, i+1)]
                    row = {'candidate_run': i, 'cap': cap, 'metric': metric, **b.paired(*values)}
                    row['eligible'] = abs(row['baseline_drift_pct']) <= 3
                    pairs.append(row)
        eligible = [p for p in pairs if p['eligible'] and p['metric'] == 'decode_ms' and p['cap'] == 6]
        passed = bool(eligible) and all(p['throughput_change_pct'] >= 1 for p in eligible)
        regressions = [p for p in pairs if p['eligible'] and p['metric'] == 'decode_ms' and p['throughput_change_pct'] < -2]
        summary = {'primary_passed': passed, 'decode_regressions_over_2pct': regressions, 'comparisons': pairs}
        data[name+'_summary'] = summary
        save()
        return passed and not regressions
    data['screen_passed'] = phase('screen')
    if data['screen_passed']:
        data['confirm_passed'] = phase('confirm')
    else:
        data['confirm_passed'] = False
    data['complete'] = True
    data['raw_sha256'] = {p.name: b.sha(p) for p in out.iterdir() if p.suffix in ('.stderr', '.protocol')}
    save()
    print('DONE predefined screen/confirmation', data['screen_passed'], data['confirm_passed'], flush=True)
    import subprocess
    spec = importlib.util.spec_from_file_location('compiler_env', R/'build-sycl-deepinfer-20261023/build.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    with (H/'cpu-final.txt').open('x') as log:
        subprocess.run([sys.executable, '-X', 'utf8', str(R/'tools/test_spec_window_limit.py'),
            '--source', str(H/'candidate.cpp'), '--cxx', str(builder.ICX/'bin/icx.exe'), '--out', str(H/'cpu-final')],
            env=builder.environment(), stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)


if __name__ == '__main__':
    with b.gpu_lock():
        main()
