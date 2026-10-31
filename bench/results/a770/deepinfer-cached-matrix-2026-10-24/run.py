"""Four-case cached-prefix A/B/A; reuse existing Engine, checkpoints and GPU lock."""
import datetime
import hashlib
import json
import math
import re
import statistics
import sys
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
sys.path.insert(0, str(F))
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as locks
import frozen_bench as bench

P = json.loads((F / 'preparation.json').read_text(encoding='utf-8'))


def save(data):
    bench.save(F / 'results.json', data)


def verify():
    for name, digest in P['frozen_sha256'].items():
        assert bench.sha(R / name) == digest, name


def median(run, name, metric):
    values = [row[metric] / row['generated'] if metric == 'decode_ms' else row[metric]
              for row in run['cases'][name]['samples']]
    assert all(math.isfinite(v) and v > 0 for v in values)
    return statistics.median(values)


def summarize(runs):
    assert len(runs) == 3 and [r['mode'] for r in runs] == P['modes']
    comparisons = []
    for name in P['prompts']:
        rows = [v for r in runs for v in [r['cases'][name]['initial'],
                                         *r['cases'][name]['warmups'], *r['cases'][name]['samples']]]
        outputs_equal = len({tuple(v['tokens']) for v in rows}) == 1
        cached = [v for r in runs for v in [*r['cases'][name]['warmups'], *r['cases'][name]['samples']]]
        reuse_equal = len({v['reused'] for v in cached}) == 1
        if name != 'Short':
            assert all(len(P['prompts'][name])-16 <= v['reused'] < len(P['prompts'][name]) for v in cached)
        metrics = {key: bench.paired(*(median(r, name, key) for r in runs))
                   for key in ('decode_ms', 'prompt_ms', 'ttft_ms')}
        d = metrics['decode_ms']
        comparisons.append(dict(case=name, outputs_equal=outputs_equal, reuse_equal=reuse_equal,
                                metrics=metrics, decode_eligible=abs(d['baseline_drift_pct']) <= 3,
                                cached_reused_tokens=sorted({v['reused'] for v in cached})))
    exact = all(c['outputs_equal'] and c['reuse_equal'] for c in comparisons)
    eligible = all(c['decode_eligible'] for c in comparisons)
    gains = [c['metrics']['decode_ms']['throughput_change_pct'] for c in comparisons]
    overall = 100*(math.exp(statistics.mean(math.log1p(g/100) for g in gains))-1)
    passed = exact and eligible and overall >= 1 and min(gains) >= -2
    return dict(comparisons=comparisons, outputs_and_reuse_equal=exact,
                all_decode_pairs_eligible=eligible, descriptive_decode_geomean_pct=overall,
                screen_pass=passed, confirmed_model_speedup=False)


def main():
    assert not (F / 'results.json').exists(), 'Use a fresh directory'
    verify()
    assert bench.paired(100, 50, 100)['throughput_change_pct'] == 100
    state = dict(complete=False, scope='Cached-prefix four-case model screen; window prompt path; '
                 'initial reads are not preheated or native batched-prefill performance measurements.',
                 rule_before_measurement=P['rule'], started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                 preparation_sha256=bench.sha(F / 'preparation.json'), runs=[])
    with locks.gpu_lock():
        verify()
        state['tooling_sha256'] = {p.name: bench.sha(p) for p in
                                   (Path(__file__), F / 'frozen_bench.py', F / 'prepare.py')}
        save(state)
        try:
            for i, mode in enumerate(P['modes']):
                env = bench.environment(P['config'], mode, reproducible=False)
                assert not any(env.get(k) for k in ('STRATA_DECODE_TIMING', 'STRATA_VERIFY_PROFILE',
                                                    'STRATA_STATE_HASH', 'STRATA_STATE_HASH_GDN'))
                print('MODEL', i, mode, flush=True)
                engine = bench.Engine(Path(P['exe']), P['args'], env, F / f'{i}-{mode}')
                info = {k: int(re.search(r'\b' + k + r'=(\d+)', engine.info)[1])
                        for k in ('expert_slots', 'expert_cache_mib')}
                assert info == dict(expert_slots=P['expected_expert_slots'], expert_cache_mib=P['expected_expert_cache_mib'])
                run = dict(mode=mode, runtime_info=engine.info, capacity=info, cases={})
                state['runs'].append(run)
                save(state)
                try:
                    for name, ids in P['prompts'].items():
                        case = dict(initial=None, warmups=[], samples=[])
                        run['cases'][name] = case
                        for phase, count in (('initial', 1), ('warmups', P['warmups']), ('samples', P['retained'])):
                            for j in range(count):
                                print('REQUEST', i, mode, name, phase, j, flush=True)
                                bench.busy(engine.p.pid)
                                row = engine.gen(ids, P['max_tokens'])
                                bench.busy(engine.p.pid)
                                assert row['generated'] > 0 and row['resume'] == row['reused']
                                if phase == 'initial':
                                    case[phase] = row
                                else:
                                    case[phase].append(row)
                                    assert row['tokens'] == case['initial']['tokens'], (mode, name, phase, j)
                                save(state)
                                print('DONE', i, name, phase, j, 'prompt_ms', row['prompt_ms'],
                                      'decode_ms', row['decode_ms'], 'reused', row['reused'], flush=True)
                finally:
                    engine.close()
                verify()
            state['summary'] = summarize(state['runs'])
            assert state['summary']['outputs_and_reuse_equal']
            state['complete'] = True
            state['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            state['production_unchanged'] = True
            save(state)
        except BaseException as error:
            state['failure'] = repr(error)
            save(state)
            raise
    print('COMPLETE', json.dumps(state['summary']), flush=True)


if __name__ == '__main__':
    main()
